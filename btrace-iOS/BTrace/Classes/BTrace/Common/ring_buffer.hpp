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
 * Includes work from google/perfetto (https://github.com/google/perfetto)
 * with modifications.
 *
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef BTRACE_RING_BUFFER_H_
#define BTRACE_RING_BUFFER_H_

#ifdef __cplusplus

#include <atomic>
#include <optional>
#include <vector>

#include "memory.hpp"
#include "monitor.hpp"
#include "utils.hpp"

namespace btrace {

// A concurrent, multi-writer single-reader ring buffer FIFO, based on a
// circular buffer over shared memory. It has similar semantics to a SEQ_PACKET
// + O_NONBLOCK socket, specifically:
//
// - Writes are atomic, data is either written fully in the buffer or not.
// - New writes are discarded if the buffer is full.
// - If a write succeeds, the reader is guaranteed to see the whole buffer.
// - Reads are atomic, no fragmentation.
// - The reader sees writes in write order (% discarding).
//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// *IMPORTANT*: The ring buffer must be written under the assumption that the
// other end modifies arbitrary shared memory without holding the spin-lock.
// This means we must make local copies of read and write pointers for doing
// bounds checks followed by reads / writes, as they might change in the
// meantime.
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
class RingBuffer {

  public:
    class Buffer {
      public:
        Buffer() {}
        Buffer(uint8_t *d, size_t s)
            : data(d), size(s) {}

        Buffer(const Buffer &) = delete;
        Buffer &operator=(const Buffer &) = delete;

        Buffer(Buffer &&) = default;
        Buffer &operator=(Buffer &&) = default;

        explicit operator bool() const { return data != nullptr; }

        uint8_t *data = nullptr;
        size_t size = 0;
    };

    enum ErrorState : uint64_t {
        kNoError = 0,
        kHitTimeout = 1,
        kInvalidStackBounds = 2,
    };

    RingBuffer(uintptr_t size)
        : read_pos_(0), write_pos_(0), nums_(0), size_(0) {
        size = Utils::RoundUp(size, VirtualMemory::PageSize());
        if (size < VirtualMemory::PageSize()) {
            size = VirtualMemory::PageSize();
        }
        memory_ = VirtualMemory::Allocate(size, "ring-buffer");
        if (memory_ == nullptr) {
            OUT_OF_MEMORY();
            return;
        }

        mem_ = reinterpret_cast<uint8_t *>(memory_->address());
        set_size(size);
    }

    ~RingBuffer() {
        delete memory_;
        memory_ = nullptr;
        size_ = 0;
        mem_ = nullptr;
    }

    RingBuffer(RingBuffer &&) noexcept;
    RingBuffer &operator=(RingBuffer &&) noexcept;

    inline uintptr_t Put(uint8_t *dst, const void *src, size_t size) {
        uintptr_t offset = ((uintptr_t)dst - (uintptr_t)mem_) % size_;

        uintptr_t l = std::min(size, size_ - offset);
        memcpy(mem_ + offset, src, l);
        memcpy(mem_, static_cast<const uint8_t *>(src) + l, size - l);

        return size;
    }

    uintptr_t Put(const void *src, size_t size);

    inline uintptr_t Get(void *dst, const void *src, size_t size) {
        uintptr_t offset =
            ((const uintptr_t)src - (const uintptr_t)mem_) % size_;

        uintptr_t l = std::min(size, size_ - offset);
        memcpy(dst, mem_ + offset, l);
        memcpy(static_cast<uint8_t *>(dst) + l, mem_, size - l);

        return size;
    }

    uintptr_t Get(void *dst, size_t size);

    Buffer BeginWrite(size_t size);

    Buffer BeginOverWrite(size_t size);

    void EndWrite(Buffer buf);

    Buffer BeginRead();
    size_t EndRead(Buffer);

    bool is_valid() const { return !!mem_; }

    size_t size() const { return size_; }

    uint64_t nums() const { return nums_; }

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
        PointerPositions pos;
        pos.write_pos = write_pos_.load(std::memory_order_acquire);
        pos.read_pos = read_pos_.load(std::memory_order_relaxed);
        
        return write_avail(pos) == 0;
    }

  protected:
    static constexpr auto kAlignment = 8; // 64 bits to use aligned memcpy().
    static constexpr auto kHeaderSize = kAlignment; // insert extra size of item in the ring buffer as item
                    // header
    static constexpr auto kGuardSize = 64 * 1024 * 1024; // 64 MB.

    struct PointerPositions {
        uint64_t read_pos;
        uint64_t write_pos;
    };

    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    inline bool IsCorrupt(const PointerPositions &pos) {
//        ASSERT(pos.write_pos % kAlignment == 0);
//        ASSERT(pos.read_pos % kAlignment == 0);
        if (unlikely(pos.write_pos < pos.read_pos ||
                     pos.write_pos - pos.read_pos > size_)) {
            return true;
        }
        return false;
    }

    inline void set_size(size_t size) { size_ = size; }

    inline size_t read_avail(const PointerPositions &pos) {
        auto res = static_cast<size_t>(pos.write_pos - pos.read_pos);
        return res;
    }

    inline size_t write_avail(const PointerPositions &pos) {
        return size_ - read_avail(pos);
    }

    inline uint8_t *at(uint64_t pos) { return mem_ + (pos % size_); }

    std::atomic<uint64_t> read_pos_;
    std::atomic<uint64_t> write_pos_;
    std::atomic<uint64_t> nums_;

    VirtualMemory *memory_ = nullptr;
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
    bool TryWrite(size_t total_size, F fn) {
        uintptr_t p_tid = (uintptr_t)OSThread::PthreadSelf();
        uintptr_t index = p_tid + total_size;
        index = index % concurrency_level_;
        
        RingBuffer *buffer = nullptr;
        RingBuffer::Buffer buf;
        
        bool locked = false;
        bool result = true;

        for (int i = 0; i < concurrency_level_; ++i) 
        {
            // TODO: Only lock BeginWrite() to improve performance.
            if (lock_list_[index]->try_lock()) {
                locked = true;
                buffer = buffer_list_[index];
                buf = buffer->BeginWrite(total_size);
                
                if (buf)
                {
                    break;
                }
  
                buffer->EndWrite(std::move(buf));
                lock_list_[index]->unlock();
                locked = false;
            }
            
            index = (index + 1) % concurrency_level_;
        }

        if (!locked) {
            lock_list_[index]->lock();
            buffer = buffer_list_[index];
            buf = buffer->BeginWrite(total_size);
            
            if (unlikely(!buf))
            {
                goto end;
                result = false;
            }
        }

        fn(buffer, &buf);

    end:
        buffer->EndWrite(std::move(buf));
        lock_list_[index]->unlock();
        
        return result;
    }

  protected:
    
    uintptr_t Lock();
    void Unlock(uintptr_t index) { lock_list_[index]->unlock(); }
    
    const uintptr_t concurrency_level_;
    std::vector<Unfairlock *> lock_list_;
    std::vector<RingBuffer *> buffer_list_;
};

} // namespace btrace

#endif // #ifdef __cplusplus
#endif // BTRACE_RING_BUFFER_H_
