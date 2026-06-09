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

import org.json.JSONObject;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.TimeUnit;

public class CallNode implements NanoSpan {
    public final int tid;
    public StackList.StackItem item;
    public long beginTime;
    public long beginCPUTime;
    public int beginIndex;
    public final List<CallNode> children = new ArrayList<>();
    public transient CallNode caller;
    public long gapTime;
    public long gapCpuTime;
    public int type;
    public JSONObject extra;
    public transient String filename;
    public transient double similarity;
    public transient StackList begin;
    public transient StackList end;
    public boolean highlight;
    public final int depth;
    /* 是否是栈回溯时的起点 */
    public boolean isBacktraceLeaf;
    public int rn;

    public String name() {
        return item == null ? "" : item.method.symbol();
    }

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
        this.depth = 0;
    }

    public CallNode(int tid, StackList.StackItem item, long beginTime, long beginCPUTime, int beginIndex, CallNode parent, long lastBeginTime, long lastBeginCPUTime, int type, int depth) {
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
        this.caller = parent;
        this.type = type;
        this.depth = depth;
    }

    public long endTime;
    public long endCPUTime;
    int endIndex;
    long selfDuration;
    long selfCpuDuration;
    long cppDuration;
    long cppCpuDuration;

    public int messageId;

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

    public int durationMs() {
        // return (int) TimeUnit.NANOSECONDS.toMillis(durationNs());
        // 比如 begin(ns) 是 11,999. end(ns) 是 12,000. 先计算 duration 再转 ms 和先转 ms 再算 duration 结果不同
        // 前者是 0, 后者是 1
        // 为了保证 begin(ms) + duration(ms) = end(ms) 需要先转再减。
        return (int) (TimeUnit.NANOSECONDS.toMillis(endTime) - TimeUnit.NANOSECONDS.toMillis(beginTime));
    }

    public int cppDurationMs() {
        return (int) TimeUnit.NANOSECONDS.toMillis(cppDuration);
    }

    public int cppCpuDurationMs() {
        return (int) TimeUnit.NANOSECONDS.toMillis(cppCpuDuration);
    }

    public int cpuDurationMs() {
        return (int) (TimeUnit.NANOSECONDS.toMillis(endCPUTime) - TimeUnit.NANOSECONDS.toMillis(beginCPUTime));
    }

    public long durationNs() {
        return (endTime - beginTime);
    }

    public long cpuDurationNs() {
        return (endCPUTime - beginCPUTime);
    }

    public long safeCpuDurationNs() {
        return (endCPUTime - beginCPUTime) < 0 ? 0 : (endCPUTime - beginCPUTime);
    }

    public void calculateSelfDurations() {
        Deque<CallNode> stack = new ArrayDeque<>();
        Deque<Boolean> processed = new ArrayDeque<>();

        stack.push(this);
        processed.push(false);

        while (!stack.isEmpty()) {
            CallNode node = stack.pop();
            boolean isProcessed = processed.pop();

            if (!isProcessed) {
                stack.push(node);
                processed.push(true);

                for (CallNode child : node.children) {
                    stack.push(child);
                    processed.push(false);
                }
            } else {
                long childrenSum = 0, childrenCpuSum = 0;
                for (CallNode child : node.children) {
                    childrenSum += child.durationNs();
                    childrenCpuSum += child.cpuDurationNs();
                }
                node.selfDuration = node.durationNs() - childrenSum;
                node.selfCpuDuration = node.cpuDurationNs() - childrenCpuSum;
            }
        }
    }

    public int selfDurationMs() {
        return (int) TimeUnit.NANOSECONDS.toMillis(selfDuration);
    }

    public int selfCpuDurationMs() {
        return (int) TimeUnit.NANOSECONDS.toMillis(selfCpuDuration);
    }

    public int blockTimeMs() {
        return (int) TimeUnit.NANOSECONDS.toMillis(blockTime);
    }

    public long blockTimeNs() {
        return blockTime;
    }

    public CallNode markStackTracePosition(int stackTraceSize, int stackTraceIndex) {
        this.isBacktraceLeaf = stackTraceIndex + 1 == stackTraceSize;
        return this;
    }

    public String typeAsString() {
        return getType(type);
    }

    public long blockTime;
    public long gcDuration = 0;
    public long binderDuration = 0;
    public long lockDuration = 0;
    public long waitDuration = 0;
    public long parkDuration = 0;
    public long syncAndDrawDuration = 0;

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
    /* JNI 内部 native 桩点首次执行会抓一个 Java 栈 */
    public static final int kJNIFirstJava = 24;
    /* JNI 内部 native call java 结束时会按需抓个栈*/
    public static final int kJNINativeCallJava = 25;
    public static final int kSuspendThreadFromJava = 27;
    public static final int kResumeThreadFromJava = 28;
    public static final int kSuspendAllFromJava = 29;
    public static final int kResumeAllFromJava = 30;
    public static final int kMalloc = 32768;
    public static final int kCalloc = 32769;
    public static final int kRealloc = 32770;
    public static final int kRealloArray = 32771;
    public static final int kFree = 32772;
    public static final int kMmap = 32773;
    public static final int kMadvise = 32774;
    public static final int kMemset = 32775;
    public static final int kMemcpy = 32776;
    public static final int kMemcmp = 32777;
    public static final int kStrcpy = 32778;
    public static final int kStpcpy = 32779;
    public static final int kStrcat = 32780;
    public static final int kStrcmp = 32781;
    public static final int kStrncmp = 32782;
    public static final int kStrstr = 32783;
    public static final int kStrchr = 32784;
    public static final int kStrrchr = 32785;
    public static final int kStrlen = 32786;
    public static final int kOpen = 32787;
    public static final int kRead = 32788;
    public static final int kWrite = 32789;
    public static final int kClose = 32790;
    public static final int kEpoll = 32791;
    public static final int kFork = 32792;
    public static final int kMutexLock = 32793;
    public static final int kRWMutexWLock = 32794;
    public static final int kRWMutexRLock = 32795;
    public static final int kCCRun = 32796;
    public static final int kCCMark = 32797;
    public static final int kCCCopy = 32798;
    public static final int kCCReclaim = 32799;
    public static final int kMSRun = 32800;
    public static final int kMSMark = 32801;
    public static final int kMSPause = 32802;
    public static final int kMSReclaim = 32803;
    public static final int kMCRun = 32804;
    public static final int kMCMark = 32805;
    public static final int kMCCompact = 32806;
    public static final int kMCReclaim = 32807;
    public static final int kSSRun = 32808;
    public static final int kSSMark = 32809;
    public static final int kSSReclaim = 32810;
    public static final int kRenderProxyStopped = 32811;
    public static final int kInputConsumerCtor = 32812;
    public static final int kBufferQueueProducerCtor = 32813;
    public static final int kRender = 32814;
    public static final int kCallJava = 32815;
    public static final int kNativeMessage = 32816;
    public static final int kPthreadMutexLock = 32817;
    public static final int kPthreadMutexUnlock = 32818;
    public static final int kNativeCallJava = 32819;
    public static final int kSuspend = 32823;
    public static final int kResume = 32824;
    public static final int kSuspendAll = 32825;
    public static final int kResumeAll = 32826;
    public static final int kSuspendAllStartPause = 32827;
    public static final int kResumeAllEndPause = 32828;

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
            case kJNIFirstJava:
                return "kJNIFirstJava";
            case kJNINativeCallJava:
                return "kJNINativeCallJava";
            case kMalloc:
                return "kMalloc";
            case kCalloc:
                return "kCalloc";
            case kRealloc:
                return "kRealloc";
            case kRealloArray:
                return "kRealloArray";
            case kFree:
                return "kFree";
            case kMmap:
                return "kMmap";
            case kMadvise:
                return "kMadvise";
            case kMemset:
                return "kMemset";
            case kMemcpy:
                return "kMemcpy";
            case kMemcmp:
                return "kMemcmp";
            case kStrcpy:
                return "kStrcpy";
            case kStpcpy:
                return "kStpcpy";
            case kStrcat:
                return "kStrcat";
            case kStrcmp:
                return "kStrcmp";
            case kStrncmp:
                return "kStrncmp";
            case kStrstr:
                return "kStrstr";
            case kStrchr:
                return "kStrchr";
            case kStrrchr:
                return "kStrrchr";
            case kStrlen:
                return "kStrlen";
            case kOpen:
                return "kOpen";
            case kRead:
                return "kRead";
            case kWrite:
                return "kWrite";
            case kClose:
                return "kClose";
            case kEpoll:
                return "kEpoll";
            case kFork:
                return "kFork";
            case kMutexLock:
                return "kMutexLock";
            case kRWMutexWLock:
                return "kRWMutexWLock";
            case kRWMutexRLock:
                return "kRWMutexRLock";
            case kCCRun:
                return "kCCRun";
            case kCCMark:
                return "kCCMark";
            case kCCCopy:
                return "kCCCopy";
            case kCCReclaim:
                return "kCCReclaim";
            case kMSRun:
                return "kMSRun";
            case kMSMark:
                return "kMSMark";
            case kMSPause:
                return "kMSPause";
            case kMSReclaim:
                return "kMSReclaim";
            case kMCRun:
                return "kMCRun";
            case kMCMark:
                return "kMCMark";
            case kMCCompact:
                return "kMCCompact";
            case kMCReclaim:
                return "kMCReclaim";
            case kSSRun:
                return "kSSRun";
            case kSSMark:
                return "kSSMark";
            case kSSReclaim:
                return "kSSReclaim";
            case kNativeCallJava:
                return "kNativeCallJava";
            case kSuspend:
            case kSuspendThreadFromJava:
                return "kSuspend";
            case kResume:
            case kResumeThreadFromJava:
                return "kResume";
            case kSuspendAll:
            case kSuspendAllFromJava:
                return "kSuspendAll";
            case kResumeAll:
            case kResumeAllFromJava:
                return "kResumeAll";
            case kSuspendAllStartPause:
                return "kSuspendAllStartPause";
            case kResumeAllEndPause:
                return "kResumeAllEndPause";
        }
        return String.valueOf(type);
    }

    @Override
    public String toString() {
        return item == null ? "<root>" : item.method.symbol;
    }

    public void calculateCppDurations() {
        long childrenSumCpp = 0;
        long childrenSumCppCpu = 0;
        for (CallNode child : children) {
            child.calculateCppDurations();
            childrenSumCpp += child.cppDuration;
            childrenSumCppCpu += child.cppCpuDuration;
        }
        if (item == null || item.method == null) {
            return;
        }
        if (item.method.isNativeSymbol()) {
            cppDuration = durationNs();
            cppCpuDuration = cpuDurationNs();
        } else {
            cppDuration = childrenSumCpp;
            cppCpuDuration = childrenSumCppCpu;
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        CallNode callNode = (CallNode) o;
        return tid == callNode.tid && beginTime == callNode.beginTime && depth == callNode.depth;
    }

    @Override
    public int hashCode() {
        return Objects.hash(tid, beginTime, depth);
    }

    @Override
    public long beginTimeNano() {
        return beginTime;
    }

    @Override
    public long endTimeNano() {
        return endTime;
    }
}
