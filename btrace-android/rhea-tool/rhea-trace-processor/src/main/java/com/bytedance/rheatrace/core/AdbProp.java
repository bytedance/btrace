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

import com.bytedance.rheatrace.Adb;

import java.io.IOException;

public class AdbProp {
    public static void setup() throws IOException, InterruptedException {
        Adb.callSafe("shell", "setprop", "persist.traced.enable", "1");
        Arguments arg = Arguments.get();
        Adb.call("shell", "setprop", "debug.rhea3.waitTraceTimeout", String.valueOf(arg.waitTraceTimeout));
        if (arg.restart || arg.waitStart) {
            Adb.call("shell", "setprop", "debug.rhea3.startWhenAppLaunch", "1");
        }
        if (arg.sampleIntervalNs > 0) {
            Adb.call("shell", "setprop", "debug.rhea3.sampleInterval", String.valueOf(arg.sampleIntervalNs));
        }
        if (arg.maxAppTraceBufferSize > 0) {
            Adb.call("shell", "setprop", "debug.rhea3.methodIdMaxSize", String.valueOf(arg.maxAppTraceBufferSize));
        }
        Runtime.getRuntime().addShutdownHook(new Thread(AdbProp::teardown));
    }

    private static void teardown() {
        Adb.callSafe("shell", "setprop", "debug.rhea3.waitTraceTimeout", "0");
        Adb.callSafe("shell", "setprop", "debug.rhea3.startWhenAppLaunch", "0");
        Adb.callSafe("shell", "setprop", "debug.rhea3.methodIdMaxSize", "0");
        Adb.callSafe("shell", "setprop", "debug.rhea3.sampleInterval", "0");
    }
}
