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
import com.bytedance.rheatrace.common.retrace.MappingCollector
import com.bytedance.rheatrace.common.utils.AGPCompat
import com.bytedance.rheatrace.common.utils.ManifestUtil
import com.bytedance.rheatrace.plugin.compiling.MethodCollector
import com.bytedance.rheatrace.plugin.compiling.ParseMappingTask
import com.bytedance.rheatrace.plugin.compiling.TraceMethod
import com.bytedance.rheatrace.plugin.compiling.filter.RheaTraceMethodFilter
import com.bytedance.rheatrace.plugin.extension.RheaBuildExtension
import com.bytedance.rheatrace.plugin.internal.RheaFileUtils
import com.bytedance.rheatrace.precise.PreciseInstrumentationContext
import com.bytedance.rheatrace.precise.extension.PreciseInstrumentationExtension
import com.ss.android.ugc.bytex.common.BaseContext
import com.ss.android.ugc.bytex.transformer.TransformEngine
import org.gradle.api.Project
import org.objectweb.asm.tree.ClassNode
import java.io.File
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger

/**
 * @author majun
 * @date 2022/2/22
 */
class RheaContext(project: Project, android: AppExtension, extension: RheaBuildExtension) :
    BaseContext<RheaBuildExtension>(project, android, extension) {
    private val TAG = "RheaContext"
    val collectedMethodMap = ConcurrentHashMap<String, TraceMethod>()
    val mappingCollector = MappingCollector()
    private val methodId = lazy {
        val methodCollectMethodFile = File(RheaFileUtils.getMethodMapFilePath(project, transformContext.variantName))
        if (methodCollectMethodFile.exists() && transformContext.isIncremental) {
            return@lazy AtomicInteger(RheaFileUtils.getFileLineCount(methodCollectMethodFile))
        }
        AtomicInteger()
    }
    lateinit var traceMethodFilter: RheaTraceMethodFilter
    private lateinit var methodCollector: MethodCollector

    private val preciseInstrumentationContext = lazy {
        val preciseInstrumentationExtension = PreciseInstrumentationExtension()
        preciseInstrumentationExtension.traceFilter = extension.compilation?.traceFilterFilePath!!
        val context = PreciseInstrumentationContext(project, android, preciseInstrumentationExtension)
        context.mappingCollector = mappingCollector
        context.transformContext = transformContext
        context.init()
        context
    }

    var applicationName = lazy {
        val mergedManifest = AGPCompat.getMergedManifestFile(project, transformContext.variantName)
        ManifestUtil.readApplicationName(mergedManifest)
    }

    override fun init() {
        super.init()
        val mappingTask = ParseMappingTask(this, methodId.value, extension.compilation?.applyMethodMappingFilePath, mappingCollector, collectedMethodMap)
        mappingTask.run()

        traceMethodFilter = RheaTraceMethodFilter(extension.compilation?.traceFilterFilePath, mappingCollector)
        methodCollector = MethodCollector(methodId.value, traceMethodFilter, collectedMethodMap, this)
    }

    fun traverse(relativePath: String, node: ClassNode) {
        if (traceMethodFilter.enablePreciseInstrumentation) {
            preciseInstrumentationContext.value.traverse(node)
        }
        methodCollector.collectMethod(node)
    }

    fun beforeTransform(engine: TransformEngine) {
        if (traceMethodFilter.enablePreciseInstrumentation) {
            preciseInstrumentationContext.value.beforeTransform()
            traceMethodFilter.updateConfig()
        }
        methodCollector.saveCollectMethod()
    }

    override fun releaseContext() {
        super.releaseContext()
        println(" $TAG : release")
        mappingCollector.release()
        methodCollector.release()
        collectedMethodMap.clear()
    }
}