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

import com.bytedance.rheatrace.common.retrace.MappingCollector
import com.bytedance.rheatrace.common.utils.FileUtil
import com.bytedance.rheatrace.common.utils.RheaLog
import com.bytedance.rheatrace.plugin.compiling.TraceMethod
import com.bytedance.rheatrace.plugin.internal.RheaConstants.DEFAULT_BLOCK_PACKAGES
import org.gradle.api.GradleException
import org.objectweb.asm.tree.ClassNode
import org.objectweb.asm.tree.MethodNode
import java.io.File

class RheaTraceMethodFilter(var traceFilterFilePath: String?, mappingCollector: MappingCollector) :
    DefaultTraceMethodFilter(mappingCollector) {
    companion object {
        private const val TAG = "RheaTraceMethodFilter"
    }

    /**
     * Classes under the default package name are not piled
     * @see DEFAULT_BLOCK_PACKAGES
     */
    private val defaultBlockPackages = HashSet<String>()

    /**
     * Classes under the package name are not piled
     */
    private val blockPackages = HashSet<String>()

    /**
     * Set the package name of the pile file. If empty, the default is full piled
     */
    private val allowPackages = HashSet<String>()

    /**
     * Method will not be piled
     */
    private val blockMethodSet = HashSet<String>()

    /**
     * Allow the method to pile the parameter information
     */
    private val allowMethodsWithParamMap = HashMap<String, List<Int>>()

    /**
     * Classes that need to be traced
     */
    private val traceClassSet = HashSet<String>()

    /**
     * Methods that need to be traced
     */
    private val traceMethodSet = HashSet<FilterTraceMethodData>()

    /**
     * Whether to enable Precise Instrumentation
     */
    var enablePreciseInstrumentation = false

    init {
        updateConfig()
    }

    fun updateConfig() {
        parseTraceFilterFile(mappingCollector)
        checkPath()
    }

    private fun parseTraceFilterFile(processor: MappingCollector) {
        var methodKeepStr = DEFAULT_BLOCK_PACKAGES
        if (traceFilterFilePath != null && File(traceFilterFilePath!!).exists()) {
            methodKeepStr += FileUtil.readFileAsString(traceFilterFilePath)
        }
        val methodKeepArray =
            methodKeepStr.trim { it <= ' ' }.replace("/", ".").split("\n").toTypedArray()
        val iterator = methodKeepArray.iterator()
        loop@ while (iterator.hasNext()) {
            val item = iterator.next()
            when {
                item.isEmpty() || item.startsWith("#") || item.startsWith("[") -> {
                    continue@loop
                }
                item.startsWith("-defaultblockpackage ") -> {
                    val packageName = item.replace("-defaultblockpackage ", "")
                    defaultBlockPackages.add(packageName)
                }
                item.startsWith("-blockpackage ") -> {
                    val blockPackageString = item.replace("-blockpackage ", "")
                    blockPackages.add(blockPackageString)
                }
                item.startsWith("-allowpackage ") -> {
                    val packageName = item.replace("-allowpackage ", "")
                    allowPackages.add(packageName)
                }
                item.startsWith("-blockclassmethods ") -> {
                    val className = item.replace("-blockclassmethods ", "").replace("{", "").trim()
                    while (iterator.hasNext()) {
                        val methodItem = iterator.next().split(";").first().replace(" ", "")
                        if (methodItem == "}") {
                            continue@loop
                        }
                        blockMethodSet.add("$className.$methodItem")
                    }
                }
                item.startsWith("-allowclassmethodswithparametervalues ") -> {
                    val className = item.replace("-allowclassmethodswithparametervalues ", "").replace("{", "").trim()
                    while (iterator.hasNext()) {
                        val methodItem = iterator.next().split(";").first().replace(" ", "")
                        if (methodItem == "}") {
                            continue@loop
                        }
                        formatClassMethodsWithParam(className, methodItem)
                    }
                }
                item.startsWith("-traceclass ") -> {
                    val traceClass = item.replace("-traceclass ", "")
                    traceClassSet.add(traceClass)
                }
                item.startsWith("-traceclassmethods ") -> {
                    val classInfoItems = item.replace("-traceclassmethods ", "").replace("{", "").trim().split(" ")
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
                item.startsWith("-enablepreciseinstrumentation") -> {
                    enablePreciseInstrumentation = true
                }
            }
        }
        RheaLog.i(TAG, " allow packages: $allowPackages, block packages: $blockPackages")
    }

    private fun formatClassMethodsWithParam(className: String, methodName: String) {
        val params = methodName.subSequence(methodName.indexOf("(") + 1, methodName.indexOf(")")).split(",")
        val paramIndexList = ArrayList<Int>()
        for (i in params.indices) {
            if (params[i].startsWith("*")) {
                paramIndexList.add(i)
            }
        }
        val fullMethodName = "$className.$methodName".replace("*", "")
        allowMethodsWithParamMap[fullMethodName] = paramIndexList
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

    override fun needFilter(methodNode: MethodNode, traceMethod: TraceMethod, classNode: ClassNode): Boolean {
        val originFullMethod: String = traceMethod.getOriginMethodName(mappingCollector)
        if (!isAllowPackage(originFullMethod)) {
            return true
        }
        if (isBlockClass(originFullMethod)) {
            return true
        }
        if (isTraceClass(originFullMethod)) {
            return false
        }
        if (isTraceMethod(originFullMethod, classNode)) {
            return false
        }
        if (enablePreciseInstrumentation) {
            return true
        }
        return onClassNeedFilter(originFullMethod) || onMethodNeedFilter(methodNode, traceMethod, originFullMethod)
    }

    private fun isAllowPackage(originFullMethod: String): Boolean {
        if (allowPackages.isEmpty()) {
            return true
        }
        var needTrace = false
        for (packageName in allowPackages) {
            if (originFullMethod.startsWith(packageName)) {
                needTrace = true
                break
            }
        }
        return needTrace
    }

    private fun isBlockClass(originFullMethod: String): Boolean {
        val methodName = originFullMethod.replace("/", ".")
        var inBlock = false
        for (packageName in blockPackages + defaultBlockPackages) {
            if (methodName.startsWith(packageName)) {
                inBlock = true
                break
            }
        }
        return inBlock
    }

    private fun isTraceClass(originFullMethod: String): Boolean {
        var needTrace = false
        for (traceClassItem in traceClassSet) {
            if (originFullMethod.startsWith(traceClassItem)) {
                needTrace = true
                break
            }
        }
        return needTrace
    }

    private fun isTraceMethod(originFullMethod: String, classNode: ClassNode): Boolean {
        traceMethodSet.forEach {
            if (it.className.isEmpty()) {
                //analysis superclass
                val proguardSuperClassName = classNode.superName.replace("/", ".")
                val originSuperclassName = mappingCollector.originalClassName(proguardSuperClassName, proguardSuperClassName)
                if (originSuperclassName == it.superClassName && originFullMethod.contains("." + it.method + "(")) {
                    return true
                }
                //analysis interfaces
                classNode.interfaces.forEach { interfaceName ->
                    val proguardInterfaceName = interfaceName.replace("/", ".")
                    val originInterfaceName = mappingCollector.originalClassName(proguardInterfaceName, proguardInterfaceName)
                    if (originInterfaceName == it.superClassName && originFullMethod.contains("." + it.method + "(")) {
                        return true
                    }
                }
            } else {
                if (it.className == "*" && (originFullMethod.contains("." + it.method + "("))) {
                    return true
                }
                if (originFullMethod.startsWith(it.className + "." + it.method + "(")) {
                    return true
                }
            }
        }
        return false
    }

    override fun isMethodWithParamValue(methodName: String): Boolean {
        allowMethodsWithParamMap.keys.forEach {
            if (methodName.startsWith(it)) {
                return true
            }
        }
        return false
    }

    override fun getMethodWithParamValue(methodName: String): List<Int>? {
        allowMethodsWithParamMap.keys.forEach {
            if (methodName.startsWith(it)) {
                return allowMethodsWithParamMap[it]
            }
        }
        return emptyList()
    }

    override fun onClassNeedFilter(originFullMethod: String): Boolean {
        val needFilter = super.onClassNeedFilter(originFullMethod)
        if (!needFilter) {
            if (!isAllowPackage(originFullMethod)) {
                return true
            }
            return isBlockClass(originFullMethod)
        }
        return needFilter
    }

    override fun onMethodNeedFilter(methodNode: MethodNode, traceMethod: TraceMethod, originFullMethod: String): Boolean {
        val isFilter = super.onMethodNeedFilter(methodNode, traceMethod, originFullMethod)
        if (!isFilter) {
            return isBlockMethod(originFullMethod)
        }
        return isFilter
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

data class FilterTraceMethodData(var className: String = "", var superClassName: String = "", var method: String = "")
