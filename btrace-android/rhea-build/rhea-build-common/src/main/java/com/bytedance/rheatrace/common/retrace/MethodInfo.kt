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
package com.bytedance.rheatrace.common.retrace

/**
 * Created by caichongyang on 2017/6/3.
 */
class MethodInfo {
    var originalClassName: String
    var originalType: String
    var originalArguments: String
    var originalName: String
    var desc: String

    constructor(
        originalClassName: String,
        originalType: String,
        originalName: String,
        originalArguments: String
    ) {
        this.originalType = originalType
        this.originalArguments = originalArguments
        this.originalClassName = originalClassName
        this.originalName = originalName
        this.desc = ""
    }

    constructor(methodInfo: MethodInfo) {
        originalType = methodInfo.originalType
        originalArguments = methodInfo.originalArguments
        originalClassName = methodInfo.originalClassName
        originalName = methodInfo.originalName
        desc = methodInfo.desc
    }

    fun matches(originalArguments: String?): Boolean {
        return (originalArguments == null || originalArguments == this.originalArguments)
    }

    fun getFullMethodName(): String {
        return "$originalClassName.${originalName}(${originalArguments})$originalType"
    }

    override fun toString(): String {
        return "MethodInfo(originalClassName='$originalClassName', originalType='$originalType', originalArguments='$originalArguments', originalName='$originalName', desc='$desc')"
    }

    companion object {
        fun deFault(): MethodInfo {
            return MethodInfo(
                "",
                "",
                "",
                ""
            )
        }
    }
}