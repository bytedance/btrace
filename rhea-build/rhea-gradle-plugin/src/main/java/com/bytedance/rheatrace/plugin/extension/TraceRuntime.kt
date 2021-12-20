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

    var atraceBufferSize: String? = null

) {

    override fun equals(other: Any?): Boolean {
        if (other == null) {
            return false
        }
        if (other !is TraceRuntime) {
            return false
        }
        return other.mainThreadOnly == mainThreadOnly
                && other.startWhenAppLaunch == startWhenAppLaunch
                && other.atraceBufferSize == atraceBufferSize
    }

    override fun toString(): String {
        return "TraceRuntime(mainThreadOnly=$mainThreadOnly, startWhenAppLaunch=$startWhenAppLaunch, atraceBufferSize=$atraceBufferSize)"
    }

    override fun hashCode(): Int {
        var result = mainThreadOnly.hashCode()
        result = 31 * result + startWhenAppLaunch.hashCode()
        result = 31 * result + atraceBufferSize.hashCode()
        return result
    }
}