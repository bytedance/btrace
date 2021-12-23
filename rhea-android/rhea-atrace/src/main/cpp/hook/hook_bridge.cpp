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

#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <libgen.h>

#include <array>
#include <vector>

#define LOG_TAG "Rhea.hookbridge"
#include <utils/build.h>
#include <utils/debug.h>
#include <utils/timers.h>
#include <second_party/byte_hook/bytehook.h>

#include "hook_bridge.h"
#include "features/io/io_proxy.h"
#include "features/trace/trace_proxy.h"
#include "trace.h"
#include "trace_provider.h"

namespace bytedance {
namespace atrace {

namespace {

struct hook_spec {
  char *fname;
  void *hook_func;
};

std::vector<hook_spec> function_hooks = {
  { "write", reinterpret_cast<void*>(proxy_write) },
  { "__write_chk", reinterpret_cast<void*>(proxy_write_chk) },
};

std::vector<hook_spec> trace_function_hooks = {
  { "atrace_begin_body", reinterpret_cast<void*>(proxy_atrace_begin_body) },
  { "atrace_end_body", reinterpret_cast<void*>(proxy_atrace_end_body) },
};

constexpr auto kIoListSize = 10;
std::array<hook_spec, kIoListSize>& GetIoList() {
  static std::array<hook_spec, kIoListSize> iolist = {
    {
      { "read", reinterpret_cast<void*>(proxy_read) },
      { "__read_chk", reinterpret_cast<void*>(proxy_read_chk) },
      { "readv", reinterpret_cast<void*>(proxy_readv) },
//      { "writev", reinterpret_cast<void*>(proxy_writev) },
      { "pread", reinterpret_cast<void*>(proxy_pread) },
      { "pwrite", reinterpret_cast<void*>(proxy_pwrite) },
      { "sync", reinterpret_cast<void*>(proxy_sync) },
      { "fsync", reinterpret_cast<void*>(proxy_fsync) },
      { "fdatasync", reinterpret_cast<void*>(proxy_fdatasync) },
      { "open", reinterpret_cast<void*>(proxy_open) }
    }
  };
  return iolist;
}

std::vector<bytehook_stub_t> stubs;

void HookCallback(bytehook_stub_t task_stub, int status,
                  const char* caller_path_name, const char *sym_name,
                  void *new_func, void *prev_func, void *hooked_arg) {
  if (status) {
    ALOGE("failed to hook: %s-%s-%d", caller_path_name, sym_name, status);
  }
}

bool AllowFilter(const char *caller_path_name, void *arg) {
    const std::vector<std::string> &block_hook_libs = TraceProvider::Get().GetBlockHookLibs();
    for (const auto &block_hook_lib : block_hook_libs) {
        if (nullptr != strstr(caller_path_name, block_hook_lib.c_str())) {
            ALOGE("don't hook lib %s", block_hook_lib.c_str());
            return false;
        }
    }
    return true;
}

void HookForATrace() {
    //for hooking atrace marker fd, so it can't be controlled by flag enableIO.
   for (auto& spec : function_hooks) {
      bytehook_stub_t stub = bytehook_hook_partial(
              AllowFilter,
              nullptr,
              nullptr,
              spec.fname,
              spec.hook_func,
              HookCallback,
              nullptr);
        stubs.push_back(stub);
    }

  if (TraceProvider::Get().IsEnableIO()) {

    auto& iolist = GetIoList();
    for (auto& spec : iolist) {
      bytehook_stub_t stub = bytehook_hook_partial(
              AllowFilter,
              nullptr,
              nullptr,
              spec.fname,
              spec.hook_func,
              HookCallback,
              nullptr);
      stubs.push_back(stub);
    }
  }

  if (TraceProvider::Get().isMainThreadOnly()) {
    for (auto& spec : trace_function_hooks) {
      bytehook_stub_t stub = bytehook_hook_all(
              nullptr,
              spec.fname,
              spec.hook_func,
              HookCallback,
              nullptr);
      stubs.push_back(stub);
    }
  }
}

void UnhookLoadedLibsInternal() {
  for (auto stub : stubs) {
    if (stub != nullptr) {
      bytehook_unhook(stub);
      stub = nullptr;
    }
  }
  stubs.clear();
}
}

HookBridge& HookBridge::Get() {
  static HookBridge kInstance;
  return kInstance;
}

HookBridge::HookBridge() = default;

HookBridge::~HookBridge() = default;

bool HookBridge::HookLoadedLibs() {
  if (IsHooked()) {
    return true;
  }
  ATRACE_BEGIN(__FUNCTION__);
  HookForATrace();
  hook_ok_ = true;
  ATRACE_END();
  return true;
}

bool HookBridge::UnhookLoadedLibs() {
  if (!IsHooked()) {
    return true;
  }
  ATRACE_BEGIN(__FUNCTION__);
  UnhookLoadedLibsInternal();
  hook_ok_ = false;
  ATRACE_END();
  return true;
}

}  // namespace atrace
}  // namespace bytedance
