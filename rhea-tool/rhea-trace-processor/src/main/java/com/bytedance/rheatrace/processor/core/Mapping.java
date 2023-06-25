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
package com.bytedance.rheatrace.processor.core;

import com.bytedance.rheatrace.processor.Adb;
import com.bytedance.rheatrace.processor.Log;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.LineIterator;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

public class Mapping {
    private static final File MAPPING_DIR = new File(System.getProperty("user.home"), ".rheatrace_mapping");
    private static final Map<Integer, String> MAPPING = new HashMap<>();
    private static final long MAPPING_EXPIRED_TIME_MILLIS = 7 * 24 * 3600 * 1000L;

    public static Map<Integer, String> get() throws IOException {
        if (MAPPING.isEmpty()) {
            loadMapping();
        }
        return MAPPING;
    }

    private static File selectMapping() throws IOException {
        // 1. mapping in cmd
        if (Arguments.get().mappingPath != null) {
            return new File(Arguments.get().mappingPath);
        }
        // 2. mapping in cache
        String version = Adb.Http.get("?name=mappingVersion");
        version = version.substring(1);
        File file = new File(MAPPING_DIR, version);
        if (file.exists()) {
            Log.i("reuse cached mapping:" + file);
        } else {
            // 3. mapping in apk
            Log.i("downloading mapping");
            Adb.Http.download("mapping", file);
        }
        cleanCachedMappings(file);
        return file;
    }

    private static void cleanCachedMappings(File whiteList) throws IOException {
        long now = System.currentTimeMillis();
        File[] files = MAPPING_DIR.listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.equals(whiteList)) {
                    continue;
                }
                if (now - file.lastModified() > MAPPING_EXPIRED_TIME_MILLIS) {
                    Log.d("clear expired mapping file:" + file);
                    FileUtils.delete(file);
                }
            }
        }
    }

    private static void loadMapping() throws IOException {
        boolean fullClassName = Arguments.get().fullClassName;
        for (LineIterator it = FileUtils.lineIterator(selectMapping()); it.hasNext(); ) {
            // 1,17,rhea.sample.android.app.SecondFragment$onViewCreated$1 onClick (Landroid/view/View;)V
            String line = it.next().trim();
            if (line.isEmpty() || line.startsWith("#")) {
                continue;
            }
            try {
                int first = line.indexOf(',');
                int second = line.indexOf(',', first + 1);
                String[] comps = line.substring(second + 1).split(" ");
                String className = comps[0];
                if (!fullClassName) {
                    className = className.substring(className.lastIndexOf('.') + 1);
                }
                String methodName = comps[1];
                MAPPING.put(Integer.parseInt(line.substring(0, first)), className + ":" + methodName);
            } catch (Throwable e) {
                throw new TraceError(e.getMessage() + ". Bad mapping line:" + line, "you may pass a wrong mapping file.", e);
            }
        }
    }
}
