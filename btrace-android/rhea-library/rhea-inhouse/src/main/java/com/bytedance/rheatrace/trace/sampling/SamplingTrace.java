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
package com.bytedance.rheatrace.trace.sampling;

import androidx.annotation.NonNull;

import com.bytedance.rheatrace.trace.base.TraceAbility;
import com.bytedance.rheatrace.trace.base.TraceMeta;

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

public class SamplingTrace extends TraceAbility<SamplingConfig> {

    private long[] startConfigs = null;

    @NonNull
    @Override
    protected TraceMeta getMeta() {
        return TraceMeta.Sampling;
    }

    @Override
    protected long[] getExtraStartConfig() {
        if (startConfigs == null) {
            long[] configArray = new long[7];
            configArray[0] = isNativeMethod(Object.class, "wait", long.class, int.class) ? 1 : 0;
            try {
                Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
                configArray[1] = isNativeMethod(unsafeClass, "park", boolean.class, long.class) ? 1 : 0;
                configArray[2] = isNativeMethod(unsafeClass, "unpark", Object.class) ? 1 : 0;
            } catch (Exception ignore) {
            }
            try {
                Class<?> unsafeClass = Class.forName("jdk.internal.misc.Unsafe");
                configArray[3] = isNativeMethod(unsafeClass, "park", boolean.class, long.class) ? 1 : 0;
                configArray[4] = isNativeMethod(unsafeClass, "unpark", Object.class) ? 1 : 0;
            } catch (Exception ignore) {
            }
            configArray[5] = isNativeMethod(Object.class, "notify") ? 1 : 0;
            configArray[6] = isNativeMethod(Object.class, "notifyAll") ? 1 : 0;
            startConfigs = configArray;
        }
        return startConfigs;
    }

    private static boolean isNativeMethod(Class<?> clazz, String name, Class<?>... parameterTypes) {
        try {
            Method method = clazz.getMethod(name, parameterTypes);
            return Modifier.isNative(method.getModifiers());
        } catch (Exception e) {
            return false;
        }
    }
}