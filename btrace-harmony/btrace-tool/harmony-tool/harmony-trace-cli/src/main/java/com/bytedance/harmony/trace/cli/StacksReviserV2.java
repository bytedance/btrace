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

import static com.bytedance.harmony.trace.cli.CallNode.kJNIFirstJava;
import static com.bytedance.harmony.trace.cli.CallNode.kJNINativeCallJava;
import static com.bytedance.harmony.trace.cli.CallNode.kMalloc;
import static com.bytedance.harmony.trace.cli.CallNode.kNativeCallJava;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;
import java.util.stream.Collectors;

public class StacksReviserV2 {
    public static List<StackList> revise(List<StackList> stacks) {
        List<StackList> result = new ArrayList<>();
        Map<Integer, List<StackList>> map = new HashMap<>();
        for (StackList stack : stacks) {
            map.computeIfAbsent(stack.tid, k -> new ArrayList<>()).add(stack);
        }
        for (List<StackList> thread : map.values()) {
            List<StackList> revised;
            try {
                revised = doRevise(thread);
                StackList lastStack = null;
                for (StackList stackList : revised) {
                    // revise 可能导致部分 allocatedBytes 无值，直接匹配上一个
                    if (stackList.allocatedBytes == 0 && stackList.allocatedObjects == 0 && lastStack != null) {
                        stackList.allocatedBytes = lastStack.allocatedBytes;
                        stackList.allocatedObjects = lastStack.allocatedObjects;
                        stackList.javaIOReadBytes = lastStack.javaIOReadBytes;
                        stackList.javaIOWriteBytes = lastStack.javaIOWriteBytes;
                    }
                    lastStack = stackList;
                }
            } catch (Throwable e) {
                revised = thread.stream().filter(s -> !s.isNative).collect(Collectors.toList());
            }
            result.addAll(revised);
        }
        result.sort(Comparator.comparingLong(o -> o.nanoTime));
        return result;
    }


    private static void writeDebugStacks(List<StackList> stackLists) {
        StackList pre = null;
        try (FileWriter writer = new FileWriter("sl.txt")) {
            for (StackList sl : stackLists) {
                writer.write("TYPE:" + sl.type + "\n");
                writer.write(" CPP:" + (sl.type >= kMalloc) + "\n");
                writer.write(" DUP:" + sl.isDurationStack + "\n");
                writer.write("NANO:" + sl.nanoTime + "\n");
                writer.write(" JNI:" + sl.jniEntry + "\n");
                writer.write(" TID:" + sl.tid + "\n");
                if (pre != null) {
                    writer.write(" GAP:" + ((sl.nanoTime - pre.nanoTime) / 1000000.0) + "\n");
                }
                pre = sl;
                writer.write(sl.getStackTraceString());
                writer.write("===\n");
            }
            writer.flush();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private static List<StackList> doRevise(List<StackList> stackLists) {
        // 部分设备 fp 回溯可以抓到 java，这里把 java 裁掉
        trimOffJavasInNativeStacks(stackLists);
        // writeDebugStacks(stackLists);
        // 将纯 native 与 java/jni 切割分开
        List<List<StackList>> groups = groupConsecutive(stackLists);
        for (List<StackList> lists : groups) {
            // 纯 native 无需处理
            if (isPureNative(lists.get(0))) continue;
            // 给所有的 native 都补上 java(24)
            fillNativeWithJavaJni(lists);
            // 24 的 java 也要补上 native
            fillJavaJniWithNative(lists);
            // 处理 32819
            fillJavaWithNativeCallJava(lists);
            // 处理 native 栈的 allocation 数据
            fixNativeAllocation(lists);
        }
        // writeDebugStacks(stackLists);
        return stackLists.stream().filter(s -> !s.stackTrace.isEmpty()).collect(Collectors.toList());
    }

    private static void trimOffJavasInNativeStacks(List<StackList> stackLists) {
        for (StackList stackList : stackLists) {
            if (stackList.isNative) {
                List<StackList.StackItem> stackTrace = stackList.stackTrace;
                int index = -1;
                for (int i = stackTrace.size() - 1; i >= 0; i--) {
                    StackList.StackItem item = stackTrace.get(i);
                    // 包含 "><" 说明是 native，不包含 "><" 且包含 "." 说明是 java
                    if (!item.method.symbol.contains("><") && item.method.symbol.contains(".")) {
                        index = i;
                    }
                }
                if (index != -1) {
                    // fix ConcurrentModification with copy
                    stackList.stackTrace = new ArrayList<>(stackTrace.subList(index, stackTrace.size()));
                }
            }
        }
    }

    private static void fixNativeAllocation(List<StackList> lists) {
        // native trace 的 allocation 信息是缺失的，直接复用前序 java 的值
        List<StackList> leadingNatives = new ArrayList<>();
        StackList preJava = null;
        StackList firstJava = null;
        for (StackList list : lists) {
            if (list.isNative) {
                if (preJava != null) {
                    list.allocatedObjects = preJava.allocatedObjects;
                    list.allocatedBytes = preJava.allocatedBytes;
                    list.javaIOReadBytes = preJava.javaIOReadBytes;
                    list.javaIOWriteBytes = preJava.javaIOWriteBytes;
                } else {
                    leadingNatives.add(list);
                }
            } else {
                if (firstJava == null) {
                    firstJava = list;
                }
                preJava = list;
            }
        }
        if (firstJava != null) {
            for (StackList nativeHead : leadingNatives) {
                nativeHead.allocatedObjects = firstJava.allocatedObjects;
                nativeHead.allocatedBytes = firstJava.allocatedBytes;
                nativeHead.javaIOReadBytes = firstJava.javaIOReadBytes;
                nativeHead.javaIOWriteBytes = firstJava.javaIOWriteBytes;
            }
        }
    }

    private static boolean isPureNative(StackList s) {
        // call java 意味着需要和 java 融合，所以不是 pure native
        return s.isNative && s.jniEntry == 0 && s.type != kNativeCallJava;
    }


    public static List<List<StackList>> groupConsecutive(List<StackList> in) {
        List<List<StackList>> result = new ArrayList<>();

        StackList current = in.get(0);

        List<StackList> currentGroup = new ArrayList<>();
        currentGroup.add(current);

        for (int i = 1; i < in.size(); i++) {
            StackList next = in.get(i);
            if (isPureNative(next) == isPureNative(current)) {
                currentGroup.add(next);
            } else {
                result.add(currentGroup);
                current = next;
                currentGroup = new ArrayList<>();
                currentGroup.add(current);
            }
        }
        result.add(currentGroup);
        return result;
    }

    private static void fillJavaWithNativeCallJava(List<StackList> stackLists) {
        Stack<Integer> callJavaStack = new Stack<>();
        for (int i = 0; i < stackLists.size(); i++) {
            StackList current = stackLists.get(i);
            if (current.type == kNativeCallJava) {
                if (current.end != null) {
                    callJavaStack.push(i);
                } else if (current.begin != null) {
                    long jni = current.jniEntry;
                    assert !callJavaStack.isEmpty();
                    int index = callJavaStack.pop();
                    if (index == i - 1) { // 中间没有其他代码，可能是丢了，这个也丢掉
                        stackLists.get(index).stackTrace.clear();
                        current.stackTrace.clear();
                        continue;
                    }
                    // 1. 拷贝一下当前的 native trace
                    List<StackList.StackItem> currentNativeTrace = new ArrayList<>(current.stackTrace);
                    // 2. 32819 native trace 补上 25
                    if (jni != 0) { // else if call java from pure native
                        StackList java = stackLists.get(i - 1);
                        if (java.type != kJNINativeCallJava) {
                            throw new RuntimeException("Internal Error");
                        }
                        current.stackTrace = new ArrayList<>();
                        current.stackTrace.addAll(java.stackTrace);
                        current.stackTrace.addAll(currentNativeTrace);
                        stackLists.get(index).stackTrace = new ArrayList<>(current.stackTrace); // begin 不要漏了
                    }
                    // 3. 中间的补上 native 堆栈
                    if (jni == 0) { // call java from pure native. insert at head.
                        for (int j = index + 1; j < i; j++) {
                            StackList sl = stackLists.get(j);
                            List<StackList.StackItem> newStack = new ArrayList<>(current.stackTrace);
                            newStack.addAll(sl.stackTrace);
                            sl.stackTrace = newStack;
                        }
                    } else { // call java from jni. insert after jni.
                        for (int j = index + 1; j < i; j++) {
                            StackList sl = stackLists.get(j);
                            int target = -1;
                            for (int k = sl.stackTrace.size() - 1; k >= 0; k--) {
                                StackList.StackItem item = sl.stackTrace.get(k);
                                if (item.method.ptr == jni) {
                                    target = k;
                                    break;
                                }
                            }
                            if (target == -1) {
                                if (sl.isNative) { // 有些 native 可能补不上 java 就丢了吧
                                    sl.stackTrace.clear();
                                    continue;
                                } else {
                                    throw new RuntimeException("jni not found");
                                }
                            }
                            List<StackList.StackItem> newStack = new ArrayList<>();
                            newStack.addAll(sl.stackTrace.subList(0, target + 1));
                            newStack.addAll(currentNativeTrace);
                            newStack.addAll(sl.stackTrace.subList(target + 1, sl.stackTrace.size()));
                            sl.stackTrace = newStack;
                        }
                    }
                } else {
                    throw new RuntimeException("Internal Error");
                }
            }
        }
    }

    private static void fillJavaJniWithNative(List<StackList> stackLists) {
        for (int i = 0; i < stackLists.size() - 1/* 最后一个不用处理 */; i++) {
            StackList sl = stackLists.get(i);
            if (sl.type == kJNIFirstJava) {
                StackList next = stackLists.get(i + 1);
                if (!next.isNative) {
//                    System.err.println("next is not native");
                } else {
                    sl.stackTrace = new ArrayList<>(next.stackTrace);
                }
            }
        }
    }

    private static void fillNativeWithJavaJni(List<StackList> stackLists) {
        for (int i = 0; i < stackLists.size(); i++) {
            StackList current = stackLists.get(i);
            if (current.end != null) continue;
            if (current.isNative && current.type != kNativeCallJava) {
                StackList java = null;
                for (int j = i - 1; j >= 0; j--) {
                    StackList pre = stackLists.get(j);
                    if (pre.type == kJNIFirstJava) {
                        long jni = pre.stackTrace.get(pre.stackTrace.size() - 1).method.ptr;
                        if (jni == current.jniEntry) {
                            java = pre;
                        }
                        break;
                    }
                }
                if (java != null) {
                    List<StackList.StackItem> newStack = new ArrayList<>();
                    newStack.addAll(java.stackTrace);
                    newStack.addAll(current.stackTrace);
                    current.stackTrace = newStack;
                    if (current.begin != null) {
                        current.begin.stackTrace = new ArrayList<>(newStack);
                    }
                } else if (!current.stackTrace.isEmpty()) {
                    // 没有找到 24，或者 24 的 JNI 方法不匹配，这个 native 栈废弃掉。
                    // TODO 统计废弃数量
                    current.stackTrace.clear();
                    if (current.begin != null) {
                        current.begin.stackTrace.clear();
                    }
                }
            }
        }
    }

}
