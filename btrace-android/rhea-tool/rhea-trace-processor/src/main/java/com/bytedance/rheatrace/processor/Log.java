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
package com.bytedance.rheatrace.processor;

import com.bytedance.rheatrace.processor.core.Debug;
import com.bytedance.rheatrace.processor.os.OS;

import java.text.DateFormat;
import java.text.SimpleDateFormat;

/**
 * Created by caodongping on 2023/2/9
 *
 * @author caodongping@bytedance.com
 */
public class Log {
    private static final String ANSI_RESET = OS.get().ansiReset();
    private static final String ANSI_RED = OS.get().ansiRed();
    private static final String ANSI_BLUE = OS.get().ansiBlue();
    @SuppressWarnings("SimpleDateFormat")
    private static final DateFormat FORMAT = new SimpleDateFormat("MM-dd HH:mm:ss.SSS");

    public static void d(String msg) {
        if (Debug.isDebug()) {
            log('D', ANSI_RESET, msg);
        }
    }

    public static void i(String msg) {
        log('I', ANSI_RESET, msg);
    }

    public static void e(String msg) {
        log('E', ANSI_RED, msg);
    }

    public static void w(String msg) {
        log('W', ANSI_RED, msg);
    }

    public static void blue(String msg) {
        log(' ', ANSI_BLUE, msg);
    }

    public static void red(String msg) {
        log(' ', ANSI_RED, msg);
    }

    public static void log(char level, String color, String msg) {
        System.out.println(FORMAT.format(System.currentTimeMillis()) + " " + level + " RheaTrace : " + color + msg + ANSI_RESET);
    }
}
