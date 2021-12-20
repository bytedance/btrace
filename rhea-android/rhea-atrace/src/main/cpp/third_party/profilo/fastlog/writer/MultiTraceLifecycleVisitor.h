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

#include <functional>
#include <unordered_map>
#include <unordered_set>

#include <logger/buffer/RingBuffer.h>
#include <writer/AbortReason.h>
#include <writer/TraceCallbacks.h>
#include <writer/TraceLifecycleVisitor.h>

namespace facebook {
namespace profilo {
namespace writer {

using namespace facebook::profilo::entries;

class MultiTraceLifecycleVisitor : public EntryVisitor {
 public:
  MultiTraceLifecycleVisitor(
      const std::string& folder,
      const std::string& trace_prefix,
      std::shared_ptr<TraceCallbacks> callbacks,
      const std::vector<std::pair<std::string, std::string>>& headers,
      std::function<void(TraceLifecycleVisitor& visitor)>
          trace_backward_callback);
  virtual void visit(const StandardEntry& entry) override;
  virtual void visit(const FramesEntry& entry) override;
  virtual void visit(const BytesEntry& entry) override;

  void abort(AbortReason reason);
  bool done();
  std::unordered_set<int64_t> getConsumedTraces();

 private:
  const std::string& folder_;
  const std::string& trace_prefix_;
  std::shared_ptr<TraceCallbacks> callbacks_;
  const std::vector<std::pair<std::string, std::string>> trace_headers_;

  std::unordered_map<int64_t, TraceLifecycleVisitor> visitors_;
  std::unordered_set<int64_t> consumed_traces_;
  std::function<void(TraceLifecycleVisitor& visitor)> trace_backward_callback_;

  bool done_;
};

} // namespace writer
} // namespace profilo
} // namespace facebook
