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

import androidx.annotation.NonNull;

import com.bytedance.rheatrace.trace.TraceConfigurations;

/**
 * Abstraction of handling different kinds of traceable perf data's collection.
 *
 * @param <T> Configuration for the collection.
 */
public abstract class TraceAbility<T extends TraceConfig> {

    private int activeCount = 0;
    private long nativeCollectorPtr = 0;

    public synchronized long start() {
        if (activeCount++ == 0) {
            if (nativeCollectorPtr == 0) {
                nativeCollectorPtr = nativeCreate(getMeta().getOffset(), getDeflatedConfigs());
            }
            nativeStart(nativeCollectorPtr, getExtraStartConfig());
        } else {
            nativeUpdateConfig(nativeCollectorPtr, getDeflatedUpdatableConfigs());
        }
        return nativeMark(nativeCollectorPtr);
    }

    public synchronized long stop() {
        long result = nativeMark(nativeCollectorPtr);
        if (--activeCount == 0) {
            nativeStop(nativeCollectorPtr);
        }
        return result;
    }

    public int dumpTokenRange(long start, long end, String path, String extra) {
        TraceMeta meta = getMeta();
        if (meta.isCore()) {
            return nativeDumpTokenRange(nativeCollectorPtr, start, end, path, extra);
        } else {
            return nativeDumpTokenRange(nativeCollectorPtr, start, end, path, null);
        }
    }

    @NonNull
    protected abstract TraceMeta getMeta();

    protected abstract long[] getExtraStartConfig();

    private long[] getDeflatedConfigs() {
        T config = TraceConfigurations.getConfig(getMeta());
        if (config != null) {
            return config.deflate();
        }
        return new long[0];
    }

    private long[] getDeflatedUpdatableConfigs() {
        TraceMeta meta = getMeta();
        T config = TraceConfigurations.getConfig(meta);
        if (config == null) {
            throw new RuntimeException(meta.getName() + " has no config");
        }
        return config.deflateUpdatable();
    }

    private native long nativeCreate(int type, long[] configs);

    private native void nativeStart(long collector, long[] extraConfigs);

    private native void nativeUpdateConfig(long collector, long[] configs);

    private native long nativeMark(long collector);

    private native int nativeDumpTokenRange(long collector, long start, long end, String path, String extra);

    private native void nativeStop(long collector);
}