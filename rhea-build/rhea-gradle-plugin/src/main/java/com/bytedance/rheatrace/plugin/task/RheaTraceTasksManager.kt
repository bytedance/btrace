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

import com.android.build.gradle.api.ApplicationVariant
import com.android.build.gradle.internal.tasks.DexArchiveBuilderTask
import com.android.build.gradle.internal.tasks.factory.dependsOn
import com.bytedance.rheatrace.plugin.extension.RheaBuildExtension
import com.bytedance.rheatrace.plugin.internal.common.RheaLog
import org.gradle.api.Project
import org.gradle.api.Task

object RheaTraceTasksManager {

    private const val TAG: String = "RheaTraceTask"

    fun createMatrixTraceTask(
        project: Project,
        variant: ApplicationVariant,
        extension: RheaBuildExtension
    ) {
        val variantName = variant.name
        val action = RheaTraceTask.CreationAction(project, variant, extension);
        val traceTaskProvider =
            project.tasks.register(
                "rhea${variantName.capitalize()}Trace",
                RheaTraceTask::class.java,
                action
            )

        val minifyTask = "minify${variantName.capitalize()}WithProguard"
        try {
            val taskProvider = project.tasks.named(minifyTask)
            traceTaskProvider.dependsOn(taskProvider)
        } catch (e: Throwable) {
            e.printStackTrace()
        }
        val dexBuilderTaskName = "dexBuilder${variantName.capitalize()}"

        try {
            val dexBuilderProvider = project.tasks.named(dexBuilderTaskName)
            dexBuilderProvider.configure { task: Task ->
                traceTaskProvider.get().wired(task as DexArchiveBuilderTask)
            }
        } catch (e: Throwable) {
            RheaLog.e(
                TAG,
                "Do not find '$dexBuilderTaskName' task. Inject rhea trace task failed."
            )
        }
    }
}