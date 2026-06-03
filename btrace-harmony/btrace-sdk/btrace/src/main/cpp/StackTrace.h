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
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef HARMONY_BTRACE_STACKTRACE_H
#define HARMONY_BTRACE_STACKTRACE_H

#include <cstddef>
#include <cstdint>

#include "RingBuffer.h"
#include "util/globals.h"


namespace btrace {

enum class Type : uint32_t {
    kInvalid = 0,
    kSignal,
    kMalloc,
    kCalloc,
    kRealloc,
    kFree,
    kMmap,
    kMadvise,
    kMsync,
    kMunmap,
    kMemset,
    kMemcpy,
    kMemcmp,
    kMemmem,
    kMemmove,
    kStrstr,
    kStrchr,
    kStrlen,
    kOpen,
    kRead,
    kWrite,
    kClose,
    // "Input" socket interfaces
    kAccept,
    kRecv,
    kRecvfrom,
    kRecvmmsg,
    kRecvmsg,
    // "Output" socket interfaces
    kConnect,
    kSend,
    kSendto,
    kSendmsg,
    // Interfaces used to wait for signals
    kPause,
    kSigsuspend,
    kSigtimedwait,
    kSigwaitinfo,
    // File descriptor multiplexing interfaces
    kEpollWait,
    kEpollPwait,
    kPoll,
    kPpoll,
    kSelect,
    kPselect,
    // System V IPC interfaces
    kMsgrcv,
    kMsgsnd,
    kSemop,
    kSemtimedop,
    // Sleep interfaces
    kClockNanosleep,
    kNanosleep,
    kUsleep,
    kSleep,
//    // Others
//    kIoGetevents,
};

struct SampleData {
    Type type;
    int32_t tid;
    uint64_t timestamp;
    uint64_t cpuTime;

    SampleData() = default;

    SampleData(Type type, int32_t tid, uint64_t timestamp, uint64_t cpuTime):
                type(type), tid(tid), timestamp(timestamp), cpuTime(cpuTime) {}
};

struct Sample {
    SampleData data;
    uint32_t depth = 0;
    uintptr_t *pcs = nullptr;

    Sample() = default;

    Sample(Type type, int32_t tid, uint64_t timestamp, uint64_t cpuTime, uint32_t depth, uintptr_t *pcs):
        data(type, tid, timestamp, cpuTime), depth(depth), pcs(pcs) {}

    Sample(SampleData data, uint32_t depth, uintptr_t *pcs):
        data(data), depth(depth), pcs(pcs) {}
};

struct SampleRecord {
    SampleData data;
    uint64_t stack_id;
    
    SampleRecord(): data(), stack_id(0) {}
    
    SampleRecord(const Sample &sample, uint64_t stack_id):
                data(sample.data), stack_id(stack_id) {}
};

struct StackTrace {
    Type type;
    int32_t tid;
    uint64_t timestamp;
    uint64_t cpuTime;
    uint32_t depth;
    uintptr_t elements[kMaxStackDepth];
};

template <typename SampleClass>
bool PutSample(ConcurrentRingBuffer *concurr_buffer, SampleClass &sample, uint64_t timestamp) {
    bool result = false;

    constexpr size_t pc_size = sizeof(decltype(sample.pcs));
    size_t total_size = sizeof(sample.data) + sample.depth * pc_size;

    result = concurr_buffer->TryWrite(timestamp, &sample, total_size,
        [](RingBuffer *buffer, RingBuffer::Buffer &buf, void *src) {
        SampleClass *sample = reinterpret_cast<SampleClass *>(src);
        buffer->CopyIn(buf.data, &sample->data, sizeof(sample->data));
        buffer->CopyIn(buf.data + sizeof(sample->data), sample->pcs,
                  sample->depth * pc_size);
    });
    
    return result;
}

template <typename SampleClass>
bool ReadSampleBuffer(const RingBuffer *ring, const RingBuffer::Buffer &buffer,
                      SampleClass &sample) {
    char *buf = reinterpret_cast<char *>(buffer.data);
    size_t size = buffer.size;
    char *end = buf + size;
    
    using SampleDataClass = decltype(sample.data);
    SampleDataClass *data;
    
    if (!RingBuffer::ViewAndAdvance<SampleDataClass>(&buf, &data, end)) {
        return false;
    }
    
    ring->CopyOut(&(sample.data), data, sizeof(sample.data));
    
    if (end < buf) { return false; }
    
    constexpr size_t pc_size = sizeof(typename get_type<decltype(sample.pcs)>::type);
    ring->CopyOut(sample.pcs, buf, static_cast<size_t>(end - buf));
    sample.depth = (uint32_t)((size_t)(end - buf) / pc_size);

    return true;
}

template <typename SampleClass>
bool GetSample(RingBuffer *buffer, SampleClass &sample) {
    bool result = false;

    RingBuffer::Buffer buf;
    buf = buffer->BeginRead();

    if (!buf) { return false; }
    
    result = ReadSampleBuffer(buffer, buf, sample);
    buffer->EndRead(std::move(buf));

    return result;
}

}

#endif //HARMONY_BTRACE_STACKTRACE_H
