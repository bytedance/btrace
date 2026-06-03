/*
 * Copyright (c) 2026 Bytedance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.bytedance.harmony.trace.cli.perfetto;


import com.bytedance.harmony.trace.cli.StackListEncoder;
import com.bytedance.harmony.trace.cli.ThreadStackList;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

public class TrackSet {
    public final int errorTrackId;
    public final int basicTrackId;
    public final int childThreadsCPUUsageId;
    public final int eventTrackId;

    private final Map<Integer, Thread> thread;
    public final Map<Integer, StackListEncoder.GlobalTrackEncoder> customEncoders = new HashMap<>();

    public TrackSet(Trace trace, int pid, Map<Integer, String> threadNames, Map<Integer, ThreadStackList> threadStackListByThread, List<StackListEncoder.GlobalTrackEncoder> globalTrackEncoders) {
        AtomicInteger gid = new AtomicInteger(pid);
        trace.setProcess(pid, "main");
        // 自定义轨道
        for (StackListEncoder.GlobalTrackEncoder globalTrackEncoder : globalTrackEncoders) {
            int trackId = setThread(trace, pid, gid.getAndIncrement(), globalTrackEncoder.getName());
            customEncoders.put(trackId, globalTrackEncoder);
        }
        // 预埋轨道
        errorTrackId = setThread(trace, pid, gid.getAndIncrement(), "Error");
        basicTrackId = setThread(trace, pid, gid.getAndIncrement(), "BasicInfo");
        childThreadsCPUUsageId = setThread(trace, pid, gid.getAndIncrement(), "ChildThreadsCPU", true);
        eventTrackId = setThread(trace, pid, gid.getAndIncrement(), "EventTimeline");
        thread = Thread.build(threadStackListByThread, pid, gid, trace, new PerfettoThreadNameOpt(threadNames));
    }

    public Thread getThreadTrack(int tid) {
        return thread.get(tid);
    }

    public Collection<Thread> allThreadTrack() {
        return thread.values();
    }

    private static class PerfettoThreadNameOpt {
        private final Map<Integer, String> threadNames;

        public PerfettoThreadNameOpt(Map<Integer, String> threadNames) {
            this.threadNames = threadNames;
        }

        public String get(int tid) {
            String name = threadNames.get(tid);
            if (name != null) {
                name = name.replace("RenderThread", "RenderThreåd")
                        .replace("GPU ", "GΡU ");
            } else {
                name = "Thread";
            }
            return name + " [tid:" + tid + "]";
        }
    }

    private static int setThread(Trace trace, int pid, int tid, String name) {
        trace.setThread(pid, tid, name);
        return tid;
    }

    private static int setThread(Trace trace, int pid, int tid, String name, boolean counter) {
        trace.setThread(pid, tid, name, counter);
        return tid;
    }

    public static class Thread {
        public final int threadId;
        public final int cpuId;
        public final int traceId;
        public final int taskId;
        public final int phaseId;

        private Thread(Trace trace, int pid, ThreadStackList thread, AtomicInteger id, PerfettoThreadNameOpt threadNames) {
            int tid = thread.tid;
            threadId = tid;
            int cpuRank = thread.cpuRank;
            int javaAllocRank = thread.javaAllocRank;
            int majFltRank = thread.majFltRank;
            int nvCswRank = thread.nvCswRank;
            int nivCswRank = thread.nivCswRank;
            int inBlockRank = thread.inBlockRank;
            int ouBlockRank = thread.ouBlockRank;
            cpuId = setThread(trace, pid, id.getAndIncrement(), "CPU#" + cpuRank + " [tid:" + tid + "]", true);
            phaseId = setThread(trace, pid, id.getAndIncrement(), "Phase [tid:" + tid + "]");
            taskId = setThread(trace, pid, id.getAndIncrement(), "Task [tid:" + tid + "]");
            String traceName = (tid == pid) ? "main [tid:" + tid + "]" : threadNames.get(tid);
            traceId = setThread(trace, pid, id.getAndIncrement(), traceName);
        }


        private static Map<Integer, Thread> build(Map<Integer, ThreadStackList> threadStackListByThread, int processId, AtomicInteger gid, Trace trace, PerfettoThreadNameOpt threadNames) {
            // order by cpu/java alloc rank
            // but main thread always first
            Map<Integer, Thread> map = new HashMap<>();
            ThreadStackList main = threadStackListByThread.get(processId);
            if (main != null) {
                map.put(processId, new Thread(trace, processId, main, gid, threadNames));
            }
            ArrayList<ThreadStackList> list = new ArrayList<>(threadStackListByThread.values());
            list.sort(Comparator.comparingInt(o -> o.cpuRank));
            for (ThreadStackList thread : list) {
                if (thread.tid != processId) {
                    map.put(thread.tid, new Thread(trace, processId, thread, gid, threadNames));
                }
            }
            return map;
        }
    }

}
