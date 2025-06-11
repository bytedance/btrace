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

#include "TraceBinderCall.h"
#include "../sampling/SamplingCollector.h"

#include <shadowhook.h>

namespace rheatrace {

static void *stub = nullptr;

int32_t IPCThreadState_transact(void *IPCThreadState, int32_t handle, uint32_t code, void *data, void *reply, uint32_t flags) {
    SHADOWHOOK_STACK_SCOPE();
    ScopeSampling ss(SamplingType::kBinder);
    return SHADOWHOOK_CALL_PREV(IPCThreadState_transact, IPCThreadState, handle, code, data, reply, flags);
}

void TraceBinderCall::init() {
    stub = shadowhook_hook_sym_name("libbinder.so", "_ZN7android14IPCThreadState8transactEijRKNS_6ParcelEPS1_j", (void *) IPCThreadState_transact, nullptr);
}

void TraceBinderCall::destroy() {
    if (stub != nullptr) {
        shadowhook_unhook(stub);
        stub = nullptr;
    }
}

}