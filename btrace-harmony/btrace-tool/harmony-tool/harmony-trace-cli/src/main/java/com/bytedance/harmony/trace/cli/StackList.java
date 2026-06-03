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

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

public class StackList {

    // 融合 Trace 字段，来自不同组的 Trace 不能融合到一起；
    // 如果两个组的 Trace 存在交集，groupId 需要合并到相同的组；
    public long groupId;

    public boolean isBlock() {
        return isDurationStack;
    }

    public String getStackTraceString() {
        StringBuilder sb = new StringBuilder();
        for (StackItem stackItem : stackTrace) {
            sb.append(stackItem.method.symbol(true));
            sb.append('\n');
        }
        return sb.toString();
    }

    public static class StackItem {
        public final MethodSymbol method;
        Object arg;

        public StackItem(MethodSymbol name) {
            this.method = name;
        }

        @Override
        public String toString() {
            return method == null ? "null" : method.toString();
        }
    }

    public long nanoTime;
    public final long nanoCPUTime;
    public List<StackItem> stackTrace;
    public int pid;
    public final int tid;
    public final int type;
    public boolean isDurationStack;
    public long duration = 0;
    public long blockDuration = 0;
    public long gcDuration = 0;
    public long binderDuration = 0;
    public long lockDuration = 0;
    public long waitDuration = 0;
    public long parkDuration = 0;
    public long syncAndDrawDuration = 0;
    public int messageId = -1;
    public long allocatedObjects;
    public long allocatedBytes;
    public long javaIOReadBytes;
    public long javaIOWriteBytes;

    public long cppAllocatedChunks;
    public long cppAllocatedBytes;
    public long jniEntry;
    public long majFlt;
    public long nvCsw;
    public long nivCsw;
    public int inBlock;
    public int ouBlock;
    public boolean isNative = false;
    public transient JSONObject extra;
    public long utcTimeDiff;
    public StackList begin;
    public StackList end;

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
            if (s.method == null || s.method.symbol.startsWith("<runtime")) {
                // TODO 这里需要区分下 method == null 和 startsWith("<runtime") 的情况，pending 在后端改动后进行
                continue;
            }
            this.stackTrace.add(s);
        }
        this.tid = tid;
        this.type = type;
    }

//    public static void handleVirtualNode(List<StackList> list) {
//        // 根据 type 添加虚拟节点
//        for (StackList sl : list) {
//            MethodSymbol typeSymbol;
//            switch (sl.type) {
//                case kGC:
//                    typeSymbol = ReservedMethodManager.gc();
//                    break;
//                case kMonitor:
//                    typeSymbol = ReservedMethodManager.lock();
//                    break;
//                case kUnlock:
//                    typeSymbol = ReservedMethodManager.unlock();
//                    break;
//                default:
//                    typeSymbol = null;
//                    break;
//            }
//            if (typeSymbol != null) {
//                StackItem gc = new StackItem(typeSymbol);
//                sl.stackTrace.add(gc);
//            }
//        }
//    }

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
        StackList stackList = new StackList(endTime, endCpuTime, new ArrayList<>(stackTrace), tid, type).setMessageId(messageId);
        stackList.isDurationStack = true;
        stackList.duration = endTime - nanoTime;
        stackList.allocatedBytes = allocatedBytes;
        stackList.allocatedObjects = allocatedObjects;
        stackList.javaIOReadBytes = javaIOReadBytes;
        stackList.javaIOWriteBytes = javaIOWriteBytes;
        stackList.cppAllocatedChunks = cppAllocatedChunks;
        stackList.cppAllocatedBytes = cppAllocatedBytes;
        stackList.inBlock = inBlock;
        stackList.ouBlock = ouBlock;
        stackList.jniEntry = jniEntry;
        stackList.majFlt = majFlt;
        stackList.nvCsw = nvCsw;
        stackList.nivCsw = nivCsw;
        stackList.isNative = isNative;
        stackList.begin = this;
        this.end = stackList;
        return stackList;
    }

    public StackList setMessageId(int messageId) {
        this.messageId = messageId;
        return this;
    }

    public void filterNativeJarvisMethod() {
        if (!isNative) return;

        int size = stackTrace.size();
        //移除叶子节点最后两层中的Jarvis函数
        if (size > 2) {
            int index1 = size - 1;
            String name1 = stackTrace.get(index1).method.symbol;
            if (name1.contains("libjarvis-trace.so")) {
                stackTrace.remove(index1);
            }
            int index2 = size - 2;
            String name2 = stackTrace.get(index2).method.symbol;
            if (name2.contains("libjarvis-trace.so")) {
                stackTrace.remove(index2);
            }
        }
    }

    public int getTid() {
        return tid;
    }

    public int getType() {
        return type;
    }

    public List<StackItem> getStackTrace() {
        return stackTrace;
    }

    public void setStackTrace(List<StackItem> stackTrace) {
        this.stackTrace = stackTrace;
    }
}