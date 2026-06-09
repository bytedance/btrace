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
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef HARMONY_BTRACE_TRACEAGENT_H
#define HARMONY_BTRACE_TRACEAGENT_H

#include "TraceConfigurations.h"
#include <bits/alltypes.h>
#include <mutex>
#include <vector>

namespace btrace {

class TraceAgent {
public:
    static TraceAgent& get();
    bool start();
    void pause();
    void resume();
    bool stop();
    bool dumpTrace(std::string& targetTraceDir, std::string& extra, const std::vector<pid_t> &tids,
                   int64_t beginTimeMs=0, int64_t endTimeMs=0);
private:
    TraceAgent() = default;
    TraceAgent(const TraceAgent&) = delete;
    TraceAgent& operator=(const TraceAgent&) = delete;
    
    int setup(TraceConfigurations& configs);
    int cleanup();
    
    enum class State : uint8_t {
        IDLE = 0,
        ERROR,
        STARTED,
        PAUSED,
    };
    
    uint64_t startMonoTimeMs_ = 0;
    uint64_t startUtcTimeMs_ = 0;
    State state = State::IDLE;
    std::mutex mutex;
};

}


#endif //HARMONY_BTRACE_TRACEAGENT_H
