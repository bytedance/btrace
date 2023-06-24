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

#include <utils/macros.h>
#include <vector>

namespace bytedance {
namespace atrace {

enum ErrorCode {
    OK = 1,
    ATRACE_LOCATION_INVALID = -1,
    CONFIG_INVALID = -2,
    START_WRITE_TRACE_FAILED = -3,
    HOOK_FAILED = -4,
    UNHOOK_FAILED = -5,
    INSTALL_ATRACE_FAILED = -6,
};

class TraceProvider {

public:
  static TraceProvider& Get();

  void SetCategories(std::vector<std::string> categories);
  bool IsEnableIO() const;
  bool isMainThreadOnly() const;
  bool isEnableMemory() const;
  bool isEnableClassLoad() const;
  bool isEnableThread() const;
  bool isEnableBinder() const;
  bool isEnableDetailRender() const;
  void SetMainThreadOnly(bool mainOnly);
  void SetMainThreadId(pid_t tid);
  pid_t GetMainThreadId() const;
  void SetBufferSize(size_t buffer_size);
  size_t GetBufferSize()const;
  void SetTraceFolder(const std::string& trace_folder);
  const std::string& GetTraceFolder();
  void SetBlockHookLibs(const std::string& trace_folder);
  const std::vector<std::string>& GetBlockHookLibs();

private:
  TraceProvider();
  ~TraceProvider();

  std::vector<std::string> categories_;
  bool mainThreadOnly_ = false;
  pid_t main_thread_id_{0};
  size_t buffer_size_{100000};
  std::vector<std::string> block_hook_libs;
  std::string trace_folder_{"/sdcard/rhea-trace"};

  DISALLOW_COPY_AND_ASSIGN(TraceProvider);
};

}  // namespace atrace
}  // namespace bytedance
