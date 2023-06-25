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

import com.bytedance.rheatrace.plugin.RheaContext
import com.bytedance.rheatrace.plugin.internal.RheaConstants
import com.ss.android.ugc.bytex.common.visitor.BaseClassVisitor
import org.objectweb.asm.MethodVisitor
import org.objectweb.asm.Opcodes
import org.objectweb.asm.Type
import org.objectweb.asm.commons.AdviceAdapter

class RheaTraceClassVisitor(private val rheaContext: RheaContext) : BaseClassVisitor() {

    private var superName = ""

    private lateinit var className: String

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
        this.className = name
        this.isApplication = name.replace("/", ".") == rheaContext.applicationName.value
        super.visit(version, access, name, signature, superName, interfaces)
        if (!superName.isNullOrEmpty()) {
            this.superName = superName
        }
    }

    override fun visitMethod(
        access: Int,
        name: String,
        desc: String,
        signature: String?,
        exceptions: Array<String>?
    ): MethodVisitor? {
        if (cv == null) {
            return null
        }
        if (access and Opcodes.ACC_ABSTRACT > 0) {
            return super.visitMethod(access, name, desc, signature, exceptions)
        } else {
            val mv = super.visitMethod(access, name, desc, signature, exceptions)
            if (isApplication) {
                if (RheaConstants.METHOD_attachBaseContext == name) {
                    needCreateAttachBaseContext = false
                    instrAttachBaseContextForRheaTrace(mv)
                }
                if (RheaConstants.METHOD_onCreate == name) {
                    needCreateOnCreate = false
                    instrOnCreateForRheaTrace(mv)
                }
            }
            return TraceMethodAdapter(
                api,
                mv,
                access,
                name,
                desc,
                className,
                rheaContext
            )
        }
    }

    override fun visitEnd() {
        if (isApplication && needCreateAttachBaseContext) {
            createAttachBaseContextForApplication()
        }
        if (isApplication && needCreateOnCreate) {
            createOnCreateForApplication()
        }
        super.visitEnd()
    }

    private fun createAttachBaseContextForApplication() {
        val mv = cv.visitMethod(
            Opcodes.ACC_PROTECTED,
            RheaConstants.METHOD_attachBaseContext,
            RheaConstants.DESC_attachBaseContext,
            null,
            null
        )
        instrAttachBaseContextForRheaTrace(mv)
        mv.visitVarInsn(Opcodes.ALOAD, 0)
        mv.visitVarInsn(Opcodes.ALOAD, 1)
        mv.visitMethodInsn(
            Opcodes.INVOKESPECIAL,
            superName,
            RheaConstants.METHOD_attachBaseContext,
            RheaConstants.DESC_attachBaseContext,
            false
        )
        mv.visitInsn(Opcodes.RETURN)
        mv.visitEnd()
    }

    private fun instrAttachBaseContextForRheaTrace(mv: MethodVisitor) {
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
        instrOnCreateForRheaTrace(mv)
        mv.visitInsn(Opcodes.RETURN)
        mv.visitEnd()
    }

    private fun instrOnCreateForRheaTrace(mv: MethodVisitor) {
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

private class TraceMethodAdapter(
    api: Int,
    mv: MethodVisitor,
    access: Int,
    name: String,
    desc: String,
    private val className: String,
    val rheaContext: RheaContext
) : AdviceAdapter(api, mv, access, name, desc) {

    companion object {
        const val TAG = "TraceMethodAdapter"
    }

    private val traceMethod = TraceMethod.create(0, methodAccess, className, name, desc)

    private val fullMethodName: String = traceMethod.getFullMethodName()

    private val originMethodName: String =
        traceMethod.getOriginMethodName(rheaContext.mappingCollector)

    private val methodName: String = name

    private var collectedMethod: TraceMethod? = null
    private var withParamsValues = false
    private var paramsValues: List<Int>? = null
    private var vTimeLocal: Int? = null

    override fun onMethodEnter() {
        val ctx = rheaContext
        collectedMethod = ctx.collectedMethodMap[fullMethodName]
        collectedMethod?.apply {
            withParamsValues = ctx.traceMethodFilter.isMethodWithParamValue(originMethodName)
            val methodName = getSimpleMethodName(this)
            if (withParamsValues) {
                paramsValues = ctx.traceMethodFilter.getMethodWithParamValue(originMethodName)
                if (!paramsValues.isNullOrEmpty()) {
                    onCustomMethodVisit(paramsValues!!, methodName, true)
                    return
                }
            }
            vTimeLocal = newLocal(Type.LONG_TYPE)
            mv.visitLdcInsn(id)
            mv.visitMethodInsn(
                INVOKESTATIC,
                RheaConstants.CLASS_TraceStub,
                RheaConstants.METHOD_i,
                RheaConstants.DESC_i,
                false
            )
            mv.visitVarInsn(Opcodes.LSTORE, vTimeLocal!!)
        }
    }

    override fun onMethodExit(opcode: Int) {
        collectedMethod?.apply {
            val methodName = getSimpleMethodName(this)
            if (withParamsValues) {
                if (!paramsValues.isNullOrEmpty()) {
                    onCustomMethodVisit(paramsValues!!, methodName, false)
                    return
                }
            }
            vTimeLocal?.let {
                mv.visitLdcInsn(id)
                mv.visitVarInsn(LLOAD, it)
                mv.visitMethodInsn(
                    INVOKESTATIC,
                    RheaConstants.CLASS_TraceStub,
                    RheaConstants.METHOD_o,
                    RheaConstants.DESC_o,
                    false
                )
            }
        }
    }

    private fun onCustomMethodVisit(
        insertParams: List<Int>,
        methodName: String,
        isBeginMethod: Boolean
    ) {
        mv.visitLdcInsn(methodName)
        /*
        BIPUSH 9
        ANEWARRAY java/lang/Object
         */
        mv.visitIntInsn(BIPUSH, insertParams.size)
        mv.visitTypeInsn(ANEWARRAY, RheaConstants.ANEWARRAY_TYPE)
        /*
        DUP
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

    private fun getSimpleMethodName(traceMethod: TraceMethod): String {
        val pClassName = className.replace("/", ".")
        val methodInfo = rheaContext.mappingCollector.originalMethodInfo(
            pClassName,
            methodName,
            methodDesc.replace("/", ".")
        )
        val originalClassName =
            rheaContext.mappingCollector.originalClassName(pClassName, pClassName)
        val splits = originalClassName.split(".")
        return splits[splits.size - 1] + ":" + methodInfo.originalName
    }
}