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

#include <fcntl.h>
#include <sys/mman.h>
#include <atomic>
#include <unistd.h>
#include <utils/timers.h>
#include "utils/debug.h"
#include <cerrno>
#include <cstdio>
#include <string>
#include <sys/system_properties.h>
#include "binary_trace.h"
#include "../trace/thread_name_collector.h"

static int bufferFd_;
static uint8_t *buffer_;
static off_t bufferSize8_;
static std::atomic_int64_t next8_(0);
static uint64_t firstNanoTime_;
static long durationThreshold_ = 0;

static jlong JNI_CriticalTraceBegin(jint mid) {
    return elapsedRealtimeNanos();
}

static void JNI_CriticalTraceEnd(jint mid, jlong begin) {
    // in some situation. when tracing started. some function may have running a while.
    // we have not record the begin time of this function. so begin time is -1.
    // we adjust the begin time as tracing start time here.
    if UNLIKELY(begin < 0) {
        begin = (jlong) firstNanoTime_;
    }
    auto dur = elapsedRealtimeNanos() - begin;
    if LIKELY(dur > durationThreshold_) {
        int64_t next = next8_.fetch_add(16, std::memory_order_relaxed);
        if LIKELY(next + 16 < bufferSize8_) {
            // startTime	45 // 9H
            // duration	    45 // 9H
            // threadId	    15
            // methodId	    23 // 800w
            auto *buffer64 = (uint64_t *) (buffer_ + next);
            uint64_t startTime = begin - firstNanoTime_; // may less than 0
            uint64_t tid = gettid();
            // 45-startTime + 19-high-bits of duration
            buffer64[0] = (startTime << 19LU) | (dur >> 26LU);
            // 25-low-bits of duration | tid | mid
            buffer64[1] = ((dur & 0x3FFFFFFLU) << 38LU) | (tid << 23LU) | mid;
        }
    }
}

static jlong JNI_FastBegin(JNIEnv *env, jclass _, jint mid) {
    return JNI_CriticalTraceBegin(mid);
}

static void JNI_FastEnd(JNIEnv *env, jclass _, jint mid, jlong begin) {
    return JNI_CriticalTraceEnd(mid, begin);
}

static jlong JNI_GetMaxAppTraceBufferSize(JNIEnv *env, jclass _) {
    return bufferSize8_;
}

static jlong JNI_GetCurrentAppTraceBufferSize(JNIEnv *env, jclass _) {
    return next8_.load(std::memory_order_relaxed);
}

int rheatrace::binary::registerNatives(JNIEnv *env) {
    auto clazz = env->FindClass("com/bytedance/rheatrace/atrace/BinaryTrace");
    if (clazz) {
        {
            JNINativeMethod t[] = {
                    {
                            "nativeGetCurrentAppTraceBufferSize", "()J", (void *) JNI_GetCurrentAppTraceBufferSize
                    },
                    {
                            "nativeGetMaxAppTraceBufferSize",     "()J", (void *) JNI_GetMaxAppTraceBufferSize
                    }
            };
            env->RegisterNatives(clazz, t, 2);
        }
        char sdk_version_str[PROP_VALUE_MAX];
        __system_property_get("ro.build.version.sdk", sdk_version_str);
        auto api = strtol(sdk_version_str, nullptr, 10);
        if (api < __ANDROID_API_O__) {
            ALOGI("register JNI as normal");
            JNINativeMethod t[] = {{"nativeBeginMethod", "!(I)J",  (void *) JNI_FastBegin},
                                   {"nativeEndMethod",   "!(IJ)V", (void *) JNI_FastEnd}};
            return env->RegisterNatives(clazz, t, 2) == 0;
        } else {
            ALOGI("register JNI as Critical");
            JNINativeMethod t[] = {{"nativeBeginMethod", "(I)J",  (void *) JNI_CriticalTraceBegin},
                                   {"nativeEndMethod",   "(IJ)V", (void *) JNI_CriticalTraceEnd}};
            return env->RegisterNatives(clazz, t, 2) == 0;
        }
    }
    return JNI_ERR;
}

constexpr int32_t METHOD_ID_MIN_SIZE = 2000000; //  200w

static void initTraceDurationThreshold() {
    char value[PROP_VALUE_MAX];
    __system_property_get("debug.rhea.methodDurThreshold", value);
    durationThreshold_ = strtoll(value, nullptr, 10);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_bytedance_rheatrace_atrace_BinaryTrace_nativeInit(JNIEnv *env, jclass clazz,
                                                           jstring filepath) {
    rheatrace::thread_name_collector::start();
    initTraceDurationThreshold();
    int headerSize = 32;
    if (bufferFd_ > 0) {
        ALOGI("reset previous binary trace file");
        next8_.store(headerSize, std::memory_order_relaxed);
    } else {
        auto *path = env->GetStringUTFChars(filepath, nullptr);
        bufferFd_ = open(path, O_RDWR | O_CREAT, (mode_t) 0600);
        LOG_ALWAYS_FATAL_IF(bufferFd_ < 0, "open file error %s %s", path, strerror(errno));
        env->ReleaseStringUTFChars(filepath, path);

        char value[PROP_VALUE_MAX];
        __system_property_get("debug.rhea.methodIdMaxSize", value);
        bufferSize8_ = strtoll(value, nullptr, 10);
        if (bufferSize8_ < METHOD_ID_MIN_SIZE) {
            bufferSize8_ = METHOD_ID_MIN_SIZE;
        }
        ftruncate(bufferFd_, (off_t) (bufferSize8_));
        buffer_ = (uint8_t *) mmap(nullptr, bufferSize8_,
                                   PROT_READ | PROT_WRITE, MAP_SHARED,
                                   bufferFd_, 0);
        LOG_ALWAYS_FATAL_IF(buffer_ == MAP_FAILED, "mmap failed %s", strerror(errno));
        ALOGI("Binary Trace init with size %ld(%ld) %p", bufferSize8_, bufferSize8_, buffer_);
        next8_.store(headerSize, std::memory_order_relaxed);
        auto buffer32 = (uint32_t *) buffer_;
        auto buffer64 = (uint64_t *) buffer_;
        buffer32[0] = 5; // version
        buffer32[1] = gettid();
        buffer64[1] = systemTime(SYSTEM_TIME_BOOTTIME);
        firstNanoTime_ = elapsedRealtimeNanos();
        buffer64[2] = firstNanoTime_;
    }
    auto buffer64 = (uint64_t *) buffer_;
    buffer64[3] = elapsedRealtimeNanos(); // current trace begin time
}

extern "C"
JNIEXPORT void JNICALL
Java_com_bytedance_rheatrace_atrace_BinaryTrace_nativeStop(JNIEnv *env, jclass clazz) {
    int64_t size8 = next8_.load(std::memory_order_relaxed);
    size_t writeSize = size8 * sizeof(uint8_t);
    msync(buffer_, writeSize, MS_SYNC);
    // munmap may cause crash. keep it commented
    // munmap(buffer_, bufferSize8_);
    // close(bufferFd_);
    ALOGD("Binary Trace stop with size8 %ld", (long) size8);
    rheatrace::thread_name_collector::stop();
}


extern "C"
JNIEXPORT jstring JNICALL
Java_com_bytedance_rheatrace_atrace_BinaryTrace_nativeGetThreadsInfo(JNIEnv *env, jclass clazz) {
    auto result = rheatrace::thread_name_collector::dump();
    return env->NewStringUTF(result.c_str());
}