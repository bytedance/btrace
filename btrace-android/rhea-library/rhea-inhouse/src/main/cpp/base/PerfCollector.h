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
#include <cstdint>
#include "PerfBuffer.h"

namespace rheatrace {

static constexpr int TYPE_SAMPLING = 0;

class PerfCollector {
public:
    virtual ~PerfCollector() {};

    virtual void start(JNIEnv* env, jlongArray asyncConfigs) = 0;

    virtual void updateConfigs(JNIEnv* env, jlongArray configs) = 0;

    virtual int dump(JNIEnv* env, const char* outDir, const char* extra, int32_t extraLen) = 0;

    virtual int dumpPart(JNIEnv* env, const char* outDir, const char* extra, int32_t extraLen,
                         int64_t startTicket, int64_t endTicket) = 0;

    virtual int64_t mark() = 0;

    virtual void stop() = 0;
};

}