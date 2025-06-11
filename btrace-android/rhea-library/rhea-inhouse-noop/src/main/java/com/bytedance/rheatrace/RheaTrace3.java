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
package com.bytedance.rheatrace;

import android.content.Context;

/**
 * This is the only API class exposed externally by RheaTrace. Users are not expected to rely on
 * other classes beyond this one.
 * <p>
 * Note: RheaTrace only support main process tracing.
 */
public class RheaTrace3 {


    /**
     * This is the entry point api for RheaTrace. The initialization stage includes two aspects:
     * 1. If we are tracing app cold launch stage, we will start tracing immediately.
     * 2. Start a http server for receiving latter tracing commands, such as start/stop tracing.
     *
     * @param context used for initializing tracing data directory
     */
    public static void init(Context context) {
        // there is nothing we should do, because this is the noop version.
    }

    /**
     * Capture current stacktrace manually and save into RheaTrace sampling buffer for latter
     * transforming to trace data.
     * <p>
     * RheaTrace3 is a stacktrace based tracing tool. Inside of it, we've built in some hook points
     * for active stack capturing. However, these hook points can't cover the execution of all
     * methods. So, we offer this method to users. Users can decide to invoke it within appropriate
     * methods to make up for the methods that our hook points fail to cover.
     *
     * @param force if true, we will ignore the stacktrace capture interval limit and force capture a stacktrace.
     */
    public static void captureStackTrace(boolean force) {
        //
    }
}