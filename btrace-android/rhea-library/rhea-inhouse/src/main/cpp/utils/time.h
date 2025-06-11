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

#include <cstdint>
#include <linux/time.h>

static uint64_t current_boot_time_nanos() {
    struct timespec t;
    clock_gettime(CLOCK_BOOTTIME, &t);
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}

static uint64_t current_boot_time_millis() {
    struct timespec t;
    clock_gettime(CLOCK_BOOTTIME, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000LL;
}

static uint64_t current_clock_id_time_nanos(clockid_t id) {
    struct timespec t;
    clock_gettime(id, &t);
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}

static uint64_t current_clock_id_time_millis(clockid_t id) {
    struct timespec t;
    clock_gettime(id, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000LL;
}

static uint64_t current_thread_cpu_time_nanos() {
    struct timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}

static uint64_t current_thread_cpu_time_millis() {
    struct timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000LL;
}

static uint64_t ms_to_ns(uint64_t ms) {
    return ms * 1000000;
}