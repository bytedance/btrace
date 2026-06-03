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

#include "TracePreloader.h"

#include "util/globals.h"
#include "util/string_util.h"
#include "util/time_utils.h"
#include "StackTrace.h"
#include "TraceBuffer.h"
#include "TraceUnwinder.h"
#include "OSThread.h"

#include <cstdint>
#include <hidebug/hidebug.h>
#include <hilog/log.h>
#include <signal.h>

#include <set>
#include <cassert>
#include <algorithm>

#undef LOG_TAG
#define LOG_TAG "btrace:Preloader"

namespace btrace {
static constexpr uint32_t kMaxSize = 128 * 1024;

bool TracePreloader::Start(int64_t mainIntervalNS, int64_t childIntervalNS) {
    if (mtx_ == nullptr) {
        mtx_ = new std::mutex();
    }
    
    if (cv_ == nullptr) {
        cv_ = new std::condition_variable();
    }

    process_period_ms_ = kProcessBufferPeriodMs;
    bool highFreq = TraceUnwinder::get().isHighFreqEnabled();
    
    if (highFreq) {
        process_period_ms_ = kProcessBufferHighPeriodMs;
    }
    
    auto size = CalcBufferSize(mainIntervalNS, childIntervalNS);
    
    std::lock_guard ml(*mtx_);
    
    if (initialized_) { return true; }
    
    if (buffer_ == nullptr) {
        uintptr_t level = 32;
#if DEBUG
        level = 64;
#endif
        buffer_ = new ConcurrentRingBuffer(size, level);
    }
    
    if (method_mapping_ == nullptr) {
        method_mapping_ = new std::unordered_map<uintptr_t, std::shared_ptr<FrameInfo>>();
    }
    
    if (callstack_table_ == nullptr) {
        callstack_table_ = new CallstackTable();
    }
    
    initialized_ = true;
    
    PreparePreloadThread();
    
    return true;
}

bool TracePreloader::Stop() {
    std::thread *thread = nullptr;

    {
        std::lock_guard ml(*mtx_);
    
        if (!initialized_) { return true; }
    
        delete buffer_;
        buffer_ = nullptr;
    
        delete method_mapping_;
        method_mapping_ = nullptr;
        
        delete callstack_table_;
        callstack_table_ = nullptr;
    
        initialized_ = false;
    
        thread = preload_thread_;
    }
    
    if (thread != nullptr) {
        cv_->notify_one();
        thread->join();
    }
    
    return true;
}

void TracePreloader::TimerAction(union sigval) {
    if (TraceUnwinder::get().isPaused()) { return; }
    
    OSThread *os_thread = nullptr;
    if (TraceUnwinder::get().isOsThreadEnabled() && (os_thread = OSThread::Current())) {
        os_thread->disableLogging();
    }
}

void TracePreloader::PreparePreloadThread() {
    preload_thread_ = new std::thread([]{
        pthread_setname_np(pthread_self(), "btrace_preload");
        
        int64_t cost = 0;
        int64_t sleep_ms = process_period_ms_;
        
        OSThread *os_thread = nullptr;
        if (TraceUnwinder::get().isOsThreadEnabled() && (os_thread = OSThread::Current())) {
            os_thread->disableLogging();
        }
        
        while(true) {
            std::unique_lock<std::mutex> lock(*mtx_);
            cv_->wait_for(lock, std::chrono::milliseconds(sleep_ms));
            
            if (!initialized_) { break; }
            
            if (TraceUnwinder::get().isPaused()) {
                sleep_ms = process_period_ms_;
                continue;
            }
            
            cost = ProcessBufferNoLock();
            
            sleep_ms = std::max((int64_t)0, process_period_ms_-cost);
        }
    });
}

int64_t TracePreloader::CalcBufferSize(int64_t mainIntervalNS, int64_t childIntervalNS) {
    assert(0 < mainIntervalNS);
    
    int64_t common = (sizeof(Sample) + sizeof(uint64_t) * kAvgDepth) *
                      process_period_ms_ * kNanosPerMilli;
    int64_t res = common * kMainDepthFactor / mainIntervalNS;
    
    if (0 < childIntervalNS) {
        res += common * kMaxThreads / childIntervalNS;
    }
    
    return res;
}

bool TracePreloader::Write(Sample &sample, uint64_t timestamp) {
    return PutSample(buffer_, sample, timestamp);
}

int64_t TracePreloader::ProcessBuffer() {
    std::lock_guard ml(*mtx_);
    if (!initialized_) { return 0; }
    
    return ProcessBufferNoLock();
}

int64_t TracePreloader::ProcessBufferNoLock() {
#if DEBUG
    OH_LOG_DEBUG(LOG_APP, "start preload process buffer");
#endif
    uint64_t nums = 0;
    uint64_t monoStart = current_monotime_millis();

    auto overwritten_stacks = std::vector<uint64_t>();
    
    nums = buffer_->Iterate([&overwritten_stacks](RingBuffer *ring_buffer, size_t num){
        uintptr_t buf[kMaxMainStackDepth];
        Sample sample;
        sample.pcs = buf;
        
        if (!GetSample(ring_buffer, sample)) { return; }
        if (sample.depth == 0) { return; }
        
        uint64_t stack_id = callstack_table_->insert(sample.pcs, sample.depth, 
                                                     [](uint64_t address, uint32_t refcount) {
            auto it = method_mapping_->find(address);
            if (it == method_mapping_->end()) {
#if DEBUG
#else
                if (kMaxSize < method_mapping_->size()) {
                    return;
                }
#endif
                bool inserted;
                std::tie(it, inserted) = method_mapping_->emplace(address, std::make_shared<FrameInfo>());
            }

            auto *frameInfo = it->second.get();
            frameInfo->refcount += refcount;
        });
        
        if (stack_id == 0) { return; }
        
        auto record = SampleRecord(sample, stack_id);

        TraceBuffer::get().preloadOverWrite(record, [&overwritten_stacks](const SampleRecord &old){
            if (old.stack_id != 0) {
                overwritten_stacks.push_back(old.stack_id);
            }
        });
    });
    
    for (auto &stack_id : overwritten_stacks) {
        callstack_table_->decrementStackRef(stack_id, 1, [](uint64_t address, uint32_t refcount) {
            auto it = method_mapping_->find(address);
            if (it == method_mapping_->end()) { return; }
            
            auto &frame_info = it->second;
            if (frame_info->refcount <= refcount) {
                frame_info->refcount = 0;
            } else {
                frame_info->refcount -= refcount;
            }
            
            if (frame_info->refcount == 0) {
                method_mapping_->erase(it);
            }
        });
    }
    
    uint64_t monoEnd = current_monotime_millis();
#if DEBUG
    OH_LOG_DEBUG(LOG_APP, "finish preload process buffer, nums: %{public}zu, cost: %{public}zums", nums, (monoEnd - monoStart));
#endif
    
    return monoEnd - monoStart;
}

void TracePreloader::SymbolicAddress(HiDebug_Backtrace_Object obj, std::set<uintptr_t>& pcs, void *args,
                                     OH_HiDebug_SymbolicAddressCallback callback, bool forceLoad, bool parseElfSymbols) {
    std::lock_guard ml(*mtx_);
    if (!initialized_) { return; }
    
    for (uintptr_t pc : pcs) {
        auto it = method_mapping_->find(pc);
        std::shared_ptr<FrameInfo> frameInfo = nullptr;
        
        if (it == method_mapping_->end()) {
            frameInfo = std::make_shared<FrameInfo>();
            
            bool reachMax = false;
#if DEBUG
#else
            reachMax = (kMaxSize < method_mapping_->size());
#endif
            if (!reachMax) {
                bool inserted;
                std::tie(it, inserted) = method_mapping_->emplace(pc, frameInfo);
            }
        } else {
            frameInfo = it->second;
        }
        
        assert(frameInfo != nullptr);
        
        if (!frameInfo->success && frameInfo->try_cnt < 2) {
            frameInfo->try_cnt += 1;
            
            HiDebug_ErrorCode ret = OH_HiDebug_SymbolicAddress(obj, (void*)pc, (void*)frameInfo.get(),
                                    [](void* pc, void* args, const HiDebug_StackFrame* stackFrame){
                auto *frameInfo = (FrameInfo *)args;
        
                if (stackFrame->type == HIDEBUG_STACK_FRAME_TYPE_JS) {
                    frameInfo->isJsFrame = true;
                    frameInfo->column = stackFrame->frame.js.column;
                    frameInfo->funcName = stackFrame->frame.js.functionName;
                    frameInfo->dfxMapName = stackFrame->frame.js.mapName;
                    frameInfo->packageName = stackFrame->frame.js.packageName;
                    frameInfo->mapName = stackFrame->frame.js.url;
                    frameInfo->relPc = stackFrame->frame.js.relativePc;
                    frameInfo->line = stackFrame->frame.js.line;
                } else {
                    frameInfo->buildId = stackFrame->frame.native.buildId;
                    frameInfo->funcOffset = stackFrame->frame.native.funcOffset;
                    frameInfo->funcName = stackFrame->frame.native.functionName;
                    frameInfo->mapName = stackFrame->frame.native.mapName;
                    frameInfo->relPc = stackFrame->frame.native.relativePc;
                }

                if (0 < frameInfo->funcName.size() || 0 < frameInfo->buildId.size()) {
                    frameInfo->success = true;
                }
            });
        }
        
        HiDebug_StackFrame stackFrame{};
        
        if (frameInfo->isJsFrame) {
            stackFrame.type = HiDebug_StackFrameType::HIDEBUG_STACK_FRAME_TYPE_JS;
            stackFrame.frame.js.column = frameInfo->column;
            stackFrame.frame.js.functionName = frameInfo->funcName.c_str();
            stackFrame.frame.js.mapName = frameInfo->dfxMapName.c_str();
            stackFrame.frame.js.packageName = frameInfo->packageName.c_str();
            stackFrame.frame.js.url = frameInfo->mapName.c_str();
            stackFrame.frame.js.relativePc = frameInfo->relPc;
            stackFrame.frame.js.line = frameInfo->line;
        } else {
            stackFrame.type = HiDebug_StackFrameType::HIDEBUG_STACK_FRAME_TYPE_NATIVE;
            stackFrame.frame.native.buildId = frameInfo->buildId.c_str();
            stackFrame.frame.native.funcOffset = frameInfo->funcOffset;
            stackFrame.frame.native.functionName = frameInfo->funcName.c_str();
            stackFrame.frame.native.mapName = frameInfo->mapName.c_str();
            stackFrame.frame.native.relativePc = frameInfo->relPc;
            stackFrame.frame.native.reserved = nullptr;
        }
        callback((void*)pc, args, &stackFrame);
    }
}

std::vector<uint64_t> TracePreloader::GetStackById(uint64_t stack_id) {
    return callstack_table_->query(stack_id);
}
}