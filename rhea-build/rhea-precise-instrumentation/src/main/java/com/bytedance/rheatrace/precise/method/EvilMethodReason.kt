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
package com.bytedance.rheatrace.precise.method

/**
 * @author majun
 * @date 2022/3/10
 */
object EvilMethodReason {

    const val TYPE_NO_EVIL = 0

    const val TYPE_NATIVE_INNER = 1

    const val TYPE_LOCK = TYPE_NATIVE_INNER * 2

    const val TYPE_LOCK_INNER = TYPE_LOCK * 2

    const val TYPE_INTEREST_METHOD = TYPE_LOCK_INNER * 2

    const val TYPE_AIDL = TYPE_INTEREST_METHOD * 2

    const val TYPE_LARGE_METHOD = TYPE_AIDL * 2

    const val TYPE_LOOP_METHOD = TYPE_LARGE_METHOD * 2

    const val TYPE_TRACE_METHOD = TYPE_LOOP_METHOD * 2

    const val TYPE_ANNOTATION_METHOD = TYPE_TRACE_METHOD * 2

    const val TYPE_TRACE_CLASS = TYPE_ANNOTATION_METHOD * 2
}