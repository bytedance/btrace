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

import java.io.IOException;
import java.io.OutputStream;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.TreeMap;

import perfetto.protos.CounterDescriptorOuterClass;
import perfetto.protos.DebugAnnotationOuterClass;
import perfetto.protos.ProcessTreeOuterClass;
import perfetto.protos.ThreadDescriptorOuterClass;
import perfetto.protos.TraceOuterClass;
import perfetto.protos.TracePacketOuterClass;
import perfetto.protos.TrackDescriptorOuterClass;
import perfetto.protos.TrackEventOuterClass;

public class Trace {
    private interface PacketHandler {
        void addPacket(TracePacketOuterClass.TracePacket.Builder packet);

        void writeTo(OutputStream out) throws IOException;
    }

    private final PacketHandler perfettoTrace;

    public Trace() {
        this.perfettoTrace = new PacketHandler() {
            private final TraceOuterClass.Trace.Builder perfettoTrace = TraceOuterClass.Trace.newBuilder();

            @Override
            public void addPacket(TracePacketOuterClass.TracePacket.Builder packet) {
                perfettoTrace.addPacket(packet);
            }

            @Override
            public void writeTo(OutputStream out) throws IOException {
                perfettoTrace.build().writeTo(out);
            }
        };
    }

    public Trace(OutputStream out) {
        this.perfettoTrace = new PacketHandler() {
            @Override
            public void addPacket(TracePacketOuterClass.TracePacket.Builder packet) {
                try {
                    TraceOuterClass.Trace.Builder trace = TraceOuterClass.Trace.newBuilder().addPacket(packet);
                    trace.build().writeTo(out);
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            }

            @Override
            public void writeTo(OutputStream out) throws IOException {
                out.close();
            }
        };
    }


    private final Map<Process, List<Thread>> processMap = new HashMap<>();
    private final Map<Integer, Map<Integer, String>> pidThreadMap = new HashMap<>();
    private final List<String> fineEventNameList = new ArrayList<>();
    private int globalTrackUuid = 0;
    private final Map<String, Integer> trackMap = new TreeMap<>(); // key is pid_tid
    // 对于进程级别，key的格式为eventName，例如 processUsage
    // 对于线程级别，key的格式为eventName_threadId，例如 threadUsage_6345
    private final Map<String, Integer> fineEventTrackMap = new TreeMap<>();
    private boolean trackAssigned;
    private boolean counterTrackAssigned;

    private void addSlice(int pid, int tid, String name, long ts, TrackEventOuterClass.TrackEvent.Type type, Map<String, Object> debug, long flowId) {
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
        if (flowId != 0) {
            event.addFlowIds(flowId);
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

    public void addCounter(int pid, int tid, long ts, long val) {
        if (!trackAssigned) {
            injectTrackDescriptorPacket();
        }
        int trackUuid = 0;
        if (pid > 0 && tid > 0) {
            trackUuid = trackMap.get(getTrackKey(pid, tid));
        }
        TrackEventOuterClass.TrackEvent.Builder event = TrackEventOuterClass.TrackEvent.newBuilder()
                .setTrackUuid(trackUuid)
                .setType(TrackEventOuterClass.TrackEvent.Type.TYPE_COUNTER)
                .setCounterValue(val);

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
        } else if (v instanceof Map) {
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) v).entrySet()) {
                DebugAnnotationOuterClass.DebugAnnotation.Builder child = DebugAnnotationOuterClass.DebugAnnotation.newBuilder().setName(entry.getKey().toString());
                value.addDictEntries(setDebugValue(child, entry.getValue()).build());
            }
        } else if (v instanceof Iterable) {
            for (Object item : (Iterable<?>) v) {
                DebugAnnotationOuterClass.DebugAnnotation.Builder child = DebugAnnotationOuterClass.DebugAnnotation.newBuilder();
                value.addArrayValues(setDebugValue(child, item).build());
            }
        } else {
            value.setStringValue(Objects.toString(v));
        }
        return value;
    }

    public void addSliceBegin(int pid, int tid, String name, long ts, Map<String, Object> debug, long flowId) {
        addSlice(pid, tid, name, ts, TrackEventOuterClass.TrackEvent.Type.TYPE_SLICE_BEGIN, debug, flowId);
    }

    public void addSliceBegin(int pid, int tid, String name, long ts, Map<String, Object> debug) {
        addSliceBegin(pid, tid, name, ts, debug, 0);
    }

    public void addSliceEnd(int pid, int tid, String name, long ts) {
        addSlice(pid, tid, name, ts, TrackEventOuterClass.TrackEvent.Type.TYPE_SLICE_END, null, 0);
    }

    public void marshal(OutputStream out) throws IOException {
        injectProcessTreePacket();
        perfettoTrace.writeTo(out);
    }

    void injectTrackDescriptorPacket() {
        for (Map.Entry<Process, List<Thread>> entry : processMap.entrySet()) {
            Process process = entry.getKey();
            TrackDescriptorOuterClass.TrackDescriptor.Builder td = TrackDescriptorOuterClass.TrackDescriptor.newBuilder()
                    .setUuid(globalTrackUuid)
                    .setName(process.name);
            TracePacketOuterClass.TracePacket.Builder p = TracePacketOuterClass.TracePacket.newBuilder()
                    .setTrackDescriptor(td);
            perfettoTrace.addPacket(p);
            int parentUuid = globalTrackUuid;
            globalTrackUuid++;
            for (Thread thread : entry.getValue()) {
                td = TrackDescriptorOuterClass.TrackDescriptor.newBuilder()
                        .setUuid(globalTrackUuid)
                        .setParentUuid(parentUuid)
                        .setName(thread.name)
                        .setThread(ThreadDescriptorOuterClass.ThreadDescriptor.newBuilder()
                                .setPid(process.pid)
                                .setTid(thread.tid)
                                .setThreadName(thread.name)
                                .build());
                trackMap.put(getTrackKey(process.pid, thread.tid), globalTrackUuid);
                p = TracePacketOuterClass.TracePacket.newBuilder().setTrackDescriptor(td);
                perfettoTrace.addPacket(p);
                globalTrackUuid++;

                if (thread.counter) {
                    TrackDescriptorOuterClass.TrackDescriptor.Builder counterTrackDescriptor = TrackDescriptorOuterClass.TrackDescriptor.newBuilder()
                            .setUuid(globalTrackUuid)
                            .setName(thread.name)
                            .setParentUuid(td.getUuid())
                            .setCounter(CounterDescriptorOuterClass.CounterDescriptor.newBuilder().build());
                    trackMap.put(getTrackKey(process.pid, thread.tid), globalTrackUuid);

                    TracePacketOuterClass.TracePacket.Builder counterTrackDescriptorPacket = TracePacketOuterClass.TracePacket.newBuilder()
                            .setTrackDescriptor(counterTrackDescriptor);
                    perfettoTrace.addPacket(counterTrackDescriptorPacket);
                    globalTrackUuid++;
                }
            }
        }
        trackAssigned = true;
    }

    private void addCounterTrackDescriptorPacket(int trackUuid, String eventName, String label, int parentUuid, long... threadId) {
        TrackDescriptorOuterClass.TrackDescriptor.Builder counterTrackDescriptor = TrackDescriptorOuterClass.TrackDescriptor.newBuilder()
                .setUuid(trackUuid)
                .setName(label)
                .setParentUuid(parentUuid)
                .setCounter(CounterDescriptorOuterClass.CounterDescriptor.newBuilder().build());
        if (threadId.length > 0) {
            fineEventTrackMap.put(eventName + '_' + threadId[0], trackUuid);
        } else {
            fineEventTrackMap.put(eventName, trackUuid);
        }

        TracePacketOuterClass.TracePacket.Builder counterTrackDescriptorPacket = TracePacketOuterClass.TracePacket.newBuilder()
                .setTrackDescriptor(counterTrackDescriptor);
        perfettoTrace.addPacket(counterTrackDescriptorPacket);
    }

    public void addCounterTrackDescriptorPacketWithoutUuid(String eventName, String label, int parentUuid, long... threadId) {
        int trackUuid = globalTrackUuid++;
        TrackDescriptorOuterClass.TrackDescriptor.Builder counterTrackDescriptor = TrackDescriptorOuterClass.TrackDescriptor.newBuilder()
                .setUuid(trackUuid)
                .setName(label)
                .setParentUuid(parentUuid)
                .setCounter(CounterDescriptorOuterClass.CounterDescriptor.newBuilder().build());
        if (threadId.length > 0) {
            fineEventTrackMap.put(eventName + '_' + threadId[0], trackUuid);
        } else {
            fineEventTrackMap.put(eventName, trackUuid);
        }

        TracePacketOuterClass.TracePacket.Builder counterTrackDescriptorPacket = TracePacketOuterClass.TracePacket.newBuilder()
                .setTrackDescriptor(counterTrackDescriptor);
        perfettoTrace.addPacket(counterTrackDescriptorPacket);
    }

    public void setProcess(int pid, String name) {
        if (trackAssigned) {
            throw new RuntimeException("can not setProcess after trackAssigned");
        }
        Process process = new Process(pid, name);
        processMap.computeIfAbsent(process, p -> new ArrayList<>());
    }

    public void setProcessForFineEvent(int pid) {
        if (counterTrackAssigned) {
            throw new RuntimeException("can not setProcessForFineEvent after counterTrackAssigned");
        }
        pidThreadMap.computeIfAbsent(pid, p -> new HashMap<>());
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

    public void setThread(int pid, int tid, String name, boolean counter) {
        if (trackAssigned) {
            throw new RuntimeException("can not setThread after trackAssigned");
        }
        for (Map.Entry<Process, List<Thread>> entry : processMap.entrySet()) {
            if (entry.getKey().pid == pid) {
                Thread thread = new Thread(tid, name, counter);
                entry.getValue().add(thread);
                break;
            }
        }
    }

    public void setThreadForFineEvent(int pid, int tid, String threadName) {
        if (counterTrackAssigned) {
            throw new RuntimeException("can not setProcessForFineEvent after counterTrackAssigned");
        }
        for (Map.Entry<Integer, Map<Integer, String>> entry : pidThreadMap.entrySet()) {
            if (entry.getKey() == pid) {
                entry.getValue().put(tid, threadName);
                break;
            }
        }

    }

    public boolean checkProcessAndThread(int pid, int... tid) {
        boolean returnValue = true;
        if (!pidThreadMap.containsKey(pid)) {
            returnValue = false;
        }

        if (tid.length > 0) {
            if (!pidThreadMap.get(pid).containsKey(tid[0])) {
                returnValue = false;
            }
        }
        return returnValue;
    }

    public void setFineEventName(String fineEventName) {
        if (counterTrackAssigned) {
            throw new RuntimeException("can not setFineEventName after counterTrackAssigned");
        }
        fineEventNameList.add(fineEventName);
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
        boolean counter = false;

        public Thread(int tid, String name) {
            this.tid = tid;
            this.name = name;
        }

        public Thread(int tid, String name, boolean counter) {
            this.tid = tid;
            this.name = name;
            this.counter = counter;
        }
    }
}
