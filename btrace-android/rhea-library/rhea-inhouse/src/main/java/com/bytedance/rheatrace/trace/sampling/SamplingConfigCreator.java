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

import com.bytedance.rheatrace.prop.TraceProperties;
import com.bytedance.rheatrace.trace.base.TraceConfigCreator;

public class SamplingConfigCreator implements TraceConfigCreator<SamplingConfig> {

    @Override
    public SamplingConfig create() {
        SamplingConfig config = new SamplingConfig(this);
        config.setBufferSize(TraceProperties.getCoreBufferSizeOrDefault(SamplingConfig.OFFLINE_BUFFER_SIZE_DEFAULT));
        long intervalNs = TraceProperties.getSampleIntervalOrDefault(SamplingConfig.OFFLINE_JAVA_SAMPLE_INTERVAL_DEFAULT);
        config.setMainThreadIntervalNs(intervalNs);
        config.setOtherThreadIntervalNs(intervalNs);
        config.setClockType(2); // boottime
        config.setStackWalkKind(0);
        config.setEnableRusage(true);
        config.setDisableObjectAllocation(false);
        config.setEnableWakeup(true);
        config.setEnableThreadNames(true);
        config.setShadowPause(true);
        return config;
    }

    @Override
    public void update(SamplingConfig config) {
        long intervalNs = TraceProperties.getSampleIntervalOrDefault(SamplingConfig.OFFLINE_JAVA_SAMPLE_INTERVAL_DEFAULT);
        config.setMainThreadIntervalNs(intervalNs);
        config.setOtherThreadIntervalNs(intervalNs);
    }
}