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
#pragma once

#include <jni.h>

namespace rheatrace::jni_hook {

void init(JNIEnv *env, jobject foo, void *fooJNI);

void hook(JNIEnv *env, jobject method, void *newEntrance, void **originEntrance);

void hookStaticMethodId(JNIEnv *env, const char *className, const char *methodName, const char *descriptor, void *newEntrance, void **originEntrance);

void hookMethodId(JNIEnv *env, const char *className, const char *methodName, const char *descriptor, void *newEntrance, void **originEntrance);

void *getStaticEntrance(JNIEnv *env, const char *className, const char *methodName, const char *descriptor);

} // rheatrace::jni_hook
