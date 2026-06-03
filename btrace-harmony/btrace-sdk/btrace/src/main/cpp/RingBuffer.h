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
// Created on 2026/4/8.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef BTRACE_HARMONY_RINGBUFFER_H
#define BTRACE_HARMONY_RINGBUFFER_H

#include <atomic>
#include <cassert>
#include <functional>
#include <vector>

#include "util/globals.h"
#include "util/lock_utils.h"
#include "util/common_utils.h"

namespace btrace {

class RingBuffer {
public:
    class Buffer {
      public:
        Buffer() {}
        Buffer(uint8_t *d, size_t s): data(d), size(s) {}

        Buffer(const Buffer &) = delete;
        Buffer &operator=(const Buffer &) = delete;

        Buffer(Buffer &&) = default;
        Buffer &operator=(Buffer &&) = default;

        explicit operator bool() const { return data != nullptr; }

        uint8_t *data = nullptr;
        size_t size = 0;
    };
    
    struct PointerPositions {
        uint64_t read_pos;
        uint64_t write_pos;
    };
    
    using OverWriteCallBack = std::function<void(const RingBuffer *ring, const Buffer &buf)>;

    RingBuffer(uintptr_t size, uintptr_t align=kMinBufferSize)
        : read_pos_(0), write_pos_(0), nums_(0), size_(0) {
        size = common::align(size, align);
        mem_ = reinterpret_cast<uint8_t *>(malloc(size));
        size_ = size;
    }

    ~RingBuffer() {
        delete mem_;
        mem_ = nullptr;
        size_ = 0;
    }

    RingBuffer(RingBuffer &&) noexcept;
    RingBuffer &operator=(RingBuffer &&) noexcept;

    Buffer BeginWrite(size_t size);
    // Thread Safety Warning:
    // - No thread safety is guaranteed between overwrite operations (BeginOverWrite/EndWrite) 
    //   and read operations (BeginRead/EndRead, Get, BeginPeek/EndPeek).
    // - Concurrent calls to the above APIs must be protected by a lock.
    // - If elements are fixed-size and buffer size is aligned to element size, seqlock is recommended for better performance.
    Buffer BeginOverWrite(size_t size, OverWriteCallBack cb);
    void EndWrite(Buffer buf);

    // Not thread safe
    Buffer BeginReserveWrite(size_t expected);
    void EndReserveWrite(Buffer buf, size_t used);
    
    Buffer BeginRead();
    size_t EndRead(const Buffer &);
    
    Buffer BeginPeek(uint64_t read_pos);
    size_t EndPeek(const Buffer &buf);
    
    BTRACE_FORCE_INLINE size_t CopyIn(uint8_t *dst, const void *src, size_t size) {
        uintptr_t offset = ((uintptr_t)dst - (uintptr_t)mem_) % size_;
        size_t l = std::min(size, size_ - offset);
        
        memcpy(mem_ + offset, src, l);
        memcpy(mem_, static_cast<const uint8_t *>(src) + l, size - l);

        return size;
    }

    uintptr_t Put(const void *src, size_t size);
    uintptr_t OverPut(const void *src, size_t size, OverWriteCallBack cb);
    BTRACE_FORCE_INLINE size_t CopyOut(void *dst, const void *src, size_t size) const {
        uintptr_t offset = ((const uintptr_t)src - (const uintptr_t)mem_) % size_;

        size_t l = std::min(size, size_ - offset);
        memcpy(dst, mem_ + offset, l);
        memcpy(static_cast<uint8_t *>(dst) + l, mem_, size - l);

        return size;
    }

    uintptr_t Get(void *dst, size_t size);
    
    uint64_t nums() const { return nums_; }
    
    uint64_t read_pos(std::memory_order order=std::memory_order_acquire) const {
        return read_pos_.load(order);
    }

    template <typename T>
    static bool ViewAndAdvance(char **ptr, T **out, const char *end) {
        if (end - sizeof(T) < *ptr) {
            return false;
        }

        *out = reinterpret_cast<T *>(*ptr);
        *ptr += sizeof(T);
        return true;
    }
    
    inline bool full() {
        PointerPositions pos = GetPointerPositions();
        return write_avail(pos) == 0;
    }
    
    inline uint8_t *at(uint64_t pos) { return mem_ + (pos % size_); }
    
    BTRACE_FORCE_INLINE PointerPositions GetPointerPositions(std::memory_order write_order=std::memory_order_acquire,
                                                             std::memory_order read_order=std::memory_order_acquire) {
        PointerPositions pos;
        pos.write_pos = write_pos_.load(write_order);
        pos.read_pos = read_pos_.load(read_order);
        
        assert(!IsCorrupt(pos));
        
        return pos;
    }

protected:
    static constexpr size_t kMinBufferSize = 4096;
    static constexpr auto kAlignment = 8; // 64 bits to use aligned memcpy().
    static constexpr auto kHeaderSize = 8;
    static constexpr auto kGuardSize = 64 * 1024 * 1024; // 64 MB.

    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    BTRACE_FORCE_INLINE bool IsCorrupt(const PointerPositions &pos) {
        if (pos.write_pos < pos.read_pos || size_ < pos.write_pos - pos.read_pos) {
            return true;
        }
        return false;
    }

    inline size_t read_avail(const PointerPositions &pos) {
        auto res = static_cast<size_t>(pos.write_pos - pos.read_pos);
        return res;
    }

    inline size_t write_avail(const PointerPositions &pos) {
        return size_ - read_avail(pos);
    }

    std::atomic<uint64_t> read_pos_;
    std::atomic<uint64_t> write_pos_;
    std::atomic<uint64_t> nums_;

    uint8_t *mem_ = nullptr; // Start of the contents.
    size_t size_ = 0;
};

class ConcurrentRingBuffer {
public:
    ConcurrentRingBuffer(uintptr_t size, uintptr_t concurrency_level);

    ~ConcurrentRingBuffer();

    template <typename F> uint64_t Iterate(F fn) {
        uint64_t total_nums = 0;
        for (uintptr_t i = 0; i < concurrency_level_; ++i) {
            auto buffer = buffer_list_[i];
            uint64_t nums = buffer->nums();
            total_nums += nums;
            for (size_t j = 0; j < nums; ++j) {
                fn(buffer, j);
            }
        }
        return total_nums;
    }
    
    template <typename F>
    bool TryWrite(uint64_t stamp, void *src, size_t total_size, F fn) {
        uintptr_t index = stamp + total_size;
        index = index % concurrency_level_;
        
        RingBuffer *buffer = nullptr;
        RingBuffer::Buffer buf;
        
        bool result = true;

        for (int i = 0; i < concurrency_level_; ++i) {
            if (lock_list_[index]->try_lock()) {
                buffer = buffer_list_[index];
                buf = buffer->BeginWrite(total_size);
                lock_list_[index]->unlock();
                
                if (buf) { break; }
            }
            
            index = (index + 1) % concurrency_level_;
        }

        if (!buf) { return false; }

        fn(buffer, buf, src);
        buffer->EndWrite(std::move(buf));
        
        return result;
    }
    
    template <typename F>
    bool TryReserveWrite(uint64_t stamp, size_t total_size, F fn) {
        uintptr_t index = stamp + total_size;
        index = index % concurrency_level_;
        
        RingBuffer *buffer = nullptr;
        RingBuffer::Buffer buf;
        
        bool result = true;

        for (int i = 0; i < concurrency_level_; ++i) {
            if (lock_list_[index]->try_lock()) {
                buffer = buffer_list_[index];
                buf = buffer->BeginReserveWrite(total_size);
                
                if (buf) { break; }
                
                lock_list_[index]->unlock();
            }
            
            index = (index + 1) % concurrency_level_;
        }

        if (!buf) { return false; }

        size_t used = fn(buffer, buf);
        buffer->EndReserveWrite(std::move(buf), used);
        
        lock_list_[index]->unlock();
        
        return result;
    }

private:
    const uintptr_t concurrency_level_;
    std::vector<Spinlock *> lock_list_;
    std::vector<RingBuffer *> buffer_list_;
};

} // namespace btrace


#endif //BTRACE_HARMONY_RINGBUFFER_H
