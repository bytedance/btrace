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
import com.bytedance.rheatrace.processor.Log;
import com.bytedance.rheatrace.processor.Main;
import com.bytedance.rheatrace.processor.os.OS;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
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
    public final List<String> categories;
    public final String appName;
    public final int timeInSeconds;
    public final String mode;
    public final boolean restart;
    public final int maxAppTraceBufferSize;
    public final Long durationThresholdNs;
    public final boolean mainThreadOnly;
    public final boolean fullClassName;
    public final String outputPath;
    public final String mappingPath;
    public String cmdLine;

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
        timeInSeconds = Parser.timeInSeconds;
        durationThresholdNs = Parser.durationThresholdNs;
        mode = Parser.mode;
        restart = Parser.restart;
        maxAppTraceBufferSize = Parser.maxAppTraceBufferSize;
        mainThreadOnly = Parser.mainThreadOnly;
        fullClassName = Parser.fullClassName;
        mappingPath = Parser.mappingPath;
        outputPath = Parser.outputPath;
        categories = Collections.unmodifiableList(Parser.categories);
        port = Parser.port;
    }

    private static class Parser {
        private static String appName;
        private static Integer timeInSeconds;
        private static Long durationThresholdNs;
        private static String mode;
        private static boolean restart;
        private static int maxAppTraceBufferSize = 500_000_000;
        private static boolean mainThreadOnly;
        private static boolean fullClassName;
        private static String outputPath;
        private static String mappingPath;

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
                    port = (int) (8000 + (System.currentTimeMillis() % 100));
                    Log.d("check port free: " + port);
                    // check port free on pc
                    boolean pcFree = OS.get().isPortFree(port);
                    // check port free on phone
                    String netstat = Adb.callString("shell", "netstat", "-tunlp");
                    boolean phoneFree = !netstat.contains(":" + port);
                    if (pcFree && phoneFree) {
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

        private static final List<String> categories = new ArrayList<>();

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
                    case "-threshold":
                        ensureKeyWithValue(args, i);
                        String durationThreshold = args[i++];
                        try {
                            durationThresholdNs = Long.parseLong(durationThreshold);
                        } catch (NumberFormatException e) {
                            throw new TraceError("can not parse `-threshold " + durationThreshold + "`. " + e.getMessage(), "please input a valid number", e);
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
                    case "-maxAppTraceBufferSize":
                        ensureKeyWithValue(args, i);
                        maxAppTraceBufferSize = Integer.parseInt(args[i++]);
                        break;
                    case "-mainThreadOnly":
                        mainThreadOnly = true;
                        AppTraceProcessor.sMainThreadOnly = true;
                        break;
                    case "-fullClassName":
                        fullClassName = true;
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
                    default:
                        if (arg.startsWith("rhea.")) {
                            int dot = arg.indexOf('.');
                            String name = arg.substring(dot + 1);
                            categories.add("debug.rhea.category." + name);
                        }
                        break;
                }
            }
            List<String> result = new ArrayList<>();
            i = 0;
            List<String> oneValueArgNames = Arrays.asList("-m", "-mode", "-maxAppTraceBufferSize", "-port", "-threshold");
            List<String> noneValueArgNames = Arrays.asList("-mainThreadOnly", "-fullClassName", "-debug", "-r");
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
                outputPath = new SimpleDateFormat("yyyy_MM_dd_HH_mm_ss_SSS").format(System.currentTimeMillis()) + ".pb";
                Log.i("no `-o output` specified. using the generated path:" + outputPath);
                Workspace.init(new File(new File(outputPath).getParentFile(), "rheatrace.workspace"));
                result.add("-o");
                result.add(Workspace.systemTrace().getAbsolutePath());
            }
            if (timeInSeconds == null) {
                timeInSeconds = 5;
                result.add("-t");
                result.add(timeInSeconds + "s");
                Log.i("no `-t time` specified. using the default time: " + timeInSeconds);
            }
            if (timeInSeconds <= 0) {
                throw new TraceError("-t must >= 0", null);
            }
            if (mappingPath != null && !new File(mappingPath).exists()) {
                throw new TraceError("mapping file not exist:" + mappingPath, "check your mapping file path");
            }
            putSchedIfNoCategory(result);
            return result.toArray(new String[0]);
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
