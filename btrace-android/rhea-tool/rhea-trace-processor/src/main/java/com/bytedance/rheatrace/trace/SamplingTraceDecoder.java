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

import com.bytedance.rheatrace.Log;
import com.bytedance.rheatrace.core.Arguments;
import com.bytedance.rheatrace.core.TraceError;
import com.bytedance.rheatrace.core.Workspace;
import com.bytedance.rheatrace.perfetto.Trace;

import org.apache.commons.io.FileUtils;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class SamplingTraceDecoder {

    private static int pid = 0;

    public static int getPid() {
        return pid;
    }

    public static Trace decode() throws IOException {
        SamplingMappingDecoder mappingDecoder = decodeMapping(Workspace.samplingMapping());
        if (Arguments.get().mappingPath != null) {
            ProguardMappingDecoder proguardMappingDecoder = new ProguardMappingDecoder();
            proguardMappingDecoder.decode();
            mappingDecoder.retrace(proguardMappingDecoder);
        }
        List<StackList> samplingTrace = new ArrayList<>();
        JSONObject extra = decodeSampling(Workspace.samplingTrace(), mappingDecoder.symbolMapping, samplingTrace);
        if (samplingTrace.isEmpty()) {
            Log.red("sampling record empty");
            return null;
        }
        if (!extra.has("processId")) {
            Log.red("missing pid value from extra");
            return null;
        }
        pid = extra.getInt("processId");

        return StackTraceConvertor.convert(pid, samplingTrace, mappingDecoder.threadNames);
    }

    private static JSONObject decodeSampling(File sampling, Map<Long, MethodSymbol> mapping, List<StackList> items) throws IOException {
        byte[] samplingBytes = FileUtils.readFileToByteArray(sampling);
        ByteBuffer buffer = ByteBuffer.wrap(samplingBytes).order(ByteOrder.LITTLE_ENDIAN);
        if (buffer.remaining() < 28) {
            Log.red("buffer underflow on " + sampling.getName() + ", size is " + samplingBytes.length);
            throw new TraceError("sample trace file is empty: " + sampling.getName(), null);
        }
        buffer.getInt();
        buffer.getInt();
        int version = buffer.getInt();
        buffer.getLong();
        int count = buffer.getInt();
        int extraLength = buffer.getInt();
        JSONObject extra;
        if (extraLength > 0) {
            byte[] b = new byte[extraLength];
            buffer.get(b);
            extra = new JSONObject(new String(b));
        } else {
            extra = new JSONObject();
        }
        int pid = extra.optInt("processId", 0);
        long traceBeginTime = extra.optLong("startTime", 0) * 1000000;
        StackList.decode(version, mapping, buffer, items, traceBeginTime, pid);
        return extra;
    }

    private static SamplingMappingDecoder decodeMapping(File mapping) throws IOException {
        byte[] mappingBytes = FileUtils.readFileToByteArray(mapping);
        return new SamplingMappingDecoder(mappingBytes).decode();
    }
}