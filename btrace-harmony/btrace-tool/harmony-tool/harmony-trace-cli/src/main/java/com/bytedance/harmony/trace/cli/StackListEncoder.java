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

package com.bytedance.harmony.trace.cli;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.EmptyStackException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.Stack;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

import com.bytedance.harmony.trace.cli.perfetto.Trace;
import com.bytedance.harmony.trace.cli.perfetto.TrackSet;

public class StackListEncoder {

    public interface TraceEncoderProvider {
        int getProcessId();

        Map<Integer, ThreadStackList> groupByThreadId();

        JSONObject getExtra();

        Map<Integer, String> getThreadNames();

        boolean isAndroid();
    }

    public abstract static class EncoderListener {
        protected void onEncodeMethod(int pid, CallNode node) {
        }
    }

    public interface GlobalTrackEncoder {
        String getName();

        void encode(Trace trace, int pid, int trackId, long begin, long end);
    }

    private static final int SINGLE_SAMPLING_DURATION = 10;

    private final TraceEncoderProvider parser;
    private EncoderListener listener;
    private final List<GlobalTrackEncoder> globalTrackEncoder = new ArrayList<>();
    private ProcessStackList processStackList;
    private ThreadStackList mainThreadStackList;
    public StackListEncoder(TraceEncoderProvider parser) {
        this.parser = parser;
    }

    public StackListEncoder setListener(EncoderListener listener) {
        this.listener = listener;
        return this;
    }

    public StackListEncoder registerGlobalTrack(GlobalTrackEncoder encoder) {
        globalTrackEncoder.add(encoder);
        return this;
    }


    public void encodeAsPb(OutputStream out) throws IOException {
        Trace trace = new Trace();
        int pid = parser.getProcessId();
        TrackSet track = encodePidAndThread(trace, pid);
        encodeSlice(trace, pid, track);
        trace.marshal(out);
    }

    public TrackSet encodePidAndThread(Trace trace, int pid) {
        Map<Integer, ThreadStackList> threadItemsMap = parser.groupByThreadId();
        mainThreadStackList = threadItemsMap.get(pid);
        processStackList = new ProcessStackList(threadItemsMap.values());
        return new TrackSet(trace, pid, parser.getThreadNames(), threadItemsMap, globalTrackEncoder);
    }

    public void encodeSlice(Trace trace, int pid, TrackSet track) {
        JSONObject extra = parser.getExtra();
        Map<Integer, ThreadStackList> threadItemsMap = parser.groupByThreadId();

        for (Map.Entry<Integer, ThreadStackList> entry : threadItemsMap.entrySet()) {
            int tid = entry.getKey();
            TrackSet.Thread tracks = track.getThreadTrack(tid);
            List<StackList> java = entry.getValue().main;
            encodeThreadStats(trace, pid, tracks.traceId, entry.getValue(), processStackList);
            CallNode callNode = encodeAsPbSingleThread(trace, pid, tracks.traceId, java, extra);
        }

        // basic info: 1 轨
        if (parser instanceof StackListParser) {
            extra.put("SamplingCount", ((StackListParser) parser).getDataSize());
        }
        trace.addSliceBegin(pid, track.basicTrackId, buildGlobalInfo(extra), processStackList.beginTime(), extra.toMap());

        for (Map.Entry<Integer, GlobalTrackEncoder> entry : track.customEncoders.entrySet()) {
            entry.getValue().encode(trace, pid, entry.getKey(), processStackList.beginTime(), processStackList.endTime());
        }

        if (parser instanceof StackListParser) {
            JSONObject extraInExtra = parser.getExtra().optJSONObject("extra");
            JSONObject phaseList = extraInExtra == null ? null : extraInExtra.optJSONObject("phase");
            if (phaseList != null) {
                for (String key : phaseList.keySet()) {
                    JSONArray data = phaseList.getJSONArray(key);
                    int tid = data.getInt(0);
                    long time0 = data.getLong(1);
                    int tid2 = data.getInt(2);
                    long time1 = data.getLong(3);
                    if (tid != tid2) {
                        continue;
                    }
                    TrackSet.Thread threadTrack = track.getThreadTrack(tid);
                    if (threadTrack != null) {
                        int taskId = threadTrack.phaseId;
                        trace.addSliceBegin(pid, taskId, key, time0 + 1, phaseList.toMap());
                        trace.addSliceEnd(pid, taskId, null, time1);
                    }
                }
            }
        }
    }

    private void encodeThreadStats(Trace trace, int pid, int tid, ThreadStackList thread, ProcessStackList process) {
        if (thread.main.isEmpty()) {
            return;
        }
        StringBuilder sb = new StringBuilder();
        sb.append("Wall:").append(thread.wallDuration() / 1000_000f).append("ms").append(percent(thread.wallDuration(), process.wallDuration()));
        sb.append(" | CPU#").append(thread.cpuRank).append(":").append(thread.cpuDuration() / 1000_000f).append("ms").append(percent(thread.cpuDuration(), process.cpuDuration()));
        trace.addSliceBegin(pid, tid, sb.toString(), thread.beginTime() - 1, null);
        trace.addSliceEnd(pid, tid, null, thread.endTime() + SINGLE_SAMPLING_DURATION + 1);
    }

    private String percent(long a, long b) {
        return String.format("(%.2f%%)", (double) a / b * 100);
    }

    private String buildGlobalInfo(JSONObject extra) {
        StringBuilder sb = new StringBuilder();
        sb.append("AppVersion:").append(extra.opt("updateVersionCode"));
        long appBootTimeMs = extra.optLong("appBootTimeMs");
        long currentTimeMs = extra.getLong("currentTimeMs");
        long usageSeconds = (currentTimeMs - appBootTimeMs) / 1000;
//        sb.append(" | UsageDuration:").append(ByteFormatter.formatDuration(usageSeconds));
        sb.append(" | Activity:").append(extra.optString("scene"));
        sb.append(" | Threads:").append(processStackList.getThreadCount());
        sb.append(" | CPU:").append(processStackList.cpuDuration() / 1000_000.0f).append("ms").append(percent(processStackList.cpuDuration(), mainThreadStackList == null ? 0 : mainThreadStackList.wallDuration()));
        return sb.toString();
    }

    private CallNode encodeAsPbSingleThread(Trace trace, int pid, int tid, List<StackList> items, JSONObject extra) {
        int os = 0;
        int sceneId = 0;
        if (parser instanceof StackListParser) {
            os = ((StackListParser) parser).getOs();
            sceneId = ((StackListParser) parser).getSceneId();
        }
        CallNode root = decodeThread(items, os, sceneId);
        if (root == null) {
            return null;
        }
        if (pid == tid) {
            for (CallNode child : root.children) {
                child.extra = extra;
            }
        }
        for (CallNode child : root.children) {
            encodeTrace(trace, pid, tid, child);
        }
        return root;
    }

    public static Set<String> entriesForDebug = new HashSet<>();

    private void encodeCounterSlice(Trace trace, int pid, int trackId, List<StackList> items, ValueProvider valueProvider) {
        long lastNanoTime = 0;
        StackList first = items.get(0);
        for (int i = 1; i < items.size(); i++) {
            StackList pre = items.get(i - 1);
            StackList cur = items.get(i);
            lastNanoTime = cur.nanoTime;
            trace.addCounter(pid, trackId, pre.nanoTime, valueProvider.getValue(first, cur));
        }
        if (0 < lastNanoTime) {
            trace.addCounter(pid, trackId, lastNanoTime, 0);
        }
    }

    private interface ValueProvider {
        long getValue(StackList first, StackList cur);
    }

    // the methods suspendDurationInRange and findClosestStackListIndex have been moved to FrameMetricsAnalyzer

    public static void encodeTimeSlice(Trace trace, int pid, int tid, List<StackList> items) {
        long lastNanoTime = 0;
        for (int i = 1; i < items.size(); i++) {
            StackList pre = items.get(i - 1);
            StackList cur = items.get(i);
            lastNanoTime = cur.nanoTime;
            long deltaWall = Math.max(0, cur.nanoTime - pre.nanoTime);
            long deltaCpu = Math.max(0, cur.nanoCPUTime - pre.nanoCPUTime);
            if (deltaCpu > deltaWall) {
                deltaCpu = deltaWall;
            }
            double percent = deltaCpu * 1.0 / deltaWall;
            int pct = (int) (percent * 100);
            trace.addCounter(pid, tid, pre.nanoTime, pct);
        }

        if (0 < lastNanoTime) {
            trace.addCounter(pid, tid, lastNanoTime, 0);
        }
    }

    public static CallNode decodeThread(List<StackList> items) {
        return decodeThread(items, 0, 0);
    }

    public static CallNode decodeThread(List<StackList> items, int os, int sceneId) {
        if (items.isEmpty()) {
            return null;
        }
        StackList first = items.get(0);
        CallNode root = new CallNode(first.tid, null, first.nanoTime, 0, 0, null, 0, 0, -1, 0); // 对象池
        Stack<CallNode> stack = new Stack<>(); // 深度是可以预计算的
        stack.push(root);
        long nanoTime = 0;
        long nanoCPUTime = 0;
        for (int i = 0; i < items.size(); i++) {
            StackList curStackList = items.get(i);
            nanoTime = curStackList.nanoTime;
            nanoCPUTime = curStackList.nanoCPUTime;
            if (i == 0) {
                List<StackList.StackItem> stackTrace = curStackList.stackTrace;
                for (int j = 0; j < stackTrace.size(); j++) {
                    StackList.StackItem item = stackTrace.get(j);
                    stack.push(new CallNode(curStackList.tid, item, nanoTime, nanoCPUTime, i, stack.peek(), nanoTime, nanoCPUTime, curStackList.type, stack.size())
                            .markStackTracePosition(stackTrace.size(), j)
                            .setMessageId(curStackList.messageId)
                            .setBegin(curStackList));
                }
            } else {
                StackList preStackList = items.get(i - 1);
                boolean sameGroup = preStackList.groupId == curStackList.groupId;
                boolean sameMessage = preStackList.messageId == curStackList.messageId;
                int preIndex = 0;
                int curIndex = 0;
                if (sameGroup) {
                    if (sameMessage) {
                        while (preIndex < preStackList.size() && curIndex < curStackList.size()) {
                            if (!Objects.equals(preStackList.getName(preIndex), curStackList.getName(curIndex))) {
                                break;
                            }
                            preIndex++;
                            curIndex++;
                        }
                    } else {
                        // preIndex = curIndex = 0; // 兜底深度
                        List<StackList.StackItem> stackTrace = preStackList.stackTrace;
                        for (int j = 0; j < stackTrace.size(); j++) {
                            StackList.StackItem stackItem = stackTrace.get(j);
                            if (stackItem.method != null)
                                if (stackItem.method.symbol().contains("android.os.Looper.loop()")
                                        || stackItem.method.symbol().contains("Landroid/os/Looper;loop()V")) {
                                    preIndex = curIndex = j + 1;
                                    break;
                                }
                        }
                    }
                }
                long single_dur = SINGLE_SAMPLING_DURATION;
                for (; preIndex < preStackList.size(); preIndex++) {
                    long endTime = Long.min(nanoTime, preStackList.nanoTime + single_dur);
                    long endCpuTime = Long.min(nanoCPUTime, preStackList.nanoCPUTime + SINGLE_SAMPLING_DURATION);
                    stack.pop().end(endTime, endCpuTime, i).setEnd(preStackList);
                }
                for (; curIndex < curStackList.size(); curIndex++) {
                    StackList.StackItem item = curStackList.get(curIndex);
                    stack.push(new CallNode(curStackList.tid, item, nanoTime, nanoCPUTime, i, stack.peek(), preStackList.nanoTime, preStackList.nanoCPUTime, curStackList.type, stack.size())
                            .markStackTracePosition(curStackList.size(), curIndex)
                            .setMessageId(curStackList.messageId)
                            .setBegin(curStackList));
                }
                // 两个栈相同且 pre 栈是包含 duration 的栈，手动添加一个结束和开始，避免 Trace 连起来。
                if (preIndex == curIndex && preIndex == preStackList.size() && preStackList.begin != null && stack.size() > 1) {
                    CallNode node = stack.pop().end(preStackList.nanoTime, preStackList.nanoCPUTime, i - 1).setEnd(preStackList);
                    stack.push(new CallNode(node.tid, node.item, nanoTime, nanoCPUTime, i, stack.peek(), curStackList.nanoTime, curStackList.nanoCPUTime, curStackList.type, stack.size())
                            .markStackTracePosition(1, 0)// 模拟一个
                            .setMessageId(curStackList.messageId)
                            .setBegin(curStackList));
                }
            }
            for (CallNode callNode : stack) {
                callNode.blockTime += curStackList.blockDuration;
                callNode.gcDuration += curStackList.gcDuration;
                callNode.binderDuration += curStackList.binderDuration;
                callNode.lockDuration += curStackList.lockDuration;
                callNode.waitDuration += curStackList.waitDuration;
                callNode.parkDuration += curStackList.parkDuration;
                callNode.syncAndDrawDuration += curStackList.syncAndDrawDuration;
            }
        }
        while (!stack.isEmpty()) {
            stack.pop().end(nanoTime + SINGLE_SAMPLING_DURATION, nanoCPUTime + SINGLE_SAMPLING_DURATION, items.size()).setEnd(items.get(items.size() - 1));
        }
        root.calculateSelfDurations();
        root.calculateCppDurations();
        return root;
    }

    private void encodeTrace(Trace trace, int pid, int tid, CallNode child) {
        if (listener != null) {
            listener.onEncodeMethod(pid, child);
        }
        String symbol = child.item.method.symbol(parser.isAndroid());
        String returnType = child.item.method.returnType(parser.isAndroid());
        if (child.highlight) {
            symbol = "❌" + symbol;
        }
        trace.addSliceBegin(pid, tid, symbol, child.beginTime, buildDebugInfo(child, returnType));
        for (CallNode c : child.children) {
            encodeTrace(trace, pid, tid, c);
        }
        trace.addSliceEnd(pid, tid, symbol, child.endTime);
    }

    public static Map<String, Object> buildDebugInfo(CallNode child) {
        return buildDebugInfo(child, null);
    }

    public static Map<String, Object> buildDebugInfo(CallNode child, String returnType) {
        HashMap<String, Object> map = new LinkedHashMap<>();

        Map<String, Object> basic = new LinkedHashMap<>();
        if (child.item.method.nativeReTracedResult != null) {
            basic.put("Address", Long.toHexString(child.item.method.nativeReTracedResult.address));
        }
        basic.put("Type", child.typeAsString());
        basic.put("MessageId", child.messageId);
        basic.put("SamplingCount", child.endIndex - child.beginIndex);
        basic.put("RawSymbol", child.item.method.raw);
        basic.put("ReturnType", returnType);
        basic.put("GlobalId", child.item.method.globalID);

        Map<String, Object> time = new LinkedHashMap<>();
        time.put("Wall", child.durationNs() / 1000000.0);
        time.put("CPU", (child.endCPUTime - child.beginCPUTime) / 1000000.0);
        time.put("Block.Traced", child.blockTime / 1000000.0);
        time.put("Block.UnTraced",  (child.durationNs()-child.cpuDurationNs()-child.blockTime) / 1000000.0);
        map.put("Time", time);
        map.put("Basic", basic);
        return map;
    }

    public static Map<Integer, ThreadStackList> groupByThreadId(List<StackList> items) { // 可以按照线程排序后分批处理
        Map<Integer, ThreadStackList> map = new HashMap<>();
        for (StackList item : items) {
            ThreadStackList threadStackList = map.computeIfAbsent(item.tid, ThreadStackList::new);
            if (item.type < CallNode.kMalloc) {
                threadStackList.main.add(item);
            } else {
                threadStackList.cpp.add(item);
            }
        }
        return map;
    }
}

