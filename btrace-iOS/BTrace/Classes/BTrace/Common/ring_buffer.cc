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

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cinttypes>
#include <type_traits>

#include "ring_buffer.hpp"

namespace btrace {

RingBuffer::Buffer RingBuffer::BeginWrite(size_t size) {
    Buffer result;

    PointerPositions pos;
    pos.write_pos = write_pos_.load(std::memory_order_acquire);
    pos.read_pos = read_pos_.load(std::memory_order_relaxed);

    if (unlikely(IsCorrupt(pos)))
        return result;

    const uint64_t size_with_header =
        Utils::RoundUp(size + kHeaderSize, kAlignment);

    if (unlikely(size_with_header > write_avail(pos))) {
        return result;
    }

    uint8_t *wr_ptr = at(pos.write_pos);

    result.size = size;
    result.data = wr_ptr + kHeaderSize;

    // set size in the header to 0.
    reinterpret_cast<std::atomic<uint64_t> *>(wr_ptr)->store(
        0, std::memory_order_relaxed);
    // This needs to happen after the store above, so the reader never
    // observes an incorrect byte count.
    write_pos_.fetch_add(size_with_header, std::memory_order_release);
    return result;
}

RingBuffer::Buffer RingBuffer::BeginOverWrite(size_t size) {
    Buffer
        result; // 初始化返回值：首先初始化一个Buffer类型的result变量，它将用于存储返回的写入信息。

    // 获取当前读写位置：通过调用GetPointerPositions函数获取当前的读写位置，并将其存储在pos变量中。如果获取失败（返回std::nullopt），
    // 则直接返回未初始化的result。
    PointerPositions pos;
    pos.write_pos = write_pos_.load(std::memory_order_acquire);
    pos.read_pos = read_pos_.load(std::memory_order_relaxed);

    const uint64_t size_with_header =
        Utils::RoundUp(size + kHeaderSize, kAlignment);

    // size_with_header < size is for catching overflow of size_with_header.
    if (unlikely(size_with_header < size)) {
        return result;
    }
    while (size_with_header > write_avail(pos)) {
        PointerPositions pos;
        pos.write_pos = write_pos_.load(std::memory_order_acquire);
        pos.read_pos = read_pos_.load(std::memory_order_relaxed);

        size_t avail_read = read_avail(pos);

        if (avail_read < kHeaderSize) {
            errno = EAGAIN;
            break; // No data
        }
        uint8_t *rd_ptr = at(pos.read_pos);
        ASSERT(reinterpret_cast<uintptr_t>(rd_ptr) % kAlignment == 0);
        const size_t size =
            reinterpret_cast<std::atomic<uint32_t> *>(rd_ptr)->load(
                std::memory_order_acquire);

        if (size == 0) {
            errno = EAGAIN;
            return Buffer();
        }

        const size_t size_with_header1 =
            Utils::RoundUp(size + kHeaderSize, kAlignment);

        if (avail_read < size_with_header1) {
            errno = EBADF;
            return Buffer();
        }

        rd_ptr += kHeaderSize;
        ASSERT(reinterpret_cast<uintptr_t>(rd_ptr) % kAlignment == 0);
        read_pos_.fetch_add(size_with_header1, std::memory_order_relaxed);
        nums_--;

        pos.read_pos = read_pos_;
    }
    uint8_t *wr_ptr = at(pos.write_pos);

    result.size = size;
    result.data = wr_ptr + kHeaderSize;

    // We can make this a relaxed store, as this gets picked up by the
    // acquire load in GetPointerPositions (and the release store below).
    // set size in the header to 0.
    reinterpret_cast<std::atomic<uint32_t> *>(wr_ptr)->store(
        0, std::memory_order_relaxed);

    // This needs to happen after the store above, so the reader never ob
    // serves an incorrect byte count. This is matched by the acquire load
    // in GetPointerPositions.
    write_pos_.fetch_add(size_with_header, std::memory_order_release);
    return result;
}

RingBuffer::Buffer RingBuffer::BeginRead() {
    PointerPositions pos;
    pos.write_pos = write_pos_.load(std::memory_order_acquire);
    pos.read_pos = read_pos_.load(std::memory_order_relaxed);

    if (IsCorrupt(pos))
        return Buffer();

    size_t avail_read = read_avail(pos);

    if (avail_read < kHeaderSize) {
        errno = EAGAIN;
        return Buffer(); // No data
    }

    uint8_t *rd_ptr = at(pos.read_pos);
    ASSERT(reinterpret_cast<uintptr_t>(rd_ptr) % kAlignment == 0);
    const size_t size = reinterpret_cast<std::atomic<uint32_t> *>(rd_ptr)->load(
        std::memory_order_acquire);
    if (size == 0) {
        errno = EAGAIN;
        return Buffer();
    }
    const size_t size_with_header =
        Utils::RoundUp(size + kHeaderSize, kAlignment);

    if (avail_read < size_with_header) {
        errno = EBADF;
        return Buffer();
    }

    rd_ptr += kHeaderSize;
    ASSERT(reinterpret_cast<uintptr_t>(rd_ptr) % kAlignment == 0);
    return Buffer(rd_ptr, size);
}

uintptr_t RingBuffer::Put(const void *src, size_t size) {
    PointerPositions pos;
    pos.write_pos = write_pos_.load(std::memory_order_acquire);
    pos.read_pos = read_pos_.load(std::memory_order_relaxed);

    if (unlikely(IsCorrupt(pos)))
        return 0;

    if (unlikely(size > write_avail(pos))) {
        return 0;
    }

    uintptr_t offset = pos.write_pos % size_;
    uintptr_t l = std::min(size, size_ - offset);

    memcpy(mem_ + offset, src, l);
    memcpy(mem_, static_cast<const uint8_t *>(src) + l, size - l);

    write_pos_.fetch_add(size);

    return size;
}

uintptr_t RingBuffer::Get(void *dst, size_t size) {
    PointerPositions pos;
    pos.write_pos = write_pos_.load(std::memory_order_acquire);
    pos.read_pos = read_pos_.load(std::memory_order_relaxed);

    if (IsCorrupt(pos))
        return 0;

    size_t avail_read = read_avail(pos);

    if (avail_read < size) {
        return 0; // No data
    }
    uintptr_t offset = pos.read_pos % size_;
    uintptr_t l = std::min(size, size_ - offset);
    memcpy(dst, mem_ + offset, l);
    memcpy(static_cast<uint8_t *>(dst) + l, mem_, size - l);

    read_pos_.fetch_add(size);

    return size;
}

void RingBuffer::EndWrite(Buffer buf) {
    if (!buf)
        return;

    uint8_t *wr_ptr = buf.data - kHeaderSize;
    ASSERT(reinterpret_cast<uintptr_t>(wr_ptr) % kAlignment == 0);

    // This needs to release to make sure the reader sees the payload written
    // between the BeginWrite and EndWrite calls.
    // This is matched by the acquire load in BeginRead where it reads the
    // record's size.
    reinterpret_cast<std::atomic<uint32_t> *>(wr_ptr)->store(
        static_cast<uint32_t>(buf.size), std::memory_order_release);
    nums_++;
}

size_t RingBuffer::EndRead(Buffer buf) {
    if (!buf)
        return 0;

    size_t size_with_header =
        Utils::RoundUp(buf.size + kHeaderSize, kAlignment);
    read_pos_.fetch_add(size_with_header, std::memory_order_relaxed);
    nums_--;
    return size_with_header;
}

RingBuffer::RingBuffer(RingBuffer &&other) noexcept {
    *this = std::move(other);
}

RingBuffer &RingBuffer::operator=(RingBuffer &&other) noexcept {
    std::tie(mem_, size_) = std::tie(other.mem_, other.size_);
    std::tie(other.mem_, other.size_) = std::make_tuple(nullptr, 0);
    return *this;
}

ConcurrentRingBuffer::ConcurrentRingBuffer(uintptr_t size,
                                           uintptr_t concurrency_level)
    : concurrency_level_(concurrency_level) {
    for (int i = 0; i < concurrency_level_; ++i) {
        buffer_list_.emplace_back(new RingBuffer(size / concurrency_level));
        lock_list_.push_back(new Unfairlock());
    }
}

ConcurrentRingBuffer::~ConcurrentRingBuffer() {
    for (int i = 0; i < concurrency_level_; ++i) {
        lock_list_[i]->lock();
    }
    for (int i = 0; i < concurrency_level_; ++i) {
        auto buffer = buffer_list_[i];
        delete buffer;
    }
    for (int i = 0; i < concurrency_level_; ++i) {
        auto lock = lock_list_[i];
        lock->unlock();
        delete lock;
    }
}

uintptr_t ConcurrentRingBuffer::Lock() {
    auto tid = OSThread::GetCurrentThreadId();

    uintptr_t result = tid;
    result ^= result >> 8;
    result ^= result >> 4;
    result = result % concurrency_level_;

    for (int i = 0; i < concurrency_level_; ++i) {
        if (lock_list_[result]->try_lock()) {
            if (!buffer_list_[result]->full()) {
                return result;
            }
            lock_list_[result]->unlock();
        }
        result = (result + 1) % concurrency_level_;
    }

    lock_list_[result]->lock();

    return result;
}

} // namespace btrace
