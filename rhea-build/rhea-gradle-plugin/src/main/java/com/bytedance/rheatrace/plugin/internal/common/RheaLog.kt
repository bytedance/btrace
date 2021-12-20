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

// modify by RheaTrace

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

package com.bytedance.rheatrace.plugin.internal.common

import java.io.PrintWriter
import java.io.StringWriter

object RheaLog {

    const val LOG_LEVEL_VERBOSE = 0

    const val LOG_LEVEL_DEBUG = 1

    const val LOG_LEVEL_INFO = 2

    const val LOG_LEVEL_WARN = 3

    const val LOG_LEVEL_ERROR = 4

    private val log: LogImp = object : LogImp {

        private var level = LOG_LEVEL_INFO

        override fun v(tag: String, msg: String, vararg obj: Any?) {
            if (level == LOG_LEVEL_VERBOSE) {
                val log = String.format(msg, *obj)
                println(String.format("[V][%s] %s", tag, log.capitalize()))
            }
        }

        override fun d(tag: String, msg: String, vararg obj: Any?) {
            if (level <= LOG_LEVEL_DEBUG) {
                val log = String.format(msg, *obj)
                println(String.format("[D][%s] %s", tag, log.capitalize()))
            }
        }

        override fun i(tag: String, msg: String, vararg obj: Any?) {
            if (level <= LOG_LEVEL_INFO) {
                val log = String.format(msg, *obj)
                println(String.format("[I][%s] %s", tag, log.capitalize()))
            }
        }

        override fun w(tag: String, msg: String, vararg obj: Any?) {
            if (level <= LOG_LEVEL_WARN) {
                val log = String.format(msg, *obj)
                println(String.format("[W][%s] %s", tag, log.capitalize()))
            }
        }

        override fun e(tag: String, msg: String, vararg obj: Any?) {
            if (level <= LOG_LEVEL_ERROR) {
                val log = String.format(msg, *obj)
                println(String.format("[E][%s] %s", tag, log.capitalize()))
            }
        }

        override fun printErrStackTrace(
            tag: String,
            tr: Throwable,
            format: String,
            vararg obj: Any?
        ) {
            var log = String.format(format!!, *obj)
            val sw = StringWriter()
            val pw = PrintWriter(sw)
            tr.printStackTrace(pw)
            log += "  $sw"
            println(String.format("[E][%s] %s", tag, log.capitalize()))
        }

        override fun setLogLevel(logLevel: Int) {
            this.level = logLevel
        }
    }
    private var logImpl = log

    private var level = LOG_LEVEL_INFO

    fun setLogImp(impl: LogImp) {
        logImpl = impl
    }

    val getLogImpl: LogImp
        get() = logImpl

    private val LOG_LEVELS = arrayOf(
        arrayOf("V", "VERBOSE", "0"),
        arrayOf("D", "DEBUG", "1"),
        arrayOf("I", "INFO", "2"),
        arrayOf("W", "WARN", "3"),
        arrayOf("E", "ERROR", "4")
    )

    fun setLogLevel(logLevel: String) {
        for (pattern in LOG_LEVELS) {
            if (pattern[0].equals(logLevel, ignoreCase = true) || pattern[1].equals(
                    logLevel,
                    ignoreCase = true
                )
            ) {
                level = pattern[2].toInt()
            }
        }
        getLogImpl.setLogLevel(
            level
        )
    }

    fun v(tag: String, msg: String, vararg obj: Any?) {
        getLogImpl?.apply {
            this.v(tag, msg, *obj)
        }

    }

    fun e(tag: String, msg: String, vararg obj: Any?) {
        getLogImpl.apply {
            this.e(tag, msg, *obj)
        }
    }

    fun w(tag: String, msg: String, vararg obj: Any?) {
        getLogImpl?.apply {
            this.w(tag, msg, *obj)
        }
    }

    fun i(tag: String, msg: String, vararg obj: Any?) {
        getLogImpl?.apply {
            this.i(tag, msg, *obj)
        }
    }

    fun d(tag: String, msg: String, vararg obj: Any?) {
        getLogImpl?.apply {
            this.d(tag, msg, *obj)
        }
    }

    fun printErrStackTrace(tag: String, tr: Throwable, format: String, vararg obj: Any?) {
        getLogImpl?.apply {
            this.printErrStackTrace(tag, tr, format, *obj)
        }
    }

    interface LogImp {
        fun v(tag: String, msg: String, vararg obj: Any?)
        fun i(tag: String, msg: String, vararg obj: Any?)
        fun w(tag: String, msg: String, vararg obj: Any?)
        fun d(tag: String, msg: String, vararg obj: Any?)
        fun e(tag: String, msg: String, vararg obj: Any?)
        fun printErrStackTrace(tag: String, tr: Throwable, format: String, vararg obj: Any?)
        fun setLogLevel(logLevel: Int)

    }
}