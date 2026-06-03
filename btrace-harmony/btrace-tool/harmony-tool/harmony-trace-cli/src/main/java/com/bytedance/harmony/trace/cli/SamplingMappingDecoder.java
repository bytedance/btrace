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

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class SamplingMappingDecoder {
    public interface OnDecodeListener {
        void onDecodeFinish(SamplingMappingDecoder decoder) throws IOException;
    }

    private static OnDecodeListener onDecodeListener;

    public static void setOnDecodeListener(OnDecodeListener onDecodeListener) {
        SamplingMappingDecoder.onDecodeListener = onDecodeListener;
    }

    private final SamplingFile samplingFile;
    public final String updateVersionCode;
    public final Map<Long, MethodSymbol> symbolMapping = new HashMap<>();
    public final Map<String, String> soBuildIds = new HashMap<>();
    public final Map<Integer, String> threadNames = new HashMap<>();
    public final Map<Integer, Integer> threadPriorities = new HashMap<>();

    public SamplingMappingDecoder(SamplingFile sf, String updateVersionCode) {
        this.samplingFile = sf;
        this.updateVersionCode = updateVersionCode;
    }

    public SamplingMappingDecoder(SamplingFile sf) {
        this(sf, null);
    }

    public SamplingMappingDecoder decode(int osType) {
        if (samplingFile.mapping != null) {
            decode(samplingFile.mapping, osType);
        } else {
            decodeJava(samplingFile.mappingJava);
            decodeThreads(samplingFile.mappingThread);
            decodeNative(samplingFile.mappingNative);
        }
        onDecodeFinish();
        return this;
    }

    private void decodeJava(byte[] java) {
        if (java == null) {
            return;
        }
        ByteBuffer buffer = ByteBuffer.wrap(java).order(ByteOrder.LITTLE_ENDIAN);
        int version = buffer.getInt();
        int count = buffer.getInt();
        for (int i = 0; i < count; i++) {
            long ptr = buffer.getLong();
            int len = buffer.getShort();
            byte[] b = new byte[len];
            buffer.get(b);
            String symbol = new String(b);
            short nativeFlag = buffer.getShort();
            symbolMapping.put(ptr, new MethodSymbol(ptr, 0, symbol, nativeFlag));
        }
    }

    private void decodeNative(byte[] mapping) {
        if (mapping == null) {
            return;
        }
        ByteBuffer buffer = ByteBuffer.wrap(mapping).order(ByteOrder.LITTLE_ENDIAN);
        int version = buffer.getInt();
        int count = buffer.getInt();
        for (int i = 0; i < count; i++) {
            long ptr = buffer.getLong();
            int len = buffer.getShort();
            byte[] b = new byte[len];
            buffer.get(b);
            String symbol = new String(b);
            symbolMapping.put(ptr, new MethodSymbol(ptr, 0, symbol, (short) 0));
        }
        int soBuildIdCount = buffer.getInt();
        for (int i = 0; i < soBuildIdCount; i++) {
            int soNameLen = buffer.getInt();
            byte[] b = new byte[soNameLen];
            buffer.get(b);
            String soName = new String(b);
            int soBuildIdLen = buffer.getInt();
            b = new byte[soBuildIdLen];
            buffer.get(b);
            String soBuildId = new String(b);
            soBuildIds.put(soName, soBuildId);
        }
    }

    private void decodeThreads(byte[] mapping) {
        if (mapping == null) {
            return;
        }
        try {
            ByteBuffer buffer = ByteBuffer.wrap(mapping).order(ByteOrder.LITTLE_ENDIAN);
            int version = buffer.getInt();
            while (buffer.remaining() > 4) {
                int tid = buffer.getInt();
                int len = buffer.get();
                byte[] b = new byte[len];
                buffer.get(b);
                String name = new String(b).trim();
                threadNames.put(tid, name);
                if (version >= 2) {
                    int priority = buffer.get();
                    threadPriorities.put(tid, priority);
                }
            }
        } catch (Throwable ignore) {
        }
    }

    private void decode(byte[] mappingBytes, int osType) {
        ByteBuffer buffer = ByteBuffer.wrap(mappingBytes).order(ByteOrder.LITTLE_ENDIAN);
        if (buffer.remaining() < 8) {
            return;
        }
        long maybeMagic = buffer.getLong();
        if (maybeMagic == 0) {
            int version = buffer.getInt();
            int count = buffer.getInt();
            int symbolBytes = 12;
            if (version >= 2) {
                symbolBytes = 14;
            }
            for (int i = 0; i < count; i++) {
                if (buffer.remaining() > symbolBytes) {
                    long pointer = buffer.getLong();
                    short len = buffer.getShort();
                    byte[] symbol = null;
                    if (len > 0) {
                        symbol = new byte[len];
                        buffer.get(symbol);
                    }
                    short symbolKind = 0;
                    if (version >= 2) {
                        symbolKind = buffer.getShort();
                    }
                    if (symbol != null) {
                        symbolMapping.put(pointer, new MethodSymbol(pointer, 0, new String(symbol), symbolKind));
                    }
                } else {
                    break;
                }
            }
            if (buffer.remaining() > 4) {
                int soBuildIdCount = buffer.getInt();
                for (int i = 0; i < soBuildIdCount; i++) {
                    if (buffer.remaining() > 8) {
                        int soNameLen = buffer.getInt();
                        String soName = null;
                        if (soNameLen > 0) {
                            byte[] b = new byte[soNameLen];
                            buffer.get(b);
                            soName = new String(b);
                        }
                        int soBuildIdLen = buffer.getInt();
                        String soBuildId = null;
                        if (soBuildIdLen > 0) {
                            byte[] b = new byte[soBuildIdLen];
                            buffer.get(b);
                            soBuildId = new String(b);
                        }
                        if (soName != null && soBuildId != null) {
                            soBuildIds.put(soName, soBuildId);
                        }
                    }
                }
            }
            try {
                while (buffer.remaining() > 4) {
                    int tid;
                    if (2 <= osType) {
                        tid = buffer.getInt();
                    } else {
                        tid = buffer.getShort();
                    }
                    int len = buffer.get();
                    byte[] name = new byte[len];
                    buffer.get(name);
                    threadNames.put(tid, new String(name).trim());
                }
            } catch (Throwable e) {
                // NegativeArraySizeException/BufferUnderflowException 等线程名称数据异常，非必须数据直接忽略
            }
        } else {
            // buffer.position(0);
            // 避免使用 position 方法，不同 JVM 有兼容性问题
            boolean first = true;
            while (buffer.remaining() > 12) {
                long pointer = first ? maybeMagic : buffer.getLong();
                first = false;
                short len = buffer.getShort();
                if (len > 0) {
                    byte[] b = new byte[len];
                    buffer.get(b);
                    symbolMapping.put(pointer, new MethodSymbol(pointer, 0, new String(b)));
                }
            }
        }
    }

    private void onDecodeFinish() {
        OnDecodeListener listener = onDecodeListener;
        if (listener != null) {
            try {
                listener.onDecodeFinish(this);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
        assignTypes();
    }


    private void assignTypes() {
        for (MethodSymbol symbol : symbolMapping.values()) {
            try {
                if (symbol.raw.length() > 2 && symbol.raw.charAt(0) == '<' && symbol.raw.charAt(symbol.raw.length() - 1) == '>'
                        && symbol.raw.contains("><")) {
                    // this is method in native stack
                    NativeStackElement element = NativeStackElement.parse(symbol.raw);
                    if (element.isJavaMethod()) {
                        String javaSymbol = element.getMethodName();
                        symbol.raw = javaSymbol;
                        symbol.symbol = symbol.raw;
                        if (javaSymbol.equals("art_jni_trampoline") || javaSymbol.equals("art_quick_generic_jni_trampoline")) {
                            symbol.type = MethodSymbol.TYPE_NATIVE_JNI;
                            continue;
                        }
                        if (javaSymbol.contains("[DEDUPED]")) {
                            // this deduped method will be discarded latter
                            symbol.type = MethodSymbol.TYPE_NATIVE_APP_JAVA;
                            continue;
                        }
                        boolean isSystem = element.getModule().startsWith("boot");
                        symbol.type = isSystem ? MethodSymbol.TYPE_NATIVE_SYSTEM_JAVA : MethodSymbol.TYPE_NATIVE_APP_JAVA;
                    } else {
                        symbol.type = MethodSymbol.TYPE_NATIVE_CPP;
                        symbol.symbolRevised = true;
                    }
                } else {
                    symbol.type = MethodSymbol.TYPE_PURE_JAVA;
                    symbol.symbolRevised = true;
                }
            } catch (Throwable e) {
                throw new RuntimeException("bad symbol " + symbol.raw);
            }
        }
    }
}

