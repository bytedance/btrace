/*
 * Copyright (C) 2021 ByteDance Inc
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
#pragma once

#include <cstdint>
#include <memory>
#include "Stack.h"
#include <unordered_set>

namespace rheatrace {

enum class SamplingType {
    kInvalid = 1,
    kBinder,
    kJankMessage,
    kCustom,
    kTraceStack,
    kWait,
    kPark,
    kMonitor,
    kObjectAllocation,
    kJNITrampoline,
    kGC,
    kMutex,
    kDispatchVsync,
    kSyncAndDrawFrame,
    kTraceArg,
    kFlush,
    kUnpark,
    kScene,
    kGCInternal,
    kLoadLibrary,
    kNativePollOnce,
    kNotify,
    kUnlock,
};

struct SamplingRecord {
    SamplingType mType;
    uint16_t mTid;
    uint32_t mMessageId;
    uint64_t mNanoTime; // when mType is kUnpark/kNotify/kUnlock, this property represents the time of corresponding park/wait/lock
    uint64_t mCpuTime;
    uint64_t mEndNanoTime;
    uint64_t mEndCpuTime;
    uint64_t mAllocatedObjects;
    uint64_t mAllocatedBytes;
    uint32_t mMajFlt;
    uint32_t mNvCsw;
    uint32_t mNivCsw;
    Stack mStack;

    uint32_t size() {
        return 2 + 2 + 4 + 8 * 6 + 4 * 3 + mStack.size();
    }

    static uint32_t maxBytes() {
        return 2 + 2 + 4 + 8 * 6 + 4 * 3 + Stack::maxSize();
    }

    uint32_t encodeInto(char* out, std::unordered_set<uint64_t>* set) {
        int size = 0;
        size += rheatrace::writeBuf(out + size, (uint16_t) mType);
        size += rheatrace::writeBuf(out + size, (uint16_t) mTid);
        size += rheatrace::writeBuf(out + size, mMessageId);
        size += rheatrace::writeBuf(out + size, mNanoTime);
        size += rheatrace::writeBuf(out + size, mEndNanoTime);
        size += rheatrace::writeBuf(out + size, mCpuTime);
        size += rheatrace::writeBuf(out + size, mEndCpuTime);
        size += rheatrace::writeBuf(out + size, mAllocatedObjects);
        size += rheatrace::writeBuf(out + size, mAllocatedBytes);
        size += rheatrace::writeBuf(out + size, mMajFlt);
        size += rheatrace::writeBuf(out + size, mNvCsw);
        size += rheatrace::writeBuf(out + size, mNivCsw);
        size += mStack.encodeInfo(out + size, set);
        return size;
    }
};

} // namespace rheatrace