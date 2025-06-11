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

import com.bytedance.rheatrace.core.Arguments;
import com.bytedance.rheatrace.core.Debug;
import com.bytedance.rheatrace.core.TraceError;
import com.bytedance.rheatrace.os.OS;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang3.StringUtils;

import java.io.File;
import java.io.IOException;
import java.io.StringWriter;
import java.io.Writer;
import java.net.URL;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class Adb {
    private static String serial;
    private static boolean connected;

    public static void init(String[] args) throws IOException, InterruptedException {
        for (int i = 0; i < args.length - 1; i++) {
            String arg = args[i];
            if (arg.equals("-s")) {
                serial = args[i + 1];
                break;
            }
        }
        String devices = callString("devices").trim();
        int deviceCount = devices.split("\n").length - 1;
        if (deviceCount > 1 && serial == null) {
            throw new TraceError("you have " + deviceCount + " devices connected. " + devices, "please select your device with -s $serial");
        } else if (deviceCount == 0) {
            throw new TraceError("you have no device connected.", null);
        }
        connected = true;
    }

    public static boolean isConnected() {
        return connected;
    }

    private static class AdbPathResolver {
        private static String sAdbPath;
        private static String sAdbDir;

        static {
            OS os = OS.get();
            String path = System.getenv(os.pathKeyName());
            String[] paths = path.split(os.pathSeparator());
            for (String p : paths) {
                File file = new File(p, os.adbExecutableName());
                if (file.exists()) {
                    sAdbPath = file.getAbsolutePath();
                    sAdbDir = p;
                    Log.d("Got adb path: " + sAdbPath);
                    break;
                }
            }
            if (sAdbPath == null) {
                throw new TraceError("adb not found in PATH", "check you have export PATH with `$DIR/Android/sdk/platform-tools`.");
            }
        }
    }

    public static String getAdbDirPath() {
        return AdbPathResolver.sAdbDir;
    }

    public static String callString(String... cmd) throws IOException, InterruptedException {
        StringWriter writer = new StringWriter();
        call(writer, cmd);
        return writer.toString();
    }

    public static void call(String... cmd) throws IOException, InterruptedException {
        call(null, cmd);
    }

    public static void callSafe(String... cmd) {
        try {
            call(null, cmd);
        } catch (Throwable ignore) {
        }
    }

    public static boolean canRunAsSu() {
        try {
            return callWithRet(null, "shell", "su", "-c", "ps") == 0;
        } catch (Exception e) {
            return false;
        }
    }

    private static void call(Writer writer, String... cmds) throws IOException, InterruptedException {
        List<String> errors = new ArrayList<>();
        String[] cmd;
        if (serial != null) {
            cmd = new String[2 + cmds.length];
            cmd[0] = "-s";
            cmd[1] = serial;
            System.arraycopy(cmds, 0, cmd, 2, cmds.length);
        } else {
            cmd = cmds;
        }
        Log.d("Run adb " + StringUtils.join(cmd, " "));
        String[] array = new String[cmd.length + 1];
        array[0] = AdbPathResolver.sAdbPath;
        System.arraycopy(cmd, 0, array, 1, cmd.length);
        Process process = Runtime.getRuntime().exec(array);
        for (String line : IOUtils.readLines(process.getInputStream(), StandardCharsets.UTF_8)) {
            if (writer != null) {
                writer.write(line);
                writer.write('\n');
            } else {
                Log.d(line);
            }
        }
        for (String error : IOUtils.readLines(process.getErrorStream(), StandardCharsets.UTF_8)) {
            if (writer != null) {
                writer.write(error);
                writer.write('\n');
            } else {
                Log.d(error);
                errors.add(error);
            }
        }
        int code = process.waitFor();
        if (code != 0) {
            throw new TraceError("adb " + String.join(" ", cmd) + " return " + code + "." + String.join(". ", errors), "have you connect your device via usb.");
        }
    }

    private static int callWithRet(Writer writer, String... cmds) throws IOException, InterruptedException {
        String[] cmd;
        if (serial != null) {
            cmd = new String[2 + cmds.length];
            cmd[0] = "-s";
            cmd[1] = serial;
            System.arraycopy(cmds, 0, cmd, 2, cmds.length);
        } else {
            cmd = cmds;
        }
        Log.d("Run adb " + StringUtils.join(cmd, " "));
        String[] array = new String[cmd.length + 1];
        array[0] = AdbPathResolver.sAdbPath;
        System.arraycopy(cmd, 0, array, 1, cmd.length);
        Process process = Runtime.getRuntime().exec(array);
        for (String line : IOUtils.readLines(process.getInputStream(), StandardCharsets.UTF_8)) {
            if (writer != null) {
                writer.write(line);
                writer.write('\n');
            } else {
                Log.d(line);
            }
        }
        for (String error : IOUtils.readLines(process.getErrorStream(), StandardCharsets.UTF_8)) {
            if (writer != null) {
                writer.write(error);
                writer.write('\n');
            }
        }
        return process.waitFor();
    }


    public static class Http {

        public static void downloadSafe(String fileName, File destination) {
            try {
                download(fileName, destination);
            } catch (Throwable e) {
                Log.e(e.getMessage());
            }
        }

        public static void download(String fileName, File destination) throws IOException {
            try {
                Arguments arg = Arguments.get();
                FileUtils.copyInputStreamToFile(new URL("http://localhost:" + arg.port + "?action=download&name=" + fileName).openStream(), destination);
            } catch (Throwable e) {
                String innerError;
                try {
                    innerError = get("?action=query&name=error");
                } catch (Throwable innerE) {
                    innerError = "unknown";
                }
                throw new TraceError("download file " + fileName + " error: " + innerError, "check your app is integrated with btrace3.0 and is running.", e);
            }
        }

        public static void downloadSafeWithoutCheck(String fileName, File destination) throws IOException {
            try {
                Arguments arg = Arguments.get();
                FileUtils.copyInputStreamToFile(new URL("http://localhost:" + arg.port + "?name=" + fileName).openStream(), destination);
            } catch (Throwable ignored) {
            }
        }

        public static String get(String path) throws IOException {
            Arguments arg = Arguments.get();
            return IOUtils.toString(new URL("http://localhost:" + arg.port + path), Charset.defaultCharset());
        }

        public static String getSafe(String path) {
            try {
                Arguments arg = Arguments.get();
                return IOUtils.toString(new URL("http://localhost:" + arg.port + path), Charset.defaultCharset());
            } catch (Throwable ignored) {
            }
            return null;
        }

        private static void checkAppStarted() {
            try {
                Adb.call("shell", "pidof", Arguments.get().appName);
            } catch (Throwable e) {
                // app is crash
                Log.e("We detect your app is not running. Check your app is started and not crashed.");
            }
        }
    }
}