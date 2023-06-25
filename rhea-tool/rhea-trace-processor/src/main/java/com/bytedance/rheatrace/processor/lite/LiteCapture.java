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
package com.bytedance.rheatrace.processor.lite;

import com.bytedance.rheatrace.processor.Adb;
import com.bytedance.rheatrace.processor.Log;
import com.bytedance.rheatrace.processor.core.AppTraceProcessor;
import com.bytedance.rheatrace.processor.core.Arguments;
import com.bytedance.rheatrace.processor.core.SystemLevelCapture;
import com.bytedance.rheatrace.processor.core.Workspace;
import com.bytedance.rheatrace.processor.trace.ATrace;
import com.bytedance.rheatrace.processor.trace.Frame;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.concurrent.locks.LockSupport;

/**
 * Created by caodongping on 2023/2/15
 *
 * @author caodongping@bytedance.com
 */
public class LiteCapture implements SystemLevelCapture {
    private Thread thread;

    @Override
    public void start(String[] sysArgs) throws IOException {
        int seconds = -1;
        for (int i = 0; i < sysArgs.length; i++) {
            if (sysArgs[i].equals("-t")) {
                seconds = Integer.parseInt(sysArgs[i + 1].substring(0, sysArgs[i + 1].length() - 1)) * 1000;
                break;
            }
        }
        int finalSeconds = seconds;
        thread = new Thread(() -> {
            LockSupport.parkNanos(finalSeconds * 1000_000L);
        });
        thread.start();
    }

    @Override
    public void print(boolean withError, PrintListener listener) throws IOException {
    }

    @Override
    public void waitForExit() throws InterruptedException {
        thread.join();
    }

    @Override
    public void process() throws IOException {
        Trace trace = new Trace();
        List<Frame> frames = AppTraceProcessor.getBinaryTrace(0);
        List<ATrace> aTraceList = AppTraceProcessor.getATrace();
        HashSet<Integer> tids = new HashSet<>();
        for (Frame frame : frames) {
            tids.add(frame.threadId);
        }
        for (ATrace aTrace : aTraceList) {
            tids.add(aTrace.tid);
        }
        int pid = frames.get(0).pid;
        trace.setProcess(pid, Arguments.get().appName);
        Map<String, String> map = getThreadsInfo();
        for (Integer tid : tids) {
            Object name = map.get(String.valueOf(tid));
            trace.setThread(pid, tid, name != null ? name + " " + tid : "<tid:" + tid + ">");
        }
        for (Frame frame : frames) {
            if (frame.isBegin()) {
                trace.addSliceBegin(frame.pid, frame.threadId, frame.method, frame.time);
            } else {
                trace.addSliceEnd(frame.pid, frame.threadId, frame.time);
            }
        }
        for (ATrace aTrace : aTraceList) {
            if (aTrace.isBegin()) {
                trace.addSliceBegin(pid, aTrace.tid, aTrace.simpleName(), aTrace.time);
            } else {
                trace.addSliceEnd(pid, aTrace.tid, aTrace.time);
            }
        }
        File output = Workspace.output();
        try (FileOutputStream out = new FileOutputStream(output)) {
            Log.red("writing trace:" + output);
            trace.marshal(out);
        }
    }

    private Map<String, String> getThreadsInfo() throws IOException {
        Map<String, String> map = new HashMap<>();
        try {
            String threads = Adb.Http.get("?name=threads");
            for (String kv : threads.split("\n")) {
                int comma = kv.indexOf(',');
                map.put(kv.substring(0, comma), kv.substring(comma + 1));
            }
        } catch (Throwable e) {
            return Collections.emptyMap();
        }
        return map;
    }
}
