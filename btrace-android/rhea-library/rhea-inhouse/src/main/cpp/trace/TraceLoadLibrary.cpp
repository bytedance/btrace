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

#include "TraceLoadLibrary.h"

#include <jni.h>
#include <shadowhook.h>

#include "../sampling/SamplingCollector.h"

namespace rheatrace {

static jstring
myJvmNativeLoad(JNIEnv* env, jstring javaFilename, jobject javaLoader, jstring javaLibSearchPath) {
    SHADOWHOOK_STACK_SCOPE();
    ScopeSampling ss(SamplingType::kLoadLibrary);
    return SHADOWHOOK_CALL_PREV(myJvmNativeLoad, env, javaFilename, javaLoader, javaLibSearchPath);
}

static void *stub = nullptr;

void TraceLoadLibrary::init() {
    stub = shadowhook_hook_sym_name("libopenjdkjvm.so", "JVM_NativeLoad", (void*) myJvmNativeLoad,
                                    nullptr);
}

void TraceLoadLibrary::destroy() {
    if (stub != nullptr) {
        shadowhook_unhook(stub);
        stub = nullptr;
    }
}

}