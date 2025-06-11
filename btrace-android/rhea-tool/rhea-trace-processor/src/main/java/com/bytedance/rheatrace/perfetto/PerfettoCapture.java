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
package com.bytedance.rheatrace.perfetto;

import com.bytedance.rheatrace.Log;
import com.bytedance.rheatrace.Main;
import com.bytedance.rheatrace.core.Arguments;
import com.bytedance.rheatrace.core.SystemLevelCapture;
import com.bytedance.rheatrace.core.TraceError;
import com.bytedance.rheatrace.core.Workspace;
import com.bytedance.rheatrace.os.OS;
import com.bytedance.rheatrace.trace.SamplingTraceDecoder;

import org.apache.commons.io.IOUtils;
import org.apache.commons.lang3.StringUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import perfetto.protos.FtraceEventBundleOuterClass;
import perfetto.protos.FtraceEventOuterClass;
import perfetto.protos.TraceOuterClass;
import perfetto.protos.TracePacketOuterClass;

public class PerfettoCapture implements SystemLevelCapture {
    private Process process;
    private boolean started = false;

    @Override
    public void start(String[] args) throws IOException {
        File bin = Workspace.perfettoBinary();
        try (InputStream perfetto = Main.class.getResourceAsStream("/" + OS.get().perfettoScriptName())) {
            assert perfetto != null;
            IOUtils.copy(perfetto, Files.newOutputStream(bin.toPath()));
            OS.get().setExecutable(bin);
        }
        Log.d("record_android_trace ready: " + bin.getAbsolutePath());
        List<String> cmd = OS.get().buildCommand(bin.getAbsolutePath(), args);
        enlargeBufferIfDefault(cmd);
        Log.d(StringUtils.join(cmd, " "));
        process = Runtime.getRuntime().exec(cmd.toArray(new String[0]));
        started = true;
    }

    private void enlargeBufferIfDefault(List<String> cmd) {
        boolean explicit = false;
        for (String s : cmd) {
            if ("-b".equals(s)) {
                explicit = true;
                break;
            }
        }
        if (!explicit) {
            cmd.add("-b");
            cmd.add("100mb");
        }
    }

    @Override
    public void stop() {
        stopInternal();
    }

    @Override
    public void print(boolean withError, PrintListener listener) throws IOException {
        IOUtils.lineIterator(process.getInputStream(), Charset.defaultCharset()).forEachRemaining(msg -> {
            if (started) {
                listener.onPrint(msg);
                Log.d(msg);
            }
        });
        IOUtils.lineIterator(process.getErrorStream(), Charset.defaultCharset()).forEachRemaining(msg -> {
            if (started) {
                if (withError) {
                    listener.onPrint(msg);
                }
                Log.e(msg);
            }
        });
    }

    @Override
    public void waitForExit() throws InterruptedException {
        int code = process.waitFor();
        if (code < 0) {
            throw new TraceError("Perfetto return with bad code " + code, "perfetto may not support your device. try with `-mode simple`.");
        }
    }

    @Override
    public void process() throws IOException, InterruptedException {
        if (!Workspace.systemTrace().exists()) {
            throw new TraceError("systrace file not found: " + Workspace.systemTrace(), "your device may not support perfetto. please retry with `-mode simple`.");
        }
        Trace sampleTrace = SamplingTraceDecoder.decode();
        if (sampleTrace == null) {
            throw new TraceError("decode app sample trace failed", null);
        }
        File output = Workspace.output();
        try (FileOutputStream out = new FileOutputStream(output)) {
            Log.red("writing trace:" + output.getAbsolutePath());
            IOUtils.copy(Files.newInputStream(Workspace.systemTrace().toPath()), out);
            sampleTrace.marshal(out);
        }
    }

    @Override
    public void cleanup() {
        stopInternal();
    }

    private void stopInternal() {
        if (!started) {
            return;
        }
        started = false;
        if (!Arguments.get().interactiveTracing) {
            return;
        }
        try {
            Process proc = Runtime.getRuntime().exec("kill -SIGINT " + process.pid());
            proc.waitFor();
        } catch (Exception e) {
            Log.e(e.toString());
        }
    }
}