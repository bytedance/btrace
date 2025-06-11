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
#include "SamplingTrace.h"

#include <cstdlib>
#include <mutex>

#include "TraceBinderCall.h"

#include "../utils/misc.h"
#include "../utils/time.h"

#define LOG_TAG "RheaTrace.SamplingTrace"
#include "../utils/log.h"
#include "TraceGC.h"
#include "TraceJNICall.h"
#include "TraceJavaMonitor.h"
#include "TraceLoadLibrary.h"
#include "TraceObjectWait.h"
#include "TraceUnsafePark.h"
#include "TraceMessageIDChange.h"
#include "java_alloc/TraceJavaAlloc.h"

#define ANDROID_8_0_SDK_INT 26
#define ANDROID_11_SDK_INT 30
#define ANDROID_12_SDK_INT 31
#define ANDROID_14_SDK_INT 34

namespace rheatrace::trace {

static  bool inited = false;
static jlong *configArray_ = nullptr;
static bool enableObjectAlloc_ = false;
static bool enableWakeup_ = false;
static bool shadowPause_ = false;

bool doInit(JNIEnv *);

bool
init(JNIEnv* env, jlongArray pArray, bool enableObjectAlloc, bool enableWakeup, bool shadowPause) {
#ifndef __aarch64__
    return false;
#endif
    auto sdk = get_android_sdk_version();
    if (sdk < ANDROID_8_0_SDK_INT) {
        return false;
    }
    if (inited) {
        return true;
    }
    enableObjectAlloc_ = enableObjectAlloc;
    enableWakeup_ = enableWakeup;
    shadowPause_ = shadowPause;
    auto len = env->GetArrayLength(pArray);
    configArray_ = static_cast<jlong*>(malloc(len * sizeof(jlong)));
    env->GetLongArrayRegion(pArray, 0, len, configArray_);
    return doInit(env);
}
static std::mutex hookUnhookLock;

bool doInit(JNIEnv* env) {
    std::lock_guard<std::mutex> lock(hookUnhookLock);
    auto now = current_boot_time_millis();
    ALOGD("init sampling: alloc:%d, wakeup:%d", enableObjectAlloc_, enableWakeup_);
    if (enableObjectAlloc_) {
        TraceJavaAlloc::init();
    }
    TraceBinderCall::init();
    TraceGC::init();
    TraceJNICall::init();
    TraceMessageIDChange::init(env);
    TraceJavaMonitor::init(enableWakeup_);
    TraceLoadLibrary::init();
    TraceObjectWait::init(env, enableWakeup_, configArray_);
    TraceUnsafePark::init(env, enableWakeup_, configArray_);
    ALOGD("rheatrace hooks cost %lums", current_boot_time_millis() - now);
    inited = true;
    return true;
}

bool doUnInit(JNIEnv *env) {
    std::lock_guard<std::mutex> lock(hookUnhookLock);
    auto now = current_boot_time_millis();
    if (enableObjectAlloc_) {
        TraceJavaAlloc::destroy();
    }
    TraceBinderCall::destroy();
    TraceGC::destroy();
    TraceJNICall::destroy();
    TraceMessageIDChange::destroy();
    TraceJavaMonitor::destroy();
    TraceLoadLibrary::destroy();
    TraceObjectWait::destroy();
    TraceUnsafePark::destroy();
    ALOGD("rheatrace unhooks cost %lums", current_boot_time_millis() - now);
    inited = false;
    return true;
}

} // namespace rheatrace::sampling

