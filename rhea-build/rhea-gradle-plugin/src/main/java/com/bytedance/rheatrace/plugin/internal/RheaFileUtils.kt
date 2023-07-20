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

import com.android.builder.model.AndroidProject
import com.google.common.base.Joiner
import org.gradle.api.Project
import java.io.File

/**
 * Created by majun.oreo on 2023/3/21
 * @author majun.oreo@bytedance.com
 */
object RheaFileUtils {
    const val MethodMappingFileName = "methodMapping.txt"
    private const val IgnoreMethodMappingFileName = "ignoreMethodMapping.txt"
    fun getMethodMapFilePath(project: Project, variantName: String): String {
        return "${getRheaOutDir(project, variantName)}/$MethodMappingFileName"
    }

    fun getIgnoreMethodMapFilePath(project: Project, variantName: String): String {
        return "${getRheaOutDir(project, variantName)}/$IgnoreMethodMappingFileName"
    }

    private fun getRheaOutDir(project: Project, variantName: String): String {
        return Joiner.on(File.separatorChar).join(
            project.buildDir.absolutePath, AndroidProject.FD_OUTPUTS, "rhea", variantName
        )
    }

    fun getFileLineCount(file: File) : Int {
        val input = file.bufferedReader()
        var count = 0
        input.useLines { it.forEach { _ -> count++ } }
        return count
    }
}