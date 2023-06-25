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

import org.objectweb.asm.Type

/**
 * Created by caichongyang on 2017/8/3.
 */
class MappingCollector :
    MappingProcessor {

    companion object {

        private const val TAG = "MappingCollector"

        private const val DEFAULT_CAPACITY = 2000

    }

    private var mObfuscatedRawClassMap: HashMap<String, String> = HashMap(DEFAULT_CAPACITY)
    private var mRawObfuscatedClassMap: HashMap<String, String> = HashMap(DEFAULT_CAPACITY)
    private var mRawObfuscatedPackageMap: HashMap<String, String> = HashMap(DEFAULT_CAPACITY)
    private val mObfuscatedClassMethodMap: MutableMap<String, MutableMap<String, MutableSet<MethodInfo>>> =
        HashMap()
    private val mOriginalClassMethodMap: MutableMap<String, MutableMap<String, MutableSet<MethodInfo>>> =
        HashMap()

    override fun processClassMapping(className: String, newClassName: String): Boolean {
        mObfuscatedRawClassMap[newClassName] = className
        mRawObfuscatedClassMap[className] = newClassName
        if (className.contains(".")) {
            mRawObfuscatedPackageMap[className.substring(0, className.lastIndexOf('.'))] =
                newClassName.substring(0, newClassName.lastIndexOf('.'))
        } else {
            mRawObfuscatedPackageMap[""] = ""
        }
        return true
    }

    override fun processMethodMapping(
        className: String,
        methodReturnType: String,
        methodName: String,
        methodArguments: String,
        newClassName: String?,
        newMethodName: String
    ) {
        val newClassNameTemp = mRawObfuscatedClassMap[className] ?: return
        var methodMap = mObfuscatedClassMethodMap[newClassNameTemp]
        if (methodMap == null) {
            methodMap = HashMap()
            mObfuscatedClassMethodMap[newClassNameTemp] = methodMap
        }
        var methodSet = methodMap[newMethodName]
        if (methodSet == null) {
            methodSet = LinkedHashSet()
            methodMap[newMethodName] = methodSet
        }
        methodSet.add(
            MethodInfo(
                className,
                methodReturnType,
                methodName,
                methodArguments
            )
        )
        var methodMap2 = mOriginalClassMethodMap[className]
        if (methodMap2 == null) {
            methodMap2 = HashMap()
            mOriginalClassMethodMap[className] = methodMap2
        }
        var methodSet2 = methodMap2[methodName]
        if (methodSet2 == null) {
            methodSet2 = LinkedHashSet()
            methodMap2[methodName] = methodSet2
        }
        methodSet2.add(
            MethodInfo(
                newClassNameTemp,
                methodReturnType,
                newMethodName,
                methodArguments
            )
        )
    }

    fun originalClassName(proguardClassName: String, defaultClassName: String): String {
        val tempProguardClassName = proguardClassName.replace("/", ".")
        return if (mObfuscatedRawClassMap.containsKey(tempProguardClassName)) {
            mObfuscatedRawClassMap[tempProguardClassName]!!
        } else {
            defaultClassName
        }
    }

    fun proguardClassName(originalClassName: String, defaultClassName: String): String {
        return if (mRawObfuscatedClassMap.containsKey(originalClassName)) {
            mRawObfuscatedClassMap[originalClassName]!!
        } else {
            defaultClassName
        }
    }

    fun proguardPackageName(originalPackage: String, defaultPackage: String): String {
        return if (mRawObfuscatedPackageMap.containsKey(originalPackage)) {
            mRawObfuscatedPackageMap[originalPackage]!!
        } else {
            defaultPackage
        }
    }

    /**
     * get original method info
     *
     * @param obfuscatedClassName
     * @param obfuscatedMethodName
     * @param obfuscatedMethodDesc
     * @return
     */
    fun originalMethodInfo(
        obfuscatedClassName: String,
        obfuscatedMethodName: String,
        obfuscatedMethodDesc: String
    ): MethodInfo {
        val descInfo = parseMethodDesc(obfuscatedMethodDesc, false)

        // obfuscated name -> original method names.
        val methodMap: Map<String, MutableSet<MethodInfo>>? =
            mObfuscatedClassMethodMap[obfuscatedClassName]
        if (methodMap != null) {
            val methodSet: Set<MethodInfo>? = methodMap[obfuscatedMethodName]
            if (methodSet != null) {
                // Find all matching methods.
                val methodInfoIterator = methodSet.iterator()
                while (methodInfoIterator.hasNext()) {
                    val methodInfo = methodInfoIterator.next()
                    if (methodInfo.matches(descInfo.arguments)) {
                        val newMethodInfo =
                            MethodInfo(
                                methodInfo
                            )
                        newMethodInfo.desc = descInfo.desc
                        return newMethodInfo
                    }
                }
            }
        }
        val defaultMethodInfo: MethodInfo =
            MethodInfo.deFault()
        defaultMethodInfo.desc = descInfo.desc
        defaultMethodInfo.originalName = obfuscatedMethodName
        defaultMethodInfo.originalClassName = obfuscatedClassName
        defaultMethodInfo.originalArguments = descInfo.arguments
        defaultMethodInfo.originalType = descInfo.returnType
        return defaultMethodInfo
    }

    /**
     * get original method info
     *
     * @param obfuscatedClassName
     * @param obfuscatedMethodName
     * @return
     */
    fun originalMethodInfoWithoutDesc(
        obfuscatedClassName: String,
        obfuscatedMethodName: String
    ): MethodInfo {
        // obfuscated name -> original method names.
        val methodMap: Map<String, MutableSet<MethodInfo>>? =
            mObfuscatedClassMethodMap[obfuscatedClassName]
        if (methodMap != null) {
            val methodSet: Set<MethodInfo>? = methodMap[obfuscatedMethodName]
            if (methodSet != null) {
                // Find all matching methods.
                val methodInfoIterator = methodSet.iterator()
                while (methodInfoIterator.hasNext()) {
                    val methodInfo = methodInfoIterator.next()
                    val newMethodInfo: MethodInfo = MethodInfo.deFault()
                    newMethodInfo.originalName = methodInfo.originalName
                    newMethodInfo.originalClassName = methodInfo.originalClassName
                    return newMethodInfo
                }
            }
        }
        val defaultMethodInfo: MethodInfo =
            MethodInfo.deFault()
        defaultMethodInfo.originalName = obfuscatedMethodName
        defaultMethodInfo.originalClassName = obfuscatedClassName
        return defaultMethodInfo
    }

    /**
     * get obfuscated method info
     *
     * @param className
     * @param originalMethodName
     * @return
     */
    fun obfuscatedInfoWithoutDesc(
        className: String,
        originalMethodName: String
    ): MethodInfo {
        // Class name -> obfuscated method names.
        val originalClassName = className.replace("/", ".")
        val methodMap: Map<String, Set<MethodInfo>>? = mOriginalClassMethodMap[originalClassName]
        if (methodMap != null) {
            val methodSet = methodMap[originalMethodName]
            if (null != methodSet) {
                // Find all matching methods.
                val methodInfoIterator = methodSet.iterator()
                while (methodInfoIterator.hasNext()) {
                    val methodInfo = methodInfoIterator.next()
                    val newMethodInfo: MethodInfo = MethodInfo.deFault()
                    newMethodInfo.originalName = methodInfo.originalName
                    newMethodInfo.originalClassName = methodInfo.originalClassName
                    return newMethodInfo
                }
            }
        }
        val defaultMethodInfo: MethodInfo =
            MethodInfo.deFault()
        defaultMethodInfo.originalName = originalMethodName
        defaultMethodInfo.originalClassName = originalClassName
        return defaultMethodInfo
    }

    /**
     * get obfuscated method info
     *
     * @param originalClassName
     * @param originalMethodName
     * @param originalMethodDesc
     * @return
     */
    fun obfuscatedMethodInfo(
        originalClassName: String,
        originalMethodName: String,
        originalMethodDesc: String
    ): MethodInfo {
        val descInfo = parseMethodDesc(originalMethodDesc, true)

        // Class name -> obfuscated method names.
        val methodMap: Map<String, Set<MethodInfo>>? = mOriginalClassMethodMap[originalClassName]
        if (methodMap != null) {
            val methodSet = methodMap[originalMethodName]
            if (null != methodSet) {
                // Find all matching methods.
                val methodInfoIterator = methodSet.iterator()
                while (methodInfoIterator.hasNext()) {
                    val methodInfo = methodInfoIterator.next()
                    val newMethodInfo =
                        MethodInfo(
                            methodInfo
                        )
                    obfuscatedMethodInfo(newMethodInfo)
                    if (newMethodInfo.matches(descInfo.arguments)) {
                        newMethodInfo.desc = descInfo.desc
                        return newMethodInfo
                    }
                }
            }
        }
        val defaultMethodInfo: MethodInfo =
            MethodInfo.deFault()
        defaultMethodInfo.desc = descInfo.desc
        defaultMethodInfo.originalName = originalMethodName
        defaultMethodInfo.originalClassName = originalClassName
        defaultMethodInfo.originalArguments = descInfo.arguments
        defaultMethodInfo.originalType = descInfo.returnType
        return defaultMethodInfo
    }

    private fun obfuscatedMethodInfo(methodInfo: MethodInfo) {
        val methodArguments = methodInfo.originalArguments
        val args = methodArguments.split(",").toTypedArray()
        val stringBuffer = StringBuffer()
        for (str in args) {
            val key = str.replace("[", "").replace("]", "")
            if (mRawObfuscatedClassMap.containsKey(key)) {
                stringBuffer.append(str.replace(key, mRawObfuscatedClassMap[key]!!))
            } else {
                stringBuffer.append(str)
            }
            stringBuffer.append(',')
        }
        if (stringBuffer.isNotEmpty()) {
            stringBuffer.deleteCharAt(stringBuffer.length - 1)
        }
        var methodReturnType = methodInfo.originalType
        val key = methodReturnType.replace("[", "").replace("]", "")
        if (mRawObfuscatedClassMap.containsKey(key)) {
            methodReturnType = methodReturnType.replace(key, mRawObfuscatedClassMap[key]!!)
        }
        methodInfo.originalArguments = stringBuffer.toString()
        methodInfo.originalType = methodReturnType
    }

    /**
     * parse method desc
     *
     * @param desc
     * @param isRawToObfuscated
     * @return
     */
    private fun parseMethodDesc(desc: String, isRawToObfuscated: Boolean): DescInfo {
        val descInfo =
            DescInfo()
        val argsObj = Type.getArgumentTypes(desc)
        val argumentsBuffer = StringBuffer()
        val descBuffer = StringBuffer()
        descBuffer.append('(')
        for (type in argsObj) {
            val key = type.className.replace("[", "").replace("]", "")
            if (isRawToObfuscated) {
                if (mRawObfuscatedClassMap.containsKey(key)) {
                    argumentsBuffer.append(
                        type.className.replace(
                            key,
                            mRawObfuscatedClassMap[key]!!
                        )
                    )
                    descBuffer.append(type.toString().replace(key, mRawObfuscatedClassMap[key]!!))
                } else {
                    argumentsBuffer.append(type.className)
                    descBuffer.append(type.toString())
                }
            } else {
                if (mObfuscatedRawClassMap.containsKey(key)) {
                    argumentsBuffer.append(
                        type.className.replace(
                            key,
                            mObfuscatedRawClassMap[key]!!
                        )
                    )
                    descBuffer.append(type.toString().replace(key, mObfuscatedRawClassMap[key]!!))
                } else {
                    argumentsBuffer.append(type.className)
                    descBuffer.append(type.toString())
                }
            }
            argumentsBuffer.append(',')
        }
        descBuffer.append(')')
        val returnObj: Type = try {
            Type.getReturnType(desc)
        } catch (e: ArrayIndexOutOfBoundsException) {
            Type.getReturnType("$desc;")
        }
        if (isRawToObfuscated) {
            val key = returnObj.className.replace("[", "").replace("]", "")
            if (mRawObfuscatedClassMap.containsKey(key)) {
                descInfo.returnType =
                    returnObj.className.replace(key, mRawObfuscatedClassMap[key]!!)
                descBuffer.append(returnObj.toString().replace(key, mRawObfuscatedClassMap[key]!!))
            } else {
                descInfo.returnType = returnObj.className
                descBuffer.append(returnObj.toString())
            }
        } else {
            val key = returnObj.className.replace("[", "").replace("]", "")
            if (mObfuscatedRawClassMap.containsKey(key)) {
                descInfo.returnType =
                    returnObj.className.replace(key, mObfuscatedRawClassMap[key]!!)
                descBuffer.append(returnObj.toString().replace(key, mObfuscatedRawClassMap[key]!!))
            } else {
                descInfo.returnType = returnObj.className
                descBuffer.append(returnObj.toString())
            }
        }

        // delete last ,
        if (argumentsBuffer.isNotEmpty()) {
            argumentsBuffer.deleteCharAt(argumentsBuffer.length - 1)
        }
        descInfo.arguments = argumentsBuffer.toString()
        descInfo.desc = descBuffer.toString()
        return descInfo
    }

    fun release() {
        mObfuscatedRawClassMap.clear()
        mRawObfuscatedClassMap.clear()
        mRawObfuscatedPackageMap.clear()
        mObfuscatedClassMethodMap.clear()
        mOriginalClassMethodMap.clear()
    }
}