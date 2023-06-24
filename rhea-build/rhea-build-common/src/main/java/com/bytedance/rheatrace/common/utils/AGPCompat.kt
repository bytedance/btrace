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

package com.bytedance.rheatrace.common.utils

import com.android.SdkConstants
import com.bytedance.rheatrace.common.ReflectUtil

import org.gradle.api.GradleException
import org.gradle.api.Project
import org.gradle.api.Task
import org.gradle.api.file.DirectoryProperty
import org.gradle.api.internal.file.DefaultFilePropertyFactory
import java.io.File
import java.lang.reflect.Method

object AGPCompat {

    fun getMergedManifestFile(project: Project, variantName: String): File {
        val mergeManifestTaskName = "process${variantName.capitalize()}Manifest"
        val processManifest = project.tasks.findByName(mergeManifestTaskName)
        return getMergedManifestFileInternal(processManifest)
    }

    private fun getMergedManifestFileInternal(processManifestTask: Task?): File {
        var manifestOutputFile: File? = null
        var manifestOutputBaseDir: String?
        var method: Method
        try {
            //compat for less than 4.0
            method = ReflectUtil.getDeclaredMethodRecursive(
                "com.android.build.gradle.tasks.ManifestProcessorTask",
                "getManifestOutputDirectory"
            )
            manifestOutputBaseDir =
                (method.invoke(processManifestTask) as DirectoryProperty).get().toString()
            manifestOutputFile = File(manifestOutputBaseDir, SdkConstants.ANDROID_MANIFEST_XML)
        } catch (e1: Throwable) {
            //compat for great than or equal to 4.0
            try {
                method = ReflectUtil.getDeclaredMethodRecursive(
                    "com.android.build.gradle.tasks.ProcessMultiApkApplicationManifest",
                    "getMainMergedManifest"
                )
                manifestOutputFile =
                    (method.invoke(processManifestTask) as DefaultFilePropertyFactory.DefaultRegularFileVar).asFile.get()
            } catch (e2: Throwable) {
                // maybe AndroidTest
                try {
                    method = ReflectUtil.getDeclaredMethodRecursive(
                        "com.android.build.gradle.tasks.ProcessTestManifest",
                        "getPackagedManifestOutputDirectory"
                    )
                    manifestOutputBaseDir =
                        ((method.invoke(processManifestTask)) as DirectoryProperty).get().toString()
                    manifestOutputFile =
                        File(manifestOutputBaseDir, SdkConstants.ANDROID_MANIFEST_XML)
                } catch (e3: Throwable) {

                }
            }
        }
        if (manifestOutputFile == null) {
            throw GradleException("Can't find merged manifest!")
        }
        return manifestOutputFile
    }
}