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

import groovy.util.XmlSlurper
import groovy.util.slurpersupport.GPathResult
import java.io.File

object ManifestUtil {

    fun readApplicationName(manifestFile: File): String {
        if (!manifestFile.exists()) {
            return ""
        }
        XmlSlurper().parse(manifestFile).children().list().forEach {
            val result = it as GPathResult
            if (result.name() == "application" && result.getProperty("@android:name").toString().isNotEmpty()) {
                return result.getProperty("@android:name").toString()
            }
        }
        return ""
    }
}