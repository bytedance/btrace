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

#include "TraceUnwinder.h"
#include "StackTrace.h"
#include "TraceBuffer.h"
#include <bits/alltypes.h>
#include <signal.h>
#include <cstdint>
#include <hilog/log.h>
#include <sys/resource.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <hidebug/hidebug.h>
#include <thread>
#include "util/thread_util.h"
#include "util/time_utils.h"
#include <sstream>
#include <atomic>
#include <sys/syscall.h>
#include <sys/resource.h>

#include "ThreadLocalStorage.h"
#include "util/time_utils.h"
#include "OSThread.h"
#include "StackTrace.h"
#include "TracePreloader.h"

#include "SlowSyscallProxy.h"

#undef LOG_TAG
#define LOG_TAG "btrace:Unwinder"

namespace btrace {

using UpdateTimer = void(*)();

void TraceUnwinder::sig_handler_backtracing(int sig, siginfo_t* si, void* context) {
    if (sig != kSigNo) { return; }

    auto startFp = reinterpret_cast<ucontext_t *>(context)->uc_mcontext.regs[29];
    TraceUnwinder::get().signalSafetyBacktraceInner(reinterpret_cast<void*>(startFp));
}

bool TraceUnwinder::setupSignalHandler() {
    struct sigaction sa{0};
    sigfillset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
    sa.sa_sigaction = sig_handler_backtracing;
    
    if (sigaction(kSigNo, &sa, nullptr) == -1) {
        OH_LOG_ERROR(LOG_APP, "register backtrace signal handler failed: %{public}s", strerror(errno));
        return false;
    }
    return true;
}

TraceUnwinder& TraceUnwinder::get() {
    static TraceUnwinder inst{};
    return inst;
}

bool TraceUnwinder::setup(const UnwindConfig& config) {
    uint64_t unit = 1000 * 1000;

    if (config.isHighFreq()) {
        unit = 1000;
        enableHighFreq = true;
    }

    mainSampleIntervalNs = config.getMainThreadSampleInterval() * unit;
    childSampleIntervalNs = config.getSubThreadSampleInterval() * unit;
    enableOsThread = config.isEnableOsThread();
    disableSigMask = config.isDisableSigMask();
    parseElfSymbols = config.isParseElfSymbolsEnabled();
    backtraceObject = OH_HiDebug_CreateBacktraceObject();
    
    auto bufferedTime = config.getBufferedTime();
    set_buffered_monotime_nanos_force_update(bufferedTime);
    
    if (backtraceObject == nullptr) { return false; }
    
    forceLoad = config.isForceLoad();
    terminated = false;
    
    if (enableOsThread) {
        bool res = OSThread::Start();
        if (!res) { return false; }
    }
    
    if (TraceBuffer::get().isPreloadEnabled()) {
        bool res = TracePreloader::Start(mainSampleIntervalNs, childSampleIntervalNs);
        if (!res) { return false; }
    }
    
    if (!config.isSignalBacktraceDisabled()) {
        mainAsyncIntervalNs = config.getMainAsyncInterval() * unit;
        childAsyncIntervalNs = config.getChildAsyncInterval() * unit;
        
        bool res = setupSignalHandler();
        
        if (!res) { return false; }
        
        if (enableOsThread && 0 < mainAsyncIntervalNs) {
            if (!SlowSysCallProxy::Setup(disableSigMask, kSigNo, [] (Type type, void* fp, bool *disable) {
                int err = errno;
                errno = 0;
                TraceUnwinder::get().backtraceThreadOnce(type, fp, false, disable);
                errno = err;
            })) {
                return false;
            }
        }
    } else {
        mainAsyncIntervalNs = 0;
        childAsyncIntervalNs = 0;
    }
    
    setupLoopThread();
    
    return true;
}

bool TraceUnwinder::cleanup() {
    terminated = true;
//    frequencyController.cleanup();
    auto obj = backtraceObject;
    backtraceObject = nullptr;
    OH_HiDebug_DestroyBacktraceObject(obj);
    
    if (loopThread != nullptr) {
        loopThread->join();
    }
    
    if (TraceBuffer::get().isPreloadEnabled()) {
        TracePreloader::Stop();
    }
    
    if (enableOsThread) {
        OSThread::Stop();
    }
    
    return true;
}

void TraceUnwinder::setupLoopThread() {
    assert(loopThread == nullptr);
    
    loopThread = new std::thread([this]{
        pthread_setname_np(pthread_self(), "btrace-clock-updator");
        setpriority(PRIO_PROCESS, gettid(), -10);
        
        pid_t pid = OSThread::Pid();
        
        OSThread *os_thread = nullptr;
        if (enableOsThread && (os_thread = OSThread::Current())) {
            os_thread->disableLogging();
        }
        
        while (true) {
            if (terminated) { break; }
            
            auto intervalNs = std::chrono::nanoseconds(mainSampleIntervalNs);
            std::this_thread::sleep_for(intervalNs);
            
            auto curTimeNs = current_monotime_nanos();
            set_buffered_monotime_nanos(curTimeNs);
            
            if (paused || !enableOsThread || mainAsyncIntervalNs <= 0) {
                continue;
            }
            
            OSThreadIterator it;
            
            while (it.HasNext()) {
                auto os_thread = it.Next();
                
                if (os_thread->signal_disabled()) { continue; }
                
                if (!os_thread->alive()) { continue; }
                
                pid_t tid = os_thread->tid();
                bool isMain = os_thread->IsMain();
                auto prevAccessTime = os_thread->access_time();
                auto diffTime = curTimeNs - prevAccessTime;
                auto interval = childAsyncIntervalNs;
                if (isMain) { interval = mainAsyncIntervalNs; }
                if (interval == 0 || diffTime < interval) { continue; }
    
                if (syscall(SYS_tgkill, pid, tid, kSigNo) != 0) {
                    if (errno == ESRCH) {
                        os_thread->set_alive(false);
                    }
                }
            }
        }
    });
}

void TraceUnwinder::backtraceOnce(Type type, void* fp, bool force) {
    if (enableOsThread) {
        backtraceThreadOnce(type, fp, force);
        return;
    }
    
    if (paused || backtraceObject == nullptr || terminated) { return; }
    
    int err = errno;
    bool isMain = is_main_thread();
    auto curTimeNs = buffered_monotime_nanos();
    auto& tls = ThreadLocalStorage::get().tlsRecord();
    uint64_t intervalNs = isMain ? mainSampleIntervalNs : childSampleIntervalNs;
//    auto curRound = frequencyController.getCurTimerRound(mainThread);
    if (tls.backtracing || (!force && curTimeNs - tls.lastActiveTimerRound < intervalNs)) {
        return;
    }
    tls.backtracing = true;
    if (TraceBuffer::get().isPreloadEnabled()) {
        backtraceInnerPreload(gettid(), type, fp);
    } else {
        int err = errno;
        backtraceInner(type, fp, err);
    }
    tls.backtracing = false;
    tls.lastActiveTimerRound = curTimeNs;
}

void TraceUnwinder::backtraceThreadOnce(Type type, void* fp, bool force, bool *disable) {
    if (paused || backtraceObject == nullptr || terminated) { return; }
    
    OSThread *os_thread = OSThread::TryNonWorkingCurrent();
    
    if (os_thread == nullptr) { return; }
    
    if (disable != nullptr) {
        os_thread->set_signal_disabled(*disable);
    }
    
    bool is_main = os_thread->IsMain();
    auto interval = childSampleIntervalNs;
    if (is_main) { interval = mainSampleIntervalNs; }
    if (interval == 0) { return; }
    
    auto curTimeNs = buffered_monotime_nanos();
    auto prevAccessTime = os_thread->access_time();
    auto diffTime = curTimeNs - prevAccessTime;

    if (diffTime < interval && !force) { return; }

    OSThread::ScopedWorking working(os_thread);

    // double check
    if (terminated) { return; }

    os_thread->set_access_time(curTimeNs);
    
    if (TraceBuffer::get().isPreloadEnabled()) {
        if (is_main) {
            backtraceInnerPreloadForMain(os_thread->tid(), type, fp,
                                        os_thread->stack_buffer_ptr(),
                                        os_thread->stack_buffer_size());
        } else {
            backtraceInnerPreload(os_thread->tid(), type, fp);
        }
    } else {
        int err = errno;
        backtraceInner(type, fp, err);
    }
}

void TraceUnwinder::backtraceInner(Type type, void* fp, int err) {
    StackTrace st;
    int depth = OH_HiDebug_BacktraceFromFp(backtraceObject, fp, (void**)&st.elements, kMaxStackDepth);
    if (depth <= 0) return;
    st.tid = gettid();
    st.type = type;
    st.cpuTime = thread_cpu_time_nanos();
    st.timestamp = current_monotime_nanos();
    st.depth = depth;
    TraceBuffer::get().write(st);
}

void TraceUnwinder::backtraceInnerPreload(pid_t tid, Type type, void* fp) {
    uint64_t buffer[kMaxStackDepth];
    
    int depth = OH_HiDebug_BacktraceFromFp(backtraceObject, fp, (void**)buffer, kMaxStackDepth);
    if (depth <= 0) return;
    
    auto curr_time = current_monotime_nanos();
    auto cpu_time = thread_cpu_time_nanos();
    Sample sample(type, tid, curr_time, cpu_time, depth, buffer);
    
    bool res = TracePreloader::Write(sample, curr_time);
}

void TraceUnwinder::backtraceInnerPreloadForMain(pid_t tid, Type type, void* fp, uint64_t *buffer, size_t size) {
    int depth = OH_HiDebug_BacktraceFromFp(backtraceObject, fp, (void**)buffer, size);
    if (depth <= 0) return;
    
    auto curr_time = current_monotime_nanos();
    auto cpu_time = thread_cpu_time_nanos();
    Sample sample(type, tid, curr_time, cpu_time, depth, buffer);
    
    bool res = TracePreloader::Write(sample, curr_time);
}

typedef struct {
    Type type;
    HiDebug_Backtrace_Object object;
    void* fp;
} BacktracePayload;

static void backtraceCallback(void* payload, StackTrace& st) {
    BacktracePayload* pl = (BacktracePayload*)payload;
    st.type = pl->type;
    st.depth = OH_HiDebug_BacktraceFromFp(pl->object, pl->fp, (void**)&st.elements, kMaxStackDepth);
    st.tid = gettid();
    st.cpuTime = thread_cpu_time_nanos();
    st.timestamp = current_monotime_nanos();
}

void TraceUnwinder::signalSafetyBacktraceInner(void* fp) {
    if (paused || terminated) { return; }

    OSThread *os_thread = OSThread::SigSafeNonWorkingCurrent();
    
    if (os_thread == nullptr) { return; }
    
    bool is_main = os_thread->IsMain();
    intptr_t interval = childAsyncIntervalNs;
    if (is_main) { interval = mainAsyncIntervalNs; }
    if (interval == 0) { return; }
    
    int64_t curr_time = buffered_monotime_nanos();
    int64_t prev_access_time = os_thread->access_time();
    int64_t diff_time = curr_time - prev_access_time;
    if (diff_time < interval) { return; }
    
    OSThread::ScopedWorking working(os_thread);
    // double check
    if (terminated) { return; }
    
    os_thread->set_access_time(curr_time);
    
    if (TraceBuffer::get().isPreloadEnabled()) {
        if (is_main) {
            backtraceInnerPreloadForMain(os_thread->tid(), Type::kSignal, fp,
                                         os_thread->stack_buffer_ptr(),
                                         os_thread->stack_buffer_size());
        } else {
            backtraceInnerPreload(os_thread->tid(), Type::kSignal, fp);
        }
    } else {
        BacktracePayload payload;
        payload.type = Type::kSignal;
        payload.fp = fp;
        payload.object = backtraceObject;
        TraceBuffer::get().signalSafetyWrite(&payload, backtraceCallback);
    }
}

struct UnwindParams {
    std::ostringstream& methodMappingOss;
    std::ostringstream& buildIdOss;
    std::set<std::string>& hasBuildIds;
    uint32_t count = 0;
    UnwindParams(std::ostringstream& moss, std::ostringstream& boss, std::set<std::string>& hasBuildIds)
    : methodMappingOss(moss), buildIdOss(boss), hasBuildIds(hasBuildIds) {}
};

void unwind_callback(void* pc, void* arg, const HiDebug_StackFrame* frame) {
    UnwindParams* params = reinterpret_cast<UnwindParams*>(arg);
    params->count += 1;
    std::ostringstream& oss = params->methodMappingOss;
    std::set<std::string>& hasBuildIds = params->hasBuildIds;
    if (frame->type == HIDEBUG_STACK_FRAME_TYPE_JS) {
        auto& js = frame->frame.js;
        oss << "J:" << (uintptr_t) pc << ':';
        if (js.functionName && *js.functionName != '\0') {
            oss << js.functionName;
        } else {
            oss << "(unknown)";
        }
        if (js.url) {
            oss << '<' << js.url << ':' <<  js.line << '>';
        } else {
            oss << "<unknown:0>"; 
        }
        oss << '\n';
    } else {
        auto& native = frame->frame.native;
        oss << "N:" << (uintptr_t) pc << ':';
        bool hasMapName = false;
        if (native.mapName && *native.mapName != '\0') {
            hasMapName = true;
            oss << '<' << native.mapName << '>';
        } else {
            oss << "<unknown>";
        }
        if (native.functionName && *native.functionName != '\0') {
            oss << '<' << native.functionName << '>';
        } else {
            oss << '<' << std::hex << (native.relativePc) << std::dec << '>';
        }
        oss << '\n';
        if (hasMapName && native.buildId && *native.buildId != '\0') {
            std::string soMap(native.mapName);
            if (hasBuildIds.find(soMap) == hasBuildIds.end()) {
                hasBuildIds.emplace(soMap);
                params->buildIdOss << soMap << ':' << native.buildId << std::endl;
            }
        }
    }
}

void TraceUnwinder::batchUnwind(HiDebug_Backtrace_Object obj, std::set<uintptr_t>& pcs,
                                std::ostringstream& methodOss, std::ostringstream& buildIdOss,
                                std::set<std::string>& hasBuildIds) {
    UnwindParams params(methodOss, buildIdOss, hasBuildIds);
    
    if (TraceBuffer::get().isPreloadEnabled()) {
        TracePreloader::SymbolicAddress(obj, pcs, &params, unwind_callback, forceLoad, parseElfSymbols);
    } else {
        for (uintptr_t pc : pcs) {
            OH_HiDebug_SymbolicAddress(obj, (void*) pc, &params, unwind_callback);
        }
    }
}

}