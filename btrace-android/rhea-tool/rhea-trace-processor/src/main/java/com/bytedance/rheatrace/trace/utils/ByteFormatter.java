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
package com.bytedance.rheatrace.trace.utils;

import java.util.Locale;

public class ByteFormatter {

    public static String format(long bytes) {
        if (bytes < 1024) {
            return bytes + " B";
        } else if (bytes < 1024 * 1024) {
            double kb = (double) bytes / 1024;
            return String.format(Locale.getDefault(), "%.2f KB", kb);
        } else if (bytes < 1024 * 1024 * 1024) {
            double mb = (double) bytes / (1024 * 1024);
            return String.format(Locale.getDefault(), "%.2f MB", mb);
        } else {
            double gb = (double) bytes / (1024 * 1024 * 1024);
            return String.format(Locale.getDefault(), "%.2f GB", gb);
        }
    }
}