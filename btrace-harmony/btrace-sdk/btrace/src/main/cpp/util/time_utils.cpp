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

#include "time_utils.h"

#include <atomic>
#include <algorithm>

#include "common_utils.h"

namespace btrace {

static std::atomic<uint64_t> s_buffered_time;
static uint64_t s_buffered_time_force_update;
static uint64_t s_buffered_time_cnt;

uint64_t buffered_monotime_nanos() {
    int64_t curr = 0;

    // force update every buffered_time_force_update_ + 1 calls
    if ((s_buffered_time_cnt & s_buffered_time_force_update) == 0) {
        curr = current_monotime_nanos();
        
        if (s_buffered_time_force_update == 0) { return curr; }
        
        s_buffered_time.store(curr, std::memory_order_relaxed);
    } else {
        curr = s_buffered_time.load(std::memory_order_relaxed);
    }

    s_buffered_time_cnt++;

    return curr;
}

void set_buffered_monotime_nanos(uint64_t t) {
    s_buffered_time.store(t, std::memory_order_relaxed);
}

void set_buffered_monotime_nanos_force_update(int64_t force_update) {
    force_update = std::max(common::round_up_to_power_of_two(force_update), (uintptr_t)1);
    s_buffered_time_force_update = force_update - 1;
}

}