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

import android.util.Log;

import androidx.annotation.Keep;
import androidx.annotation.RestrictTo;

/**
 * don't modify manually, those set methods would be invoked automatically.
 */
@Keep
@RestrictTo(RestrictTo.Scope.LIBRARY)
public final class TraceRuntimeConfig {

    private static final String TAG = "Rhea:RuntimeConfig";

    private static boolean sMainThreadOnly = false;

    private static boolean sStartWhenAppLaunch = true;

    private static long sATraceBufferSize = 0;

    private TraceRuntimeConfig() {

    }

    static {
        Log.d(TAG, "start to init runtime config");
    }

    @SuppressWarnings("unused")
    public static void setMainThreadOnly(boolean value) {
        TraceRuntimeConfig.sMainThreadOnly = value;
    }

    @SuppressWarnings("unused")
    public static void setStartWhenAppLaunch(boolean value) {
        TraceRuntimeConfig.sStartWhenAppLaunch = value;
    }

    @SuppressWarnings("unused")
    public static void setATraceBufferSize(long aTraceBufferSize) {
        if (aTraceBufferSize == 0) {
            TraceRuntimeConfig.sATraceBufferSize = TraceConfiguration.DEFAULT_BUFFER_SIZE;
        } else if (aTraceBufferSize < TraceConfiguration.MIN_BUFFER_SIZE) {
            TraceRuntimeConfig.sATraceBufferSize = TraceConfiguration.MIN_BUFFER_SIZE;
        } else if (aTraceBufferSize > TraceConfiguration.MAX_BUFFER_SIZE) {
            TraceRuntimeConfig.sATraceBufferSize = TraceConfiguration.MIN_BUFFER_SIZE;
        } else {
            TraceRuntimeConfig.sATraceBufferSize = aTraceBufferSize;
        }
    }

    static boolean isMainThreadOnly() {
        return sMainThreadOnly;
    }

    static boolean isStartWhenAppLaunch() {
        return sStartWhenAppLaunch;
    }

    static long getATraceBufferSize() {
        return sATraceBufferSize;
    }

}
