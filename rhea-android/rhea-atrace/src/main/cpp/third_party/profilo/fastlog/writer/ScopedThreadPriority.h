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

#include <sys/resource.h>
#include <cerrno>
#include <cstring>
#include <system_error>

#include <unistd.h>

namespace facebook {
namespace profilo {
namespace writer {

// RAII helper to change the current thread's priority to a requested value.
class ScopedThreadPriority {
 public:
  explicit ScopedThreadPriority(int priority) {
    //auto tid = threadID();
    auto tid = gettid();

    // -1 is a legitimate return value so we need to
    // clear errno instead.
    errno = 0;
    old_value_ = getpriority(PRIO_PROCESS, tid);
    if (errno != 0) {
      throw std::system_error(
          errno, std::system_category(), "Could not pthread_getschedparam()");
    }

    if (setpriority(PRIO_PROCESS, tid, priority)) {
      throw std::system_error(
          errno, std::system_category(), "Could not pthread_setschedparam()");
    }
  }

  ~ScopedThreadPriority() {
    // Undo the change.
    if (setpriority(PRIO_PROCESS, gettid(), old_value_)) {
      // Intentionally ignored, can't throw.
    }
  }

 private:
  int old_value_ = 0;
};

} // namespace writer
} // namespace profilo
} // namespace facebook
