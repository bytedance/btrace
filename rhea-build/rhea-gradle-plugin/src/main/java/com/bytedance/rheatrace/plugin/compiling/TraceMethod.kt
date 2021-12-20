/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//------ modify by RheaTrace ------

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

package com.bytedance.rheatrace.plugin.compiling

import com.bytedance.rheatrace.plugin.retrace.MappingCollector
import org.apache.http.util.TextUtils
import org.objectweb.asm.Opcodes
import org.objectweb.asm.Type

/**
 * Created by caichongyang on 2017/6/3.
 */
open class TraceMethod {

    var id: Int = 0

    var accessFlag: Int = 0

    var className: String = ""
        set(value) {
            field = value.replace("/", ".")
        }

    var methodName: String = ""

    var desc: String? = null
        set(value) {
            field = value?.apply {
                value.replace("/", ".")
            }
        }

    fun getFullMethodName(): String {
        return if (desc == null || isNativeMethod) {
            "$className.$methodName"
        } else {
            "$className.$methodName.$desc"
        }
    }

    fun getOriginMethodName(processor: MappingCollector?): String {
        if (null == processor) {
            return ""
        }
        val originClassName = processor.originalClassName(className, className)
        val methodInfo = processor.originalMethodInfo(className, methodName, desc!!)
        return "$originClassName.${methodInfo.originalName}${methodInfo.desc.replace("/", ".")}"
    }

    /**
     * proguard -> original
     *
     * @param processor
     */
    fun revert(processor: MappingCollector?) {
        if (null == processor) {
            return
        }
        val methodInfo = processor.originalMethodInfo(className, methodName, desc!!)
        methodName = methodInfo.originalName
        desc = methodInfo.desc
        className = processor.originalClassName(className, className)
    }

    /**
     * original -> proguard
     *
     * @param processor
     */
    fun proguard(processor: MappingCollector?) {
        if (null == processor) {
            return
        }
        val methodInfo = processor.obfuscatedMethodInfo(className, methodName, desc!!)
        methodName = methodInfo.originalName
        desc = methodInfo.desc
        className = processor.proguardClassName(className, className)
    }

    fun getReturnType(): String? {
        return if (TextUtils.isEmpty(desc)) {
            null
        } else Type.getReturnType(desc).toString()
    }

    override fun toString(): String {
        return if (desc == null || isNativeMethod) {
            "$id,$accessFlag,$className $methodName"
        } else {
            "$id,$accessFlag,$className $methodName $desc"
        }
    }

    fun toIgnoreString(): String {
        return if (desc == null || isNativeMethod) {
            "$className $methodName"
        } else {
            "$className $methodName $desc"
        }
    }

    private val isNativeMethod: Boolean
        get() = accessFlag and Opcodes.ACC_NATIVE != 0

    override fun equals(other: Any?): Boolean {
        return if (other is TraceMethod) {
            other.getFullMethodName() == getFullMethodName()
        } else {
            false
        }
    }

    override fun hashCode(): Int {
        return super.hashCode()
    }

    companion object {

        @JvmStatic
        fun create(
            id: Int,
            accessFlag: Int,
            className: String,
            methodName: String,
            desc: String?
        ): TraceMethod {
            val traceMethod =
                TraceMethod()
            traceMethod.id = id
            traceMethod.accessFlag = accessFlag
            traceMethod.className = className
            traceMethod.methodName = methodName
            traceMethod.desc = desc
            return traceMethod
        }
    }
}
