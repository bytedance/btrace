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
#include "SamplingConfig.h"

namespace rheatrace {

SamplingConfig::SamplingConfig(JNIEnv* env, jlongArray rawConfigArray) {
    auto intervals = env->GetLongArrayElements(rawConfigArray, nullptr);
    capacity = intervals[0];
    mainThreadJavaIntervalNs = intervals[1];
    otherThreadJavaIntervalNs = intervals[2];
    jlong rawClockId = intervals[3];
    if (rawClockId == 0) {
        clockId = CLOCK_MONOTONIC;
    } else if (rawClockId == 1) {
        clockId = CLOCK_MONOTONIC_COARSE;
    } else {
        clockId = CLOCK_BOOTTIME;
    }
    stackWalkKind = intervals[4] == 0 ? StackVisitor::StackWalkKind::kIncludeInlinedFrames
                                      : StackVisitor::StackWalkKind::kSkipInlinedFrames;
    enableObjectAllocationStub = intervals[5];
    enableRusage = intervals[6];
    enableWakeup = intervals[7];
    enabledThreadNames = intervals[8];
    shadowPauseMode = intervals[9] != 0;
    env->ReleaseLongArrayElements(rawConfigArray, intervals, JNI_ABORT);
}

void SamplingConfig::update(JNIEnv* env, jlongArray updatableConfigArray) {
    auto intervals = env->GetLongArrayElements(updatableConfigArray, nullptr);
    mainThreadJavaIntervalNs = intervals[0];
    otherThreadJavaIntervalNs = intervals[1];
    env->ReleaseLongArrayElements(updatableConfigArray, intervals, JNI_ABORT);
}

}