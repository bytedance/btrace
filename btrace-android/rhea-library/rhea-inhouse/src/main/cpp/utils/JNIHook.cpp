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

#include "JNIHook.h"

#include "log.h"
#include <atomic>

namespace rheatrace::jni_hook {

static int jniEntranceIndex_ = -1;

void init(JNIEnv* env, jobject foo, void* fooJNI) {
    void** fooArtMethod;
    if (android_get_device_api_level() >= 30) {
        jclass Executable = env->FindClass("java/lang/reflect/Executable");
        jfieldID artMethodField = env->GetFieldID(Executable, "artMethod", "J");
        fooArtMethod = (void**) env->GetLongField(foo, artMethodField);
    } else {
        fooArtMethod = (void**) env->FromReflectedMethod(foo);
    }
    for (int i = 0; i < 50; ++i) {
        if (fooArtMethod[i] == fooJNI) {
            jniEntranceIndex_ = i;
            break;
        }
    }
}

static void hookJNIMethod(JNIEnv* env, jobject method, void* newEntrance, void** originEntrance) {
    if (jniEntranceIndex_ < 0 || originEntrance == nullptr) {
        ALOGE("JNIEntranceIndex is not ready");
        return;
    }
    void** targetArtMethod;
    if (android_get_device_api_level() >= 30) {
        jclass Executable = env->FindClass("java/lang/reflect/Executable");
        jfieldID artMethodField = env->GetFieldID(Executable, "artMethod", "J");
        targetArtMethod = (void**) env->GetLongField(method, artMethodField);
    } else {
        targetArtMethod = (void**) env->FromReflectedMethod(method);
    }
    void* oldEntrance = targetArtMethod[jniEntranceIndex_];
    if (oldEntrance == newEntrance || oldEntrance == nullptr) {
        return;
    }

    std::atomic<void*>* atomicEntry =
            reinterpret_cast<std::atomic<void*>*>(targetArtMethod + jniEntranceIndex_);
    if (std::atomic_compare_exchange_weak_explicit(atomicEntry, &oldEntrance, newEntrance,
                                                   std::memory_order_relaxed,
                                                   std::memory_order_acquire)) {
        *originEntrance = oldEntrance;
    }
}

static void
hookJNIMethodId(JNIEnv* env, jclass cls, jmethodID methodId, bool isStatic, void* newEntrance,
                void** originEntrance) {
    if (jniEntranceIndex_ < 0 || originEntrance == nullptr) {
        ALOGE("JNIEntranceIndex is not ready");
        return;
    }
    void** targetArtMethod;
    if (android_get_device_api_level() >= 30) {
        auto method = env->ToReflectedMethod(cls, methodId, isStatic);
        jclass Executable = env->FindClass("java/lang/reflect/Executable");
        jfieldID artMethodField = env->GetFieldID(Executable, "artMethod", "J");
        targetArtMethod = (void**) env->GetLongField(method, artMethodField);
    } else {
        targetArtMethod = (void**) methodId;
    }
    void* oldEntrance = targetArtMethod[jniEntranceIndex_];
    if (oldEntrance == newEntrance || oldEntrance == nullptr) {
        return;
    }

    std::atomic<void*>* atomicEntry =
            reinterpret_cast<std::atomic<void*>*>(targetArtMethod + jniEntranceIndex_);
    *originEntrance = oldEntrance;
    std::atomic_compare_exchange_weak_explicit(atomicEntry, originEntrance, newEntrance,
                                               std::memory_order_relaxed,
                                               std::memory_order_acquire);
}

void hook(JNIEnv* env, jobject method, void* newEntrance, void** originEntrance) {
    hookJNIMethod(env, method, newEntrance, originEntrance);
}

void hookStaticMethodId(JNIEnv* env, const char* className, const char* methodName,
                        const char* descriptor, void* newEntrance, void** originEntrance) {
    auto cls = env->FindClass(className);
    auto methodId = env->GetStaticMethodID(cls, methodName, descriptor);
    if (methodId == nullptr) {
        env->ExceptionClear();
        ALOGE("%s error. NoSuchMethod %s#%s%s", __FUNCTION__, className, methodName, descriptor);
        return;
    }
    hookJNIMethodId(env, cls, methodId, true, newEntrance, originEntrance);
    ALOGD("%s hook %s#%s%s success", __FUNCTION__, className, methodName, descriptor);
}

void* getStaticEntrance(JNIEnv* env, const char* className, const char* methodName,
                        const char* descriptor) {
    auto cls = env->FindClass(className);
    auto methodId = env->GetStaticMethodID(cls, methodName, descriptor);
    if (methodId == nullptr) {
        env->ExceptionClear();
        ALOGE("%s error. NoSuchMethod %s#%s%s", __FUNCTION__, className, methodName, descriptor);
        return nullptr;
    }
    void** targetArtMethod;
    if (android_get_device_api_level() >= 30) {
        auto method = env->ToReflectedMethod(cls, methodId, true);
        jclass Executable = env->FindClass("java/lang/reflect/Executable");
        jfieldID artMethodField = env->GetFieldID(Executable, "artMethod", "J");
        targetArtMethod = (void**) env->GetLongField(method, artMethodField);
    } else {
        targetArtMethod = (void**) methodId;
    }
    return targetArtMethod[jniEntranceIndex_];
}

void
hookMethodId(JNIEnv* env, const char* className, const char* methodName, const char* descriptor,
             void* newEntrance, void** originEntrance) {
    auto cls = env->FindClass(className);
    auto methodId = env->GetMethodID(cls, methodName, descriptor);
    if (methodId == nullptr) {
        env->ExceptionClear();
        ALOGE("%s error. NoSuchMethod %s#%s%s", __FUNCTION__, className, methodName, descriptor);
        return;
    }
    hookJNIMethodId(env, cls, methodId, false, newEntrance, originEntrance);
    ALOGD("%s hook %s#%s%s success", __FUNCTION__, className, methodName, descriptor);
}

} // rheatrace

extern "C"
JNIEXPORT void JNICALL
Java_com_bytedance_rheatrace_trace_utils_JNIHook_nativePlaceHolder(JNIEnv* env, jclass clazz) {
}

extern "C"
JNIEXPORT void JNICALL
Java_com_bytedance_rheatrace_trace_utils_JNIHook_nativeInit(JNIEnv* env, jclass clazz, jobject foo) {
    rheatrace::jni_hook::init(env, foo,
                              (void*) Java_com_bytedance_rheatrace_trace_utils_JNIHook_nativePlaceHolder);
}