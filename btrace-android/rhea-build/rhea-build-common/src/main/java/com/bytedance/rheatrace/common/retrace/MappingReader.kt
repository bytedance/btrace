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

package com.bytedance.rheatrace.common.retrace

import com.bytedance.rheatrace.common.utils.RheaLog
import java.io.*

/**
 * Created by caichongyang on 2017/6/3.
 */
class MappingReader(private val proguardMappingFile: File) {

    companion object {
        private const val TAG = "MappingReader"
        private const val SPLIT = ":"
        private const val SPACE = " "
        private const val ARROW = "->"
        private const val LEFT_PUNC = "("
        private const val RIGHT_PUNC = ")"
        private const val DOT = "."
    }

    /**
     * Reads the mapping file
     */
    @Throws(IOException::class)
    fun read(mappingProcessor: MappingProcessor) {
        val reader = LineNumberReader(BufferedReader(FileReader(proguardMappingFile)))
        try {
            var className: String? = null
            // Read the class and class member mappings.
            while (true) {
                var line = reader.readLine() ?: break
                line = line.trim { it <= ' ' }
                if (!line.startsWith("#")) {
                    // a class mapping
                    if (line.endsWith(SPLIT)) {
                        className = parseClassMapping(line, mappingProcessor)
                    } else className?.let { parseClassMemberMapping(it, line, mappingProcessor) }
                } else {
                    RheaLog.i(TAG, "comment:# %s", line)
                }
            }
        } catch (err: IOException) {
            throw IOException("Can't read mapping file", err)
        } finally {
            try {
                reader.close()
            } catch (ex: IOException) {
                // do nothing
            }
        }
    }

    /**
     * @param line read content
     * @param mappingProcessor
     * @return
     */
    private fun parseClassMapping(line: String, mappingProcessor: MappingProcessor): String? {
        val leftIndex = line.indexOf(ARROW)
        if (leftIndex < 0) {
            return null
        }
        val offset = 2
        val rightIndex = line.indexOf(SPLIT, leftIndex + offset)
        if (rightIndex < 0) {
            return null
        }

        // trim the elements.
        val className = line.substring(0, leftIndex).trim { it <= ' ' }
        val newClassName = line.substring(leftIndex + offset, rightIndex).trim { it <= ' ' }

        // Process this class name mapping.
        val ret = mappingProcessor.processClassMapping(className, newClassName)
        return if (ret) className else null
    }

    /**
     * Parses the a class member mapping
     *
     * @param className
     * @param line
     * @param mappingProcessor parse line such as
     * ___ ___ -> ___
     * ___:___:___ ___(___) -> ___
     * ___:___:___ ___(___):___ -> ___
     * ___:___:___ ___(___):___:___ -> ___
     */
    private fun parseClassMemberMapping(
        className: String,
        line: String,
        mappingProcessor: MappingProcessor
    ) {
        var classNameTemp = className
        val leftIndex1 = line.indexOf(SPLIT)
        val leftIndex2 = if (leftIndex1 < 0) -1 else line.indexOf(SPLIT, leftIndex1 + 1)
        val spaceIndex = line.indexOf(SPACE, leftIndex2 + 2)
        val argIndex1 = line.indexOf(LEFT_PUNC, spaceIndex + 1)
        val argIndex2 = if (argIndex1 < 0) -1 else line.indexOf(RIGHT_PUNC, argIndex1 + 1)
        val leftIndex3 = if (argIndex2 < 0) -1 else line.indexOf(SPLIT, argIndex2 + 1)
        val leftIndex4 = if (leftIndex3 < 0) -1 else line.indexOf(SPLIT, leftIndex3 + 1)
        val rightIndex = line.indexOf(
            ARROW,
            (if (leftIndex4 >= 0) leftIndex4 else if (leftIndex3 >= 0) leftIndex3 else if (argIndex2 >= 0) argIndex2 else spaceIndex) + 1
        )
        if (spaceIndex < 0 || rightIndex < 0) {
            return
        }

        // trim the elements.
        val type = line.substring(leftIndex2 + 1, spaceIndex).trim { it <= ' ' }
        var name = line.substring(spaceIndex + 1, if (argIndex1 >= 0) argIndex1 else rightIndex)
            .trim { it <= ' ' }
        val newName = line.substring(rightIndex + 2).trim { it <= ' ' }
        val newClassName = classNameTemp
        val dotIndex = name.lastIndexOf(DOT)
        if (dotIndex >= 0) {
            classNameTemp = name.substring(0, dotIndex)
            name = name.substring(dotIndex + 1)
        }

        // parse class member mapping.
        if (type.isNotEmpty() && name.isNotEmpty() && newName.isNotEmpty() && argIndex2 >= 0) {
            val arguments = line.substring(argIndex1 + 1, argIndex2).trim { it <= ' ' }
            mappingProcessor.processMethodMapping(
                classNameTemp,
                type,
                name,
                arguments,
                newClassName,
                newName
            )
        }
    }
}