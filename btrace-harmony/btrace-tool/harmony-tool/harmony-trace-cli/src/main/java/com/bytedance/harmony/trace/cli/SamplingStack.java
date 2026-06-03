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


import static com.bytedance.harmony.trace.cli.StackListParser.OS_ANDROID;
import static com.bytedance.harmony.trace.cli.StackListParser.OS_HARMONY;
import static com.bytedance.harmony.trace.cli.StackListParser.OS_IOS;

import java.nio.ByteBuffer;

public class SamplingStack {
    public int type;
    public int tid;
    public int messageId;
    public long nanoTime;
    public long nanoTimeEnd;
    public long cpuTime;
    public long cpuTimeEnd;
    public long allocatedObjects;
    public long allocatedBytes;
    public long javaIOReadBytes;
    public long javaIOWriteBytes;

    public long cppAllocatedChunks;
    public long cppAllocatedBytes;
    public int majFlt;
    public int nvCsw;
    public int nivCsw;
    public int inBlock;
    public int ouBlock;
    public int batchIndex;
    public int batchCount;
    public int savedDepth;
    public int actualDepth;
    public long[] stack;

    public SamplingStack(ByteBuffer buffer, int os, int version) {
        type = buffer.getShort() & 0xffff;
        if (os == OS_ANDROID) {
            tid = buffer.getShort();
        } else if (os == OS_IOS || os == OS_HARMONY) {
            tid = buffer.getInt();
        } else {
            throw new RuntimeException("unknown os " + os);
        }
        messageId = buffer.getInt();
        nanoTime = buffer.getLong();
        nanoTimeEnd = buffer.getLong();
        cpuTime = buffer.getLong();
        cpuTimeEnd = buffer.getLong();
        if (version >= 4) {
            allocatedObjects = buffer.getLong();
            allocatedBytes = buffer.getLong();
        }
        if (version >= 7) {
            cppAllocatedChunks = buffer.getLong();
            cppAllocatedBytes = buffer.getLong();
        }
        if (version >= 5) {
            majFlt = buffer.getInt();
            nvCsw = buffer.getInt();
            nivCsw = buffer.getInt();
        }
        if (version >= 6) {
            batchIndex = buffer.getShort();
            batchCount = buffer.getShort();
        } else {
            batchCount = 1;
        }
        if (version >= 6) {
            savedDepth = buffer.getShort();
            actualDepth = buffer.getShort();
        } else {
            savedDepth = buffer.getInt();
            actualDepth = buffer.getInt();
        }
        stack = new long[savedDepth];
        for (int i = 0; i < savedDepth; i++) {
            stack[i] = buffer.getLong();
        }
    }

    public StackList.StackItem[] buildStackTrace(SamplingMappingDecoder mappingDecoder) {
        StackList.StackItem[] result = new StackList.StackItem[this.stack.length];
        for (int i = 0; i < this.stack.length; i++) {
            long pointer = this.stack[i];
            MethodSymbol method = mappingDecoder.symbolMapping.get(pointer);
            result[this.stack.length - i - 1] = new StackList.StackItem(method);
        }
        return result;
    }
}
