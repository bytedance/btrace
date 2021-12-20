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

import com.android.build.api.transform.Status
import com.android.utils.FileUtils
import com.bytedance.rheatrace.plugin.compiling.filter.RheaTraceMethodFilter
import com.bytedance.rheatrace.plugin.extension.TraceCompilation
import com.bytedance.rheatrace.plugin.extension.TraceRuntime
import com.bytedance.rheatrace.plugin.internal.common.FileUtil
import com.bytedance.rheatrace.plugin.internal.common.RheaLog
import com.bytedance.rheatrace.plugin.retrace.MappingCollector
import com.google.common.hash.Hashing
import java.io.File
import java.util.*
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.Future
import java.util.concurrent.atomic.AtomicInteger

class TraceWeaver(
    private val applicationName: String?,
    private val methodMappingDir: String,
    private val methodMapFilePath: String,
    private val ignoreMethodMapFilePath: String,
    private val traceCompilation: TraceCompilation?,
    private val traceRuntime: TraceRuntime?
) {

    companion object {
        private const val TAG: String = "TraceWeaver"
    }

    fun doTransform(
        classInputs: Collection<File>,
        changedFiles: Map<File, Status>,
        inputToOutput: Map<File, File>,
        isIncremental: Boolean,
        traceClassOutputDirectory: File,
        legacyReplaceChangedFile: ((File, Map<File, Status>) -> Any)?,
        legacyReplaceFile: ((File, File) -> (Any))?
    ) {

        val executor: ExecutorService = Executors.newFixedThreadPool(16)

        val mappingCollector = MappingCollector()
        val methodId = AtomicInteger(0)
        val collectedMethodMap = ConcurrentHashMap<String, TraceMethod>()
        val dirInputOutMap = ConcurrentHashMap<File, File>()
        val jarInputOutMap = ConcurrentHashMap<File, File>()


        val traceMethodFilter =
            RheaTraceMethodFilter(
                traceCompilation?.traceFilterFilePath,
                mappingCollector
            )

        /**
         * step 1 parse mapping, collect need inject src file & jar file
         */
        var start = System.currentTimeMillis()

        val futures = LinkedList<Future<*>>()

        val mappingTask = ParseMappingTask(
            methodId,
            methodMappingDir,
            traceCompilation?.applyMethodMappingFilePath,
            mappingCollector,
            collectedMethodMap
        )
        futures.add(executor.submit(mappingTask))

        for (file in classInputs) {
            if (file.isDirectory) {
                val collectDirectoryTask =
                    CollectDirectoryInputTask(
                        file, changedFiles, inputToOutput,
                        isIncremental, traceClassOutputDirectory, legacyReplaceChangedFile,
                        legacyReplaceFile, dirInputOutMap
                    )
                futures.add(executor.submit(collectDirectoryTask))
            } else {
                val status = Status.CHANGED
                val collectJarInputTask =
                    CollectJarInputTask(
                        file, status, inputToOutput, isIncremental,
                        traceClassOutputDirectory, legacyReplaceFile, jarInputOutMap
                    )
                futures.add(executor.submit(collectJarInputTask))
            }
        }

        for (future in futures) {
            future.get()
        }
        futures.clear()
        var time = System.currentTimeMillis() - start
        RheaLog.i(TAG, "[doTransform] Step(1)[Parse]... cost:%sms", time)

        /**
         * step 2 collector need inject method
         */

        start = System.currentTimeMillis()
        val methodCollector = MethodCollector(
            methodId,
            executor,
            methodMapFilePath,
            ignoreMethodMapFilePath,
            traceMethodFilter,
            collectedMethodMap
        )
        methodCollector.collect(dirInputOutMap.keys, jarInputOutMap.keys)
        time = System.currentTimeMillis() - start
        RheaLog.i(TAG, "[doTransform] Step(2)[Collection]... cost:%sms", time)

        /**
         * step 3 inject
         */
        start = System.currentTimeMillis()

        val tracer = MethodTracer(
            executor,
            mappingCollector,
            collectedMethodMap,
            traceCompilation!!,
            applicationName,
            traceRuntime,
            traceMethodFilter
        )
        tracer.trace(dirInputOutMap, jarInputOutMap)
        time = System.currentTimeMillis() - start
        RheaLog.i(TAG, "[doTransform] Step(3)[Trace]... cost:%sms", time)
    }
}


class CollectDirectoryInputTask(
    private val directoryInput: File,
    private val mapOfChangedFiles: Map<File, Status>,
    private val mapOfInputToOutput: Map<File, File>,
    private val isIncremental: Boolean,
    private val traceClassOutput: File,
    private val legacyReplaceChangedFile: ((File, Map<File, Status>) -> (Any))?,     // Will be removed in the future
    private val legacyReplaceFile: ((File, File) -> (Any))?,                         // Will be removed in the future
    private val resultOfDirInputToOut: MutableMap<File, File>
) : Runnable {
    companion object {
        private const val TAG: String = "CollectDirectoryInputTask"
    }

    override fun run() {
        try {
            handle()
        } catch (e: Exception) {
            e.printStackTrace()
            RheaLog.e(TAG, "%s", e.toString())
        }
    }

    private fun handle() {
        val dirInput = directoryInput
        val dirOutput = if (mapOfInputToOutput.containsKey(dirInput)) {
            mapOfInputToOutput.getValue(dirInput)
        } else {
            File(traceClassOutput, dirInput.name)
        }
        val inputFullPath = dirInput.absolutePath
        val outputFullPath = dirOutput.absolutePath

        if (!dirOutput.exists()) {
            dirOutput.mkdirs()
        }

        if (!dirInput.exists() && dirOutput.exists()) {
            if (dirOutput.isDirectory) {
                FileUtils.deletePath(dirOutput)
            } else {
                FileUtils.delete(dirOutput)
            }
        }

        if (isIncremental) {
            val outChangedFiles = HashMap<File, Status>()
            for ((changedFileInput, status) in mapOfChangedFiles) {
                val changedFileInputFullPath = changedFileInput.absolutePath
                val changedFileOutput = if (changedFileInputFullPath.contains(inputFullPath)) {
                    File(changedFileInputFullPath.replace(inputFullPath, outputFullPath))
                } else { // if not contains, changedFileOutput should be modify, else when we read and write the same file, the jar would be empty
                    File(outputFullPath, changedFileInput.name)
                }
                if (status == Status.ADDED || status == Status.CHANGED) {
                    resultOfDirInputToOut[changedFileInput] = changedFileOutput
                } else if (status == Status.REMOVED) {
                    changedFileOutput.delete()
                }
                outChangedFiles[changedFileOutput] = status
            }

            legacyReplaceChangedFile?.invoke(dirInput, outChangedFiles)
        } else {
            resultOfDirInputToOut[dirInput] = dirOutput
        }

        legacyReplaceFile?.invoke(dirInput, dirOutput)
    }
}

class CollectJarInputTask(
    private val inputJar: File,
    private val inputJarStatus: Status,
    private val inputToOutput: Map<File, File>,
    private val isIncremental: Boolean,
    private val traceClassFileOutput: File,
    private val legacyReplaceFile: ((File, File) -> (Any))?,
    private val resultOfJarInputToOut: MutableMap<File, File>
) : Runnable {
    companion object {
        private const val TAG: String = "CollectJarInputTask"
    }

    override fun run() {
        try {
            handle()
        } catch (e: Exception) {
            e.printStackTrace()
            RheaLog.e(TAG, "%s", e.toString())
        }
    }

    private fun handle() {

        val jarInput = inputJar
        val jarOutput = if (inputToOutput.containsKey(jarInput)) {
            inputToOutput[jarInput]!!
        } else {
            File(traceClassFileOutput, getUniqueJarName(jarInput))
        }

        if (!isIncremental && jarOutput.exists()) {
            jarOutput.delete()
        }
        if (!jarOutput.parentFile.exists()) {
            jarOutput.parentFile.mkdirs()
        }

        if (FileUtil.isRealZipOrJar(jarInput)) {
            if (isIncremental) {
                if (inputJarStatus == Status.ADDED || inputJarStatus == Status.CHANGED) {
                    resultOfJarInputToOut[jarInput] = jarOutput
                } else if (inputJarStatus == Status.REMOVED) {
                    jarOutput.delete()
                }
            } else {
                resultOfJarInputToOut[jarInput] = jarOutput
            }
        }
        legacyReplaceFile?.invoke(jarInput, jarOutput)
    }

    @Suppress("DEPRECATION")
    fun getUniqueJarName(jarFile: File): String {
        val origJarName = jarFile.name
        val hashing = Hashing.sha1().hashString(jarFile.path, Charsets.UTF_16LE).toString()
        val dotPos = origJarName.lastIndexOf('.')
        return if (dotPos < 0) {
            String.format("%s_%s", origJarName, hashing)
        } else {
            val nameWithoutDotExt = origJarName.substring(0, dotPos)
            val dotExt = origJarName.substring(dotPos)
            String.format("%s_%s%s", nameWithoutDotExt, hashing, dotExt)
        }
    }
}
