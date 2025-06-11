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

package com.bytedance.rheatrace.plugin.transform;

import com.android.build.api.transform.QualifiedContent
import com.android.build.api.transform.SecondaryFile
import com.android.build.api.transform.Transform
import com.android.build.api.transform.TransformInvocation
import com.android.build.gradle.internal.pipeline.TransformManager
import com.android.builder.model.AndroidProject
import com.bytedance.rheatrace.common.utils.AGPCompat
import com.bytedance.rheatrace.common.utils.ManifestUtil
import com.bytedance.rheatrace.common.utils.RheaLog
import com.bytedance.rheatrace.plugin.extension.RheaBuildExtension
import com.bytedance.rheatrace.plugin.extension.TraceCompilation
import com.bytedance.rheatrace.plugin.internal.RheaConstants
import com.bytedance.rheatrace.plugin.internal.RheaFileUtils
import com.google.common.base.Joiner
import org.gradle.api.Project
import java.io.File

abstract class RheaBaseTransform(
    val project: Project,
    private val extension: RheaBuildExtension
) : Transform() {


    companion object {
        const val TAG: String = "Rhea:Transform"
    }

    val mappingFileMap: HashMap<String, File?> = HashMap()

    override fun getInputTypes(): Set<QualifiedContent.ContentType> {
        return TransformManager.CONTENT_CLASS
    }

    override fun getScopes(): MutableSet<in QualifiedContent.Scope> {
        return TransformManager.SCOPE_FULL_PROJECT
    }

    override fun isIncremental(): Boolean {
        return true
    }

    override fun getParameterInputs(): Map<String, Any>? {
        val inputs = HashMap<String, String>()
        if (extension.compilation != null) {
            inputs["compilation"] = extension.compilation.toString()
        }
        if (extension.runtime != null) {
            inputs["runtime"] = extension.runtime.toString()
        }
        return inputs
    }

    override fun getSecondaryFiles(): Collection<SecondaryFile> {
        if (extension.compilation != null) {
            val applyMethodMappingFile = File(extension.compilation!!.applyMethodMappingFilePath)
            val traceFilterFile = File(extension.compilation!!.traceFilterFilePath)
            val pathList = mutableListOf<String>()
            if (applyMethodMappingFile.exists()) {
                pathList.add(applyMethodMappingFile.absolutePath)
            }
            if (traceFilterFile.exists()) {
                pathList.add(traceFilterFile.absolutePath)
            }
            if (pathList.isNotEmpty()) {
                val files =
                    project.files(*pathList.toTypedArray())
                val secondaryFiles = mutableSetOf<SecondaryFile>()
                secondaryFiles.add(SecondaryFile.nonIncremental(files))
                return secondaryFiles
            }
        }

        return super.getSecondaryFiles()
    }

    override fun transform(transformInvocation: TransformInvocation) {
        super.transform(transformInvocation)
        val variantName = transformInvocation.context.variantName
        val mergedManifest =
            AGPCompat.getMergedManifestFile(project, variantName)
        val applicationName = ManifestUtil.readApplicationName(mergedManifest)

        val mappingFileDir = mappingFileMap[variantName]?.parentFile

        RheaLog.i(TAG, "mappingFile path: ${mappingFileDir?.absolutePath}")

        val mappingOutput = if (mappingFileDir == null) {
            Joiner.on(File.separatorChar).join(
                project.buildDir.absolutePath,
                AndroidProject.FD_OUTPUTS,
                "mapping",
                variantName
            )
        } else {
            mappingFileDir.absolutePath
        }

        val traceClassOutputDir = Joiner.on(File.separatorChar).join(
            project.buildDir.absolutePath,
            RheaConstants.RHEA_TRACE_ROOT,
            AndroidProject.FD_OUTPUTS,
            RheaConstants.TRACE_CLASS_DIR,
            variantName
        )

        val methodMappingFilePath = RheaFileUtils.getMethodMapFilePath(project, variantName)
        val ignoreMethodMappingFilePath = RheaFileUtils.getIgnoreMethodMapFilePath(project, variantName)

        val transformArgs = TransformArgs(
            applicationName,
            traceClassOutputDir,
            mappingOutput,
            methodMappingFilePath,
            ignoreMethodMappingFilePath,
            extension.compilation
        )

        transform(transformInvocation, transformArgs)
    }

    abstract fun transform(transformInvocation: TransformInvocation, transformArgs: TransformArgs)

}

class TransformArgs(
    val applicationName: String?,
    val traceClassOutputDir: String,
    val mappingOutputDir: String,
    val methodMappingFilePath: String,
    val ignoreMethodMappingFilePath: String,
    val traceCompilation: TraceCompilation?
)
