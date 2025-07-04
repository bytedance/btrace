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

#include <jni.h>
#include <sys/types.h>
#include "StackVisitor.h"

namespace rheatrace {

class SamplingConfig {
public:
    SamplingConfig(JNIEnv* env, jlongArray rawConfigArray);
    SamplingConfig(const SamplingConfig&) = default;
    ~SamplingConfig() = default;
    void update(JNIEnv* env, jlongArray updatableConfigArray);
private:

    SamplingConfig& operator=(const SamplingConfig&) = delete;

    int64_t capacity;
    clockid_t clockId;
    StackVisitor::StackWalkKind stackWalkKind;
    uint64_t mainThreadJavaIntervalNs;
    uint64_t otherThreadJavaIntervalNs;
    bool enableObjectAllocationStub;
    bool enableRusage;
    bool enableWakeup;
    bool enabledThreadNames;
    bool shadowPauseMode;

    friend class SamplingCollector;
};

}