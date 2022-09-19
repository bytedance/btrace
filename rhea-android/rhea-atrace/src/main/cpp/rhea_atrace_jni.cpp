/*
 * Copyright (C) 2008 The Android Open Source Project
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

#define LOG_TAG "Rhea.ATrace.JNI"

#include <utils/debug.h>
#include <unistd.h>
#include "atrace.h"
#include "trace_provider.h"

using namespace bytedance::atrace;

static bool SetATraceLocation(JNIEnv *env, jobject thiz, jstring traceDir) {
    const char *trace_dir;
    if (traceDir == nullptr) {
        ALOGE("atrace location must not be null.\n");
        return false;
    }

    trace_dir = env->GetStringUTFChars(traceDir, nullptr);
    TraceProvider::Get().SetTraceFolder(trace_dir);

    env->ReleaseStringUTFChars(traceDir, trace_dir);
    return true;
}

static void SetBlockHookLibs(JNIEnv *env, jobject thiz, jstring blockHookLibs) {
    if (blockHookLibs == nullptr) {
        TraceProvider::Get().SetBlockHookLibs("");
        return;
    }
    const char *block_hook_libs;
    block_hook_libs = env->GetStringUTFChars(blockHookLibs, nullptr);
    TraceProvider::Get().SetBlockHookLibs(block_hook_libs);
    env->ReleaseStringUTFChars(blockHookLibs, block_hook_libs);
}

static bool ParseConfig(JNIEnv *env, jobject thiz, jint key) {
    if (key < 0) {
        ALOGE("config is invalid, the value must > 0");
        return false;
    }
    TraceProvider::Get().SetConfig(ConfigKey::kIO, key & 1);
    TraceProvider::Get().SetConfig(ConfigKey::KMainThreadOnly, key & 2);
    TraceProvider::Get().SetConfig(ConfigKey::kMemory, key & 4);
    TraceProvider::Get().SetConfig(ConfigKey::kClassLoad, key & 8);
    return true;
}

static void SetMainThreadId(pid_t tid) {
    TraceProvider::Get().SetMainThreadId(tid);
    ALOGE("main thread id %d", tid);
}

static void SetBufferSize(size_t buffer_size) {
    TraceProvider::Get().SetBufferSize(buffer_size);
}

// ATrace
static int32_t JNI_startTrace(JNIEnv *env, jobject thiz, jstring traceDir, jstring blockHookLibs,jlong bufferSize, jint configs) {
    if (!SetATraceLocation(env, thiz, traceDir)) {
        return ATRACE_LOCATION_INVALID;
    }
    if (!ParseConfig(env, thiz, configs)) {
        return CONFIG_INVALID;
    }
    SetBlockHookLibs(env, thiz, blockHookLibs);
    SetBufferSize(bufferSize);
    SetMainThreadId(gettid());
    return ATrace::Get().StartTrace();
}

static bool JNI_stopTrace(JNIEnv *env, jobject thiz) {
    return ATrace::Get().StopTrace();
}

static JNINativeMethod methods[] = {
        // ATrace
        {"nativeStart",           "(Ljava/lang/String;Ljava/lang/String;JI)I", (void *) JNI_startTrace},
        {"nativeStop",            "()I",                    (void *) JNI_stopTrace},
};

/*
 * Register several native methods for one class.
 */
static int registerNativeMethods(JNIEnv *env, const char *className,
                                 JNINativeMethod *gMethods, int numMethods) {

    jclass clazz;
    clazz = env->FindClass(className);
    if (clazz == nullptr) {
        ALOGE("Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        ALOGE("RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

/*
 * Register native methods for all classes we know about.
 *
 * returns JNI_TRUE on success.
 */
static int registerNatives(JNIEnv *env) {
    if (!registerNativeMethods(env, "com/bytedance/rheatrace/atrace/RheaATrace",
                               methods, sizeof(methods) / sizeof(methods[0]))) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

// ----------------------------------------------------------------------------
/*
 * This is called by the VM when the shared library is first loaded.
 */

typedef union {
    JNIEnv *env;
    void *venv;
} UnionJNIEnvToVoid;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void * /*reserved*/) {
    UnionJNIEnvToVoid uenv;
    uenv.venv = nullptr;
    jint result = -1;
    JNIEnv *env = nullptr;

    ALOGD("JNI_OnLoad");
    if (vm->GetEnv((void **) &uenv.venv, JNI_VERSION_1_6) != JNI_OK) {
        ALOGE("failed to init jni env");
        goto bail;
    }
    env = uenv.env;
    if (registerNatives(env) != JNI_TRUE) {
        ALOGE("ERROR: registerNatives failed");
        goto bail;
    }

    result = JNI_VERSION_1_6;

    bail:
    return result;
}
