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

package sample.android.jarvis.test;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;

import com.bytedance.apm6.java_alloc.JavaAllocCollector;
import com.bytedance.jarvis.experience.metric.alloc.JavaAllocAutoSignal;
import com.bytedance.jarvis.experience.metric.alloc.JavaAllocDetailMetrics;
import com.bytedance.jarvis.experience.metric.alloc.JavaAllocRegister;
import com.bytedance.jarvis.experience.metric.alloc.JavaAllocSignalHandler;
import com.bytedance.monitor.collector.PerfMonitorManager;

public class JavaAllocTest {
    public static void init(Context context) {
        PerfMonitorManager.loadLibrary(context);
        JavaAllocDetailMetrics metrics = JavaAllocDetailMetrics.getInstance();
        metrics.setJavaAllocRegister(new JavaAllocRegister() {
            @Override
            public boolean register(long func) {
                return JavaAllocCollector.getInstance().registerJavaAllocMonitor(func);
            }
            @Override
            public boolean unregister(long func) {
                return JavaAllocCollector.getInstance().unregisterJavaAllocMonitor(func);
            }
        });

        JavaAllocAutoSignal.getInstance().addHandler(new JavaAllocSignalHandler() {
            @Override
            public void notify(int cause, float oldCountSpeed, float newCountSpeed, float oldSizeSpeed, float newSizeSpeed) {
                Log.e("JavaAlloc", "notify: " + cause + ", " + (oldSizeSpeed / 1024 / 1024) + ", " + (newSizeSpeed / 1024 / 1024) + ", " + (oldCountSpeed / 1024 / 1024) + ", " + (newCountSpeed / 1024 / 1024));
            }
        });
        HandlerThread thread = new HandlerThread("JavaAlloc");
        thread.start();
        Handler handler = new Handler(thread.getLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
//                JavaAllocAutoSignal.getInstance().setConfig(4 * 1024 * 1024, 1024 * 1024 / 10, 100, new JavaAllocAutoSignal.ByteAllocatedGetter() {
//                    @Override
//                    public long getBytes() {
//                        return JavaAllocCollector.getInstance().getBytesAllocatedEver();
//                    }
//                });
                JavaAllocAutoSignal.getInstance().init();
            }
        });
    }
}
