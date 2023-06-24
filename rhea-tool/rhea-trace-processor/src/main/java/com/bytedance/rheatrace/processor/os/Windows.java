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
package com.bytedance.rheatrace.processor.os;

import com.bytedance.rheatrace.processor.Log;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

class Windows implements OS {
    static final OS INSTANCE = new Windows();

    @Override
    public String pathSeparator() {
        return ";";
    }

    @Override
    public String pathKeyName() {
        return "Path";
    }

    @Override
    public String adbExecutableName() {
        return "adb.exe";
    }

    @Override
    public String perfettoScriptName() {
        return "record_android_trace_win";
    }

    @Override
    public void setExecutable(File bin) throws IOException {
        if (!bin.setExecutable(true)) {
            Log.i("make Perfetto executable failed.");
        }
    }

    @Override
    public String ansiReset() {
        return "";
    }

    @Override
    public String ansiRed() {
        return "";
    }

    @Override
    public String ansiBlue() {
        return "";
    }

    @Override
    public boolean isPortFree(int port) throws IOException {
        // no check yet
        return true;
    }

    @Override
    public List<String> buildCommand(String exec, String[] args) {
        List<String> cmd = new ArrayList<>();
        cmd.add("python3");
        cmd.add(exec);
        cmd.add("-no_open");
        cmd.addAll(Arrays.asList(args));
        return cmd;
    }
}
