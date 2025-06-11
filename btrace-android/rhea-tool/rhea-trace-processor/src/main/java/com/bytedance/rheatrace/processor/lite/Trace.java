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

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import perfetto.protos.ProcessTreeOuterClass;
import perfetto.protos.ThreadDescriptorOuterClass;
import perfetto.protos.TraceOuterClass;
import perfetto.protos.TracePacketOuterClass;
import perfetto.protos.TrackDescriptorOuterClass;
import perfetto.protos.TrackEventOuterClass;

public class Trace {
    private final TraceOuterClass.Trace.Builder perfettoTrace = TraceOuterClass.Trace.newBuilder();
    private final Map<Process, List<Thread>> processMap = new HashMap<>();
    private final Map<String, Integer> trackMap = new TreeMap<>(); // key is pid_tid
    private boolean trackAssigned;

    private void addSlice(int pid, int tid, String name, long ts, TrackEventOuterClass.TrackEvent.Type type) {
        if (!trackAssigned) {
            injectTrackDescriptorPacket();
        }
        int trackUuid = 0;
        if (pid > 0 && tid > 0) {
            trackUuid = trackMap.get(getTrackKey(pid, tid));
        }
        TrackEventOuterClass.TrackEvent.Builder event = TrackEventOuterClass.TrackEvent.newBuilder()
                .setTrackUuid(trackUuid)
                .setType(type);
        if (type == TrackEventOuterClass.TrackEvent.Type.TYPE_SLICE_BEGIN) {
            event.setName(name);
        }
        TracePacketOuterClass.TracePacket.Builder p = TracePacketOuterClass.TracePacket.newBuilder().setTimestamp(ts).setTrackEvent(event).setTrustedPacketSequenceId(0);
        perfettoTrace.addPacket(p);
    }

    public void addSliceBegin(int pid, int tid, String name, long ts) {
        addSlice(pid, tid, name, ts, TrackEventOuterClass.TrackEvent.Type.TYPE_SLICE_BEGIN);
    }

    public void addSliceEnd(int pid, int tid, long ts) {
        addSlice(pid, tid, null, ts, TrackEventOuterClass.TrackEvent.Type.TYPE_SLICE_END);
    }

    public void marshal(OutputStream out) throws IOException {
        injectProcessTreePacket();
        perfettoTrace.build().writeTo(out);
    }

    void injectTrackDescriptorPacket() {
        int trackUuid = 0;
        for (Map.Entry<Process, List<Thread>> entry : processMap.entrySet()) {
            Process process = entry.getKey();
            TrackDescriptorOuterClass.TrackDescriptor.Builder td = TrackDescriptorOuterClass.TrackDescriptor.newBuilder()
                    .setUuid(trackUuid)
                    .setName(process.name);
            TracePacketOuterClass.TracePacket.Builder p = TracePacketOuterClass.TracePacket.newBuilder()
                    .setTrackDescriptor(td);
            perfettoTrace.addPacket(p);
            int parentUuid = trackUuid;
            trackUuid++;
            for (Thread thread : entry.getValue()) {
                td = TrackDescriptorOuterClass.TrackDescriptor.newBuilder()
                        .setUuid(trackUuid)
                        .setParentUuid(parentUuid)
                        .setName(thread.name)
                        .setThread(ThreadDescriptorOuterClass.ThreadDescriptor.newBuilder()
                                .setPid(process.pid)
                                .setTid(thread.tid)
                                .setThreadName(thread.name)
                                .build());
                trackMap.put(getTrackKey(process.pid, thread.tid), trackUuid);
                p = TracePacketOuterClass.TracePacket.newBuilder().setTrackDescriptor(td);
                perfettoTrace.addPacket(p);
                trackUuid++;
            }
        }
        trackAssigned = true;
    }

    public void setProcess(int pid, String name) {
        if (trackAssigned) {
            throw new RuntimeException("can not setProcess after trackAssigned");
        }
        Process process = new Process(pid, name);
        processMap.computeIfAbsent(process, p -> new ArrayList<>());
    }

    public void setThread(int pid, int tid, String name) {
        if (trackAssigned) {
            throw new RuntimeException("can not setThread after trackAssigned");
        }
        for (Map.Entry<Process, List<Thread>> entry : processMap.entrySet()) {
            if (entry.getKey().pid == pid) {
                Thread thread = new Thread(tid, name);
                entry.getValue().add(thread);
                break;
            }
        }
    }

    void injectProcessTreePacket() {
        ProcessTreeOuterClass.ProcessTree.Builder processTree = ProcessTreeOuterClass.ProcessTree.newBuilder();
        for (Map.Entry<Process, List<Thread>> entry : processMap.entrySet()) {
            Process process = entry.getKey();
            ProcessTreeOuterClass.ProcessTree.Process.Builder processPB = ProcessTreeOuterClass.ProcessTree.Process.newBuilder().setPid(process.pid).addCmdline(process.name);
            processTree.addProcesses(processPB);
            List<Thread> threads = entry.getValue();
            for (Thread thread : threads) {
                ProcessTreeOuterClass.ProcessTree.Thread.Builder threadPB = ProcessTreeOuterClass.ProcessTree.Thread.newBuilder().setTid(thread.tid).setTgid(process.pid).setName(thread.name);
                processTree.addThreads(threadPB);
            }
        }
        TracePacketOuterClass.TracePacket.Builder processTreePacket = TracePacketOuterClass.TracePacket.newBuilder().setProcessTree(processTree);
        perfettoTrace.addPacket(processTreePacket);
    }

    String getTrackKey(int pid, int tid) {
        return pid + "_" + tid;
    }

    private static class Process {
        int pid;
        String name;

        public Process(int pid, String name) {
            this.pid = pid;
            this.name = name;
        }
    }

    private static class Thread {
        int tid;
        String name;

        public Thread(int tid, String name) {
            this.tid = tid;
            this.name = name;
        }
    }
}
