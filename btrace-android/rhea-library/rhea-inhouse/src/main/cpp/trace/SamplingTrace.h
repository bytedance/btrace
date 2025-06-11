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

namespace rheatrace {
namespace trace {

/**
 * This is the main entry point for RheaTrace to add our trace point,
 * such as object allocation and lock monitor etc.
 * @param env jni environment
 * @param pArray detail configuration for all kinds of trace points,
 *     a jlongArray type seems to be magic and should be optimized in the future.
 * @param enableObjectAllocationStub enable object allocation trace point
 * @param enableWakeup enable java lock wakeup trace point
 * @param shadowPause shadow pause means we don't remove the trace point when stop, just stop collecting tracing data
 * @return return true for successful initialization, return false otherwise.
 */
bool init(JNIEnv *env, jlongArray pArray, bool enableObjectAllocationStub, bool enableWakeup, bool shadowPause);

} // namespace sampling
} // namespace rheatrace