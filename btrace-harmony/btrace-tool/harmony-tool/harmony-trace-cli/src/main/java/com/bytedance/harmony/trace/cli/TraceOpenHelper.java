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

import java.awt.Desktop;
import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.file.Files;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;

import fi.iki.elonen.NanoHTTPD;

public class TraceOpenHelper {
    private static final String MINE_BIN = "application/octet-stream";
    private static final int PORT = 9001;
    private static final String PATH = "/trace";
    private static final Object LOCK = new Object();
    private final SingleFileServer server;
    private final File trace;

    public TraceOpenHelper(File trace) throws IOException {
        this.trace = trace;
        server = new SingleFileServer(trace, PORT);
        server.start();
    }

    public void openInBrowser() throws IOException, URISyntaxException, InterruptedException {
        Log.i("open " + trace + " in browser");
        String url = "https://ui.perfetto.dev/#!/?url=http://127.0.0.1:" + PORT + PATH;
        Desktop.getDesktop().browse(new URI(url));
        await();
    }

    private void await() throws InterruptedException {
        while (!server.isDone()) {
            synchronized (LOCK) {
                LOCK.wait();
            }
        }
        LockSupport.parkNanos(TimeUnit.SECONDS.toNanos(1));
    }

    private static class SingleFileServer extends NanoHTTPD {
        private final File target;
        private volatile boolean done;


        public SingleFileServer(File target, int port) {
            super(port);
            this.target = target;
        }

        public boolean isDone() {
            return done;
        }

        @Override
        public Response serve(IHTTPSession session) {
            try {
                if (PATH.equals(session.getUri())) {
                    Response resp = newChunkedResponse(Response.Status.OK, MINE_BIN, Files.newInputStream(target.toPath()));
                    resp.addHeader("Access-Control-Allow-Origin", "*");
                    synchronized (LOCK) {
                        done = true;
                        LOCK.notifyAll();
                    }
                    return resp;
                } else {
                    Response resp = newFixedLengthResponse(Response.Status.NOT_FOUND, MIME_PLAINTEXT, "404");
                    resp.addHeader("Access-Control-Allow-Origin", "*");
                    return resp;
                }
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }
}
