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

#define LOG_TAG "Rhea.thread_proxy"

#include <debug.h>
#include <trace.h>
#include <xcc_fmt.h>
#include <jni.h>
#include <atomic>
#include "thread_proxy.h"
#include "bytehook.h"
#include "../hook/shadowhook_util.h"

namespace bytedance {
    namespace atrace {
        // https://cs.android.com/android/platform/superproject/+/master:bionic/libc/bionic/pthread_internal.h
        /*
         * Copyright (C) 2008 The Android Open Source Project
         * All rights reserved.
         *
         * Redistribution and use in source and binary forms, with or without
         * modification, are permitted provided that the following conditions
         * are met:
         *  * Redistributions of source code must retain the above copyright
         *    notice, this list of conditions and the following disclaimer.
         *  * Redistributions in binary form must reproduce the above copyright
         *    notice, this list of conditions and the following disclaimer in
         *    the documentation and/or other materials provided with the
         *    distribution.
         *
         * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
         * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
         * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
         * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
         * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
         * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
         * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
         * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
         * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
         * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
         * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
         * SUCH DAMAGE.
         */
        struct pthread_internal_t {
            struct pthread_internal_t *next;
            struct pthread_internal_t *prev;
            pid_t tid;
        };

        int pthread_create_proxy(pthread_t *thread, const pthread_attr_t *attr,
                                 void *(*start_routine)(void *), void *arg) {
            BYTEHOOK_STACK_SCOPE();
            int ret = BYTEHOOK_CALL_PREV(pthread_create_proxy,
                                         thread, attr, start_routine, arg);
            if (ret == 0) {
                ATRACE_FORMAT("pthread_create tid=%lu", ((pthread_internal_t *) *thread)->tid);
            }
            return ret;
        }

        void JNI_Thread_sleep_proxy(JNIEnv *env, jclass cls, jobject lock, jlong ms, jint ns) {
            SHADOWHOOK_STACK_SCOPE();
            ATRACE_NAME("Thread#sleep");
            SHADOWHOOK_CALL_PREV(JNI_Thread_sleep_proxy, env, cls, lock, ms, ns);
        }
    }  // namespace atrace
}  // namespace bytedance