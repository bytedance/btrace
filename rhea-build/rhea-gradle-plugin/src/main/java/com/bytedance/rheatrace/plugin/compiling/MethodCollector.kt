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

import com.bytedance.rheatrace.plugin.compiling.filter.TraceMethodFilter
import com.bytedance.rheatrace.plugin.internal.common.RheaConstants
import com.bytedance.rheatrace.plugin.internal.common.RheaLog
import com.bytedance.rheatrace.plugin.retrace.MappingCollector
import org.objectweb.asm.*
import org.objectweb.asm.tree.MethodNode
import java.io.*
import java.util.*
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ExecutionException
import java.util.concurrent.ExecutorService
import java.util.concurrent.Future
import java.util.concurrent.atomic.AtomicInteger
import java.util.zip.ZipFile

class MethodCollector(
    private val methodId: AtomicInteger,
    private val executor: ExecutorService,
    private val methodMapFilePath: String,
    private val ignoreMethodMapFilePath: String,
    private val traceMethodFilter: TraceMethodFilter,
    private val collectedMethodMap: ConcurrentHashMap<String, TraceMethod>
) {

    companion object {
        private const val TAG = "MethodCollector"
    }

    var collectedClassExtendMap = ConcurrentHashMap<String, String>()

    var collectedIgnoreMethodMap = ConcurrentHashMap<String, TraceMethod?>()

    var ignoreCount = AtomicInteger()

    var incrementCount = AtomicInteger()

    @Throws(ExecutionException::class, InterruptedException::class)
    fun collect(srcFolderList: Set<File>, dependencyJarList: Set<File?>) {
        val futures: MutableList<Future<*>> = LinkedList()
        for (srcFile in srcFolderList) {
            val classFileList = ArrayList<File>()
            if (srcFile.isDirectory) {
                listClassFiles(classFileList, srcFile)
            } else {
                classFileList.add(srcFile)
            }
            for (classFile in classFileList) {
                futures.add(executor.submit(CollectSrcTask(classFile)))
            }
        }
        for (jarFile in dependencyJarList) {
            futures.add(executor.submit(CollectJarTask(jarFile!!)))
        }
        for (future in futures) {
            future.get()
        }
        futures.clear()
        futures.add(executor.submit { saveIgnoreCollectedMethod(traceMethodFilter.mappingCollector) })
        futures.add(executor.submit { saveCollectedMethod(traceMethodFilter.mappingCollector) })
        for (future in futures) {
            future.get()
        }
        futures.clear()
    }

    internal inner class CollectSrcTask(var classFile: File) : Runnable {
        override fun run() {
            var inputStream: InputStream? = null
            try {
                inputStream = FileInputStream(classFile)
                val classReader = ClassReader(inputStream)
                val classWriter = ClassWriter(ClassWriter.COMPUTE_MAXS)
                val visitor: ClassVisitor = TraceClassAdapter(Opcodes.ASM5, classWriter)
                classReader.accept(visitor, 0)
            } catch (e: Exception) {
                e.printStackTrace()
            } finally {
                try {
                    inputStream!!.close()
                } catch (ignored: Exception) {
                    //do nothing
                }
            }
        }

    }

    internal inner class CollectJarTask(var fromJar: File) : Runnable {
        override fun run() {
            var zipFile: ZipFile? = null
            try {
                zipFile = ZipFile(fromJar)
                val enumeration = zipFile.entries()
                while (enumeration.hasMoreElements()) {
                    val zipEntry = enumeration.nextElement()
                    val zipEntryName = zipEntry.name
                    try {
                        if (zipEntryName.endsWith(".class")) {
                            val inputStream = zipFile.getInputStream(zipEntry)
                            val classReader = ClassReader(inputStream)
                            val classWriter = ClassWriter(ClassWriter.COMPUTE_MAXS)
                            val visitor: ClassVisitor = TraceClassAdapter(Opcodes.ASM5, classWriter)
                            classReader.accept(visitor, 0)
                        }
                    } catch (error: Exception) {
                        RheaLog.e(
                            TAG,
                            "Failed to read class:%s, e:%s",
                            zipEntryName,
                            error
                        )
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            } finally {
                try {
                    zipFile!!.close()
                } catch (e: Exception) {
                    RheaLog.e(TAG, "close stream err! fromJar:%s", fromJar.absolutePath)
                }
            }
        }

    }

    private fun saveIgnoreCollectedMethod(mappingCollector: MappingCollector) {
        val methodMapFile = File(ignoreMethodMapFilePath)
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
            val fileOutputStream = FileOutputStream(methodMapFile, false)
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
        val methodMapFile = File(methodMapFilePath)
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
            val fileOutputStream = FileOutputStream(methodMapFile, false)
            val w: Writer = OutputStreamWriter(fileOutputStream, "UTF-8")
            pw = PrintWriter(w)
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

    private inner class TraceClassAdapter(i: Int, classVisitor: ClassVisitor) :
        ClassVisitor(i, classVisitor) {

        private var className: String? = null

        private var isABSClass = false

        override fun visit(
            version: Int,
            access: Int,
            name: String,
            signature: String?,
            superName: String,
            interfaces: Array<String>?
        ) {
            super.visit(version, access, name, signature, superName, interfaces)
            className = name
            if (access and Opcodes.ACC_ABSTRACT > 0 || access and Opcodes.ACC_INTERFACE > 0) {
                isABSClass = true
            }
            className?.apply {
                collectedClassExtendMap[this] = superName
            }
        }

        override fun visitMethod(
            access: Int, name: String, desc: String,
            signature: String?, exceptions: Array<String>?
        ): MethodVisitor {
            return if (isABSClass) {
                super.visitMethod(access, name, desc, signature, exceptions)
            } else {
                CollectMethodNode(className!!, access, name, desc, signature, exceptions)
            }
        }
    }

    private inner class CollectMethodNode(
        private val className: String,
        access: Int, name: String?, desc: String?,
        signature: String?, exceptions: Array<String>?
    ) : MethodNode(Opcodes.ASM5, access, name, desc, signature, exceptions) {

        override fun visitEnd() {
            super.visitEnd()
            val traceMethod: TraceMethod = TraceMethod.create(0, access, className, name, desc)
            val needFilter: Boolean =
                traceMethodFilter.onClassNeedFilter(className) || traceMethodFilter.onMethodNeedFilter(this, traceMethod)
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
    }

    private fun listClassFiles(classFiles: ArrayList<File>, folder: File) {
        val files = folder.listFiles()
        if (null == files) {
            RheaLog.e(TAG, "[listClassFiles] files is null! %s", folder.absolutePath)
            return
        }
        for (file in files) {
            if (file == null) {
                continue
            }
            if (file.isDirectory) {
                listClassFiles(classFiles, file)
            } else if (file.name.endsWith(".class")) {
                classFiles.add(file)
            }
        }
    }
}
