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
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <thread>

#define LOG_TAG "Rhea.ATrace"
#include <utils/debug.h>
#include <utils/build.h>
#include <utils/fs.h>
#include <utils/threads.h>

#include "atrace.h"
#include "hook/hook_bridge.h"
#include "trace.h"
#include "trace_provider.h"

#include "recoder.h"

namespace bytedance {
namespace atrace {

namespace {
// Magic FD to simply write to tracer logger and bypassing real write
constexpr int kTracerMagicFd = -100;
constexpr ssize_t kAtraceMessageLen = 1024;
constexpr char kAtraceHeader[] = "TRACE:\n# tracer: nop\n";

}

ATrace& ATrace::Get() {
  static ATrace kInstance;
  return kInstance;
}

ATrace::ATrace() = default;

ATrace::~ATrace() = default;

int32_t ATrace::StartTrace() {
  int64_t start = elapsedRealtimeMicros();

  main_thread_only_ = TraceProvider::Get().isMainThreadOnly();

  if (atrace_started_) {
    ALOGW("atrace has been started.");
    return OK;
  }

  if (!PostCreateTrace(TraceProvider::Get().GetBufferSize())) {
    return START_WRITE_TRACE_FAILED;
  }

  int32_t result = InstallProbe();
  if (result != OK) {
    ALOGE("failed to install rhea-trace, errno:%d", result);
    return result;
  }

  if (!first_start_trace_) {
    // On every start, except the first one, find if new libs were loaded
    // and install systrace hook for them
    HookBridge::Get().HookLoadedLibs();
  }
  first_start_trace_ = false;

  auto prev = atrace_enabled_tags_->exchange(DEFAULT_ATRACE_TAG);
  if (prev != UINT64_MAX) {
    original_tags_ = prev;
  }

  atrace_started_ = true;

  // ATRACE_BEGIN(("monotonic_time: " + std::to_string(systemTime(SYSTEM_TIME_MONOTONIC) / 1000000000.0)).c_str());

  int64_t cost_us = elapsedRealtimeMicros() - start;
  ALOGD("start trace cost us: %" PRId64, cost_us);
  return OK;
}

int32_t ATrace::StopTrace() {
  int64_t start = elapsedRealtimeMicros();

  if (!atrace_started_) {
    ALOGE("please start trace firstly");
    return OK;
  }

  uint64_t tags = original_tags_;
  if (tags != UINT64_MAX) {
    atrace_enabled_tags_->store(tags);
  }

//  if (!HookBridge::Get().UnhookLoadedLibs()) {
//    ALOGE("failed to unhook loaded libs");
//    return UNHOOK_FAILED;
//  }

  ALOGD("log atrace cost us: %" PRIu64, log_trace_cost_us_);
  log_trace_cost_us_ = 0;

  PostFinishTrace();
  atrace_started_ = false;

  int64_t cost_us = elapsedRealtimeMicros() - start;
  ALOGD("stop trace cost us: %" PRId64, cost_us);

  return OK;
}

bool ATrace::IsATrace(int fd, size_t count) {
  return (atrace_marker_fd_ != nullptr && fd == *atrace_marker_fd_ && count > 0);
}

void ATrace::LogTrace(const void *buf, size_t count) const {
  if (!atrace_started_) {
    return;
  }

#define PRINT_LOG_TIME 0
#if PRINT_LOG_TIME
  int64_t start = elapsedRealtimeMicros();
#endif
  const char *msg = (const char*)buf;
  switch (msg[0]) {
    case 'B': { // begin synchronous event. format: "B|<pid>|<name>"
      break;
    }
    case 'E': { // end synchronous event. format: "E"
      break;
    }
    // the following events we don't currently log.
    case 'S': // start async event. format: "S|<pid>|<name>|<cookie>"
    case 'F': // finish async event. format: "F|<pid>|<name>|<cookie>"
    case 'C':  // counter. format: "C|<pid>|<name>|<value>"
    default:
      return;
  }

  ssize_t len;
  char tmp_buf[kAtraceMessageLen] = {0};

  if (main_thread_only_) {
      len = FillTimestampAndTid(tmp_buf, 0);
  } else {
      len = FillTimestampAndTid(tmp_buf, gettid());
  }

  if ((len + count + 1) < kAtraceMessageLen) {
    memcpy(tmp_buf + len, msg, count);
    len += count;
    strcpy(tmp_buf + len, "\n");
    len += 1;
  } else {
    ALOGE("atrace message is too long, total count is %zu", len + count + 1);
    return;
  }
  auto& logger = Logger::get();
  logger.writeBytes(
      EntryType::STRING_NAME,
      0,
      (const uint8_t*)tmp_buf,
      std::min(len, kAtraceMessageLen));

#if PRINT_LOG_TIME
  int64_t cost_us = elapsedRealtimeMicros() - start;
  ALOGE("log trace cost us: %lld", cost_us);
  log_trace_cost_us_ += cost_us;
#endif
}

// Private functions
int32_t ATrace::InstallProbe() {

  if (atrace_probe_installed_) {
    return OK;
  }

  if (!HookBridge::Get().HookLoadedLibs()) {
    ALOGE("failed to hook loaded libs");
    return HOOK_FAILED;
  }

  int32_t result = InstallAtraceProbe();
  if (result != OK) {
    ALOGE("failed to install atrace, errno:%d", result);
    return result;
  }

  atrace_probe_installed_ = true;

  return true;
}

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
int32_t ATrace::InstallAtraceProbe() {
  void *handle = nullptr;
  auto sdk = utils::Build::getAndroidSdk();
  {
    std::string lib_name("libcutils.so");
    std::string enabled_tags_sym("atrace_enabled_tags");
    std::string marker_fd_sym("atrace_marker_fd");

    if (sdk < 18) {
      lib_name = "libutils.so";
      // android::Tracer::sEnabledTags
      enabled_tags_sym = "_ZN7android6Tracer12sEnabledTagsE";
      // android::Tracer::sTraceFD
      marker_fd_sym = "_ZN7android6Tracer8sTraceFDE";
    }

    if (sdk < 21) {
      handle = dlopen(lib_name.c_str(), RTLD_LOCAL);
    } else {
      handle = dlopen(nullptr, RTLD_GLOBAL);
    }
    // safe check the handle
    if (handle == nullptr) {
      ALOGE("'atrace_handle' is null");
      return INSTALL_ATRACE_FAILED;
    }

    atrace_enabled_tags_ = reinterpret_cast<std::atomic<uint64_t> *>(
            dlsym(handle, enabled_tags_sym.c_str()));
    if (atrace_enabled_tags_ == nullptr) {
      ALOGE("'atrace_enabled_tags' is not defined");
      dlclose(handle);
      return INSTALL_ATRACE_FAILED;
    }

    atrace_marker_fd_ = reinterpret_cast<int*>(
        dlsym(handle, marker_fd_sym.c_str()));

    if (atrace_marker_fd_ == nullptr) {
      ALOGE("'atrace_marker_fd' is not defined");
      dlclose(handle);
      return INSTALL_ATRACE_FAILED;
    }
    if (*atrace_marker_fd_ == -1) {
      // This is a case that can happen for older Android version i.e. 4.4
      // in which scenario the marker fd is not initialized/opened  by Zygote.
      // Nevertheless for Profilo trace it is not necessary to have an open fd,
      // since all we really need is to ensure that we 'know' it is marker
      // fd to continue writing Profilo logs, thus the usage of marker fd
      // acting really as a placeholder for magic id.
      *atrace_marker_fd_ = kTracerMagicFd;
    }
  }

  dlclose(handle);
  return OK;
}

}  // namespace atrace
}  // namespace bytedance
