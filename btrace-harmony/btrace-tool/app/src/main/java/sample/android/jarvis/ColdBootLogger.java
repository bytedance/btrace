/*
 * Copyright (c) 2026 Bytedance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package sample.android.jarvis;

import android.os.Process;

import androidx.annotation.Keep;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

@Keep
public class ColdBootLogger {
    public static final String COLD_BOOT_APPLICATION_ATTACH_DURATION = "cold_boot_application_attach_duration";
    public static final String COLD_BOOT_APPLICATION_ATTACH_TO_CREATE = "cold_boot_application_attach_to_create";
    public static final String COLD_BOOT_APPLICATION_CREATE_DURATION = "cold_boot_application_create_duration";
    public static final String COLD_BOOT_APPLICATION_TO_MAIN = "cold_boot_application_to_main";
    public static final String COLD_BOOT_MAIN_CREATE_DURATION = "cold_boot_main_create_duration";
    public static final String COLD_BOOT_MAIN_CREATE_TO_RESUME = "cold_boot_main_create_to_resume";
    public static final String COLD_BOOT_MAIN_RESUME_DURATION = "cold_boot_main_resume_duration";
    public static final String COLD_BOOT_MAIN_RESUME_TO_FOCUS = "cold_boot_main_resume_to_focus";
    public static final String COLD_BOOT_MAIN_FOCUS_DURATION = "cold_boot_main_focus_duration";
    private static final Map<String, long[]> phase = new HashMap<>();

    public static void begin(String name) {
        dispatch(name, phase, 0);
    }

    public static void end(String name) {
        dispatch(name, phase, 2);
    }

    public static void dispatch(String name, Map<String, long[]> map, int index) {
        long time = System.nanoTime();
        long[] data;
        synchronized (phase) {
            data = phase.get(name);
            if (data == null) {
                data = new long[4];
                phase.put(name, data);
            }
        }
        data[index] = Process.myTid();
        data[index + 1] = time;
    }

    public static Map<String, long[]> getPhase() {
        return Collections.unmodifiableMap(phase);
    }
}