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
 *
 * Includes work from dart-lang/sdk (https://github.com/dart-lang/sdk)
 * with modifications.
 */

// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include <mach/mach.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <pthread/stack_np.h>

#include <memory>
#include <vector>

#if defined(__arm64__)
#include <mach/arm/thread_status.h>
#include <ptrauth.h>
#endif

#include "profiler.hpp"

#include "utils.hpp"
#include "monitor.hpp"
#include "ImageInfo.hpp"
#include "callstack_table.hpp"
#include "event_loop.hpp"


namespace btrace
{
    class Profile;

    static ProfileCallback profile_callback = nullptr;

// The layout of C stack frames.
#if defined(HOST_ARCH_IA32) || defined(HOST_ARCH_X64) || \
    defined(HOST_ARCH_ARM) || defined(HOST_ARCH_ARM64)
    //Higher address-> +------------------------+
    //                 |  ....................  |
    //caller's sp ---> +------------------------+
    //                 |return address (8 bytes)|
    //                 +------------------------+
    //                 |  caller's fp (8 bytes) |
    //         fp ---> +------------------------+
    //                 |                        |
    //                 |  ....................  |
    //                 |                        |
    //         sp ---> +------------------------+
    //                 |                        |
    //Lower address->  +------------------------+
#else
#error What architecture?
#endif

    class ProfilerStackWalker : public ValueObject
    {
    public:
        ProfilerStackWalker(Sample *sample,
                            OSThread *os_thread,
                            ThreadContext *threadContext,
                            bool fast_unwind = true,
                            intptr_t skip_count = 0)
            : sample_(sample),
              thread_(os_thread),
              stackbot_(os_thread->stack_limit_),
              stacktop_(os_thread->stack_base_),
              skip_count_(skip_count),
              frames_skipped_(0),
              frame_index_(0),
              threadContext_(threadContext),
              fast_unwind_(fast_unwind)
        {
            is_main_thread_ = (thread_ == OSThread::MainThread());
        }

        bool Append(uword pc, uword fp)
        {
            if (frames_skipped_ < skip_count_)
            {
                frames_skipped_++;
                return true;
            }

            if (frame_index_ >= kMaxStackDepth)
            {
                return false;
            }

            ASSERT(sample_ != nullptr);

            sample_->SetAt(frame_index_, pc);
            frame_index_++;
            return true;
        }

        void walk()
        {
            UnwindContext unwindContext;
            memset(reinterpret_cast<void *>(&unwindContext), 0, sizeof(UnwindContext));
            unwindContext.registers = *threadContext_;

            uword pc = 0;
            uword sp = UnwindGetStackPointer(&unwindContext);
            uword fp = UnwindGetFramePointer(&unwindContext);

            if (!GetAndValidateThreadStackBounds(fp, sp))
            {
                return;
            }

            if (!ValidFramePointer(reinterpret_cast<uword *>(fp))) {
                return;
            }
            
            while (UnwindNextFrame(&unwindContext))
            {
                pc = UnwindGetPC(&unwindContext);
                fp = UnwindGetFramePointer(&unwindContext);
                sp = UnwindGetStackPointer(&unwindContext);
                
                stackbot_ = sp;

                if (!ValidFramePointer(reinterpret_cast<uword *>(fp))) {
                    break;
                }

                const uword pc_value = pc;
                if ((pc_value + 1) < pc_value)
                {
                    // It is not uncommon to encounter an invalid pc as we
                    // traverse a stack frame.  Most of these we can tolerate.  If
                    // the pc is so large that adding one to it will cause an
                    // overflow it is invalid and it will cause headaches later
                    // while we are building the profile.  Discard it.
                    break;
                }
                
                if (!Append(pc_value, fp))
                {
                    break;
                }
            }
        }

    protected:
        bool fast_unwind_;
        bool is_main_thread_;
        Sample *sample_;
        OSThread *thread_;
        uword stacktop_;
        uword stackbot_;
        intptr_t skip_count_;
        intptr_t frames_skipped_;
        intptr_t frame_index_;

    private:
        bool ValidFramePointer(uword* fp) const {
            if (fp == nullptr) {
                return false;
            }
            uword cursor = reinterpret_cast<uword>(fp);
            bool r = (cursor >= stackbot_ &&
                      cursor <= stacktop_ - sizeof(uintptr_t) * 2 &&
                      ISALIGNED(cursor));
            return r;
        }
        
        bool ValidateThreadStackBounds(uintptr_t fp, uintptr_t sp)
        {
            if (stackbot_ >= stacktop_)
            {
                // Stack boundary is invalid.
                return false;
            }

            if ((sp < stackbot_) || (sp >= stacktop_))
            {
                // Stack pointer is outside thread's stack boundary.
                return false;
            }

            if ((fp < stackbot_) || (fp >= stacktop_))
            {
                // Frame pointer is outside threads's stack boundary.
                return false;
            }

            return true;
        }

        bool GetAndValidateThreadStackBounds(uintptr_t fp, uintptr_t sp)
        {
            stackbot_ = thread_->stack_limit();
            stacktop_ = thread_->stack_base();

            if ((stackbot_ == 0) || (stacktop_ == 0))
            {
                return false;
            }

            if (sp > stackbot_)
            {
                // The stack pointer gives us a tighter lower bound.
                stackbot_ = sp;
            }

            return ValidateThreadStackBounds(fp, sp);
        }
        
        bool UnwindNextFrameUsingAllStrategies(UnwindContext *context)
        {
            if (!IsValidPointer(context))
            {
                //                Utils::Print("Arguments invalid\n");
                return false;
            }

            uintptr_t pc = UnwindGetPC(context);

            // Ok, what's going on here? libunwind's UnwindCursor<A,R>::setInfoBasedOnIPRegister has a
            // parameter that, if true, does this subtraction. Despite the comments in the code
            // (of 35.1), I found that the parameter was almost always set to true.
            //
            // I then ran into a problem when unwinding from _pthread_start -> thread_start. This
            // is a common transition, which happens in pretty much every report. An extra frame
            // was being generated, because the PC we get for _pthread_start was mapping to exactly
            // one greater than the function's last byte, according to the compact unwind info. This
            // resulted in using the wrong compact encoding, and picking the next function, which
            // turned out to be dwarf instead of a frame pointer.

            // So, the moral is - do the subtraction for all frames except the first. I haven't found
            // a case where it produces an incorrect result. Also note that at first, I thought this would
            // subtract one from the final addresses too. But, the end of this function will *compute* PC,
            // so this value is used only to look up unwinding data.

            if (context->frameCount > 0)
            {
                --pc;
                if (!ThreadContextSetPC(&context->registers, pc))
                {
                    return false;
                }
            }

            if (!IsValidPointer(pc))
            {
                return false;
            }

            // the first frame is special - as the registers we need
            // are already loaded by definition
            if (context->frameCount == 0)
            {
                return true;
            }

            // If the frame pointer is zero, we cannot use an FP-based unwind and we can reasonably
            // assume that we've just gotten to the end of the stack.
            if (UnwindGetFramePointer(context) == 0)
            {
                // make sure to set the PC to zero, to indicate the unwind is complete
                return ThreadContextSetPC(&context->registers, 0);
            }

            // Only allow stack scanning (as a last resort) if we're on the first frame. All others
            // are too likely to screw up.
            if (UnwindWithFramePointer(&context->registers, context->frameCount == 1))
            {
                return true;
            }

            return false;
        }

        bool UnwindWithLRRegister(ThreadContext *registers)
        {
            if (!IsValidPointer(registers))
            {
                return false;
            }

            // Return address is in LR, SP is pointing to the next frame.
            uintptr_t value = ThreadContextGetLinkRegister(registers);

            if (!IsValidPointer(value))
            {
                //                Utils::Print("Error: LR value is invalid\n");
                return false;
            }

            return ThreadContextSetPC(registers, value);
        }

        uintptr_t ThreadContextGetStackPointer(const ThreadContext *registers)
        {
            if (!registers)
            {
                return 0;
            }

            return GET_SP_REGISTER(registers);
        }

        bool UnwindWithFramePointer(ThreadContext *registers, bool allowScanning)
        {
            if (allowScanning)
            {
                // The LR register does have the return address here, but there are situations where
                // this can produce false matches. Better backend rules can fix this up in many cases.
                if (UnwindWithLRRegister(registers))
                {
                    return true;
                }
            }

            const uintptr_t prevSP = ThreadContextGetStackPointer(registers);
            // read the values from the stack
            const uintptr_t prevFP = ThreadContextGetFramePointer(registers);
            uintptr_t stack[2];

            if (!ReadNextStackPCAndFP(prevFP, stack))
            {
                // unable to read the first stack frame
                return false;
            }
            
            uword fp = stack[0];
            uword pc = stack[1];
            uword sp = UnwindStackPointerFromFramePointer(prevFP);
            
            if (fp <= prevFP || sp <= prevSP) {
                return false;
            }

            if (!ThreadContextSetPC(registers, pc))
            {
                return false;
            }

            if (!ThreadContextSetFramePointer(registers, fp))
            {
                return false;
            }
            
            if (!ThreadContextSetStackPointer(registers, sp)) {
              return false;
            }

            return true;
        }

        bool ReadNextStackPCAndFP(uword frame, uword *dest) {
            // Enable fast_unwind_ only if 3 < frame_index_. When pthread is exiting, the thread's stack will be cleaned
            // and the valid call chain will be '_pthread_terminate_invoke->_pthread_terminate->__bsdthread_terminate'.
            // and the upper address in this stack will be invalid. So it is dangerous to unwind stack using fast unwind mode
            // when pthread is exiting, and we must use vm_read_overwrite to unwind stack of the exiting thread.
            // If 3 < frame_index_, we believe that thread is not exiting and it is safe to unwind stack by using fast unwind mode.
            if (fast_unwind_) {
                return UnsafeReadNextStackPCAndFP(frame, dest);
            } else if (is_main_thread_ || 3 < frame_index_) {
                return UnsafeReadNextStackPCAndFP(frame, dest);
            }
            return ReadMemory(frame, dest, 2 * sizeof(uword));
        }

        bool UnsafeReadNextStackPCAndFP(uword frame, uword *dest)
        {
            // The stack layout must remain unchanged when calling this method!!!
            if (!IsValidPointer(frame))
            {
                return false;
            }
            
            uword next = 0;
            uword retaddr = 0;
//            if (__builtin_available(iOS 12, *)) {
//                next = pthread_stack_frame_decode_np(frame, &retaddr);
//            } else {
                retaddr = *(uword *)(((uword **)frame) + 1);
                next = (uword) * (uword **)frame;
//            }

            dest[0] = next;
            dest[1] = retaddr;

            return true;
        }

        bool ReadMemory(vm_address_t src, void *dest, size_t len)
        {
            if (!IsValidPointer(src))
            {
                return false;
            }

            vm_size_t readSize = len;
            kern_return_t res = vm_read_overwrite(mach_task_self(), src, len, (pointer_t)dest, &readSize);

            if (res != KERN_SUCCESS)
            {
                return false;
            }

            return true;
        }
        
        uintptr_t UnwindStackPointerFromFramePointer(uintptr_t framePtr) {
          // the stack pointer is the frame pointer plus the two saved pointers for the frame
          return framePtr + 2 * sizeof(void*);
        }
        
        bool ThreadContextSetStackPointer(ThreadContext *registers, uintptr_t value)
        {
            if (!IsValidPointer(registers))
            {
                return false;
            }

            SET_SP_REGISTER(registers, value);

            return true;
        }

        bool ThreadContextSetFramePointer(ThreadContext *registers, uintptr_t value)
        {
            if (!IsValidPointer(registers))
            {
                return false;
            }

            SET_FP_REGISTER(registers, value);

            return true;
        }

        uintptr_t ThreadContextGetFramePointer(const ThreadContext *registers)
        {
            if (!IsValidPointer(registers))
            {
                return 0;
            }

            return GET_FP_REGISTER(registers);
        }

        uintptr_t ThreadContextGetLinkRegister(const ThreadContext *registers)
        {
            if (!IsValidPointer(registers))
            {
                return 0;
            }

            return GET_LR_REGISTER(registers);
        }

        bool ThreadContextSetPC(ThreadContext *registers, uintptr_t value)
        {
            if (!registers)
            {
                return false;
            }

            SET_IP_REGISTER(registers, value);

            return true;
        }

        uintptr_t UnwindGetPC(UnwindContext *context)
        {
            if (!IsValidPointer(context))
            {
                return 0;
            }
            const ThreadContext *registers = &context->registers;

            if (!registers)
            {
                return 0;
            }

            return GET_IP_REGISTER(registers);
        }

        uintptr_t UnwindGetStackPointer(UnwindContext *context)
        {
            if (!IsValidPointer(context))
            {
                return 0;
            }

            const ThreadContext *registers = &context->registers;

            if (!registers)
            {
                return 0;
            }

            return GET_SP_REGISTER(registers);
        }

        static uintptr_t UnwindGetFramePointer(UnwindContext *context)
        {
            if (!IsValidPointer(context))
            {
                return 0;
            }

            const ThreadContext *registers = &context->registers;

            if (!IsValidPointer(registers))
            {
                return 0;
            }

            return GET_FP_REGISTER(registers);
        }

        inline bool UnwindContextHasValidPCAndSP(UnwindContext *context)
        {
            return IsValidPointer(UnwindGetPC(context)) &&
                   IsValidPointer(UnwindGetStackPointer(context));
        }

        bool UnwindNextFrame(UnwindContext *context)
        {
            if (!UnwindContextHasValidPCAndSP(context))
            {
                // This is a special-case. It is possible to try to unwind a thread that has no stack (ie, is
                // executing zero functions. I believe this happens when a thread has exited, but before the
                // kernel has actually cleaned it up. This situation can only apply to the first frame. So, in
                // that case, we don't count it as an error. But, if it happens mid-unwind, it's a problem.

                if (context->frameCount == 0)
                {
                    return false;
                }

                return false;
            }

            if (!UnwindNextFrameUsingAllStrategies(context))
            {
                return false;
            }

            uintptr_t pc = UnwindGetPC(context);

            // Unwinding will complete when this is no longer a valid value
            if (!IsValidPointer(pc))
            {
                return false;
            }

            context->frameCount += 1;
            context->lastFramePC = pc;

            return true;
        }

        ThreadContext *threadContext_;
    };

    void Profiler::Init()
    {
        if (profiler_lock_ == nullptr) {
            // do not delete profiler_lock_
            profiler_lock_ = new std::mutex();
        }
        std::lock_guard<std::mutex> ml(*profiler_lock_);

        if (initialized_)
        {
            return;
        }

        if (monitor_ == nullptr)
        {
            monitor_ = new Monitor();
        }
        ASSERT(monitor_ != nullptr);
        
        ASSERT(g_callstack_table != nullptr);

        Utils::Print("Start Profiling...");

        SetSamplePeriod(profile_period_);
        // The profiler may have been shutdown previously, in which case the sample
        // buffer will have already been initialized.
        if (sample_buffer_ == nullptr)
        {
            constexpr uintptr_t sample_size = sizeof(Sample) + sizeof(uword) * Sample::kAvgDepth;
            uintptr_t buffer_size = CalculateCapacity() * sample_size;
            sample_buffer_ = new SampleBuffer(buffer_size);
        }

        ThreadInterrupter::Init();
        ThreadInterrupter::Startup();
        EventLoop::Register(kMainUpdatePeriod, UpdateThreadState);
        EventLoop::Register(process_period_, ProcessSampleBuffer);
        initialized_ = true;
    }

    void Profiler::Cleanup()
    {
        std::lock_guard<std::mutex> ml(*profiler_lock_);

        if (!initialized_)
        {
            return;
        }

        Utils::Print("Stop Profiling...");
        
        ThreadInterrupter::Cleanup();

        ClearSamples();

        {
            MonitorLocker timeout_ml(Profiler::monitor_);
            // Notify.
            timeout_ml.Notify();
        }

        initialized_ = false;
        // Do not delete monitor_, otherwise the timeout thread will crash
//        delete monitor_;
//        monitor_ = nullptr;
        Zone::Stop();
    }

    intptr_t Profiler::CalculateCapacity()
    {
        return Sample::kMaxThreads * process_period_ / profile_period_;
    }

    void Profiler::SetSamplePeriod(intptr_t period)
    {
        const int kMinimumProfilePeriod = 1;
        if (period < kMinimumProfilePeriod)
        {
            profile_period_ = kMinimumProfilePeriod;
        }
        else
        {
            profile_period_ = period;
        }
        ThreadInterrupter::SetPeriod(profile_period_, profile_bg_period_);
    }

    void Profiler::ClearSamples()
    {
        Profiler::ProcessCompletedSamples();
        delete sample_buffer_;
        sample_buffer_ = nullptr;
    }

    void Profiler::UpdateThreadState()
    {
        static uint64_t count = 1;
        
        constexpr intptr_t interval = kBgUpdatePeriod / kMainUpdatePeriod;
        
        OSThreadIterator it;
        while (it.HasNext())
        {
            OSThread *t = it.Next();
            
            if (t != OSThread::MainThread() &&
                !Utils::HitSampling(count, interval)) {
                continue;
            }
            
            t->set_has_sampled(false);
            
            if (t->apple_thread()) {
                t->set_cpu_usage(0);
            } else {
                thread_basic_info_data_t info;
                t->BasicInfo(&info);
                t->set_cpu_usage(info.cpu_usage / (float)TH_USAGE_SCALE);
            }
        }
        
        count += 1;
    }

    bool SampleBuffer::PutSample(Sample &sample) {
        ASSERT(sample.pcs_ != nullptr);
        
        constexpr size_t pc_size = sizeof(get_type<decltype(sample.pcs_)>::type);
        size_t total_size = sizeof(sample.data_) + sample.size_ * pc_size;
        
        RingBuffer::Buffer buf = BeginWrite(total_size);
        
        if (unlikely(!buf))
        {
            EndWrite(std::move(buf));
            return false;
        }
        
        Put(buf.data, &sample.data_, sizeof(sample.data_));
        Put(buf.data + sizeof(sample.data_), sample.pcs_, sample.size_ * pc_size);
        
        EndWrite(std::move(buf));
        
        return true;
    }

    bool SampleBuffer::GetSample(Sample *sample) {
        ASSERT(sample->pcs_ != nullptr);
        
        bool result = false;

        RingBuffer::Buffer buffer;
        buffer = BeginRead();
        
        if (buffer)
        {
            result = GetSampleFromBuffer(&buffer, sample);
        }

        EndRead(std::move(buffer));

        return result;
    }

    bool SampleBuffer::GetSampleFromBuffer(RingBuffer::Buffer *buffer, Sample *out) {
        char *buf = reinterpret_cast<char *>(buffer->data);
        size_t size = buffer->size;
        char *end = buf + size;
        
        decltype(out->data_) *data;

        if (!ViewAndAdvance<decltype(out->data_)>(&buf, &data, end))
        {
            return false;
        }
        Get(&out->data_, data, sizeof(out->data_));

        if (buf > end)
        {
            return false;
        }
        Get(out->pcs_, buf, static_cast<size_t>(end - buf));
        constexpr size_t pc_size = sizeof(get_type<decltype(out->pcs_)>::type);
        out->size_ = (uint32_t)((size_t)(end - buf) / pc_size);
        
        return true;
    }

    ProcessedSampleBuffer *SampleBuffer::BuildProcessedSampleBuffer(
        ProcessedSampleBuffer *buffer)
    {
        Zone *zone = OSThread::zone();

        if (buffer == nullptr)
        {
            buffer = new (zone) ProcessedSampleBuffer();
        }

        uintptr_t sample_num = nums();

        for (intptr_t i = 0; i < sample_num; ++i)
        {
            Sample sample;
            uword stack_buffer[kMaxStackDepth];
            sample.SetPCArray(stack_buffer);
            if (!GetSample(&sample)) {
                continue;
            }
            sample.BuildProcessedSampleBuffer(buffer);
        }

        return buffer;
    }

    void Profiler::SampleThread(OSThread *os_thread,
                                ThreadContext &thread_state)
    {
        ASSERT(os_thread != nullptr);

        SampleBuffer *sample_buffer = Profiler::sample_buffer();
        // Setup sample.
        uword buffer[kMaxStackDepth];
        uint64_t curr_time = Utils::GetCurrMachMicros();
        uint32_t alloc_size = (uint32_t)(os_thread->alloc_size()/MB);
        uint32_t alloc_count = (uint32_t)os_thread->alloc_count();
        os_thread->set_access_time(curr_time);
        Sample sample = Sample(curr_time, os_thread->id(), os_thread->cpu_time()/10,
                               alloc_size, alloc_count);
        sample.SetPCArray(buffer);

        bool fast_unwind = Profiler::fast_unwind_;
        ProfilerStackWalker stack_walker(&sample, os_thread, &thread_state, fast_unwind);

        stack_walker.walk();
        
        if (unlikely(0 == sample.size() || sample.At(0) == 0)) {
            return;
        }
        
        bool success = sample_buffer->PutSample(sample);
        if (likely(success)) {
//            os_thread->set_stack(sample.GetPCArray(), sample.size());
            os_thread->set_has_sampled(true);
        }
//        else {
//            // ONLY FOR DEBUG, OTHERWISE DO NOT PRINT IN THE SAMPLING PROCESS
//            // Utils::PrintErr("sample buffer is empty!");
//        }
    }

    Sample::Sample(int64_t timestamp, ThreadId tid, uint32_t cpu_time,
                   uint32_t alloc_size, uint32_t alloc_count)
    {
        data_.timestamp = timestamp;
        data_.tid = tid;
        data_.cpu_time = cpu_time;
        data_.alloc_size = alloc_size;
        data_.alloc_count = alloc_count;
        size_ = 0;
    }

    ProcessedSampleBuffer *Sample::BuildProcessedSampleBuffer(
        ProcessedSampleBuffer *buffer)
    {
        Zone *zone = OSThread::zone();

        if (buffer == nullptr)
        {
            buffer = new (zone) ProcessedSampleBuffer();
        }

        if (data_.timestamp == 0)
        {
            // Empty.
            return buffer;
        }
        
        ProcessedSample *sample = BuildProcessedSample();
        if (sample == nullptr) {
            return buffer;
        }
        
        buffer->Add(sample);

        return buffer;
    }

    ProcessedSample *Sample::BuildProcessedSample()
    {
        uint32_t stack_id = g_callstack_table->insert(pcs_, size_);

        if (stack_id == 0) {
            return nullptr;
        }
        
        Zone *zone = OSThread::zone();

        ProcessedSample *processed_sample = new (zone) ProcessedSample();

        // Copy state bits from sample.
        processed_sample->set_timestamp(data_.timestamp);
        processed_sample->set_tid(data_.tid);
        processed_sample->set_cpu_time(data_.cpu_time);
        processed_sample->set_stack_id(stack_id);
        processed_sample->set_alloc_size(data_.alloc_size);
        processed_sample->set_alloc_count(data_.alloc_count);

        return processed_sample;
    }

    ProcessedSample::ProcessedSample()
        : timestamp_(0) {}

    ProcessedSampleBuffer::ProcessedSampleBuffer() {}

    void Profiler::SetProfileCallback(ProfileCallback callback)
    {
        profile_callback = callback;
    }

    void Profiler::ProcessSampleBuffer()
    {
        std::lock_guard<std::mutex> ml(*profiler_lock_);

        if (!initialized_)
        {
            return;
        }
        ProcessCompletedSamples();
    }

    void Profiler::ProcessCompletedSamples()
    {
        Profile profile;
        StackZone zone;
        profile.Build(Profiler::sample_buffer());

        if (profile_callback)
        {
            profile_callback(&profile);
        }
    }

    void Profiler::EnableProfiler(intptr_t profile_period, intptr_t profile_bg_period,
                                  bool main_thread_only, bool active_thread_only,
                                  bool fast_unwind)
    {
        profile_period_ = profile_period;
        profile_bg_period_ = profile_bg_period;
        main_thread_only_ = main_thread_only;
        active_thread_only_ = active_thread_only;
        fast_unwind_ = fast_unwind;
        Init();
    }

    void Profiler::DisableProfiler()
    {
        Profiler::Cleanup();
    }

    class ProfileBuilder : public ValueObject
    {
    public:
        ProfileBuilder(SampleBuffer *sample_buffer,
                       Profile *profile)
            : sample_buffer_(sample_buffer),
              profile_(profile),
              samples_(nullptr)
        {
            ASSERT(profile_ != nullptr);
        }

        void Build()
        {
            samples_ = sample_buffer_->BuildProcessedSampleBuffer();
            profile_->samples_ = samples_;
            profile_->sample_count_ = samples_->length();
        }

        SampleBuffer *sample_buffer_;
        Profile *profile_;
        ProcessedSampleBuffer *samples_;
    }; // ProfileBuilder.

    Profile::Profile()
        : samples_(nullptr),
          sample_count_(0) {}

    void Profile::Build(SampleBuffer *sample_buffer)
    {
        ProfileBuilder builder(sample_buffer, this);
        builder.Build();
    }

    ProcessedSample *Profile::SampleAt(intptr_t index)
    {
        ASSERT(index >= 0);
        ASSERT(index < sample_count_);
        return samples_->At(index);
    }

} // namespace btrace
