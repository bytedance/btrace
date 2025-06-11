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

import com.bytedance.rheatrace.common.utils.FileUtil
import com.bytedance.rheatrace.precise.PreciseInstrumentationContext
import org.objectweb.asm.tree.ClassNode
import java.io.BufferedReader
import java.io.File
import java.io.InputStreamReader

/**
 * @author majun
 * @date 2022/3/10
 */
class MethodFilter(private val preciseInstrContext: PreciseInstrumentationContext) {
    val evilRootMethods = hashSetOf<String>()
    private val traceMethodSet: HashSet<FilterTraceMethodData> = HashSet()
    private val traceClassSet: HashSet<String> = HashSet()
    private val traceAnnotationMethodSet: HashSet<String> = HashSet()

    companion object {
        var isSynchronizeEnable = false
        var isNativeEnable = false
        var isLoopEnable = false
        var isAIDLEnable = false
        var defaultPreciseInstrEnable = true
        var largeMethodSize = 0
    }

    fun parseTraceFilterFile() {
        var configStr = ""
        if (!preciseInstrContext.extension.traceFilter.isNullOrEmpty()
            && File(preciseInstrContext.extension.traceFilter!!).exists()
        ) {
            configStr += "\n" + FileUtil.readFileAsString(preciseInstrContext.extension.traceFilter)
        }
        val configArray = configStr.trim { it <= ' ' }.split("\n").toTypedArray()
        val iterator = configArray.iterator()
        loop@ while (iterator.hasNext()) {
            val keepItem = iterator.next()
            when {
                keepItem.isEmpty() || keepItem.startsWith("#") || keepItem.startsWith("[") -> {
                    continue@loop
                }
                keepItem.startsWith("-tracesynchronize") -> {
                    isSynchronizeEnable = true
                }
                keepItem.startsWith("-tracenative") -> {
                    isNativeEnable = true
                }
                keepItem.startsWith("-traceloop") -> {
                    isLoopEnable = true
                }
                keepItem.startsWith("-traceaidl") -> {
                    isAIDLEnable = true
                }
                keepItem.startsWith("-disabledefaultpreciseinstrumentation") -> {
                    defaultPreciseInstrEnable = false
                }
                keepItem.startsWith("-tracelargemethod ") -> {
                    largeMethodSize = keepItem.replace("-tracelargemethod ", "").trim().toInt()
                }
                keepItem.startsWith("-traceevilmethodcallee ") -> {
                    val classInfo = keepItem.replace("-traceevilmethodcallee ", "").replace("{", "").trim()
                    val classInfoItems = classInfo.split(" ")
                    var className: String
                    if (classInfoItems.size > 2) {
                        throw Exception("blockMethodCall error：more info")
                    } else {
                        className = classInfoItems[0]
                    }
                    while (iterator.hasNext()) {
                        val methodInfoItems = iterator.next().trim().split(" ")
                        if (methodInfoItems[0] == "}") {
                            continue@loop
                        }
                        val obfuscatedMethodInfo = preciseInstrContext.mappingCollector!!.obfuscatedInfoWithoutDesc(className.replace("/", "."), methodInfoItems[0])
                        evilRootMethods.add(obfuscatedMethodInfo.originalClassName.replace(".", "/") + "#" + obfuscatedMethodInfo.originalName)
                    }
                }
                keepItem.startsWith("-traceclassmethods ") -> {
                    val classInfo = keepItem.replace("-traceclassmethods ", "").replace("{", "").trim()
                    val classInfoItems = classInfo.split(" ")
                    var className = ""
                    var superClassName = ""
                    if (classInfoItems.size == 3) {
                        if (classInfoItems[1] != "implements") {
                            throw Exception("traceclassmethods error：implements cannot parse correctly")
                        }
                        superClassName = classInfoItems[2]
                    } else {
                        className = classInfoItems[0]
                    }
                    // handle methods
                    while (iterator.hasNext()) {
                        val methodInfoItems = iterator.next().trim().split(" ")
                        if (methodInfoItems[0] == "}") {
                            continue@loop
                        }
                        val traceMethod = FilterTraceMethodData()
                        traceMethod.method = methodInfoItems[0]
                        traceMethod.className = className
                        traceMethod.superClassName = superClassName
                        traceMethodSet.add(traceMethod)
                    }
                }
                keepItem.startsWith("-tracemethodannotation ") -> {
                    val traceMethodAnnotation = keepItem.replace("-tracemethodannotation ", "")
                    traceAnnotationMethodSet.add(traceMethodAnnotation)
                }
                keepItem.startsWith("-traceclass ") -> {
                    val traceClass = keepItem.replace("-traceclass ", "")
                    traceClassSet.add(traceClass)
                }
            }
        }
        addDefaultThirdPartMethod()
    }

    private fun addDefaultThirdPartMethod() {
        if (!defaultPreciseInstrEnable) {
            return
        }
        val inputStream = MethodFilter::class.java.classLoader.getResourceAsStream("third_part_evil_method.txt") ?: return
        BufferedReader(InputStreamReader(inputStream, Charsets.UTF_8)).forEachLine {
            val traceMethod = FilterTraceMethodData()
            traceMethod.method = it.split("#")[1]
            traceMethod.className = it.split("#")[0].replace("/", ".")
            traceMethodSet.add(traceMethod)
        }
    }


    fun isTraceMethod(originFullMethod: String, classNode: ClassNode): Boolean {
        traceMethodSet.forEach {
//          "\\.[A-Za-z]+[init][A-Za-z0-9]+\\("
            val methodRegex = "." + it.method + "("
            if (it.className.isEmpty()) {
                val proguardSuperClassName = classNode.superName.replace("/", ".")
                val originSuperclassName = preciseInstrContext.mappingCollector!!.originalClassName(proguardSuperClassName, proguardSuperClassName)
                if (originSuperclassName == it.superClassName && originFullMethod.contains(methodRegex)) {
                    return true
                }
                classNode.interfaces.forEach { interfaceName ->
                    val proguardInterfaceName = interfaceName.replace("/", ".")
                    val originInterfaceName = preciseInstrContext.mappingCollector!!.originalClassName(proguardInterfaceName, proguardInterfaceName)
                    if (originInterfaceName == it.superClassName
                        && originFullMethod.contains(methodRegex)
                    ) {
                        return true
                    }
                }
            } else {
                if (it.className == "*" && originFullMethod.contains(methodRegex)) {
                    return true
                }
                if (originFullMethod.startsWith(it.className + ".") && originFullMethod.contains(methodRegex)) {
                    return true
                }
            }
        }
        return false
    }

    fun isTraceAnnotation(originAnnotation: String): Boolean {
        return traceAnnotationMethodSet.contains(originAnnotation)
    }

    fun isTraceClass(originClassName: String): Boolean {
        return traceClassSet.contains(originClassName)
    }

    data class FilterTraceMethodData(
        var className: String = "",
        var superClassName: String = "",
        var method: String = "",
        var desc: String = ""
    )
}