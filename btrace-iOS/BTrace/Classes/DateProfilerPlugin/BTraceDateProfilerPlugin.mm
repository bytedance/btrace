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
//  BTraceLockProfilerPlugin.m
//  Pods-PerfAnalysis_Core
//
//  Created by ByteDance on 2024/11/6.
//

#import <Foundation/Foundation.h>

#include <memory>
#include <vector>
#include <unordered_map>

#import "BTrace.h"
#import "InternalUtils.h"
#import "BTraceDateProfilerConfig.h"
#import "BTraceDateProfilerPlugin.h"

#include "BTraceRecord.hpp"
#include "BTraceDataBase.hpp"

#include "utils.hpp"
#include "os_thread.hpp"
#include "event_loop.hpp"
#include "concurrentqueue.hpp"
#include "callstack_table.hpp"


namespace btrace {
    extern uint8_t trace_id;
    extern int64_t start_mach_time;

    static std::atomic<bool> s_running = false;
    static std::atomic<bool> s_main_running = false;
    static std::atomic<uint32_t> s_working_thread = 0;
    static BTraceDateProfilerConfig *s_config = nullptr;
    static int s_period = 200; // ms
    
#pragma pack(push, 4)
    struct DateData {
        uint32_t tid;
        uint32_t time;
        uint32_t cpu_time;
        uint32_t alloc_size;
        uint32_t alloc_count;
        
        DateData(uint32_t tid, uint32_t time, uint32_t cpu_time,
                 uint32_t alloc_size, uint32_t alloc_count)
        : tid(tid), time(time), cpu_time(cpu_time),
        alloc_size(alloc_size), alloc_count(alloc_count){}
    };
#pragma pack(pop)

    struct DateSample {
        
#if DEBUG || INHOUSE_TARGET || TEST_MODE
    static constexpr uintptr_t kAvgDepth = 32;
    static constexpr uintptr_t kMaxThreads = 64;
#else
    static constexpr uintptr_t kAvgDepth = 16;
    static constexpr uintptr_t kMaxThreads = 64;
#endif
        
        DateData data;
        uint32_t stack_size = 0;
        uword *pcs = nullptr;
        
        DateSample()
            : data(0, 0, 0, 0, 0),
            stack_size(0), pcs(nullptr) {}
        
        DateSample(uint32_t tid, uint32_t time, uint32_t cpu_time,
                   uint32_t stack_size, uword *pcs, uint32_t alloc_size,
                   uint32_t alloc_count)
            : data(tid, time, cpu_time, alloc_size, alloc_count),
            stack_size(stack_size), pcs(pcs) {}
    };

    class DateSampleBuffer : public ConcurrentRingBuffer
    {
    public:
        DateSampleBuffer(uintptr_t size) : ConcurrentRingBuffer(size, 32) {}
        
        bool GetSample(RingBuffer *ring_buffer, DateSample *sample);
        
        bool PutSample(DateSample &sample);
        
    private:
        bool GetSampleFromBuffer(RingBuffer *buffer, RingBuffer::Buffer *buf, DateSample *out);
    };

    static DateSampleBuffer *s_buffer;

    static uintptr_t CalculateBufferSize(int process_period, int period);
    
    static void ProcessDateSampleBuffer();

    static void ProcessSamples();
}

@interface BTraceDateProfilerPlugin ()

+ (NSDate *)btrace_date;

@end

void epilogue()
{
    {
        if unlikely(!s_running.load(std::memory_order_relaxed)) {
            goto end;
        }
        
        auto p_tid = OSThread::PthreadSelf();
        bool is_main = (p_tid == OSThread::MainJoinId());
        
        if unlikely(OSThread::ThreadLogging(is_main))
        {
            goto end;
        }

        ScopedCounter counter(s_working_thread, s_main_running, is_main);
        
        if unlikely(!s_running) {
            goto end;
        }
        
        OSThread::ScopedDisableLogging disable(is_main);
        
        auto os_thread = OSThread::TryCurrent(is_main);
        
        if (!os_thread) {
            return;
        }
        
        int64_t prev_access_time = os_thread->access_time();
        int64_t curr_time = Utils::GetCurrMachMicros();
        int64_t diff_time = curr_time - prev_access_time;
        
        auto *main_thread = OSThread::MainThread();
        auto period = s_config.bg_period;
        
        if unlikely(os_thread == main_thread) {
            period = s_config.period;
        }
        
        if (diff_time < period) {
            goto end;
        }
        
        os_thread->set_access_time(curr_time);
        auto cpu_time = (uint32_t)(os_thread->update_cpu_time()/10);
        auto rel_time = (uint32_t)((curr_time - start_mach_time)/10);
        uword buffer[kMaxStackDepth];
        constexpr int max_size = sizeof(buffer) / sizeof(uword);
        auto stack_top = (uword*)os_thread->stack_base();
        auto stack_bot = (uword*)os_thread->stack_limit();
        auto stack_range = PthreadStackRange(stack_bot, stack_top);
        auto stack_size = Utils::ThreadStackPCs(buffer, max_size, 1, &stack_range);
        auto buffer_ptr = (uword*)buffer;
        auto alloc_size = (uint32_t)(os_thread->alloc_size()/MB);
        auto alloc_count = (uint32_t)(os_thread->alloc_count());
        auto sample = DateSample(os_thread->id(), rel_time, cpu_time, stack_size,
                             buffer_ptr, alloc_size, alloc_count);
    
        s_buffer->PutSample(sample);
    }

end:
    return;
}

BTRACE_HOOK(int, gettimeofday, struct timeval *tv, void *ptr) {
    int res = ORI(gettimeofday)(tv, ptr);
    epilogue();
    return res;
}

BTRACE_HOOK(int, clock_gettime, clockid_t __clock_id, struct timespec *__tp) {
    int res = ORI(clock_gettime)(__clock_id, __tp);

    if (__clock_id == CLOCK_THREAD_CPUTIME_ID) {
        return res;
    }
    
    epilogue();
    return res;
}


__attribute__((objc_direct_members))
@implementation BTraceDateProfilerPlugin

+ (NSString *)name {
    return @"dateprofiler";
}

+ (instancetype)shared {
    static BTraceDateProfilerPlugin *instance = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      instance = [[self alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];

    s_running = false;

    Class Date = NSClassFromString(@"NSDate");
        
    swizzleMethod(Date,
                  NSSelectorFromString(@"date"),
                  self.class,
                  @selector(btrace_date),
                  NO);
        
    [self dateFishhook];

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
        self.config = [BTraceDateProfilerConfig defaultConfig];
    }
    
    if (![self initTable]) {
        return;
    }
    
    auto config = (BTraceDateProfilerConfig *)self.config;

    if (s_buffer == nullptr) {
        auto size = CalculateBufferSize(s_period * kMicrosPerMilli, config.period);
        s_buffer = new DateSampleBuffer(size);
    }
    
    s_config = (BTraceDateProfilerConfig *)self.config;
    s_running = true;

    EventLoop::Register(200, ProcessDateSampleBuffer);
}

-(void)dump{
    if (!self.btrace.enable) {
        return;
    }
    
    if (!s_running) {
        return;
    }
    
    ProcessDateSampleBuffer();
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
    
    [self ClearSampleBuffer];
}

- (bool)initTable {
    bool result = false;
    
    if (0 < self.btrace.bufferSize) {      
        result = self.btrace.db->drop_table<DateBatchSampleModel>();
        
        if (!result) {
            return result;
        }
        
        result = self.btrace.db->create_table<DateBatchSampleModel>();

        if (!result) {
            return result;
        }
    } else {
        
        result = self.btrace.db->drop_table<DateSampleModel>();
        
        if (!result) {
            return result;
        }
        
        result = self.btrace.db->create_table<DateSampleModel>();

        if (!result) {
            return result;
        }
    }

    return result;
}

-(void)dateFishhook {
    dispatch_async(self.btrace.queue, ^{
        struct rebinding r[] = {
            REBINDING(gettimeofday),
            REBINDING(clock_gettime),
        };
        rebind_symbols(r, sizeof(r)/sizeof(struct rebinding));
    });
}

+ (NSDate *)btrace_date {
    NSDate *date = [self btrace_date];
    epilogue();
    return date;
}

- (void)ClearSampleBuffer {
    ProcessSamples();

    delete s_buffer;
    s_buffer = nullptr;
}

@end

namespace btrace {

uintptr_t CalculateBufferSize(int process_period, int period)
{
    if (period <= 0) {
        return 4 * MB;
    }
    return (sizeof(DateSample) + sizeof(uword) * DateSample::kAvgDepth) * DateSample::kMaxThreads * process_period / period;
}

bool DateSampleBuffer::GetSample(RingBuffer *buffer, DateSample *sample)
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

bool DateSampleBuffer::GetSampleFromBuffer(RingBuffer *ring_buffer, RingBuffer::Buffer *buffer, DateSample *out)
{
    char *buf = reinterpret_cast<char *>(buffer->data);
    size_t size = buffer->size;
    char *end = buf + size;
    
    DateData *data;
    
    if (!RingBuffer::ViewAndAdvance<DateData>(&buf, &data, end))
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

bool DateSampleBuffer::PutSample(DateSample &sample)
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

void ProcessDateSampleBuffer() {
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
        DateSample sample;
        sample.pcs = buffer;
        
        if (!s_buffer->GetSample(ring_buffer, &sample))
        {
            return;
        }
        
        uint32_t stack_id = g_callstack_table->insert(sample.pcs, sample.stack_size);
        auto date_record = DateRecord(sample.data.tid, sample.data.time, sample.data.cpu_time,
                                      stack_id, sample.data.alloc_size, sample.data.alloc_count);
        if (g_record_buffer) {
            auto overwritten_record = Record();
            auto record = Record(date_record);
            g_record_buffer->OverWrittenRecord(record, overwritten_record);
            if (overwritten_record.record_type != RecordType::kNone) {
                overwritten_records.push_back(overwritten_record);
            }
        } else {
            auto date_sample_record = DateSampleModel(trace_id, sample.data.tid, sample.data.time,
                                      sample.data.cpu_time, stack_id, sample.data.alloc_size, 
                                      sample.data.alloc_count);
            [BTrace shared].db->insert(date_sample_record);
        }
    });

    transaction.commit();
    RecordBuffer::ProcessOverWrittenRecord(overwritten_records);
}

}
