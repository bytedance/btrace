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

import java.util.Map;

public class SamplingFile {
    public final byte[] sampling;
    @Deprecated
    public final byte[] mapping;
    public byte[] mappingJava;
    public byte[] mappingNative;
    public byte[] mappingThread;

    public SamplingFile(Map<String, byte[]> files) {
        this.sampling = files.get("sampling");
        this.mapping = files.get("sampling-mapping");
        this.mappingJava = files.get("mapping-java");
        this.mappingNative = files.get("mapping-native");
        this.mappingThread = files.get("mapping-thread");
    }

    public boolean valid() {
        return sampling != null && (mapping != null || mappingJava != null);
    }
}
