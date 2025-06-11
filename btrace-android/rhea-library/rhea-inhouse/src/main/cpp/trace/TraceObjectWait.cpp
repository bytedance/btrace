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

#include "TraceObjectWait.h"

#include "../sampling/SamplingCollector.h"
#include "../utils/JNIHook.h"
#include "../utils/misc.h"
#define LOG_TAG "RheaTrace:Wait"
#include "../utils/log.h"

namespace rheatrace {

static void (* Origin_waitJI)(JNIEnv*, jobject, jlong, jint) = nullptr;

static void (* Origin_notify)(JNIEnv*, jobject) = nullptr;

static void (* Origin_notifyAll)(JNIEnv*, jobject) = nullptr;

static bool wakeupEnabled = false;
static jint currentMainWait = 0;
static uint64_t currentMainNano = 0;

static jint (* identityHashCode)(JNIEnv* env, jclass cls, jobject obj) = nullptr;

static jint identityHashCodeFallback(JNIEnv* env, jclass cls, jobject obj) {
    ALOGD(__FUNCTION__);
    return 0;
}

static void Object_waitJI(JNIEnv* env, jobject java_this, jlong ms, jint ns) {
    if (wakeupEnabled && is_main_thread()) {
        ScopeSampling ss(SamplingType::kWait);
        currentMainWait = identityHashCode(env, nullptr, java_this);
        currentMainNano = ss.beginNano_;
        ALOGD("wakeup main WAIT on %ld", currentMainNano);
        Origin_waitJI(env, java_this, ms, ns);
        currentMainWait = 0;
    } else {
        ScopeSampling ss(SamplingType::kWait);
        Origin_waitJI(env, java_this, ms, ns);
    }
}

static void Object_notify(JNIEnv* env, jobject java_this) {
    if (currentMainWait != 0 && currentMainWait == identityHashCode(env, nullptr, java_this)) {
        SamplingCollector::request(SamplingType::kNotify, nullptr,
                                                  true, true, currentMainNano);
        ALOGD("wakeup main wait %ld", currentMainNano);
    }
    Origin_notify(env, java_this);
}

static void Object_notifyAll(JNIEnv* env, jobject java_this) {
    if (currentMainWait != 0 && currentMainWait == identityHashCode(env, nullptr, java_this)) {
        SamplingCollector::request(SamplingType::kNotify, nullptr,
                                                  true, true, currentMainNano);
        ALOGD("wakeup main wait %ld", currentMainNano);
    }
    Origin_notifyAll(env, java_this);
}

void TraceObjectWait::init(JNIEnv* env, bool enableWakeup, const jlong* isNatives) {
    wakeupEnabled = enableWakeup;
    if (enableWakeup && identityHashCode == nullptr) {
        identityHashCode =
                (jint(*)(JNIEnv*, jclass, jobject)) rheatrace::jni_hook::getStaticEntrance(
                        env,
                        "java/lang/Object",
                        "identityHashCodeNative",
                        "(Ljava/lang/Object;)I");
    }
    if (identityHashCode == nullptr) {
        identityHashCode = identityHashCodeFallback;
    }
    if (isNatives[0] > 0 && Origin_waitJI == nullptr) {
        rheatrace::jni_hook::hookMethodId(
                env,
                "java/lang/Object",
                "wait", "(JI)V",

                (void*) Object_waitJI,
                (void**) &Origin_waitJI);
        ALOGD("hook wait");
    }
    if (enableWakeup && isNatives[5] > 0 && Origin_notify == nullptr) {
        rheatrace::jni_hook::hookMethodId(
                env,
                "java/lang/Object",
                "notify",
                "()V",
                (void*) Object_notify,
                (void**) &Origin_notify);
        ALOGD("hook notify");
    }
    if (enableWakeup && isNatives[6] > 0 && Origin_notifyAll == nullptr) {
        rheatrace::jni_hook::hookMethodId(
                env,
                "java/lang/Object",
                "notifyAll",
                "()V",
                (void*) Object_notifyAll,
                (void**) &Origin_notifyAll);
        ALOGD("hook notifyAll");
    }
}

void TraceObjectWait::destroy() {
    // JNI no unhook
}

} // rheatrace