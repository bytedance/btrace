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

#include "TraceBuffer.h"
#include <cassert>
#include <hilog/log.h>
#include <sys/mman.h>
#include <cstdint>
#include <unistd.h>

#include "util/globals.h"

#undef LOG_TAG
#define LOG_TAG "btrace:TraceBuffer"

namespace btrace {

TraceBuffer& TraceBuffer::get() {
    static TraceBuffer inst{};
    return inst;
}

bool TraceBuffer::setup(const BufferConfig& config) {
    uint32_t count = config.getBufferSize();
    enablePreload_ = config.getEnablePreload();
    
    if (enablePreload_) {
        uint64_t size = count * sizeof(StackTrace);
        buffer_ = new TraceRingBuffer(size);
        return true;
    }
    
    uint64_t pageMask = getpagesize() - 1;
    uint64_t memSize = ((uint64_t) count * sizeof(Slot) + pageMask) & ~pageMask;
    void* addr = mmap(nullptr, memSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        OH_LOG_ERROR(LOG_APP, "mmap failed for trace buffer: %{public}s", strerror(errno));
        return false;
    }
    slots_ = reinterpret_cast<Slot*>(addr);
    slotCount_ = count;
    mapSize_ = memSize;
    ticket_.store(0, std::memory_order_relaxed);
    memset((char*)addr, 0, memSize);
    for (uint32_t i = 0; i < count; ++i) {
        new (static_cast<void*>(&slots_[i])) Slot;
    }
    return true;
}

bool TraceBuffer::cleanup() {
    for (uint32_t i = 0; i < slotCount_; ++i) {
        slots_[i].~Slot();
    }
    
    int result = 0;
    if (slots_ != nullptr) {
        result = munmap(slots_, mapSize_);
        slots_ = nullptr;
    }
    
    enablePreload_ = false;
    
    if (buffer_ != nullptr) {
        delete buffer_;
        buffer_ = nullptr;
    }
    
    slotCount_ = 0;
    mapSize_ = 0;
    ticket_.store(0, std::memory_order_relaxed);
    return result == 0;
}

bool TraceBuffer::write(StackTrace& stackTrace) {
    auto curTicket = ticket_.fetch_add(1, std::memory_order_relaxed);
    uint32_t index = curTicket % slotCount_;
    auto& slot = slots_[index];
    if (slot.ticket > curTicket) {
        return false;
    }
    static constexpr uint32_t kSpinMax = 8;
    static constexpr uint32_t kYieldMax = 12;
    uint32_t checkRound = 0;
    int32_t flag = 0;
    // 当另外的线程在读该 slot 时，做一个简单自旋等待 read 结束，如果自旋足够多次读操作还没有结束则放弃此次写操作
    while (!std::atomic_compare_exchange_strong(&slot.flag, &flag, -1)) {
        if (checkRound < kSpinMax) {
            // 做一个简单的循环，当做 cpu 空转等待
            // volatile 变量以及 spinCount 非常量是为了避免编译期把这块代码优化成 testX = spinCount;
            volatile uint32_t testX = 0;
            const uint32_t spinCount = 10 * checkRound;
            for (uint32_t spin = 0; spin < spinCount; ++spin){
                ++testX;
            }
        } else if (checkRound < kYieldMax) {
            sched_yield();
        } else {
            break;
        }
        flag = 0;
        ++checkRound;
    }
    if (checkRound < kYieldMax && slot.ticket <= curTicket) {
        slot.ticket = curTicket;
        memcpy(&(slot.stackTrace), &stackTrace, sizeof(StackTrace));
        slot.flag = 0;
        return true;
    }
    return false;
}

bool TraceBuffer::signalSafetyWrite(void* payload, WriteCallback callback) {
    auto curTicket = ticket_.fetch_add(1, std::memory_order_relaxed);
    uint32_t index = curTicket % slotCount_;
    auto& slot = slots_[index];
    if (slot.ticket > curTicket) {
        return false;
    }
    static constexpr uint32_t kSpinMax = 12;
    uint32_t checkRound = 0;
    int32_t flag = 0;
    // 当另外的线程在读该 slot 时，做一个简单自旋等待 read 结束，如果自旋足够多次读操作还没有结束则放弃此次写操作
    while (!std::atomic_compare_exchange_strong(&slot.flag, &flag, -1)) {
        if (checkRound < kSpinMax) {
            // 做一个简单的循环，当做 cpu 空转等待
            // volatile 变量以及 spinCount 非常量是为了避免编译期把这块代码优化成 testX = spinCount;
            volatile uint32_t testX = 0;
            const uint32_t spinCount = 10 * checkRound;
            for (uint32_t spin = 0; spin < spinCount; ++spin){
                ++testX;
            }
        } else {
            break;
        }
        flag = 0;
        ++checkRound;
    }
    if (checkRound < kSpinMax && slot.ticket <= curTicket) {
        slot.ticket = curTicket;
        callback(payload, slot.stackTrace);
        slot.flag = 0;
        return true;
    }
    return false;
}

bool TraceBuffer::read(uint64_t readTicket, StackTrace* outStackTrace) {
    uint32_t index = readTicket % slotCount_;
    auto& slot = slots_[index];
    if (readTicket < slot.ticket) {
        return false;
    }
    
    int32_t flag = slot.flag.load();
    if (flag < 0) {
        // 当前在写的时候不等待写结束，毕竟读时还在写的数据极大概率是 buffer 被覆盖一轮后的新数据，也就是超期数据
        return false;
    }
    while (!std::atomic_compare_exchange_strong(&slot.flag, &flag, flag + 1)) {
        if (flag < 0) {
            return false;
        }
    }
    if (slot.ticket == readTicket) {
        memcpy(outStackTrace, &slot.stackTrace, sizeof(StackTrace));
        slot.flag.fetch_sub(1);
        return true;
    }
    return false;
}

bool TraceBuffer::preloadOverWrite(SampleRecord &in, const TraceRingBuffer::Callback &cb) {
    return buffer_->OverWrite(in, cb);
}

uint64_t TraceBuffer::preloadIterate(const TraceRingBuffer::Callback &fn) {
    return buffer_->Iterate(fn);
}

bool TraceRingBuffer::OverWrite(SampleRecord &record, const Callback &cb) {
    auto src = (uint8_t*)&record;
    auto size = buffer_->OverPut(src, sizeof(record), [&cb](const RingBuffer *ring,
                                                            const RingBuffer::Buffer &buffer){
        SampleRecord old;
        assert(buffer.size == sizeof(old));
        bool res = ring->CopyOut(&old, buffer.data, sizeof(old));
        
        if (res) {
            cb(old);
        }
    });
    
    return 0 < size;
}
    
uint64_t TraceRingBuffer::Iterate(const Callback &fn) {
    uint64_t total_nums = 0;
    auto pos = buffer_->GetPointerPositions();
    uint64_t curr_pos = pos.read_pos;

    do {
        auto record = SampleRecord{};
        auto src = buffer_->at(curr_pos);
        buffer_->CopyOut(&record, src, sizeof(record));
        curr_pos += sizeof(record);
        total_nums++;
        
        fn(record);
    } while (curr_pos < pos.write_pos);
    
    return total_nums;
}
}