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


import java.io.*
import java.util.zip.ZipEntry
import java.util.zip.ZipException
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

object FileUtil {

    private const val TAG = "FileUtil"

    private const val BUFFER_SIZE = 16384

    fun isRealZipOrJar(input: File): Boolean {
        var zf: ZipFile? = null
        return try {
            zf = ZipFile(input)
            true
        } catch (e: Exception) {
            false
        } finally {
            closeQuietly(
                zf
            )
        }
    }

    /**
     * Close `target` quietly.
     *
     * @param obj
     * Object to be closed.
     */
    private fun closeQuietly(obj: Any?) {
        if (obj == null) {
            return
        }
        if (obj is Closeable) {
            try {
                obj.close()
            } catch (ignored: Throwable) {
                // ignore
            }
        } else {
            throw IllegalArgumentException("obj $obj is not closeable")
        }
    }

    @Throws(IOException::class)
    fun copyFileUsingStream(source: File, dest: File) {
        var `is`: FileInputStream? = null
        var os: FileOutputStream? = null
        val parent = dest.parentFile
        if (parent != null && !parent.exists()) {
            parent.mkdirs()
        }
        try {
            `is` = FileInputStream(source)
            os = FileOutputStream(dest, false)
            val buffer = ByteArray(BUFFER_SIZE)
            var length: Int
            while (`is`.read(buffer).also { length = it } > 0) {
                os.write(buffer, 0, length)
            }
        } finally {
            closeQuietly(
                `is`
            )
            closeQuietly(
                os
            )
        }
    }

    fun readFileAsString(filePath: String?): String {
        if (filePath == null) {
            return ""
        }
        if (!File(filePath).exists()) {
            return ""
        }
        val fileData = StringBuffer()
        var fileReader: Reader? = null
        var inputStream: InputStream? = null
        try {
            inputStream = FileInputStream(filePath)
            fileReader = InputStreamReader(inputStream, "UTF-8")
            val buf = CharArray(BUFFER_SIZE)
            var numRead: Int
            while (fileReader.read(buf).also { numRead = it } != -1) {
                val readData = String(buf, 0, numRead)
                fileData.append(readData)
            }
        } catch (e: Exception) {
            e.printStackTrace()
            return ""
        } finally {
            try {
                closeQuietly(
                    fileReader
                )
                closeQuietly(
                    inputStream
                )
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
        return fileData.toString()
    }

    @Throws(Exception::class)
    fun addZipEntry(
        zipOutputStream: ZipOutputStream,
        zipEntry: ZipEntry,
        inputStream: InputStream
    ) {
        try {
            zipOutputStream.putNextEntry(zipEntry)
            val buffer = ByteArray(BUFFER_SIZE)
            var length: Int
            while (inputStream.read(buffer, 0, buffer.size).also { length = it } != -1) {
                zipOutputStream.write(buffer, 0, length)
                zipOutputStream.flush()
            }
        } catch (e: ZipException) {
            e.printStackTrace()
        } finally {
            closeQuietly(
                inputStream
            )
            zipOutputStream.closeEntry()
        }
    }
}