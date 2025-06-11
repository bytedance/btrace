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

#include <unordered_set>
#include <string>
#include <vector>

#include <utils/macros.h>

namespace bytedance {
namespace atrace {

class HookBridge {
public:
  static HookBridge& Get();

  bool IsHooked() const { return hook_ok_; }
  bool HookLoadedLibs();
  bool UnhookLoadedLibs();

private:
  HookBridge();
  ~HookBridge();

  static std::unordered_set<std::string> s_seen_libs_;

  bool hook_init_{false};
  bool hook_ok_{false};

  DISALLOW_COPY_AND_ASSIGN(HookBridge);
};

}  // namespace atrace
}  // namespace bytedance
