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

import com.bytedance.rheatrace.processor.Log;
import com.bytedance.rheatrace.processor.trace.ATrace;
import com.bytedance.rheatrace.processor.trace.Frame;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.io.LineIterator;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.GZIPInputStream;

/**
 * Created by caodongping on 2023/2/15
 *
 * @author caodongping@bytedance.com
 */
public class AppTraceProcessor {
    public static boolean sMainThreadOnly;
    private static long sAppSystemTimeDiff;

    public static List<Frame> getBinaryTrace(long systemFtraceTime) throws IOException {
        List<Frame> result = processBinary(systemFtraceTime);
        if (result.isEmpty()) {
            throw new TraceError("apptrace is empty", "your app may not trace with any stub. please have a check.");
        }
        return result;
    }

    public static List<ATrace> getATrace() throws IOException {
        String pattern = "\\d+.\\d+ +\\d+: +[B|E]\\|\\d+.*";
        List<ATrace> list = new ArrayList<>();
        try (GZIPInputStream gzip = new GZIPInputStream(Files.newInputStream(Workspace.appAtrace().toPath()))) {
            LineIterator it = IOUtils.lineIterator(IOUtils.buffer(gzip), Charset.defaultCharset());
            try {
                while (it.hasNext()) {
                    String line = it.nextLine();
                    if (line.contains("|TRACE_")) {
                        continue;
                    }
                    if (!line.matches(pattern)) {
                        Log.d("bad atrace: " + line);
                        continue;
                    }
                    if (!sMainThreadOnly) {
                        // 27438.977315541 28095: B|28069|open
                        // 24903.044390707 25135: B|25135|B:RheaApplication:printApplicationName[rhea.sample.android.app.RheaApplication]
                        int first = line.indexOf(" ");
                        int second = line.indexOf(" ", first + 1);
                        String time = line.substring(0, first);
                        String tid = line.substring(first + 1, second - 1);
                        String buf = line.substring(second).trim() + "\n";
                        list.add(new ATrace(Integer.parseInt(tid), Long.parseLong(time.replace(".", "")) + sAppSystemTimeDiff, buf));
                    } else {
                        // 11207.478111764: B|14220|ResourcesImpl#updateConfiguration
                        int first = line.indexOf(":");
                        int firstLine = line.indexOf('|');
                        int secondLine = line.indexOf('|', firstLine + 1);
                        String tid = secondLine != -1 ? line.substring(firstLine + 1, secondLine) : line.substring(firstLine + 1);
                        String time = line.substring(0, first);
                        String buf = line.substring(first + 1).trim() + "\n";
                        list.add(new ATrace(Integer.parseInt(tid), Long.parseLong(time.replace(".", "")) + sAppSystemTimeDiff, buf));
                    }
                }
            } catch (Throwable e) {
                Log.i("read atrace with exception. " + e.getMessage());
            } finally {
                Log.d("finally atrace size " + list.size());
            }
        }
        return processDraft(list);
    }

    private static List<ATrace> processDraft(List<ATrace> list) throws IOException {
        // #android.view.IWindowSession
        // TRANSACTION_addToDisplay:1
        // TRANSACTION_addToDisplayAsUser:2
        List<String> binders = FileUtils.readLines(Workspace.binderInfo(), Charset.defaultCharset());
        // interfaceToken -> transactCode -> transactName
        Map<String, Map<String, String>> binderMap = new HashMap<>();
        Map<String, String> current = null;
        for (String line : binders) {
            if (line.charAt(0) == '#') {
                current = new HashMap<>();
                binderMap.put(line.substring(1), current);
            } else if (current != null) {
                int colon = line.indexOf(':');
                String code = line.substring(colon + 1);
                String name = line.substring(0, colon);
                current.put(code, name);
            } else {
                throw new TraceError("error parse binder.txt", "re-try with remove `rhea.binder` category in your command.");
            }
        }
        for (ATrace aTrace : list) {
            // <rhea-draft>binder transact[android.content.pm.IPackageManager:9]
            try {
                if (aTrace.buf.contains("<rhea-draft>")) {
                    int bracketBegin = aTrace.buf.indexOf('[');
                    int colon = aTrace.buf.indexOf(':');
                    int bracketEnd = aTrace.buf.indexOf(']');
                    int angle = aTrace.buf.indexOf('<');
                    String interfaceToken = aTrace.buf.substring(bracketBegin + 1, colon);
                    String code = aTrace.buf.substring(colon + 1, bracketEnd);
                    String name = binderMap.getOrDefault(interfaceToken, Collections.emptyMap()).getOrDefault(code, "unknown");
                    String prefix = aTrace.buf.substring(0, angle);
                    aTrace.buf = prefix + "binder transact[" + interfaceToken + ":" + name + "|" + code + "]\n";
                }
            } catch (Throwable e) {
                Log.e("bad " + aTrace.buf.trim());
            }
        }
        return list;
    }

    @SuppressWarnings("SdCardPath")
    private static List<Frame> processBinary(long systemFtraceTime) throws IOException {
        Map<Integer, String> mapping = Mapping.get();
        List<Frame> result = new ArrayList<>();
        File bin = Workspace.appBinaryTrace();
        byte[] bytes = FileUtils.readFileToByteArray(bin);
        ByteBuffer buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        long version = buffer.getInt();// version
        if (version < 5) {
            throw new TraceError("RheaTrace integrated of your app is no longer supported.", "Please upgrade to the latest version and try again.");
        }
        int pid = buffer.getInt();
        Log.d("Binary Trace format version code " + version + " pid " + pid);
        long bootTime = buffer.getLong();
        long monotonicTime = buffer.getLong();
        long traceBeginMonotonicTime = buffer.getLong();
        Log.d("SystemTime is " + systemFtraceTime);
        Log.d("BootTime   is " + bootTime);
        Log.d("Monotonic  is " + monotonicTime);
        long diff = resolveAppSystemTimeDiff(systemFtraceTime, bootTime, monotonicTime);
        while (buffer.hasRemaining()) {
            // startTime	45 // 9H
            // duration	    45 // 9H
            // threadId	    15
            // methodId	    23 // 800w
            // auto *buffer64 = (uint64_t *) (buffer_ + next);
            // auto startTime = beginTime - firstNanoTime_;
            // auto tid = gettid();
            // 45-startTime + 19-high-bits of duration
            // buffer64[0] = (startTime << 19LU) | (dur >> 26LU);
            // 25-low-bits of duration | tid | mid
            // buffer64[1] = (dur & 0x3FFFFFFLU) | (tid << 23LU) | mid;
            long a = buffer.getLong();
            if (!buffer.hasRemaining()) {
                continue;
            }
            long b = buffer.getLong();
            if (b == 0) {
                continue;
            }
            long startTime = a >>> 19;
            long dur0 = a & 0x7FFFF;
            long dur1 = (b >>> 38) & 0x3FFFFFF;
            long dur = (dur0 << 26) + dur1;
            int tid = (int) ((b >>> 23) & 0x7FFF);
            int mid = (int) (b & 0x7FFFFF);

            startTime += monotonicTime;
            result.add(new Frame(Frame.B, startTime, dur, pid, tid, mid, mapping));
            result.add(new Frame(Frame.E, startTime, dur, pid, tid, mid, mapping));
        }
        if (diff != 0) {
            for (Frame frame : result) {
                frame.time += diff;
            }
        }
        sAppSystemTimeDiff = diff;
        Log.d("App process id " + result.get(0).pid);
        result.sort(Comparator.comparingLong(frame -> frame.time));
        checkNameResolveResult(mapping, result);
        for (Frame frame : result) {
            if (frame.time < traceBeginMonotonicTime) {
                frame.time = traceBeginMonotonicTime;
            }
        }
        return result;
    }

    /**
     * When the @CriticalNative annotation is removed during compilation, it is possible that both
     * the timestamp and the method id are parsed incorrectly, resulting in a large number of
     * methods that cannot be parsed to the method name. We check whether the @CriticalNative
     * annotation is removed by the method name parsing success rate
     */
    private static void checkNameResolveResult(Map<Integer, String> mapping, List<Frame> result) {
        int i = 0;
        for (Frame frame : result) {
            if (mapping.containsKey(frame.methodId)) {
                i++;
            }
        }
        if (i < result.size() * 0.5f) {
            throw new TraceError("Only " + i + " of the " + result.size() + " methods are resolved the method name.", "Please check whether the @CriticalNative annotation is filtered when packaging.");
        }
    }

    private static long resolveAppSystemTimeDiff(long systemFtraceTime, long bootTime, long monotonicTime) {
        if (systemFtraceTime == 0) {
            return 0;
        }
        if (Math.abs(bootTime - monotonicTime) < 1000_000) {
            Log.d("BootTime and monotonic time is same.");
            return 0;
        }
        if (Math.abs(systemFtraceTime - monotonicTime) < Math.abs(systemFtraceTime - bootTime)) {
            Log.d("System is monotonic time.");
            Log.d("System and app diff is " + (systemFtraceTime - monotonicTime) / 1000_000_000.0);
            return 0;
        }
        long diff = bootTime - monotonicTime;
        Log.d("System is BootTime. time diff is " + diff);
        Log.d("System and app diff is " + (systemFtraceTime - bootTime) / 1000_000_000.0);
        return diff;
    }
}
