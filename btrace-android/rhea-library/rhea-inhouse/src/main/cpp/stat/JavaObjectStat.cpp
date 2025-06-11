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
#include "JavaObjectStat.h"

#include <atomic>

namespace rheatrace {

thread_local JavaObjectStat::ObjectStat stats;
std::atomic_uint64_t globalBytes = 0;
std::atomic_uint64_t globalObjects = 0;

void JavaObjectStat::onObjectAllocated(size_t b) {
    stats.objects++;
    stats.bytes += b;
    globalBytes.fetch_add(b, std::memory_order_relaxed);
    globalObjects.fetch_add(1, std::memory_order_relaxed);
}

JavaObjectStat::ObjectStat& JavaObjectStat::getAllocatedObjectStat() {
    return stats;
}

uint64_t JavaObjectStat::getGlobalAllocatedBytes() {
    return globalBytes.load(std::memory_order_relaxed);
}

}