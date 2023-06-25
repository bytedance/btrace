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
#include <utils/debug.h>
#include <trace.h>

static int jniEntranceIndex_ = -1;

static void **GetArtMethod(JNIEnv *env, jobject foo) {
    void **fooArtMethod;
    if (android_get_device_api_level() >= 30) {
        jclass Executable = env->FindClass("java/lang/reflect/Executable");
        jfieldID artMethodField = env->GetFieldID(Executable, "artMethod", "J");
        fooArtMethod = (void **) env->GetLongField(foo, artMethodField);
    } else {
        fooArtMethod = (void **) env->FromReflectedMethod(foo);
    }
    return fooArtMethod;
}

static void InitJNIHook(JNIEnv *env, jobject foo, void *fooJNI) {
    void **fooArtMethod = GetArtMethod(env, foo);
    for (int i = 0; i < 50; ++i) {
        if (fooArtMethod[i] == fooJNI) {
            jniEntranceIndex_ = i;
            break;
        }
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_bytedance_rheatrace_atrace_BlockTrace_nativePlaceHolder(JNIEnv *env, jclass clazz) {
}

static jint (*Object_identityHashCodeNative)(JNIEnv *, jclass, jobject);
// wait / notify
namespace Wait {

    static void (*Origin_notify)(JNIEnv *, jobject);

    static void (*Origin_notifyAll)(JNIEnv *, jobject);

    static void (*Origin_waitJI)(JNIEnv *, jobject, jlong, jint);

    static void Object_waitJI(JNIEnv *env, jobject java_this, jlong ms, jint ns) {
        ATRACE_FORMAT("Object#wait(obj:0x%x, timeout:%d)", Object_identityHashCodeNative(env, nullptr, java_this), ms);
        Origin_waitJI(env, java_this, ms, ns);
    }

    static void Object_notify(JNIEnv *env, jobject java_this) {
        ATRACE_FORMAT("Object#notify(obj:0x%x)", Object_identityHashCodeNative(env, nullptr, java_this));
        Origin_notify(env, java_this);
    }

    static void Object_notifyAll(JNIEnv *env, jobject java_this) {
        ATRACE_FORMAT("Object#notifyAll(obj:0x%x)", Object_identityHashCodeNative(env, nullptr, java_this));
        Origin_notifyAll(env, java_this);
    }

}


// park / unpark
namespace Park {
    static void (*Unsafe_park_origin)(JNIEnv *, jobject, jboolean, jlong);

    static void (*Unsafe_unpark_origin)(JNIEnv *, jobject, jobject);

    static void Unsafe_park(JNIEnv *env, jobject _, jboolean isAbsolute, jlong time) {
        jclass thread = env->FindClass("java/lang/Thread");
        jmethodID currentThread = env->GetStaticMethodID(thread, "currentThread", "()Ljava/lang/Thread;");
        jobject current = env->CallStaticObjectMethod(thread, currentThread);
        ATRACE_FORMAT("Unsafe#park(obj:0x%x, time:%d)", Object_identityHashCodeNative(env, nullptr, current), time);
        Unsafe_park_origin(env, _, isAbsolute, time);
    }

    static void Unsafe_unpark(JNIEnv *env, jobject _, jobject jthread) {
        ATRACE_FORMAT("Unsafe#unpark(obj:0x%x)", Object_identityHashCodeNative(env, nullptr, jthread));
        Unsafe_unpark_origin(env, _, jthread);
    };
}

static void HookJNI(JNIEnv *env, jobject method, void *newEntrance, void **originEntrance) {
    LOG_ALWAYS_FATAL_IF(jniEntranceIndex_ < 0, "JNIEntranceIndex is not ready");
    void **targetArtMethod = GetArtMethod(env, method);
    if (targetArtMethod[jniEntranceIndex_] == newEntrance) {
        return;
    }
    *originEntrance = targetArtMethod[jniEntranceIndex_];
    targetArtMethod[jniEntranceIndex_] = newEntrance;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_bytedance_rheatrace_atrace_BlockTrace_nativeInit(JNIEnv *env, jclass clazz, jobject native_place_holder, jobject identityHashCodeNative, jobject wait, jobject notify, jobject notify_all,
                                                          jobject park, jobject unpark) {
    InitJNIHook(env, native_place_holder, (void *) Java_com_bytedance_rheatrace_atrace_BlockTrace_nativePlaceHolder);
    if (jniEntranceIndex_ == -1) {
        ALOGE("init jni hook error");
        return;
    }
    void **identityHashCodeNativeArtMethod = GetArtMethod(env, identityHashCodeNative);
    Object_identityHashCodeNative = (jint (*)(JNIEnv *, jclass, jobject)) identityHashCodeNativeArtMethod[jniEntranceIndex_];
    using namespace Wait;
    using namespace Park;
    HookJNI(env, wait, (void *) Object_waitJI, (void **) &Origin_waitJI);
    HookJNI(env, notify, (void *) Object_notify, (void **) &Origin_notify);
    HookJNI(env, notify_all, (void *) Object_notifyAll, (void **) &Origin_notifyAll);
    HookJNI(env, park, (void *) Unsafe_park, (void **) &Unsafe_park_origin);
    HookJNI(env, unpark, (void *) Unsafe_unpark, (void **) &Unsafe_unpark_origin);
}