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

import java.io.File;
import java.io.IOException;
import java.util.List;

public interface OS {

    static OS get() {
        boolean win = System.getProperty("os.name").toLowerCase().contains("win");
        return win ? Windows.INSTANCE : MacOS.INSTANCE;
    }

    String pathSeparator();

    String pathKeyName();

    String adbExecutableName();

    String perfettoScriptName();

    void setExecutable(File bin) throws IOException;

    String ansiReset();

    String ansiRed();

    String ansiBlue();

    boolean isPortFree(int port) throws IOException;

    List<String> buildCommand(String exec, String[] args);
}
