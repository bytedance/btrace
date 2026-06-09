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

#ifndef BTRACE_HARMONY_GLOBALS_H
#define BTRACE_HARMONY_GLOBALS_H

#include <stdint.h>

#include <atomic>

namespace btrace {
#define BTRACE_FORCE_INLINE inline __attribute__((always_inline))

#if !defined(DISALLOW_COPY_AND_ASSIGN)
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
private:                                   \
    TypeName(const TypeName &) = delete;   \
    void operator=(const TypeName &) = delete
#endif // !defined(DISALLOW_COPY_AND_ASSIGN)

#ifdef DEBUG
constexpr size_t kMaxStackDepth = 256;
#else
constexpr size_t kMaxStackDepth = 128;
#endif

constexpr size_t kMaxMainStackDepth = 256;

static_assert(kMaxMainStackDepth >= kMaxStackDepth,
              "kMaxMainStackDepth must be greater than or equal to kMaxStackDepth");

// 需要屏蔽对 btrace so 本身的调用 hook
// 主要是 thread local storage 里 new TLSRecord 不能被 hook，否则会死循环崩溃
bool bytehook_filter(const char* callerPath, void*);

// Time constants.
constexpr int64_t kMillisPerSec = 1000;
constexpr int64_t kMicrosPerMilli = 1000;
constexpr int64_t kMicrosPerSec = kMicrosPerMilli * kMillisPerSec;
constexpr int64_t kNanosPerMicro = 1000;
constexpr int64_t kNanosPerMilli = kNanosPerMicro * kMicrosPerMilli;
constexpr int64_t kNanosPerSecond = kNanosPerMicro * kMicrosPerSec;

constexpr uint64_t kPointerMask = 0x00007FFFFFFFFFFF;

template<typename T>
struct get_type {
    using type = T;
};

template<typename T>
struct get_type<T*> {
    using type = T;
};
}

#endif //BTRACE_HARMONY_GLOBALS_H
