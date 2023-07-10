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

package com.bytedance.rheatrace.plugin.task

import com.android.build.api.transform.Status
import com.android.build.gradle.api.ApplicationVariant
import com.android.build.gradle.internal.tasks.DexArchiveBuilderTask
import com.android.builder.model.AndroidProject
import com.bytedance.rheatrace.common.utils.AGPCompat
import com.bytedance.rheatrace.common.utils.ManifestUtil
import com.bytedance.rheatrace.common.utils.RheaLog
import com.bytedance.rheatrace.plugin.compiling.TraceWeaver
import com.bytedance.rheatrace.plugin.extension.RheaBuildExtension
import com.bytedance.rheatrace.plugin.extension.TraceCompilation
import com.bytedance.rheatrace.plugin.extension.TraceRuntime
import com.bytedance.rheatrace.plugin.internal.RheaConstants
import com.bytedance.rheatrace.plugin.internal.RheaFileUtils.getIgnoreMethodMapFilePath
import com.bytedance.rheatrace.plugin.internal.RheaFileUtils.getMethodMapFilePath
import com.bytedance.rheatrace.plugin.transform.RheaBaseTransform
import com.google.common.base.Joiner
import org.gradle.api.Action
import org.gradle.api.DefaultTask
import org.gradle.api.Project
import org.gradle.api.file.ConfigurableFileCollection
import org.gradle.api.file.FileType
import org.gradle.api.file.RegularFileProperty
import org.gradle.api.provider.Property
import org.gradle.api.tasks.*
import org.gradle.work.ChangeType
import org.gradle.work.Incremental
import org.gradle.work.InputChanges
import java.io.File
import java.util.concurrent.Callable
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ExecutionException

abstract class RheaTraceTask : DefaultTask() {

    companion object {
        private const val TAG: String = "RheaTraceTask"
    }

    var traceCompilation: TraceCompilation? = null

    var dexBuilderTask: DexArchiveBuilderTask? = null

    lateinit var variantName: String

    @get:Input
    abstract val rheaBuildExtension: Property<String>

    @get:InputFile
    abstract val manifestFile: RegularFileProperty

    @get:Incremental
    @get:InputFiles
    abstract val classInputs: ConfigurableFileCollection

    @get:InputFile
    @get:Optional
    @get:PathSensitive(PathSensitivity.ABSOLUTE)
    abstract val applyMethodMappingFile: RegularFileProperty

    @get:OutputDirectory
    abstract val traceClassOutputDirectory: RegularFileProperty

    @get:Input
    @get:Optional
    abstract val mappingDir: Property<String>

    @get:InputFile
    @get:Optional
    @get:PathSensitive(PathSensitivity.ABSOLUTE)
    abstract val traceFilterFile: RegularFileProperty

    @get:OutputFile
    @get:PathSensitive(PathSensitivity.ABSOLUTE)
    @get:Optional
    abstract val ignoreMethodMappingFile: RegularFileProperty

    @get:OutputFile
    @get:PathSensitive(PathSensitivity.ABSOLUTE)
    @get:Optional
    abstract val methodMappingFile: RegularFileProperty

    @TaskAction
    fun execute(inputChanges: InputChanges) {
        RheaLog.i(TAG, "rhea build config:" + rheaBuildExtension.get().toString())

        val incremental = inputChanges.isIncremental
        val changedFiles = ConcurrentHashMap<File, Status>()
        if (incremental) {
            inputChanges.getFileChanges(classInputs).forEach { change ->
                if (change.fileType == FileType.DIRECTORY) return@forEach

                changedFiles[change.file] = when (change.changeType) {
                    ChangeType.REMOVED -> Status.REMOVED
                    ChangeType.MODIFIED -> Status.CHANGED
                    ChangeType.ADDED -> Status.ADDED
                    else -> Status.NOTCHANGED
                }
            }
        }

        val start = System.currentTimeMillis()
        try {

            val outputDirectory = traceClassOutputDirectory.get().asFile
            TraceWeaver(
                ManifestUtil.readApplicationName(manifestFile.asFile.get()),
                mappingDir.get(),
                methodMappingFile.asFile.get().absolutePath,
                ignoreMethodMappingFile.asFile.get().absolutePath,
                traceCompilation,
                variantName,
                project
            ).doTransform(
                classInputs = classInputs.files,
                changedFiles = changedFiles,
                isIncremental = incremental,
                traceClassOutputDirectory = outputDirectory,
                inputToOutput = ConcurrentHashMap(),
                legacyReplaceChangedFile = null,
                legacyReplaceFile = null
            )

        } catch (e: ExecutionException) {
            e.printStackTrace()
        }
        val cost = System.currentTimeMillis() - start
        RheaLog.i(TAG, " Insert rhea trace instrumentations cost time: %sms.", cost)

        val outputFiles = project.files(Callable<Collection<File>> {
            val outputDirectory = traceClassOutputDirectory.get().asFile
            val collection = ArrayList<File>()
            outputDirectory.walkTopDown().filter { it.isFile }.forEach {
                collection.add(it)
            }
            return@Callable collection
        })
        // Wire trace task outputs to dex task inputs.
        dexBuilderTask?.mixedScopeClasses?.setFrom(outputFiles)
    }

    fun wired(task: DexArchiveBuilderTask) {

        RheaLog.i(TAG, "Wiring ${this.name} to task '${task.name}'.")

        task.dependsOn(this)
        // Wire minify task outputs to trace task inputs.
        classInputs.setFrom(task.mixedScopeClasses.files)

        dexBuilderTask = task
    }

    class CreationAction(
        private val project: Project, private val variant: ApplicationVariant, private val extension: RheaBuildExtension
    ) : Action<RheaTraceTask> {

        override fun execute(task: RheaTraceTask) {

            val mergedManifest = AGPCompat.getMergedManifestFile(project, variant.name)

            val mappingFileDir = variant.mappingFile?.parentFile

            RheaLog.i(RheaBaseTransform.TAG, "mappingFile path: ${mappingFileDir?.absolutePath}")

            val mappingOutput = if (mappingFileDir == null) {
                Joiner.on(File.separatorChar).join(
                    project.buildDir.absolutePath,
                    AndroidProject.FD_OUTPUTS,
                    "mapping",
                    variant.name
                )
            } else {
                mappingFileDir.absolutePath
            }

            val traceClassOutputDir = Joiner.on(File.separatorChar).join(
                project.buildDir.absolutePath,
                RheaConstants.RHEA_TRACE_ROOT,
                AndroidProject.FD_OUTPUTS,
                RheaConstants.TRACE_CLASS_DIR,
                variant.name
            )
            val methodMappingFilePath = getMethodMapFilePath(project, variant.name)
            val ignoreMethodMappingFilePath = getIgnoreMethodMapFilePath(project, variant.name)

            task.traceCompilation = extension.compilation
            task.variantName = variant.name

            task.rheaBuildExtension.set(extension.toString())
            task.manifestFile.set(mergedManifest)
            val applyMethodMappingFilePath = extension.compilation?.applyMethodMappingFilePath
            if (applyMethodMappingFilePath != null) {
                val applyMethodMappingFile = File(applyMethodMappingFilePath)
                if (applyMethodMappingFile.exists()) {
                    task.applyMethodMappingFile.set(applyMethodMappingFile)
                }
            }
            task.traceClassOutputDirectory.set(File(traceClassOutputDir))

            task.mappingDir.set(mappingOutput)
            val traceFilterFilePath = extension.compilation?.traceFilterFilePath
            if (traceFilterFilePath != null) {
                val traceFilterFile = File(traceFilterFilePath)
                if (traceFilterFile.exists()) {
                    task.traceFilterFile.set(traceFilterFile)
                }
            }
            task.ignoreMethodMappingFile.set(File(ignoreMethodMappingFilePath))
            task.methodMappingFile.set(File(methodMappingFilePath))
        }
    }
}