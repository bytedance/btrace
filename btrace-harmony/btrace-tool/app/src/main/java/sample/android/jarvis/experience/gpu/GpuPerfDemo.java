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

import com.bytedance.jarvis.experience.metric.gpu.core.GpuInfo;
import com.bytedance.jarvis.experience.metric.gpu.core.GpuMetricStats;
import com.bytedance.jarvis.experience.metric.gpu.core.GpuMonitor;
import com.bytedance.jarvis.experience.metric.gpu.util.GpuInfoFetcher;
import com.bytedance.jarvis.experience.metric.gpu.util.GpuMemoryUtils;

public class GpuPerfDemo {

    private static final String TAG = "GpuPerfDemo";

    public static void demo() {
        new Thread(() -> {
            GpuInfo info = GpuInfoFetcher.getGpuInfo();
            Log.d(TAG, "" + info);
            GpuMonitor.initialize(1000, 10);
            GpuMonitor.get().start();
            new Thread(new Runnable() {
                @Override
                public void run() {
                    for (int i = 0; i < 10; i++) {
                        try {
                            Thread.sleep(10_000);
                        } catch (InterruptedException e) {
                            throw new RuntimeException(e);
                        }
                        Log.d(TAG, "---- GPU PERF TEST -> Round " + i + "----");
                        GpuMetricStats stats = GpuMonitor.get().getGpuMetricStats();
                        Log.d(TAG, "" + stats);
                        int graphics = GpuMemoryUtils.getGraphicsUsage();
                        Log.d(TAG, "Graphics = " + graphics);
                    }
                    GpuMonitor.get().stop();
                }
            }).start();
        }).start();
    }
}
