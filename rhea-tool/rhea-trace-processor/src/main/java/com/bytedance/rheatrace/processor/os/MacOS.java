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

import org.apache.commons.io.IOUtils;

import java.io.File;
import java.io.IOException;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.attribute.PosixFilePermission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

class MacOS implements OS {
    static final OS INSTANCE = new MacOS();

    @Override
    public String pathSeparator() {
        return ":";
    }

    @Override
    public String pathKeyName() {
        return "PATH";
    }

    @Override
    public String adbExecutableName() {
        return "adb";
    }

    @Override
    public String perfettoScriptName() {
        return "record_android_trace";
    }

    @Override
    public void setExecutable(File bin) throws IOException {
        Set<PosixFilePermission> perms = new HashSet<>();
        perms.add(PosixFilePermission.OWNER_EXECUTE);
        perms.add(PosixFilePermission.OWNER_READ);
        perms.add(PosixFilePermission.OWNER_WRITE);
        Files.setPosixFilePermissions(bin.toPath(), perms);
    }

    @Override
    public String ansiReset() {
        return "\u001B[0m";
    }

    @Override
    public String ansiRed() {
        return "\u001B[31m";
    }

    @Override
    public String ansiBlue() {
        return "\u001B[34m";
    }

    @Override
    public boolean isPortFree(int port) throws IOException {
        Process lsof = Runtime.getRuntime().exec(new String[]{"lsof", "-i:" + port});
        List<String> lsofResult = IOUtils.readLines(lsof.getInputStream(), Charset.defaultCharset());
        return lsofResult.isEmpty();
    }

    @Override
    public List<String> buildCommand(String exec, String[] args) {
        List<String> cmd = new ArrayList<>();
        cmd.add(exec);
        cmd.add("-no_open");
        cmd.addAll(Arrays.asList(args));
        return cmd;
    }
}
