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

#include <jni.h>
#include <shadowhook.h>
#include "RheaContext.h"
#include "trace/java_alloc/thread_list.h"
#include "utils/log.h"
#include "sampling/SamplingCollector.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "RheaTrace:GlobalDeps"


extern "C"
JNIEXPORT void JNICALL
Java_com_bytedance_rheatrace_trace_base_TraceGlobal_nativeInit(JNIEnv* env, jclass clazz, jobject main_thread) {
    int shadow_result = shadowhook_init(SHADOWHOOK_MODE_SHARED, false);
    if (shadow_result != SHADOWHOOK_ERRNO_OK) {
        ALOGE("shadowhook_init failed %d", shadow_result);
    }
    rheatrace::RheaContext::javaMainThread = env->NewGlobalRef(main_thread);
    if (!rheatrace::init_thread_list()) {
        ALOGE("init_thread_list failed");
    }

}
extern "C"
JNIEXPORT void JNICALL
Java_com_bytedance_rheatrace_trace_base_TraceGlobal_nativeCapture(JNIEnv* env, jclass clazz, jboolean force) {
    rheatrace::SamplingCollector* collector = rheatrace::SamplingCollector::getInstance();
    if (collector != nullptr) {
        collector->request(rheatrace::SamplingType::kCustom, nullptr, force);
    }
}