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

package com.bytedance.rheatrace.plugin

import com.android.build.gradle.AppExtension
import com.bytedance.rheatrace.common.utils.RheaLog
import com.bytedance.rheatrace.plugin.compiling.RheaTraceClassVisitor
import com.bytedance.rheatrace.plugin.extension.RheaBuildExtension
import com.bytedance.rheatrace.plugin.extension.TraceCompilation
import com.bytedance.rheatrace.plugin.extension.TraceRuntime
import com.bytedance.rheatrace.plugin.internal.CopyMappingTask.registerTaskSaveMappingToAssets
import com.ss.android.ugc.bytex.common.CommonPlugin
import com.ss.android.ugc.bytex.common.flow.main.Process
import com.ss.android.ugc.bytex.common.visitor.ClassVisitorChain
import com.ss.android.ugc.bytex.transformer.TransformEngine
import com.ss.android.ugc.bytex.transformer.cache.FileData
import org.gradle.api.Project
import org.gradle.api.plugins.ExtensionAware
import org.objectweb.asm.ClassReader
import org.objectweb.asm.tree.ClassNode

/**
 * @author majun
 * @date 2022/3/10
 */
class RheaTracePlugin : CommonPlugin<RheaBuildExtension, RheaContext>() {
    private val TAG = "RheaTracePlugin"

    override fun onApply(project: Project) {
        super.onApply(project)
        RheaLog.i(TAG, "Rhea Plugin 2.0")
        extension.runtime = (extension as ExtensionAware).extensions.create("runtime", TraceRuntime::class.java)
        extension.compilation = (extension as ExtensionAware).extensions.create("compilation", TraceCompilation::class.java)
        registerTaskSaveMappingToAssets(project, extension)
    }

    override fun getContext(
        project: Project,
        android: AppExtension,
        extension: RheaBuildExtension
    ): RheaContext {
        return RheaContext(project, android, extension)
    }

    override fun hookTransformName(): String {
        return "dexBuilder"
    }

    override fun hookTask(): Boolean {
        return if (project.gradle.startParameter.taskNames.toString().contains("release", true)) {
            RheaLog.i(TAG, "project is release,Rhea Plugin hook dexBuilder task")
            true
        } else {
            RheaLog.i(TAG, "project is debug,Rhea Plugin do not hook dexBuilder task")
            false
        }
    }

    /**
     * Collect ClassNode
     */
    override fun traverse(relativePath: String, node: ClassNode) {
        super.traverse(relativePath, node)
        context.traverse(relativePath, node)
    }

    override fun traverseIncremental(fileData: FileData, node: ClassNode?) {
        super.traverseIncremental(fileData, node)
        if (node != null) {
            context.traverse(fileData.relativePath, node)
        }
    }

    /**
     * The methods are pretreated to determine which methods need to be piled
     */
    override fun beforeTransform(engine: TransformEngine) {
        super.beforeTransform(engine)
        context.beforeTransform(engine)
    }

    /**
     * Pile the collected methods
     */
    override fun transform(relativePath: String, chain: ClassVisitorChain): Boolean {
        chain.connect(RheaTraceClassVisitor(context))
        return super.transform(relativePath, chain)
    }

    override fun flagForClassReader(process: Process?): Int {
        return ClassReader.EXPAND_FRAMES
    }
}