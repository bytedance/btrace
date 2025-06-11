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

#include "TraceJavaMonitor.h"

#include <shadowhook.h>
#include "../sampling/SamplingCollector.h"
#include "../utils/misc.h"

#define LOG_TAG "RheaTrace:Monitor"
#include "../utils/log.h"

namespace rheatrace {

static bool wakeupEnabled = false;
static void *currentMainMonitor = nullptr;
static uint64_t currentMainNano = 0;

void *Monitor_MonitorEnter(void *self, void *obj, bool trylock) {
    SHADOWHOOK_STACK_SCOPE();
    if (wakeupEnabled && is_main_thread()) {
        ScopeSampling ss(SamplingType::kMonitor, self);
        currentMainMonitor = obj;
        currentMainNano = ss.beginNano_;
        void *result = SHADOWHOOK_CALL_PREV(Monitor_MonitorEnter, self, obj, trylock);
        currentMainMonitor = nullptr;
        return result;
    } else {
        ScopeSampling ss(SamplingType::kMonitor, self);
        return SHADOWHOOK_CALL_PREV(Monitor_MonitorEnter, self, obj, trylock);
    }
}

bool Monitor_MonitorExit(void *self, void *obj) {
    SHADOWHOOK_STACK_SCOPE();
    if (!is_main_thread()) {
        if (currentMainMonitor == obj) {
            SamplingCollector::request(SamplingType::kUnlock, self, true, true, currentMainNano);
            ALOGD("Monitor_MonitorExit wakeup main lock %ld", currentMainNano);
        }
    }
    return SHADOWHOOK_CALL_PREV(Monitor_MonitorExit, self, obj);
}

void Monitor_Lock(void *monitor, void *threadSelf) {
    SHADOWHOOK_STACK_SCOPE();
    if (wakeupEnabled && is_main_thread()) {
        ScopeSampling ss(SamplingType::kMonitor, threadSelf);
        currentMainMonitor = monitor;
        currentMainNano = ss.beginNano_;
        SHADOWHOOK_CALL_PREV(Monitor_Lock, monitor, threadSelf);
        currentMainMonitor = nullptr;
    } else {
        ScopeSampling ss(SamplingType::kMonitor, threadSelf);
        SHADOWHOOK_CALL_PREV(Monitor_Lock, monitor, threadSelf);
    }
}

bool Monitor_Unlock(void *monitor, void *threadSelf) {
    if (!is_main_thread()) {
        if (currentMainMonitor == monitor) {
            SamplingCollector::request(SamplingType::kUnlock, threadSelf, true, true, currentMainNano);
            ALOGD("wakeup main lock %ld", currentMainNano);
        }
    }
    return SHADOWHOOK_CALL_PREV(Monitor_Unlock, monitor, threadSelf);
}

static void *stubEnter = nullptr;
static void *stubExit = nullptr;

void TraceJavaMonitor::init(bool enableWakeup) {
    wakeupEnabled = enableWakeup;
    stubEnter = shadowhook_hook_sym_name("libart.so", "_ZN3art7Monitor12MonitorEnterEPNS_6ThreadENS_6ObjPtrINS_6mirror6ObjectEEEb", (void *) Monitor_MonitorEnter, nullptr);
    if (enableWakeup) {
        stubExit = shadowhook_hook_sym_name("libart.so", "_ZN3art7Monitor11MonitorExitEPNS_6ThreadENS_6ObjPtrINS_6mirror6ObjectEEE", (void *) Monitor_MonitorExit, nullptr);
    }
}

void TraceJavaMonitor::destroy() {
    if (stubEnter != nullptr) {
        shadowhook_unhook(stubEnter);
        stubEnter = nullptr;
    }
    if (stubExit != nullptr) {
        shadowhook_unhook(stubExit);
        stubExit = nullptr;
    }
}
} // sampling