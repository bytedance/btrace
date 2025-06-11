/*
 * Soot - a J*va Optimization Framework
 * %%
 * Copyright (C) 1997 Clark Verbrugge
 * %%
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Lesser Public License for more details.
 *
 * You should have received a copy of the GNU General Lesser Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/lgpl-2.1.html>.
 * #L%
 *
 * ------ modify by RheaTrace ------
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
package com.bytedance.rheatrace.common.utils

/**
 * @author majun
 * @date 2022/7/28
 */
object JavaToClassFormat {
    private const val DESC_BYTE = "B"

    private const val DESC_CHAR = "C"

    private const val DESC_DOUBLE = "D"

    private const val DESC_FLOAT = "F"

    private const val DESC_INT = "I"

    private const val DESC_LONG = "J"

    private const val DESC_OBJECT = "L"

    private const val DESC_SHORT = "S"

    private const val DESC_BOOLEAN = "Z"

    private const val DESC_VOID = "V"

    /**
     * @param paramString list of parameter types
     * @return return method desc_params.
     */
    fun getMethodDescParams(paramString: String): String {
        if (paramString.isEmpty()) {
            return "()"
        }
        val params = paramString.split(",")
        val byteParamStringBuilder = StringBuilder()
        byteParamStringBuilder.append("(")
        for (i in params.indices) {
            byteParamStringBuilder.append(parseJavaToDesc(params[i]))
        }
        byteParamStringBuilder.append(")")
        return byteParamStringBuilder.toString()
    }

    /**
     * @param returnString return type.
     * @return return method desc_return.
     */
    fun getMethodDescReturn(returnString: String): String {
        return parseJavaToDesc(returnString)
    }

    /**
     * @param type
     * @return return desc.
     */
    private fun parseJavaToDesc(type: String): String {
        val stringBuilder = StringBuilder()
        var param = type.replace(" ", "")
        if (param.endsWith("[]")) {
            stringBuilder.append("[")
            param = param.replace("[]", "")
        }
        if (param.isNotEmpty()) {
            stringBuilder.append(
                when (param) {
                    "" -> ""
                    "boolean" -> DESC_BOOLEAN
                    "char" -> DESC_CHAR
                    "byte" -> DESC_BYTE
                    "short" -> DESC_SHORT
                    "int" -> DESC_INT
                    "float" -> DESC_FLOAT
                    "long" -> DESC_LONG
                    "double" -> DESC_DOUBLE
                    "void" -> DESC_VOID
                    else -> {
                        "$DESC_OBJECT$param;"
                    }
                }
            )
        }

        return stringBuilder.toString().replace(".", "/")
    }
}