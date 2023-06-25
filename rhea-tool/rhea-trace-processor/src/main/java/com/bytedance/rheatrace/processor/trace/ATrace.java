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

import perfetto.protos.FtraceEventOuterClass;

public class ATrace implements TraceEvent {
    public final int tid;
    public final long time;
    public String buf;

    public ATrace(int tid, long time, String buf) {
        this.tid = tid;
        this.time = time;
        this.buf = buf;
    }

    @Override
    public String toString() {
        return "time:" + time + "|tid:" + tid + "|" + buf;
    }

    @Override
    public FtraceEventOuterClass.FtraceEvent.Builder toEvent() {
        return perfetto.protos.FtraceEventOuterClass.FtraceEvent.newBuilder()
                .setPid(tid) // Kernel pid (do not confuse with userspace pid aka tgid)
                .setTimestamp(time)
                .setPrint(perfetto.protos.Ftrace.PrintFtraceEvent.newBuilder()
                        .setBuf(buf));
    }

    public String simpleName() {
        try {
            int second = buf.indexOf('|', buf.indexOf('|') + 1);
            return buf.substring(second + 1);
        } catch (Throwable e) {
            return buf;
        }
    }

    public boolean isBegin() {
        return buf.charAt(0) == 'B';
    }
}