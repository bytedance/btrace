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
import com.bytedance.rheatrace.precise.method.EvilMethodReason.TYPE_AIDL
import com.bytedance.rheatrace.precise.method.EvilMethodReason.TYPE_NATIVE_INNER
import com.bytedance.rheatrace.precise.method.EvilMethodReason.TYPE_NO_EVIL
import org.objectweb.asm.tree.ClassNode
import org.objectweb.asm.tree.MethodNode

/**
 * @author majun
 * @date 2022/3/10
 */
class EvilRootMethodDetector {

    private val evilRootMethods = mutableMapOf<String, RootMethodNodeInfo>()

    /**
     * Collect method information
     */
    fun collect(node: ClassNode) {
        node.methods.forEach {
            RootMethodNodeInfo(node, it).apply {
                synchronized(evilRootMethods) {
                    this.detectEvilRootMethod()
                    if (this.methodType > TYPE_NO_EVIL) {
                        evilRootMethods[methodId(this.className, this.methodName, this.methodDesc)] = this
                    }
                }
            }
        }
    }

    fun marchEvilRootMethod(owner: String, name: String, desc: String): RootMethodNodeInfo? {
        return evilRootMethods[methodId(owner, name, desc)]
    }

    private fun methodId(className: String, methodName: String, methodDesc: String): String =
        "$className.${methodName}${methodDesc}"

    fun release() {
        evilRootMethods.clear()
    }

    /**
     * Methods used primarily to manage sources of evil functions
     */
    inner class RootMethodNodeInfo(classNode: ClassNode, methodNode: MethodNode) {
        var methodType = TYPE_NO_EVIL

        val className: String = classNode.name
        val methodName: String = methodNode.name
        val methodDesc: String = methodNode.desc

        private val isAidlClass = classNode.interfaces.contains("android/os/IInterface")

        private val isNative = TypeUtil.isNative(methodNode.access)

        /**
         * Determine the methodType according to the native and aidl configurations
         */
        fun detectEvilRootMethod() {
            if (MethodFilter.isAIDLEnable && isAidlClass) {
                methodType = TYPE_AIDL
                return
            }
            if (MethodFilter.isNativeEnable && isNative) {
                methodType = methodType or TYPE_NATIVE_INNER
            }
        }

        override fun toString(): String {
            return "RootMethodNodeInfo(className='$className', methodName='$methodName', methodDesc='$methodDesc')"
        }
    }
}