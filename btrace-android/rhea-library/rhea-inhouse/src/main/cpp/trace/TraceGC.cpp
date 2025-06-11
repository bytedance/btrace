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

#include "TraceGC.h"

#include <shadowhook.h>
#include "../sampling/SamplingCollector.h"
#include "../utils/time.h"

namespace rheatrace {

static void* proxyWaitForGcToCompleteLocked(void* proxy, void* cause, void* self) {
    SHADOWHOOK_STACK_SCOPE();
    uint64_t beginNano = current_boot_time_nanos();
    uint64_t beginCpuNano = current_thread_cpu_time_nanos();
    void* result = SHADOWHOOK_CALL_PREV(proxyWaitForGcToCompleteLocked, proxy, cause, self);
    SamplingCollector::request(SamplingType::kGC, self, true, true, beginNano, beginCpuNano);
    return result;
}

static void *stub = nullptr;

void TraceGC::init() {
    stub = shadowhook_hook_sym_name(
            "libart.so",
            "_ZN3art2gc4Heap25WaitForGcToCompleteLockedENS0_7GcCauseEPNS_6ThreadE",
            reinterpret_cast<void *>(proxyWaitForGcToCompleteLocked),
            nullptr);

}

void TraceGC::destroy() {
    if (stub != nullptr) {
        shadowhook_unhook(stub);
        stub = nullptr;
    }
}

}