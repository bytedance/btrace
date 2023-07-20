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

package com.bytedance.rheatrace.plugin.transform

import com.android.build.api.transform.Format
import com.android.build.api.transform.Status
import com.android.build.api.transform.TransformInvocation
import com.android.utils.FileUtils
import com.bytedance.rheatrace.common.utils.RheaLog
import com.bytedance.rheatrace.plugin.compiling.TraceWeaver
import com.bytedance.rheatrace.plugin.extension.RheaBuildExtension
import org.gradle.api.Project
import java.io.File
import java.util.concurrent.ConcurrentHashMap

class RheaTraceTransform(
    project: Project,
    extension: RheaBuildExtension
) : RheaBaseTransform(project, extension) {

    companion object {
        const val TAG = "RheaTraceTransform"
    }

    val transparentMap: HashMap<String, Boolean> = HashMap()

    override fun getName(): String {
        return "rheaTrace"
    }

    override fun transform(transformInvocation: TransformInvocation, transformArgs: TransformArgs) {
        if (transparentMap.getOrDefault(transformInvocation.context.variantName, true)) {
            transparent(transformInvocation)
        } else {
            transforming(transformInvocation, transformArgs)
        }
    }

    /**
     * Passes all inputs throughout.
     */
    private fun transparent(invocation: TransformInvocation) {

        val outputProvider = invocation.outputProvider!!

        if (!invocation.isIncremental) {
            outputProvider.deleteAll()
        }

        for (ti in invocation.inputs) {
            for (jarInput in ti.jarInputs) {
                val inputJar = jarInput.file
                val outputJar = outputProvider.getContentLocation(
                    jarInput.name,
                    jarInput.contentTypes,
                    jarInput.scopes,
                    Format.JAR
                )

                if (invocation.isIncremental) {
                    when (jarInput.status) {
                        Status.NOTCHANGED -> {
                        }
                        Status.ADDED, Status.CHANGED -> {
                            copyFileAndMkdirsAsNeed(inputJar, outputJar)
                        }
                        Status.REMOVED -> FileUtils.delete(outputJar)
                        else -> {
                            //do nothing
                        }
                    }
                } else {
                    copyFileAndMkdirsAsNeed(inputJar, outputJar)
                }
            }
            for (directoryInput in ti.directoryInputs) {
                val inputDir = directoryInput.file
                val outputDir = outputProvider.getContentLocation(
                    directoryInput.name,
                    directoryInput.contentTypes,
                    directoryInput.scopes,
                    Format.DIRECTORY
                )

                if (invocation.isIncremental) {
                    for (entry in directoryInput.changedFiles.entries) {
                        val inputFile = entry.key
                        when (entry.value) {
                            Status.NOTCHANGED -> {
                                //do nothing
                            }
                            Status.ADDED, Status.CHANGED -> if (!inputFile.isDirectory) {
                                val outputFile = toOutputFile(outputDir, inputDir, inputFile)
                                copyFileAndMkdirsAsNeed(inputFile, outputFile)
                            }
                            Status.REMOVED -> {
                                val outputFile = toOutputFile(outputDir, inputDir, inputFile)
                                FileUtils.deleteIfExists(outputFile)
                            }
                            else -> {
                                //do nothing.
                            }
                        }
                    }
                } else {
                    for (inputFile in FileUtils.getAllFiles(inputDir)) {
                        val out = toOutputFile(outputDir, inputDir, inputFile)
                        copyFileAndMkdirsAsNeed(inputFile, out)
                    }
                }
            }
        }
    }

    private fun transforming(invocation: TransformInvocation, transformArgs: TransformArgs) {

        val start = System.currentTimeMillis()

        val outputProvider = invocation.outputProvider!!

        val isIncremental = invocation.isIncremental && this.isIncremental
        if (!isIncremental) {
            outputProvider.deleteAll()
        }

        val changedFiles = ConcurrentHashMap<File, Status>()
        val inputToOutput = ConcurrentHashMap<File, File>()
        val inputFiles = ArrayList<File>()

        for (input in invocation.inputs) {
            for (directoryInput in input.directoryInputs) {
                changedFiles.putAll(directoryInput.changedFiles)
                val inputDir = directoryInput.file
                inputFiles.add(inputDir)
                val outputDirectory = outputProvider.getContentLocation(
                    directoryInput.name,
                    directoryInput.contentTypes,
                    directoryInput.scopes,
                    Format.DIRECTORY
                )
                inputToOutput[inputDir] = outputDirectory
            }

            for (jarInput in input.jarInputs) {
                val inputFile = jarInput.file
                changedFiles[inputFile] = jarInput.status
                inputFiles.add(inputFile)
                val outputJar = outputProvider.getContentLocation(
                    jarInput.name,
                    jarInput.contentTypes,
                    jarInput.scopes,
                    Format.JAR
                )
                inputToOutput[inputFile] = outputJar
            }
        }

        if (inputFiles.size == 0) {
            RheaLog.i(TAG, "rhea trace do not find any input files")
            return
        }

        TraceWeaver(
            transformArgs.applicationName,
            transformArgs.mappingOutputDir,
            transformArgs.methodMappingFilePath,
            transformArgs.ignoreMethodMappingFilePath,
            transformArgs.traceCompilation,
            invocation.context.variantName,
            project
        ).doTransform(
            classInputs = inputFiles,
            changedFiles = changedFiles,
            inputToOutput = inputToOutput,
            isIncremental = isIncremental,
            traceClassOutputDirectory = File(transformArgs.traceClassOutputDir),
            legacyReplaceChangedFile = null,
            legacyReplaceFile = null
        )

        val cost = System.currentTimeMillis() - start
        RheaLog.i(TAG, " Insert rhea trace instrumentations cost time: %sms.", cost)
    }

    private fun copyFileAndMkdirsAsNeed(from: File, to: File) {
        if (from.exists()) {
            to.parentFile.mkdirs()
            FileUtils.copyFile(from, to)
        }
    }

    private fun toOutputFile(outputDir: File, inputDir: File, inputFile: File): File {
        return File(outputDir, FileUtils.relativePossiblyNonExistingPath(inputFile, inputDir))
    }
}