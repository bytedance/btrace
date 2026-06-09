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
// Created on 2025/10/9.
//


#include "TraceAgent.h"
#include "ThreadLocalStorage.h"
#include "TraceBuffer.h"
#include "TraceDumper.h"
#include "TraceHooks.h"
#include "TraceUnwinder.h"
#include "OSThread.h"
#include "util/time_utils.h"

#include <cstdint>
#include <hilog/log.h>


#undef LOG_TAG
#define LOG_TAG "btrace:TraceAgent"

namespace btrace {

TraceAgent& TraceAgent::get() {
    static TraceAgent inst{};
    return inst;
}

bool TraceAgent::start() {
    auto& configs = TraceConfigurations::get();
    if (!configs.isEnabled()) { return false; }
    
    std::lock_guard<std::mutex> lock(mutex);
    if (State::IDLE < state) { return state == State::STARTED; }
    
    startMonoTimeMs_ = current_monotime_millis();
    startUtcTimeMs_ = current_realtime_mills();
    
    int setupCode = setup(configs);
    if (setupCode != 0) {
        OH_LOG_ERROR(LOG_APP, "setup failure: %{public}d", setupCode);
        state = State::ERROR;
        return false;
    }
    
    state = State::STARTED;
    OH_LOG_INFO(LOG_APP, "start succeed");
    return true;
}

void TraceAgent::pause() {
    TraceUnwinder::get().pause();
}

void TraceAgent::resume() {
    TraceUnwinder::get().resume();
}

bool TraceAgent::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    if (state <= State::ERROR) { return false; }
    
    int cleanupCode = cleanup();
    
    if (cleanupCode != 0) {
        OH_LOG_ERROR(LOG_APP, "cleanup failure: %{public}d", cleanupCode);
        state = State::ERROR;
        return false;
    }
    state = State::IDLE;
    OH_LOG_INFO(LOG_APP, "stop succeed");
    return true;
}

bool TraceAgent::dumpTrace(std::string& traceDir, std::string& extra, const std::vector<pid_t> &tids,
                   int64_t beginTimeMs, int64_t endTimeMs) {
    std::lock_guard<std::mutex> lock(mutex);
    if (state != State::STARTED) { return false; }
    
    if (beginTimeMs) {
        beginTimeMs = beginTimeMs - startUtcTimeMs_ + startMonoTimeMs_;
    }

    if (endTimeMs) {
        endTimeMs = endTimeMs - startUtcTimeMs_ + startMonoTimeMs_;
    }

    bool res = TraceDumper::get().dump(TraceBuffer::get(), TraceUnwinder::get(), traceDir,
                                   extra, tids, beginTimeMs, endTimeMs) > 0;
    return res;
}

class ScopeGuard {
public:
    explicit ScopeGuard(std::function<void()> cleanup) 
        : cleanup_(std::move(cleanup)) {}
    
    explicit ScopeGuard(ScopeGuard&& guard): cleanup_(guard.cleanup_) {
        guard.cleanup_ = nullptr;
    }
    
    ~ScopeGuard() { if (cleanup_) cleanup_(); }
    
    void dismiss() { cleanup_ = nullptr; }
    
private:
    std::function<void()> cleanup_;
    
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard operator=(const ScopeGuard&) = delete;
};

int TraceAgent::setup(TraceConfigurations& configs) {
    std::vector<ScopeGuard> guards;
    
    if (!TraceBuffer::get().setup(configs.getBufferConfig())) {
        return -1;
    }
    guards.emplace_back([] { TraceBuffer::get().cleanup(); });
    
    if (!ThreadLocalStorage::get().setup()) {
        return -2;
    }
    guards.emplace_back([] { ThreadLocalStorage::get().cleanup(); });
    
    if (!TraceUnwinder::get().setup(configs.getUnwindConfig())) {
        return -3;
    }
    guards.emplace_back([] { TraceUnwinder::get().cleanup(); });
    
    if (!TraceHooks::get().setup(configs.getHookConfig(), [] (Type type, void* fp) {
            TraceUnwinder::get().backtraceOnce(type, fp);
    })) {
        return -4;
    }
    guards.emplace_back([] { TraceHooks::get().cleanup(); });
    
    for (auto& guard : guards) {
        guard.dismiss();
    }
    
    return 0;
}

int TraceAgent::cleanup() {
    TraceHooks::get().cleanup();
    TraceUnwinder::get().cleanup();
    ThreadLocalStorage::get().cleanup();
    TraceBuffer::get().cleanup();
    return 0;
}

}