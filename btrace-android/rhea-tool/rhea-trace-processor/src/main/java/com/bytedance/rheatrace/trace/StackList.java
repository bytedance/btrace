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

import static com.bytedance.rheatrace.trace.CallNode.kBinder;
import static com.bytedance.rheatrace.trace.CallNode.kFlush;
import static com.bytedance.rheatrace.trace.CallNode.kGC;
import static com.bytedance.rheatrace.trace.CallNode.kMonitor;
import static com.bytedance.rheatrace.trace.CallNode.kMutex;
import static com.bytedance.rheatrace.trace.CallNode.kNotify;
import static com.bytedance.rheatrace.trace.CallNode.kPark;
import static com.bytedance.rheatrace.trace.CallNode.kTraceArg;
import static com.bytedance.rheatrace.trace.CallNode.kUnlock;
import static com.bytedance.rheatrace.trace.CallNode.kUnpark;
import static com.bytedance.rheatrace.trace.CallNode.kWait;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;

public class StackList {

    private static final HashSet<String> BLACK = new HashSet<>();

    static {
        BLACK.add("com.android.internal.os.ZygoteInit.main(java.lang.String[])");
        BLACK.add("com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run()");
        BLACK.add("java.lang.reflect.Method.invoke(java.lang.Object, java.lang.Object[])");
        BLACK.add("android.app.ActivityThread.main(java.lang.String[])");
    }

    public static class StackItem {
        final MethodSymbol method;
        Object arg;

        public StackItem(MethodSymbol name) {
            this.method = name;
        }
    }

    long nanoTime;
    final long nanoCPUTime;
    final List<StackItem> stackTrace;
    final int tid;
    final int type;
    boolean isDurationStack;
    long duration = 0;
    long blockDuration = 0;
    int messageId = -1;
    public long allocatedObjects;
    public long allocatedBytes;
    public long majFlt;
    public long nvCsw;
    public long nivCsw;
    public long wakeupTid;

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof StackList)) return false;
        StackList stackList = (StackList) o;
        return nanoTime == stackList.nanoTime && tid == stackList.tid;
    }

    @Override
    public int hashCode() {
        return Objects.hash(nanoTime, tid);
    }

    private StackList(long nanoTime, long nanoCPUTime, List<StackItem> stackTrace, int tid, int type) {
        this.nanoTime = nanoTime;
        this.nanoCPUTime = nanoCPUTime;
        this.stackTrace = stackTrace;
        this.tid = tid;
        this.type = type;
    }

    public StackList(long nanoTime, long nanoCPUTime, StackItem[] stackTrace, int tid, int type) {
        this.stackTrace = new ArrayList<>();
        this.nanoTime = nanoTime;
        this.nanoCPUTime = nanoCPUTime;
        for (StackItem s : stackTrace) {
            if (s.method == null) {
                throw new RuntimeException("single method mapping missing");
            }
            if (!BLACK.contains(s.method.symbol) && !s.method.symbol.startsWith("<runtime")) {
                this.stackTrace.add(s);
            }
        }
        this.tid = tid;
        this.type = type;
        MethodSymbol typeSymbol;
        switch (type) {
            case kGC:
                typeSymbol = ReservedMethodManager.gc();
                break;
            case kMonitor:
                typeSymbol = ReservedMethodManager.lock();
                break;
            default:
                typeSymbol = null;
                break;
        }
        if (typeSymbol != null) {
            StackItem gc = new StackItem(typeSymbol);
            this.stackTrace.add(gc);
        }
    }

    public int size() {
        return stackTrace.size();
    }

    public String getName(int i) {
        return get(i).method.symbol;
    }

    public long getPtr(int i) {
        return get(i).method.ptr;
    }

    public StackItem get(int i) {
        return stackTrace.get(i);
    }

    public StackList dup(long endTime, long endCpuTime) {
        StackList stackList = new StackList(endTime, endCpuTime, stackTrace, tid, type).setMessageId(messageId);
        stackList.isDurationStack = true;
        stackList.duration = endTime - nanoTime;
        stackList.allocatedBytes = allocatedBytes;
        stackList.allocatedObjects = allocatedObjects;
        stackList.majFlt = majFlt;
        stackList.nvCsw = nvCsw;
        stackList.nivCsw = nivCsw;
        if (type == kBinder || type == kGC || type == kMonitor || type == kPark || type == kWait || type == kMutex) {
            stackList.blockDuration = stackList.duration;
        }
        return stackList;
    }

    private StackList setMessageId(int messageId) {
        this.messageId = messageId;
        return this;
    }

    public static boolean decode(int version, Map<Long, MethodSymbol> mapping, ByteBuffer buffer, List<StackList> result, long traceBeginTime, int pid) {
        Map<Long, Integer> wakers = new HashMap<>();
        while (buffer.hasRemaining()) {
            int type = buffer.getShort() & 0xffff;
            int tid = buffer.getShort();
            int messageId = buffer.getInt();
            long nanoTime = buffer.getLong();
            long nanoTimeEnd = buffer.getLong();
            if (traceBeginTime > 0 && nanoTime < traceBeginTime && nanoTimeEnd > nanoTime) {
                nanoTime = traceBeginTime;
            }
            long cpuTime = buffer.getLong();
            long cpuTimeEnd = buffer.getLong();
            if (version >= 5) {
                if (type == kUnlock || type == kUnpark || type == kNotify) {
                    wakers.put(nanoTime, tid);
                    nanoTime = nanoTimeEnd;
                    cpuTime = cpuTimeEnd;
                }
            }
            Object arg = null;
            if (type == kTraceArg) {
                arg = buffer.getLong();
            }
            long allocatedObjects = version < 4 ? 0 : buffer.getLong();
            long allocatedBytes = version < 4 ? 0 : buffer.getLong();
            int majFlt = version < 5 ? 0 : buffer.getInt();
            int nvCsw = version < 5 ? 0 : buffer.getInt();
            int nivCsw = version < 5 ? 0 : buffer.getInt();
            int savedDepth = buffer.getInt();
            int actualDepth = buffer.getInt();
            boolean valid = actualDepth == savedDepth && savedDepth > 0; // invalid 数据可以快速处理
            if (savedDepth > Short.MAX_VALUE) {
                throw new RuntimeException("invalid depth " + savedDepth);
            }
            StackItem[] stack = new StackItem[savedDepth];
            for (int i = 0; i < savedDepth; i++) {
                long pointer = buffer.getLong();
                MethodSymbol method = mapping.get(pointer);
                stack[savedDepth - i - 1] = new StackItem(method);
            }
            if (type == kTraceArg) {
                stack[savedDepth - 2].arg = arg;
            }
            if (valid) {
                if (type == kFlush) {
                    // flush 是主线程最后一个堆栈 dump，复制成 2 个栈
                    StackList preMain = null;
                    for (int i = result.size() - 1; i >= 0; i--) {
                        StackList temp = result.get(i);
                        if (temp.tid == tid) {
                            preMain = temp;
                            break;
                        }
                    }
                    if (preMain == null) {
                        continue;
                    }
                    long beginTime = preMain.nanoTime + 2000_000;
                    if (beginTime >= nanoTime) { // 上一个栈结束时间小于 2ms，说明这个栈无意义，直接忽略
                        continue;
                    }
                    long mockCPUTime = preMain.nanoCPUTime;// flush 是异步抓栈，没有 cpu time
                    int preMessageId = preMain.messageId;
                    StackList item = new StackList(beginTime, mockCPUTime, stack, tid, type).setMessageId(preMessageId);
                    item.allocatedObjects = preMain.allocatedObjects;
                    item.allocatedBytes = preMain.allocatedBytes;
                    item.majFlt = preMain.majFlt;
                    item.nvCsw = preMain.nvCsw;
                    item.nivCsw = preMain.nivCsw;
                    result.add(item);
                    result.add(item.dup(nanoTime, mockCPUTime));
                } else {
                    StackList item = new StackList(nanoTime, cpuTime, stack, tid, type).setMessageId(messageId);
                    item.allocatedObjects = allocatedObjects;
                    item.allocatedBytes = allocatedBytes;
                    item.majFlt = majFlt;
                    item.nvCsw = nvCsw;
                    item.nivCsw = nivCsw;
                    result.add(item);
                    if (nanoTimeEnd > nanoTime) { // 存在异常数据，nanoTimeEnd 小于 nanoTime 的情况，可能与时间戳同步有关，暂时忽略。
                        StackList end = item.dup(nanoTimeEnd, cpuTimeEnd);
                        result.add(end);
                    }
                }
            }
        }
        for (StackList item : result) {
            if (item.tid == pid && (item.type == kMonitor || item.type == kPark || item.type == kWait)) {
                int wakeupTid = wakers.getOrDefault(item.nanoTime, 0);
                if (wakeupTid != 0) {
                    item.wakeupTid = wakeupTid;
                }
            }
        }
        result.sort(Comparator.comparingLong(o -> o.nanoTime));
        return true;
    }
}