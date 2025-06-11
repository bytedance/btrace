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
package com.bytedance.rheatrace.lite;

import com.bytedance.rheatrace.Log;
import com.bytedance.rheatrace.core.SystemLevelCapture;
import com.bytedance.rheatrace.core.TraceError;
import com.bytedance.rheatrace.core.Workspace;
import com.bytedance.rheatrace.perfetto.Trace;
import com.bytedance.rheatrace.trace.SamplingTraceDecoder;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.locks.LockSupport;

public class LiteCapture implements SystemLevelCapture {
    private Thread thread;

    @Override
    public void start(String[] sysArgs) throws IOException {
        int seconds = -1;
        for (int i = 0; i < sysArgs.length; i++) {
            if (sysArgs[i].equals("-t")) {
                seconds = Integer.parseInt(sysArgs[i + 1].substring(0, sysArgs[i + 1].length() - 1)) * 1000;
                break;
            }
        }
        int finalSeconds = seconds;
        thread = new Thread(() -> {
            LockSupport.parkNanos(finalSeconds * 1000_000L);
        });
        thread.start();
    }

    @Override
    public void stop() {

    }

    @Override
    public void print(boolean withError, PrintListener listener) throws IOException {
    }

    @Override
    public void waitForExit() throws InterruptedException {
        thread.join();
    }

    @Override
    public void process() throws IOException {
        Trace sampleTrace = SamplingTraceDecoder.decode();
        if (sampleTrace == null) {
            throw new TraceError("decode app sample trace failed", null);
        }
        File output = Workspace.output();
        try (FileOutputStream out = new FileOutputStream(output)) {
            Log.red("writing trace:" + output.getAbsolutePath());
            sampleTrace.marshal(out);
        }
    }

    @Override
    public void cleanup() {

    }
}