/*
 * Copyright (c) 2026 Bytedance Inc.
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

//
// Created on 2025/11/13.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef BTRACE_HARMONY_STRING_UTIL_H
#define BTRACE_HARMONY_STRING_UTIL_H

#include <string>

namespace btrace {

inline bool StartsWith(const std::string& s, const std::string& prefix)
{
    return s.substr(0, prefix.size()) == prefix;
}

inline bool StartsWith(const std::string& s, char prefix)
{
    return !s.empty() && s.front() == prefix;
}

inline bool StartsWithIgnoreCase(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size() && strncasecmp(s.data(), prefix.data(), prefix.size()) == 0;
}

inline bool EndsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() &&
            s.substr(s.size() - suffix.size(), suffix.size()) == suffix;
}

inline bool EndsWith(const std::string& s, char suffix)
{
    return !s.empty() && s.back() == suffix;
}

inline bool EndsWithIgnoreCase(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() &&
        strncasecmp(s.data() + (s.size() - suffix.size()), suffix.data(), suffix.size()) == 0;
}

inline void Trim(std::string& str)
{
    std::string blanks("\f\v\r\t\n ");
    str.erase(0, str.find_first_not_of(blanks));
    str.erase(str.find_last_not_of(blanks) + sizeof(char));
}

}

#endif //BTRACE_HARMONY_STRING_UTIL_H
