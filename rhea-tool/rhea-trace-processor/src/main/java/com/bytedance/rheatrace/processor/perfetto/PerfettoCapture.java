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
package com.bytedance.rheatrace.processor.perfetto;

import com.bytedance.rheatrace.processor.Log;
import com.bytedance.rheatrace.processor.core.SystemLevelCapture;
import com.bytedance.rheatrace.processor.core.TraceError;
import com.bytedance.rheatrace.processor.core.Workspace;
import com.bytedance.rheatrace.processor.os.OS;

import org.apache.commons.io.IOUtils;
import org.apache.commons.lang3.StringUtils;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.util.List;

/**
 * Created by caodongping on 2023/2/15
 *
 * @author caodongping@bytedance.com
 */
public class PerfettoCapture implements SystemLevelCapture {
    private Process process;

    @Override
    public void start(String[] args) throws IOException {
        File bin = Workspace.perfettoBinary();
        try (InputStream perfetto = TraceProcessor.class.getResourceAsStream("/" + OS.get().perfettoScriptName())) {
            assert perfetto != null;
            IOUtils.copy(perfetto, Files.newOutputStream(bin.toPath()));
            OS.get().setExecutable(bin);
        }
        Log.d("record_android_trace ready: " + bin.getAbsolutePath());
        List<String> cmd = OS.get().buildCommand(bin.getAbsolutePath(), args);
        enlargeBufferIfDefault(cmd);
        Log.d(StringUtils.join(cmd, " "));
        this.process = Runtime.getRuntime().exec(cmd.toArray(new String[0]));
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
    public void print(boolean withError, PrintListener listener) throws IOException {
        IOUtils.lineIterator(process.getInputStream(), Charset.defaultCharset()).forEachRemaining(msg -> {
            listener.onPrint(msg);
            Log.d(msg);
        });
        IOUtils.lineIterator(process.getErrorStream(), Charset.defaultCharset()).forEachRemaining(msg -> {
            if (withError) {
                listener.onPrint(msg);
            }
            Log.e(msg);
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
        new TraceProcessor().generate();
    }
}
