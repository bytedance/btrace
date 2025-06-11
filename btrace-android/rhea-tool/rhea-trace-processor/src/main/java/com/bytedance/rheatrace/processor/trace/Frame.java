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
package com.bytedance.rheatrace.processor.trace;

import com.bytedance.rheatrace.processor.core.TraceError;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

import perfetto.protos.FtraceEventOuterClass;

public class Frame implements TraceEvent {
    public static final int B = 1;
    public static final int E = 0;
    public static final int STATUS_UNKNOWN = 0;
    public static final int STATUS_PERFECT = 1;
    public static final int STATUS_NO_BEGIN = 2;
    public static final int STATUS_NO_END = 3;

    public int flag;
    public int methodId;
    public String method;
    public long time;
    public int threadId;
    public int pid;
    public int status;

    @Override
    public String toString() {
        return "time:" + time + "|tid:" + threadId + "|" + buf();
    }

    public Frame(int flag, long startTime, long dur, int pid, int tid, int mid, Map<Integer, String> mapping) {
        this.flag = flag;
        threadId = tid;
        methodId = mid;
        method = mapping.containsKey(methodId) ? mapping.get(methodId) : "unknown:" + mid;
        time = flag == B ? startTime : startTime + dur;
        this.pid = pid;
    }

    public Frame(int typeTid, long midTime, int pid, Map<Integer, String> mapping) {
        // type / tid / mid / nano
        //    1 /  15 /  23 /   41 = 80 (16+64)
        // 2^41 = 2199,023,255,552 = 36min
        flag = (typeTid >> 15) == 0 ? E : B;
        threadId = typeTid & 0x7FFF;
        methodId = (int) (midTime >> 41L);
        method = mapping.get(methodId);
        time = midTime & 0x1FFFFFFFFFFL;
        this.pid = pid;
    }

    private Frame() {
    }

    @Override
    public FtraceEventOuterClass.FtraceEvent.Builder toEvent() {
        return perfetto.protos.FtraceEventOuterClass.FtraceEvent.newBuilder()
                .setPid(threadId) // Kernel pid (do not confuse with userspace pid aka tgid)
                .setTimestamp(time)
                .setPrint(perfetto.protos.Ftrace.PrintFtraceEvent.newBuilder()
                        .setBuf(buf()));
    }

    public int getMethodId() {
        return methodId;
    }

    public long getTime() {
        return time;
    }

    public int getThreadId() {
        return threadId;
    }

    public boolean isBegin() {
        return flag == B;
    }

    public boolean isEnd() {
        return flag == E;
    }

    public Frame duplicate() {
        Frame dup = new Frame();
        dup.flag = flag;
        dup.methodId = methodId;
        dup.time = time;
        dup.threadId = threadId;
        dup.method = method;
        dup.pid = pid;
        dup.status = status;
        return dup;
    }

    public String buf() {
        if (flag == B) {
            return "B|" + pid + "|" + (status == STATUS_NO_BEGIN ? "*" : "") + method + (status == STATUS_NO_END ? "*" : "") + "\n";
        } else {
            return "E|" + pid + "|\n";
        }
    }

    public static List<Frame> fix(List<Frame> raw) {
        Map<Integer, List<Frame>> threadMap = new HashMap<>();
        for (Frame frame : raw) {
            threadMap.computeIfAbsent(frame.threadId, tid -> new ArrayList<>()).add(frame);
        }
        List<Frame> result = new ArrayList<>();
        for (List<Frame> value : threadMap.values()) {
            result.addAll(fixThread(value));
        }
        return result;
    }

    private static List<Frame> fixThread(List<Frame> raw) {
        if (raw.isEmpty()) {
            return raw;
        }
        Stack<Frame> stack = new Stack<>();
        for (Frame frame : raw) {
            if (frame.isBegin()) {
                stack.push(frame);
            } else if (frame.isEnd()) {
                boolean match = false;
                while (!stack.isEmpty()) {
                    Frame pop = stack.pop(); // begin candidate
                    if (pop.methodId == frame.methodId) {
                        frame.status = pop.status = Frame.STATUS_PERFECT;
                        match = true;
                        break;
                    } else {
                        pop.status = Frame.STATUS_NO_END;
                    }
                }
                if (!match) {
                    frame.status = Frame.STATUS_NO_BEGIN;
                }
            }
        }
        while (!stack.isEmpty()) {
            stack.pop().status = Frame.STATUS_NO_END;
        }
        List<Frame> ret = new ArrayList<>();
        for (int i = 0; i < raw.size(); i++) {
            Frame frame = raw.get(i);
            Frame next = i + 1 < raw.size() ? raw.get(i + 1) : null;
            switch (frame.status) {
                case Frame.STATUS_PERFECT:
                    ret.add(frame);
                    break;
                case Frame.STATUS_NO_END:
                    if (next != null) {
                        ret.add(frame);
                        Frame end = frame.duplicate();
                        end.flag = Frame.E;
                        end.time = next.time;
                        ret.add(end);
                    }
                    break;
                case Frame.STATUS_NO_BEGIN:
                    // no begin. abandon frame.
                    // we can not mock begin for this frame.
                    // it will cause problems if we merge with other trace(atrace.gz or perfetto trace)
                    break;
                case Frame.STATUS_UNKNOWN:
                default:
                    throw new TraceError("FrameFix with unexpected status " + frame.status, "please retry or report and issue.");
            }
        }
        return ret;
    }
}
