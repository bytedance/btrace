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

import com.android.build.api.transform.DirectoryInput
import com.android.build.api.transform.QualifiedContent
import com.android.build.api.transform.Status
import com.android.build.api.transform.Transform
import com.android.build.api.transform.TransformInvocation
import com.android.build.gradle.api.BaseVariant
import com.android.build.gradle.internal.pipeline.TransformTask
import com.bytedance.rheatrace.common.utils.ReflectUtil
import com.bytedance.rheatrace.common.utils.RheaLog
import com.bytedance.rheatrace.plugin.compiling.TraceWeaver
import com.bytedance.rheatrace.plugin.extension.RheaBuildExtension
import org.gradle.api.Project
import java.io.File
import java.util.concurrent.ConcurrentHashMap

class RheaTraceLegacyTransform(
    project: Project,
    extension: RheaBuildExtension,
    var origTransform: Transform
) : RheaBaseTransform(project, extension) {

    companion object {

        const val TAG = "rheaTraceLegacyTransform"

        fun inject(
            extension: RheaBuildExtension,
            project: Project,
            variant: BaseVariant
        ) {
            val hardTask =
                getTransformTaskName(
                    variant.name
                )
            for (task in project.tasks) {
                for (str in hardTask) {
                    if (task.name.equals(str, ignoreCase = true) && task is TransformTask) {
                        val field = ReflectUtil.getDeclaredFieldRecursive(
                            task::class.java,
                            "transform"
                        )
                        val legacyTransform =
                            RheaTraceLegacyTransform(project, extension, task.transform)
                        legacyTransform.mappingFileMap[variant.name] = variant.mappingFile
                        field.set(task, legacyTransform)
                        RheaLog.i(TAG, "successfully inject task:" + task.name)
                        break
                    }
                }
            }
        }

        private fun getTransformTaskName(
            buildTypeSuffix: String
        ) = arrayOf(
            "transformClassesWithDexBuilderFor$buildTypeSuffix",
            "transformClassesWithDexFor$buildTypeSuffix"
        )
    }

    override fun getName(): String {
        return TAG
    }

    override fun transform(transformInvocation: TransformInvocation, transformArgs: TransformArgs) {
        RheaLog.i(TAG, "start run RheaTraceLegacyTransform")
        val start = System.currentTimeMillis()
        try {
            doTransform(transformInvocation, transformArgs) // hack
        } catch (e: Throwable) {
            e.printStackTrace()
        }
        val cost = System.currentTimeMillis() - start
        val begin = System.currentTimeMillis()
        origTransform.transform(transformInvocation)
        val origTransformCost = System.currentTimeMillis() - begin;
        RheaLog.i(
            TAG,
            "[transform] cost time: %dms %s:%s ms RheaTraceTransform:%sms",
            System.currentTimeMillis() - start,
            origTransform.javaClass.simpleName,
            origTransformCost,
            cost
        )
    }

    private fun doTransform(invocation: TransformInvocation, transformArgs: TransformArgs) {

        val start = System.currentTimeMillis()

        val isIncremental = invocation.isIncremental && this.isIncremental
        val changedFiles = ConcurrentHashMap<File, Status>()
        val inputFiles = ArrayList<File>()
        val fileToInput = ConcurrentHashMap<File, QualifiedContent>()
        for (input in invocation.inputs) {
            for (directoryInput in input.directoryInputs) {
                changedFiles.putAll(directoryInput.changedFiles)
                inputFiles.add(directoryInput.file)
                fileToInput[directoryInput.file] = directoryInput
            }

            for (jarInput in input.jarInputs) {
                changedFiles[jarInput.file] = jarInput.status
                inputFiles.add(jarInput.file)
                fileToInput[jarInput.file] = jarInput
            }
        }

        if (inputFiles.size == 0) {
            RheaLog.i(TAG, "rhea trace do not find any input files")
            return
        }

        val legacyReplaceChangedFile = { inputDir: File, map: Map<File, Status> ->
            replaceChangedFile(fileToInput[inputDir] as DirectoryInput, map)
            inputDir as Object
        }

        val legacyReplaceFile = { input: File, output: File ->
            replaceFile(fileToInput[input] as QualifiedContent, output)
            input as Object
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
            inputToOutput = ConcurrentHashMap(),
            isIncremental = isIncremental,
            traceClassOutputDirectory = File(transformArgs.traceClassOutputDir),
            legacyReplaceChangedFile = legacyReplaceChangedFile,
            legacyReplaceFile = legacyReplaceFile
        )

        val cost = System.currentTimeMillis() - start
        RheaLog.i(TAG, " Insert rhea trace instrumentations cost time: %sms.", cost)
    }

    private fun replaceFile(input: QualifiedContent, newFile: File) {
        val fileField = ReflectUtil.getDeclaredFieldRecursive(input.javaClass, "file")
        fileField.set(input, newFile)
    }

    private fun replaceChangedFile(dirInput: DirectoryInput, changedFiles: Map<File, Status>) {
        val changedFilesField =
            ReflectUtil.getDeclaredFieldRecursive(dirInput.javaClass, "changedFiles")
        changedFilesField.set(dirInput, changedFiles)
    }
}