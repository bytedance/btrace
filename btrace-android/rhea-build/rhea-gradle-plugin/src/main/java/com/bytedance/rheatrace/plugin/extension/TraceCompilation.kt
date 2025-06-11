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

    var applyMethodMappingFilePath: String = "",

    var needPackageWithMethodMap: Boolean = true,

    var radicalFilterMode: Boolean = false

) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as TraceCompilation

        if (traceWithMethodID != other.traceWithMethodID) return false
        if (traceFilterFilePath != other.traceFilterFilePath) return false
        if (applyMethodMappingFilePath != other.applyMethodMappingFilePath) return false
        if (needPackageWithMethodMap != other.needPackageWithMethodMap) return false
        if (radicalFilterMode != other.radicalFilterMode) return false

        return true
    }

    override fun hashCode(): Int {
        var result = traceWithMethodID.hashCode()
        result = 31 * result + traceFilterFilePath.hashCode()
        result = 31 * result + applyMethodMappingFilePath.hashCode()
        result = 31 * result + needPackageWithMethodMap.hashCode()
        result = 31 * result + radicalFilterMode.hashCode()
        return result
    }

    override fun toString(): String {
        return "TraceCompilation(traceWithMethodID=$traceWithMethodID, traceFilterFilePath='$traceFilterFilePath', applyMethodMappingFilePath='$applyMethodMappingFilePath', needPackageWithMethodMap=$needPackageWithMethodMap, radicalFilterMode=$radicalFilterMode)"
    }


}