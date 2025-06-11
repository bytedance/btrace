/*
 * Copyright (C) 2025 ByteDance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <execinfo.h>
#include <pthread.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <pthread/stack_np.h>

#include <algorithm>

#include "memory_profiler.hpp"
#include "event_loop.hpp"


#define stack_logging_type_free 0
#define stack_logging_type_generic 1        /* anything that is not allocation/deallocation */
#define stack_logging_type_alloc 2          /* malloc, realloc, etc... */
#define stack_logging_type_dealloc 4        /* free, realloc, etc... */
#define stack_logging_type_vm_allocate 16   /* vm_allocate or mmap */
#define stack_logging_type_vm_deallocate 32 /* vm_deallocate or munmap */
#define stack_logging_type_mapped_file_or_shared_mem 128

typedef void(malloc_logger_t)(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t result, uint32_t skip);

extern malloc_logger_t *__syscall_logger;
extern malloc_logger_t *malloc_logger;

namespace btrace
{

    extern int64_t start_mach_time;

    bool MemorySampleBuffer::GetSample(RingBuffer *buffer, MemorySample *sample)
    {
        bool result = false;

        RingBuffer::Buffer buf;
        buf = buffer->BeginRead();

        if (buf)
        {
            result = GetSampleFromBuffer(buffer, &buf, sample);
        }

        buffer->EndRead(std::move(buf));

        return result;
    }

    bool MemorySampleBuffer::GetSampleFromBuffer(RingBuffer *ring_buffer, RingBuffer::Buffer *buffer, MemorySample *out)
    {
        char *buf = reinterpret_cast<char *>(buffer->data);
        size_t size = buffer->size;
        char *end = buf + size;
        
        MemoryData *event_data;
        if (!RingBuffer::ViewAndAdvance<MemoryData>(&buf, &event_data, end))
        {
            return false;
        }
        ring_buffer->Get(&out->data, event_data, sizeof(out->data));
        
        if (buf > end)
        {
            return false;
        }
        
        constexpr size_t pc_size = sizeof(get_type<decltype(out->pcs)>::type);
        
        if (out->data.event_type == MemEventType::Malloc)
        {
            ring_buffer->Get(out->pcs, buf, static_cast<size_t>(end - buf));
            out->stack_size = (uint32_t)((size_t)(end - buf) / pc_size);
        }
        else if (out->data.event_type == MemEventType::Free)
        {
            if (!MemoryProfiler::lite_mode_) {
                ring_buffer->Get(out->pcs, buf, static_cast<size_t>(end - buf));
                out->stack_size = (uint32_t)((size_t)(end - buf) / pc_size);
            }
        }
        else
        {
            return false;
        }

        return true;
    }

    bool MemorySampleBuffer::PutSample(MemorySample &sample)
    {
        bool result = false;
        constexpr size_t pc_size = sizeof(get_type<decltype(sample.pcs)>::type);
        size_t total_size = sizeof(sample.data) + sample.stack_size * pc_size;
        
        switch (sample.data.event_type)
        {
        case MemEventType::Malloc:
        {
            
            result = TryWrite(total_size, [&sample](RingBuffer *buffer, RingBuffer::Buffer *buf)
            {
                buffer->Put(buf->data, &sample.data, sizeof(sample.data));
                buffer->Put(buf->data+sizeof(sample.data), sample.pcs, sample.stack_size * pc_size);
            });
            break;
        }
        case MemEventType::Free:
        {
            result = TryWrite(total_size, [&sample](RingBuffer *buffer, RingBuffer::Buffer *buf)
            {
                buffer->Put(buf->data, &sample.data, sizeof(sample.data));
                if (!MemoryProfiler::lite_mode_) {
                    buffer->Put(buf->data+sizeof(sample.data), sample.pcs, sample.stack_size * pc_size);
                }
            });
            break;
        }
        }

        return result;
    }

    void MemoryProfiler::EnableProfiler(bool lite_mode, uint32_t interval,
                                        intptr_t period, intptr_t bg_period) {
        lite_mode_ = lite_mode;
        interval_ = interval;
        period_ = period * kMicrosPerMilli;
        bg_period_ = bg_period * kMicrosPerMilli;
        Init();
    }
    
    void MemoryProfiler::DisableProfiler() {
        Cleanup();
    }

    intptr_t MemoryProfiler::CalculateBufferSize()
    {
        if (period_ <= 0) {
            return 8 * MB;
        }
        return (sizeof(MemorySample) + sizeof(uword) * MemorySample::kAvgDepth) *
        MemorySample::kMaxThreads * process_period_ * kMicrosPerMilli / period_;
    }

    void MemoryProfiler::Init()
    {
        {
            ScopedUnfairlock l(profiler_lock_);
            
            if (initialized_)
                return;
            
            ASSERT(g_callstack_table != nullptr);
            
            if (sample_buffer_ == nullptr)
            {
                intptr_t size = CalculateBufferSize();
                sample_buffer_ = new MemorySampleBuffer(size);
            }
            
            initialized_ = true;
        }
        
        EventLoop::Register(process_period_, ProcessSampleBuffer);
        
        if (!malloc_logger)
        {
            malloc_logger = MemoryLogging;
        }
            
        if (!__syscall_logger)
        {
            __syscall_logger = MemoryLogging;
        }
    }

    void MemoryProfiler::Cleanup()
    {
        ScopedUnfairlock l(profiler_lock_);

        if (!initialized_)
            return;

        initialized_ = false;
        
        while (working_thread_ != 0 || s_main_running) {
            Utils::WaitFor(1);
        }
        
        ClearSamples();
    }

    void MemoryProfiler::ClearSamples()
    {
        ProcessCompletedSamples();
        delete sample_buffer_;
        sample_buffer_ = nullptr;
    }

    void MemoryProfiler::MemoryLogging(uint32_t type_flags,
                                       uintptr_t zone_ptr,
                                       uintptr_t arg2,
                                       uintptr_t arg3,
                                       uintptr_t return_val,
                                       uint32_t num_hot_to_skip)
    {
        uintptr_t size;
        uintptr_t ptr_arg;

        // check incoming data
        if (type_flags & stack_logging_type_alloc && type_flags & stack_logging_type_dealloc)
        {
            ptr_arg = arg2; // the original pointer
            size = arg3;
            if (ptr_arg == 0)
            { // realloc(NULL, size) same as malloc(size)
                type_flags ^= stack_logging_type_dealloc;
            }
            else
            {
                // realloc(arg1, arg2) -> result is same as free(arg1); malloc(arg2) -> result
                MemoryLogging(
                    stack_logging_type_dealloc, zone_ptr, ptr_arg, (uintptr_t)0, (uintptr_t)0, num_hot_to_skip + 1);
                MemoryLogging(stack_logging_type_alloc, zone_ptr, size, (uintptr_t)0, return_val, num_hot_to_skip + 1);
                return;
            }
        }

        if (type_flags & stack_logging_type_dealloc || type_flags & stack_logging_type_vm_deallocate)
        {
            // For VM deallocations we need to know the size, since they don't always match the
            // VM allocations.  It would be nice if arg2 was the size, for consistency with alloc and
            // realloc events.  However we can't easily make that change because all projects
            // (malloc.c, GC auto_zone, and gmalloc) have historically put the pointer in arg2 and 0 as
            // the size in arg3.  We'd need to change all those projects in lockstep, which isn't worth
            // the trouble.
            ptr_arg = arg2;
            size = arg3;
            if (ptr_arg == 0)
            {
                return; // free(nil)
            }
            RecordFree(ptr_arg, num_hot_to_skip + 1);
        }
        else if (type_flags & stack_logging_type_alloc || type_flags & stack_logging_type_vm_allocate)
        {
            if (return_val == 0 || return_val == (uintptr_t)MAP_FAILED)
            {
                return; // alloc that failed
            }
            size = arg2;
            RecordMalloc(return_val, size, num_hot_to_skip + 1);
        }
    }

    void MemoryProfiler::RecordMalloc(const uint64_t ptr, size_t size, int skip)
    {
        if (1 < interval_ && !Utils::HitSampling(ptr >> 4, interval_))
        {
            return;
        }
        
        if unlikely(!initialized_.load(std::memory_order_relaxed))
        {
            return;
        }
        
        auto p_tid = OSThread::PthreadSelf();
        bool is_main = (p_tid == OSThread::MainJoinId());
        
        if unlikely(OSThread::ThreadLogging(is_main))
        {
            return;
        }

        ScopedCounter counter(working_thread_, s_main_running, is_main);
        
        // double check
        if unlikely(!initialized_)
        {
            return;
        }

        OSThread::ScopedDisableLogging disable(is_main);
        
        auto os_thread = OSThread::TryCurrent(is_main);
        
        if (!os_thread) {
            return;
        }

#if DEBUG || INHOUSE_TARGET || TEST_MODE
#else
        if (1 == interval_)
#endif
        {
            os_thread->update_alloc_count(1);
            os_thread->update_alloc_size(size);
        }
        
        int64_t curr_time = Utils::GetCurrMachMicros();
        int64_t prev_access_time = os_thread->access_time();
        int64_t diff_time = curr_time - prev_access_time;
        intptr_t period = bg_period_;

        if unlikely(is_main) {
            period = period_;
        }
        
        if (diff_time < period) {
            return;
        }
        
        os_thread->set_access_time(curr_time);

        skip += 1;
        uword buffer[kMaxStackDepth];
        constexpr int max_size = sizeof(buffer) / sizeof(uword);
        uword* stack_top = (uword*)os_thread->stack_base();
        uword* stack_bot = (uword*)os_thread->stack_limit();
        PthreadStackRange stack_range(stack_bot, stack_top);
        int stack_size = Utils::ThreadStackPCs(buffer, max_size, skip, &stack_range);
        auto buffer_ptr = (uword*)buffer;
        
        MemorySample sample;
        sample.data.event_type = MemEventType::Malloc;
        sample.data.address = ptr;
        sample.data.timestamp = curr_time;
        sample.data.size = size;
        sample.data.tid = os_thread->id();
        sample.data.alloc_size = (uint32_t)(os_thread->alloc_size()/MB);
        sample.data.alloc_count = (uint32_t)(os_thread->alloc_count());
        sample.pcs = buffer_ptr;
        sample.stack_size = stack_size;
        
        uint32_t cpu_time = (uint32_t)(os_thread->update_cpu_time()/10);
        sample.data.cpu_time = cpu_time;
        
        sample_buffer_->PutSample(sample);
    }

    void MemoryProfiler::RecordFree(const uint64_t ptr, int skip)
    {        
        if (lite_mode_)
        {
            return;
        }
        
        if (1 < interval_ && !Utils::HitSampling(ptr >> 4, interval_))
        {
            return;
        }

        if unlikely(!initialized_.load(std::memory_order_relaxed))
        {
            return;
        }
        
        auto p_tid = OSThread::PthreadSelf();
        bool is_main = (p_tid == OSThread::MainJoinId());
        
        if unlikely(OSThread::ThreadLogging(is_main))
        {
            return;
        }

        ScopedCounter counter(working_thread_, s_main_running, is_main);
        
        // double check
        if unlikely(!initialized_)
        {
            return;
        }

        OSThread::ScopedDisableLogging disable(is_main);
        
        auto os_thread = OSThread::TryCurrent(is_main);
        
        if (!os_thread)
        {
            return;
        }

        if (1 == interval_)
        {
            os_thread->update_free_count(1);
        }

        int64_t curr_time = Utils::GetCurrMachMicros();
        
        MemorySample sample;
        sample.data.event_type = MemEventType::Free;
        sample.data.size = 0;
        sample.data.address = ptr;
        sample.data.timestamp = curr_time;
        sample.data.tid = os_thread->id();
        sample.data.alloc_size = (uint32_t)(os_thread->alloc_size()/MB);
        sample.data.alloc_count = (uint32_t)(os_thread->alloc_count());
        sample.pcs = nullptr;
        sample.stack_size = 0;
        sample.data.cpu_time = 0;
        
        OSThread *main_thread = OSThread::MainThread();
        int64_t prev_access_time = os_thread->access_time();
        int64_t diff_time = curr_time - prev_access_time;
        intptr_t period = bg_period_;
            
        if unlikely(is_main) {
            period = period_;
        }
        
        if (diff_time < period) {
            return;
        }
        
        os_thread->set_access_time(curr_time);

        skip += 1;
        uword buffer[kMaxStackDepth];
        constexpr int max_size = sizeof(buffer) / sizeof(uword);
        auto stack_top = (uword*)os_thread->stack_base();
        auto stack_bot = (uword*)os_thread->stack_limit();
        PthreadStackRange stack_range(stack_bot, stack_top);
        auto stack_size = Utils::ThreadStackPCs(buffer, max_size, skip, &stack_range);
        auto buffer_ptr = (uword*)buffer;
        auto cpu_time = (uint32_t)(os_thread->update_cpu_time()/10);
        
        sample.pcs = buffer_ptr;
        sample.stack_size = stack_size;
        sample.data.cpu_time = cpu_time;

        sample_buffer_->PutSample(sample);
    }

    void MemoryProfiler::ProcessSampleBuffer()
    {
        ScopedUnfairlock l(profiler_lock_);
        
        if (!initialized_)
        {
            return;
        }
        ProcessCompletedSamples();
    }

    void MemoryProfiler::ProcessCompletedSamples()
    {
        std::vector<MemRecord> processed_buffer;
        uint64_t nums = sample_buffer_->Iterate([&processed_buffer](RingBuffer *ring_buffer, size_t num){
            uword buffer[kMaxStackDepth];
            MemorySample sample;
            sample.pcs = buffer;
            if (!sample_buffer_->GetSample(ring_buffer, &sample))
            {
                return;
            }
            ProcessSample(sample, processed_buffer);
        });
        
        mem_profile_callback(processed_buffer);
    }

    void MemoryProfiler::ProcessSample(MemorySample &sample, std::vector<MemRecord>& processed_buffer)
    {
        uint32_t tid = sample.data.tid;
        uint32_t cpu_time = sample.data.cpu_time;
        uint64_t timestamp = sample.data.timestamp;
        auto relative_time = (uint32_t)((timestamp - start_mach_time)/10);
        uint64_t address = sample.data.address;
//        ASSERT(address <= (uint64_t)1 << 36);
        auto addr = (uint32_t)(address>>4);
        uint32_t alloc_size = sample.data.alloc_size;
        uint32_t alloc_count = sample.data.alloc_count;
        uint32_t size = UINT32_MAX<sample.data.size?UINT32_MAX:(uint32_t)sample.data.size;
        uint32_t stack_id = 0;
        
        if (sample.data.event_type == MemEventType::Malloc)
        {
            stack_id = g_callstack_table->insert(sample.pcs, sample.stack_size);
            
            if (!stack_id) {
                return;
            }
        }
        else if (sample.data.event_type == MemEventType::Free)
        {
            if (!lite_mode_) {
                stack_id = g_callstack_table->insert(sample.pcs, sample.stack_size);
                
                if (!stack_id) {
                    return;
                }
            }
        }
        
        processed_buffer.emplace_back(tid, addr, size, relative_time, cpu_time, stack_id,
                                      alloc_size, alloc_count);
    }
} // namespace profiling
