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
package com.bytedance.rheatrace.trace.base;

import android.os.Looper;
import android.util.Log;

import com.bytedance.rheatrace.trace.utils.JNIHook;

public class TraceGlobal {

    private static final String TAG = "RheaTrace:Deps";

    private static boolean success;

    static {
        try {
            System.loadLibrary("rheatrace");
            nativeInit(Looper.getMainLooper().getThread());
            JNIHook.init();
            success = true;
        } catch (Exception e) {
            Log.e(TAG, "rhea-trace init failed: ", e);
            success = false;
        }
    }

    public static boolean init() {
        return success;
    }

    private static native void nativeInit(Thread mainThread);

    public static void capture(boolean force) {
        if (success) {
            nativeCapture(force);
        }
    }

    private static native void nativeCapture(boolean force);
}