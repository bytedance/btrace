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

import static com.bytedance.jarvis.core.deliver.HostEnv.BOOT_TYPE;
import static com.bytedance.jarvis.core.deliver.HostEnv.BUILD_TIME;

import android.annotation.SuppressLint;
import android.app.Application;
import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.Process;
import android.os.SystemClock;
import android.telephony.PhoneStateListener;
import android.util.Log;
import android.widget.Toast;

import com.bytedance.android.bytehook.ByteHook;
import com.bytedance.apm6.java_alloc.JavaAllocCollector;
import com.bytedance.jarvis.base.extra.HostMetric;
import com.bytedance.jarvis.core.Jarvis;
import com.bytedance.jarvis.core.config.PackedMonitorConfig;
import com.bytedance.jarvis.core.deliver.FileDelivery;
import com.bytedance.jarvis.core.deliver.HostEnv;
import com.bytedance.jarvis.core.scene.monitor.ANRMonitor;
import com.bytedance.jarvis.core.scene.monitor.AppStartMonitor;
import com.bytedance.jarvis.core.scene.monitor.BenchmarkMonitor;
import com.bytedance.jarvis.core.scene.monitor.ContinuousDropMonitorConfig;
import com.bytedance.jarvis.core.scene.monitor.CrashMonitor;
import com.bytedance.jarvis.core.scene.monitor.CrashMonitorConfig;
import com.bytedance.jarvis.core.scene.monitor.FeedbackMonitor;
import com.bytedance.jarvis.core.scene.monitor.FirstFeedMonitor;
import com.bytedance.jarvis.core.scene.monitor.FluentMonitor;
import com.bytedance.jarvis.core.scene.monitor.ScreenRecordMonitorConfig;
import com.bytedance.jarvis.core.scene.monitor.ViewTraceMonitorConfig;
import com.bytedance.jarvis.core.scene.monitor.ddpf.DDPFMonitorConfig;
import com.bytedance.jarvis.core.util.JavaAllocationListener;
import com.bytedance.jarvis.cpu.CpuMonitorConfig;
import com.bytedance.jarvis.experiencemap.ExpMap;
import com.bytedance.jarvis.experiencemap.UserExpMapMonitor;
import com.bytedance.jarvis.experiencemap.UserExpMapMonitorConfig;
import com.bytedance.jarvis.experiencemap.config.Config;
import com.bytedance.jarvis.experiencemap.config.impl.CpuConfig;
import com.bytedance.jarvis.experiencemap.config.impl.GcConfig;
import com.bytedance.jarvis.experiencemap.config.impl.LightCpuConfig;
import com.bytedance.jarvis.experiencemap.config.impl.LightMemoryConfig;
import com.bytedance.jarvis.experiencemap.config.impl.MemoryConfig;
import com.bytedance.jarvis.experiencemap.config.impl.TrafficConfig;
import com.bytedance.jarvis.ext.applog.AppLogDeliver;
import com.bytedance.jarvis.memory.monitor.MemMonitorConfig;
import com.bytedance.jarvis.scene.AnrMonitorConfig;
import com.bytedance.jarvis.scene.AppStartMonitorConfig;
import com.bytedance.jarvis.scene.FeedbackMonitorConfig;
import com.bytedance.jarvis.scene.GeneralMonitorConfig;
import com.bytedance.jarvis.scene.SpikeAllocationMonitorConfig;
import com.bytedance.jarvis.scene.SpikeThreadHighLoadMonitorConfig;
import com.bytedance.jarvis.scene.VideoPlayMonitorConfig;
import com.bytedance.jarvis.stacktrace.JavaStack;
import com.bytedance.jarvis.trace.fps.JankFrameMonitorConfig;
import com.bytedance.jarvis.trace.fps.JankMessageMonitor;
import com.bytedance.jarvis.trace.fps.JankMessageMonitorConfig;
import com.bytedance.jarvis.trace.fps.expmap.FpsMonitorConfig;
import com.bytedance.jarvis.trace.message.MessageTrace;
import com.bytedance.jarvis.trace.metrics.MetricsMonitorConfig;
import com.bytedance.jarvis.trace.stack.SamplingMonitorConfig;
import com.bytedance.monitor.collector.PerfMonitorManager;
import com.bytedance.rheatrace.RheaDependencyManager;
import com.bytedance.rheatrace.core.RheaTrace;

import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;

import sample.android.jarvis.experience.gpu.GpuPerfDemo;

public class App extends Application {

    private static final String TAG = "Jarvis:App";

    static {
        ByteHook.init();
        Thread.UncaughtExceptionHandler defaultUncaughtExceptionHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((t, e) -> {
            if (Jarvis.hasInitialized()) {
                long now = SystemClock.elapsedRealtimeNanos();
                CrashMonitor.INSTANCE.flushSync();
                Log.e(TAG, "flush sync cost " + (SystemClock.elapsedRealtimeNanos() - now) / 1000000.0 + "ms");
                String crashTraceURL = CrashMonitor.INSTANCE.getCrashTraceURL("1128", "1128");
                Log.e(TAG, "crash trace url " + crashTraceURL);
            }
            if (defaultUncaughtExceptionHandler != null) {
                defaultUncaughtExceptionHandler.uncaughtException(t, e);
            }
        });
    }

    private MessageQueue.IdleHandler unreachableIdleHandler = () -> {
        throw new RuntimeException("unreachable code");
    };

    private MessageQueue.IdleHandler runningOnceIdleHandler = new MessageQueue.IdleHandler() {
        @Override
        public boolean queueIdle() {
            Log.i(TAG, "on queue idle");
            return false;
        }
    };

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        RheaTrace.init(this, RheaDependencyManager.getAppStartCallback());
        Jarvis.forceEnableForDebug();
        ColdBootLogger.begin(ColdBootLogger.COLD_BOOT_APPLICATION_ATTACH_DURATION);
//        Debug.startMethodTracing(new File(base.getExternalFilesDir(""), "cba.trace").getAbsolutePath(), 512 * 1024 * 1024);
        if (!Jarvis.isEnabled()) {
            Log.e(TAG, "Jarvis is not enabled.");
            return;
        }
        Log.e(TAG, "Jarvis is enabled.");
        PackedMonitorConfig config = makeConfig();
        if (config != null) {
            Toast.makeText(this, "init jarvis sync", Toast.LENGTH_SHORT).show();
            initJarvis(config);
        } else {
            Toast.makeText(this, "init jarvis async", Toast.LENGTH_SHORT).show();
            new Thread(() -> initJarvis(config)).start();
        }
        ColdBootLogger.end(ColdBootLogger.COLD_BOOT_APPLICATION_ATTACH_DURATION);
        ColdBootLogger.begin(ColdBootLogger.COLD_BOOT_APPLICATION_ATTACH_TO_CREATE);
//        JavaAllocTest.init(this);
    }

    private void initJarvis(PackedMonitorConfig config) {
        PerfMonitorManager.loadLibrary(this);
        long now = SystemClock.elapsedRealtime();
        HostEnv env = HostEnv.newBuilder().setAppLaunchTime(System.currentTimeMillis())
                .setBootTypeSupplier(new HostEnv.BootTypeSupplier() {
                    @Override
                    public String get(String key) {
                        switch (key) {
                            case BOOT_TYPE:
                                return "main";
                            case BUILD_TIME:
                                return "1234";
                            //case BIZ_EVENT_NAME:
                            //    return "first_feed_show_time_v3";
                            //case NEW_USER:
                            //    return "true";
                        }
                        return null;
                    }
                })
                .setBootSource("main")
                .setUpdateVersionCode("100")
                .setHostAbiBit(64)
                .setAppLogHandler((event, param) -> {
                    Log.e(TAG, event + " " + Thread.currentThread().getName());
                    Log.e(TAG, param == null ? "null" : param.toString());
                })
                .build();
        ExecutorService executorService = Executors.newFixedThreadPool(Runtime.getRuntime().availableProcessors());
        Jarvis.init(this, config, new AppLogDeliver(env, true), executorService, (path, id, time) -> {
            Log.e("cdpcdp", "send " + path);
            try {
                String timeStr = new SimpleDateFormat("yyyyMMdd_HHmmss.SSS").format(time);
                File target = new File(new File(getExternalFilesDir(""), "cprf"), String.valueOf(Process.myPid()));
                target.mkdirs();
                File file = new File(target, id + "_" + timeStr + ".zip");
                File dir = new File(path);
                FileUtils.zipDirToFile(dir, file);
                FileDelivery.remove(dir);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }, new JavaAllocationListener.Implementation() {
            @Override
            public boolean set(long ptr) {
                return JavaAllocCollector.getInstance().registerJavaAllocMonitor(ptr);
            }

            @Override
            public boolean remove(long ptr) {
                return JavaAllocCollector.getInstance().unregisterJavaAllocMonitor(ptr);
            }
        }, null, callback -> {
            MockDDPF.callback = callback;
            Log.d("shenyunlong", ">>> ddpf callback = " + callback);
        }, () -> false);
        AppStartMonitor.INSTANCE.start();
        BenchmarkMonitor.INSTANCE.start();
        FirstFeedMonitor.INSTANCE.start();
        ANRMonitor.INSTANCE.start();
        FeedbackMonitor.INSTANCE.start();
        FluentMonitor.INSTANCE.start();
        UserExpMapMonitor.INSTANCE.start();
        long cost = SystemClock.elapsedRealtime() - now;
        Log.i("cdpcdp", "init jarvis cost " + cost + "ms");
        JankMessageMonitor.INSTANCE.setCallback((info, interval) -> Log.e(TAG, "onJank " + info.toString()));
        mockBigAlloc();
        initExpMap(env, executorService);
        testCrashOn13();
    }

    private void testCrashOn13() {
        new Thread(() -> {
            LockSupport.parkNanos(TimeUnit.SECONDS.toNanos(1));
            while (true) {
                try {
                    System.out.println(new PhoneStateListener());
                } catch (Throwable ignore) {
                }
                LockSupport.parkNanos(TimeUnit.MICROSECONDS.toNanos(1));
            }
        }).start();
    }

    @SuppressLint("[ByDesign7.4]WeakPRNG")
    private void mockBigAlloc() {
        new Thread(() -> {
            while (true) {
                int[] ints = new int[1024 * 1024];
                System.out.println(ints.length);
                long sleepMillis = (long) (Math.random() * 5000);
                LockSupport.parkNanos(TimeUnit.MILLISECONDS.toNanos(sleepMillis));
            }
        }).start();
    }

    private void initExpMap(HostEnv env, ExecutorService executor) {
        ExpMap.init(this, initConfig(), new AppLogDeliver(env, true), executor,
                new HostMetric.ExtraInfoProvider() {
                    @Override
                    public JSONObject getJitInfo() {
                        return null;
                    }

                    @Override
                    public JSONObject getGcInfo() {
                        return null;
                    }

                    @Override
                    public JSONObject getResourceInfo() {
                        return null;
                    }

                    @Override
                    public JSONObject getAllocInfo() {
                        return null;
                    }
                }, null, null
        );
    }

    private Config initConfig() {
        LightCpuConfig cpuLight = new LightCpuConfig();
        cpuLight.setOpen(true);
        LightMemoryConfig memoryLight = new LightMemoryConfig();
        memoryLight.setOpen(true);
        CpuConfig cpu = new CpuConfig();
        cpu.setOpen(true);  //cpu
        cpu.setSystemSamplingRate(1);
        cpu.setThreadSamplingRate(1);
        cpu.setSchedStatSamplingRate(1);
        cpu.setContextSwitchSamplingRate(1);
        cpu.setPowerSamplingRate(1);
        cpu.setPageFaultSamplingRate(1);
        GcConfig gc = new GcConfig();
        gc.setOpen(true);  //gc
        MemoryConfig memory = new MemoryConfig();
        memory.setOpen(true);
        memory.setProcessMemSamplingRate(1);  //memory
        memory.setJavaMemSamplingRate(1);
        memory.setSystemMemSamplingRate(1);
        memory.setAllocMemSamplingRate(1);
        memory.setResourceSamplingRate(1);
        TrafficConfig traffic = new TrafficConfig();
        traffic.setOpen(true);  //traffic
        return Config.builder()
                .setOpen(true)
                .setBufferSize(128)
                .setReportMode(2)
                .setIntervalTime(2000)
                .setBgIntervalTime(10000)
                .setCpu(cpu)
                .setCpuLight(cpuLight)
                .setGc(gc)
                .setMemory(memory)
                .setMemoryLight(memoryLight)
                .setTraffic(traffic)
                .setVision(null)
                .setInteract(null)
                .setOther(null)
                .setView(null)
                .setDebugMode(true)
                .build();
    }

    private PackedMonitorConfig makeConfig() {
        CpuMonitorConfig cpuMonitorConfig = new CpuMonitorConfig();
        cpuMonitorConfig.setOpen(true);
        MemMonitorConfig memMonitorConfig = new MemMonitorConfig();
        memMonitorConfig.setOpen(true);
        memMonitorConfig.setEnableGC(true);
        memMonitorConfig.setEnablePageFault(true);
        JankFrameMonitorConfig jankFrameMonitorConfig = new JankFrameMonitorConfig();
        jankFrameMonitorConfig.setOpen(true);
        jankFrameMonitorConfig.setFrameDropThreshold(7);
        jankFrameMonitorConfig.setProbability(1);
        jankFrameMonitorConfig.setMemoryInfo(true);
        Map<String, Double> map = new HashMap<>();
        map.put("default", 1.0);
        jankFrameMonitorConfig.setProbabilityMap(map);
        jankFrameMonitorConfig.setSceneEventBufferSize(128);
        JankMessageMonitorConfig jankMessageMonitorConfig = new JankMessageMonitorConfig();
        jankMessageMonitorConfig.setOpen(true);
        jankMessageMonitorConfig.setThreshold(200);
        jankMessageMonitorConfig.setInterval(10);
        jankMessageMonitorConfig.setUserSampleRate(1);
        jankMessageMonitorConfig.setMessageSampleRate(1);
        AnrMonitorConfig anrMonitorConfig = new AnrMonitorConfig();
        anrMonitorConfig.setOpen(true);
        anrMonitorConfig.setSamplingRate(1.0);
        AppStartMonitorConfig appStartMonitorConfig = new AppStartMonitorConfig();
        appStartMonitorConfig.setOpen(true);
        appStartMonitorConfig.setSamplingRate(1.0);
        UserExpMapMonitorConfig userExpMapMonitorConfig = new UserExpMapMonitorConfig();
        userExpMapMonitorConfig.setOpen(true);
        userExpMapMonitorConfig.setIntervalTime(10 * 1000);  // 10s
        userExpMapMonitorConfig.setBufferSize(128);
        userExpMapMonitorConfig.setEnableCollect(true);
        userExpMapMonitorConfig.setReportMode(2);
        userExpMapMonitorConfig.setDisableTags("1024,1025");
        VideoPlayMonitorConfig videoPlayMonitorConfig = new VideoPlayMonitorConfig();
        videoPlayMonitorConfig.setOpen(true);
        videoPlayMonitorConfig.setSamplingRate(1.0);
        SamplingMonitorConfig samplingMonitorConfig = new SamplingMonitorConfig();
        samplingMonitorConfig.setOpen(true);
        samplingMonitorConfig.setBufferSize(4096);
        samplingMonitorConfig.setBackgroundBufferSize(16384);
        samplingMonitorConfig.setAtraceBufferSize(1024);
        samplingMonitorConfig.setAtraceTags(9);
        samplingMonitorConfig.setMainThreadInterval(3);
        samplingMonitorConfig.setMainThreadMode(false);
        samplingMonitorConfig.setOtherThreadInterval(10);
        samplingMonitorConfig.setNativeSamplingRate(1);
        samplingMonitorConfig.setJavaAllocStatMode(1);
        samplingMonitorConfig.setJavaThreadStateBufferSize(1024);
        samplingMonitorConfig.setNativeSamplingType(1);
        samplingMonitorConfig.setMainThreadNativeInterval(3);
        samplingMonitorConfig.setOtherThreadNativeInterval(10);
        samplingMonitorConfig.setClockType(2);
        samplingMonitorConfig.setClockResolution(1);
        samplingMonitorConfig.setStackWalkKind(samplingMonitorConfig.getClockType());
        samplingMonitorConfig.setEnableRusage(true);
        samplingMonitorConfig.setEnableWakeup(true);
        samplingMonitorConfig.setEnableThreadNames(true);
        samplingMonitorConfig.setThreadStateInterval(16);
        samplingMonitorConfig.setEnableOnCMC(true);
        samplingMonitorConfig.setThreadStateBufferSize(3000);
        samplingMonitorConfig.setFrameInfoBufferSize(6000);
        samplingMonitorConfig.setFrameThresholdMs(16);
        samplingMonitorConfig.setGcEventBufferSize(500);
        samplingMonitorConfig.setArgumentBufferSize(200);
        samplingMonitorConfig.setVisionBufferSize(128);
        samplingMonitorConfig.setEnableSuspendTrace(true);
        samplingMonitorConfig.setThreadBornBufferSize(512);
        samplingMonitorConfig.setThreadPriorityBufferSize(512);

        FeedbackMonitorConfig feedbackMonitorConfig = new FeedbackMonitorConfig();
        GeneralMonitorConfig generalMonitorConfig = new GeneralMonitorConfig();
        generalMonitorConfig.setOpen(true);
        generalMonitorConfig.setSamplingRate(1.0);
        ArrayList<Integer> businessIds = new ArrayList<>();
        businessIds.add(1002);
        businessIds.add(1000);
        generalMonitorConfig.setBusinessIds(businessIds);
        MetricsMonitorConfig metricsMonitorConfig = new MetricsMonitorConfig();
        metricsMonitorConfig.setSamplingRate(1);
        metricsMonitorConfig.setOpen(true);
        metricsMonitorConfig.setGcEnabled(true);
        metricsMonitorConfig.setJitEnabled(false);
        SpikeAllocationMonitorConfig spikeAllocationMonitorConfig = new SpikeAllocationMonitorConfig();
        spikeAllocationMonitorConfig.setOpen(true);
        spikeAllocationMonitorConfig.setSpikeThresholdMB(1);
        spikeAllocationMonitorConfig.setMaxCombo(5);
        spikeAllocationMonitorConfig.setSamplingRate(0.5);
        SpikeThreadHighLoadMonitorConfig spikeThreadHighLoadMonitorConfig = new SpikeThreadHighLoadMonitorConfig();
        spikeThreadHighLoadMonitorConfig.setOpen(true);
        spikeThreadHighLoadMonitorConfig.setWindowInSecond(8);
        spikeThreadHighLoadMonitorConfig.setCpuTimeThresholdMs(1000);
        spikeThreadHighLoadMonitorConfig.setSamplingRate(0.5);
        spikeThreadHighLoadMonitorConfig.setMinSecondBetweenThreadReport(10);

        FpsMonitorConfig fpsMonitorConfig = new FpsMonitorConfig();
        fpsMonitorConfig.setOpen(true);
        fpsMonitorConfig.setFrameDropThreshold(7);

        ContinuousDropMonitorConfig continuousDropMonitorConfig = new ContinuousDropMonitorConfig();
        continuousDropMonitorConfig.setOpen(true);
        continuousDropMonitorConfig.setSamplingRate(1.0);

        ScreenRecordMonitorConfig screenRecordMonitorConfig = new ScreenRecordMonitorConfig();
        screenRecordMonitorConfig.setOpen(true);
        screenRecordMonitorConfig.setSamplingRate(1.0);

        CrashMonitorConfig crashMonitorConfig = new CrashMonitorConfig();
        crashMonitorConfig.setOpen(true);
        crashMonitorConfig.setSamplingRate(1.0);
        crashMonitorConfig.setDelaySeconds(5);
        crashMonitorConfig.setUrlPattern(
                "https://sol.bytedance.net/open_api/perfetto_trace/download?api_id=7477931432082490408&key=trace_pb&params" +
                        "={\"scene_id\": \"{SCENE_ID}\", \"app_id\": \"{APP_ID}\", \"os_name\": \"android\", \"device_id\": " +
                        "\"{DEVICE_ID}\", \"current_time_ms\": \"{TIME}\"}");

        DDPFMonitorConfig ddpfMonitorConfig = new DDPFMonitorConfig();
        ddpfMonitorConfig.setOpen(true);
        ddpfMonitorConfig.setSamplingRate(1);

        ViewTraceMonitorConfig viewTraceMonitorConfig = new ViewTraceMonitorConfig();
        viewTraceMonitorConfig.setOpen(true);
        viewTraceMonitorConfig.setSamplingRate(1);

        Log.i(TAG, "enable sampling trace. disable apptrace/cputime/binder/lock/stack");
        return PackedMonitorConfig.builder()
                .setPreciseClock(false)
                .setCpuMonitorConfig(cpuMonitorConfig)
                .setMemMonitorConfig(memMonitorConfig)
                .setJankFrameMonitorConfig(jankFrameMonitorConfig)
                .setFpsMonitorConfig(fpsMonitorConfig)
                .setJankMessageMonitorConfig(jankMessageMonitorConfig)
                .setAnrMonitorConfig(anrMonitorConfig)
                .setAppStartMonitorConfig(appStartMonitorConfig)
                .setUserExpMapMonitorConfig(userExpMapMonitorConfig)
                .setGeneralMonitorConfig(generalMonitorConfig)
                .setSamplingMonitorConfig(samplingMonitorConfig)
                .setAsyncInit(System.currentTimeMillis() % 2 == 0)
                .setMetricsMonitorConfig(metricsMonitorConfig)
                .setFeedbackMonitorConfig(feedbackMonitorConfig)
                .setVideoPlayMonitorConfig(videoPlayMonitorConfig)
                .setSpikeAllocationMonitorConfig(spikeAllocationMonitorConfig)
                .setSpikeThreadHighLoadMonitorConfig(spikeThreadHighLoadMonitorConfig)
                .setContinuousDropMonitorConfig(continuousDropMonitorConfig)
                .setScreenRecordMonitorConfig(screenRecordMonitorConfig)
                .setAnrRiskMonitorConfig(null)
                .setCrashMonitorConfig(crashMonitorConfig)
                .setDdpfMonitorConfig(ddpfMonitorConfig)
                .setViewTraceMonitorConfig(viewTraceMonitorConfig)
                .build();
    }

    private void monitorMainThreadMessage() {
        Looper mainLooper = getMainLooper();
        try {
            Class<?> observerClass = Class.forName("android.os.Looper$Observer");
            Object observer =
                    Proxy.newProxyInstance(App.class.getClassLoader(), new Class[] {observerClass}, new InvocationHandler() {
                        @Override
                        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
                            if (Looper.myLooper() != mainLooper) {
                                return null;
                            }
                            if ("messageDispatchStarting".equals(method.getName())) {
                                MessageTrace.onMessageStart();
                            } else if ("messageDispatched".equals(method.getName())) {
                                MessageTrace.onMessageEnd((Message) args[1]);
                            }
                            return null;
                        }
                    });
            @SuppressLint("BlockedPrivateApi")
            Field observerField = Looper.class.getDeclaredField("sObserver");
            observerField.setAccessible(true);
            observerField.set(null, observer);
        } catch (Exception e) {
            Log.e(TAG, "monitor message failed: " + e.toString());
            mainLooper.setMessageLogging(x -> {
                if (x.charAt(0) == '>') {
                    MessageTrace.onMessageStart();
                } else if (x.charAt(0) == '<') {
                    MessageTrace.onMessageEnd(null);
                }
            });
        }
    }

    @Override
    public void onCreate() {
        ColdBootLogger.end(ColdBootLogger.COLD_BOOT_APPLICATION_ATTACH_TO_CREATE);
        super.onCreate();
        ColdBootLogger.begin(ColdBootLogger.COLD_BOOT_APPLICATION_CREATE_DURATION);
        Looper looper = Looper.getMainLooper();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            looper.getQueue().addIdleHandler(unreachableIdleHandler);
            looper.getQueue().addIdleHandler(runningOnceIdleHandler);
        }
        monitorMainThreadMessage();

        JavaStack.tryInit();

        Future<Object> job = Executors.newSingleThreadExecutor().submit(() -> {
            Thread.sleep(100);
            return UUID.randomUUID().toString();
        });
        try {
            Object o = job.get();
            System.out.println(o);
        } catch (ExecutionException e) {
            e.printStackTrace();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        for (int i = 0; i < 10; i++) {
            new Thread(() -> {
                synchronized (App.class) {
                    try {
                        Thread.sleep(100);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }).start();
        }
        new Handler().postAtFrontOfQueue(() -> {
            synchronized (App.class) {
                try {
                    Thread.sleep(50);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        });

        Executors.newSingleThreadExecutor().execute(() -> {
            synchronized (App.class) {
                try {
                    Thread.sleep(200);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                App.class.notifyAll();
            }
        });

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            looper.getQueue().removeIdleHandler(unreachableIdleHandler);
        }
        synchronized (App.class) {
            try {
                App.class.wait(1000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        ColdBootLogger.end(ColdBootLogger.COLD_BOOT_APPLICATION_CREATE_DURATION);
        ColdBootLogger.begin(ColdBootLogger.COLD_BOOT_APPLICATION_TO_MAIN);
//        MaliGpuPerfDemo.demo();
        GpuPerfDemo.demo();
    }
}
