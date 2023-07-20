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
package com.bytedance.rheatrace.plugin.internal

import com.android.build.gradle.AppExtension
import com.android.build.gradle.api.ApplicationVariant
import com.android.build.gradle.internal.pipeline.TransformTask
import com.bytedance.rheatrace.common.utils.RheaLog
import com.bytedance.rheatrace.plugin.extension.RheaBuildExtension
import com.bytedance.rheatrace.plugin.internal.RheaFileUtils.MethodMappingFileName
import com.bytedance.rheatrace.plugin.internal.RheaFileUtils.getMethodMapFilePath
import org.gradle.api.Action
import org.gradle.api.Project
import org.gradle.api.Task
import org.gradle.api.file.Directory
import org.gradle.api.provider.Provider
import java.io.File

/**
 * Created by majun.oreo on 2023/3/21
 * @author majun.oreo@bytedance.com
 */
object CopyMappingTask {
    private val TAG = "SaveMappingTask"

    @Suppress("DefaultLocale")
    fun registerTaskSaveMappingToAssets(project: Project, extension: RheaBuildExtension) {
        RheaLog.i(TAG, "registerTaskSaveMappingToAssets")
        val android = project.extensions.getByName("android") as AppExtension
        // set additional assets dir
        val assetsDir = project.layout.buildDirectory.dir("generated/rhea_assets/")
        android.sourceSets.getByName("main").assets.srcDir(assetsDir)
        RheaLog.i(TAG, "add assets dir: ${assetsDir.get().asFile}")
        android.applicationVariants.configureEach { variant ->
            RheaLog.i(TAG, "variant: ${variant.name}")
            //Add methodMapping to determine whether packing is required
            if (extension.compilation?.needPackageWithMethodMap == false) {
                RheaLog.i(TAG, "needPackageWithMethodMap = false ,registerTaskSaveMappingToAssets failed")
                return@configureEach
            }
            // copy mapping into assets
            RheaLog.i(TAG, "hookAssetsTask work")
            val enableR8 = "false" != project.properties["android.enableR8"]
            if (enableR8) {
                hookAssetsTaskR8(variant, project, assetsDir)
            } else {
                hookAssetsTask(variant, project, assetsDir)
            }
        }
    }

    /**
     * hook assetsTask injects the methodMapping file into the Apk in the R8 compilation environment
     */
    private fun hookAssetsTaskR8(variant: ApplicationVariant, project: Project, assetsDir: Provider<Directory>) {
        kotlin.runCatching {
            val copyMappingTask = getCopyMappingTask(variant, project, assetsDir)
            // RheaTraceTask -> copyMappingTask -> mergeAssetsTask
            val mergeAssetsTask = project.tasks.getByName("merge${variant.name.capitalize()}Assets")
            val rheaTraceTask =  project.tasks.getByName("transformClassesWithRheaTraceFor${variant.name.capitalize()}")
            RheaLog.i(TAG, "${copyMappingTask.name} dependsOn ${rheaTraceTask.name}")
            copyMappingTask.dependsOn(rheaTraceTask)
            RheaLog.i(TAG, "${mergeAssetsTask.name} dependsOn ${copyMappingTask.name}")
            mergeAssetsTask.dependsOn(copyMappingTask)
        }.onFailure {
            RheaLog.i(TAG, "hookAssetsTaskR8 error")
            RheaLog.e(TAG, it.message.toString())
        }
    }

    /**
     * hook assetsTask injects the methodMapping file into the Apk
     */
    private fun hookAssetsTask(variant: ApplicationVariant, project: Project, assetsDir: Provider<Directory>) {
        kotlin.runCatching {
            val copyMappingTask = getCopyMappingTask(variant, project, assetsDir)
            // dexBuilder -> copyMapping -> mergeAssets
            val mergeAssetsTask = project.tasks.getByName("merge${variant.name.capitalize()}Assets")
            val dexBuilderTask = project.tasks.firstOrNull { it is TransformTask && it.transform.name == "dexBuilder" } ?: project.tasks.getByName("dexBuilder${variant.name.capitalize()}")
            RheaLog.i(TAG, "${copyMappingTask.name} dependsOn ${dexBuilderTask.name}")
            copyMappingTask.dependsOn(dexBuilderTask)
            RheaLog.i(TAG, "${mergeAssetsTask.name} dependsOn ${copyMappingTask.name}")
            mergeAssetsTask.dependsOn(copyMappingTask)
        }.onFailure {
            RheaLog.e(TAG, it.message.toString())
        }
    }

    private fun getCopyMappingTask(variant: ApplicationVariant, project: Project, assetsDir: Provider<Directory>): Task {
        return project.tasks.register("copyRhea${variant.name.capitalize()}Mapping") {
            it.actions.add(Action {
                assetsDir.get().asFile.mkdirs()
                val input = File(getMethodMapFilePath(project, variant.name))
                input.copyTo(File(assetsDir.get().asFile, MethodMappingFileName), true)
                RheaLog.i(TAG, "copy $input into ${assetsDir.get().asFile}")
            })
        }.get()
    }
}