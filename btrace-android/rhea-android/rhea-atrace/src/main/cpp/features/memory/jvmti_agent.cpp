/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define LOG_TAG "jvmti"

#include <utils/debug.h>
#include <utils/proc_fs.h>
#include <utils/strings.h>
#include <utils/timers.h>

#include "trace_provider.h"
#include "atrace.h"
#include "trace.h"

#include "../../third_party/jvmti/jvmti.h"

#define MIN_CLASS_SIZE 4096
#define MIN_ALLOC_OBJECT_SIZE 4096

namespace bytedance {
namespace atrace {

void AgentMain(jvmtiEnv* jvmti, JNIEnv* jni, void* arg) {
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, nullptr);
}

void CbVmInit(jvmtiEnv* jvmti, JNIEnv* env, jthread thr) {
    jobject  thread_name = env->NewStringUTF("Agent Thread");
    jclass thread_klass = env->FindClass("java/lang/Thread");
    jobject thread = env->AllocObject(thread_klass);

    env->CallNonvirtualVoidMethod(
            thread,
            thread_klass,
            env->GetMethodID(thread_klass, "<init>", "(Ljava/lang/String;)V"),
            thread_name);
    env->CallVoidMethod(thread, env->GetMethodID(thread_klass, "setPriority", "(I)V"), 1);
    env->CallVoidMethod(
            thread, env->GetMethodID(thread_klass, "setDaemon", "(Z)V"), JNI_TRUE);

    jvmti->RunAgentThread(thread, AgentMain, nullptr, JVMTI_THREAD_MIN_PRIORITY);
}

void CbClassFileLoadHook(jvmtiEnv* jvmti,
                                JNIEnv* env,
                                jclass classBeingRedefined,
                                jobject loader,
                                const char* name,
                                jobject protectionDomain,
                                jint classDataLen,
                                const unsigned char* classData,
                                jint* newClassDataLen,
                                unsigned char** newClassData) {

    if (!TraceProvider::Get().isEnableClassLoad()) {
        return;
    }
    if (classDataLen < MIN_CLASS_SIZE) {
        return;
    }

    std::string classDataString(name);
    ATRACE_BEGIN_VALUE("ClassFileLoad:", classDataString.c_str());
    ATRACE_END();
}


void cbVMObjectAlloc(jvmtiEnv *jvmti,
                            JNIEnv* env,
                            jthread thread,
                            jobject object,
                            jclass object_klass,
                            jlong size) {
    if (!TraceProvider::Get().isEnableMemory()) {
        return;
    }
    if (size < MIN_ALLOC_OBJECT_SIZE) {
        return;
    }

    jclass cls = env->FindClass("java/lang/Class");
    jmethodID mid_getName = env->GetMethodID(cls, "getName", "()Ljava/lang/String;");
    jstring strObj = (jstring)env->CallObjectMethod(cls, mid_getName);
    const char* local_name = env->GetStringUTFChars(strObj, 0);

    std::string allocString(local_name);
    allocString.append("  size:");
    allocString.append(utils::to_string(size));

    ATRACE_BEGIN_VALUE("ObjectAlloc:", allocString.c_str());
    env->ReleaseStringUTFChars(strObj, local_name);
    ATRACE_END();
}


template <bool kIsOnLoad>
jint AgentStart(JavaVM* vm, char* options, void* reserved) {
    jvmtiEnv* jvmti = nullptr;

    if (vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_1) != JNI_OK ||
        jvmti == nullptr) {
        ALOGE("unable to obtain JVMTI env.");
        return JNI_ERR;
    }

    jvmtiCapabilities caps{
            .can_retransform_classes = 1,
            .can_signal_thread = 1,
            .can_get_owned_monitor_info = 1,
            .can_generate_method_entry_events = 1,
            .can_generate_exception_events = 1,
            .can_generate_vm_object_alloc_events = 1,
            .can_tag_objects = 1,
    };
    if (jvmti->AddCapabilities(&caps) != JVMTI_ERROR_NONE) {
        ALOGE("Unable to get retransform_classes capability!");
        return JNI_ERR;
    }
    jvmtiEventCallbacks cb{
            .VMInit = CbVmInit,
            .ClassFileLoadHook = CbClassFileLoadHook,
            .VMObjectAlloc = cbVMObjectAlloc,
    };
    jvmti->SetEventCallbacks(&cb, sizeof(cb));
    if (kIsOnLoad) {
        jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
    } else {
        JNIEnv* jni = nullptr;
        vm->GetEnv(reinterpret_cast<void**>(&jni), JNI_VERSION_1_2);
        jthread thr;
        jvmti->GetCurrentThread(&thr);
        CbVmInit(jvmti, jni, thr);
    }
    return JNI_OK;
}


// Late attachment (e.g. 'am attach-agent').
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
    ALOGE("Agent_OnAttach");
    return AgentStart<false>(vm, options, reserved);
}

// Early attachment
extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* reserved) {
    ALOGE("Agent_OnLoad");
    return AgentStart<true>(jvm, options, reserved);
}
}  // namespace atrace
}  // namespace bytedance