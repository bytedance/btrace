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

import com.bytedance.rheatrace.plugin.internal.common.RheaConstants
import com.bytedance.rheatrace.plugin.internal.common.RheaLog
import com.bytedance.rheatrace.plugin.retrace.MappingCollector
import com.bytedance.rheatrace.plugin.retrace.MappingReader
import java.io.File
import java.util.*
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger
import kotlin.collections.HashMap

class ParseMappingTask constructor(
    private val methodId: AtomicInteger,
    private val methodMappingDir: String,
    private val applyMethodMappingFilePath: String?,
    private val mappingCollector: MappingCollector,
    private val collectedMethodMap: ConcurrentHashMap<String, TraceMethod>
) : Runnable {

    companion object {
        private const val TAG: String = "ParseMappingTask"
    }

    override fun run() {
        val start = System.currentTimeMillis()

        val mappingFile = File(methodMappingDir, "mapping.txt")
        if (mappingFile.isFile) {
            val mappingReader =
                MappingReader(
                    mappingFile
                )
            mappingReader.read(mappingCollector)
        }
        getAppliedMethodMapping(applyMethodMappingFilePath, collectedMethodMap)
        retraceMethodMap(mappingCollector, collectedMethodMap)
        val duration = System.currentTimeMillis() - start
        RheaLog.i(
            TAG, "[ParseMappingTask#run] cost:%sms,  collect %s method from %s",
            duration, collectedMethodMap.size, applyMethodMappingFilePath
        )
    }

    private fun getAppliedMethodMapping(
        applyMethodMappingFilePath: String?,
        collectedMethodMap: ConcurrentHashMap<String, TraceMethod>
    ) {
        if (applyMethodMappingFilePath == null) {
            RheaLog.w(TAG, "[applyMethodMappingFilePath] is null!")
            return
        }

        val applyMethodMappingFile = File(applyMethodMappingFilePath)

        if (!applyMethodMappingFile.exists()) {
            RheaLog.w(
                TAG,
                "[applyMethodMappingFile] is not existed, " + applyMethodMappingFile.absoluteFile
            )
        }

        try {
            Scanner(applyMethodMappingFile, "UTF-8").use { fileReader ->
                while (fileReader.hasNext()) {
                    var nextLine = fileReader.nextLine()
                    if (!(nextLine == null || nextLine.isEmpty())) {
                        nextLine = nextLine.trim()
                        if (nextLine.startsWith("#")) {
                            RheaLog.i("[getMethodFromKeepMethod] comment %s", nextLine)
                            continue
                        }
                        val fields = nextLine.split(",")
                        val traceMethod =
                            TraceMethod()
                        traceMethod.id = Integer.parseInt(fields[0])
                        traceMethod.accessFlag = Integer.parseInt(fields[1])
                        val methodField = fields[2].split(" ")
                        traceMethod.className = methodField[0].replace("/", ".")
                        traceMethod.methodName = methodField[1]
                        if (methodField.size > 2) {
                            traceMethod.desc = methodField[2].replace("/", ".")
                        }
                        collectedMethodMap[traceMethod.getFullMethodName()] = traceMethod
                        if (methodId.get() < traceMethod.id && traceMethod.id != RheaConstants.METHOD_ID_DISPATCH) {
                            methodId.set(traceMethod.id)
                        }
                    }
                }
            }
        } catch (e: Throwable) {
            RheaLog.w(TAG, "Failed to parse applied method mapping file")
        }
    }

    private fun retraceMethodMap(
        processor: MappingCollector,
        methodMap: ConcurrentHashMap<String, TraceMethod>
    ) {
        val retraceMethodMap = HashMap<String, TraceMethod>(methodMap.size)
        for (traceMethod in methodMap.values) {
            traceMethod.proguard(processor)
            retraceMethodMap[traceMethod.getFullMethodName()] = traceMethod
        }
        methodMap.clear()
        methodMap.putAll(retraceMethodMap)
        retraceMethodMap.clear()
    }
}
