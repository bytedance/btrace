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
package com.bytedance.rheatrace.trace.sampling;

import com.bytedance.rheatrace.trace.base.TraceConfig;

public class SamplingConfig extends TraceConfig {

    public static final int OFFLINE_BUFFER_SIZE_DEFAULT = 200_000;

    public static final long OFFLINE_JAVA_SAMPLE_INTERVAL_DEFAULT = 1000_000;

    private int bufferSize;
    private long mainThreadIntervalNs;
    private long otherThreadIntervalNs;
    private int clockType; // 0: CLOCK_MONOTONIC, 1: CLOCK_MONOTONIC_COARSE
    private int stackWalkKind; // 0: kIncludeInlinedFrames, 1: kSkipInlinedFrames
    private boolean disableObjectAllocation; // 开启对象监控抓栈疑似性能较大，支持可配置；
    private boolean enableRusage; // 开启 rusage 相关数据统计，包含 majflt/nvcsw/nivcsw
    private boolean enableWakeup; // 开启锁、park、wait 唤醒监控
    private boolean enableThreadNames; // 是否采集线程名称
    private boolean shadowPause;

    public SamplingConfig(SamplingConfigCreator creator) {
        super(creator);
    }

    public int getBufferSize() {
        return bufferSize;
    }

    public void setBufferSize(int bufferSize) {
        this.bufferSize = bufferSize;
    }

    public long getMainThreadIntervalNs() {
        return mainThreadIntervalNs;
    }

    public void setMainThreadIntervalNs(long mainThreadIntervalNs) {
        this.mainThreadIntervalNs = mainThreadIntervalNs;
    }

    public long getOtherThreadIntervalNs() {
        return otherThreadIntervalNs;
    }

    public void setOtherThreadIntervalNs(long otherThreadIntervalNs) {
        this.otherThreadIntervalNs = otherThreadIntervalNs;
    }

    public int getClockType() {
        return clockType;
    }

    public void setClockType(int clockType) {
        this.clockType = clockType;
    }

    public int getStackWalkKind() {
        return stackWalkKind;
    }

    public void setStackWalkKind(int stackWalkKind) {
        this.stackWalkKind = stackWalkKind;
    }

    public boolean isDisableObjectAllocation() {
        return disableObjectAllocation;
    }

    public void setDisableObjectAllocation(boolean disableObjectAllocation) {
        this.disableObjectAllocation = disableObjectAllocation;
    }

    public boolean isEnableRusage() {
        return enableRusage;
    }

    public void setEnableRusage(boolean enableRusage) {
        this.enableRusage = enableRusage;
    }

    public boolean isEnableWakeup() {
        return enableWakeup;
    }

    public void setEnableWakeup(boolean enableWakeup) {
        this.enableWakeup = enableWakeup;
    }

    public boolean isEnableThreadNames() {
        return enableThreadNames;
    }

    public void setEnableThreadNames(boolean enableThreadNames) {
        this.enableThreadNames = enableThreadNames;
    }

    public boolean isShadowPause() {
        return shadowPause;
    }

    public void setShadowPause(boolean shadowPause) {
        this.shadowPause = shadowPause;
    }

    @Override
    public long[] deflate() {
        long[] results = new long[10];
        results[0] = bufferSize;
        results[1] = mainThreadIntervalNs;
        results[2] = otherThreadIntervalNs;
        results[3] = clockType;
        results[4] = stackWalkKind;
        results[5] = disableObjectAllocation ? 0 : 1;
        results[6] = enableRusage ? 1 : 0;
        results[7] = enableWakeup ? 1 : 0;
        results[8] = enableThreadNames ? 1 : 0;
        results[9] = shadowPause? 1 : 0;
        return results;
    }

    @Override
    public long[] deflateUpdatable() {
        long[] results = new long[2];
        results[0] = mainThreadIntervalNs;
        results[1] = otherThreadIntervalNs;
        return results;
    }
}