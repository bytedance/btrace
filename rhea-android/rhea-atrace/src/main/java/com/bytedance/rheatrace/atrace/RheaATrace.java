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

import android.content.Context;
import android.os.Build;
import android.util.Log;

import androidx.annotation.Keep;
import androidx.annotation.MainThread;
import androidx.annotation.RestrictTo;

import com.bytedance.android.bytehook.ByteHook;
import com.bytedance.android.bytehook.ILibLoader;
import com.bytedance.rheatrace.common.ReflectUtil;

import java.io.File;
import java.lang.reflect.Method;

@RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
public class RheaATrace {

    private static final String TAG = "rhea:atrace";

    private static boolean started = false;

    private static boolean inited = false;

    @MainThread
    public static boolean start(Context context, File externalDir, Configuration config) {
        if (started) {
            Log.d(TAG, "rhea atrace has been started!");
            return true;
        }
        if (!init()) {
            return false;
        }
        if (!externalDir.exists()) {
            if (!externalDir.mkdirs()) {
                Log.e(TAG, "failed to create directory " + externalDir.getAbsolutePath());
                return false;
            }
        }

        if (config.enableMemory || config.enableClassLoad) {
            if (Build.VERSION.SDK_INT > Build.VERSION_CODES.O) {
                try {
                    Method attachAgentMethod = ReflectUtil.INSTANCE.getDeclaredMethodRecursive(
                            "android.os.Debug", "attachJvmtiAgent", String.class, String.class, ClassLoader.class);
                    attachAgentMethod.invoke(null, "librhea-trace.so", null, context.getClassLoader());
                } catch (Exception e) {
                    e.printStackTrace();
                }
            } else {
                Log.w(TAG, "device below android P, failed to trace memory");
            }
        }
        int resultCode = nativeStart(externalDir.getAbsolutePath(), config.blockHookLibs, config.atraceBufferSize, assembleATraceConfig(config));
        if (resultCode != 1) {
            Log.d(TAG, "failed to start rhea-trace, errno: " + resultCode);
        } else {
            if (TraceEnableTagsHelper.updateEnableTags()) {
                started = true;
                return true;
            }
        }
        return false;
    }

    @MainThread
    public static boolean stop() {
        if (!started) {
            Log.d(TAG, "rhea atrace has not been started!");
            return true;
        }
        int resultCode = nativeStop();
        if (resultCode != 1) {
            Log.d(TAG, "failed to stop rhea-trace, errno: " + resultCode);
        } else {
            if (TraceEnableTagsHelper.updateEnableTags()) {
                started = false;
                return true;
            }
        }
        return false;
    }

    private static boolean init() {
        if (inited) {
            return true;
        }
        if (!loadJni()) {
            return false;
        }
        int retCode = ByteHook.init(new ByteHook.ConfigBuilder().setLibLoader(new ILibLoader() {
            @Override
            public void loadLibrary(String libName) {
                RheaATrace.loadLib(libName);
            }
        }).build());
        if (retCode != 0) {
            Log.d(TAG, "bytehook init failed, errno: " + retCode);
            return false;
        }
        inited = true;
        return true;
    }

    @Keep
    private static void loadLib(String libName) {
        try {
            System.loadLibrary(libName);
        } catch (Throwable e) {
            throw new UnsatisfiedLinkError("failed to load bytehook lib:" + libName);
        }
    }

    private static int assembleATraceConfig(Configuration config) {
        int atraceConfig = 0;
        if (config.enableIO) {
            atraceConfig = atraceConfig | 1;
        }
        if (config.mainThreadOnly) {
            atraceConfig = atraceConfig | 2;
        }
        if (config.enableMemory) {
            atraceConfig = atraceConfig | 4;
        }
        if (config.enableClassLoad) {
            atraceConfig = atraceConfig | 8;
        }
        return atraceConfig;
    }

    private static boolean loadJni() {
        try {
            System.loadLibrary("rhea-trace");
        } catch (Throwable error) {
            Log.d(TAG, "failed to load rhea-trace so.");
            return false;
        }
        return true;
    }

    @MainThread
    private static native int nativeStart(String atraceLocation, String blockHookLibs, long bufferSize, int atraceConfig);

    @MainThread
    private static native int nativeStop();

    public static class Configuration {

        final boolean enableIO;

        final boolean mainThreadOnly;

        final boolean enableMemory;

        final boolean enableClassLoad;

        final long atraceBufferSize;

        final String blockHookLibs;

        public Configuration(boolean enableIO,
                             boolean mainThreadOnly,
                             boolean enableMemory,
                             boolean enableClassLoad,
                             long atraceBufferSize,
                             String blockHookLibs) {
            this.enableIO = enableIO;
            this.mainThreadOnly = mainThreadOnly;
            this.enableMemory = enableMemory;
            this.enableClassLoad = enableClassLoad;
            this.atraceBufferSize = atraceBufferSize;
            this.blockHookLibs = blockHookLibs;
        }
    }

}
