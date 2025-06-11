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

//
//  BTraceDispatchProfilerPlugin.m
//  Pods-PerfAnalysis_Core
//
//  Created by ByteDance on 2024/11/6.
//

#import <mach-o/dyld.h>
#import <Foundation/Foundation.h>

#include <memory>
#include <unordered_map>
#include <vector>

#import "BTrace.h"
#import "InternalUtils.h"
#import "BTraceDispatchProfilerConfig.h"
#import "BTraceDispatchProfilerPlugin.h"

#include "BTraceDataBase.hpp"
#include "BTraceRecord.hpp"

#include "utils.hpp"
#include "os_thread.hpp"
#include "event_loop.hpp"
#include "concurrentqueue.hpp"
#include "callstack_table.hpp"

#define DISPATCH_BLOCK_PRIVATE_DATA_MAGIC 0xD159B10C // 0xDISPatch_BLOCk

typedef unsigned long pthread_priority_t;

#define DISPATCH_BLOCK_API_MASK (0x100u - 1)
#define DISPATCH_BLOCK_HAS_VOUCHER (1u << 31)
#define DISPATCH_BLOCK_HAS_PRIORITY (1u << 30)

#define DISPATCH_BLOCK_PRIVATE_DATA_HEADER()                                   \
    unsigned long dbpd_magic;                                                  \
    dispatch_block_flags_t dbpd_flags;                                         \
    unsigned int volatile dbpd_atomic_flags;                                   \
    int volatile dbpd_performed;                                               \
    pthread_priority_t dbpd_priority;                                          \
    void *dbpd_voucher;                                                        \
    dispatch_block_t dbpd_block;                                               \
    dispatch_group_t dbpd_group;                                               \
    dispatch_queue_t dbpd_queue;                                               \
    mach_port_t dbpd_thread;

constexpr static int BUFFER_MAX_SIZE = 16 * KB;

namespace btrace {

    extern uint8_t trace_id;
    extern int64_t start_mach_time;
    
    static std::atomic<bool> s_running = false;
    static std::atomic<bool> s_main_running = false;
    static std::atomic<uint32_t> s_working_thread = 0;
    static BTraceDispatchProfilerConfig *s_config = nullptr;
    static std::atomic<int32_t> *s_buffer_size = nullptr;
    static int s_period = 200; // ms

#pragma pack(push, 4)
    struct DispatchData {
        uint32_t source_tid;
        uint32_t source_time;
        uint32_t source_cpu_time;
        uint32_t target_tid;
        uint32_t target_time;
        uint32_t alloc_size;
        uint32_t alloc_count;
        
        DispatchData(uint32_t source_tid, uint32_t source_time,
                  uint32_t source_cpu_time, uint32_t target_tid,
                  uint32_t target_time, uint32_t alloc_size,
                  uint32_t alloc_count)
        : source_tid(source_tid), source_time(source_time),
        source_cpu_time(source_cpu_time), target_tid(target_tid),
        target_time(target_time), alloc_size(alloc_size),
        alloc_count(alloc_count) {}
    };
#pragma pack(pop)
    
    struct DispatchSample {
        
#if DEBUG || INHOUSE_TARGET || TEST_MODE
    static constexpr uintptr_t kAvgDepth = 32;
    static constexpr uintptr_t kMaxThreads = 64;
#else
    static constexpr uintptr_t kAvgDepth = 16;
    static constexpr uintptr_t kMaxThreads = 64;
#endif

        DispatchData data;
        uint32_t stack_size = 0;
        uword *pcs = nullptr;
        
        DispatchSample()
            : data(0, 0, 0, 0, 0, 0, 0),
            stack_size(0), pcs(nullptr) {}
        
        DispatchSample(uint32_t source_tid, uint32_t source_time,
                    uint32_t source_cpu_time, uint32_t target_tid,
                    uint32_t target_time, uint32_t stack_size, uword *pcs,
                    uint32_t alloc_size, uint32_t alloc_count)
            : data(source_tid, source_time, source_cpu_time,
                   target_tid, target_time, alloc_size, alloc_count),
            stack_size(stack_size), pcs(pcs) {}
    };

    class DispatchSampleBuffer : public ConcurrentRingBuffer
    {
    public:
        DispatchSampleBuffer(uintptr_t size) : ConcurrentRingBuffer(size, 32) {}
        
        bool GetSample(RingBuffer *ring_buffer, DispatchSample *sample);
        
        bool PutSample(DispatchSample &sample);
        
    private:
        bool GetSampleFromBuffer(RingBuffer *buffer, RingBuffer::Buffer *buf, DispatchSample *out);
    };
    
    static DispatchSampleBuffer *s_buffer;

    static uintptr_t CalculateBufferSize(int process_period, int period);
    
    static void ProcessDispatchSampleBuffer();

    static void ProcessSamples();
    
    struct BlockLiteral {
        void *isa; // initialized to &_NSConcreteStackBlock or &_NSConcreteGlobalBlock
        int flags;
        int reserved;
        void (*invoke)(void *, ...);
        struct block_descriptor {
            unsigned long int reserved;    // NULL
            unsigned long int size;         // sizeof(struct Block_literal_1)
            // optional helper functions
            void (*copy_helper)(void *dst, void *src);     // IFF (1<<25)
            void (*dispose_helper)(void *src);             // IFF (1<<25)
            // required ABI.2010.3.16
            const char *signature;                         // IFF (1<<30)
        } *descriptor;
        // imported variables
    };
    
    struct dispatch_block_private_data_s {
        DISPATCH_BLOCK_PRIVATE_DATA_HEADER();
    };
    
    typedef struct dispatch_block_private_data_s *dispatch_block_private_data_t;
    
    static dispatch_block_private_data_t get_block_private_data(dispatch_block_t block) {
        struct BlockLiteral *blockRef = (__bridge struct BlockLiteral *)block;
        uint8_t *b = (uint8_t *)blockRef;
        b += sizeof(struct BlockLiteral);
        return (dispatch_block_private_data_t)b;
    }
    
    /// 判断当前block的函数指针是否等于_dispatch_block_special_invoke
    static void *dispatch_block_special_invoke;
    BOOL init_dispatch_block_special_invoke(void)
    {
        dispatch_block_t block = dispatch_block_create((dispatch_block_flags_t)0, ^{});
        struct BlockLiteral *blockRef = (__bridge struct BlockLiteral *)block;
        
        dispatch_block_special_invoke = (void *)blockRef->invoke;
        
        dispatch_block_private_data_t dbpd = get_block_private_data(block);
        
        if (dbpd->dbpd_magic == DISPATCH_BLOCK_PRIVATE_DATA_MAGIC) {
            return YES;
        }
        return NO;
    }
    
    static BOOL dispatch_block_has_private_data(dispatch_block_t block)
    {
        struct BlockLiteral *blockRef = (__bridge struct BlockLiteral *)block;
        if (blockRef->invoke == dispatch_block_special_invoke) {
            dispatch_block_private_data_t dbpd = get_block_private_data(block);
            if (dbpd->dbpd_magic == DISPATCH_BLOCK_PRIVATE_DATA_MAGIC) {
                return YES;
            }
        }
        
        return NO;
    }
} // namespace btrace

@interface BTraceDispatchProfilerPlugin ()

@end

static dispatch_block_t dispatch_hook_block(dispatch_queue_t queue,
                                            dispatch_block_t block) {
    if (block == NULL) {
        return NULL;
    }

    if unlikely(!s_running.load(std::memory_order_relaxed)) {
        return block;
    }
    
    auto p_tid = OSThread::PthreadSelf();
    bool is_main = (p_tid == OSThread::MainJoinId());
    
    if unlikely(OSThread::ThreadLogging(is_main))
    {
        return block;
    }

    ScopedCounter counter(s_working_thread, s_main_running, is_main);

    if unlikely(!s_running) {
        return block;
    }

    if (BUFFER_MAX_SIZE < s_buffer_size->load()) {
        return block;
    }
    
    OSThread::ScopedDisableLogging disable(is_main);

    OSThread *os_thread = OSThread::Current();
    
    int64_t access_time = os_thread->access_time();
    int64_t curr_time = Utils::GetCurrMachMicros();
    int64_t diff_time = curr_time - access_time;
    
    auto *main_thread = OSThread::MainThread();
    auto period = s_config.bg_period;
    
    if unlikely(os_thread == main_thread) {
        period = s_config.period;
    }

    if (diff_time < period) {
        return block;
    }

    s_buffer_size->fetch_add(1);

    os_thread->set_access_time(curr_time);
    mach_port_t source_tid = os_thread->id();
    uint32_t source_time = (uint32_t)((curr_time - start_mach_time)/10);
    uint32_t source_cpu_time = (uint32_t)(os_thread->update_cpu_time()/10);
    uword* stack_top = (uword*)os_thread->stack_base();
    uword* stack_bot = (uword*)os_thread->stack_limit();
    PthreadStackRange stack_range(stack_bot, stack_top);
    SharedStack stack = Utils::GetThreadStack(0, &stack_range);
    auto alloc_size = (uint32_t)(os_thread->alloc_size()/MB);
    auto alloc_count = os_thread->alloc_count();

    dispatch_block_t hook_block = ^{
      int64_t curr_time = Utils::GetCurrMachMicros();

      block();

      if unlikely(!s_running.load(std::memory_order_relaxed)) {
          return;
      }
        
      auto p_tid = OSThread::PthreadSelf();
      bool is_main = (p_tid == OSThread::MainJoinId());

      ScopedCounter counter(s_working_thread, s_main_running, is_main);

      if unlikely(!s_running) {
          return;
      }

      OSThread::ScopedDisableLogging disable(is_main);

      mach_port_t target_tid = OSThread::GetCurrentThreadId();
      uint32_t target_time = (uint32_t)((curr_time - start_mach_time)/10);

      auto sample = DispatchSample(source_tid, source_time, source_cpu_time,
                                target_tid, target_time, stack->size(), stack->data(),
                                alloc_size, alloc_count);
      s_buffer->PutSample(sample);
    };

    if (dispatch_block_has_private_data(block)) {
        dispatch_block_private_data_t dbpd = get_block_private_data(block);
        hook_block = dispatch_block_create(
            (dispatch_block_flags_t)(dbpd->dbpd_flags &
                                     DISPATCH_BLOCK_API_MASK),
            hook_block);
        dispatch_block_private_data_t new_dbpd =
            get_block_private_data(hook_block);
        new_dbpd->dbpd_priority = dbpd->dbpd_priority;

        if (dbpd->dbpd_flags & DISPATCH_BLOCK_HAS_PRIORITY) {
            auto flag = (dispatch_block_flags_t)(new_dbpd->dbpd_flags | DISPATCH_BLOCK_HAS_PRIORITY);
            new_dbpd->dbpd_flags = flag;
        }
    }

end:
    hook_block = [hook_block copy];
    return hook_block;
}

BTRACE_HOOK(void, dispatch_async, dispatch_queue_t queue,
               dispatch_block_t block) {
    dispatch_block_t hook_block = dispatch_hook_block(queue, block);
    ORI(dispatch_async)(queue, hook_block);
}

__attribute__((objc_direct_members))
@implementation BTraceDispatchProfilerPlugin

+ (NSString *)name {
    return @"dispatchprofiler";
}

+ (instancetype)shared {
    static BTraceDispatchProfilerPlugin *instance = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      instance = [[self alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];

    s_running = false;

    [self dispatchAsyncFishhook];

    return self;
}

- (void)start {
    if (!self.btrace.enable) {
        return;
    }
    
    if (s_running) {
        return;
    }

    if (!self.config) {
        self.config = [BTraceDispatchProfilerConfig defaultConfig];
    }

    if (![self initTable]) {
        return;
    }

    auto config = (BTraceDispatchProfilerConfig *)self.config;
    
    if (s_buffer_size == nullptr) {
        s_buffer_size = new std::atomic<int32_t>();
    }

    if (s_buffer == nullptr) {
        auto size = CalculateBufferSize(s_period * kMicrosPerMilli, config.period);
        s_buffer = new DispatchSampleBuffer(size);
    }

    s_config = (BTraceDispatchProfilerConfig *)self.config;
    s_running = true;

    EventLoop::Register(s_period, ProcessDispatchSampleBuffer);
}

- (void)dump {
    if (!self.btrace.enable) {
        return;
    }
    
    if (!s_running) {
        return;
    }
    
    ProcessDispatchSampleBuffer();
}

- (void)stop {
    if (!self.btrace.enable) {
        return;
    }
    
    if (!s_running) {
        return;
    }

    s_running = false;

    while (s_working_thread != 0 || s_main_running) {
        Utils::WaitFor(1);
    }

    ProcessSamples();

    delete s_buffer;
    s_buffer = nullptr;
    
    delete s_buffer_size;
    s_buffer_size = nullptr;
}

- (bool)initTable {
    bool result = false;

    if (0 < self.btrace.bufferSize) {
        result = self.btrace.db->drop_table<DispatchBatchSampleModel>();

        if (!result) {
            return result;
        }

        result = self.btrace.db->create_table<DispatchBatchSampleModel>();

        if (!result) {
            return result;
        }
    } else {
        result = self.btrace.db->drop_table<DispatchSampleModel>();

        if (!result) {
            return result;
        }

        result = self.btrace.db->create_table<DispatchSampleModel>();

        if (!result) {
            return result;
        }
    }

    return result;
}

- (void)dispatchAsyncFishhook {
    if (!init_dispatch_block_special_invoke()) {
        return;
    }

    dispatch_async(self.btrace.queue, ^{
      for (uint32_t i = 0; i < _dyld_image_count(); i++) {
          struct rebinding r[] = {
              REBINDING(dispatch_async)
          };
          rebind_symbols_image((void *)_dyld_get_image_header(i),
                                  _dyld_get_image_vmaddr_slide(i), r,
                                  sizeof(r) / sizeof(struct rebinding));
      }
    });
}

@end

namespace btrace {

uintptr_t CalculateBufferSize(int process_period, int period)
{
    if (period <= 0) {
        return 4 * MB;
    }
    return (sizeof(DispatchSample) + sizeof(uword) * DispatchSample::kAvgDepth) * DispatchSample::kMaxThreads * process_period / period;
}

bool DispatchSampleBuffer::GetSample(RingBuffer *buffer, DispatchSample *sample)
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

bool DispatchSampleBuffer::GetSampleFromBuffer(RingBuffer *ring_buffer, RingBuffer::Buffer *buffer, DispatchSample *out)
{
    char *buf = reinterpret_cast<char *>(buffer->data);
    size_t size = buffer->size;
    char *end = buf + size;
    
    DispatchData *data;
    
    if (!RingBuffer::ViewAndAdvance<DispatchData>(&buf, &data, end))
    {
        return false;
    }
    
    ring_buffer->Get(&out->data, data, sizeof(out->data));
    
    if (buf > end)
    {
        return false;
    }
    
    ring_buffer->Get(out->pcs, buf, static_cast<size_t>(end - buf));
    constexpr size_t pc_size = sizeof(get_type<decltype(out->pcs)>::type);
    out->stack_size = (uint32_t)((size_t)(end - buf) / pc_size);

    return true;
}

bool DispatchSampleBuffer::PutSample(DispatchSample &sample)
{
    bool result = false;
    
    constexpr size_t pc_size = sizeof(get_type<decltype(sample.pcs)>::type);
    size_t total_size = sizeof(sample.data) + sample.stack_size * pc_size;
    result = TryWrite(total_size, [&sample](RingBuffer *buffer, RingBuffer::Buffer *buf)
    {
        buffer->Put(buf->data, &sample.data, sizeof(sample.data));
        buffer->Put(buf->data+sizeof(sample.data), sample.pcs, sample.stack_size * pc_size);
    });

    return result;
}

void ProcessDispatchSampleBuffer() {
    if (!s_running) {
        return;
    }
    
    ScopedCounter counter(s_working_thread, s_main_running, false);
    
    if (!s_running) {
        return;
    }
    
    ProcessSamples();
}

void ProcessSamples() {
    auto overwritten_records = std::vector<Record>();

    Transaction transaction([BTrace shared].db->getHandle());
    
    uint64_t nums = s_buffer->Iterate([&overwritten_records](RingBuffer *ring_buffer, size_t num){
        uword buffer[kMaxStackDepth];
        DispatchSample sample;
        sample.pcs = buffer;
        
        if (!s_buffer->GetSample(ring_buffer, &sample))
        {
            return;
        }
        
        s_buffer_size->fetch_sub(1);
        
        uint32_t stack_id = g_callstack_table->insert(sample.pcs, sample.stack_size);
        auto dispatch_record = DispatchRecord(sample.data.source_tid, sample.data.source_time,
                                        sample.data.source_cpu_time, sample.data.target_tid,
                                        sample.data.target_time, stack_id, sample.data.alloc_size,
                                        sample.data.alloc_count);
        if (g_record_buffer) {
            auto overwritten_record = Record();
            auto record = Record(dispatch_record);
            g_record_buffer->OverWrittenRecord(record, overwritten_record);
            if (overwritten_record.record_type != RecordType::kNone) {
                overwritten_records.push_back(overwritten_record);
            }
        } else {
            auto dispatch_sample_record = DispatchSampleModel(trace_id, dispatch_record);
            [BTrace shared].db->insert(dispatch_sample_record);
        }
    });

    transaction.commit();
    RecordBuffer::ProcessOverWrittenRecord(overwritten_records);
}

} // namespace btrace
