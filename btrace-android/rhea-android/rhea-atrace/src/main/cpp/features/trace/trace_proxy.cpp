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

#include <utils/debug.h>
#include <unistd.h>

#include "second_party/byte_hook/bytehook.h"

#include "trace_provider.h"
#include "trace_proxy.h"

namespace bytedance {
    namespace atrace {

void proxy_atrace_begin_body(const char *name) {
    BYTEHOOK_STACK_SCOPE();
    if (gettid() == TraceProvider::Get().GetMainThreadId()) {
        BYTEHOOK_CALL_PREV(proxy_atrace_begin_body, name);
    }
}

void proxy_atrace_end_body() {
    BYTEHOOK_STACK_SCOPE();
    if (gettid() == TraceProvider::Get().GetMainThreadId()) {
        BYTEHOOK_CALL_PREV(proxy_atrace_end_body);
    }
}

}  // namespace atrace
}  // namespace bytedance