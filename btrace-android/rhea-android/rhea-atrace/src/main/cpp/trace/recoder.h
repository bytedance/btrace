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

#include <logger/Logger.h>

namespace bytedance {
namespace atrace {

using Logger = facebook::profilo::Logger;
using StandardEntry = facebook::profilo::StandardEntry;
using EntryType = facebook::profilo::EntryType;

namespace {
constexpr int64_t kRheaAtraceID = 18316;
constexpr size_t kRingBufferSize = 100000;
}

bool PostCreateTrace(size_t ring_buffer_size = kRingBufferSize, int64_t trace_id = kRheaAtraceID);
void PostFinishTrace(int64_t trace_id = kRheaAtraceID);

}  // namespace atrace
}  // namespace bytedance
