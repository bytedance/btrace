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

namespace {

static void free_java_reflections(JNIEnv *env) {
    static const char *VMRuntime_class_name = "dalvik/system/VMRuntime";
    jclass vmRuntime_class = env->FindClass(VMRuntime_class_name);
    void *getRuntime_art_method = env->GetStaticMethodID(vmRuntime_class,
                                                         "getRuntime",
                                                         "()Ldalvik/system/VMRuntime;");
    jobject vmRuntime_instance = env->CallStaticObjectMethod(vmRuntime_class, (jmethodID) getRuntime_art_method);

    const char *target_char = "L";
    jstring stringL = env->NewStringUTF(target_char);
    jclass cls = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(1, cls, nullptr);
    env->SetObjectArrayElement(array, 0, stringL);

    void *setHiddenApiExemptions_art_method = env->GetMethodID(vmRuntime_class,
                                                               "setHiddenApiExemptions",
                                                               "([Ljava/lang/String;)V");
    if (setHiddenApiExemptions_art_method == nullptr) {
        env->ExceptionClear();
        return;
    }
    env->CallVoidMethod(vmRuntime_instance, (jmethodID) setHiddenApiExemptions_art_method, array);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

}


JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *) {
    JNIEnv *env;
    vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    free_java_reflections(env);
    return JNI_VERSION_1_6;
}

