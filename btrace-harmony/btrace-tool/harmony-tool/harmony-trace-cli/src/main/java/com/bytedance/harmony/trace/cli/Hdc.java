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

import com.bytedance.harmony.trace.cli.os.OS;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.StringWriter;
import java.io.Writer;
import java.net.URL;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;
import java.util.UUID;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.json.JSONObject;

import org.apache.commons.io.IOUtils;
import org.apache.commons.lang3.StringUtils;

public class Hdc {

    private static int sPort = 0;
    private static String sTarget;
    private static String sBundleName;

    public static void Init(String targetKey, String bundleName) throws IOException, InterruptedException {
        sBundleName = bundleName;

        String devices = callString("list", "targets").trim();
        var deviceList = devices.split("\n");
        int deviceCount = deviceList.length;

        if (0 < deviceCount) {
            if (targetKey == null && deviceCount == 1) {
                String target = deviceList[0];

                if (!target.equals("[Empty]")) {
                    sTarget = target;
                } else {
                    throw new TraceError("Could not find any connected devices.", null);
                }
            } else if (targetKey == null) {
                throw new TraceError("You have " + deviceCount + " devices connected. " + devices,
                        "Please select your device with -k $target");
            } else {
                for (String device : deviceList) {
                    if (Objects.equals(device, targetKey)) {
                        sTarget = targetKey;
                        break;
                    }
                }

                if (sTarget == null) {
                    throw new TraceError("Could not find provided device key: " + targetKey,
                            "Connected devices: " + devices);
                }
            }
        } else {
            throw new TraceError("Could not find any connected devices.", null);
        }
    }

    private static class HdcPathResolver {
        private static String sHdcPath;
        private static String sHdcDir;

        static {
            OS os = OS.get();
            String path = System.getenv(os.pathKeyName());
            String[] paths = path.split(os.pathSeparator());

            for (String p : paths) {
                File file = new File(p, os.hdcExecName());

                if (file.exists()) {
                    sHdcPath = file.getAbsolutePath();
                    sHdcDir = p;
                    Log.d("Got hdc path: " + sHdcPath);
                    break;
                }
            }

            if (sHdcPath == null) {
                throw new TraceError("hdc not found in PATH",
                        "check you have export PATH with `$DIR/Android/sdk/platform-tools`.");
            }
        }
    }

    public static String getHdcDirPath() {
        return HdcPathResolver.sHdcDir;
    }

    public static boolean StartRecordWithRetry(long bufferSize, int sampleInterval, boolean mainOnly,
                                              boolean highFreq, int asyncInterval, boolean enableOsThread,
                                              boolean syncMode, int bufferedTime, int retry)
                                              throws IOException, InterruptedException {
        boolean res = false;

        for (int i=0;i<retry+1;++i) {
            try {
                res = StartRecord(bufferSize, sampleInterval, mainOnly, highFreq, asyncInterval,
                                  enableOsThread, syncMode, bufferedTime);
            } catch (IOException e) {
                if (i == retry) {
                    throw e;
                }
                continue;
            }

            break;
        }

        return res;
    }

    public static boolean StartRecord(long bufferSize, int sampleInterval, boolean mainOnly,
                                      boolean highFreq, int asyncInterval, boolean enableOsThread,
                                      boolean syncMode, int bufferedTime)
            throws IOException, InterruptedException {
        var body = new JSONObject();
        body.put("buffer_size", bufferSize);
        body.put("sample_interval", sampleInterval);
        body.put("main_only", mainOnly);
        body.put("high_freq", highFreq);
        body.put("async_interval", asyncInterval);
        body.put("enable_os_thread", enableOsThread);
        body.put("sync_mode", syncMode);
        body.put("buffered_time", bufferedTime);

        var resp = Http.post("/record/start", body, 10);

        return resp.code == 200;
    }

    public static String StopRecordWithRetry(String destDir, long timeout, int retry) throws IOException, InterruptedException {
        String resultFilePath = "";

        for (int i=0;i<retry+1;++i) {
            try {
                resultFilePath = StopRecord(destDir, timeout);
            } catch (IOException e) {
                if (i == retry) {
                    throw e;
                }
                continue;
            }

            break;
        }

        return resultFilePath;
    }

    public static String StopRecord(String destDir, long timeout) throws IOException, InterruptedException {
        String resultFilePath = "";
        var resp = Http.post("/record/stop", new JSONObject("{}"), timeout);

        if (!destDir.isEmpty()) {
            if (resp.code == 500) {
                String msg = resp.body.getString("msg");
                Log.red("Resp error msg: " + msg);
                return resultFilePath;
            } else if (resp.code != 200) {
                Log.red("Resp error, code: " + resp.code);
                return resultFilePath;
            }

            resultFilePath = Paths.get(destDir, "trace_" + System.currentTimeMillis()).toString();

            var respBody = resp.body;
            Iterator<String> keys = respBody.keys();

            FileOutputStream fos = new FileOutputStream(resultFilePath);
            ZipOutputStream zos = new ZipOutputStream(fos);

            while (keys.hasNext()) {
                String fileName = keys.next();
                String value = respBody.getString(fileName);

                Base64.Decoder decoder = Base64.getDecoder();
                byte[] decodedBytes = decoder.decode(value);

                ZipEntry zipEntry = new ZipEntry(fileName);
                zos.putNextEntry(zipEntry);
                zos.write(decodedBytes, 0, decodedBytes.length);
                zos.closeEntry();
            }
        }

        return resultFilePath;
    }

    public static boolean UploadConfig(long bufferSize, int sampleInterval, boolean mainOnly,
                                       boolean highFreq, int asyncInterval, boolean enableOsThread,
                                       boolean syncMode, int bufferedTime)
            throws IOException, InterruptedException {
        var body = new JSONObject();
        body.put("buffer_size", bufferSize);
        body.put("sample_interval", sampleInterval);
        body.put("main_only", mainOnly);
        body.put("high_freq", highFreq);
        body.put("async_interval", asyncInterval);
        body.put("enable_os_thread", enableOsThread);
        body.put("sync_mode", syncMode);
        body.put("buffered_time", bufferedTime);

        var resp = Http.post("/config/upload", body, 10);

        return resp.code == 200;
    }

    public static String callString(String... cmd) throws IOException, InterruptedException {
        StringWriter writer = new StringWriter();
        call(writer, cmd);
        return writer.toString();
    }

    public static void call(String... cmd) throws IOException, InterruptedException {
        call(null, cmd);
    }

    public static void callSafe(String... cmd) {
        try {
            call(null, cmd);
        } catch (Throwable ignore) {
        }
    }

    public static boolean canRunAsSu() {
        try {
            return callWithRet(null, "shell", "su", "-c", "ps") == 0;
        } catch (Exception e) {
            return false;
        }
    }

    private static void call(Writer writer, String... cmds) throws IOException,
                                                            InterruptedException {
        List<String> errors = new ArrayList<>();
        String[] cmd;

        if (sTarget != null) {
            cmd = new String[2 + cmds.length];
            cmd[0] = "-t";
            cmd[1] = sTarget;
            System.arraycopy(cmds, 0, cmd, 2, cmds.length);
        } else {
            cmd = cmds;
        }

        Log.d("Run hdc " + StringUtils.join(cmd, " "));

        String[] array = new String[cmd.length + 1];
        array[0] = HdcPathResolver.sHdcPath;
        System.arraycopy(cmd, 0, array, 1, cmd.length);
        String cmdStr = String.join(" ", array);
        Process process = Runtime.getRuntime().exec(cmdStr);

        for (String line : IOUtils.readLines(process.getInputStream(), StandardCharsets.UTF_8)) {
            if (writer != null) {
                writer.write(line);
                writer.write('\n');
            } else {
                Log.d(line);
            }
        }

        for (String error : IOUtils.readLines(process.getErrorStream(), StandardCharsets.UTF_8)) {
            if (writer != null) {
                writer.write(error);
                writer.write('\n');
            } else {
                Log.d(error);
                errors.add(error);
            }
        }

        int code = process.waitFor();

        if (code != 0) {
            throw new TraceError("hdc " + String.join(" ", cmd) + " return " + code +"." +
                    String.join(". ", errors), "have you connect your device via usb.");
        }
    }

    private static int callWithRet(Writer writer, String... cmds) throws IOException, InterruptedException {
        String[] cmd;
        if (sTarget != null) {
            cmd = new String[2 + cmds.length];
            cmd[0] = "-s";
            cmd[1] = sTarget;
            System.arraycopy(cmds, 0, cmd, 2, cmds.length);
        } else {
            cmd = cmds;
        }
        Log.d("Run hdc " + StringUtils.join(cmd, " "));
        String[] array = new String[cmd.length + 1];
        array[0] = HdcPathResolver.sHdcPath;
        System.arraycopy(cmd, 0, array, 1, cmd.length);
        Process process = Runtime.getRuntime().exec(array);
        for (String line : IOUtils.readLines(process.getInputStream(), StandardCharsets.UTF_8)) {
            if (writer != null) {
                writer.write(line);
                writer.write('\n');
            } else {
                Log.d(line);
            }
        }
        for (String error : IOUtils.readLines(process.getErrorStream(), StandardCharsets.UTF_8)) {
            if (writer != null) {
                writer.write(error);
                writer.write('\n');
            }
        }
        return process.waitFor();
    }

    public static class HttpRespValue {
        public int code;
        public JSONObject body;


        HttpRespValue(int statusCode, JSONObject respBody) {
            code = statusCode;
            body = respBody;
        }
    }


    public static class Http {
        static String sConnKey = null;

        static {
            try {
                if (sBundleName != null) {
                    sPort = calcPortNumber(sBundleName);
                    sConnKey = "tcp:" + sPort + " " + "tcp:" + sPort;
                    Hdc.call("fport", sConnKey);
                }
            } catch (IOException | InterruptedException e) {
                throw new RuntimeException(e);
            }
        }

        private static int calcPortNumber(String bundleName) {
            int basePort = 11288;
            int asciiSum = 0;

            for (int i = 0; i < bundleName.length(); i++) {
                char ch = bundleName.charAt(i);
                asciiSum += ch;
            }

            int portRange = 65536 - basePort;
            return (asciiSum % portRange) + basePort;
        }

        public static void RmPortForward() {
            if (sConnKey == null) {
                return;
            }

            try {
                Hdc.call("fport", "rm", sConnKey);
            } catch (IOException | InterruptedException e) {
                throw new RuntimeException(e);
            }
        }

        public static HttpRespValue get(String path) throws IOException, InterruptedException {
            HttpClient client = HttpClient.newHttpClient();
            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create("http://localhost:" + sPort + path))
                    .GET()
                    .build();
            var resp = client.send(request, HttpResponse.BodyHandlers.ofString());

            int code = resp.statusCode();
            var respBody = new JSONObject(resp.body());
            return new HttpRespValue(code, respBody);
        }

        public static HttpRespValue post(String path, JSONObject reqBody, long timeout) throws IOException,
                                                                    InterruptedException {
            HttpClient client = HttpClient.newHttpClient();
            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create("http://localhost:" + sPort + path))
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(reqBody.toString()))
                    .timeout(Duration.ofSeconds(timeout))
                    .build();

            var resp = client.send(request, HttpResponse.BodyHandlers.ofString());

            int code = resp.statusCode();
            var respBody = new JSONObject(resp.body());
            return new HttpRespValue(code, respBody);
        }

        private static void checkAppStarted() {
//            try {
//                Hdc.call("shell", "pidof", Arguments.get().appName);
//            } catch (Throwable e) {
//                // app is crash
//                Log.e("App is not running. Make sure your app has started and does not crashed.");
//            }
        }
    }
}
