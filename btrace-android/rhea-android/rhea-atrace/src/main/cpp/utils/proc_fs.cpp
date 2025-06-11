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

#include <unistd.h>

#include "utils/fs.h"
#include "utils/proc_fs.h"
#include <utils/xcc_fmt.h>

namespace bytedance {
namespace utils {

namespace {
constexpr int kProcFileLen = 64;
}

std::string GetPath(int fd) {
  char fd_path[kProcFileLen];
  char buf[PATH_MAX] = {0};

  xcc_fmt_snprintf(fd_path, sizeof (fd_path), "/proc/self/fd/%d", fd);
  readlink(fd_path, buf, sizeof (buf));

  return std::string(buf);
}

}  // namespace utils
}  // namespace bytedance
