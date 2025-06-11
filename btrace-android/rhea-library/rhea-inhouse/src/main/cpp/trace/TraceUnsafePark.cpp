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

#include "TraceUnsafePark.h"

#include "../sampling/SamplingCollector.h"
#include "../RheaContext.h"
#include "../utils/JNIHook.h"
#include "../utils/misc.h"

#define LOG_TAG "RheaTrace:Park"
#include "../utils/log.h"

namespace rheatrace {

static bool wakeupEnabled = false;
static bool currentMainPark = false;
static uint64_t currentMainNano = 0;

static void (*Unsafe_park_origin)(JNIEnv *, jobject, jboolean, jlong) = nullptr;

static void Unsafe_park(JNIEnv *env, jobject java_this, jboolean isAbsolute, jlong time) {
    if (wakeupEnabled && is_main_thread()) {
        ScopeSampling ss(SamplingType::kPark);
        currentMainPark = true;
        currentMainNano = ss.beginNano_;
        Unsafe_park_origin(env, java_this, isAbsolute, time);
        currentMainPark = false;
    } else {
        Unsafe_park_origin(env, java_this, isAbsolute, time);
    }
}

static void (*Unsafe_unpark_origin)(JNIEnv *, jobject, jobject) = nullptr;

static void Unsafe_unpark(JNIEnv *env, jobject java_this, jobject target) {
    if (currentMainPark && env->IsSameObject(target, rheatrace::RheaContext::javaMainThread)) {
        SamplingCollector::request(SamplingType::kUnpark, nullptr, true, true, currentMainNano);
        ALOGD("wakeup main park %ld", currentMainNano);
        Unsafe_unpark_origin(env, java_this, target);
    } else {
        Unsafe_unpark_origin(env, java_this, target);
    }
}

void TraceUnsafePark::init(JNIEnv *env, bool enableWakeup, const jlong *isNatives) {
    wakeupEnabled = enableWakeup;
    if (Unsafe_park_origin != nullptr && Unsafe_unpark_origin != nullptr) {
        return;
    }

    if (isNatives[1] > 0) {
        rheatrace::jni_hook::hookMethodId(
                env,
                "sun/misc/Unsafe",
                "park",
                "(ZJ)V",
                (void*) Unsafe_park,
                (void**) &Unsafe_park_origin);
        ALOGD("hook misc park");
    }
    if (enableWakeup && isNatives[2] > 0) {
        rheatrace::jni_hook::hookMethodId(
                env,
                "sun/misc/Unsafe",
                "unpark",
                "(Ljava/lang/Object;)V",
                (void*) Unsafe_unpark,
                (void**) &Unsafe_unpark_origin);
        ALOGD("hook misc unpark");
    }
    if (isNatives[3] > 0) {
        rheatrace::jni_hook::hookMethodId(
                env,
                "jdk/internal/misc/Unsafe",
                "park",
                "(ZJ)V",
                (void*) Unsafe_park,
                (void**) &Unsafe_park_origin);
        ALOGD("hook jdk park");
    }
    if (enableWakeup && isNatives[4] > 0) {
        rheatrace::jni_hook::hookMethodId(
                env,
                "jdk/internal/misc/Unsafe",
                "unpark",
                "(Ljava/lang/Object;)V",
                (void*) Unsafe_unpark,
                (void**) &Unsafe_unpark_origin);
        ALOGD("hook jdk unpark");
    }
}

void TraceUnsafePark::destroy() {
    // let empty
}

} // rheatrace