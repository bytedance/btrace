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

package com.bytedance.rheatrace.plugin.compiling.filter


import com.bytedance.rheatrace.common.retrace.MappingCollector
import com.bytedance.rheatrace.plugin.compiling.TraceMethod
import com.bytedance.rheatrace.plugin.internal.RheaConstants
import org.objectweb.asm.Opcodes
import org.objectweb.asm.tree.MethodNode

abstract class DefaultTraceMethodFilter(mappingCollector: MappingCollector) :
    TraceMethodFilter(mappingCollector) {

    override fun onMethodNeedFilter(methodNode: MethodNode, traceMethod: TraceMethod, originFullMethod: String): Boolean {
        return isGetOrSetMethod(
            methodNode
        ) || isSingleMethod(
            methodNode
        ) || isEmptyMethod(
            methodNode
        )
    }

    override fun onClassNeedFilter(originFullMethod: String): Boolean {
        for (unTraceCls in RheaConstants.UN_TRACE_CLASS) {
            if (originFullMethod.contains(unTraceCls)) {
                return true
            }
        }
        return false
    }

    companion object {
        private fun isGetOrSetMethod(node: MethodNode): Boolean {
            var ignoreCount = 0
            val iterator = node.instructions.iterator()
            while (iterator.hasNext()) {
                val insnNode = iterator.next()
                val opcode = insnNode.opcode
                if (-1 == opcode) {
                    continue
                }
                if (opcode != Opcodes.GETFIELD
                    && opcode != Opcodes.GETSTATIC
                    && opcode != Opcodes.H_GETFIELD
                    && opcode != Opcodes.H_GETSTATIC
                    && opcode != Opcodes.RETURN
                    && opcode != Opcodes.ARETURN
                    && opcode != Opcodes.DRETURN
                    && opcode != Opcodes.FRETURN
                    && opcode != Opcodes.LRETURN
                    && opcode != Opcodes.IRETURN
                    && opcode != Opcodes.PUTFIELD
                    && opcode != Opcodes.PUTSTATIC
                    && opcode != Opcodes.H_PUTFIELD
                    && opcode != Opcodes.H_PUTSTATIC
                    && opcode > Opcodes.SALOAD
                ) {
                    if ("<init>" == node.name && opcode == Opcodes.INVOKESPECIAL) {
                        ignoreCount++
                        if (ignoreCount > 1) {
                            return false
                        }
                        continue
                    }
                    return false
                }
            }
            return true
        }

        private fun isSingleMethod(methodNode: MethodNode): Boolean {
            val iterator = methodNode.instructions.iterator()
            while (iterator.hasNext()) {
                val insnNode = iterator.next()
                val opcode = insnNode.opcode
                if (-1 == opcode) {
                    continue
                } else if (Opcodes.INVOKEVIRTUAL <= opcode && opcode <= Opcodes.INVOKEDYNAMIC) {
                    return false
                }
            }
            return true
        }

        private fun isEmptyMethod(methodNode: MethodNode): Boolean {
            val iterator = methodNode.instructions.iterator()
            while (iterator.hasNext()) {
                val insnNode = iterator.next()
                val opcode = insnNode.opcode
                return if (-1 == opcode) {
                    continue
                } else {
                    false
                }
            }
            return true
        }
    }
}
