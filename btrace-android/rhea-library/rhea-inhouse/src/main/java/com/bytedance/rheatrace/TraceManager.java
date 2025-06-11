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
package com.bytedance.rheatrace;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Process;
import android.util.Log;

import com.bytedance.rheatrace.server.HttpServer;
import com.bytedance.rheatrace.prop.TraceProperties;
import com.bytedance.rheatrace.trace.TraceAbilityCenter;
import com.bytedance.rheatrace.trace.base.TraceAbility;
import com.bytedance.rheatrace.trace.base.TraceGlobal;
import com.bytedance.rheatrace.trace.base.TraceMeta;
import com.bytedance.rheatrace.utils.HandlerThreadUtils;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.Collections;
import java.util.List;


public class TraceManager {

    private static final String TAG = "RheaTrace:Manager";

    private String tracingDirPath;
    private long[] traceTokens = null;

    public static TraceManager getInstance() {
        return Holder.INSTANCE;
    }

    public void init(Context context) {
        if (TraceProperties.shouldStartWhenAppLaunch()) {
            startTracing(false);
        }
        tracingDirPath = context.getFilesDir().getAbsolutePath() + "/rhea/tracing/" + Process.myPid();
        Thread serverThread = new Thread(() -> HttpServer.start(context.getExternalFilesDir(null), tracingDirPath));
        serverThread.start();
    }

    public boolean startTracing(boolean async) {
        if (traceTokens != null) {
            Log.e(TAG, "start failed: already started and not yet stopped");
            return false;
        }
        if (!TraceGlobal.init()) {
            Log.e(TAG, "start failed: global dependency init failed");
            return false;
        }
        List<TraceMeta> traceMetas = requireTraceMetas();
        if (traceMetas.isEmpty()) {
            return false;
        }
        if (async) {
            Thread startThread = new Thread(() -> {
                List<TraceAbility<?>> traceAbilities = TraceAbilityCenter.getAbilities(traceMetas);
                long[] tokens = new long[traceMetas.size()];
                for (int i = 0; i < traceMetas.size(); i++) {
                    tokens[i] = traceAbilities.get(i).start();
                }
                this.traceTokens = tokens;
            });
            startThread.start();
        } else {
            List<TraceAbility<?>> traceAbilities = TraceAbilityCenter.getAbilities(traceMetas);
            long[] tokens = new long[traceMetas.size()];
            for (int i = 0; i < traceMetas.size(); i++) {
                tokens[i] = traceAbilities.get(i).start();
            }
            this.traceTokens = tokens;
        }
        return true;
    }

    public boolean stopTracing() {
        long[] startTokens = this.traceTokens;
        traceTokens = null;
        if (startTokens == null) {
            Log.e(TAG, "stop failed: no start tokens");
            return false;
        }

        List<TraceMeta> traceMetas = requireTraceMetas();
        List<TraceAbility<?>> traceAbilities = TraceAbilityCenter.getAbilities(traceMetas);
        long[] endTokens = new long[traceMetas.size()];
        for (int i = 0; i < traceMetas.size(); i++) {
            endTokens[i] = traceAbilities.get(i).stop();
        }
        HandlerThreadUtils.getCollectorThreadHandler().post(() -> {
            JSONObject extra = getExtra();
            String extraStr = extra == null ? "" : extra.toString();
            String path = getDumpPath();
            if (!makeDumpDir(path)) {
                Log.e(TAG, "make dump dir failed: " + path);
                HttpServer.getServer().onTraceDumpFinished(-100, getDumpPath(), traceMetas, startTokens, endTokens);
                return;
            }
            for (int i = 0; i < traceMetas.size(); i++) {
                long startToken = startTokens[i];
                long endToken = endTokens[i];
                TraceAbility<?> ability = traceAbilities.get(i);
                int result = ability.dumpTokenRange(startToken, endToken, path, extraStr);
                if (result != 0) {
                    Log.e(TAG, "dumping failed for " + traceMetas.get(i).getName() + ", error code is " + result);
                }
            }
            HttpServer.getServer().onTraceDumpFinished(0, getDumpPath(), traceMetas, startTokens, endTokens);
        });
        return true;
    }

    public void clearAfterTracing() {
        File directory = new File(tracingDirPath);
        if (directory.exists() && directory.isDirectory()) {
            File[] files = directory.listFiles();
            if (files != null) {
                for (File file : files) {
                    file.delete();
                }
            }
            directory.delete();
        }
    }

    private List<TraceMeta> requireTraceMetas() {
        // currently there is only sampling trace data, we will extend it later
        return Collections.singletonList(TraceMeta.Sampling);
    }

    private String getDumpPath() {
        File directory = new File(tracingDirPath);
        if (!directory.exists()) {
            directory.mkdirs();
        }
        return tracingDirPath;
    }

    private boolean makeDumpDir(String path) {
        if (path == null || path.isEmpty()) {
            return false;
        }
        File directory = new File(path);
        if (!directory.exists()) {
            return directory.mkdirs();
        }
        return directory.isDirectory();
    }

    private JSONObject getExtra() {
        try {
            JSONObject params = new JSONObject();
            params.put("processId", Process.myPid());
            return params;
        } catch (JSONException e) {
            return null;
        }
    }

    private static class Holder {
        @SuppressLint("StaticFieldLeak")
        private static final TraceManager INSTANCE = new TraceManager();
    }
}