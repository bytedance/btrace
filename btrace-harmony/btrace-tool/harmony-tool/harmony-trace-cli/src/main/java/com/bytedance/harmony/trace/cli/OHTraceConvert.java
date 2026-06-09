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

import org.apache.commons.io.FileUtils;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;

public class OHTraceConvert {

    private static String getSoNameFromPath(String soPath) {
        int splitPos = soPath.lastIndexOf('/');
        if (splitPos < 0) {
            return soPath;
        }
        return soPath.substring(splitPos + 1);
    }

    private static String methodSignatureStripSoPath(String methodDesc) {
        int pos = methodDesc.indexOf('>');
        if (pos <= 0) {
            return methodDesc;
        }
        String fullSoPath = methodDesc.substring(1, pos);
        int splitPos = fullSoPath.lastIndexOf('/');
        if (splitPos < 0) {
            return methodDesc;
        }
        return '<' + fullSoPath.substring(splitPos + 1) + '>' + methodDesc.substring(pos + 1);
    }

    static String clipJsMethodDesc(String methodDesc) {
        if (methodDesc == null) {
            return null;
        }
        if (methodDesc.indexOf('<') < 0) {
            return methodDesc;
        }
        int splitPos;
        String functionName;
        if (methodDesc.charAt(0) == '<') {
            splitPos = methodDesc.indexOf('<', 1);
            functionName = methodDesc.substring(1, splitPos - 1);
        } else {
            splitPos = methodDesc.indexOf('<');
            functionName = methodDesc.substring(0, splitPos);
        }

        String moduleName = "";
        int moduleAtPos = methodDesc.indexOf('@', splitPos);
        if (moduleAtPos != -1) {
            int moduleEndPos = methodDesc.indexOf('|', moduleAtPos);
            if (moduleEndPos != -1) {
                moduleName = methodDesc.substring(moduleAtPos, moduleEndPos);
            }
        }
        String fileName = "";
        int realFileStartPos = Math.max(methodDesc.lastIndexOf('/'), methodDesc.lastIndexOf('|'));
        if (realFileStartPos != -1) {
            int fileEndPos = methodDesc.lastIndexOf('.');
            if (fileEndPos != -1) {
                fileName = methodDesc.substring(realFileStartPos + 1, fileEndPos);
            }
        }
        if (!fileName.isEmpty()) {
            if (!moduleName.isEmpty()) {
                return fileName + ":" + functionName + " " + moduleName;
            } else {
                return fileName + ":" + functionName;
            }
        } else {
            return functionName;
        }
    }

    private static byte[] convertHarmonyMappingBytes(byte[] bytes) {
        if (bytes == null) {
            return null;
        }
        try {
            ByteBuffer reader = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
            long magic = reader.getLong();
            int version = reader.getInt();
            HashMap<Long, byte[]> jsMappings = new HashMap<>();
            HashMap<Long, byte[]> nativeMappings = new HashMap<>();
            int bufferContentLen = 16; // magic + version +
            int length = reader.getInt();
            while (length > 0 && length != 0x01010101 && reader.remaining() >= length) {
                byte[] tmp = new byte[length];
                reader.get(tmp);
                String content = new String(tmp);
                String[] lines = content.split("\n");
                for (String line : lines) {
                    boolean valid = line.startsWith("N:") || line.startsWith("J:");
                    if (valid) {
                        try {
                            int pcEndPos = line.indexOf(':', 2);
                            long pc = Long.parseLong(line.substring(2, pcEndPos));
                            String methodDesc = line.substring(pcEndPos + 1);
                            byte[] stringBytes;
                            if (line.charAt(0) == 'N') {
//                                methodDesc = methodSignatureStripSoPath(methodDesc);
                                stringBytes = methodDesc.getBytes();
                                nativeMappings.put(pc, stringBytes);
                            } else {
                                stringBytes = methodDesc.getBytes();
                                jsMappings.put(pc, stringBytes);
                            }
                            bufferContentLen += stringBytes.length + 12;
                        } catch (Exception e) {
                            System.err.println("parse mapping line error: " + line);
                        }
                    }
                }
                length = reader.getInt();
            }
            HashMap<byte[], byte[]> soBuildIds = new HashMap<>();
            bufferContentLen += 4;
            if (length == 0x01010101) {
                int buildIdContentLen = reader.getInt();
                if (buildIdContentLen > 0) {
                    byte[] tmp = new byte[buildIdContentLen];
                    reader.get(tmp);
                    String content = new String(tmp);
                    String[] lines = content.split("\n");
                    for (String line : lines) {
                        int pos = line.indexOf(':');
                        if (pos > 0) {
                            String soName = getSoNameFromPath(line.substring(0, pos));
                            byte[] soNameByes = soName.getBytes();
                            String buildId = line.substring(pos + 1);
                            byte[] buildIdBytes = buildId.getBytes();
                            soBuildIds.put(soNameByes, buildIdBytes);
                            bufferContentLen += soNameByes.length + buildIdBytes.length + 8;
                        } else {
                            System.err.println("parse build id line error: " + line);
                        }
                    }
                }
            }
            HashMap<Integer, byte[]> threadNames = new HashMap<>();
            try {
                int threadNameLength = reader.getInt();
                byte[] tmp = new byte[threadNameLength];
                reader.get(tmp);
                String content = new String(tmp);
                String[] lines = content.split("\n");
                for (String line : lines) {
                    int pos = line.indexOf(':');
                    if (pos > 0) {
                        int tid = Integer.parseInt(line.substring(0, pos));
                        String threadName = line.substring(pos + 1);
                        byte[] threadNameBytes = threadName.getBytes();
                        threadNames.put(tid, threadNameBytes);
                        bufferContentLen += Integer.BYTES + threadNameBytes.length + 1;
                    } else {
                        System.err.println("parse thread name line error: " + line);
                    }
                }
            } catch (Throwable e) {
                // NegativeArraySizeException/BufferUnderflowException 等线程名称数据异常，非必须数据直接忽略
            }
            byte[] outBytes = new byte[bufferContentLen];
            ByteBuffer out = ByteBuffer.wrap(outBytes).order(ByteOrder.LITTLE_ENDIAN);
            out.putLong(0);//magic
            out.putInt(2); // version
            out.putInt(jsMappings.size() + nativeMappings.size()); // count
            for (Map.Entry<Long, byte[]> entry : jsMappings.entrySet()) {
                out.putLong(entry.getKey());
                out.putShort((short) entry.getValue().length);
                out.put(entry.getValue());
                out.putShort((short) 3);
            }
            for (Map.Entry<Long, byte[]> entry : nativeMappings.entrySet()) {
                out.putLong(entry.getKey());
                out.putShort((short) entry.getValue().length);
                out.put(entry.getValue());
                out.putShort((short) 4);
            }
            if (!soBuildIds.isEmpty()) {
                out.putInt(soBuildIds.size());
                for (Map.Entry<byte[], byte[]> entry : soBuildIds.entrySet()) {
                    out.putInt(entry.getKey().length);
                    out.put(entry.getKey());
                    out.putInt(entry.getValue().length);
                    out.put(entry.getValue());
                }

                if (!threadNames.isEmpty()) {
                    for (Map.Entry<Integer, byte[]> entry : threadNames.entrySet()) {
                        out.putInt(entry.getKey());
                        out.put((byte) entry.getValue().length);
                        out.put(entry.getValue());
                    }
                }
            } else {
                out.putInt(0);
            }
            return outBytes;
        } catch (Exception e) {
            return null;
        }
    }

    static class OH_RawStackElement {
        int type;
        int tid;
        long wallTime;
        long cpuTime;
        int depth;
        long[] pcs;
        int outSize() {
            return 94 + 8 * depth;
        }
    }

    private static byte[] convertHarmonyTraceBytes(List<byte[]> bytes) {
        if (bytes == null || bytes.isEmpty()) {
            return null;
        }
        String extra = null;
        int size = 0;
        ArrayList<OH_RawStackElement> ses = new ArrayList<>();
        HashSet<Integer> threads = new HashSet<>();
        int pid = 0;
        for (byte[] singleBytes : bytes) {
            ByteBuffer reader = ByteBuffer.wrap(singleBytes).order(ByteOrder.LITTLE_ENDIAN);
            long magic = reader.getLong();
            if (magic == 0) { // read header
                reader.getInt(); // type
                reader.getInt(); // OS
                reader.getInt(); // version
                int extraLen = reader.getInt();
                if (extraLen > 0) {
                    byte[] extraBytes = new byte[extraLen];
                    reader.get(extraBytes);
                    extra = new String(extraBytes);
                    JSONObject extraJson = new JSONObject(extra);
                    pid = extraJson.optInt("processId");
                } else {
                    extra = "";
                }
            }
            while (reader.remaining() >= 28) {
                OH_RawStackElement se = new OH_RawStackElement();
                se.type = reader.getInt() + 16384;
                se.tid = reader.getInt();
                threads.add(se.tid);
                se.wallTime = reader.getLong();
                se.cpuTime = reader.getLong();
                se.depth = reader.getInt();
                se.pcs = new long[se.depth];
                for (int i = 0; i < se.depth; ++i) {
                    se.pcs[i] = reader.getLong();
                }
                ses.add(se);
                size += se.outSize();
            }
        }
        int headerSize = 24;
        int extraSize = 4;
        if (extra != null) {
            extraSize += extra.length();
        }
        if (extra != null) {
            if (!threads.contains(pid)) {
                System.err.println("main thread trace not found");
                return null;
            }
        }
        int totalSize = headerSize + extraSize + size;
        byte[] outBytes = new byte[totalSize];
        ByteBuffer out = ByteBuffer.wrap(outBytes).order(ByteOrder.LITTLE_ENDIAN);
        out.put((byte) 'J'); // magic-1
        out.put((byte) 'V'); // magic-2
        out.put((byte) 'S'); // magic-3
        out.put((byte) 'X'); // magic-4
        out.putShort((short) 7); // type
        out.putShort((short) 4); // OS type
        out.putInt(7);
        out.putLong(0);
        out.putInt(ses.size());
        if (extra == null) {
            out.putInt(0);
        } else{
            byte[] extraBytes = extra.getBytes();
            out.putInt(extraBytes.length);
            out.put(extraBytes);
        }
        for (OH_RawStackElement se : ses) {
            out.putShort((short) se.type);
            out.putInt(se.tid);
            out.putInt(0);
            out.putLong(se.wallTime);
            out.putLong(0);
            out.putLong(se.cpuTime);
            out.putLong(0);
            out.putLong(0);
            out.putLong(0);
            out.putLong(0);
            out.putLong(0);
            out.putInt(0);
            out.putInt(0);
            out.putInt(0);
            out.putShort((short) 0);
            out.putShort((short) 1);
            out.putShort((short) se.depth);
            out.putShort((short) se.depth);
            for (int i = 0; i < se.depth; ++i) {
                out.putLong(se.pcs[i]);
            }
        }
        return outBytes;
    }

    static SamplingFile decodeHarmonyTrace(byte[] bytes) {
        try {
            Map<String, byte[]> unzip = ZipDecoder.unzip(bytes);
            Map<String, byte[]> convertedUnzip = new HashMap<>();
            List<byte[]> traceBytes = new ArrayList<>();
            byte[] mappingBytes = null;
            byte[] convertedMappingBytes = null;
            for (Map.Entry<String, byte[]> entry : unzip.entrySet()) {
                String name = entry.getKey();
                if (name.startsWith("trace-") && name.endsWith(".bin")) {
                    traceBytes.add(entry.getValue());
                } else if ("mapping.bin".equals(name)) {
                    mappingBytes = entry.getValue();
                } else if ("sampling-mapping".equals(name)) {
                    convertedMappingBytes = entry.getValue();
                } else {
                    convertedUnzip.put(entry.getKey(), entry.getValue());
                }
            }
            if (convertedMappingBytes == null) {
                convertedMappingBytes = convertHarmonyMappingBytes(mappingBytes);
                if (convertedMappingBytes == null) {
                    return null;
                }
            }
            byte[] convertedTraceBytes = convertHarmonyTraceBytes(traceBytes);
            if (convertedTraceBytes == null) {
                return null;
            }
            convertedUnzip.put("sampling", convertedTraceBytes);
            convertedUnzip.put("sampling-mapping", convertedMappingBytes);
            return new SamplingFile(convertedUnzip);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
