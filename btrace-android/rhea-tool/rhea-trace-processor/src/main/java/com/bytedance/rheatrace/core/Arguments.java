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
import com.bytedance.rheatrace.Log;
import com.bytedance.rheatrace.Main;
import com.bytedance.rheatrace.os.OS;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class Arguments {
    public static abstract class TcpPort {
        private String port;

        protected abstract int port();

        @Override
        public String toString() {
            if (port == null) {
                port = String.valueOf(port());
            }
            return port;
        }
    }

    public final TcpPort port;
    public final String appName;
    public final int timeInSeconds;
    public final boolean interactiveTracing;

    public final String mode;
    public final boolean restart;
    public final boolean waitStart;
    public final int maxAppTraceBufferSize;
    public final Long sampleIntervalNs;
    public final String outputPath;
    public final String mappingPath;
    public final String launcher;
    public String cmdLine;


    public final int waitTraceTimeout;

    public final String[] systraceArgs;

    private static Arguments instance;

    public static Arguments init(String[] args) {
        instance = new Arguments(args);
        return instance;
    }

    public static Arguments get() {
        if (instance == null) {
            throw new TraceError("Arguments#init is not call", null);
        }
        return instance;
    }

    private Arguments(String[] args) {
        cmdLine = String.join(" ", args);
        systraceArgs = Parser.resolveArgs(args);
        appName = Parser.appName;
        timeInSeconds = Parser.timeInSeconds != null? Parser.timeInSeconds : 0;
        interactiveTracing = Parser.interactiveTracing;
        sampleIntervalNs = Parser.sampleIntervalNs;
        mode = Parser.mode;
        restart = Parser.restart;
        waitStart = Parser.waitStart;
        maxAppTraceBufferSize = Parser.maxAppTraceBufferSize;
        mappingPath = Parser.mappingPath;
        outputPath = Parser.outputPath;
        port = Parser.port;
        waitTraceTimeout = Parser.waitTraceTimeout;
        launcher = Parser.launcher;
    }

    private static class Parser {
        private static String appName;
        private static Integer timeInSeconds;
        private static boolean interactiveTracing = false;
        private static Long sampleIntervalNs = 0L;
        private static String mode;
        private static boolean restart;
        private static boolean waitStart;
        private static int maxAppTraceBufferSize = 0;
        private static String outputPath;
        private static String mappingPath;
        private static String launcher = null;
        private static int waitTraceTimeout = 20;
        private static TcpPort port = new TcpPort() {
            @Override
            public int port() {
                return nextFreePort();
            }
        };

        private static int nextFreePort() {
            int port = -1;
            try {
                while (true) {
                    port = (int) (9000 + (System.currentTimeMillis() % 100));
                    Log.d("check port free: " + port);
                    // check port free on pc
                    boolean pcFree = OS.get().isPortFree(port);
                    if (pcFree) {
                        Log.d("port " + port + " is free");
                        return port;
                    }
                }
            } catch (Throwable e) {
                if (e instanceof TraceError) {
                    throw (TraceError) e;
                } else {
                    throw new TraceError(e.getMessage(), "auto port detection failed. please re-try like `-port 8088` to set port manually.");
                }
            }
        }

        @SuppressWarnings("SimpleDateFormat")
        private static String[] resolveArgs(String[] args) {
            int i = 0;
            while (i < args.length) {
                String arg = args[i++];
                switch (arg) {
                    case "-o":
                        ensureKeyWithValue(args, i);
                        outputPath = args[i++];
                        Workspace.init(new File(new File(outputPath).getParentFile(), "rheatrace.workspace"));
                        args[i - 1] = Workspace.systemTrace().getAbsolutePath();
                        break;
                    case "-a":
                        ensureKeyWithValue(args, i);
                        appName = args[i++];
                        break;
                    case "-t":
                        ensureKeyWithValue(args, i);
                        String time = args[i++];
                        try {
                            if (time.endsWith("s")) {
                                timeInSeconds = Integer.parseInt(time.substring(0, time.length() - 1));
                            } else {
                                timeInSeconds = Integer.parseInt(time);
                                args[i - 1] = (timeInSeconds + 1) + "s";
                            }
                        } catch (NumberFormatException e) {
                            throw new TraceError("can not parse `-t " + time + "`. " + e.getMessage(), "please -t with valid number", e);
                        }
                        break;
                    case "-sampleInterval":
                        ensureKeyWithValue(args, i);
                        String interval = args[i++];
                        try {
                            sampleIntervalNs = Long.parseLong(interval);
                        } catch (NumberFormatException e) {
                            throw new TraceError("can not parse `-sampleInterval " + interval + "`. " + e.getMessage(), "please input a valid interval value (ns)", e);
                        }
                        break;
                    case "-m":
                        ensureKeyWithValue(args, i);
                        mappingPath = args[i++];
                        break;
                    case "-mode":
                        ensureKeyWithValue(args, i);
                        mode = args[i++];
                        break;
                    case "-r":
                        restart = true;
                        break;
                    case "-w":
                        waitStart = true;
                        break;
                    case "-maxAppTraceBufferSize":
                        ensureKeyWithValue(args, i);
                        maxAppTraceBufferSize = Integer.parseInt(args[i++]);
                        break;
                    case "-port":
                        ensureKeyWithValue(args, i);
                        int value = Integer.parseInt(args[i++]);
                        port = new TcpPort() {
                            @Override
                            public int port() {
                                return value;
                            }
                        };
                        break;
                    case "-waitTraceTimeout":
                        waitTraceTimeout = Integer.parseInt(args[i++]);
                        break;
                    case "-launcher":
                        launcher = args[i++];
                        break;
                    default:
                        break;
                }
            }
            List<String> result = new ArrayList<>();
            i = 0;
            List<String> oneValueArgNames = Arrays.asList("-m", "-mode", "-maxAppTraceBufferSize", "-port", "-sampleInterval", "-waitTraceTimeout", "-launcher");
            List<String> noneValueArgNames = Arrays.asList("-debug", "-r", "-w");
            while (i < args.length) {
                String cur = args[i++].trim();
                if (noneValueArgNames.contains(cur)) {
                    continue;
                }
                if (oneValueArgNames.contains(cur)) {
                    i++;
                    continue;
                }
                if (cur.startsWith("rhea.") && !cur.equals(appName)) {
                    continue;
                }
                result.add(cur);
            }
            if (appName == null) {
                throw new TraceError("missing -a $appName", Main.usage());
            }
            if (outputPath == null) {
                outputPath = getDefaultOutputPath(appName);
                Log.i("no `-o output` specified. using the generated path:" + outputPath);
                Workspace.init(new File(new File(outputPath).getParentFile(), "rheatrace.workspace"));
                result.add("-o");
                result.add(Workspace.systemTrace().getAbsolutePath());
            }
            if (timeInSeconds == null) {
                if (!OS.get().supportInterruptKey()) {
                    throw new TraceError("Current OS does not support interactive tracing mode.", "please use `-t time` to specify tracing time.");
                }
                interactiveTracing = true;
                Log.blue("no `-t time` specified. using interactive tracing mode");
            } else if (timeInSeconds <= 0) {
                throw new TraceError("-t must >= 0", null);
            }
            if (mappingPath != null && !new File(mappingPath).exists()) {
                throw new TraceError("mapping file not exist:" + mappingPath, "check your mapping file path");
            }
            putSchedIfNoCategory(result);
            return result.toArray(new String[0]);
        }

        @SuppressWarnings("SimpleDateFormat")
        private static String getDefaultOutputPath(String appName) {
            String dateTime = new SimpleDateFormat("yyyy_MM_dd_HH_mm_ss").format(System.currentTimeMillis());
            String delimiter = "_";
            try {
                String content = Adb.callString("shell", "dumpsys", "package", appName);
                for (String line : content.split("\n")) {
                    if (line.contains("versionName")) {
                        String version = line.replace("versionName=", "").trim();
                        return String.join(delimiter, appName, version, dateTime) + ".pb";
                    }
                }
            } catch (Throwable ignore) {
            }
            return String.join(delimiter, appName, dateTime) + ".pb";
        }

        private static void ensureKeyWithValue(String[] args, int next) {
            if (next >= args.length) {
                throw new TraceError("value not present for key:" + args[next - 1], "check your command");
            }
        }

        private static void putSchedIfNoCategory(List<String> result) {
            int keyCount = 0;
            int valueCount = 0;
            for (String s : result) {
                if (s.startsWith("-")) {
                    keyCount++;
                } else {
                    valueCount++;
                }
            }
            if (keyCount == valueCount) {
                result.add("sched");
            }
        }
    }
}