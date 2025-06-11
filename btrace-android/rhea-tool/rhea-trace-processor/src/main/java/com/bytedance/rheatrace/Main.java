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

import com.bytedance.rheatrace.core.AdbProp;
import com.bytedance.rheatrace.core.Arguments;
import com.bytedance.rheatrace.core.Debug;
import com.bytedance.rheatrace.core.SystemLevelCapture;
import com.bytedance.rheatrace.core.TraceError;
import com.bytedance.rheatrace.core.Version;
import com.bytedance.rheatrace.core.Workspace;
import com.bytedance.rheatrace.lite.LiteCapture;
import com.bytedance.rheatrace.perfetto.PerfettoCapture;

import org.json.JSONObject;

import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.List;
import java.util.Scanner;
import java.util.StringTokenizer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;

public class Main {

    private static Arguments arg;
    public static void main(String[] args) throws Exception {
        if (args.length == 0 || Arrays.asList(args).contains("-v")) {
            System.out.println("Version: " + Version.NAME);
            System.out.println("  Usage: " + usage());
            return;
        }
        System.out.println("rhea-trace-processor-" + Version.NAME + " started");
        boolean adbForwarded = false;
        SystemLevelCapture capture = null;
        try {
            Debug.init(args);
            Adb.init(args);
            arg = Arguments.init(args);
            AdbProp.setup();
            capture = getSystemLevelCapture();
            if (capture instanceof PerfettoCapture && handlePerfettoQuickOuput(capture, arg.systraceArgs)) {
                return;
            }
            CountDownLatch latch = new CountDownLatch(1);
            final SystemLevelCapture sysCapture = capture;
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
                        Log.e(e.toString());
//                        throw new TraceError(e.getMessage(), null, e);
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
            } else if (!arg.waitStart){
                prepareAdbForward(arg);
                adbForwarded = true;
                Adb.Http.get("?action=start");
            }
            if (arg.interactiveTracing) {
                Log.blue("press enter to stop tracing...");
                Scanner scanner = new Scanner(System.in);
                scanner.nextLine();
            } else {
                LockSupport.parkNanos(arg.timeInSeconds * 1000_000_000L);
            }
            Log.blue("stop tracing...");
            if (!adbForwarded) {
                prepareAdbForward(arg);
                adbForwarded = true;
            }
            Adb.Http.get("?action=stop");
            sysCapture.stop();
            thread.join();
            Adb.Http.download("sampling", Workspace.samplingTrace());
            Adb.Http.download("sampling-mapping", Workspace.samplingMapping());
            showBufferUsage();
            sysCapture.process();
        } catch (Throwable e) {
            if (e instanceof TraceError) {
                Log.e("Error: " + e.getMessage());
                String prompt = ((TraceError) e).getPrompt();
                if (prompt != null) {
                    Log.e(" Tips: " + prompt);
                }
                e.printStackTrace();
            } else {
                StringWriter out = new StringWriter();
                PrintWriter writer = new PrintWriter(out);
                e.printStackTrace(writer);
                Log.e(out.toString());
            }
        } finally {
            if (capture != null) {
                capture.cleanup();
            }
            if (Adb.isConnected() && adbForwarded) {
                Adb.Http.getSafe("?action=clean");
            }
            if (adbForwarded) {
                removeForwardPort();
            }
        }
    }

    private static void prepareAdbForward(Arguments arg) throws IOException, InterruptedException {
        int appServerPort;
        try {
            appServerPort = getAppServerPort(arg);
        } catch (Exception e) {
            throw new TraceError("prepare adb forward failed", "get app server port failed", e);
        }
        if (appServerPort <= 0) {
            throw new TraceError("cannot establish connection to app trace server", "server port not found");
        }
        Adb.call("forward", "tcp:" + arg.port, "tcp:" + appServerPort);
    }

    private static int getAppServerPort(Arguments arg) throws IOException, InterruptedException {
        String dirPath = "/storage/emulated/0/Android/data/" + arg.appName + "/files/rhea-port";
        String results = Adb.callString("shell", "ls", dirPath);
        String[] lines = results.split("\n");
        for (String line : lines) {
            try {
                int possiblePort = Integer.parseInt(line);
                if (possiblePort > 0) {
                    return possiblePort;
                }
            } catch (Exception e) {
                Log.red(e.getMessage());
            }
        }
        return -1;
    }

    private static void removeForwardPort() {
        String port = arg.port.toString();
        try {
            Adb.call("forward", "--remove", "tcp:" + port);
        } catch (Exception e) {
            Log.d("close port:" + port + " failed. Error: " + e.getMessage());
            if (Debug.isDebug()) {
                e.printStackTrace();
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

    private static void showBufferUsage() {
        try {
            String json = Adb.Http.get("?action=query&name=debug");
            JSONObject debug = new JSONObject(json);
            JSONObject sampleBuffer = debug.optJSONObject("sampling");
            if (sampleBuffer != null) {
                long start = sampleBuffer.optLong("start");
                long end = sampleBuffer.optLong("end");
                long capacity = sampleBuffer.optLong("capacity");
                long currentSize = end - start;
                if (currentSize <= capacity) {
                    Log.blue("MaxAppTraceBufferSize usage " + currentSize + "/" + capacity + " (" + (currentSize * 100 / capacity) + "%)");
                } else {
                    Log.red("MaxAppTraceBufferSize is too small. Expected " + currentSize + " Actual " + capacity + ". Add `-maxAppTraceBufferSize " + currentSize + "` to your command");
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
        try {
            String launcher = getActivityLauncherPrecise();
            if (launcher != null) {
                return launcher;
            }
        } catch (Exception e) {
            // ignore
        }
        return getActivityLauncherBackup();
    }

    private static String getActivityLauncherPrecise() throws IOException, InterruptedException {
        String dump = Adb.callString("shell", "cmd", "package", "resolve-activity", "-a", "android.intent.action.MAIN", "-c", "android.intent.category.LAUNCHER", arg.appName);
        StringTokenizer tokenizer = new StringTokenizer(dump, "\n");
        while (tokenizer.hasMoreTokens()) {
            String next = tokenizer.nextToken();
            if (next.trim().equals("ActivityInfo:")) {
                while (tokenizer.hasMoreTokens()) {
                    String line = tokenizer.nextToken().trim();
                    // ActivityInfo:
                    //   name=com.ss.android.ugc.aweme.splash.SplashActivity
                    //   packageName=com.ss.android.ugc.aweme
                    if (line.startsWith("name=")) {
                        String activity = line.substring(5);
                        return arg.appName + "/" + activity;
                    }
                }
            }
        }
        return null;
    }

    private static String getActivityLauncherBackup() throws IOException, InterruptedException {
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
        // try with old version format
        // Activity Resolver Table:
        //  Non-Data Actions:
        //      android.intent.action.MAIN:
        //        e1df9b2 rhea.sample.android/.app.MainActivity
        tokenizer = new StringTokenizer(dump, "\n");
        while (tokenizer.hasMoreTokens()) {
            String next = tokenizer.nextToken().trim();
            if (next.equals("android.intent.action.MAIN:")) {
                String action = tokenizer.nextToken().trim();
                return action.substring(action.indexOf(" ")).trim();
            }
        }
        throw new TraceError("can not get launcher for your app " + arg.appName, "have you install your app? or you pass a wrong package name.");
    }

    public static String usage() {
        return "java -jar some.jar -a $packageName [-o $outputPath -t $timeInSecond $categories -debug -r -port $port]";
    }
}