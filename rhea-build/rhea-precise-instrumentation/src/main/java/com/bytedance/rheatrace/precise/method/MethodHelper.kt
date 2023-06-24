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

import com.bytedance.rheatrace.common.utils.ClassToJavaFormat
import com.bytedance.rheatrace.precise.PreciseInstrumentationContext
import org.objectweb.asm.tree.ClassNode
import java.io.BufferedReader
import java.io.InputStreamReader
import java.util.LinkedList

/**
 * @author majun
 * @date 2022/3/10
 */
class MethodHelper(private val preciseInstrContext: PreciseInstrumentationContext) {

    private var methodFilter: MethodFilter = MethodFilter(preciseInstrContext)

    private val evilRootMethods = HashMap<String, MutableList<MethodMatcher>>()

    init {
        methodFilter.parseTraceFilterFile()
        addDefaultEvilRootMethod()
        methodFilter.evilRootMethods.forEach {
            addEvilRootMethod(it)
        }
    }

    /**
     * Time-consuming method of pre-embedding
     */
    private fun addDefaultEvilRootMethod() {
        if (!MethodFilter.defaultPreciseInstrEnable) {
            return
        }
        val inputStream = MethodHelper::class.java.classLoader.getResourceAsStream("framework_evil_method.txt") ?: return
        BufferedReader(InputStreamReader(inputStream, Charsets.UTF_8)).forEachLine {
            val obfuscatedMethod = preciseInstrContext.mappingCollector!!.obfuscatedInfoWithoutDesc(it.split("#")[0].replace("/", "."), it.split("#")[1])
            val obfuscatedClassName = obfuscatedMethod.originalClassName.replace(".", "/")
            val obfuscatedMethodName = obfuscatedMethod.originalName.replace(".", "/")
            addEvilRootMethod("$obfuscatedClassName#$obfuscatedMethodName")
        }
    }

    private fun addEvilRootMethod(method: String) {
        val key = method.split("#")[0]
        if (!evilRootMethods.containsKey(key)) {
            evilRootMethods[key] = LinkedList()
        }
        evilRootMethods[key]!!.add(MethodMatcher(method))
    }

    fun matchEvilRootMethod(owner: String, name: String, desc: String): Int {
        val matcherList = LinkedList<MethodMatcher>()
        if (evilRootMethods.size != 0 && evilRootMethods[owner] != null) {
            matcherList.addAll(evilRootMethods[owner]!!)
        }
        for (matcher in matcherList) {
            if (matcher.match(owner, name, desc)) {
                return EvilMethodReason.TYPE_INTEREST_METHOD
            }
        }
        val evilRootMethod = preciseInstrContext.evilRootMethodDetector.marchEvilRootMethod(owner, name, desc)
        if (evilRootMethod != null) {
            return evilRootMethod.methodType
        }
        return EvilMethodReason.TYPE_NO_EVIL
    }

    fun getOriginMethodName(className: String, methodName: String, methodDesc: String): String {
        val methodInfo = preciseInstrContext.mappingCollector!!.originalMethodInfo(className.replace("/", "."), methodName, methodDesc)
        return methodInfo.getFullMethodName()
    }

    fun isTraceMethod(originFullMethod: String, classNode: ClassNode): Boolean {
        return methodFilter.isTraceMethod(originFullMethod, classNode)
    }

    fun isTraceAnnotation(desc: String): Boolean {
        val proguardClassName = ClassToJavaFormat.parseDesc(desc, "")
        val originAnnotationClassName = preciseInstrContext.mappingCollector!!.originalClassName(proguardClassName, proguardClassName)
        return methodFilter.isTraceAnnotation(originAnnotationClassName)
    }

    fun isTraceClass(className: String): Boolean {
        val originClassName = preciseInstrContext.mappingCollector!!.originalClassName(className, className)
        return methodFilter.isTraceClass(originClassName)
    }

    fun release() {
        evilRootMethods.clear()
    }
}