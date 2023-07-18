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

// modify by RheaTrace

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


package com.bytedance.rheatrace.common.utils

import java.lang.reflect.Field
import java.lang.reflect.Method

object ReflectUtil {

    @Throws(NoSuchFieldException::class, ClassNotFoundException::class)
    fun getDeclaredFieldRecursive(clazz: Any, fieldName: String): Field {
        val realClazz: Class<*>? = when (clazz) {
            is String -> {
                Class.forName(clazz)
            }
            is Class<*> -> {
                clazz
            }
            else -> {
                throw IllegalArgumentException("Illegal clazz type: " + clazz.javaClass)
            }
        }
        var currClazz = realClazz
        while (true) {
            try {
                val field = currClazz!!.getDeclaredField(fieldName)
                field.isAccessible = true
                return field
            } catch (e: NoSuchFieldException) {
                if (currClazz == Any::class.java) {
                    throw e
                }
                currClazz = currClazz!!.superclass
            }
        }
    }

    @Throws(NoSuchMethodException::class, ClassNotFoundException::class)
    fun getDeclaredMethodRecursive(
        clazz: Any,
        methodName: String,
        vararg argTypes: Class<*>?
    ): Method {
        val realClazz: Class<*>? = when (clazz) {
            is String -> {
                Class.forName(clazz)
            }
            is Class<*> -> {
                clazz
            }
            else -> {
                throw IllegalArgumentException("Illegal clazz type: " + clazz.javaClass)
            }
        }
        var currClazz = realClazz
        while (true) {
            try {
                val method = currClazz!!.getDeclaredMethod(methodName, *argTypes)
                method.isAccessible = true
                return method
            } catch (e: NoSuchMethodException) {
                if (currClazz == Any::class.java) {
                    throw e
                }
                currClazz = currClazz!!.superclass
            }
        }
    }
}

