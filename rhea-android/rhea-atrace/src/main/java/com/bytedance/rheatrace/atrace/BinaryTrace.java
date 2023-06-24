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
package com.bytedance.rheatrace.atrace;

import androidx.annotation.Keep;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;

import dalvik.annotation.optimization.CriticalNative;

/**
 * Created by caodongping on 2022/9/27
 *
 * @author caodongping@bytedance.com
 */
@Keep
public class BinaryTrace {
    private static final String TAG = "BinaryTrace";
    private static volatile boolean on;

    public static void init(File file) {
        nativeInit(file.getAbsolutePath());
        on = true;
    }

    public static String getThreadsInfo() {
        return nativeGetThreadsInfo();
    }

    public static long beginMethod(int methodID) {
        if (on) {
            return nativeBeginMethod(methodID);
        }
        return -1;
    }

    public static void endMethod(int methodId, long begin) {
        if (on) {
            nativeEndMethod(methodId, begin);
        }
    }

    public static void stop() {
        on = false;
        nativeStop();
    }

    public static long currentBufferUsage() {
        return nativeGetCurrentAppTraceBufferSize();
    }

    private static native void nativeInit(String filepath);

    private static native void nativeStop();

    private static native String nativeGetThreadsInfo();

    @CriticalNative
    private static native long nativeBeginMethod(int methodId);

    @CriticalNative
    private static native void nativeEndMethod(int methodId, long beginTime);

    private static native long nativeGetCurrentAppTraceBufferSize();

    private static native long nativeGetMaxAppTraceBufferSize();

    public static String debugInfo() throws JSONException {
        JSONObject json = new JSONObject();
        JSONObject traceBuffer = new JSONObject();
        traceBuffer.put("currentSize", nativeGetCurrentAppTraceBufferSize());
        traceBuffer.put("maxSize", nativeGetMaxAppTraceBufferSize());
        json.put("appTraceBuffer", traceBuffer);
        return json.toString();
    }
}
