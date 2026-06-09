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

package sample.android.jarvis;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.util.Log;
import android.widget.ListView;
import android.widget.Toast;

import com.bytedance.jarvis.core.Jarvis;
import com.bytedance.jarvis.core.scene.monitor.ANRMonitor;
import com.bytedance.jarvis.core.scene.monitor.AppStartMonitor;
import com.bytedance.jarvis.core.scene.monitor.BenchmarkMonitor;
import com.bytedance.jarvis.core.scene.monitor.FirstFeedMonitor;
import com.bytedance.jarvis.core.scene.monitor.GeneralMonitor;
import com.bytedance.jarvis.core.scene.monitor.ViewTraceMonitor;
import com.bytedance.jarvis.stacktrace.JavaStack;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;

import sample.android.jarvis.test.FutureTest;
import sample.android.jarvis.view.TestActivity;

public class MainActivity extends Activity {
    static {
        System.loadLibrary("jarvis-demo");
    }

    GeneralMonitor generalMonitor = new GeneralMonitor(1000);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Jarvis.traceVision("MainActivity#onCreate", true);
        assert !JavaStack.getStackTrace().isEmpty();
        Thread.currentThread().setPriority(Thread.MIN_PRIORITY);
        ColdBootLogger.end(ColdBootLogger.COLD_BOOT_APPLICATION_TO_MAIN);
        ColdBootLogger.begin(ColdBootLogger.COLD_BOOT_MAIN_CREATE_DURATION);
        super.onCreate(savedInstanceState);
        generalMonitor.start();
        setContentView(R.layout.activity_main);
        setTitle(String.valueOf(Process.myPid()));
        FutureTest.INSTANCE.test();
        initClicks();
        ColdBootLogger.end(ColdBootLogger.COLD_BOOT_MAIN_CREATE_DURATION);
        ColdBootLogger.begin(ColdBootLogger.COLD_BOOT_MAIN_CREATE_TO_RESUME);
        monitorCprfOutput();
        Jarvis.traceVision("MainActivity#onCreate", false);
    }

    private void initClicks() {
        findViewById(R.id.frame_drop).setOnClickListener(v -> {
            new Thread(() -> {
                LockSupport.parkNanos(TimeUnit.SECONDS.toNanos(1));
                synchronized (MainActivity.this) {
                    MainActivity.this.notifyAll();
                }
            }).start();
            LockSupport.parkNanos(TimeUnit.MILLISECONDS.toNanos(100));
            synchronized (MainActivity.this) {
                try {
                    MainActivity.this.wait();
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }
            Toast.makeText(MainActivity.this, "frame drop", Toast.LENGTH_SHORT).show();
        });
        findViewById(R.id.dump_anr).setOnClickListener(v -> {
            long now = System.currentTimeMillis();
            ANRMonitor.INSTANCE.setLastAnrTime(now);
            ANRMonitor.INSTANCE.flushANR(new Runnable() {
                @Override
                @SuppressLint("[ByDesign6.4]UnsafeFile")
                public void run() {
                    File dir = new File(new File(getExternalFilesDir(""), "cprf"), String.valueOf(Process.myPid()));
                    boolean mkdirs = dir.mkdirs();
                    File zip = new File(dir, "anr_" + System.currentTimeMillis() + ".zip");
                    Jarvis.traceFileName(zip.getAbsolutePath());
                    String flushDirPath = ANRMonitor.INSTANCE.getFlushDirPath();
                    assert flushDirPath != null;
                    try {
                        FileUtils.zipDirToFile(new File(flushDirPath), zip);
                    } catch (IOException e) {
                        throw new RuntimeException(e);
                    }
                    long cost = System.currentTimeMillis() - now;
                    Toast.makeText(MainActivity.this, "dump anr cost " + cost + "ms", Toast.LENGTH_SHORT).show();
                }
            });
        });
        findViewById(R.id.dump_types).setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, SamplingTypeStatActivity.class);
            startActivity(intent);
        });
        findViewById(R.id.cpu_high_load).setOnClickListener(v -> {
            for (int i = 0; i < 10; i++) {
                new Thread(new Runnable() {
                    @SuppressLint("[ByDesign7.4]WeakPRNG")
                    @Override
                    public void run() {
                        while (true) {
                            double result = Math.sin(Math.random()) * Math.cos(Math.random());
                            System.out.println(result);
                        }
                    }
                }).start();
            }
        });
        findViewById(R.id.view_trace).setOnClickListener(v -> {
            ViewTraceMonitor.INSTANCE.flush();
            long key = ViewTraceMonitor.INSTANCE.getJarvisCurrentTime();
            Toast.makeText(MainActivity.this, "view trace key " + key, Toast.LENGTH_SHORT).show();
        });
        findViewById(R.id.capture_main_from_child).setOnClickListener(v -> {
            Future<?> submit = Executors.newSingleThreadExecutor().submit(() -> {
                long[] stackTrace = JavaStack.fastGetStackTraceForTarget(Looper.getMainLooper().getThread());
                if (stackTrace == null) {
                    return null;
                } else {
                    return JavaStack.getSymbols(stackTrace);
                }
            });
            try {
                String[] data = (String[]) submit.get();
                Toast.makeText(MainActivity.this, data == null ? "null" : Arrays.toString(data), Toast.LENGTH_SHORT).show();
            } catch (Throwable e) {
                throw new RuntimeException(e);
            }
        });
        findViewById(R.id.debug_native_trace).setOnClickListener(v -> {
            nativeTest();
            Toast.makeText(MainActivity.this, "native test", Toast.LENGTH_SHORT).show();
        });
        findViewById(R.id.user_sense).setOnClickListener(v -> {
            Intent intent = new Intent(this, TestActivity.class);
            startActivity(intent);
        });
        findViewById(R.id.debug_ddpf).setOnClickListener(v->{
            MockDDPF.callback.onSignal(10010, 0, null);
            Toast.makeText(MainActivity.this, "ddpf flush 10010", Toast.LENGTH_SHORT).show();
            MockDDPF.callback.onSignal(10086, -1, null);
            new Thread(() -> {
                LockSupport.parkNanos(TimeUnit.SECONDS.toNanos(2));
                MockDDPF.callback.onSignal(10086, 1, null);
            }).start();
            Toast.makeText(MainActivity.this, "ddpf stop 10086", Toast.LENGTH_SHORT).show();
        });
    }

    private void monitorCprfOutput() {
        FileListAdapter adapter = new FileListAdapter(this);
        ListView listView = findViewById(R.id.list_view);
        listView.setAdapter(adapter);
        adapter.update();
        adapter.notifyDataSetChanged();
        int checkInterval = 1000;
        listView.postDelayed(new Runnable() {
            @Override
            public void run() {
                adapter.update();
                adapter.notifyDataSetChanged();
                listView.postDelayed(this, checkInterval);
            }
        }, checkInterval);
    }

    private void visitFile(File files, StringBuilder sb) {
        if (files.isDirectory()) {
            File[] listFiles = files.listFiles();
            if (listFiles == null) {
                return;
            }
            for (File file : listFiles) {
                visitFile(file, sb);
            }
        } else {
            sb.append(files.getName()).append("\n");
        }
    }

    public void setContentView(int layout) {
        super.setContentView(layout);
        Jarvis.traceLayoutID(layout);
    }

    @Override
    protected void onResume() {
        ColdBootLogger.end(ColdBootLogger.COLD_BOOT_MAIN_CREATE_TO_RESUME);
        ColdBootLogger.begin(ColdBootLogger.COLD_BOOT_MAIN_RESUME_DURATION);
        super.onResume();
        ColdBootLogger.end(ColdBootLogger.COLD_BOOT_MAIN_RESUME_DURATION);
        ColdBootLogger.begin(ColdBootLogger.COLD_BOOT_MAIN_RESUME_TO_FOCUS);
    }

    public static void sleepFromJNI() {
        long now = System.currentTimeMillis();
        while (System.currentTimeMillis() - now < 100) {
            LockSupport.parkNanos(TimeUnit.MILLISECONDS.toNanos(10));
        }
    }

    private static native void nativeTest();

    @Override
    protected void onPause() {
        LockSupport.parkNanos(TimeUnit.SECONDS.toNanos(1));
        super.onPause();
        try {
            InputStream io = getAssets().open("abc");
        } catch (IOException e) {
            e.printStackTrace();
        }
        try {
            System.loadLibrary("abc");
        } catch (Throwable e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        ColdBootLogger.end(ColdBootLogger.COLD_BOOT_MAIN_RESUME_TO_FOCUS);
        ColdBootLogger.begin(ColdBootLogger.COLD_BOOT_MAIN_FOCUS_DURATION);
        super.onWindowFocusChanged(hasFocus);
        generalMonitor.stop();
        Handler handler = new Handler(Looper.getMainLooper());
        AppStartMonitor.INSTANCE.setBizEventName("first_feed_show_time_v3");
        AppStartMonitor.INSTANCE.setIsNewUser("true");
        AppStartMonitor.INSTANCE.setPhase(ColdBootLogger.getPhase());
        handler.post(() -> {
            Log.i("Jarvis", "app start finish");
            AppStartMonitor.INSTANCE.stop();
        });
        handler.postDelayed(() -> {
            Log.i("Jarvis", "app first feed finish");
            FirstFeedMonitor.INSTANCE.stop();
            BenchmarkMonitor.INSTANCE.stop();
        }, 3000);
        FutureTest.INSTANCE.test();
        ColdBootLogger.end(ColdBootLogger.COLD_BOOT_MAIN_FOCUS_DURATION);
    }
}
