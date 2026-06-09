/*
 * Copyright (c) 2026 Bytedance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jni.h"

#include <cxxabi.h>


static jstring JNI_demangle(JNIEnv*env, jclass, jstring in) {
    auto* name = env->GetStringUTFChars(in, nullptr);
    auto result = abi::__cxa_demangle(name, nullptr, nullptr, nullptr);
    env->ReleaseStringUTFChars(in, name);
    return env->NewStringUTF(result);
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *) {
    JNIEnv *env;
    vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    auto clazz = env->FindClass("com/bytedance/jarvis/trace/utils/Demangler");
    if (clazz) {
        JNINativeMethod a{"nativeDemangle", "(Ljava/lang/String;)Ljava/lang/String;", (void *) JNI_demangle};
        JNINativeMethod methods[]{a};
        env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(methods[0]));
    }
    return JNI_VERSION_1_6;
}
