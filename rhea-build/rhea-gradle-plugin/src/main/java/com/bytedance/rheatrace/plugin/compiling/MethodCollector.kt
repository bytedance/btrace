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

// modify by RheaTrace

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

package com.bytedance.rheatrace.plugin.compiling

import com.bytedance.rheatrace.common.retrace.MappingCollector
import com.bytedance.rheatrace.common.utils.RheaLog
import com.bytedance.rheatrace.plugin.RheaContext
import com.bytedance.rheatrace.plugin.compiling.filter.TraceMethodFilter
import com.bytedance.rheatrace.plugin.internal.RheaConstants
import com.bytedance.rheatrace.plugin.internal.RheaFileUtils
import org.objectweb.asm.Opcodes
import org.objectweb.asm.tree.ClassNode
import java.io.*
import java.util.*
import java.util.concurrent.*
import java.util.concurrent.atomic.AtomicInteger

class MethodCollector(
    private val methodId: AtomicInteger,
    private val traceMethodFilter: TraceMethodFilter,
    private val collectedMethodMap: ConcurrentHashMap<String, TraceMethod>,
    private val rheaContext: RheaContext
) {
    private val executor: ExecutorService = Executors.newFixedThreadPool(16)

    companion object {
        private const val TAG = "MethodCollector"
    }

    private val classNodeList = arrayListOf<ClassNode>()

    var collectedClassExtendMap = ConcurrentHashMap<String, String>()

    var collectedClassImplementMap = ConcurrentHashMap<String, List<String>>()

    var collectedIgnoreMethodMap = ConcurrentHashMap<String, TraceMethod?>()

    var ignoreCount = AtomicInteger()

    var incrementCount = AtomicInteger()

    @Throws(ExecutionException::class, InterruptedException::class)
    fun saveCollectMethod() {
        RheaLog.i(TAG, "[saveCollectMethod] start")
        RheaLog.i(TAG, "[saveCollectMethod] classNodeList :${classNodeList.size}")
        val futures: MutableList<Future<*>> = LinkedList()
        classNodeList.forEach { classNode: ClassNode ->
            try {
                futures.add(executor.submit(CollectMethodTask(classNode)))
            } catch (e: Throwable) {
                RheaLog.e(TAG, "[saveCollectMethod] ${e.message}")
            }
        }

        for (future in futures) {
            future.get()
        }
        futures.add(executor.submit { saveIgnoreCollectedMethod(traceMethodFilter.mappingCollector) })
        futures.add(executor.submit { saveCollectedMethod(traceMethodFilter.mappingCollector) })
        for (future in futures) {
            future.get()
        }
        futures.clear()
    }

    internal inner class CollectMethodTask(var classNode: ClassNode) : Runnable {
        override fun run() {
            try {
                classNode.methods.forEach {
                    val traceMethod: TraceMethod = TraceMethod.create(0, it.access, classNode.name, it.name, it.desc)
                    val needFilter: Boolean = traceMethodFilter.needFilter(it, traceMethod, classNode)
                    if (needFilter) {
                        if (!collectedIgnoreMethodMap.containsKey(traceMethod.getFullMethodName())) {
                            ignoreCount.incrementAndGet()
                            collectedIgnoreMethodMap[traceMethod.getFullMethodName()] = traceMethod
                        }
                    } else {
                        if (!collectedMethodMap.containsKey(traceMethod.getFullMethodName())) {
                            traceMethod.id = methodId.incrementAndGet()
                            incrementCount.incrementAndGet()
                            collectedMethodMap[traceMethod.getFullMethodName()] = traceMethod
                        }
                    }
                }
            } catch (e: Exception) {
                RheaLog.e(TAG, "[saveCollectMethod] error:" + e.message)
            }
        }

    }

    private fun saveIgnoreCollectedMethod(mappingCollector: MappingCollector) {
        val methodMapFile = File(RheaFileUtils.getIgnoreMethodMapFilePath(rheaContext.project, rheaContext.transformContext.variantName))
        if (!methodMapFile.parentFile.exists()) {
            methodMapFile.parentFile.mkdirs()
        }
        val ignoreMethodList: MutableList<TraceMethod?> = ArrayList()
        ignoreMethodList.addAll(collectedIgnoreMethodMap.values)

        RheaLog.i(
            TAG,
            "[saveIgnoreCollectedMethod] size:%s path:%s",
            collectedIgnoreMethodMap.size,
            methodMapFile.absolutePath
        )

        ignoreMethodList.sortWith(Comparator { method0, method1 -> method0!!.className.compareTo(method1!!.className) })

        var pw: PrintWriter? = null
        try {
            val fileOutputStream = FileOutputStream(methodMapFile, rheaContext.transformContext.isIncremental)
            val w: Writer = OutputStreamWriter(fileOutputStream, "UTF-8")
            pw = PrintWriter(w)
            pw.println("ignore methods:")
            for (traceMethod in ignoreMethodList) {
                traceMethod?.apply {
                    this.revert(mappingCollector)
                    pw.println(this.toIgnoreString())
                }

            }
        } catch (e: Exception) {
            RheaLog.e(TAG, "write method map Exception:%s", e.message)
            e.printStackTrace()
        } finally {
            if (pw != null) {
                pw.flush()
                pw.close()
            }
        }
    }

    private fun saveCollectedMethod(mappingCollector: MappingCollector) {
        val methodMapFile = File(RheaFileUtils.getMethodMapFilePath(rheaContext.project, rheaContext.transformContext.variantName))
        if (!methodMapFile.parentFile.exists()) {
            methodMapFile.parentFile.mkdirs()
        }
        val methodList: MutableList<TraceMethod?> = ArrayList()

        val extra: TraceMethod = TraceMethod.create(
            RheaConstants.METHOD_ID_DISPATCH,
            Opcodes.ACC_PUBLIC,
            "android.os.Handler",
            "dispatchMessage",
            "(Landroid.os.Message;)V"
        )
        collectedMethodMap[extra.getFullMethodName()] = extra
        methodList.addAll(collectedMethodMap.values)
        RheaLog.i(
            TAG,
            "[saveCollectedMethod] size:%s incrementCount:%s path:%s",
            collectedMethodMap.size,
            incrementCount.get(),
            methodMapFile.absolutePath
        )
        Collections.sort(methodList, Comparator<TraceMethod?> { o1, o2 -> o1!!.id - o2!!.id })
        var pw: PrintWriter? = null
        try {
            val fileOutputStream = FileOutputStream(methodMapFile, rheaContext.transformContext.isIncremental)
            val w: Writer = OutputStreamWriter(fileOutputStream, "UTF-8")
            pw = PrintWriter(w)
            pw.println("#" + UUID.randomUUID().toString())
            for (traceMethod in methodList) {
                traceMethod?.apply {
                    traceMethod.revert(mappingCollector)
                    pw.println(traceMethod.toString())
                }

            }
        } catch (e: Exception) {
            RheaLog.e(TAG, "write method map Exception:%s", e.message)
            e.printStackTrace()
        } finally {
            if (pw != null) {
                pw.flush()
                pw.close()
            }
        }
    }

    fun collectMethod(classNode: ClassNode) {
        if (classNode.superName != null) {
            collectedClassExtendMap[classNode.name] = classNode.superName
        }
        if (classNode.interfaces != null) {
            collectedClassImplementMap[classNode.name] = classNode.interfaces
        }
        synchronized(this) {
            classNodeList.add(classNode)
        }
    }

    fun release() {
        classNodeList.clear()
        collectedClassExtendMap.clear()
        collectedClassImplementMap.clear()
        collectedIgnoreMethodMap.clear()
    }
}
