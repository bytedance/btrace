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
// Created on 2025/10/13.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef HARMONY_BTRACE_TIME_UTILS_H
#define HARMONY_BTRACE_TIME_UTILS_H

#include "util/globals.h"
#include <ctime>
#include <stdint.h>
#include <time.h>

namespace btrace {

inline uint64_t thread_cpu_time_nanos() {
    struct timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec * kNanosPerSecond + t.tv_nsec;
}

inline uint64_t current_monotime_nanos() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * kNanosPerSecond + t.tv_nsec;
}

inline uint64_t current_realtime_nanos() {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return t.tv_sec * kNanosPerSecond + t.tv_nsec;
}

inline uint64_t thread_cpu_time_millis() {
    struct timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec * 1000L + t.tv_nsec / kNanosPerMilli;
}

inline uint64_t current_monotime_millis() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / kNanosPerMilli;
}

inline uint64_t current_realtime_mills() {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return t.tv_sec * 1000L + t.tv_nsec / kNanosPerMilli;
}

uint64_t buffered_monotime_nanos();

void set_buffered_monotime_nanos(uint64_t t);

void set_buffered_monotime_nanos_force_update(int64_t force_update);

}

#endif //HARMONY_BTRACE_TIME_UTILS_H
