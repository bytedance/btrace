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

import android.annotation.SuppressLint;
import android.os.Looper;
import android.os.Trace;

import com.bytedance.rheatrace.atrace.BinaryTrace;

final class RheaTrace {

    private final static Thread sMainThread = Looper.getMainLooper().getThread();

    static boolean isMainProcess = false;

    static boolean mainThreadOnly = true;

    @SuppressLint("NewApi")
    static void i(String methodId) {
        if (!isMainProcess) {
            return;
        }
        if (mainThreadOnly) {
            if (Thread.currentThread() == sMainThread) {
                Trace.beginSection(methodId);
            }
        } else {
            Trace.beginSection(methodId);
        }
    }

    @SuppressLint("NewApi")
    static void o(String methodId) {
        if (!isMainProcess) {
            return;
        }
        if (mainThreadOnly) {
            if (Thread.currentThread() == sMainThread) {
                Trace.endSection();
            }
        } else {
            Trace.endSection();
        }
    }

    static long i(int methodId) {
        if (!isMainProcess) {
            return -1;
        }
        if (!mainThreadOnly || Thread.currentThread() == sMainThread) {
            return BinaryTrace.beginMethod(methodId);
        }
        return -1;
    }

    static void o(int methodId, long begin) {
        if (!isMainProcess) {
            return;
        }
        if (!mainThreadOnly || Thread.currentThread() == sMainThread) {
            BinaryTrace.endMethod(methodId, begin);
        }
    }
}
