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

import android.content.Context;
import android.os.Environment;
import android.os.Trace;
import android.util.Log;
import android.widget.Toast;

import androidx.annotation.MainThread;

import com.bytedance.rheatrace.atrace.RheaATrace;
import com.bytedance.rheatrace.common.ReflectUtil;

import java.io.File;
import java.lang.reflect.Method;

class TraceSwitch {

    private static final String TAG = "Rhea:TraceSwitch";

    private static boolean started;

    private static TraceConfiguration traceConfiguration;

    @MainThread
    static void init(Context context) {
        boolean isMainProcess = context.getPackageName().equals(ProcessUtil.getProcessName(context));
        if (!isMainProcess) {
            return;
        }
        traceConfiguration = LocalConfigManager.createRheaConfig(context.getPackageName());
        traceConfiguration.checkConfig();
        RheaTrace.isMainProcess = true;
        RheaTrace.mainThreadOnly = traceConfiguration.mainThreadOnly;
        //using rhea config to decide whether start atrace when launch app.
        if (traceConfiguration.startWhenAppLaunch) {
            Log.d(TAG, "start tracing when launch app.");
            start(context);
        }
        enableAppTracing();
    }

    @MainThread
    static void start(Context context) {
        if (traceConfiguration == null) {
            Log.w(TAG, "TraceSwitch#init is not invoked.");
            return;
        }
        if (started) {
            Log.d(TAG, "RheaTrace has been started, just ignore!");
        } else {
            boolean result = RheaATrace.start(context, getRheaTraceDir(context.getPackageName()),
                    new RheaATrace.Configuration(
                            traceConfiguration.enableIO,
                            traceConfiguration.mainThreadOnly,
                            traceConfiguration.enableMemory,
                            traceConfiguration.enableClassLoad,
                            traceConfiguration.atraceBufferSize,
                            traceConfiguration.blockHookLibs));
            if (result) {
                started = true;
            } else {
                Toast.makeText(context, "unfortunately, start trace failed!", Toast.LENGTH_LONG).show();
            }
        }
    }

    @MainThread
    static void stop(Context context) {
        if (traceConfiguration == null) {
            Log.w(TAG, "TraceSwitch#init is not invoked.");
            return;
        }
        if (started) {
            boolean result = RheaATrace.stop();
            if (result) {
                started = false;
            } else {
                Toast.makeText(context, "unfortunately, stop trace failed!", Toast.LENGTH_LONG).show();
            }
        } else {
            Log.d(TAG, "RheaTrace has not been started, just ignore!");
        }
    }

    static boolean isStarted() {
        return started;
    }

    static File getRheaTraceDir(String packageName) {
        return new File(Environment.getExternalStorageDirectory(), "rhea-trace" + File.separator + packageName);
    }

    private static void enableAppTracing() {
        try {
            Method setAppTracingAllowed_method = ReflectUtil.INSTANCE.getDeclaredMethodRecursive(Trace.class, "setAppTracingAllowed", boolean.class);
            setAppTracingAllowed_method.invoke(null, true);
        } catch (Throwable e) {
            e.printStackTrace();
        }
    }

}
