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

import android.content.Context;
import android.os.Build;
import android.util.Log;

import com.bytedance.rheatrace.atrace.BinaryTrace;
import com.bytedance.rheatrace.atrace.RheaATrace;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;

import fi.iki.elonen.NanoHTTPD;

/**
 * Created by caodongping on 2023/2/22
 *
 * @author caodongping@bytedance.com
 */
public class HttpServer {
    private static Server server;
    private static final String MINE_BIN = "application/octet-stream";
    private static final String ASSETS_MAPPING = "methodMapping.txt";
    private static final String BINARY_FILE = "rhea-atrace.bin";

    public static void start(Context context, File hostDir) {
        if (server != null && server.isAlive()) {
            Log.i("RheaServer", "stop previous server on port " + server.getListeningPort());
            server.stop();
        }
        server = new Server(context, hostDir, RheaATrace.getHttpServerPort());
        try {
            server.start();
            Log.i("RheaServer", "start new http server on port " + server.getListeningPort());
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private static class Server extends NanoHTTPD {
        private final File hostDir;
        private final Context context;

        public Server(Context context, File hostDir, int port) {
            super(port);
            this.context = context;
            this.hostDir = hostDir;
        }

        @Override
        public Response serve(IHTTPSession session) {
            try {
                String name = session.getParms().get("name");
                if (name == null) {
                    return newFixedLengthResponse(Response.Status.NOT_FOUND, MIME_PLAINTEXT, "not found");
                }
                switch (name) {
                    case "debug":
                        return newFixedLengthResponse(BinaryTrace.debugInfo());
                    case "deviceInfo":
                        return newFixedLengthResponse(deviceInfo());
                    case "threads":
                        return newFixedLengthResponse(BinaryTrace.getThreadsInfo());
                    case "mappingVersion":
                        try (BufferedReader br = new BufferedReader(new InputStreamReader(context.getAssets().open(ASSETS_MAPPING)))) {
                            return newFixedLengthResponse(Response.Status.OK, MIME_PLAINTEXT, br.readLine());
                        }
                    case "mapping":
                        return newChunkedResponse(Response.Status.OK, MIME_PLAINTEXT, context.getAssets().open(ASSETS_MAPPING));
                    case "trace":
                        return newFixedLengthResponse(Response.Status.OK, MINE_BIN, new FileInputStream(new File(hostDir, BINARY_FILE)), BinaryTrace.currentBufferUsage());
                    default:
                        return newChunkedResponse(Response.Status.OK, MINE_BIN, new FileInputStream(new File(hostDir, name)));
                }
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    private static String deviceInfo() {
        JSONObject json = new JSONObject();
        try {
            json.put("brand", Build.MANUFACTURER);
            json.put("model", Build.MODEL);
            json.put("os", Build.VERSION.SDK_INT);
            json.put("arch", RheaATrace.getArch());
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json.toString();
    }
}