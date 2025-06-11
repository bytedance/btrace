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
#include "sampling/SamplingCollector.h"
#include "utils/log.h"

extern "C"
JNIEXPORT jlong JNICALL
Java_com_bytedance_rheatrace_trace_base_TraceAbility_nativeCreate(
        JNIEnv* env, jobject thiz, jint type, jlongArray configs) {
    switch (type) {
        case rheatrace::TYPE_SAMPLING:
            return reinterpret_cast<jlong>(rheatrace::SamplingCollector::create(env,configs));
        default:
            return 0;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_bytedance_rheatrace_trace_base_TraceAbility_nativeStart(
        JNIEnv* env, jobject thiz, jlong collector, jlongArray extraConfigs) {
    reinterpret_cast<rheatrace::PerfCollector*>(collector)->start(env, extraConfigs);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_bytedance_rheatrace_trace_base_TraceAbility_nativeUpdateConfig(
        JNIEnv* env, jobject thiz, jlong collector, jlongArray configs) {
    reinterpret_cast<rheatrace::PerfCollector*>(collector)->updateConfigs(env,configs);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_bytedance_rheatrace_trace_base_TraceAbility_nativeMark(
        JNIEnv* env, jobject thiz, jlong collector) {
    return reinterpret_cast<rheatrace::PerfCollector*>(collector)->mark();
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_bytedance_rheatrace_trace_base_TraceAbility_nativeDumpTokenRange(
        JNIEnv* env, jobject thiz, jlong collector, jlong start, jlong end, jstring path, jstring jextra) {
    const char *dump_path = env->GetStringUTFChars(path, nullptr);
    const char* extra = nullptr;
    int32_t extraLen = 0;
    if (jextra != nullptr) {
        extra = env->GetStringUTFChars(jextra, nullptr);
        extraLen = env->GetStringUTFLength(jextra);
    }
    int result = reinterpret_cast<rheatrace::PerfCollector*>(collector)->dumpPart(env, dump_path,
                                                                                  extra, extraLen,
                                                                                  start, end);
    ALOGI("dump [%ld, %ld] result is %d, error is %s", start, end, result, strerror(result));
    env->ReleaseStringUTFChars(path, dump_path);
    if (extra != nullptr) {
        env->ReleaseStringUTFChars(jextra, extra);
    }
    return result;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_bytedance_rheatrace_trace_base_TraceAbility_nativeStop(
        JNIEnv* env, jobject thiz, jlong collector) {
    reinterpret_cast<rheatrace::PerfCollector*>(collector)->stop();
}