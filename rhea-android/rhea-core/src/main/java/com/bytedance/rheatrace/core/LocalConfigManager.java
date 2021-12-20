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

package com.bytedance.rheatrace.core;

import android.os.Environment;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Properties;

class LocalConfigManager {

    private static final String TAG = "rhea:config";

    private static final String RHEA_CONFIG_FILE_NAME = "rheatrace.config";

    private static final String IO = "io";

    private static final String MAIN_THREAD_ONLY = "mainThreadOnly";

    private static final String MEMORY = "memory";

    private static final String CLASS_LOAD = "classLoad";

    private static final String START_WHEN_APP_LAUNCH = "startWhenAppLaunch";

    private static final String ATRACE_BUFFER_SIZE = "atraceBufferSize";

    private static final String BLOCK_HOOK_LIBS = "blockHookLibs";

    /**
     * create rhea config from local.
     */
    @NonNull
    static TraceConfiguration createRheaConfig(String packageName) {
        File rheaConfigDir = new File(Environment.getExternalStorageDirectory(), "rhea-trace" + File.separator + packageName);
        TraceConfiguration traceConfiguration = LocalConfigManager.readConfigFile(rheaConfigDir);
        if (traceConfiguration == null) {
            traceConfiguration = new TraceConfiguration.Builder()
                    .setMainThreadOnly(TraceRuntimeConfig.isMainThreadOnly())
                    .startWhenAppLaunch(TraceRuntimeConfig.isStartWhenAppLaunch())
                    .setATraceBufferSize(TraceRuntimeConfig.getATraceBufferSize())
                    .build();
        }
        Log.d(TAG, "trace config: " + traceConfiguration.toString());
        return traceConfiguration;
    }

    private static TraceConfiguration readConfigFile(File externalDir) {
        if (externalDir.exists()) {
            File configFile = new File(externalDir, RHEA_CONFIG_FILE_NAME);
            if (configFile.exists() && configFile.length() > 0) {
                return readProperties(configFile);
            } else {
                Log.w(TAG, externalDir + File.separator + "rheatrace.config is illegal!");
            }
        } else {
            Log.w(TAG, externalDir.getPath() + " is not exit!");
        }
        return null;
    }

    private static TraceConfiguration readProperties(File file) {
        boolean isReadPatchSuccessful = false;
        int numAttempts = 0;
        String io = null;
        String mainThreadOnly = null;
        String memory = null;
        String classLoad = null;
        String startWhenAppLaunch = null;
        String atraceBufferSize = null;
        String blockHookLibs = null;
        while (numAttempts < 3 && !isReadPatchSuccessful) {
            numAttempts++;
            Properties properties = new Properties();
            FileInputStream inputStream = null;
            try {
                inputStream = new FileInputStream(file);
                properties.load(inputStream);
                io = properties.getProperty(IO);
                mainThreadOnly = properties.getProperty(MAIN_THREAD_ONLY);
                memory = properties.getProperty(MEMORY);
                classLoad = properties.getProperty(CLASS_LOAD);
                startWhenAppLaunch = properties.getProperty(START_WHEN_APP_LAUNCH);
                atraceBufferSize = properties.getProperty(ATRACE_BUFFER_SIZE);
                blockHookLibs = properties.getProperty(BLOCK_HOOK_LIBS);
            } catch (IOException e) {
                Log.w(TAG, "read property failed, e:" + e);
            } finally {
                try {
                    if (inputStream != null) {
                        inputStream.close();
                    }
                } catch (Throwable ignored) {

                }
            }
            if (io == null
                    && mainThreadOnly == null
                    && startWhenAppLaunch == null
                    && blockHookLibs == null
                    && atraceBufferSize == null) {
                continue;
            }
            isReadPatchSuccessful = true;
        }
        if (isReadPatchSuccessful) {
            long atraceBufferSizeValue = 0;
            try {
                atraceBufferSizeValue = Long.parseLong(atraceBufferSize);
            } catch (NumberFormatException ignored) {

            }
            //remove all spaces.
            if (blockHookLibs != null) {
                blockHookLibs = blockHookLibs.replace(" ", "");
            }
            if (TextUtils.isEmpty(blockHookLibs)) {
                blockHookLibs = null;
            }
            return new TraceConfiguration.Builder()
                    .enableIO(!"false".equals(io))
                    .setMainThreadOnly("true".equals(mainThreadOnly))
                    .enableMemory("true".equals(memory))
                    .enableClassLoad("true".equals(classLoad))
                    .startWhenAppLaunch(!"false".equals(startWhenAppLaunch))
                    .setATraceBufferSize(atraceBufferSizeValue)
                    .setBlockHookLibs(blockHookLibs)
                    .build();
        }
        return null;
    }
}
