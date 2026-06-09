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

#ifndef BTRACE_HARMONY_TRACEPRELOADER_H
#define BTRACE_HARMONY_TRACEPRELOADER_H

#include <bits/alltypes.h>
#include <hidebug/hidebug.h>
#include <hidebug/hidebug_type.h>
#include <set>
#include <signal.h>

#include <mutex>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "CallstackTable.h"
#include "RingBuffer.h"
#include "StackTrace.h"

namespace btrace {
class TracePreloader {
public:
    using WriteFunc = size_t(RingBuffer *, RingBuffer::Buffer &);
    
    struct FrameInfo {
        bool success{false};
        int32_t try_cnt{0};
        uint32_t refcount{0};
        // info
        bool isJsFrame{false};
        uint64_t relPc{0};
        uint64_t mapOffset{0};
        uint64_t funcOffset{0};
        int32_t line {0};
        int32_t column {0};
        std::string mapName{""};
        std::string funcName{""};
        std::string buildId{""};
        std::string dfxMapName{""};
        std::string packageName{""};
    };
    
    static bool Start(int64_t mainIntervalNS, int64_t childIntervalNS);
    static bool Stop();
    static int64_t ProcessBuffer();
    static bool Write(Sample &sample, uint64_t timestamp);
    static bool ReserveWrite(uint64_t timestamp, size_t total_size, WriteFunc fn);
    static std::vector<uint64_t> GetStackById(uint64_t stack_id);
    static void SymbolicAddress(HiDebug_Backtrace_Object obj, std::set<uintptr_t>& pcs, void *args,
                                OH_HiDebug_SymbolicAddressCallback callback, bool forceLoad=false, bool parseElfSymbols=false);
    static void Lock() { mtx_->lock(); }
    static void Unlock() { mtx_->unlock(); }
private:
    static constexpr uintptr_t kAvgDepth = 32;
    static constexpr uintptr_t kMainDepthFactor = 8;
    static constexpr uintptr_t kMaxThreads = 128;
    static constexpr int64_t kProcessBufferPeriodMs = 300;
    static constexpr int64_t kProcessBufferHighPeriodMs = 50;
    static constexpr int64_t kPreloadSymbolsPeriodS = 60;

    static void PreloadSymbols();
    static bool RegisterTimer();
    static void TimerAction(union sigval);
    
    static void PreparePreloadThread();
    static int64_t ProcessBufferNoLock();
    
    static int64_t CalcBufferSize(int64_t mainIntervalNS, int64_t childIntervalNS);
    
    static inline int64_t process_period_ms_ = kProcessBufferPeriodMs;
    static inline std::thread *preload_thread_ = nullptr;
    static inline ConcurrentRingBuffer *buffer_ = nullptr;
    
    static inline std::mutex *mtx_ = nullptr;
    static inline std::condition_variable *cv_ = nullptr;
    static inline bool initialized_ = false;
    
    static inline CallstackTable *callstack_table_ = nullptr;
    static inline std::unordered_map<uintptr_t, std::shared_ptr<FrameInfo>> *method_mapping_ = nullptr;
};
}



#endif //BTRACE_HARMONY_TRACEPRELOADER_H
