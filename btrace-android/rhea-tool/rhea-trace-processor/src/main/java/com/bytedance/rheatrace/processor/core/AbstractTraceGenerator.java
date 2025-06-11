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
package com.bytedance.rheatrace.processor.core;

import com.bytedance.rheatrace.processor.trace.ATrace;
import com.bytedance.rheatrace.processor.trace.Frame;

import java.io.IOException;
import java.util.List;

/**
 * Created by caodongping on 2023/2/16
 *
 * @author caodongping@bytedance.com
 */
public abstract class AbstractTraceGenerator {

    public void generate() throws IOException {
        List<Frame> appTraceFrames = getAppTraceFrames();
        merge(appTraceFrames, AppTraceProcessor.getATrace());
    }

    protected abstract void merge(List<Frame> appTraceFrames, List<ATrace> aTrace) throws IOException;

    protected final List<Frame> getAppTraceFrames() throws IOException {
        return AppTraceProcessor.getBinaryTrace(getSystemFTraceTime());
    }

    protected abstract long getSystemFTraceTime();
}
