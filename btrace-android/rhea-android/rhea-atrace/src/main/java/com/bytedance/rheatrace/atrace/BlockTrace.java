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

import android.annotation.SuppressLint;

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

/**
 * monitor park/unpark wait/notify/notifyAll etc
 *
 * @author caodongping@bytedance.com
 */
public class BlockTrace {

    @SuppressLint("SoonBlockedPrivateApi")
    public static void init() {
        nativePlaceHolder();
        try {
            Method nativePlaceHolder = BlockTrace.class.getDeclaredMethod("nativePlaceHolder");
            Method wait = Object.class.getMethod("wait", long.class, int.class);
            Method notify = Object.class.getMethod("notify");
            Method notifyAll = Object.class.getMethod("notifyAll");
            Method identityHashCodeNative;
            try {
                identityHashCodeNative = Object.class.getDeclaredMethod("identityHashCodeNative", Object.class);
            } catch (NoSuchMethodException e) {
                identityHashCodeNative = System.class.getDeclaredMethod("identityHashCode", Object.class);
            }
            if (!Modifier.isNative(identityHashCodeNative.getModifiers())) {
                throw new RuntimeException(identityHashCodeNative + " is not native");
            }
            Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
            Method park = unsafeClass.getMethod("park", boolean.class, long.class);
            Method unpark = unsafeClass.getMethod("unpark", Object.class);
            if (Modifier.isNative(wait.getModifiers()) &&
                    Modifier.isNative(notify.getModifiers()) &&
                    Modifier.isNative(notifyAll.getModifiers()) &&
                    Modifier.isNative(park.getModifiers()) &&
                    Modifier.isNative(unpark.getModifiers())) {
                // Some device may make park/unpark not native. eg. OPPO R11 android 9
                nativeInit(nativePlaceHolder, identityHashCodeNative, wait, notify, notifyAll, park, unpark);
            }
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }

    private static native void nativeInit(Method nativePlaceHolder, Method identityHashCodeNative, Method wait, Method notify, Method notifyAll, Method park, Method unpark);

    private static native void nativePlaceHolder();
}
