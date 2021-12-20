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

#include "native_trace_callbacks.h"

#define LOG_TAG "Rhea.NativeTraceCallback"
#include <utils/debug.h>

namespace bytedance {
namespace atrace {

void NativeTraceCallbacks::OnTraceStart(
    int64_t trace_id, int32_t flags, std::string trace_file) {
  ALOGE("OnTraceStart: trace_id=%lld, flags=%d, trace_file=%s",
        trace_id, flags, (char*)trace_file.c_str());
}

void NativeTraceCallbacks::OnTraceEnd(int64_t trace_id) {
  ALOGE("OnTraceEnd: trace_id=%lld", trace_id);
}

void NativeTraceCallbacks::OnTraceAbort(int64_t trace_id, AbortReason reason) {
  ALOGE("OnTraceAbort: trace_id=%lld, abort reason=%d", trace_id, reason);
}

}  // namespace atrace
}  // namespace bytedance
