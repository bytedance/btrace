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

#pragma once

#include <stdint.h>

#include <string>

#include <writer/TraceCallbacks.h>

namespace bytedance {
namespace atrace {

using TraceCallbacks = facebook::profilo::writer::TraceCallbacks;
using AbortReason = facebook::profilo::writer::AbortReason;

struct NativeTraceCallbacks : public TraceCallbacks {
  void OnTraceStart(int64_t trace_id, int32_t flags, std::string trace_file);

  void OnTraceEnd(int64_t trace_id);

  void OnTraceAbort(int64_t trace_id, AbortReason reason);
};

}  // namespace atrace
}  // namespace bytedance
