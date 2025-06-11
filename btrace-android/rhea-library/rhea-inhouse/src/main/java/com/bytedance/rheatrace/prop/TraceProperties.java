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
package com.bytedance.rheatrace.prop;

import android.annotation.SuppressLint;

import com.bytedance.rheatrace.trace.sampling.SamplingConfig;

import java.lang.reflect.Method;

public class TraceProperties {

    private static final String KEY_TRACE_ON_LAUNCH = "debug.rhea3.startWhenAppLaunch";
    private static final String KEY_WAIT_TRACE_TIMEOUT = "debug.rhea3.waitTraceTimeout";
    private static final String KEY_BUFFER_SIZE = "debug.rhea3.methodIdMaxSize";
    private static final String KEY_SAMPLE_INTERVAL = "debug.rhea3.sampleInterval";

    private static final int DEFAULT_WAIT_TRACE_TIMEOUT_SECONDS = 20;

    public static boolean shouldStartWhenAppLaunch() {
        String startWhenAppLaunchStr = Fetcher.fetch(KEY_TRACE_ON_LAUNCH);
        if (startWhenAppLaunchStr == null) {
            return false;
        }
        return startWhenAppLaunchStr.equals("1");
    }

    public static int getWaitTraceTimeoutSeconds() {
        String timeoutStr = Fetcher.fetch(KEY_WAIT_TRACE_TIMEOUT);
        if (timeoutStr == null) {
            return DEFAULT_WAIT_TRACE_TIMEOUT_SECONDS;
        }
        try {
            return Integer.parseInt(timeoutStr);
        } catch (Exception e) {
            return DEFAULT_WAIT_TRACE_TIMEOUT_SECONDS;
        }
    }

    public static int getCoreBufferSize() {
        return getCoreBufferSizeOrDefault(SamplingConfig.OFFLINE_BUFFER_SIZE_DEFAULT);
    }

    public static int getCoreBufferSizeOrDefault(int defaultSize) {
        String bufferSizeStr = Fetcher.fetch(KEY_BUFFER_SIZE);
        if (bufferSizeStr == null) {
            return defaultSize;
        }
        try {
            int bufferSize = Integer.parseInt(bufferSizeStr);
            return bufferSize > 0 ? bufferSize : defaultSize;
        } catch (Exception e) {
            return defaultSize;
        }
    }

    public static long getSampleIntervalOrDefault(long defaultIntervalNs) {
        String sampleIntervalStr = Fetcher.fetch(KEY_SAMPLE_INTERVAL);
        if (sampleIntervalStr == null) {
            return defaultIntervalNs;
        }
        try {
            long interval = Long.parseLong(sampleIntervalStr);
            if (interval > 0) {
                return interval;
            }
        } catch (Exception e) {
            return defaultIntervalNs;
        }
        return defaultIntervalNs;
    }

    private static class Fetcher {
        private static Method sGetPropertiesMethod = null;

        public static String fetch(String key) {
            String value = null;
            try {
                if (sGetPropertiesMethod == null) {
                    @SuppressLint("PrivateApi")
                    Class<?> systemPropertiesClass = Class.forName("android.os.SystemProperties");
                    sGetPropertiesMethod = systemPropertiesClass.getMethod("get", String.class);
                }
                value = (String) sGetPropertiesMethod.invoke(null, key);
            } catch (Exception ignored) {
            }
            return value;
        }
    }
}