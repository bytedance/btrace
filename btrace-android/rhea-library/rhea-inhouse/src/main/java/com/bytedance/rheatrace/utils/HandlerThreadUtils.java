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
package com.bytedance.rheatrace.utils;

import android.os.Handler;
import android.os.HandlerThread;

public class HandlerThreadUtils {

    private static HandlerThread sCollectorHandlerThread;
    private static Handler sCollectorHandler;

    private static void initCollectorThread() {
        if (sCollectorHandlerThread == null) {
            sCollectorHandlerThread = new HandlerThread("rhea-collector-thread");
            sCollectorHandlerThread.start();
        }
    }

    public synchronized static Handler getCollectorThreadHandler() {
        if (sCollectorHandlerThread == null) {
            initCollectorThread();
        }
        if (sCollectorHandler == null) {
            sCollectorHandler = new Handler(sCollectorHandlerThread.getLooper());
        }
        return sCollectorHandler;
    }
}
