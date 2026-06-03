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

import static com.bytedance.harmony.trace.cli.OHTraceConvert.decodeHarmonyTrace;

import static java.lang.Math.max;
import static java.lang.Math.min;


import org.apache.commons.io.FileUtils;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.net.URISyntaxException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.zip.GZIPOutputStream;

import picocli.CommandLine;
import picocli.CommandLine.Command;
import picocli.CommandLine.Option;
import sun.misc.Signal;
import sun.misc.SignalHandler;

@Command(name = "ohtrace", version = "1.0", mixinStandardHelpOptions = true)
public class Cli implements Runnable {
    private static boolean sShouldExit = false;

    private final static String sDestDir = "/data/storage/el2/base/cache/ohtraceOffline";

    @Option(names = { "-a", "--all_symbol" }, description = "Show all symbol (include system symbols)")
    boolean allSymbol;

    @Option(names = { "-b", "--buldle_name" }, description = "App bundle name", required = true)
    String bundleName = "";

    @Option(names = { "-o", "--output_path" }, description = "Output path")
    String outputPath = "";

    @Option(names = { "-t", "--time_limit" }, description = "Time limit in second")
    long timeLimit;

    @Option(names = { "-s", "--source_mapping" }, description = "Source Mapping path")
    String sourceMapping = "";

    @Option(names = { "-n", "--name_cache" }, description = "Name Cache path")
    String nameCache = "";

    @Option(names = { "-N", "--native_path" }, description = "Native so path")
    String nativePath = "";

    @Option(names = { "-S", "--buffer_size" }, description = "Max app trace buffer size")
    long bufferSize;

    @Option(names = { "-i", "--sample_interval" }, description = "Sample interval")
    int sampleInterval;

    @Option(names = { "-w", "--wait_timeout" }, description = "Wait timeout")
    int waitTimeout;

    @Option(names = { "-k", "--key" }, description = "Connected device key")
    String deviceKey;

    @Option(names = { "-m", "--main_only" }, description = "Main thread only")
    boolean mainOnly;

    @Option(names = { "-H", "--high_freq" }, description = "High frequency sampling mode")
    boolean highFreq;

    @Option(names = { "-r", "--restart" }, description = "Restart app")
    boolean restart;

    @Option(names = { "-sp", "--skip_perfetto" }, description = "Skip open perfetto UI after export")
    boolean skipPerfetto = false;

    private void check() throws IOException {
        if (bundleName.isEmpty()) {
            throw new TraceError("bundleName is empty", "please provide bundle name");
        }

        if (outputPath.isEmpty()) {
            String userHome = System.getProperty("user.home");
            String desktopPath = Paths.get(userHome, "Desktop").toAbsolutePath().toString();
            outputPath = Paths.get(desktopPath, "ohtrace").toString();
            Files.createDirectories(Path.of(outputPath));
        }

        if (!sourceMapping.isEmpty() && !new File(sourceMapping).exists()) {
            throw new TraceError("source mapping file not exist:" + sourceMapping, "check your file path");
        }

        if (!nameCache.isEmpty() && !new File(nameCache).exists()) {
            throw new TraceError("name cache file not exist:" + nameCache, "check your file path");
        }

        if (!nativePath.isEmpty() && !new File(nativePath).exists()) {
            throw new TraceError("native so file not exist:" + nativePath, "check your mapping file path");
        }

        if (timeLimit == 0) {
            timeLimit = 60;
        }

        timeLimit = max(timeLimit, 1) * 1000;

        if (sampleInterval == 0) {
            sampleInterval = 1;
        }

        sampleInterval = max(sampleInterval, 1);

        long unit = 1;

        if (highFreq) {
            unit = 1000;
            sampleInterval = max(sampleInterval, 10);
        }

        long dur = min(timeLimit, 30 * 1000);

        if (bufferSize == 0) {
            bufferSize = dur * unit / sampleInterval;
        }
        bufferSize = min(bufferSize, 30 * 1000);

        if (waitTimeout == 0) {
            waitTimeout = 60;
        }
    }

    @Override
    public void run() {
        boolean success = true;

        try {
            check();

            Hdc.Init(deviceKey, bundleName);

            String mainAbility = "";
            String mainAbilityRes = Hdc.callString("shell", "bm", "dump", "-n", bundleName,
                                                    "|", "grep", "mainAbility");
            String[] mainAbilityResList = mainAbilityRes.strip().split(":");

            if (mainAbilityResList.length != 2) {
                throw new TraceError("failed to get mainAbility", "");
            } else {
                mainAbility = mainAbilityResList[1].split(",")[0];
            }

            int bufferedTime = 0;
            int asyncInterval = sampleInterval;
            boolean syncMode = false;
            boolean enableOsThread = true;
    
            if (restart) {
                syncMode = true;

                try {
                    Hdc.UploadConfig(bufferSize, sampleInterval, mainOnly, highFreq, asyncInterval,
                            enableOsThread, syncMode, bufferedTime);
                    Log.blue("put config file to device by post api");
                } catch (IOException | InterruptedException e) {
                    Log.blue("put config file to device by hdc");
                    File tempFile = new File(System.getProperty("java.io.tmpdir"), "config.json");
                    tempFile.deleteOnExit();
                    var body = new JSONObject();
                    body.put("buffer_size", bufferSize);
                    body.put("sample_interval", sampleInterval);
                    body.put("main_only", mainOnly);
                    body.put("high_freq", highFreq);
                    body.put("async_interval", asyncInterval);
                    body.put("enable_os_thread", enableOsThread);
                    body.put("sync_mode", syncMode);
                    body.put("buffered_time", bufferedTime);
                    Files.writeString(tempFile.toPath(), body.toString());

                    Hdc.call("file", "send", "-b", bundleName, tempFile.getAbsolutePath(),
                            sDestDir);
                }

                Hdc.call("shell", "\"aa force-stop " + bundleName + "\"");
                Hdc.call("shell", "\"aa start -b " + bundleName + " -a " + mainAbility +
                        "\"");

                Log.i("delay 3s for app launch.");
                Thread.sleep(3000);
            } else {
                Hdc.StopRecordWithRetry("", 10, 3);
                Hdc.StartRecordWithRetry(bufferSize, sampleInterval, mainOnly, highFreq, asyncInterval,
                        enableOsThread, syncMode, bufferedTime, 3);
            }

            Log.blue("start tracing...");
    
            Signal.handle(new Signal("INT"), signal -> {
                sShouldExit = true;
                Signal.handle(signal, SignalHandler.SIG_DFL);
            });
    
            Log.i("press ctrl+c to stop");
    
            long sleepTimeMs = 200;
            long currTimeMs = System.currentTimeMillis();
            long recordTimeMs = 0;
            long startTimeMs = currTimeMs;
            long prevPrintTimeMs = startTimeMs;
    
            while (!sShouldExit && recordTimeMs < timeLimit) {
                Thread.sleep(sleepTimeMs);

                currTimeMs = System.currentTimeMillis();
                recordTimeMs = currTimeMs - startTimeMs;
    
                if (1000 < currTimeMs - prevPrintTimeMs) {
                    prevPrintTimeMs = currTimeMs;
                    System.out.printf("\r>>> record %.2fs",((double)recordTimeMs)/1000);
                }
            }

            System.out.print("\n");
    
            Log.blue("stop tracing...");
    
            String appOutputDir = Paths.get(outputPath, bundleName).toString();
            Files.createDirectories(Path.of(appOutputDir));
            String traceFilePath = Hdc.StopRecordWithRetry(appOutputDir, waitTimeout, 3);
    
            if (!traceFilePath.isEmpty()) {
                long recordTimeNs = recordTimeMs * 1000 * 1000;

                if (restart) {
                    recordTimeNs = 1800L * 1000 * 1000 * 1000;
                }

                byte[] bytes = parseTraceFile(traceFilePath, recordTimeNs);
                String tracePbPath = traceFilePath + ".pb.gz";
                File outFile = new File(tracePbPath);
                FileUtils.writeByteArrayToFile(outFile, bytes);
                if (!skipPerfetto) {
                    new TraceOpenHelper(outFile).openInBrowser();
                } else {
                    Log.i("skip open perfetto, trace file: " + outFile.getAbsolutePath());
                }
            } else {
                throw new TraceError("empty trace", "failed to fetch trace data");
            }
        } catch (IOException | InterruptedException | URISyntaxException e) {
            success = false;
            throw new RuntimeException(e);
        } finally {
            Statistics.safeSend(bundleName, success);
            Hdc.Http.RmPortForward();
        }
    }

    public byte[] parseTraceFile(String traceFilePath, long recordTimeNs) throws IOException {
        byte[] bytes = null;
        File file = new File(traceFilePath);
        byte[] data = FileUtils.readFileToByteArray(file);
        SamplingFile sf = decodeHarmonyTrace(data);

        if (sf != null && sf.valid()) {
            var listener = new Symbol(sourceMapping, nameCache, nativePath, allSymbol);
            SamplingMappingDecoder.setOnDecodeListener(listener);
            StackListParser.setOnParserListener(listener);
            StackListParser parser = new StackListParser(StackParseFactory.JAVA_AND_NATIVE);
            parser.parse(sf).revise().trimEmpty().retainTheLast(recordTimeNs).trimFalseStart();

            try (ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                new StackListEncoder(parser).encodeAsPb(out);
                bytes = out.toByteArray();

                try (ByteArrayOutputStream bos = new ByteArrayOutputStream();
                     GZIPOutputStream gzipOut = new GZIPOutputStream(bos)) {
                    gzipOut.write(bytes);
                    gzipOut.finish();
                    bytes = bos.toByteArray();
                }
            }
        } else {
            System.err.println("parse trace file failed: " + file.getName());
        }

        return bytes;
    }
}
