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

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

public class CallNode {
    public final int tid;
    public StackList.StackItem item;
    public long beginTime;
    public long beginCPUTime;
    public int beginIndex;
    public final List<CallNode> children = new ArrayList<>();
    public long gapTime;
    public long gapCpuTime;
    public int type;
    public transient String filename;
    public transient StackList begin;
    public transient StackList end;

    public CallNode setBegin(StackList begin) {
        this.begin = begin;
        return this;
    }

    public CallNode setEnd(StackList end) {
        this.end = end;
        return this;
    }

    public CallNode(int tid) {
        this.tid = tid;
    }

    public CallNode(int tid, StackList.StackItem item, long beginTime, long beginCPUTime, int beginIndex, CallNode parent, long lastBeginTime, long lastBeginCPUTime, int type) {
        this.tid = tid;
        this.item = item;
        this.beginTime = beginTime;
        this.beginCPUTime = beginCPUTime;
        this.beginIndex = beginIndex;
        this.gapTime = beginTime - lastBeginTime;
        this.gapCpuTime = beginCPUTime - lastBeginCPUTime;
        if (parent != null) {
            parent.children.add(this);
        }
        this.type = type;
    }

    long endTime;
    long endCPUTime;
    int endIndex;
    long selfDuration;
    long selfCpuDuration;

    int messageId;

    CallNode end(long endTime, long endCPUTime, int endIndex) {
        this.endTime = endTime;
        this.endCPUTime = endCPUTime;
        this.endIndex = endIndex;
        return this;
    }

    public CallNode setMessageId(int messageId) {
        this.messageId = messageId;
        return this;
    }

    long beginTimeNs() {
        return beginTime;
    }

    long beginTimeMs() {
        return TimeUnit.NANOSECONDS.toMillis(beginTimeNs());
    }

    int durationMs() {
        // return (int) TimeUnit.NANOSECONDS.toMillis(durationNs());
        // 比如 begin(ns) 是 11,999. end(ns) 是 12,000. 先计算 duration 再转 ms 和先转 ms 再算 duration 结果不同
        // 前者是 0, 后者是 1
        // 为了保证 begin(ms) + duration(ms) = end(ms) 需要先转再减。
        return (int) (TimeUnit.NANOSECONDS.toMillis(endTime) - TimeUnit.NANOSECONDS.toMillis(beginTime));
    }

    int cpuDurationMs() {
        return (int) (TimeUnit.NANOSECONDS.toMillis(endCPUTime) - TimeUnit.NANOSECONDS.toMillis(beginCPUTime));
    }

    long durationNs() {
        return (endTime - beginTime);
    }

    long cpuDurationNs() {
        return (endCPUTime - beginCPUTime);
    }

    public void calculateSelfDurations() {
        long childrenSum = 0;
        long childrenCpuSum = 0;
        for (CallNode child : children) {
            childrenSum += child.durationNs();
            childrenCpuSum += child.cpuDurationNs();
            child.calculateSelfDurations();
        }
        selfDuration = durationNs() - childrenSum;
        selfCpuDuration = cpuDurationNs() - childrenCpuSum;
    }

    public int selfDurationMs() {
        return (int) TimeUnit.NANOSECONDS.toMillis(selfDurationNs());
    }

    public int selfCpuDurationMs() {
        return (int) TimeUnit.NANOSECONDS.toMillis(selfCpuDurationNs());
    }

    public int selfDurationNs() {
        return (int) selfDuration;
    }

    public int selfCpuDurationNs() {
        return (int) selfCpuDuration;
    }

    public int blockTimeMs() {
        return (int) TimeUnit.NANOSECONDS.toMillis(blockTime);
    }

    public String typeAsString() {
        return getType(type);
    }

    public long blockTime;

    public static final int kInvalid = 1;
    public static final int kBinder = 2;
    public static final int kJankMessage = 3;
    public static final int kCustom = 4;
    public static final int kTraceStack = 5;
    public static final int kWait = 6;
    public static final int kPark = 7;
    public static final int kMonitor = 8;
    public static final int kObjectAllocation = 9;
    public static final int kJNITrampoline = 10;
    public static final int kGC = 11;
    public static final int kMutex = 12;
    public static final int kDispatchVsync = 13;
    public static final int kSyncAndDrawFrame = 14;
    public static final int kTraceArg = 15;
    public static final int kFlush = 16;
    public static final int kUnpark = 17;
    public static final int kScene = 18;
    public static final int kGCInternal = 19;
    public static final int kNativePollOnce = 21;
    public static final int kNotify = 22;
    public static final int kUnlock = 23;

    public static String getType(int type) {
        switch (type) {
            case kInvalid:
                return "kInvalid";
            case kBinder:
                return "kBinder";
            case kJankMessage:
                return "kJankMessage";
            case kCustom:
                return "kCustom";
            case kTraceStack:
                return "kTraceStack";
            case kWait:
                return "kWait";
            case kPark:
                return "kPark";
            case kMonitor:
                return "kMonitor";
            case kObjectAllocation:
                return "kObjectAllocation";
            case kJNITrampoline:
                return "kJNITrampoline";
            case kGC:
                return "kGC";
            case kMutex:
                return "kMutex";
            case kDispatchVsync:
                return "kDispatchVsync";
            case kSyncAndDrawFrame:
                return "kSyncAndDrawFrame";
            case kTraceArg:
                return "kTraceArg";
            case kFlush:
                return "kFlush";
            case kUnpark:
                return "kUnpark";
            case kScene:
                return "kScene";
            case kGCInternal:
                return "kGCInternal";
            case kNativePollOnce:
                return "kNativePollOnce";
            case kNotify:
                return "kNotify";
            case kUnlock:
                return "kUnlock";
        }
        return String.valueOf(type);
    }

    @Override
    public String toString() {
        return item == null ? "<root>" : item.method.symbol;
    }
}