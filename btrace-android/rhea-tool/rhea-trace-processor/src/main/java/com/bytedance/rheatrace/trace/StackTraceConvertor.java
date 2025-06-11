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
package com.bytedance.rheatrace.trace;

import com.bytedance.rheatrace.core.Arguments;
import com.bytedance.rheatrace.perfetto.Trace;
import com.bytedance.rheatrace.trace.utils.ByteFormatter;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Stack;

public class StackTraceConvertor {

    private static final int SINGLE_SAMPLING_DURATION = 10;

    public static Trace convert(int pid, List<StackList> items, Map<Integer, String> threadNames) {
        items.sort(Comparator.comparingLong(o -> o.nanoTime));
        Map<Integer, List<StackList>> threadItemsMap = groupByThreadId(items);
        Trace trace = new Trace();
        trace.setProcess(pid, Arguments.get().appName);
        for (Map.Entry<Integer, List<StackList>> entry : threadItemsMap.entrySet()) {
            Integer tid = entry.getKey();
            String threadName = threadNames.get(tid);
            if (threadName == null) {
                threadName = "Thread-" + tid;
            }
            trace.setThread(pid, tid, threadName);
        }
        for (Map.Entry<Integer, List<StackList>> entry : threadItemsMap.entrySet()) {
            convertSingleThread(trace, pid, entry.getKey(), entry.getValue());
        }
        return trace;
    }

    private static void convertSingleThread(Trace trace, int pid, int tid, List<StackList> items) {
        CallNode root = decodeCallNode(items, pid == tid);
        for (CallNode child : root.children) {
            encodeTrace(trace, pid, tid, child);
        }
    }

    public static CallNode decodeCallNode(List<StackList> items, boolean main) {
        StackList first = items.get(0);
        CallNode root = new CallNode(first.tid, null, first.nanoTime, 0, 0, null, 0, 0, -1);
        Stack<CallNode> stack = new Stack<>();
        stack.push(root);
        long nanoTime = 0;
        long nanoCPUTime = 0;
        for (int i = 0; i < items.size(); i++) {
            StackList curStackList = items.get(i);
            nanoTime = curStackList.nanoTime;
            nanoCPUTime = curStackList.nanoCPUTime;
            if (i == 0) {
                for (StackList.StackItem item : curStackList.stackTrace) {
                    stack.push(new CallNode(curStackList.tid, item, nanoTime, nanoCPUTime, i, stack.peek(), nanoTime, nanoCPUTime, curStackList.type).setMessageId(curStackList.messageId).setBegin(curStackList));
                }
            } else {
                StackList preStackList = items.get(i - 1);
                // 暂时只处理主线程的 messageId
                // boolean sameMessage = !main || preStackList.messageId == curStackList.messageId;
                boolean sameMessage = true; // 相关逻辑有 bug，非必要能力先关闭
                int preIndex = 0;
                int curIndex = 0;
                while (preIndex < preStackList.size() && curIndex < curStackList.size()) {
                    // same message pop until first diff
                    // diff message pop until loopOnce
                    if (sameMessage) {
                        if (!Objects.equals(preStackList.getName(preIndex), curStackList.getName(curIndex))) {
                            break;
                        }
                        preIndex++;
                        curIndex++;
                    } else {
                        preIndex++;
                        curIndex++;
                        if (preStackList.getName(preIndex - 1).equals("void android.os.Looper.loop()")) {
                            break;
                        }
                    }
                }
                for (; preIndex < preStackList.size(); preIndex++) {
                    long endTime = Long.min(nanoTime, preStackList.nanoTime + SINGLE_SAMPLING_DURATION);
                    long endCpuTime = Long.min(nanoCPUTime, preStackList.nanoCPUTime + SINGLE_SAMPLING_DURATION);
                    stack.pop().end(endTime, endCpuTime, i).setEnd(curStackList); // FIXME: 这里导致叶子函数内存偏大
                }
                for (; curIndex < curStackList.size(); curIndex++) {
                    StackList.StackItem item = curStackList.get(curIndex);
                    stack.push(new CallNode(curStackList.tid, item, nanoTime, nanoCPUTime, i, stack.peek(), preStackList.nanoTime, preStackList.nanoCPUTime, curStackList.type).setMessageId(curStackList.messageId)).setBegin(curStackList);
                }
                // 两个栈相同且 pre 栈是包含 duration 的栈，手动添加一个结束和开始，避免 Trace 连起来。
                if (preIndex == curIndex && preIndex == preStackList.size() && preStackList.isDurationStack && stack.size() > 1) {
                    CallNode node = stack.pop().end(curStackList.nanoTime, curStackList.nanoCPUTime, i - 1).setEnd(curStackList);
                    stack.push(new CallNode(node.tid, node.item, nanoTime, nanoCPUTime, i, stack.peek(), curStackList.nanoTime, curStackList.nanoCPUTime, curStackList.type)).setMessageId(curStackList.messageId).setBegin(curStackList);
                }
            }
            for (CallNode callNode : stack) {
                callNode.blockTime += curStackList.blockDuration;
            }
        }
        while (!stack.isEmpty()) {
            stack.pop().end(nanoTime + SINGLE_SAMPLING_DURATION, nanoCPUTime + SINGLE_SAMPLING_DURATION, items.size()).setEnd(items.get(items.size() - 1));
        }
        root.calculateSelfDurations();
        return root;
    }

    private static void encodeTrace(Trace trace, int pid, int tid, CallNode child) {
        trace.addSliceBegin(pid, tid, child.item.method.symbol(), child.beginTime, buildDebugInfo(child));
        for (CallNode c : child.children) {
            encodeTrace(trace, pid, tid, c);
        }
        trace.addSliceEnd(pid, tid, child.item.method.symbol(), child.endTime);
    }

    public static Map<String, Object> buildDebugInfo(CallNode child) {
        HashMap<String, Object> map = new HashMap<>();
        if (child.begin.wakeupTid != 0) {
            map.put("WakeUpBy", child.begin.wakeupTid);
        }
        map.put("BlockTime", child.blockTime / 1000000.0);
        map.put("CPUTime", (child.endCPUTime - child.beginCPUTime) / 1000000.0);
        map.put("Count", child.endIndex - child.beginIndex);
        map.put("Gap", child.gapTime / 1000000.0);
        map.put("Gap.CPU", child.gapCpuTime / 1000000.0);
        map.put("SelfTime", child.selfDuration / 1000000.0);
        map.put("SelfCpuTime", child.selfCpuDuration / 1000000.0);
        map.put("Type", child.typeAsString());
        map.put("MessageId", child.messageId);
        map.put("AllocatedObjects", child.end.allocatedObjects - child.begin.allocatedObjects);
        map.put("AllocatedBytes", ByteFormatter.format(child.end.allocatedBytes - child.begin.allocatedBytes));
        map.put("AllocatedBytesNum", child.end.allocatedBytes - child.begin.allocatedBytes);
        map.put("MajFlt", child.end.majFlt - child.begin.majFlt);
        map.put("NvCsw", child.end.nvCsw - child.begin.nvCsw);
        map.put("NivCsw", child.end.nivCsw - child.begin.nivCsw);
        map.put("_Begin", child.beginTime / 1000000.0);
        map.put("_End", child.endTime / 1000000.0);
        if (child.item.arg != null) {
            map.put("Arg", child.item.arg);
        }
        return map;
    }

    private static Map<Integer, List<StackList>> groupByThreadId(List<StackList> items) {
        Map<Integer, List<StackList>> map = new HashMap<>();
        for (StackList item : items) {
            map.computeIfAbsent(item.tid, s -> new ArrayList<>()).add(item);
        }
        return map;
    }
}