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
// Created on 2026/3/31.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef BTRACE_HARMONY_COMMON_UTILS_H
#define BTRACE_HARMONY_COMMON_UTILS_H

#include <cstdint>

namespace btrace {

namespace common {

template <typename T>
static constexpr inline T round_down(T x, intptr_t alignment) {
    return (x & (-alignment));
}

template <typename T>
static constexpr inline T round_up(T x, uintptr_t alignment) {
    return round_down(x + alignment - 1, alignment) ;
}

static constexpr inline uintptr_t align(uintptr_t x, uintptr_t alignment) {
    return (x + alignment - 1) / alignment * alignment;
}

static constexpr uint64_t round_up_to_power_of_two(uint64_t x) {
    if (x == 0) { return 1; }

    x = x - 1;
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    x = x | (x >> 32);

    return x + 1;
}

}

};

#endif //BTRACE_HARMONY_COMMON_UTILS_H
