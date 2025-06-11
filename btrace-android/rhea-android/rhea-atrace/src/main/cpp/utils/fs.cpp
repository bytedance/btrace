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

#include "utils/fs.h"

#include <errno.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

namespace bytedance {
namespace utils {

bool MakeDir(const std::string& path, mode_t mode) {
    return mkdir(path.c_str(), mode) == 0;
}

bool MakeDirRecursive(const std::string& path, mode_t mode) {
    std::string::size_type slash = 0;
    while ((slash = path.find('/', slash + 1)) != std::string::npos) {
        auto directory = path.substr(0, slash);
        struct stat info;
        if (stat(directory.c_str(), &info) != 0) {
            auto ret = MakeDir(directory, mode);
            if (!ret && errno != EEXIST) return false;
        }
    }
    auto ret = MakeDir(path, mode);
    if (!ret && errno != EEXIST) return false;
    return true;
}

void ReadWithDefault(const char* path, char* buf, size_t len, const char* default_value) {
    int fd = open(path, O_RDONLY);
    if (fd != -1) {
      int rc = TEMP_FAILURE_RETRY(read(fd, buf, len - 1));
      if (rc != -1) {
        buf[rc] = '\0';

        // Trim trailing newlines.
        if (rc > 0 && buf[rc - 1] == '\n') {
          buf[rc - 1] = '\0';
        }
        close(fd);
        return;
      }
    }
    strcpy(buf, default_value);
}

}  // namespace utils
}  // namespace bytedance
