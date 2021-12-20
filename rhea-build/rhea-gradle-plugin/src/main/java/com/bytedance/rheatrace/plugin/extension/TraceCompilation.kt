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

open class TraceCompilation(

    var traceWithMethodID: Boolean = false,

    var traceFilterFilePath: String = "",

    var applyMethodMappingFilePath: String = ""
) {
    override fun toString(): String {
        return "TraceCompilation(traceWithMethodID=$traceWithMethodID, traceFilterFilePath='$traceFilterFilePath', applyMethodMappingFilePath='$applyMethodMappingFilePath')"
    }

    override fun equals(other: Any?): Boolean {
        if (other == null) {
            return false
        }
        if (other !is TraceCompilation) {
            return false
        }
        return other.traceWithMethodID == traceWithMethodID
                && other.traceFilterFilePath == traceFilterFilePath
                && other.applyMethodMappingFilePath == applyMethodMappingFilePath
    }

    override fun hashCode(): Int {
        var result = traceWithMethodID.hashCode()
        result = 31 * result + traceFilterFilePath.hashCode()
        result = 31 * result + applyMethodMappingFilePath.hashCode()
        return result
    }

}