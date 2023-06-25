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

import com.bytedance.rheatrace.processor.core.AdbProp;
import com.bytedance.rheatrace.processor.core.Arguments;
import com.bytedance.rheatrace.processor.core.Debug;
import com.bytedance.rheatrace.processor.core.SystemLevelCapture;
import com.bytedance.rheatrace.processor.core.TraceError;
import com.bytedance.rheatrace.processor.core.Version;
import com.bytedance.rheatrace.processor.core.Workspace;
import com.bytedance.rheatrace.processor.lite.LiteCapture;
import com.bytedance.rheatrace.processor.perfetto.PerfettoCapture;
import com.bytedance.rheatrace.processor.statistics.Statistics;

import org.json.JSONObject;

import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.List;
import java.util.StringTokenizer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;
import java.util.function.Function;

/**
 * Created by caodongping on 2023/2/8
 *
 * @author caodongping@bytedance.com
 */
public class Main {
    private static Arguments arg;

    public static void main(String[] args) throws Exception {
        if (args.length == 0 || Arrays.asList(args).contains("-v")) {
            System.out.println("Version: " + Version.NAME);
            System.out.println("  Usage: " + usage());
            return;
        }
        boolean success = false;
        String errorMessage = null;
        try {
            Debug.init(args);
            Adb.init(args);
            arg = Arguments.init(args);
            AdbProp.setup();
            SystemLevelCapture sysCapture = getSystemLevelCapture();
            if (sysCapture instanceof PerfettoCapture) {
                if (handlePerfettoQuickOuput(sysCapture, arg.systraceArgs)) {
                    return;
                }
            }
            CountDownLatch latch = new CountDownLatch(1);
            Thread thread = new Thread(() -> {
                if (sysCapture instanceof PerfettoCapture) {
                    try {
                        sysCapture.start(arg.systraceArgs);
                        sysCapture.print(true, msg -> {
                            if (msg.contains("enabled ftrace")) {
                                latch.countDown();
                            }
                        });
                        sysCapture.waitForExit();
                    } catch (Throwable e) {
                        throw new TraceError(e.getMessage(), null, e);
                    }
                } else {
                    latch.countDown();
                }
            });
            thread.start();
            if (!latch.await(2, TimeUnit.SECONDS)) {
                Log.d("perfetto starts slowly. ");
            }
            Log.blue("start tracing...");
            String appName = arg.appName;
            if (arg.restart) {
                Adb.call("shell", "am", "force-stop", appName);
                Adb.call("shell", "am", "start", "-n", getActivityLauncher(), "-a", "android.intent.action.MAIN", "-c", "android.intent.category.LAUNCHER");
            } else {
                Adb.call("shell", "am", "broadcast", "-a", "com.bytedance.rheatrace.switch.start", appName);
            }
            LockSupport.parkNanos(arg.timeInSeconds * 1000_000_000L);
            Log.blue("stop tracing...");
            Adb.call("shell", "am", "broadcast", "-a", "com.bytedance.rheatrace.switch.stop", appName);
            thread.join();
            Adb.call("forward", "tcp:" + arg.port, "tcp:" + arg.port);
            showBufferUsage();
            Adb.Http.download("trace", Workspace.appBinaryTrace());
            Adb.Http.download("rhea-atrace.gz", Workspace.appAtrace());
            Adb.Http.download("binder.txt", Workspace.binderInfo());
            sysCapture.process();
            success = true;
        } catch (Throwable e) {
            StringWriter out = new StringWriter();
            PrintWriter writer = new PrintWriter(out);
            e.printStackTrace(writer);
            errorMessage = out.toString();

            if (e instanceof TraceError) {
                Log.e("Error: " + e.getMessage());
                String prompt = ((TraceError) e).getPrompt();
                if (prompt != null) {
                    Log.e(" Tips: " + prompt);
                }
                if (Debug.isDebug()) {
                    e.printStackTrace();
                }
            } else {
                throw e;
            }
        } finally {
            if (Adb.isConnected()) {
                Statistics.safeSend(success, errorMessage);
            }
        }
    }

    private static boolean handlePerfettoQuickOuput(SystemLevelCapture sysCapture, String[] systraceArgs) throws IOException, InterruptedException {
        boolean contains = false;
        List<String> quick = Arrays.asList("-h", "--list", "--list-ftrace");
        for (String arg : systraceArgs) {
            if (quick.contains(arg)) {
                contains = true;
                break;
            }
        }
        if (contains) {
            sysCapture.start(systraceArgs);
            sysCapture.print(false, Log::i);
            sysCapture.waitForExit();
        }
        return contains;
    }

    public static void setReporter(Function<String, Integer> reporter) {
        Statistics.setReporter(reporter);
    }

    private static void showBufferUsage() {
        try {
            String json = Adb.Http.get("?name=debug");
            JSONObject debug = new JSONObject(json);
            JSONObject appTraceBuffer = debug.optJSONObject("appTraceBuffer");
            if (appTraceBuffer != null) {
                long currentSize = appTraceBuffer.optLong("currentSize");
                long maxSize = appTraceBuffer.optLong("maxSize");
                if (currentSize <= maxSize) {
                    Log.blue("MaxAppTraceBufferSize usage " + currentSize + "/" + maxSize + " (" + (currentSize * 100 / maxSize) + "%)");
                } else {
                    Log.red("MaxAppTraceBufferSize is too small. Expected " + currentSize + " Actual " + maxSize + ". Add `-maxAppTraceBufferSize " + currentSize + "` to your command");
                }
            }
        } catch (Throwable e) {
            if (Debug.isDebug()) {
                e.printStackTrace();
            }
        }
    }

    private static SystemLevelCapture getSystemLevelCapture() throws IOException, InterruptedException {
        SystemLevelCapture capture;
        String mode = arg.mode;
        if (mode == null) {
            String version = Adb.callString("shell", "getprop", "ro.build.version.sdk").trim();
            if (Integer.parseInt(version) >= 28) {
                capture = new PerfettoCapture();
            } else {
                capture = new LiteCapture();
            }
            Log.i("os version is " + version + ". default capture is " + capture.getClass().getSimpleName());
        } else {
            switch (mode) {
                case "perfetto":
                    capture = new PerfettoCapture();
                    break;
                case "simple":
                    capture = new LiteCapture();
                    break;
                default:
                    throw new TraceError("unknown mode: " + mode, "only `-mode perfetto` or `-mode simple` is supported.");
            }
        }
        Log.d("system level capture: " + capture.getClass().getSimpleName());
        return capture;
    }

    private static String getActivityLauncher() throws IOException, InterruptedException {
        String dump = Adb.callString("shell", "dumpsys", "package", arg.appName);
        String launcher = null;
        StringTokenizer tokenizer = new StringTokenizer(dump, "\n");
        boolean actionMatch = false;
        boolean categoryMatch = false;
        while (tokenizer.hasMoreTokens()) {
            String next = tokenizer.nextToken();
            if (next.trim().equals("android.intent.action.MAIN:")) {
                while (tokenizer.hasMoreTokens()) {
                    String line = tokenizer.nextToken().trim();
                    // 5f3565c rhea.sample.android/.app.MainActivity filter 413fd65
                    // Action: "android.intent.action.MAIN"
                    // Category: "android.intent.category.LAUNCHER"
                    if (line.startsWith("Action: ")) {
                        actionMatch = actionMatch || line.equals("Action: \"android.intent.action.MAIN\"");
                    } else if (line.startsWith("Category: ")) {
                        categoryMatch = categoryMatch || line.equals("Category: \"android.intent.category.LAUNCHER\"");
                    } else if (actionMatch && categoryMatch) {
                        return launcher;
                    } else {
                        int first = line.indexOf(" ");
                        int second = line.indexOf(" ", first + 1);
                        if (first > 0 && second > 0) {
                            launcher = line.substring(first, second);
                        }
                        actionMatch = false;
                        categoryMatch = false;
                    }
                }
            }
        }
        throw new TraceError("can not get launcher for your app " + arg.appName, "have you install your app? or you pass a wrong package name.");
    }

    public static String usage() {
        return "java -jar some.jar -a $packageName [-o $outputPath -t $timeInSecond $categories -debug -r -port $port]";
    }
}
