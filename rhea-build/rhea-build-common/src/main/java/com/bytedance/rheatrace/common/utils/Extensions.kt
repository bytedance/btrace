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
package com.bytedance.rheatrace.common.utils

import java.io.BufferedInputStream
import java.io.File
import java.io.FileInputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream


fun String.isZipName(): Boolean = endsWith("jar") || endsWith("aar") || endsWith("zip") || endsWith("apk")

fun Long.fileSize(): String {
    if (this < 1024) {
        return this.toString()
    }
    val kb = this / (1024.toFloat())
    if (kb < 1024) {
        return String.format("%.2fKB", kb)
    }
    val mb = kb / (1024.toFloat())
    if (mb < 1024) {
        return String.format("%.2fMB", mb)
    }
    return String.format("%.2fGB", mb / (1024.toFloat()))
}

fun <R> runWithPrintTime(name: String, action: () -> R): R {
    val start = System.currentTimeMillis()
    val r = action.invoke()
    println("[$name] ${System.currentTimeMillis() - start}ms")
    return r
}

fun File.unzip(action: (String, ByteArray) -> Unit) {
    ZipInputStream(BufferedInputStream(FileInputStream(this))).use { zin ->
        var zipEntry: ZipEntry? = zin.nextEntry
        while (zipEntry != null) {
            if (!zipEntry.isDirectory) {
                action.invoke(zipEntry.name, zin.readBytes())
            }
            zipEntry = zin.nextEntry
        }
    }
}

@Throws(IllegalArgumentException::class)
fun String.matchPair(left: String, right: String, startIndex: Int = 0, endIndex: Int = length): Pair<Int, Int>? {
    if (left == right) {
        throw IllegalArgumentException("left can not be equals right:${left}")
    }
    val sl = indexOf(left, startIndex)
    if (sl < 0) {
        return null
    }
    var start = sl + left.length
    var stack = 1
    while (stack != 0 && 0 < start && start < endIndex) {
        val startRight = indexOf(right, start)
        if (startRight < start) {
            //Not Closed
            throw IllegalArgumentException("Can matchPair by $left and $right from:$this")
        }
        val startLeft = indexOf(left, start)
        if (0 <= startLeft && startLeft < startRight) {
            //reach left
            stack++
            start = startLeft + left.length
        } else {
            //reach right
            stack--
            start = startRight + right.length
        }
    }
    if (stack != 0) {
        //Not Closed
        throw IllegalArgumentException("Can matchPair by $left and $right from:$this")
    }
    return Pair(sl, start)
}

fun File.traverseFile(action: (File) -> Unit) {
    if (this.isFile) {
        action.invoke(this)
    }
    listFiles()?.forEach {
        it.traverseFile(action)
    }
}