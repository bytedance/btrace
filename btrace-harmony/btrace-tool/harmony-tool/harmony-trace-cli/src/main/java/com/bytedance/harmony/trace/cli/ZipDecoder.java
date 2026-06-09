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

import org.apache.commons.io.IOUtils;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.util.Base64;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class ZipDecoder {

    private static byte[] openStream(File dir, String name) throws IOException {
        return IOUtils.toByteArray(Files.newInputStream(new File(dir, name).toPath()));
    }

    public static Map<String, byte[]> unzip(byte[] data) throws IOException {
        if (isZipFile(data)) {
            return handleZip(data);
        } else {
            return handleNonZip(data);
        }
    }

    private static boolean isZipFile(byte[] data) {
        if (data == null || data.length < 4) {
            return false;
        }
        return data[0] == 0x50 && data[1] == 0x4B && data[2] == 0x03 && data[3] == 0x04;
    }

    private static Map<String, byte[]> handleZip(byte[] zip) throws IOException {
        byte[] buffer = new byte[4096];
        Map<String, byte[]> map = new HashMap<>();
        try (ZipInputStream zis = new ZipInputStream(new ByteArrayInputStream(zip))) {
            ZipEntry entry;
            while ((entry = zis.getNextEntry()) != null) {
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                int len;
                while ((len = zis.read(buffer)) > 0) {
                    baos.write(buffer, 0, len);
                }
                map.put(entry.getName(), baos.toByteArray());
            }
        }
        return map;
    }

    private static Map<String, byte[]> handleNonZip(byte[] data) {
        Map<String, byte[]> result = new HashMap<>();
        ByteBuffer buffer = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
        while (buffer.hasRemaining()) {
            int keyLength = buffer.getInt();
            if (keyLength <= 0) {
                break;
            }
            byte[] keyBytes = new byte[keyLength];
            buffer.get(keyBytes);
            String key = new String(keyBytes);
            int valueLength = buffer.getInt();
            if (valueLength < 0) {
                break;
            }
            byte[] valueBytes = new byte[valueLength];
            buffer.get(valueBytes);
            result.put(key, valueBytes);
        }
        return result;
    }

    public static String parseExtra(byte[] sampling) {
        ByteBuffer buffer = ByteBuffer.wrap(sampling).order(ByteOrder.LITTLE_ENDIAN);
        int magic = buffer.getInt();
        int type = buffer.getInt();
        int version = buffer.getInt();
        long time = buffer.getLong();
        int count = buffer.getInt();
        int extraLength = buffer.getInt();
        if (extraLength > 0) {
            byte[] extra = new byte[extraLength];
            buffer.get(extra);
            return new String(extra);
        } else {
            return "";
        }
    }
}
