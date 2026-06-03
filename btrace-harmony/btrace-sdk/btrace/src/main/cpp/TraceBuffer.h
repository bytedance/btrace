/*
 * Copyright (c) 2026 Bytedance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Created on 2025/10/10.
//

#ifndef HARMONY_BTRACE_TRACEBUFFER_H
#define HARMONY_BTRACE_TRACEBUFFER_H

#include <cassert>
#include <functional>
#include <mutex>
#include <stdint.h>

#include <atomic>
#include <cstdint>

#include "StackTrace.h"
#include "RingBuffer.h"
#include "TraceConfigurations.h"

namespace btrace {

class TraceRingBuffer {
public:
    using Callback = std::function<void(const SampleRecord &)>;
    TraceRingBuffer(uintptr_t size) {
        size = common::round_up(size, sizeof(SampleRecord));
        buffer_ = new RingBuffer(size);
    }
    
    ~TraceRingBuffer() {
        delete buffer_;
        buffer_ = nullptr;
    }
    
    bool OverWrite(SampleRecord &in, const Callback &cb);
    
    uint64_t Iterate(const Callback &fn);
    
private:
    RingBuffer *buffer_ = nullptr;
    DISALLOW_COPY_AND_ASSIGN(TraceRingBuffer);
};

class TraceBuffer {
public:
    using WriteCallback = void(*)(void*, StackTrace&);
    static TraceBuffer& get();
    bool setup(const BufferConfig& config);
    bool cleanup();
    bool write(StackTrace& stackTrace);
    bool signalSafetyWrite(void* payload, WriteCallback callback);
    bool read(uint64_t ticket, StackTrace* out);
    uint64_t getCurTicket() {
        return ticket_.load(std::memory_order_relaxed);
    }
    void getCurTicketRange(uint64_t* outBegin, uint64_t* outEnd) {
        uint64_t curTicket = ticket_.load(std::memory_order_relaxed);
        *outBegin = curTicket > slotCount_ ? curTicket - slotCount_ : 0;
        *outEnd = curTicket;
    }
    uint32_t getMapSize() { return mapSize_; }
    
    bool isPreloadEnabled() { return enablePreload_; }
    bool preloadOverWrite(SampleRecord &record, const TraceRingBuffer::Callback &cb);
    uint64_t preloadIterate(const TraceRingBuffer::Callback &fn);
private:
    TraceBuffer() = default;
    TraceBuffer(const TraceBuffer&) = delete;
    TraceBuffer& operator=(const TraceBuffer&) = delete;
    
    struct Slot {
        // 0 表示空闲状态，-1 表示写状态（不允许 < -1，只支持单线程写），> 0 表示读状态（支持多线程读）
        // 通过该 flag 实现读写互斥，以及控制写不能并发，读可以并发
        std::atomic_int32_t flag;
        uint64_t ticket;
        StackTrace stackTrace;
    };

    Slot* slots_ = nullptr;
    uint32_t mapSize_ = 0;
    uint32_t slotCount_ = 0;
    std::atomic_uint64_t ticket_ = 0;
    
    bool enablePreload_ = false;
    TraceRingBuffer *buffer_ = nullptr;

    static_assert(
        std::is_nothrow_default_constructible<Slot>::value,
        "TraceBuffer Slot must be nothrow default constructible");
    static_assert(
        std::is_trivially_copyable<StackTrace>::value,
        "StackTrace must be trivially copyable");

};

}

#endif //HARMONY_BTRACE_TRACEBUFFER_H
