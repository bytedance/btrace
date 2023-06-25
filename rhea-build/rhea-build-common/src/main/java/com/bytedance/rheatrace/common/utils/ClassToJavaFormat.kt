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
 * @date 2022/5/26
 */
public object ClassToJavaFormat {
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

    private const val DESC_ARRAY = "["

    /**
     * @param s descriptor string.
     * @return return type of method.
     */
    fun parseMethodReturn(s: String): String {
        val j: Int = s.lastIndexOf(')')
        return if (j >= 0) {
            parseDesc(s.substring(j + 1), ",")
        } else parseDesc(s, ",")
    }

    /**
     * @param s descriptor string.
     * @return  list of parameter types
     */
    fun parseMethodParams(s: String): String {
        val j: Int
        val i: Int = s.indexOf('(')
        if (i >= 0) {
            j = s.indexOf(')', i + 1)
            if (j >= 0) {
                return parseDesc(s.substring(i + 1, j), ",")
            }
        }
        return "<parse error>"
    }

    /**
     * @param desc descriptor string.
     * @param sep String to use as a separator between types.
     * @return String of types parsed.
     */
    fun parseDesc(desc: String, sep: String): String {
        var params = ""
        var param: String
        var c: Char
        var i: Int
        var arraylevel = 0
        var didone = false
        val len: Int = desc.length
        i = 0
        while (i < len) {
            c = desc[i]
            if (c == DESC_BYTE[0]) {
                param = "byte"
            } else if (c == DESC_CHAR[0]) {
                param = "char"
            } else if (c == DESC_DOUBLE[0]) {
                param = "double"
            } else if (c == DESC_FLOAT[0]) {
                param = "float"
            } else if (c == DESC_INT[0]) {
                param = "int"
            } else if (c == DESC_LONG[0]) {
                param = "long"
            } else if (c == DESC_SHORT[0]) {
                param = "short"
            } else if (c == DESC_BOOLEAN[0]) {
                param = "boolean"
            } else if (c == DESC_VOID[0]) {
                param = "void"
            } else if (c == DESC_ARRAY[0]) {
                arraylevel++
                i++
                continue
            } else if (c == DESC_OBJECT[0]) {
                val j: Int = desc.indexOf(';', i + 1)
                if (j < 0) {
                    RheaLog.i("ClassToJavaFormat", "Parse error -- can't find a ; in " + desc.substring(i + 1))
                    param = "<error>"
                } else {
                    param = desc.substring(i + 1, j)
                    param = param.replace('/', '.')
                    i = j
                }
            } else {
                param = "???"
            }
            if (didone) {
                params += sep
            }
            params += param
            while (arraylevel > 0) {
                params = "$params[]"
                arraylevel--
            }
            didone = true
            i++
        }
        return params
    }
}