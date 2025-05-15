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
import android.os.SystemClock;
import android.util.Log;

import androidx.annotation.Keep;
import androidx.annotation.MainThread;
import androidx.annotation.RestrictTo;

import com.bytedance.android.bytehook.ByteHook;
import com.bytedance.android.bytehook.ILibLoader;
import com.bytedance.rheatrace.atrace.render.RenderTracer;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

@RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
public class RheaATrace {

    private static final AtomicBoolean jniLoadSuccess = new AtomicBoolean(false);

    static {
        jniLoadSuccess.set(loadJni());
    }

    private static final String TAG = "rhea:atrace";

    private static boolean started = false;

    private static boolean inited = false;
    private static File externalDirectory;

    public static boolean isStartWhenAppLaunch() {
        return nativeStartWhenAppLaunch();
    }

    public static boolean isMainThreadOnly() {
        return nativeMainThreadOnly();
    }

    public static int getHttpServerPort() {
        return nativeGetHttpServerPort();
    }
    @MainThread
    public static boolean start(Context context, File externalDir) {
        externalDirectory = externalDir;
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
        BinaryTrace.init(new File(externalDir, "rhea-atrace.bin"));
        BlockTrace.init();
        int resultCode = nativeStart(externalDir.getAbsolutePath());
        if (resultCode != 1) {
            Log.d(TAG, "failed to start rhea-trace, errno: " + resultCode);
            return false;
        }
        if (nativeRenderCategoryEnabled()) {
            RenderTracer.onTraceStart();
        }
//        TraceEnableTagsHelper.updateEnableTags();
        started = true;
        Log.d(TAG, "trace started");
        return true;
    }

    @MainThread
    public static boolean stop() {
        if (!started) {
            Log.d(TAG, "rhea atrace has not been started!");
            return true;
        }
        BinaryTrace.stop();
        int resultCode = nativeStop();
        if (resultCode != 1) {
            Log.d(TAG, "failed to stop rhea-trace, errno: " + resultCode);
            return false;
        }
        try {
            writeBinderInterfaceTokens();
        } catch (IOException e) {
            Log.e(TAG, "failed to write binder interface tokens", e);
        }
        TraceEnableTagsHelper.updateEnableTags();
        started = false;
        return true;
    }

    private static void writeBinderInterfaceTokens() throws IOException {
        String[] tokens = nativeGetBinderInterfaceTokens();
        if (tokens == null) {
            Log.e(TAG, "writerBinderInterfaceTokens error. may be oom");
            return;
        }
        try (FileWriter writer = new FileWriter(new File(externalDirectory, "binder.txt"))) {
            long now = SystemClock.uptimeMillis();
            for (String token : tokens) {
                writer.write("#");
                writer.write(token);
                writer.write("\n");
                try {
                    // try $Stub first
                    for (Field field : Class.forName(token + "$Stub").getDeclaredFields()) {
                        if (Modifier.isStatic(field.getModifiers()) && field.getType() == int.class) {
                            appendFieldValue(writer, field);
                        }
                    }
                } catch (ClassNotFoundException e) {
                    try {
                        // then fall back to self
                        for (Field field : Class.forName(token).getDeclaredFields()) {
                            if (Modifier.isStatic(field.getModifiers()) && field.getType() == int.class) {
                                appendFieldValue(writer, field);
                            }
                        }
                    } catch (ClassNotFoundException ignore) {
                    } catch (IllegalAccessException ignore) {
                    }
                } catch (IllegalAccessException ignore) {
                }
            }
            long cost = SystemClock.uptimeMillis() - now;
            Log.d(TAG, "writeBinderInterfaceTokens cost " + cost + "ms");
        }
    }

    private static void appendFieldValue(FileWriter writer, Field field) throws IllegalAccessException, IOException {
        field.setAccessible(true);
        Object value = field.get(null);
        if (value instanceof Integer) {
            writer.write(field.getName());
            writer.write(":");
            writer.write(value.toString());
            writer.write("\n");
        }
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

    private static boolean loadJni() {
        if (jniLoadSuccess.get()) {
            return true;
        }
        System.loadLibrary("rhea-trace");
        return true;
    }

    public static int getArch() {
        return nativeGetArch();
    }

    @MainThread
    private static native int nativeStart(String atraceLocation);

    @MainThread
    private static native int nativeStop();

    private static native boolean nativeStartWhenAppLaunch();

    public static native boolean nativeMainThreadOnly();

    public static native boolean nativeRenderCategoryEnabled();

    public static native int nativeGetHttpServerPort();

    private static native String[] nativeGetBinderInterfaceTokens();

    private static native int nativeGetArch();

    public static class Configuration {

        final List<String> categories;

        final boolean mainThreadOnly;
        final long atraceBufferSize;
        final long methodIdBufferSize;

        final String blockHookLibs;

        public Configuration(List<String> categories,
                             boolean mainThreadOnly,
                             long atraceBufferSize,
                             long methodIdBufferSize,
                             String blockHookLibs) {
            this.categories = categories;
            this.mainThreadOnly = mainThreadOnly;
            this.atraceBufferSize = atraceBufferSize;
            this.methodIdBufferSize = methodIdBufferSize;
            this.blockHookLibs = blockHookLibs;
        }
    }

}
