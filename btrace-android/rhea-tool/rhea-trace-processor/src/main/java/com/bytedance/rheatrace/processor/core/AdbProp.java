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
package com.bytedance.rheatrace.processor.core;

import com.bytedance.rheatrace.processor.Adb;

import java.io.IOException;

public class AdbProp {
    public static void setup() throws IOException, InterruptedException {
        Adb.callSafe("shell", "setprop", "persist.traced.enable", "1");
        Arguments arg = Arguments.get();
        Adb.call("shell", "setprop", "debug.rhea.httpServerPort", String.valueOf(arg.port));
        Adb.call("shell", "setprop", "debug.rhea.startWhenAppLaunch", "1");
        Adb.call("shell", "setprop", "debug.rhea.mainThreadOnly", arg.mainThreadOnly ? "1" : "0");
        Adb.call("shell", "setprop", "debug.rhea.methodIdMaxSize", String.valueOf(arg.maxAppTraceBufferSize));
        if (arg.durationThresholdNs != null) {
            Adb.call("shell", "setprop", "debug.rhea.methodDurThreshold", String.valueOf(arg.durationThresholdNs));
        }
        for (String category : arg.categories) {
            Adb.call("shell", "setprop", category, "1");
        }
        Runtime.getRuntime().addShutdownHook(new Thread(AdbProp::teardown));
    }

    private static void teardown() {
        Adb.callSafe("shell", "setprop", "debug.rhea.startWhenAppLaunch", "0");
        Adb.callSafe("shell", "setprop", "debug.rhea.mainThreadOnly", "0");
        Adb.callSafe("shell", "setprop", "debug.rhea.methodDurThreshold", "0");
        Arguments arg = Arguments.get();
        for (String category : arg.categories) {
            Adb.callSafe("shell", "setprop", category, "0");
        }
    }
}
