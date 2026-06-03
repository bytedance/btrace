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

package sample.android.jarvis.experience.gpu;

import android.util.Log;

import com.bytedance.jarvis.experience.metric.gpu.collector.arm.MaliGpuInfo;
import com.bytedance.jarvis.experience.metric.gpu.collector.arm.GpuMaliPerf;
import com.bytedance.jarvis.experience.metric.gpu.collector.arm.GpuUsageCallback;
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

public class MaliGpuPerfDemo {

    private static final String TAG = "MaliGpuPerfDemo";

    public static void demo() {
        new Thread(() -> {
            for(int i = 0;i<5;i++) {
                String gpuInfoJson = GpuMaliPerf.getGpuInfoJson();
                Log.d(TAG, "gpuInfo:" + gpuInfoJson);
                GsonBuilder builder = new GsonBuilder();
                Gson gson = builder.create();
                MaliGpuInfo gpuInfo = gson.fromJson(gpuInfoJson, MaliGpuInfo.class);
                GpuMaliPerf.startGpuPerf();
                GpuUsageCallback sampleCallback = new GpuUsageCallback() {
                    @Override
                    public void onRefresh(double gpuUsage,
                                          double executionUsage,
                                          double arithmeticUsage,
                                          double textureUsage,
                                          double loadStoreUsage,
                                          double shaderUsage) {
                        Log.d(TAG, String.format(
                                "gpuUsage: %.2f%%, executionUsage: %.2f%%, arithmeticUsage: %.2f%%, " +
                                        "textureUsage: %.2f%%, loadStoreUsage: %.2f%%, shaderUsage: %.2f%%",
                                gpuUsage, executionUsage, arithmeticUsage,
                                textureUsage, loadStoreUsage, shaderUsage
                        ));
                    }
                };
                for (int j = 0; j < 10; j++) {
                    try {
                        Thread.sleep(500);
                    } catch (InterruptedException e) {
                        throw new RuntimeException(e);
                    }
                    GpuMaliPerf.sampleGpuPerf(gpuInfo.getMax_freq_mhz(), sampleCallback, null);
                }
                GpuMaliPerf.stopGpuPerf();
            }


        }).start();
    }
}
