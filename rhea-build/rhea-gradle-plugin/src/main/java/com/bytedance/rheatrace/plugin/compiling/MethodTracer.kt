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

package com.bytedance.rheatrace.plugin.compiling

import com.bytedance.rheatrace.plugin.compiling.filter.TraceMethodFilter
import com.bytedance.rheatrace.plugin.extension.TraceCompilation
import com.bytedance.rheatrace.plugin.extension.TraceRuntime
import com.bytedance.rheatrace.plugin.internal.common.FileUtil
import com.bytedance.rheatrace.plugin.internal.common.RheaConstants
import com.bytedance.rheatrace.plugin.internal.common.RheaConstants.ANEWARRAY_TYPE
import com.bytedance.rheatrace.plugin.internal.common.RheaLog
import com.bytedance.rheatrace.plugin.retrace.MappingCollector
import org.objectweb.asm.*
import org.objectweb.asm.commons.AdviceAdapter
import java.io.*
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.util.*
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ExecutionException
import java.util.concurrent.ExecutorService
import java.util.concurrent.Future
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream
import kotlin.collections.ArrayList

/**
 * Created by caichongyang on 2017/6/4.
 *
 *
 * This class hooks all collected methods in oder to trace method in/out.
 *
 */
open class MethodTracer(
    private val executor: ExecutorService,
    private var mappingCollector: MappingCollector,
    private val collectedMethodMap: ConcurrentHashMap<String, TraceMethod>,
    private val traceCompilation: TraceCompilation,
    private val applicationName: String?,
    private val traceRuntime: TraceRuntime?,
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
                val classWriter = ClassWriter(ClassWriter.COMPUTE_MAXS)
                val classVisitor: ClassVisitor =
                    RheaTraceClassVisitor(
                        Opcodes.ASM5,
                        classWriter,
                        mappingCollector,
                        collectedMethodMap,
                        applicationName,
                        traceCompilation,
                        traceRuntime,
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
        var zipFile: ZipFile? = null
        try {
            zipOutputStream = ZipOutputStream(FileOutputStream(output))
            zipFile = ZipFile(input)
            val enumeration = zipFile.entries()
            while (enumeration.hasMoreElements()) {
                try {
                    val zipEntry = enumeration.nextElement()
                    val zipEntryName = zipEntry.name
                    if (zipEntryName.endsWith(".class")) {
                        val inputStream = zipFile.getInputStream(zipEntry)
                        val classReader = ClassReader(inputStream)
                        val classWriter = ClassWriter(ClassWriter.COMPUTE_MAXS)
                        val classVisitor: ClassVisitor =
                            RheaTraceClassVisitor(
                                Opcodes.ASM5,
                                classWriter,
                                mappingCollector,
                                collectedMethodMap,
                                applicationName,
                                traceCompilation,
                                traceRuntime,
                                traceMethodFilter
                            )
                        classReader.accept(classVisitor, ClassReader.EXPAND_FRAMES)
                        val data = classWriter.toByteArray()
                        val byteArrayInputStream: InputStream = ByteArrayInputStream(data)
                        val newZipEntry = ZipEntry(zipEntryName)
                        FileUtil.addZipEntry(zipOutputStream, newZipEntry, byteArrayInputStream)
                    }
                } catch (e: Throwable) {
                    RheaLog.e(
                        TAG,
                        "Failed to trace class:%s, e:%s",
                        enumeration.nextElement().name,
                        e
                    )
                }
            }
        } catch (e: Exception) {
            RheaLog.e(
                TAG,
                "[innerTraceMethodFromJar] input:%s output:%s e:%s",
                input.name,
                output,
                e
            )
            try {
                Files.copy(
                    input.toPath(),
                    output.toPath(),
                    StandardCopyOption.REPLACE_EXISTING
                )
            } catch (e1: Exception) {
                e1.printStackTrace()
            }
        } finally {
            try {
                if (zipOutputStream != null) {
                    zipOutputStream.finish()
                    zipOutputStream.flush()
                    zipOutputStream.close()
                }
                zipFile?.close()
            } catch (e: Exception) {
                RheaLog.e(TAG, "close stream err!")
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

class RheaTraceClassVisitor(
    i: Int,
    classVisitor: ClassVisitor,
    private val mappingCollector: MappingCollector,
    private var collectedMethodMap: ConcurrentHashMap<String, TraceMethod>,
    private val applicationName: String?,
    private val traceCompilation: TraceCompilation,
    private val traceRuntime: TraceRuntime?,
    private val traceMethodFilter: TraceMethodFilter
) :
    ClassVisitor(i, classVisitor) {

    private var superName = ""

    private lateinit var className: String

    private var isABSClass = false

    private var isApplication = false

    private var needCreateOnCreate = true

    private var needCreateAttachBaseContext = true

    override fun visit(
        version: Int,
        access: Int,
        name: String,
        signature: String?,
        superName: String?,
        interfaces: Array<String>?
    ) {
        super.visit(version, access, name, signature, superName, interfaces)
        this.className = name
        if (access and Opcodes.ACC_ABSTRACT > 0 || access and Opcodes.ACC_INTERFACE > 0) {
            isABSClass = true
        }
        if (!superName.isNullOrEmpty()) {
            this.superName = superName
        }
        this.isApplication = name.replace("/", ".") == applicationName
    }

    override fun visitMethod(
        access: Int,
        name: String,
        desc: String,
        signature: String?,
        exceptions: Array<String>?
    ): MethodVisitor {
        if (isABSClass) {
            return super.visitMethod(access, name, desc, signature, exceptions)
        } else {
            val mv = cv.visitMethod(access, name, desc, signature, exceptions)
            if (isApplication) {
                if (RheaConstants.METHOD_attachBaseContext == name) {
                    needCreateAttachBaseContext = false
                    injectAttachBaseContextForRheaTrace(mv)
                }
                if (RheaConstants.METHOD_onCreate == name) {
                    needCreateOnCreate = false
                    injectOnCreateForRheaTrace(mv)
                }
            }
            return TraceMethodAdapter(
                api,
                mv,
                access,
                name,
                desc,
                className,
                mappingCollector,
                collectedMethodMap,
                traceCompilation,
                traceRuntime,
                traceMethodFilter
            )
        }
    }

    override fun visitEnd() {
        super.visitEnd()
        if (isApplication && needCreateAttachBaseContext) {
            createAttachBaseContextForApplication()
        }
        if (isApplication && needCreateOnCreate) {
            createOnCreateForApplication()
        }
    }

    private fun createAttachBaseContextForApplication() {
        val mv = cv.visitMethod(
            Opcodes.ACC_PROTECTED,
            RheaConstants.METHOD_attachBaseContext,
            RheaConstants.DESC_attachBaseContext,
            null,
            null
        )
        mv.visitVarInsn(Opcodes.ALOAD, 0)
        mv.visitVarInsn(Opcodes.ALOAD, 1)
        mv.visitMethodInsn(
            Opcodes.INVOKESPECIAL,
            superName,
            RheaConstants.METHOD_attachBaseContext,
            RheaConstants.DESC_attachBaseContext,
            false
        )
        injectAttachBaseContextForRheaTrace(mv)
        mv.visitInsn(Opcodes.RETURN)
        mv.visitEnd()
    }

    private fun injectAttachBaseContextForRheaTrace(mv: MethodVisitor) {
        mv.visitVarInsn(Opcodes.ALOAD, 1)
        mv.visitMethodInsn(
            Opcodes.INVOKESTATIC,
            RheaConstants.CLASS_ApplicationLike,
            RheaConstants.METHOD_attachBaseContext,
            RheaConstants.DESC_attachBaseContext,
            false
        )
    }

    private fun createOnCreateForApplication() {
        val mv = cv.visitMethod(
            Opcodes.ACC_PROTECTED,
            RheaConstants.METHOD_onCreate,
            RheaConstants.DESC_onCreate,
            null,
            null
        )
        mv.visitVarInsn(Opcodes.ALOAD, 0)
        mv.visitMethodInsn(
            Opcodes.INVOKESPECIAL,
            superName,
            RheaConstants.METHOD_onCreate,
            "()V",
            false
        )
        injectOnCreateForRheaTrace(mv)
        mv.visitInsn(Opcodes.RETURN)
        mv.visitEnd()
    }

    private fun injectOnCreateForRheaTrace(mv: MethodVisitor) {
        mv.visitVarInsn(Opcodes.ALOAD, 0)
        mv.visitMethodInsn(
            Opcodes.INVOKESTATIC,
            RheaConstants.CLASS_ApplicationLike,
            RheaConstants.METHOD_onCreate,
            RheaConstants.DESC_onCreate,
            false
        )
    }
}

class TraceMethodAdapter(
    api: Int,
    mv: MethodVisitor,
    access: Int,
    name: String,
    desc: String,
    private val className: String,
    private val mappingCollector: MappingCollector,
    private val collectedMethodMap: ConcurrentHashMap<String, TraceMethod>,
    private val traceCompilation: TraceCompilation,
    private val traceRuntime: TraceRuntime?,
    private val traceMethodFilter: TraceMethodFilter
) : AdviceAdapter(api, mv, access, name, desc) {

    companion object {
        const val TAG = "TraceMethodAdapter"
    }

    private val traceMethod = TraceMethod.create(0, methodAccess, className, name, desc)

    private val fullMethodName: String = traceMethod.getFullMethodName()

    private val originMethodName: String = traceMethod.getOriginMethodName(mappingCollector)

    private val methodName: String = name

    override fun onMethodEnter() {
        //init runtime config
        collectedMethodMap[fullMethodName]?.apply {
            val methodName = getSimpleMethodName(this)
            if (traceMethodFilter.isMethodWithParamesValue(originMethodName)) {
                val params = traceMethodFilter.getMethodWithParamesValue(originMethodName)
                if (!params.isNullOrEmpty()) {
                    onCustomMethodVisit(params, methodName, true)
                }
                return
            }

            if (methodName.length <= 125) {
                mv.visitLdcInsn("B:$methodName")
                mv.visitMethodInsn(
                    INVOKESTATIC,
                    RheaConstants.CLASS_TraceStub,
                    RheaConstants.METHOD_i,
                    RheaConstants.DESC_TraceStub,
                    false
                )
            }
        }
    }

    private fun onCustomMethodVisit(insertParams: List<Int>, methodName: String, isBeginMethod: Boolean) {
        if (isBeginMethod) {
            mv.visitLdcInsn("B:$methodName")
        } else {
            mv.visitLdcInsn("E:$methodName")
        }
        /**
         *
        BIPUSH 9
        ANEWARRAY java/lang/Object
         */
        mv.visitIntInsn(BIPUSH, insertParams.size)
        mv.visitTypeInsn(ANEWARRAY, ANEWARRAY_TYPE)

        /**
         * DUP
        BIPUSH 8
        ALOAD 8
        AASTORE
         */
        var paramIndex = if ((methodAccess and ACC_STATIC) == ACC_STATIC) 0 else 1
        var arrayIndex = 0
        val argumentTypes = Type.getArgumentTypes(methodDesc)
        for (i in argumentTypes.indices) {
            if (i in insertParams) {
                mv.visitInsn(DUP)
                mv.visitIntInsn(BIPUSH, arrayIndex++)
                mv.visitVarInsn(getLoadType(argumentTypes[i]), paramIndex)
                insertBasicTypeToObject(argumentTypes[i])
                mv.visitInsn(AASTORE)
            }
            paramIndex++
            if (argumentTypes[i].sort == Type.DOUBLE || argumentTypes[i].sort == Type.LONG) {
                paramIndex++
            }
        }
        mv.visitMethodInsn(
            INVOKESTATIC,
            RheaConstants.CLASS_TraceStub,
            if (isBeginMethod) RheaConstants.METHOD_i else RheaConstants.METHOD_o,
            RheaConstants.DESC_Custom_TraceStub,
            false
        )
    }

    private fun getLoadType(type: Type): Int {
        return when (type.sort) {
            Type.BOOLEAN, Type.CHAR, Type.BYTE, Type.SHORT, Type.INT -> ILOAD
            Type.FLOAT -> FLOAD
            Type.LONG -> LLOAD
            Type.DOUBLE -> DLOAD
            else -> ALOAD
        }
    }

    private fun insertBasicTypeToObject(type: Type) {
        when (type.sort) {
            Type.BOOLEAN -> mv.visitMethodInsn(
                INVOKESTATIC,
                "java/lang/Boolean",
                "valueOf",
                "(Z)Ljava/lang/Boolean;",
                false
            )
            Type.CHAR -> mv.visitMethodInsn(
                INVOKESTATIC,
                "java/lang/Character",
                "valueOf",
                "(C)Ljava/lang/Character;",
                false
            )
            Type.BYTE -> mv.visitMethodInsn(
                INVOKESTATIC,
                "java/lang/Byte",
                "valueOf",
                "(B)Ljava/lang/Byte;",
                false
            )
            Type.SHORT -> mv.visitMethodInsn(
                INVOKESTATIC,
                "java/lang/Short",
                "valueOf",
                "(S)Ljava/lang/Short;",
                false
            )
            Type.INT -> mv.visitMethodInsn(
                INVOKESTATIC,
                "java/lang/Integer",
                "valueOf",
                "(I)Ljava/lang/Integer;",
                false
            )
            Type.FLOAT -> mv.visitMethodInsn(
                INVOKESTATIC,
                "java/lang/Float",
                "valueOf",
                "(F)Ljava/lang/Float;",
                false
            )
            Type.LONG -> mv.visitMethodInsn(
                INVOKESTATIC,
                "java/lang/Long",
                "valueOf",
                "(J)Ljava/lang/Long;",
                false
            )
            Type.DOUBLE -> mv.visitMethodInsn(
                INVOKESTATIC,
                "java/lang/Double",
                "valueOf",
                "(D)Ljava/lang/Double;",
                false
            )
        }
    }

    override fun onMethodExit(opcode: Int) {
        if (traceRuntime != null && className == RheaConstants.CLASS_RuntimeConfig && methodName == "<clinit>") {
            mv.visitLdcInsn(traceRuntime.mainThreadOnly)
            mv.visitMethodInsn(
                INVOKESTATIC,
                RheaConstants.CLASS_RuntimeConfig,
                "setMainThreadOnly",
                "(Z)V",
                false
            )

            mv.visitLdcInsn(traceRuntime.startWhenAppLaunch)
            mv.visitMethodInsn(
                INVOKESTATIC,
                RheaConstants.CLASS_RuntimeConfig,
                "setStartWhenAppLaunch",
                "(Z)V",
                false
            )
            if (traceRuntime.atraceBufferSize != null) {
                var atraceBufferSize: Long = 0
                try {
                    atraceBufferSize = traceRuntime.atraceBufferSize!!.toLong()
                } catch (e: Exception) {
                    RheaLog.w(TAG, "Failed to parse 'atraceBufferSize', 'atraceBufferSize' is not a illegal string of Long type.")
                }
                mv.visitLdcInsn(atraceBufferSize)
                mv.visitMethodInsn(
                    INVOKESTATIC,
                    RheaConstants.CLASS_RuntimeConfig,
                    "setATraceBufferSize",
                    "(J)V",
                    false
                )
            }
        }
        collectedMethodMap[fullMethodName]?.apply {
            val methodName = getSimpleMethodName(this)
            if (traceMethodFilter.isMethodWithParamesValue(originMethodName)) {
                val params = traceMethodFilter.getMethodWithParamesValue(originMethodName)
                if (!params.isNullOrEmpty()) {
                    onCustomMethodVisit(params, methodName, false)
                }
                return
            }


            if (methodName.length <= 125) {
                if (opcode == ATHROW) {
                    //ATHROW special handling
                    mv.visitLdcInsn("T:$methodName")
                } else {
                    mv.visitLdcInsn("E:$methodName")
                }
                mv.visitMethodInsn(
                    INVOKESTATIC,
                    RheaConstants.CLASS_TraceStub,
                    RheaConstants.METHOD_o,
                    RheaConstants.DESC_TraceStub,
                    false
                )
            } else {
                RheaLog.w(TAG, "method $methodName is too long, we can't trace it.")
            }
        }
    }

    private fun getSimpleMethodName(traceMethod: TraceMethod): String {
        return if (traceCompilation.traceWithMethodID) {
            traceMethod.id.toString()
        } else {
            val pClassName = className.replace("/", ".")
            val methodInfo = mappingCollector.originalMethodInfo(
                pClassName,
                methodName,
                methodDesc.replace("/", ".")
            )
            val originalClassName =
                mappingCollector.originalClassName(pClassName, pClassName)
            val splits = originalClassName.split(".")
            return splits[splits.size - 1] + ":" + methodInfo.originalName
        }
    }
}