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

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import java.util.Map;

public class SamplingMappingDecoder {
    private final byte[] mappingBytes;
    public final Map<Long, MethodSymbol> symbolMapping = new HashMap<>();
    public final Map<Integer, String> threadNames = new HashMap<>();

    public SamplingMappingDecoder(byte[] mappingBytes) {
        this.mappingBytes = mappingBytes;
    }

    public SamplingMappingDecoder decode() {
        ByteBuffer buffer = ByteBuffer.wrap(mappingBytes).order(ByteOrder.LITTLE_ENDIAN);
        if (buffer.remaining() < 8) {
            return this;
        }
        long maybeMagic = buffer.getLong();
        int version = buffer.getInt();
        int count = buffer.getInt();
        for (int i = 0; i < count; i++) {
            if (buffer.remaining() > 12) {
                long pointer = buffer.getLong();
                short len = buffer.getShort();
                if (len > 0) {
                    byte[] b = new byte[len];
                    buffer.get(b);
                    symbolMapping.put(pointer, new MethodSymbol(pointer, 0, new String(b)));
                }
            } else {
                break;
            }
        }
        while (buffer.hasRemaining()) {
            int tid = buffer.getShort();
            int len = buffer.get();
            if (len > 0 && len <= buffer.remaining()) {
                byte[] name = new byte[len];
                buffer.get(name);
                threadNames.put(tid, new String(name));
            } else {
                Log.e("Decode thread names failed: buffer underflow, expected name length is " + len + ", actual remaining length is " + buffer.remaining());
                break;
            }
        }
        return this;
    }

    public void retrace(ProguardMappingDecoder decoder) {
        for (MethodSymbol symbol : symbolMapping.values()) {
            symbol.symbol = decoder.retrace(symbol.symbol);
        }
    }
}