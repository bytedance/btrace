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

package com.bytedance.harmony.trace.cli;

import java.util.Collection;

public class ProcessStackList implements IStackListGroup {
    private long sumWallDuration;
    private long sumCpuDuration;
    private final int threadCount;
    private long beginTime = Long.MAX_VALUE;
    private long endTime = Long.MIN_VALUE;

    public ProcessStackList(Collection<ThreadStackList> list) {
        threadCount = list.size();
        for (ThreadStackList threadStackList : list) {
            beginTime = Math.min(beginTime, threadStackList.beginTime());
            endTime = Math.max(endTime, threadStackList.endTime());
            sumWallDuration += threadStackList.wallDuration();
            sumCpuDuration += threadStackList.cpuDuration();
        }
    }

    @Override
    public long wallDuration() {
        return sumWallDuration;
    }

    @Override
    public long cpuDuration() {
        return sumCpuDuration;
    }

    @Override
    public long beginTime() {
        return beginTime;
    }

    @Override
    public long endTime() {
        return endTime;
    }

    public int getThreadCount() {
        return threadCount;
    }
}
