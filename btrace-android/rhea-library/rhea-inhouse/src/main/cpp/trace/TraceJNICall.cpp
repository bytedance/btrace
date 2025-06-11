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
//
// Created by ByteDance on 2024/12/4.
//

#include "TraceJNICall.h"
#include <shadowhook.h>
#include "../sampling//SamplingCollector.h"
#include "../utils/misc.h"

#define LOG_TAG "RheaTrace:JNI"
#include "../utils/log.h"

#define ANDROID_8_0_SDK_INT 26
#define ANDROID_11_SDK_INT 30
#define ANDROID_12_SDK_INT 31
#define ANDROID_14_SDK_INT 34


struct TwoWordReturn {
    uintptr_t lo;
    uintptr_t hi;
};

namespace rheatrace {

static TwoWordReturn JNI_artQuickGenericJniTrampoline_8to10(void *self, void **managed_sp) {
    SHADOWHOOK_STACK_SCOPE();
    auto result = SHADOWHOOK_CALL_PREV(JNI_artQuickGenericJniTrampoline_8to10, self, managed_sp);
    SamplingCollector::request(SamplingType::kJNITrampoline, self);
    return result;
}

static void *JNI_artQuickGenericJniTrampoline_11to14(void *self, void **managed_sp, uintptr_t *reserved_area) {
    SHADOWHOOK_STACK_SCOPE();
    auto result = SHADOWHOOK_CALL_PREV(JNI_artQuickGenericJniTrampoline_11to14, self, managed_sp, reserved_area);
    SamplingCollector::request(SamplingType::kJNITrampoline, self);
    return result;
}

static void *jniStub = nullptr;

void TraceJNICall::init() {
    auto sdk = get_android_sdk_version();
    auto jniTrampoline = sdk < ANDROID_11_SDK_INT ? (void *) JNI_artQuickGenericJniTrampoline_8to10 : (void *) JNI_artQuickGenericJniTrampoline_11to14;
    jniStub = shadowhook_hook_sym_name("libart.so", "artQuickGenericJniTrampoline", jniTrampoline, nullptr);
    ALOGE("artQuickGenericJniTrampoline %p", jniStub);
}

void TraceJNICall::destroy() {
    if (jniStub != nullptr) {
        shadowhook_unhook(jniStub);
        jniStub = nullptr;
    }
}

} // rheatrace