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

import java.util.List;

import perfetto.protos.FtraceEventBundleOuterClass;
import perfetto.protos.TraceOuterClass;
import perfetto.protos.TracePacketOuterClass;

public class TraceAssembler {
    private static final int FTRACE_PER_PACKET = 4096;

    public static void assemble(TraceOuterClass.Trace.Builder target, List<Frame> frames, List<ATrace> atrace) {
        assemble(target, frames);
        assemble(target, atrace);
    }

    private static void assemble(TraceOuterClass.Trace.Builder target, List<? extends TraceEvent> events) {
        if (events.isEmpty()) {
            return;
        }
        FtraceEventBundleOuterClass.FtraceEventBundle.Builder bundle = null;
        for (int i = 0; i < events.size(); i++) {
            if (i % FTRACE_PER_PACKET == 0) {
                if (bundle != null) {
                    target.addPacket(TracePacketOuterClass.TracePacket.newBuilder().setFtraceEvents(bundle).build());
                }
                bundle = FtraceEventBundleOuterClass.FtraceEventBundle.newBuilder().setCpu(0);
            }
            bundle.addEvent(events.get(i).toEvent());
        }
        target.addPacket(TracePacketOuterClass.TracePacket.newBuilder().setFtraceEvents(bundle).build());
    }
}
