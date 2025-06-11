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

#include "recoder.h"

#include <unistd.h>

#include <limits>
#include <thread>

#define LOG_TAG "Rhea.Logger"
#include <utils/debug.h>
#include <utils/timers.h>
#include <utils/threads.h>

#include "native_trace_writer.h"
#include "trace_provider.h"

namespace bytedance {
namespace atrace {

namespace {
constexpr int32_t kTimeout = std::numeric_limits<int>::max();
constexpr int32_t kFlags = 1 << 1;
constexpr char kDefaultTraceFile[] = "rhea-atrace";

static std::unique_ptr<NativeTraceWriter> sTraceWriter = nullptr;
}

static bool WriteTraceStart(NativeTraceWriter* writer, int64_t trace_id = 1) {
  if (writer == nullptr) {
    ALOGE("WriteTraceStart:native trace writer is null");
    return false;
  }

  TraceBuffer::Cursor cursor = RingBuffer::get().currentTail();
  int id = Logger::get().writeAndGetCursor(
      StandardEntry{
        .id = 0,
        .type = EntryType::TRACE_START,
        .timestamp = systemTime(SYSTEM_TIME_BOOTTIME),
        .tid = gettid(),
        .callid = kTimeout,
        .matchid = kFlags,
        .extra = trace_id,
      },
      cursor);
  writer->submit(cursor, trace_id);
  return true;
}

static void WriteTraceEnd(NativeTraceWriter* writer, int64_t trace_id = 1) {
  if (writer == nullptr) {
    ALOGE("WriteTraceEnd:native trace writer is null.");
    return;
  }

  TraceBuffer::Cursor cursor = RingBuffer::get().currentTail();
  int id = Logger::get().writeAndGetCursor(
      StandardEntry{
        .id = 0,
        .type = EntryType::TRACE_END,
        .timestamp = systemTime(SYSTEM_TIME_BOOTTIME),
        .tid = gettid(),
        .callid = kTimeout,
        .matchid = kFlags,
        .extra = trace_id,
      },
      cursor);

  writer->submit(cursor, TraceWriter::kStopLoopTraceID);
}

static void StartWorkerThreadIfNecessary(size_t ring_buffer_size) {
  sTraceWriter = std::make_unique<NativeTraceWriter>(
          TraceProvider::Get().GetTraceFolder(),
          std::string(kDefaultTraceFile),
          ring_buffer_size);

  std::thread([=]() {
    utils::SetThreadName("async-writer");
    ALOGD("start async atrace writer thread");
    sTraceWriter->loop();
    ALOGD("Stop async atrace writer thread");
  }).detach();
}

bool PostCreateTrace(size_t ring_buffer_size, int64_t trace_id) {
  ALOGE("buffer size %zu", ring_buffer_size);
  StartWorkerThreadIfNecessary(ring_buffer_size);
  return WriteTraceStart(sTraceWriter.get(), trace_id);
}

void PostFinishTrace(int64_t trace_id) {
  WriteTraceEnd(sTraceWriter.get(), trace_id);
}

}  // namespace atrace
}  // namespace bytedance
