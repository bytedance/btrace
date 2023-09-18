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
import com.bytedance.rheatrace.common.utils.FileUtil
import com.bytedance.rheatrace.common.utils.RheaLog
import com.bytedance.rheatrace.plugin.compiling.filter.TraceMethodFilter
import com.bytedance.rheatrace.plugin.extension.TraceCompilation
import com.bytedance.rheatrace.plugin.internal.compat.VersionsCompat
import org.objectweb.asm.ClassReader
import org.objectweb.asm.ClassVisitor
import org.objectweb.asm.ClassWriter
import java.io.ByteArrayInputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.InputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.util.LinkedList
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ExecutionException
import java.util.concurrent.ExecutorService
import java.util.concurrent.Future
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

/**
 * Created by majun.oreo on 2023/6/30
 * @author majun.oreo@bytedance.com
 */
open class MethodTracer(
    private val executor: ExecutorService,
    private var mappingCollector: MappingCollector,
    private val collectedMethodMap: ConcurrentHashMap<String, TraceMethod>,
    private val traceCompilation: TraceCompilation,
    private val applicationName: String?,
    private val traceMethodFilter: TraceMethodFilter
) {

    companion object {
        private const val TAG = "MethodTracer"
    }

    @Throws(ExecutionException::class, InterruptedException::class)
    fun trace(srcFolderList: Map<File, File>?, dependencyJarList: Map<File, File>?) {
        val futures: MutableList<Future<*>> = LinkedList()
        traceMethodFromSrc(srcFolderList, futures)
        traceMethodFromJar(dependencyJarList, futures)
        for (future in futures) {
            future.get()
        }
        futures.clear()
    }

    private fun traceMethodFromSrc(srcMap: Map<File, File>?, futures: MutableList<Future<*>>) {
        if (null != srcMap) {
            for ((key, value) in srcMap) {
                futures.add(executor.submit { innerTraceMethodFromSrc(key, value) })
            }
        }
    }

    private fun traceMethodFromJar(
        dependencyMap: Map<File, File>?,
        futures: MutableList<Future<*>>
    ) {
        if (null != dependencyMap) {
            for ((key, value) in dependencyMap) {
                futures.add(executor.submit { innerTraceMethodFromJar(key, value) })
            }
        }
    }

    private fun innerTraceMethodFromSrc(input: File, output: File) {
        val classFileList = ArrayList<File>()
        if (input.isDirectory) {
            listClassFiles(classFileList, input)
        } else {
            classFileList.add(input)
        }
        for (classFile in classFileList) {
            var inputStream: InputStream? = null
            var outputStream: FileOutputStream? = null
            try {
                val changedFileOutput = getChangedFileOutput(classFile, input, output)
                inputStream = FileInputStream(classFile)
                val classReader = ClassReader(inputStream)
                val classWriter = ClassWriter(ClassWriter.COMPUTE_FRAMES)
                val classVisitor: ClassVisitor =
                    RheaTraceClassVisitor(
                        VersionsCompat.asmApi,
                        classWriter,
                        mappingCollector,
                        collectedMethodMap,
                        applicationName,
                        traceCompilation,
                        traceMethodFilter
                    )
                classReader.accept(classVisitor, ClassReader.EXPAND_FRAMES)
                inputStream.close()
                outputStream = if (output.isDirectory) {
                    FileOutputStream(changedFileOutput)
                } else {
                    FileOutputStream(output)
                }
                outputStream.write(classWriter.toByteArray())
                outputStream.close()
            } catch (e: Exception) {
                RheaLog.e(TAG, "[innerTraceMethodFromSrc] input:${input.name} e:${e}")
                RheaLog.e(TAG, "[innerTraceMethodFromSrc] classFile:${classFile}")
                try {
                    val changedFileOutput = getChangedFileOutput(classFile, input, output)
                    Files.copy(
                        classFile.toPath(),
                        changedFileOutput.toPath(),
                        StandardCopyOption.REPLACE_EXISTING
                    )
                } catch (e1: Exception) {
                    e1.printStackTrace()
                }
            } finally {
                try {
                    inputStream!!.close()
                    outputStream!!.close()
                } catch (e: Exception) {
                    // ignore
                }
            }
        }
    }

    private fun getChangedFileOutput(classFile: File, input: File, output: File): File {
        val changedFileInputFullPath = classFile.absolutePath
        val changedFileOutput = File(
            changedFileInputFullPath.replace(input.absolutePath, output.absolutePath)
        )
        if (!changedFileOutput.exists()) {
            changedFileOutput.parentFile.mkdirs()
        }
        changedFileOutput.createNewFile()
        return changedFileOutput
    }
    private fun innerTraceMethodFromJar(input: File, output: File) {
        var zipOutputStream: ZipOutputStream? = null
        var inputZipFile: ZipFile? = null
        try {
            zipOutputStream = ZipOutputStream(FileOutputStream(output))
            inputZipFile = ZipFile(input)
            val enumeration = inputZipFile.entries()
            while (enumeration.hasMoreElements()) {
                val inputZipEntry = enumeration.nextElement()
                val zipEntryName = inputZipEntry.name
                if (zipEntryName.endsWith(".class")) {
                    try {
                        val inputStream = inputZipFile.getInputStream(inputZipEntry)
                        val classReader = ClassReader(inputStream)
                        val classWriter = ClassWriter(ClassWriter.COMPUTE_FRAMES)
                        val classVisitor: ClassVisitor =
                            RheaTraceClassVisitor(
                                VersionsCompat.asmApi,
                                classWriter,
                                mappingCollector,
                                collectedMethodMap,
                                applicationName,
                                traceCompilation,
                                traceMethodFilter
                            )
                        classReader.accept(classVisitor, ClassReader.EXPAND_FRAMES)
                        val data = classWriter.toByteArray()

                        // 创建新的 ZipEntry 并将数据写入
                        val newZipEntry = ZipEntry(zipEntryName)
                        zipOutputStream.putNextEntry(newZipEntry)
                        zipOutputStream.write(data)
                        zipOutputStream.closeEntry()
                    } catch (e: Throwable) {
                        RheaLog.e(
                            TAG,
                            "Failed to trace class: %s, e: %s",
                            inputZipEntry.name,
                            e
                        )
                    }
                } else {
                    // 直接将条目复制到输出压缩包中
                    val newZipEntry = ZipEntry(zipEntryName)
                    zipOutputStream.putNextEntry(newZipEntry)
                    val inputStream = inputZipFile.getInputStream(inputZipEntry)
                    inputStream.copyTo(zipOutputStream)
                    zipOutputStream.closeEntry()
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
            RheaLog.e(
                TAG,
                "[innerTraceMethodFromJar] input: %s output: %s e: %s",
                input.name,
                output,
                e
            )
        } finally {
            try {
                zipOutputStream?.close()
            } catch (e: Exception) {
                // 处理关闭异常
            }
            try {
                inputZipFile?.close()
            } catch (e: Exception) {
                // 处理关闭异常
            }
        }
    }


    private fun listClassFiles(
        classFiles: ArrayList<File>,
        folder: File
    ) {
        val files = folder.listFiles()
        if (null == files) {
            RheaLog.e(
                TAG,
                "[listClassFiles] files is null! %s",
                folder.absolutePath
            )
            return
        }
        for (file in files) {
            if (file == null) {
                continue
            }
            if (file.isDirectory) {
                listClassFiles(classFiles, file)
            } else {
                if (file.isFile) {
                    classFiles.add(file)
                }
            }
        }
    }
}