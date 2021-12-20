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

#include <errno.h>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include <entries/Entry.h>
#include <entries/EntryParser.h>
#include <writer/AbortReason.h>
#include <writer/ScopedThreadPriority.h>
#include <writer/TraceCallbacks.h>

#include <zstr/zstr.hpp>

namespace facebook {
namespace profilo {
namespace writer {

using namespace facebook::profilo::entries;

class TraceLifecycleVisitor : public EntryVisitor {
 public:
  // Timestamp precision is microsec by default.
  static const size_t kTimestampPrecision = 6;

  static const size_t kTraceFormatVersion = 3;

  TraceLifecycleVisitor(
      const std::string& folder,
      const std::string& trace_prefix,
      std::shared_ptr<TraceCallbacks> callbacks,
      const std::vector<std::pair<std::string, std::string>>& headers,
      int64_t trace_id);

  virtual void visit(const StandardEntry& entry) override;
  virtual void visit(const FramesEntry& entry) override;
  virtual void visit(const BytesEntry& entry) override;

  void abort(AbortReason reason);

  inline bool done() const {
    return done_;
  }

 private:
  const std::string folder_;
  const std::string trace_prefix_;
  const std::vector<std::pair<std::string, std::string>> trace_headers_;
  std::unique_ptr<std::ofstream> output_;

  // chain of delegates
  std::deque<std::unique_ptr<EntryVisitor>> delegates_;
  int64_t expected_trace_;
  std::shared_ptr<TraceCallbacks> callbacks_;
  bool done_;
  std::unique_ptr<ScopedThreadPriority> thread_priority_;

  inline bool hasDelegate() {
    return !delegates_.empty();
  }

  void onTraceStart(int64_t trace_id, int32_t flags);
  void onTraceAbort(int64_t trace_id, AbortReason reason);
  void onTraceEnd(int64_t trace_id);
  void cleanupState();
  void writeHeaders(std::ostream& output, std::string id);
};

} // namespace writer
} // namespace profilo
} // namespace facebook
