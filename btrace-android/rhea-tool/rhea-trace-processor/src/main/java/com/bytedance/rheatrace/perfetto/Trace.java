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
package com.bytedance.rheatrace.perfetto;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.TreeMap;
import java.util.UUID;

import perfetto.protos.CounterDescriptorOuterClass;
import perfetto.protos.DebugAnnotationOuterClass;
import perfetto.protos.ProcessDescriptorOuterClass;
import perfetto.protos.ProcessTreeOuterClass;
import perfetto.protos.ThreadDescriptorOuterClass;
import perfetto.protos.TraceOuterClass;
import perfetto.protos.TracePacketOuterClass;
import perfetto.protos.TrackDescriptorOuterClass;
import perfetto.protos.TrackEventOuterClass;

public class Trace {

    private static long sTrackUuidGen = 0;
    private static final Map<String, Long> sTrackMap = new TreeMap<>(); // key is pid_tid

    private final TraceOuterClass.Trace.Builder perfettoTrace = TraceOuterClass.Trace.newBuilder();
    private final Map<Process, List<Thread>> processMap = new HashMap<>();

    private boolean trackAssigned;

    private void addSlice(int pid, int tid, String name, long ts, TrackEventOuterClass.TrackEvent.Type type, Map<String, Object> debug) {
        if (!trackAssigned) {
            injectTrackDescriptorPacket();
        }
        long trackUuid = -1L;
        if (pid > 0 && tid > 0) {
            Long uuid = sTrackMap.get(getTrackKey(pid, tid));
            if (uuid != null) {
                trackUuid = uuid;
            }
        }
        TrackEventOuterClass.TrackEvent.Builder event = TrackEventOuterClass.TrackEvent.newBuilder()
                .setType(type);
        if (trackUuid >= 0) {
            event.setTrackUuid(trackUuid);
        }
        if (type == TrackEventOuterClass.TrackEvent.Type.TYPE_SLICE_BEGIN) {
            event.setName(name);
        }
        if (debug != null) {
            for (Map.Entry<String, Object> entry : debug.entrySet()) {
                DebugAnnotationOuterClass.DebugAnnotation.Builder value = DebugAnnotationOuterClass.DebugAnnotation.newBuilder().setName(entry.getKey());
                event.addDebugAnnotations(setDebugValue(value, entry.getValue()));
            }
        }
        TracePacketOuterClass.TracePacket.Builder p = TracePacketOuterClass.TracePacket.newBuilder().setTimestamp(ts).setTrackEvent(event).setTrustedPacketSequenceId(0);
        perfettoTrace.addPacket(p);
    }

    private DebugAnnotationOuterClass.DebugAnnotation.Builder setDebugValue(DebugAnnotationOuterClass.DebugAnnotation.Builder value, Object v) {
        if (v instanceof Double) {
            value.setDoubleValue((Double) v);
        } else if (v instanceof Float) {
            value.setDoubleValue((Float) v);
        } else if (v instanceof Long) {
            value.setIntValue((Long) v);
        } else if (v instanceof Integer) {
            value.setIntValue((Integer) v);
        } else if (v instanceof Short) {
            value.setIntValue((Short) v);
        } else if (v instanceof Byte) {
            value.setIntValue((Byte) v);
        } else if (v instanceof Boolean) {
            value.setBoolValue((Boolean) v);
        } else {
            value.setStringValue(Objects.toString(v));
        }
        return value;
    }

    public void addSliceBegin(int pid, int tid, String name, long ts, Map<String, Object> debug) {
        addSlice(pid, tid, name, ts, TrackEventOuterClass.TrackEvent.Type.TYPE_SLICE_BEGIN, debug);
    }

    public void addSliceEnd(int pid, int tid, String name, long ts) {
        addSlice(pid, tid, name, ts, TrackEventOuterClass.TrackEvent.Type.TYPE_SLICE_END, null);
    }

    public void marshal(OutputStream out) throws IOException {
        injectProcessTreePacket();
        perfettoTrace.build().writeTo(out);
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

    private void injectTrackDescriptorPacket() {
        for (Map.Entry<Process, List<Thread>> entry : processMap.entrySet()) {
            Process process = entry.getKey();
            Long pUUid = getTrackUuid(process.pid, 0);
            if (pUUid == null) {
                pUUid = sTrackUuidGen++;
                TrackDescriptorOuterClass.TrackDescriptor.Builder pd = TrackDescriptorOuterClass.TrackDescriptor.newBuilder()
                        .setUuid(pUUid)
                        .setName(process.name)
                        .setProcess(ProcessDescriptorOuterClass.ProcessDescriptor.newBuilder()
                                .setPid(process.pid)
                                .setProcessName(process.name)
                                .build());
                TracePacketOuterClass.TracePacket.Builder p = TracePacketOuterClass.TracePacket.newBuilder()
                        .setTrackDescriptor(pd);
                perfettoTrace.addPacket(p);
                sTrackMap.put(getTrackKey(process.pid, 0), pUUid);
            }
            for (Thread thread : entry.getValue()) {
                String trackKey = getTrackKey(process.pid, thread.tid);
                Long tUUid = sTrackMap.get(trackKey);
                if (tUUid != null) {
                    continue;
                }
                tUUid = sTrackUuidGen++;
                TrackDescriptorOuterClass.TrackDescriptor.Builder td = TrackDescriptorOuterClass.TrackDescriptor.newBuilder()
                        .setUuid(tUUid)
                        .setParentUuid(pUUid)
                        .setName(thread.name)
                        .setThread(ThreadDescriptorOuterClass.ThreadDescriptor.newBuilder()
                                .setPid(process.pid)
                                .setTid(thread.tid)
                                .setThreadName(thread.name)
                                .build())
                        .setDisallowMergingWithSystemTracks(true);
                TracePacketOuterClass.TracePacket.Builder t = TracePacketOuterClass.TracePacket.newBuilder().setTrackDescriptor(td);
                perfettoTrace.addPacket(t);
                sTrackMap.put(trackKey, tUUid);
            }
        }
        trackAssigned = true;
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



    public void addCounter(long counterValue, long start_time, long trackUuid) {
        TrackEventOuterClass.TrackEvent.Builder event = TrackEventOuterClass.TrackEvent.newBuilder()
                .setTrackUuid(trackUuid)
                .setType(TrackEventOuterClass.TrackEvent.Type.TYPE_COUNTER)
                .setCounterValue(counterValue);

        TracePacketOuterClass.TracePacket.Builder p = TracePacketOuterClass.TracePacket.newBuilder().setTimestamp(start_time).setTrackEvent(event).setTrustedPacketSequenceId(1);
        perfettoTrace.addPacket(p);
    }

    private static long generateTrackUuid() {
        UUID uuid = UUID.randomUUID();
        return uuid.getMostSignificantBits() & Long.MAX_VALUE;
    }

    public long addCounterTrackDescriptorPacket(String eventName) {
        long trackUuid = generateTrackUuid();
        TrackDescriptorOuterClass.TrackDescriptor.Builder counterTrackDescriptor = TrackDescriptorOuterClass.TrackDescriptor.newBuilder()
                .setUuid(trackUuid)
                .setName(eventName)
                .setCounter(CounterDescriptorOuterClass.CounterDescriptor.newBuilder().build());

        TracePacketOuterClass.TracePacket.Builder counterTrackDescriptorPacket = TracePacketOuterClass.TracePacket.newBuilder()
                .setTrustedPacketSequenceId(1)
                .setTrackDescriptor(counterTrackDescriptor);
        perfettoTrace.addPacket(counterTrackDescriptorPacket);
        return trackUuid;
    }

    private static Long getTrackUuid(int pid, int tid) {
        return sTrackMap.get(getTrackKey(pid, tid));
    }

    private static String getTrackKey(int pid, int tid) {
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