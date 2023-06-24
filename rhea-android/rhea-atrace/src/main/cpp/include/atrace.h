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

#include <atomic>
#include <string>
#include <mutex>
#include <unordered_map>

#include <utils/macros.h>
#include <utils/timers.h>

namespace bytedance {
namespace atrace {

class ATrace {
public:
  static ATrace& Get();

  int32_t StartTrace();
  int32_t StopTrace();

  bool IsATrace(int fd, size_t count);
  void LogTrace(const void *buf, size_t count) const;

  bool IsATraceStarted() const {
    return atrace_started_;
  }

private:
  ATrace();
  ~ATrace();

  int32_t InstallProbe();
  int32_t InstallAtraceProbe();

  static int FillTimestampAndTid(char *tmp_buffer, pid_t tid) {
      int64_t timestamp = elapsedRealtimeNanos();
      int timestamp_int = (int) (timestamp / 1000000000);
      int timestamp_decimals = (int) (timestamp % 1000000000);

      int shadow = timestamp_int;
      int timestamp_length = 0;
      while (shadow) {
          shadow /= 10;
          timestamp_length ++;
      }

      int i = 0;
      while (timestamp_int) {
          tmp_buffer[timestamp_length - 1 - i]= timestamp_int % 10 + '0';
          timestamp_int /= 10;
          i++;
      }
      tmp_buffer[timestamp_length] = '.';
      //10 means total length of '.'(1) and decimals(9)
      timestamp_length += 10;
      i = 0;
      while(timestamp_decimals) {
          tmp_buffer[timestamp_length - 1 - i] = timestamp_decimals % 10 + '0';
          timestamp_decimals /= 10;
          i++;
      }
      //decimals maybe start with 0.
      if (i < 9) {
          while (0 < (9 - i)) {
              tmp_buffer[timestamp_length - i - 1] = '0';
              i++;
          }
      }
      if (tid > 0) {
          tmp_buffer[timestamp_length] = ' ';
          timestamp_length += 1;
          shadow = tid;
          while (shadow) {
              shadow /= 10;
              timestamp_length ++;
          }
          i = 0;
          while (tid) {
              tmp_buffer[timestamp_length - 1 - i] = tid % 10 + '0';
              tid /= 10;
              i++;
          }
      }
      tmp_buffer[timestamp_length] = ':';
      tmp_buffer[timestamp_length + 1] = ' ';
      timestamp_length += 2;
      return timestamp_length;
  }

  std::atomic<uint64_t>* atrace_enabled_tags_{nullptr};
  std::atomic<uint64_t> original_tags_{UINT64_MAX};
  int *atrace_marker_fd_{nullptr};

  bool main_thread_only_{false};
  bool atrace_probe_installed_{false};
  bool atrace_started_{false};
  bool first_start_trace_{true};

  uint64_t log_trace_cost_us_{0};

  DISALLOW_COPY_AND_ASSIGN(ATrace);
};

}  // namespace atrace
}  // namespace bytedance
