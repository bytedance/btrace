package com.bytedance.rheatrace.plugin.internal.common


import java.io.*
import java.util.zip.ZipEntry
import java.util.zip.ZipException
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

object FileUtil {

    private const val TAG = "FileUtil"

    private const val BUFFER_SIZE = 16384

    fun isRealZipOrJar(input: File?): Boolean {
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
    fun copyFileUsingStream(source: File?, dest: File) {
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
        zipEntry: ZipEntry?,
        inputStream: InputStream
    ) {
        try {
            zipOutputStream.putNextEntry(zipEntry)
            val buffer = ByteArray(BUFFER_SIZE)
            var length = -1
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