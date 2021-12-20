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

package com.bytedance.rheatrace.plugin.internal.compat

import com.android.build.gradle.AppExtension
import com.bytedance.rheatrace.plugin.extension.RheaBuildExtension
import com.bytedance.rheatrace.plugin.internal.common.RheaLog
import com.bytedance.rheatrace.plugin.task.RheaTraceTasksManager
import com.bytedance.rheatrace.plugin.transform.RheaTraceLegacyTransform
import com.bytedance.rheatrace.plugin.transform.RheaTraceTransform
import org.gradle.api.Project

class RheaTraceCompat {

    companion object {
        const val TAG = "RheaTraceCompat"
    }

    fun inject(appExtension: AppExtension, project: Project, extension: RheaBuildExtension) {
        val enableR8 = "false" != project.properties["android.enableR8"]
        when {
            enableR8 -> {
                val transparentTransform = createTransparentTransform(appExtension, project, extension)
                project.afterEvaluate {
                    appExtension.applicationVariants.all { variant ->
                        transparentTransform.transparentMap[variant.name] = false
                    }
                }
            }
            VersionsCompat.lessThan(AGPVersion.AGP_3_6_0) ->
                legacyInject(appExtension, project, extension)
            VersionsCompat.lessThan(AGPVersion.AGP_4_0_0) -> {
                val transparentTransform = createTransparentTransform(appExtension, project, extension)
                project.afterEvaluate {
                    appExtension.applicationVariants.all { variant ->
                        transparentTransform.transparentMap[variant.name] = false
                    }
                }
            }
            VersionsCompat.greatThanOrEqual(AGPVersion.AGP_4_0_0) -> {
                val transparentTransform =
                    createTransparentTransform(appExtension, project, extension)
                project.afterEvaluate {
                    appExtension.applicationVariants.all { variant ->
                        transparentTransform.mappingFileMap[variant.name] = variant.mappingFile
                        if (variant.buildType.isMinifyEnabled) {
                            RheaTraceTasksManager.createMatrixTraceTask(project, variant, extension)
                        } else {
                            transparentTransform.transparentMap[variant.name] = false
                        }
                    }
                }
            }
            else ->
                RheaLog.e(
                    TAG,
                    "rheaTrace does not support Android Gradle Plugin %s",
                    VersionsCompat.androidGradlePluginVersion
                )
        }
    }

    private fun createTransparentTransform(
        appExtension: AppExtension,
        project: Project,
        extension: RheaBuildExtension
    ): RheaTraceTransform {
        val transparentTransform =
            RheaTraceTransform(
                project,
                extension
            )
        appExtension.registerTransform(transparentTransform)
        return transparentTransform
    }

    private fun legacyInject(
        appExtension: AppExtension,
        project: Project,
        extension: RheaBuildExtension
    ) {
        project.afterEvaluate {
            appExtension.applicationVariants.all {
                RheaTraceLegacyTransform.inject(
                    extension,
                    project,
                    it
                )
            }
        }
    }
}
