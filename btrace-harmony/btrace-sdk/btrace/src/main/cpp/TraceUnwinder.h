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

#ifndef HARMONY_BTRACE_TRACEUNWINDER_H
#define HARMONY_BTRACE_TRACEUNWINDER_H

#include <signal.h>
#include <hidebug/hidebug_type.h>

#include <csignal>
#include <cstdint>
#include <set>
#include <thread>
#include <atomic>

#include "StackTrace.h"
#include "TraceConfigurations.h"

namespace btrace {

class TraceUnwinder {
public:
    static bool setupSignalHandler();
    static TraceUnwinder& get();
    bool setup(const UnwindConfig& config);
    bool isPaused() { return paused; }
    void pause() { paused = true; }
    void resume() { paused = false; }
    bool cleanup();
    bool isCustomUnwindEnabled() { return enableCustomUnwind; }
    bool isHighFreqEnabled() { return enableHighFreq; }
    bool isOsThreadEnabled() { return enableOsThread; }
    bool isParseElfSymbolsEnabled() { return parseElfSymbols; }
    void backtraceThreadOnce(Type type, void* fp, bool force=false, bool *disable=nullptr);
    void backtraceOnce(Type type, void* fp, bool force = false);
    void batchUnwind(HiDebug_Backtrace_Object obj, std::set<uintptr_t>& pcs,
                     std::ostringstream& methodOss, std::ostringstream& buildIdOss,
                     std::set<std::string>& hasBuildIds);
private:
    static inline int kSigNo = SIGRTMIN + 15; // SIGRTMIN == 35
    static void sig_handler_backtracing(int sig, siginfo_t* si, void* context);
    
    void setupLoopThread();
    
    void backtraceInner(Type type, void* fp, int err);
    void backtraceInnerPreload(pid_t tid, Type type, void* fp);
    void backtraceInnerPreloadForMain(pid_t tid, Type type, void* fp, uint64_t *buffer, size_t size);
    void signalSafetyBacktraceInner(void* fp);

    volatile HiDebug_Backtrace_Object backtraceObject;
    bool forceLoad;
    bool enableCustomUnwind;
    bool disableSigMask = false;
    bool enableHighFreq = false;
    bool parseElfSymbols = false;
    uint64_t mainSampleIntervalNs;
    uint64_t childSampleIntervalNs;
    uint64_t mainAsyncIntervalNs = 0;
    uint64_t childAsyncIntervalNs = 0;
    bool paused{false};
    std::atomic<bool> terminated{false};
    bool enableOsThread = false;
    std::thread *loopThread = nullptr;
};

}

#endif //HARMONY_BTRACE_TRACEUNWINDER_H
