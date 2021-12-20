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

#include "utils/threads.h"

#include <sys/prctl.h>
#include <unistd.h>

namespace bytedance {
namespace utils {

uint32_t GetThreadId()
{
  return gettid();
}

bool GetCurrentThreadName(std::string* name) {
  char buf[100] = {0};
  int res = prctl(PR_GET_NAME, buf);
  if (res != 0) {
    return false;
  }
  *name = buf;
  return true;
}

void SetThreadName(const char* name) {
#if defined(__linux__)
    // Mac OS doesn't have this, and we build libutil for the host too
    int hasAt = 0;
    int hasDot = 0;
    const char *s = name;
    while (*s) {
        if (*s == '.') hasDot = 1;
        else if (*s == '@') hasAt = 1;
        s++;
    }
    int len = s - name;
    if (len < 15 || hasAt || !hasDot) {
        s = name;
    } else {
        s = name + len - 15;
    }
    prctl(PR_SET_NAME, (unsigned long) s, 0, 0, 0);
#endif
}

}  // namespace utils
}  // namespace bytedance
