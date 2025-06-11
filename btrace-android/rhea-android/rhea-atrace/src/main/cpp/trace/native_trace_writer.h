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
#include <stdlib.h>

#include <memory>
#include <string>

#include <logger/buffer/RingBuffer.h>
#include <writer/TraceWriter.h>

#include "native_trace_callbacks.h"

namespace bytedance {
namespace atrace {

using RingBuffer = facebook::profilo::RingBuffer;
using TraceBuffer = facebook::profilo::TraceBuffer;
using TraceWriter = facebook::profilo::writer::TraceWriter;

class NativeTraceWriter {
public:
  NativeTraceWriter(
      const std::string trace_folder,
      const std::string trace_file,
      const size_t ring_buffer_size);

  void loop();

  void submit(TraceBuffer::Cursor cursor, int64_t trace_id);

private:
  TraceWriter writer_;
};

}  // namespace atrace
}  // namespace bytedance
