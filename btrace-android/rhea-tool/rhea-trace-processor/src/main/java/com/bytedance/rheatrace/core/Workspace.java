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
package com.bytedance.rheatrace.core;

import com.bytedance.rheatrace.Log;
import com.bytedance.rheatrace.os.OS;

import org.apache.commons.io.FileUtils;

import java.io.File;
import java.io.IOException;

public class Workspace {
    private static File sRoot;
    private static File sOutput;

    public static File root() {
        if (sRoot == null) {
            throw new TraceError("workspace not inited", null);
        }
        return sRoot;
    }

    public static void init(File root) {
        if (sRoot != null) {
            throw new TraceError("workspace already initialized with root " + sRoot, null);
        }
        sRoot = root;
        clear();
    }

    public static void clear() {
        try {
            File workspace = Workspace.root();
            if (!workspace.exists() && !workspace.mkdirs()) {
                throw new TraceError("can not create dir " + workspace, "you can retry or create the directory by your self:" + workspace);
            }
            FileUtils.cleanDirectory(workspace);
            Log.i("workspace clear: " + workspace.getAbsolutePath());
        } catch (IOException e) {
            throw new TraceError(e.getMessage(), "you can retry or create the directory by your self:" + Workspace.root());
        }
    }

    public static File systemTrace() {
        return new File(root(), "systemTrace.trace");
    }

    public static File samplingTrace() {
        return new File(root(), "sampling.bin");
    }

    public static File samplingMapping() {
        return new File(root(), "sampling-mapping.bin");
    }

    public static File perfettoBinary() {
        return new File(root(), OS.get().perfettoScriptName());
    }

    public static File output() {
        if (sOutput == null) {
            sOutput = new File(Arguments.get().outputPath);
        }
        return sOutput;
    }
}