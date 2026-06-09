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

package xposed.jarvis;

import static com.bytedance.jarvis.core.deliver.HostEnv.BOOT_TYPE;
import static com.bytedance.jarvis.core.deliver.HostEnv.BUILD_TIME;

import android.annotation.SuppressLint;
import android.app.Application;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.util.Log;

import com.bytedance.jarvis.core.Jarvis;
import com.bytedance.jarvis.core.config.PackedMonitorConfig;
import com.bytedance.jarvis.core.deliver.FileDelivery;
import com.bytedance.jarvis.core.deliver.HostEnv;
import com.bytedance.jarvis.core.scene.monitor.AppStartMonitor;
import com.bytedance.jarvis.core.scene.monitor.ContinuousDropMonitorConfig;
import com.bytedance.jarvis.core.scene.monitor.CrashMonitorConfig;
import com.bytedance.jarvis.core.scene.monitor.ScreenRecordMonitorConfig;
import com.bytedance.jarvis.core.scene.monitor.ddpf.DDPFMonitorConfig;
import com.bytedance.jarvis.cpu.CpuMonitorConfig;
import com.bytedance.jarvis.experiencemap.UserExpMapMonitorConfig;
import com.bytedance.jarvis.ext.applog.AppLogDeliver;
import com.bytedance.jarvis.memory.monitor.MemMonitorConfig;
import com.bytedance.jarvis.scene.AnrMonitorConfig;
import com.bytedance.jarvis.scene.AppStartMonitorConfig;
import com.bytedance.jarvis.scene.FeedbackMonitorConfig;
import com.bytedance.jarvis.scene.GeneralMonitorConfig;
import com.bytedance.jarvis.scene.SpikeAllocationMonitorConfig;
import com.bytedance.jarvis.scene.SpikeThreadHighLoadMonitorConfig;
import com.bytedance.jarvis.scene.VideoPlayMonitorConfig;
import com.bytedance.jarvis.trace.fps.JankFrameMonitorConfig;
import com.bytedance.jarvis.trace.fps.JankMessageMonitorConfig;
import com.bytedance.jarvis.trace.fps.expmap.FpsMonitorConfig;
import com.bytedance.jarvis.trace.metrics.MetricsMonitorConfig;
import com.bytedance.jarvis.trace.stack.SamplingMonitorConfig;

import java.io.File;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import sample.android.jarvis.FileUtils;

public class JarvisInside {
    private static final String TAG = "jarvisx";
    private final Application application;

    public JarvisInside(Application application) {
        this.application = application;
    }

    public void init() {
        initJarvis(makeConfig());
    }

    private void initJarvis(PackedMonitorConfig config) {
        Log.i(TAG, "init jarvis 1");
        HostEnv env = HostEnv.newBuilder().setAppLaunchTime(System.currentTimeMillis())
                .setBootTypeSupplier(key -> {
                    switch (key) {
                        case BOOT_TYPE:
                            return "main";
                        case BUILD_TIME:
                            return "1234";
                    }
                    return null;
                })
                .setBootSource("main")
                .setUpdateVersionCode("100")
                .setHostAbiBit(64)
                .setAppLogHandler((event, param) -> {
                    Log.e(TAG, event + " " + Thread.currentThread().getName());
                    Log.e(TAG, param == null ? "null" : param.toString());
                })
                .build();
        Log.i(TAG, "init jarvis 2");
        ExecutorService executorService = Executors.newFixedThreadPool(Runtime.getRuntime().availableProcessors());
        Log.i(TAG, "init jarvis 3");
        Jarvis.init(application, config, new AppLogDeliver(env, true), executorService, (path, id, time) -> {
            Log.e(TAG, "send " + path);
            try {
                @SuppressLint("SimpleDateFormat") String timeStr = new SimpleDateFormat("yyyyMMdd_HHmmss.SSS").format(time);
                File target = new File(new File(application.getExternalFilesDir(""), "cprf"), String.valueOf(Process.myPid()));
                boolean mkdirs = target.mkdirs();
                File file = new File(target, id + "_" + timeStr + ".zip");
                File dir = new File(path);
                FileUtils.zipDirToFile(dir, file);
                FileDelivery.remove(dir);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }, null, null, callback -> {
            Log.d("shenyunlong", ">>> ddpf callback = " + callback);
        }, () -> false);
        Log.i(TAG, "init jarvis 4");
        AppStartMonitor.INSTANCE.start();
        new Handler(Looper.getMainLooper()).postDelayed(new Runnable() {
            @Override
            public void run() {
                AppStartMonitor.INSTANCE.stop();
                Log.i(TAG, "AppStartMonitor.stop done");
            }
        }, 5000);
        Log.i(TAG, "app start jarvis done");
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
//        samplingMonitorConfig.setAtraceBufferSize(1024);
//        samplingMonitorConfig.setAtraceTags(9);
        samplingMonitorConfig.setMainThreadInterval(1);
        samplingMonitorConfig.setMainThreadMode(false);
        samplingMonitorConfig.setOtherThreadInterval(1);
        samplingMonitorConfig.setNativeSamplingRate(1);
        samplingMonitorConfig.setJavaAllocStatMode(1);
        samplingMonitorConfig.setJavaThreadStateBufferSize(1024);
        samplingMonitorConfig.setNativeSamplingType(1);
        samplingMonitorConfig.setMainThreadNativeInterval(1);
        samplingMonitorConfig.setOtherThreadNativeInterval(1);
        samplingMonitorConfig.setClockType(2);
        samplingMonitorConfig.setClockResolution(1);
        samplingMonitorConfig.setStackWalkKind(samplingMonitorConfig.getClockType());
        samplingMonitorConfig.setEnableRusage(true);
        samplingMonitorConfig.setEnableWakeup(true);
        samplingMonitorConfig.setEnableThreadNames(true);
        samplingMonitorConfig.setThreadStateInterval(16);
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
                .build();
    }

}
