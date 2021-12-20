/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <unistd.h>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_set>
#include <utility>
#include <vector>

#include <entries/EntryParser.h>
#include <logger/buffer/RingBuffer.h>
#include <writer/PacketReassembler.h>
#include <writer/TraceCallbacks.h>

namespace facebook {
namespace profilo {
namespace writer {

using TraceBackwardsCallback = std::function<
    void(entries::EntryVisitor&, TraceBuffer&, TraceBuffer::Cursor&)>;

class TraceWriter {
 public:
  static const int64_t kStopLoopTraceID = 0;

  //
  // folder: the absolute path to the folder that will store any trace folders.
  // trace_prefix: a file prefix for every trace file written by this writer.
  // buffer: the ring buffer instance to use.
  // headers: a list of key-value headers to output at
  //          the beginning of the trace
  //
  TraceWriter(
      const std::string&& folder,
      const std::string&& trace_prefix,
      TraceBuffer& buffer,
      std::shared_ptr<TraceCallbacks> callbacks = nullptr,
      std::vector<std::pair<std::string, std::string>>&& headers =
          std::vector<std::pair<std::string, std::string>>(),
      TraceBackwardsCallback trace_backwards_callback = nullptr);

  //
  // Wait until a submit() call and then process a submitted trace ID.
  //
  void loop();

  //
  // Implements synchronous single trace processing logic starting from the
  // given cursor. Can be used separately from the loop logic for single trace
  // processing. It's recommended to use either loop logic or direct processing
  // with this method on a single Writer (and Buffer) instance, but not both.
  // Mixed mode usage is not safe.
  //
  std::unordered_set<int64_t> processTrace(TraceBuffer::Cursor& cursor);

  //
  // Submit a trace ID for processing. Walk will start from `cursor`.
  // Will wake up the thread and let it run until the trace is finished.
  //
  // Call with trace_id = kStopLoopTraceID to terminate loop()
  // without processing a trace.
  //
  void submit(TraceBuffer::Cursor cursor, int64_t trace_id);

  //
  // Equivalent to write(buffer_.currentTail(), trace_id).
  // This will force the TraceWriter to scan the entire ring buffer for the
  // start event. Prefer the cursor version of submit() where appropriate.
  //
  void submit(int64_t trace_id);

 private:
  std::mutex wakeup_mutex_;
  std::condition_variable wakeup_cv_;
  std::queue<std::pair<TraceBuffer::Cursor, int64_t>> wakeup_trace_ids_;

  const std::string trace_folder_;
  const std::string trace_prefix_;
  TraceBuffer& buffer_;
  std::vector<std::pair<std::string, std::string>> trace_headers_;

  std::shared_ptr<TraceCallbacks> callbacks_;
  TraceBackwardsCallback trace_backwards_callback_;
};

} // namespace writer
} // namespace profilo
} // namespace facebook
