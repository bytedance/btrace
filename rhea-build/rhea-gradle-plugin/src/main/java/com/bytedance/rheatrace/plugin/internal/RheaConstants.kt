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

package com.bytedance.rheatrace.plugin.internal

object RheaConstants {

    private const val METHOD_ID_MAX = 0xFFFFFF

    const val METHOD_ID_DISPATCH = METHOD_ID_MAX - 1

    const val RHEA_TRACE_ROOT = "rhea-trace"

    const val TRACE_CLASS_DIR = "trace-class"

    const val DEFAULT_BLOCK_PACKAGES = ("[package]\n"
            + "-defaultblockpackage android/\n"
            + "-defaultblockpackage com/bytedance/rheatrace/\n")

    const val METHOD_attachBaseContext = "attachBaseContext"
    const val DESC_attachBaseContext = "(Landroid/content/Context;)V"

    const val CLASS_ApplicationLike = "com/bytedance/rheatrace/core/TraceApplicationLike"
    const val METHOD_onCreate = "onCreate"
    const val DESC_onCreate = "(Landroid/content/Context;)V"

    const val CLASS_TraceStub = "com/bytedance/rheatrace/core/TraceStub"
    const val METHOD_i = "i"
    const val METHOD_o = "o"
    const val DESC_i = "(I)J"
    const val DESC_o = "(IJ)V"
    const val DESC_Custom_TraceStub = "(Ljava/lang/String;[Ljava/lang/Object;)V"

    const val ANEWARRAY_TYPE = "java/lang/Object"

    val UN_TRACE_CLASS = arrayOf("R.class", "R$", "Manifest", "BuildConfig")
}

