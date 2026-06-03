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

#include "RingBuffer.h"
#include <cassert>
#include <cstdint>
#include <sys/types.h>

namespace btrace {

RingBuffer::Buffer RingBuffer::BeginWrite(size_t size) {
    Buffer result;

    PointerPositions pos = GetPointerPositions();

    const uint64_t size_with_header = common::round_up(size + kHeaderSize, kAlignment);

    if (size_with_header > write_avail(pos)) {
        return result;
    }

    uint8_t *wr_ptr = at(pos.write_pos);

    result.size = size;
    result.data = wr_ptr + kHeaderSize;

    // set size in the header to 0.
    auto atomic_wr_ptr = reinterpret_cast<std::atomic<uint64_t> *>(wr_ptr);
    atomic_wr_ptr->store(0, std::memory_order_relaxed);
    // This needs to happen after the store above, so the reader never
    // observes an incorrect byte count.
    write_pos_.fetch_add(size_with_header, std::memory_order_release);
    return result;
}

RingBuffer::Buffer RingBuffer::BeginOverWrite(size_t size, OverWriteCallBack cb) {
    Buffer result;

    PointerPositions pos = GetPointerPositions(std::memory_order_relaxed);
    const uint64_t size_header = common::round_up(size + kHeaderSize, kAlignment);

    uint64_t new_write_pos = pos.write_pos + size_header;
    uint64_t overwrite_pos = pos.read_pos;

    // read old data
    while (size_ < new_write_pos - overwrite_pos) {
        uint8_t *rd_ptr = at(overwrite_pos);
        auto rd_header_ptr = (uint64_t *)rd_ptr;
        const size_t size = *rd_header_ptr;
        if (0 == size) { return result; }
    
        rd_ptr += kHeaderSize;
        Buffer rd_buf(rd_ptr, size);
        cb(this, rd_buf);
        nums_.fetch_sub(1, std::memory_order_relaxed);
    
        const size_t rd_size_header = common::round_up(size + kHeaderSize, kAlignment);
        overwrite_pos += rd_size_header;
    }
    read_pos_.store(overwrite_pos, std::memory_order_release);
    // end read

    uint8_t *wr_ptr = at(pos.write_pos);

    result.size = size;
    result.data = wr_ptr + kHeaderSize;

    // set size in the header to 0.
    auto atomic_wr_ptr = reinterpret_cast<std::atomic<uint64_t> *>(wr_ptr);
    atomic_wr_ptr->store(0, std::memory_order_relaxed);
    // This needs to happen after the store above, so the reader never
    // observes an incorrect byte count.
    write_pos_.fetch_add(size_header, std::memory_order_release);
    return result;
}

RingBuffer::Buffer RingBuffer::BeginReserveWrite(size_t size) {
    Buffer result;

    PointerPositions pos = GetPointerPositions();

    const uint64_t size_with_header = common::round_up(size + kHeaderSize, kAlignment);

    if (write_avail(pos) < size_with_header) {
        return result;
    }

    uint8_t *wr_ptr = at(pos.write_pos);

    result.size = size;
    result.data = wr_ptr + kHeaderSize;
    
    return result;
}

RingBuffer::Buffer RingBuffer::BeginRead() {
    PointerPositions pos = GetPointerPositions();
    size_t avail_read = read_avail(pos);

    if (avail_read < kHeaderSize) { return Buffer(); } // No data

    uint8_t *rd_ptr = at(pos.read_pos);
    assert(reinterpret_cast<uintptr_t>(rd_ptr) % kAlignment == 0);
    auto atomic_rd_ptr = reinterpret_cast<std::atomic<uint64_t> *>(rd_ptr);
    const size_t size = atomic_rd_ptr->load(std::memory_order_acquire);
    if (size == 0) { return Buffer(); }

    const size_t size_header = common::round_up(size + kHeaderSize, kAlignment);

    if (avail_read < size_header) { return Buffer(); }

    rd_ptr += kHeaderSize;
    assert(reinterpret_cast<uintptr_t>(rd_ptr) % kAlignment == 0);
    return Buffer(rd_ptr, size);
}

RingBuffer::Buffer RingBuffer::BeginPeek(uint64_t read_pos) {
    PointerPositions pos = {
        .read_pos = read_pos,
        .write_pos = write_pos_.load(std::memory_order_acquire)
    };
    size_t avail_read = read_avail(pos);

    if (avail_read < kHeaderSize) { return Buffer(); } // No data

    uint8_t *rd_ptr = at(pos.read_pos);
    if (read_pos % 8 != 0) {
        int x = 0;
    }
    assert(reinterpret_cast<uintptr_t>(rd_ptr) % kAlignment == 0);
    auto atomic_rd_ptr = reinterpret_cast<std::atomic<uint64_t> *>(rd_ptr);
    const size_t size = atomic_rd_ptr->load(std::memory_order_acquire);
    if (size == 0) { return Buffer(); }

    const size_t size_header = common::round_up(size + kHeaderSize, kAlignment);

    if (avail_read < size_header) { return Buffer(); }

    rd_ptr += kHeaderSize;
    assert(reinterpret_cast<uintptr_t>(rd_ptr) % kAlignment == 0);
    return Buffer(rd_ptr, size);
}

uintptr_t RingBuffer::Put(const void *src, size_t size) {
    PointerPositions pos = GetPointerPositions();

    if (write_avail(pos) < size) { return 0; }

    auto dst = at(pos.write_pos);
    CopyIn(dst, src, size);

    write_pos_.store(pos.write_pos+size, std::memory_order_release);
    nums_.fetch_add(1, std::memory_order_relaxed);

    return size;
}

uintptr_t RingBuffer::OverPut(const void *src, size_t size, OverWriteCallBack cb) {
    PointerPositions pos = GetPointerPositions();

    if (write_avail(pos) < size) {
        uint8_t *rd_ptr = at(pos.read_pos);
        auto rd_buf = Buffer(rd_ptr, size);
        cb(this, rd_buf);
        read_pos_.store(pos.read_pos+size, std::memory_order_release);
        nums_.fetch_sub(1, std::memory_order_relaxed);
    }
    
    auto dst = at(pos.write_pos);
    CopyIn(dst, src, size);

    write_pos_.store(pos.write_pos+size, std::memory_order_release);
    nums_.fetch_add(1, std::memory_order_relaxed);

    return size;
}

uintptr_t RingBuffer::Get(void *dst, size_t size) {
    PointerPositions pos = GetPointerPositions();
    if (read_avail(pos) < size) { return 0; }
    
    auto src = at(pos.read_pos);
    CopyOut(dst, src, size);

    read_pos_.store(pos.read_pos + size, std::memory_order_release);
    nums_.fetch_sub(1, std::memory_order_relaxed);

    return size;
}

void RingBuffer::EndWrite(Buffer buf) {
    assert(buf);

    uint8_t *wr_ptr = buf.data - kHeaderSize;
    assert(reinterpret_cast<uintptr_t>(wr_ptr) % kAlignment == 0);

    // This needs to release to make sure the reader sees the payload written
    // between the BeginWrite and EndWrite calls.
    // This is matched by the acquire load in BeginRead where it reads the
    // record's size.
    auto atomic_wr_ptr = reinterpret_cast<std::atomic<uint64_t> *>(wr_ptr);
    atomic_wr_ptr->store(buf.size, std::memory_order_release);
    nums_.fetch_add(1, std::memory_order_relaxed);
}

void RingBuffer::EndReserveWrite(Buffer buf, size_t used) {
    assert(buf);
    assert(used <= buf.size);

    uint64_t *wr_ptr = reinterpret_cast<uint64_t *>(buf.data - kHeaderSize);
    assert(reinterpret_cast<uintptr_t>(wr_ptr) % kAlignment == 0);
    *wr_ptr = used;
    
    const uint64_t size_with_header = common::round_up(used + kHeaderSize, kAlignment);
    write_pos_.fetch_add(size_with_header, std::memory_order_relaxed);
    nums_.fetch_add(1, std::memory_order_relaxed);
}

size_t RingBuffer::EndRead(const Buffer &buf) {
    assert(buf);

    size_t size_header = common::round_up(buf.size + kHeaderSize, kAlignment);
    read_pos_.fetch_add(size_header, std::memory_order_relaxed);
    nums_.fetch_sub(1, std::memory_order_relaxed);
    return size_header;
}

size_t RingBuffer::EndPeek(const Buffer &buf) {
    assert(buf);

    size_t size_header = common::round_up(buf.size + kHeaderSize, kAlignment);
    return size_header;
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
        lock_list_.push_back(new Spinlock());
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

}