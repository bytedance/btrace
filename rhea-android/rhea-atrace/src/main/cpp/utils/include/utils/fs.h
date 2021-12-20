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

#include <sys/stat.h>

#include <string>

namespace bytedance {
namespace utils {

bool MakeDir(const std::string& path, mode_t mode);
bool MakeDirRecursive(const std::string& path, mode_t mode);
void ReadWithDefault(const char* path, char* buf, size_t len, const char* default_value);

}  // namespace utils
}  // namespace bytedance
