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
package com.bytedance.rheatrace.processor.perfetto;

import com.bytedance.rheatrace.processor.Log;
import com.bytedance.rheatrace.processor.core.AbstractTraceGenerator;
import com.bytedance.rheatrace.processor.core.TraceError;
import com.bytedance.rheatrace.processor.core.Workspace;
import com.bytedance.rheatrace.processor.trace.ATrace;
import com.bytedance.rheatrace.processor.trace.Frame;
import com.bytedance.rheatrace.processor.trace.TraceAssembler;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.util.List;

import perfetto.protos.FtraceEventOuterClass;
import perfetto.protos.TraceOuterClass;
import perfetto.protos.TracePacketOuterClass;

public class TraceProcessor extends AbstractTraceGenerator {
    private final TraceOuterClass.Trace.Builder systemTrace;

    public TraceProcessor() throws IOException {
        if (Workspace.systemTrace().exists()) {
            systemTrace = TraceOuterClass.Trace.parseFrom(Files.newInputStream(Workspace.systemTrace().toPath())).toBuilder();
        } else {
            throw new TraceError("systrace file not found: " + Workspace.systemTrace(), "your device may not support perfetto. please retry with `-mode simple`.");
        }
    }

    @Override
    protected long getSystemFTraceTime() {
        for (TracePacketOuterClass.TracePacket tracePacket : systemTrace.getPacketList()) {
            if (tracePacket.getDataCase() == TracePacketOuterClass.TracePacket.DataCase.FTRACE_EVENTS) {
                for (FtraceEventOuterClass.FtraceEvent ftraceEvent : tracePacket.getFtraceEvents().getEventList()) {
                    return ftraceEvent.getTimestamp();
                }
            }
        }
        Log.e("no ftrace found at " + Workspace.systemTrace());
        throw new TraceError("ftrace time not found in systrace", "check your app is running and try re-run command with `-b 200mb`.");
    }

    @Override
    protected void merge(List<Frame> binaryFrame, List<ATrace> aTrace) throws IOException {
        TraceAssembler.assemble(systemTrace, binaryFrame, aTrace);
        File output = Workspace.output();
        try (FileOutputStream out = new FileOutputStream(output)) {
            Log.red("writing trace:" + output);
            systemTrace.build().writeTo(out);
        }
    }
}