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
#include <utils/build.h>
#include <features/binder/binder_proxy.h>
#include <second_party/byte_hook/shadowhook.h>
#include "atrace.h"
#include "trace_provider.h"
#include "features/render/render_proxy.h"
#include "binary_trace.h"

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

static void SetBlockHookLibs(JNIEnv *env, jobject thiz) {
    char value[PROP_VALUE_MAX] = {0};
    __system_property_get("debug.rhea.blockHookLibs", value);
    TraceProvider::Get().SetBlockHookLibs(value);
}

static bool ParseConfig(JNIEnv *env, jobject thiz, jobjectArray categories) {
    if (nullptr == categories) {
        ALOGE("config is invalid, the value must > 0");
        return false;
    }
    std::vector<std::string> categoriesVector;
    int len = env->GetArrayLength(categories);
    for (int i = 0; i < len; ++i) {
        auto jitem = (jstring) env->GetObjectArrayElement(categories, i);
        auto item = env->GetStringUTFChars(jitem, nullptr);
        categoriesVector.emplace_back(item);
        env->ReleaseStringUTFChars(jitem, item);
    }
    TraceProvider::Get().SetCategories(categoriesVector);
    return true;
}

static void SetMainThread(pid_t tid) {
    TraceProvider::Get().SetMainThreadId(tid);
    ALOGE("main thread id %d", tid);
}

constexpr int32_t MIN_BUFFER_SIZE = 500000; // 50w

static void SetBufferSize() {
    char value[PROP_VALUE_MAX];
    __system_property_get("debug.rhea.atraceBufferSize", value);
    auto size = strtoll(value, nullptr, 10);
    if (size < MIN_BUFFER_SIZE) {
        size = MIN_BUFFER_SIZE;
    }
    TraceProvider::Get().SetBufferSize(size);
}

// ATrace
static int32_t
JNI_startTrace(JNIEnv *env, jobject thiz, jstring traceDir) {
    shadowhook_init(SHADOWHOOK_MODE_SHARED, false);
    if (!SetATraceLocation(env, thiz, traceDir)) {
        return ATRACE_LOCATION_INVALID;
    }
    SetBlockHookLibs(env, thiz);
    SetBufferSize();
    SetMainThread( gettid());
    return ATrace::Get().StartTrace();
}

static bool JNI_stopTrace(JNIEnv *env, jobject thiz) {
    return ATrace::Get().StopTrace();
}

static jboolean JNI_startWhenAppLaunch(JNIEnv *env, jobject thiz) {
    char value[PROP_VALUE_MAX];
    __system_property_get("debug.rhea.startWhenAppLaunch", value);
    return value[0] == '1';
}

static jboolean JNI_isMainThreadOnly(JNIEnv *env, jobject thiz) {
    char value[PROP_VALUE_MAX];
    __system_property_get("debug.rhea.mainThreadOnly", value);
    return value[0] == '1';
}

static jboolean JNI_isRenderCategoryEnabled(JNIEnv *env, jobject thiz) {
    return TraceProvider::Get().isEnableDetailRender();
}

static jint JNI_getHttpServerPort(JNIEnv *env, jobject thiz) {
    char value[PROP_VALUE_MAX];
    __system_property_get("debug.rhea.httpServerPort", value);
    return (jint) strtol(value, nullptr, 10);
}

static jobjectArray JNI_getBinderInterfaceTokens(JNIEnv *env, jobject thiz) {
    auto names = all_interface_token_names();
    jobjectArray array = env->NewObjectArray((jsize) names.size(),
                                             env->FindClass("java/lang/String"),
                                             nullptr);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    int i = 0;
    for (const auto &name : names) {
        jstring jname = env->NewStringUTF(name.c_str());
        env->SetObjectArrayElement(array, i, jname);
        env->DeleteLocalRef(jname);
        i++;
    }
    auto gArray = (jobjectArray) env->NewGlobalRef(array);
    env->DeleteLocalRef(array);
    return gArray;
}

static jint JNI_getArch(JNIEnv *env, jclass) {
#ifdef __LP64__
    return 64;
#else
    return 32;
#endif
}

static JNINativeMethod methods[] = {
        {"nativeStart",                    "(Ljava/lang/String;)I", (void *) JNI_startTrace},
        {"nativeStop",                     "()I",                   (void *) JNI_stopTrace},
        {"nativeGetBinderInterfaceTokens", "()[Ljava/lang/String;", (void *) JNI_getBinderInterfaceTokens},
        {"nativeStartWhenAppLaunch",       "()Z",                   (void *) JNI_startWhenAppLaunch},
        {"nativeMainThreadOnly",           "()Z",                   (void *) JNI_isMainThreadOnly},
        {"nativeRenderCategoryEnabled",    "()Z",                   (void *) JNI_isRenderCategoryEnabled},
        {"nativeGetHttpServerPort",        "()I",                   (void *) JNI_getHttpServerPort},
        {"nativeGetArch",                  "()I",                   (void *) JNI_getArch},
};

static void RenderUtils_attachViewInfo(JNIEnv* env, jclass utils, jlong nativeRenderNode, jstring viewNameString, jstring xmlNameString, jint viewId) {
    void* renderNode = reinterpret_cast<void*>(nativeRenderNode);
    const char* viewName;
    if (viewNameString != nullptr) {
        viewName = env->GetStringUTFChars(viewNameString, nullptr);
    } else {
        viewName = nullptr;
    }
    const char* xmlName;
    if (xmlNameString != nullptr) {
        xmlName = env->GetStringUTFChars(xmlNameString, nullptr);
    } else {
        xmlName = nullptr;
    }
    bytedance::atrace::render::renderNodeAttachViewInfo(renderNode, viewName, xmlName, viewId);
    if (viewNameString != nullptr) {
        env->ReleaseStringUTFChars(viewNameString, viewName);
    }
    if (xmlNameString != nullptr) {
        env->ReleaseStringUTFChars(xmlNameString, xmlName);
    }
}

static void RenderUtils_attachRootViewInfo(JNIEnv* env, jclass utils, jlong nativeRenderNode, jstring windowNameString) {
    if (windowNameString != nullptr) {
        const char* windowName = env->GetStringUTFChars(windowNameString, nullptr);
        void* renderNode = reinterpret_cast<void*>(nativeRenderNode);
        bytedance::atrace::render::renderNodeAttachViewInfo(renderNode, "RootRenderNode", windowName, -1);
        env->ReleaseStringUTFChars(windowNameString, windowName);
    }
}

static JNINativeMethod renderUtilsMethods[] = {
        {
                "nAttachViewInfo",
                "(JLjava/lang/String;Ljava/lang/String;I)V",
                (void *) RenderUtils_attachViewInfo
        },
        {
                "nAttachRootViewInfo",
                "(JLjava/lang/String;)V",
                (void*) RenderUtils_attachRootViewInfo
        },
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
    if (!registerNativeMethods(env, "com/bytedance/rheatrace/atrace/render/RenderUtils", renderUtilsMethods, sizeof(renderUtilsMethods) / sizeof(methods[0]))) {
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

static void free_java_reflections(JNIEnv *env) {
    const char *VMRuntime_class_name = "dalvik/system/VMRuntime";
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
    env->CallVoidMethod(vmRuntime_instance, (jmethodID) setHiddenApiExemptions_art_method, array);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

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
    if (bytedance::utils::Build::getAndroidSdk() >= __ANDROID_API_P__) {
        free_java_reflections(env);
    }
    if (registerNatives(env) != JNI_TRUE || rheatrace::binary::registerNatives(env) != JNI_TRUE) {
        ALOGE("ERROR: registerNatives failed");
        goto bail;
    }

    result = JNI_VERSION_1_6;

    bail:
    return result;
}
