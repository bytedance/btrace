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

#include "native_trace_writer.h"

#include <stdint.h>
#include <stdlib.h>

#include <writer/trace_headers.h>
#include <writer/trace_backwards.h>

#define LOG_TAG "Rhea.NativeTraceWriter"
#include <utils/debug.h>

namespace bytedance {
namespace atrace {

NativeTraceWriter::NativeTraceWriter(
    const std::string trace_folder,
    const std::string trace_file,
    const size_t ring_buffer_size)
    : writer_(std::move(trace_folder),
              std::move(trace_file),
              RingBuffer::init(ring_buffer_size),
              std::make_shared<NativeTraceCallbacks>(),
              facebook::profilo::writer::calculateHeaders(),
              facebook::profilo::writer::traceBackwards) {}

void NativeTraceWriter::loop() {
  try {
    writer_.loop();
  } catch (...) {
    ALOGE("maybe you should request permissions for writing external storage.");
  }
}

void NativeTraceWriter::submit(TraceBuffer::Cursor cursor, int64_t trace_id) {
  writer_.submit(cursor, trace_id);
}

}  // namespace atrace
}  // namespace bytedance
