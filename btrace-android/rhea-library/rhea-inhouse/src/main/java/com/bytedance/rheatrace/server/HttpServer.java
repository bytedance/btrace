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
package com.bytedance.rheatrace.server;

import android.util.Log;

import com.bytedance.rheatrace.TraceManager;
import com.bytedance.rheatrace.prop.TraceProperties;
import com.bytedance.rheatrace.trace.TraceConfigurations;
import com.bytedance.rheatrace.trace.base.TraceMeta;
import com.bytedance.rheatrace.trace.sampling.SamplingConfig;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.List;

import fi.iki.elonen.NanoHTTPD;

public final class HttpServer {

    private static final String TAG = "RheaServer";
    private static Server server = null;

    public static Server getServer() {
        return server;
    }

    public static void start(File externalFileDir, String hostDirPath) {
        int waitTraceTimeoutSeconds = TraceProperties.getWaitTraceTimeoutSeconds();
        if (server != null && server.isAlive()) {
            Log.i(TAG, "stop previous server on port " + server.getListeningPort());
            server.stop();
        }
        server = new Server(new File(hostDirPath), waitTraceTimeoutSeconds);
        try {
            server.start();
            int port = server.getListeningPort();
            makePortDir(externalFileDir, port);
            Log.i(TAG, "start new http server on port " + port);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private static void makePortDir(File externalFileDir, int port) {
        File portDir = new File(externalFileDir, "rhea-port");
        if (!portDir.exists()) {
            portDir.mkdirs();
        }
        File[] ports = portDir.listFiles();
        if (ports == null || ports.length == 0) {
            File realPortDir = new File(portDir, String.valueOf(port));
            realPortDir.mkdirs();
        } else {
            ports[0].renameTo(new File(portDir, String.valueOf(port)));
        }
    }

    private static final String MINE_BIN = "application/octet-stream";

    public static class Server extends NanoHTTPD {

        private final File hostDir;
        private final int waitTraceTimeoutSeconds;
        private boolean dataFlushFinished = false;
        private String error = null;
        private JSONObject traceDebugInfo = null;

        public Server(File hostDir, int waitTraceTimeoutSeconds) {
            super(0);
            this.hostDir = hostDir;
            this.waitTraceTimeoutSeconds = waitTraceTimeoutSeconds;
        }

        @Override
        public Response serve(IHTTPSession session) {
            try {
                String action = session.getParms().get("action");
                if (action == null) {
                    return newFixedLengthResponse(Response.Status.NOT_FOUND, MIME_PLAINTEXT, "no action provided");
                }
                String name = session.getParms().get("name");

                Log.i(TAG, "action=" + action + ", name=" + name);
                switch (action) {
                    case "start":
                        // Start trace asynchronously so that hooked Thread::init() will be called on newly created thread
                        // and we can get thread_list instance to monitor java object allocation.
                        TraceManager.getInstance().startTracing(true);
                        dataFlushFinished = false;
                        error = null;
                        traceDebugInfo = null;
                        Log.i(TAG, "rhea trace started");
                        return newFixedLengthResponse(Response.Status.OK, MIME_PLAINTEXT, "start trace");
                    case "stop":
                        TraceManager.getInstance().stopTracing();
                        Log.i(TAG, "rhea trace stopped");
                        return newFixedLengthResponse(Response.Status.OK, MIME_PLAINTEXT, "stop trace");
                    case "clean":
                        TraceManager.getInstance().clearAfterTracing();
                        return newFixedLengthResponse(Response.Status.OK, MIME_PLAINTEXT, "clear trace");
                    case "query":
                        if (name == null) {
                            return newFixedLengthResponse(Response.Status.NOT_FOUND, MIME_PLAINTEXT, "no name provided for action " + action);
                        }
                        String queryResult = query(name);
                        if (queryResult == null) {
                            return newFixedLengthResponse(Response.Status.NOT_FOUND, MIME_PLAINTEXT, "no result for action " + action + " name " + name);
                        } else {
                            return newFixedLengthResponse(Response.Status.OK, MIME_PLAINTEXT, queryResult);
                        }
                    case "download":
                        if (!waitForDataReady()) {
                            error = "wait for trace ready timeout";
                            return newFixedLengthResponse(Response.Status.NOT_FOUND, MIME_PLAINTEXT, error);
                        }
                        if (name == null) {
                            return newFixedLengthResponse(Response.Status.NOT_FOUND, MIME_PLAINTEXT, "no name provided for action " + action);
                        }
                        File file = new File(hostDir, name);
                        if (!file.exists()) {
                            error = "trace file not exists: " + file.getAbsolutePath();
                            return newFixedLengthResponse(Response.Status.NOT_FOUND, MIME_PLAINTEXT, error);
                        } else {
                            return newChunkedResponse(Response.Status.OK, MINE_BIN, new FileInputStream(file));
                        }
                    default:
                        return newFixedLengthResponse(Response.Status.NOT_FOUND, MIME_PLAINTEXT, "unknown action " + action);
                }
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        private String query(String name) {
            switch (name) {
                case "error":
                    if (error != null) {
                        return error;
                    } else {
                        return "no error";
                    }
                case "debug":
                    if (traceDebugInfo!= null) {
                        return traceDebugInfo.toString();
                    } else {
                        return "";
                    }
            }
            return null;
        }

        public void onTraceDumpFinished(int code, String path, List<TraceMeta> traceMetas, long[] startTokens, long[] endTokens) {
            if (code != 0) {
                error = "dump trace failed, error code is " + code;
            }
            dataFlushFinished = true;
            JSONObject debugInfo = new JSONObject();
            for (int i = 0; i < traceMetas.size(); i++) {
                TraceMeta meta = traceMetas.get(i);
                JSONObject metaJson = new JSONObject();
                try {
                    metaJson.put("start", startTokens[i]);
                    metaJson.put("end", endTokens[i]);
                    if (meta.isCore()) {
                        metaJson.put("capacity", TraceProperties.getCoreBufferSize());
                    }
                    debugInfo.put(meta.getName(), metaJson);
                } catch (JSONException ignored) {

                }
            }
            traceDebugInfo = debugInfo;
        }

        private boolean waitForDataReady() {
            for (int i = 0; i < waitTraceTimeoutSeconds; i++) {
                if (dataFlushFinished) {
                    return true;
                }
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }
            return false;
        }
    }
}