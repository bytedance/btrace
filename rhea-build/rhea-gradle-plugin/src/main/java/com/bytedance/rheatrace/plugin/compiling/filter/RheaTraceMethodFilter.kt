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

package com.bytedance.rheatrace.plugin.compiling.filter

import com.bytedance.rheatrace.plugin.internal.common.FileUtil
import com.bytedance.rheatrace.plugin.internal.common.RheaConstants
import com.bytedance.rheatrace.plugin.compiling.TraceMethod
import com.bytedance.rheatrace.plugin.internal.common.RheaLog
import com.bytedance.rheatrace.plugin.retrace.MappingCollector
import org.gradle.api.GradleException
import org.objectweb.asm.tree.MethodNode
import java.io.File
import kotlin.collections.HashSet

class RheaTraceMethodFilter(var traceFilterFilePath: String?, mappingCollector: MappingCollector) :
    DefaultTraceMethodFilter(mappingCollector) {
    companion object {
        private const val TAG = "RheaTraceMethodFilter"
    }

    /**
     * @see RheaConstants.DEFAULT_BLOCK_PACKAGES
     */
    private val defaultBlockPackages = HashSet<String>()

    private val blockPackages = HashSet<String>()

    /**
     * Set the package name of the pile file. If empty, the default is full pile
     */
    private val allowPackages = HashSet<String>()

    private val blockMethodSet = HashSet<String>()

    private val allowClassMethodsWithParamMap = HashMap<String, List<Int>>()

    init {
        parseTraceFilterFile(mappingCollector)
        checkPath()
    }

    private fun parseTraceFilterFile(processor: MappingCollector) {
        var methodKeepStr = RheaConstants.DEFAULT_BLOCK_PACKAGES
        if (traceFilterFilePath != null && File(traceFilterFilePath!!).exists()) {
            methodKeepStr += FileUtil.readFileAsString(traceFilterFilePath);
        }
        val methodKeepArray =
            methodKeepStr.trim { it <= ' ' }.replace("/", ".").split("\n").toTypedArray()

        var iterator = methodKeepArray.iterator()
        loop@ while (iterator.hasNext()) {
            var item = iterator.next()
            when {
                item.isEmpty() || item.startsWith("#") || item.startsWith("[") -> {
                    continue@loop
                }
                item.startsWith("-defaultblockpackage ") -> {
                    val packageName = item.replace("-defaultblockpackage ", "")
                    defaultBlockPackages.add(processor.proguardPackageName(packageName, packageName))
                }
                item.startsWith("-blockpackage ") -> {
                    val blockPackageString = item.replace("-blockpackage ", "")
                    var packageName = processor.proguardPackageName(blockPackageString, "")
                    if (packageName.isEmpty()) {
                        packageName = processor.proguardClassName(blockPackageString, blockPackageString)
                    }
                    blockPackages.add(packageName)
                }
                item.startsWith("-allowpackage") -> {
                    val packageName = item.replace("-allowpackage ", "")
                    allowPackages.add(processor.proguardPackageName(packageName, packageName))
                }
                item.startsWith("-allowclassmethodswithparametervalues") -> {
                    val className = item.replace("-allowclassmethodswithparametervalues ", "").replace("{", "").trim()
                    while (iterator.hasNext()) {
                        val methodItem = iterator.next().split(";").first().trim()
                        if (methodItem == "}") {
                            continue@loop
                        }
                        formatClassMethodsWithParam(className, methodItem)
                    }
                    throw GradleException("Rhea Plugin: allowclassmethodswithparametervalues wrong setting.")
                }
                item.startsWith("-blockclassmethods") -> {
                    val className = item.replace("-blockclassmethods ", "").replace("{", "").trim()
                    while (iterator.hasNext()) {
                        val methodItem = iterator.next().split(";").first().trim()
                        if (methodItem == "}") {
                            continue@loop
                        }
                        formatBlockMethod(className, methodItem)
                    }
                    throw GradleException("Rhea Plugin: blockclassmethods wrong setting.")
                }
            }
        }
        RheaLog.i(TAG, " allow packages: $allowPackages, block packages: $blockPackages")
    }

    private fun formatClassMethodsWithParam(className: String, methodName: String) {
        val params = methodName.subSequence(methodName.indexOf("(") + 1, methodName.indexOf(")")).split(",")
        val method = StringBuilder()
        method.append(className).append(".").append(methodName.subSequence(0, methodName.indexOf("(") + 1))
        val paramIndexList = ArrayList<Int>()
        for (i in params.indices) {
            var param = params[i].replace(" ", "")
            if (param.startsWith("*")) {
                paramIndexList.add(i)
                param = param.replace("*", "")
            }
            if (param.endsWith("[]")) {
                method.append("[")
                param = param.replace("[]", "")
            }
            method.append(
                when (param) {
                    "boolean" -> "Z"
                    "char" -> "C"
                    "byte" -> "B"
                    "short" -> "S"
                    "int" -> "I"
                    "float" -> "F"
                    "long" -> "J"
                    "double" -> "D"
                    else -> {
                        "L$param;"
                    }
                }
            )
        }
        method.append(")")
        allowClassMethodsWithParamMap[method.toString()] = paramIndexList
    }

    private fun formatBlockMethod(className: String, methodName: String) {
        val params = methodName.subSequence(methodName.indexOf("(") + 1, methodName.indexOf(")")).split(",")
        val method = StringBuilder()
        method.append(className).append(".").append(methodName.subSequence(0, methodName.indexOf("(") + 1))
        for (i in params.indices) {
            var param = params[i].replace(" ", "")
            if (param.endsWith("[]")) {
                method.append("[")
                param = param.replace("[]", "")
            }
            method.append(
                when (param) {
                    "boolean" -> "Z"
                    "char" -> "C"
                    "byte" -> "B"
                    "short" -> "S"
                    "int" -> "I"
                    "float" -> "F"
                    "long" -> "J"
                    "double" -> "D"
                    else -> {
                        "L$param;"
                    }
                }
            )
        }
        method.append(")")
        blockMethodSet.add(method.toString())
    }

    /**
     * Check that BlockPath is correct and must be included in an AllowPackage
     */
    private fun checkPath() {
        if (allowPackages.isEmpty()) {
            return
        }
        for (blockPath in blockPackages) {
            var isContain = false
            for (allowPackage in allowPackages) {
                if (blockPath == allowPackage) {
                    RheaLog.e(TAG, "blockpackage cannot be equal to allowpackage:%s", blockPath)
                }
                if (blockPath.startsWith(allowPackage)) {
                    isContain = true
                    break
                }
            }
            if (!isContain) {
                throw GradleException("allowpackage do not contains $blockPath")
            }
        }
    }

    override fun onClassNeedFilter(className: String): Boolean {
        val needFilter = super.onClassNeedFilter(className)
        if (!needFilter) {
            if (!isAllowPackage(className)) {
                return true
            }
            return isBlockClass(className)
        }
        return needFilter
    }

    /**
     * Determine whether it is in the path that allows piling
     */
    private fun isAllowPackage(clsName: String): Boolean {
        if (allowPackages.isEmpty()) {
            return true
        }
        var className = clsName.replace("/", ".")
        className = mappingCollector.proguardClassName(className, className)
        var needTrace = false
        for (packageName in allowPackages) {
            if (className.startsWith(packageName)) {
                needTrace = true
                break
            }
        }
        return needTrace
    }

    private fun isBlockClass(clsName: String): Boolean {
        var className = clsName.replace("/", ".")
        className = mappingCollector.proguardClassName(className, className)
        var inBlock = false
        for (packageName in blockPackages + defaultBlockPackages) {
            if (className.startsWith(packageName)) {
                inBlock = true
                break
            }
        }
        return inBlock
    }

    override fun onMethodNeedFilter(methodNode: MethodNode, traceMethod: TraceMethod): Boolean {
        val isFilter = super.onMethodNeedFilter(methodNode, traceMethod)
        if (!isFilter) {
            //TODO::more filter
            val originClassMethodName = traceMethod.getOriginMethodName(mappingCollector)
            return isBlockMethod(originClassMethodName)
        }
        return isFilter
    }


    override fun isMethodWithParamesValue(methodName: String): Boolean {
        allowClassMethodsWithParamMap.keys.forEach {
            if (methodName.startsWith(it)) {
                return true
            }
        }
        return false
    }

    override fun getMethodWithParamesValue(methodName: String): List<Int>? {
        allowClassMethodsWithParamMap.keys.forEach {
            if (methodName.startsWith(it)) {
                return allowClassMethodsWithParamMap[it]
            }
        }
        return emptyList()
    }

    override fun isBlockMethod(methodName: String): Boolean {
        blockMethodSet.forEach {
            if (methodName.startsWith(it)) {
                return true
            }
        }
        return false
    }
}