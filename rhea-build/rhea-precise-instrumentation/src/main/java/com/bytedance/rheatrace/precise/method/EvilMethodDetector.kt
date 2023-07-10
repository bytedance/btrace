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
package com.bytedance.rheatrace.precise.method

import com.bytedance.rheatrace.common.utils.TypeUtil
import com.bytedance.rheatrace.precise.method.EvilMethodReason.TYPE_ANNOTATION_METHOD
import com.bytedance.rheatrace.precise.method.EvilMethodReason.TYPE_LARGE_METHOD
import com.bytedance.rheatrace.precise.method.EvilMethodReason.TYPE_LOCK
import com.bytedance.rheatrace.precise.method.EvilMethodReason.TYPE_LOCK_INNER
import com.bytedance.rheatrace.precise.method.EvilMethodReason.TYPE_LOOP_METHOD
import com.bytedance.rheatrace.precise.method.EvilMethodReason.TYPE_NO_EVIL
import com.bytedance.rheatrace.precise.method.EvilMethodReason.TYPE_TRACE_CLASS
import com.bytedance.rheatrace.precise.method.EvilMethodReason.TYPE_TRACE_METHOD
import org.objectweb.asm.Opcodes
import org.objectweb.asm.tree.ClassNode
import org.objectweb.asm.tree.JumpInsnNode
import org.objectweb.asm.tree.MethodInsnNode
import org.objectweb.asm.tree.MethodNode
import java.util.LinkedList

/**
 * @author majun
 * @date 2022/3/10
 */
class EvilMethodDetector(val methodHelper: MethodHelper, val evilRootMethodDetector: EvilRootMethodDetector) {
    private val methods = mutableListOf<MethodNodeInfo>()

    private val evilMethods = mutableListOf<EvilMethodInfo>()

    /**
     * collect methods
     */
    fun collect(node: ClassNode) {
        node.methods.forEach {
            MethodNodeInfo(node, it).apply {
                synchronized(EvilMethodDetector::class.java) {
                    methods.add(this)
                }
            }
        }
    }


    fun findEvilMethods(): List<EvilMethodInfo> {
        evilMethods.clear()
        methods.parallelStream().forEach { methodNodeInfo ->
            if (methodNodeInfo.methodType == TYPE_NO_EVIL) {
                methodNodeInfo.detectEvilMethod()
                if (methodNodeInfo.methodType > TYPE_NO_EVIL) {
                    synchronized(EvilMethodDetector::class.java) {
                        evilMethods.add(EvilMethodInfo(methodNodeInfo.className, methodNodeInfo.methodName, methodNodeInfo.methodDesc, methodNodeInfo.access.toString()))
                    }
                }
            }
        }
        return evilMethods
    }

    fun release() {
        evilMethods.clear()
        methods.clear()
    }

    inner class MethodNodeInfo(private val classNode: ClassNode, methodNode: MethodNode) {
        val className: String = classNode.name
        val methodName: String = methodNode.name
        val methodDesc: String = methodNode.desc
        var methodType: Int = TYPE_NO_EVIL
        val access = methodNode.access

        private val isSynchronized = TypeUtil.isSynchronized(methodNode.access)
        private val invokeMethodInsNode = LinkedList<MethodInsnNode>()
        private val isTraceAnnotationMethod by lazy {
            methodNode.invisibleAnnotations?.any {
                methodHelper.isTraceAnnotation(it.desc)
            } ?: false || methodNode.visibleAnnotations?.any {
                methodHelper.isTraceAnnotation(it.desc)
            } ?: false
        }
        private val originMethodName = lazy {
            methodHelper.getOriginMethodName(className, methodName, methodDesc)
        }
        private var monitorSize: Int = 0
        private var looperSize = 0

        init {
            for (index in 0 until methodNode.instructions.size()) {
                val insn = methodNode.instructions.get(index)
                if (insn.opcode == Opcodes.MONITORENTER) {
                    monitorSize++
                } else if (insn is JumpInsnNode && methodNode.instructions.indexOf(insn.label) < index) {
                    looperSize++
                } else if (insn is MethodInsnNode) {
                    invokeMethodInsNode.add(insn)
                }
            }
        }

        fun detectEvilMethod() {
            if (methodType > TYPE_NO_EVIL) {
                return
            }
            if (invokeMethodInsNode.size == 0) {
                return
            }
            if (methodHelper.isTraceMethod(originMethodName.value, classNode)) {
                methodType = methodType or TYPE_TRACE_METHOD
            }
            if (methodHelper.isTraceClass(className)) {
                methodType = methodType or TYPE_TRACE_CLASS
            }
            if (isTraceAnnotationMethod) {
                methodType = methodType or TYPE_ANNOTATION_METHOD
            }
            if (MethodFilter.isSynchronizeEnable) {
                if (isSynchronized) {
                    methodType = methodType or TYPE_LOCK
                }
                if (monitorSize > 0) {
                    methodType = methodType or TYPE_LOCK_INNER
                }
            }

            if (MethodFilter.isLoopEnable && looperSize > 0) {
                methodType = methodType or TYPE_LOOP_METHOD
            }

            if (MethodFilter.largeMethodSize > 0 && invokeMethodInsNode.size >= MethodFilter.largeMethodSize) {
                methodType = methodType or TYPE_LARGE_METHOD
            }
            invokeMethodInsNode.forEach {
                methodType = methodType or methodHelper.matchEvilRootMethod(it.owner, it.name, it.desc)
            }
        }

        override fun toString(): String {
            return "MethodNodeInfo(className='$className', methodName='$methodName', methodDesc='$methodDesc')"
        }
    }
}