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

#import "InternalUtils.h"
#import "BTrace.h"
#import "BTraceLockProfilerConfig.h"
#import "BTraceLockProfilerPlugin.h"

#include "LockSampleModel.hpp"
#include "BTraceDataBase.hpp"
#include "BTraceRecord.hpp"

#include "callstack_table.hpp"
#include "concurrentqueue.hpp"
#include "event_loop.hpp"
#include "os_thread.hpp"
#include "utils.hpp"

namespace btrace {
extern uint8_t trace_id;
extern int64_t start_mach_time;

static std::atomic<uint64_t> s_main_lock_id = 0;
static std::atomic<bool> s_running = false;
static std::atomic<bool> s_main_running = false;
static std::atomic<uint32_t> s_working_thread = 0;
static BTraceLockProfilerConfig *s_config = nullptr;
static int s_period = 200; // ms

enum class LockAction : uint16_t {
    kMtxLock = 1,
    kMtxTrylock,
    kMtxUnlock,
    kCondWait,
    kCondTimedwait,
    kCondSignal,
    kCondBroadcast,
    kRdlock,
    kTryrdlock,
    kWrlock,
    kTrywrlock,
    kRwUnlock,
    kUnfairLock,
    kUnfairUnlock,
    kMtxLockContention,
    kMtxUnlockContention,
    kRdlockContention,
    kWrlockContention,
    kRwUnlockContention,
    kUnfairTrylock,
    kUnfairLockContention,
    kUnfairUnlockContention,
};

#pragma pack(push, 4)
struct LockData {
    uint32_t tid;
    uint64_t id: 48;
    uint64_t action: 16;
    uint32_t time;
    uint32_t cpu_time;
    uint32_t alloc_size;
    uint32_t alloc_count;

    LockData(uint32_t tid, uint64_t id, uint64_t action, uint32_t time,
               uint32_t cpu_time, uint32_t alloc_size, uint32_t alloc_count)
        : tid(tid), id(id), action(action), time(time), cpu_time(cpu_time),
        alloc_size(alloc_size), alloc_count(alloc_count) {}
};
#pragma pack(pop)

struct LockSample {
#if DEBUG || INHOUSE_TARGET || TEST_MODE
    static constexpr uintptr_t kAvgDepth = 32;
    static constexpr uintptr_t kMaxThreads = 128;
#else
    static constexpr uintptr_t kAvgDepth = 16;
    static constexpr uintptr_t kMaxThreads = 128;
#endif
    
    LockData data;
    uint32_t stack_size = 0;
    uword *pcs = nullptr;
    
    LockSample()
        : data(0, 0, 0, 0, 0, 0, 0),
        stack_size(0), pcs(nullptr) {}

    LockSample(uint32_t tid, uint64_t id, uint64_t action, uint32_t time,
               uint32_t cpu_time, uint32_t stack_size, uword *pcs,
               uint32_t alloc_size, uint32_t alloc_count)
        : data(tid, id, action, time, cpu_time, alloc_size, alloc_count),
        stack_size(stack_size), pcs(pcs) {}
};

class LockSampleBuffer : public ConcurrentRingBuffer
{
public:
    LockSampleBuffer(uintptr_t size) : ConcurrentRingBuffer(size, 32) {}
    
    bool GetSample(RingBuffer *ring_buffer, LockSample *sample);
    
    bool PutSample(LockSample &sample);
    
private:
    bool GetSampleFromBuffer(RingBuffer *buffer, RingBuffer::Buffer *buf, LockSample *out);
};

static LockSampleBuffer *s_buffer;

static uintptr_t CalculateBufferSize(int process_period, int period);

static void ProcessLockSampleBuffer();

static void ProcessSamples();
} // namespace btrace

@interface BTraceLockProfilerPlugin ()

@end

void epilogue(void *lock_id, LockAction action, bool force=false,
              int64_t start_access_time=0)
{
    {
        if unlikely(!s_running.load(std::memory_order_relaxed)) {
            goto end;
        }
        
        auto p_tid = OSThread::PthreadSelf();
        bool is_main = (p_tid == OSThread::MainJoinId());
        
        if unlikely(OSThread::ThreadLogging(is_main)) {
            goto end;
        }

        ScopedCounter counter(s_working_thread, s_main_running, is_main);
        
        if unlikely(!s_running) {
            goto end;
        }
        
        OSThread::ScopedDisableLogging disable(is_main);
        
        OSThread *os_thread = nullptr;

        if (is_main) {
            os_thread = OSThread::MainThread();
        } else {
            os_thread = OSThread::GetCurrentTLS();
        }
        
        if (!os_thread) {
            return;
        }
        
        int64_t prev_access_time = os_thread->access_time();
        int64_t curr_time = Utils::GetCurrMachMicros();
        int64_t diff_time = curr_time - prev_access_time;

        auto period = s_config.bg_period;
        
        if unlikely(is_main) {
            period = s_config.period;
        }
        
        if (!force && diff_time < period) {
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
        auto sample = LockSample(os_thread->id(), (uint64_t)lock_id, 
                                 static_cast<uint64_t>(action),
                                 rel_time, cpu_time, stack_size, 
                                 buffer_ptr, alloc_size, alloc_count);
        
        s_buffer->PutSample(sample);
        
        if (start_access_time) {
            sample.data.time = (uint32_t)((start_access_time - start_mach_time)/10);
            s_buffer->PutSample(sample);
        }
    }

end:
    return;
}

BTRACE_HOOK(int, pthread_mutex_trylock, pthread_mutex_t *mutex) {
    int res = ORI(pthread_mutex_trylock)(mutex);
    
    bool enable = s_config != nullptr && s_config.mtx;
    
    if (enable) {
        epilogue(mutex, LockAction::kMtxTrylock);
    }

    return res;
}

BTRACE_HOOK(int, pthread_mutex_lock, pthread_mutex_t *mutex) {
    int res;
    bool force = false;
    int64_t start_access_time = 0;
    auto action = LockAction::kMtxLock;
    bool enable = s_config != nullptr && s_config.mtx;

    if (enable && ORI(pthread_mutex_trylock) &&
        OSThread::PthreadSelf() == OSThread::MainJoinId()) {
        res = ORI(pthread_mutex_trylock)(mutex);
        
        if unlikely(res == EBUSY) {
            force = true;
            s_main_lock_id = (uint64_t)mutex;
            action = LockAction::kMtxLockContention;
            start_access_time = Utils::GetCurrMachMicros();
            goto lock;
        } else {
            goto epi;
        }
    } else {
        goto lock;
    }
    
lock:
    res = ORI(pthread_mutex_lock)(mutex);
    s_main_lock_id = 0;
epi:
    if (enable) {
        epilogue(mutex, action, force, start_access_time);
    }
    return res;
}

BTRACE_HOOK(int, pthread_mutex_unlock, pthread_mutex_t *mutex) {
    bool force = false;
    auto action = LockAction::kMtxUnlock;
    bool enable = s_config != nullptr && s_config.mtx;
    
    if (enable && (uint64_t)mutex == s_main_lock_id) {
        force = true;
        action = LockAction::kMtxUnlockContention;
    }

    int res = ORI(pthread_mutex_unlock)(mutex);
    
    if (enable) {
        epilogue(mutex, action, force);
    }
    return res;
}

BTRACE_HOOK(int, pthread_cond_wait, pthread_cond_t *cond,
               pthread_mutex_t *mutex) {
    int res = ORI(pthread_cond_wait)(cond, mutex);
    
    bool enable = s_config != nullptr && s_config.cond;
    
    if (enable) {
        epilogue(cond, LockAction::kCondWait, false, 0);
    }
    return res;
}

BTRACE_HOOK(int, pthread_cond_timedwait, pthread_cond_t *cond,
               pthread_mutex_t *mutex, const struct timespec *abstime) {
    int res = ORI(pthread_cond_timedwait)(cond, mutex, abstime);
    
    bool enable = s_config != nullptr && s_config.cond;
    
    if (enable) {
        epilogue(cond, LockAction::kCondTimedwait, false, 0);
    }
    return res;
}

BTRACE_HOOK(int, pthread_cond_signal, pthread_cond_t *cond) {
    int res = ORI(pthread_cond_signal)(cond);
    
    bool enable = s_config != nullptr && s_config.cond;
    
    if (enable) {
        epilogue(cond, LockAction::kCondSignal);
    }
    return res;
}

BTRACE_HOOK(int, pthread_cond_broadcast, pthread_cond_t *cond) {
    int res = ORI(pthread_cond_broadcast)(cond);
    
    bool enable = s_config != nullptr && s_config.cond;
    
    if (enable) {
        epilogue(cond, LockAction::kCondBroadcast);
    }
    return res;
}

BTRACE_HOOK(bool, os_unfair_lock_trylock, os_unfair_lock_t lock) {
    bool res = ORI(os_unfair_lock_trylock)(lock);
    
    bool enable = s_config != nullptr && s_config.unfair;
    
    if (enable) {
        epilogue(lock, LockAction::kUnfairTrylock);
    }
    return res;
}

BTRACE_HOOK(void, os_unfair_lock_lock, os_unfair_lock_t lock) {
    bool res;
    bool force = false;
    int64_t start_access_time = 0;
    auto action = LockAction::kUnfairLock;
    bool enable = s_config != nullptr && s_config.unfair;
    
    if (enable && ORI(os_unfair_lock_trylock) &&
        OSThread::PthreadSelf() == OSThread::MainJoinId()) {
        res = ORI(os_unfair_lock_trylock)(lock);
        
        if unlikely(!res) {
            force = true;
            s_main_lock_id = (uint64_t)lock;
            action = LockAction::kUnfairLockContention;
            start_access_time = Utils::GetCurrMachMicros();
            goto lock;
        } else {
            goto epi;
        }
    } else {
        goto lock;
    }
    
lock:
    ORI(os_unfair_lock_lock)(lock);
    s_main_lock_id = 0;
epi:
    if (enable) {
        epilogue(lock, action, force, start_access_time);
    }
}

BTRACE_HOOK(void, os_unfair_lock_unlock, os_unfair_lock_t lock) {
    bool force = false;
    auto action = LockAction::kUnfairUnlock;
    bool enable = s_config != nullptr && s_config.unfair;
    
    if (enable && (uint64_t)lock == s_main_lock_id) {
        force = true;
        action = LockAction::kUnfairUnlockContention;
    }

    ORI(os_unfair_lock_unlock)(lock);
    
    if (enable) {
        epilogue(lock, action, force);
    }
}

// BTRACE_HOOK(intptr_t, dispatch_semaphore_wait, dispatch_semaphore_t dsema,
// dispatch_time_t timeout) {
//     intptr_t res = ORI(dispatch_semaphore_wait)(dsema, timeout);
//     epilogue(dsema, LockAction::kSemaWait);
//     return res;
// }

// BTRACE_HOOK(intptr_t, dispatch_semaphore_signal, dispatch_semaphore_t
// dsema) {
//     intptr_t res = ORI(dispatch_semaphore_signal)(dsema);
//     epilogue(dsema, LockAction::kSemaSignal);
//     return res;
// }

BTRACE_HOOK(int, pthread_rwlock_tryrdlock, pthread_rwlock_t *rwlock) {
    int res = ORI(pthread_rwlock_tryrdlock)(rwlock);
    
    bool enable = s_config != nullptr && s_config.rw;
    
    if (enable) {
        epilogue(rwlock, LockAction::kTryrdlock);
    }
    return res;
}

BTRACE_HOOK(int, pthread_rwlock_rdlock, pthread_rwlock_t *rwlock) {
    int res;
    bool force = false;
    int64_t start_access_time = 0;
    auto action = LockAction::kRdlock;
    bool enable = s_config != nullptr && s_config.rw;
    
    if (enable && ORI(pthread_rwlock_tryrdlock) &&
        OSThread::PthreadSelf() == OSThread::MainJoinId()) {
        res = ORI(pthread_rwlock_tryrdlock)(rwlock);
        
        if unlikely(res == EBUSY) {
            force = true;
            s_main_lock_id = (uint64_t)rwlock;
            action = LockAction::kRdlockContention;
            start_access_time = Utils::GetCurrMachMicros();
            goto lock;
        } else {
            goto epi;
        }
    } else {
        goto lock;
    }
lock:
    res = ORI(pthread_rwlock_rdlock)(rwlock);
    s_main_lock_id = 0;
epi:
    if (enable) {
        epilogue(rwlock, action, force, start_access_time);
    }
    return res;
}

BTRACE_HOOK(int, pthread_rwlock_trywrlock, pthread_rwlock_t *rwlock) {
    int res = ORI(pthread_rwlock_trywrlock)(rwlock);
    
    bool enable = s_config != nullptr && s_config.rw;
    
    if (enable) {
        epilogue(rwlock, LockAction::kTrywrlock);
    }
    return res;
}

BTRACE_HOOK(int, pthread_rwlock_wrlock, pthread_rwlock_t *rwlock) {
    int res;
    bool force = false;
    int64_t start_access_time = 0;
    auto action = LockAction::kWrlock;
    bool enable = s_config != nullptr && s_config.rw;
    
    if (enable && ORI(pthread_rwlock_trywrlock) &&
        OSThread::PthreadSelf() == OSThread::MainJoinId()) {
        res = ORI(pthread_rwlock_trywrlock)(rwlock);
        
        if unlikely(res == EBUSY) {
            force = true;
            s_main_lock_id = (uint64_t)rwlock;
            action = LockAction::kWrlockContention;
            start_access_time = Utils::GetCurrMachMicros();
            goto lock;
        } else {
            goto epi;
        }
    } else {
        goto lock;
    }
lock:
    res = ORI(pthread_rwlock_wrlock)(rwlock);
    s_main_lock_id = 0;
epi:
    if (enable) {
        epilogue(rwlock, action, force, start_access_time);
    }
    return res;
}

BTRACE_HOOK(int, pthread_rwlock_unlock, pthread_rwlock_t *rwlock) {
    bool force = false;
    auto action = LockAction::kRwUnlock;
    bool enable = s_config != nullptr && s_config.rw;
    
    if (enable && (uint64_t)rwlock == s_main_lock_id) {
        force = true;
        action = LockAction::kRwUnlockContention;
    }

    int res = ORI(pthread_rwlock_unlock)(rwlock);
    
    if (enable) {
        epilogue(rwlock, action, force);
    }
    return res;
}

__attribute__((objc_direct_members))
@implementation BTraceLockProfilerPlugin

+ (NSString *)name {
    return @"lockprofiler";
}

+ (instancetype)shared {
    static BTraceLockProfilerPlugin *instance = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      instance = [[self alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];

    [self lockFishhook];

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
        self.config = [BTraceLockProfilerConfig defaultConfig];
    }

    if (![self initTable]) {
        return;
    }
    
    auto config = (BTraceLockProfilerConfig *)self.config;

    if (s_buffer == nullptr) {
        auto size = CalculateBufferSize(s_period * kMicrosPerMilli, config.period);
        s_buffer = new LockSampleBuffer(size);
    }

    s_config = (BTraceLockProfilerConfig *)self.config;
    s_running = true;

    EventLoop::Register(200, ProcessLockSampleBuffer);
}

- (void)dump {
    if (!self.btrace.enable) {
        return;
    }
    
    if (!s_running) {
        return;
    }
    
    ProcessLockSampleBuffer();
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
        result = self.btrace.db->drop_table<LockBatchSampleModel>();

        if (!result) {
            return result;
        }

        result = self.btrace.db->create_table<LockBatchSampleModel>();

        if (!result) {
            return result;
        }

    } else {

        result = self.btrace.db->drop_table<LockSampleModel>();

        if (!result) {
            return result;
        }

        result = self.btrace.db->create_table<LockSampleModel>();

        if (!result) {
            return result;
        }
    }

    return result;
}

- (void)lockFishhook {
    dispatch_async(self.btrace.queue, ^{
      struct rebinding r[] = {
          REBINDING(pthread_mutex_trylock),
          REBINDING(pthread_mutex_lock),
          REBINDING(pthread_mutex_unlock),
          REBINDING(pthread_cond_wait),
          REBINDING(pthread_cond_timedwait),
          REBINDING(pthread_cond_signal),
          REBINDING(pthread_cond_broadcast),
          REBINDING(pthread_rwlock_tryrdlock),
          REBINDING(pthread_rwlock_rdlock),
          REBINDING(pthread_rwlock_trywrlock),
          REBINDING(pthread_rwlock_wrlock),
          REBINDING(pthread_rwlock_unlock),
          REBINDING(os_unfair_lock_trylock),
          REBINDING(os_unfair_lock_lock),
          REBINDING(os_unfair_lock_unlock),
          //            REBINDING(dispatch_semaphore_wait),
          //            REBINDING(dispatch_semaphore_signal),
      };
      rebind_symbols(r, sizeof(r) / sizeof(struct rebinding));
    });
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
        return 6 * MB;
    }
    return (sizeof(LockSample) + sizeof(uword) * LockSample::kAvgDepth) * LockSample::kMaxThreads * process_period / period;
}

bool LockSampleBuffer::GetSample(RingBuffer *buffer, LockSample *sample)
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

bool LockSampleBuffer::GetSampleFromBuffer(RingBuffer *ring_buffer, RingBuffer::Buffer *buffer, LockSample *out)
{
    char *buf = reinterpret_cast<char *>(buffer->data);
    size_t size = buffer->size;
    char *end = buf + size;
    
    LockData *data;
    
    if (!RingBuffer::ViewAndAdvance<LockData>(&buf, &data, end))
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

bool LockSampleBuffer::PutSample(LockSample &sample)
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


void ProcessLockSampleBuffer() {
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
        LockSample sample;
        sample.pcs = buffer;
        
        if (!s_buffer->GetSample(ring_buffer, &sample))
        {
            return;
        }
        
        uint32_t stack_id = g_callstack_table->insert(sample.pcs, sample.stack_size);
        auto lock_record = LockRecord(sample.data.tid, sample.data.id, sample.data.action,
                                      sample.data.time, sample.data.cpu_time, stack_id,
                                      sample.data.alloc_size, sample.data.alloc_count);
        if (g_record_buffer) {
            auto overwritten_record = Record();
            auto record = Record(lock_record);
            g_record_buffer->OverWrittenRecord(record, overwritten_record);
            if (overwritten_record.record_type != RecordType::kNone) {
                overwritten_records.push_back(overwritten_record);
            }
        } else {
            auto lock_sample_record = LockSampleModel(trace_id, lock_record);
            [BTrace shared].db->insert(lock_sample_record);
        }
    });

    transaction.commit();
    RecordBuffer::ProcessOverWrittenRecord(overwritten_records);
}

} // namespace btrace
