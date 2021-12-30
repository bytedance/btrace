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

package com.bytedance.rheatrace.plugin.extension

open class TraceRuntime(

    var mainThreadOnly: Boolean = false,

    var startWhenAppLaunch: Boolean = true,

    var enableIO: Boolean = true,

    var enableMemory: Boolean = false,

    var enableClassLoad: Boolean = false,

    var atraceBufferSize: String? = null

) {

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as TraceRuntime

        if (mainThreadOnly != other.mainThreadOnly) return false
        if (startWhenAppLaunch != other.startWhenAppLaunch) return false
        if (enableIO != other.enableIO) return false
        if (enableMemory != other.enableMemory) return false
        if (enableClassLoad != other.enableClassLoad) return false
        if (atraceBufferSize != other.atraceBufferSize) return false

        return true
    }

    override fun hashCode(): Int {
        var result = mainThreadOnly.hashCode()
        result = 31 * result + startWhenAppLaunch.hashCode()
        result = 31 * result + enableIO.hashCode()
        result = 31 * result + enableMemory.hashCode()
        result = 31 * result + enableClassLoad.hashCode()
        result = 31 * result + (atraceBufferSize?.hashCode() ?: 0)
        return result
    }

    override fun toString(): String {
        return "TraceRuntime(mainThreadOnly=$mainThreadOnly, startWhenAppLaunch=$startWhenAppLaunch, enableIO=$enableIO, enableMemory=$enableMemory, enableClassLoad=$enableClassLoad, atraceBufferSize=$atraceBufferSize)"
    }
}