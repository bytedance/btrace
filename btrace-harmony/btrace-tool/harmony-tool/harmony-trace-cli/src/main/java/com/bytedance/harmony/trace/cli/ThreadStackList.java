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

import java.util.ArrayList;
import java.util.List;

public class ThreadStackList implements IStackListGroup {
    public final int tid;

    public final List<StackList> main = new ArrayList<>();
    @Deprecated
    public final List<StackList> cpp = new ArrayList<>();
    public int cpuRank;
    public int javaAllocRank;
    public int cppAllocRank;
    public int majFltRank;
    public int nvCswRank;
    public int nivCswRank;
    public int inBlockRank;
    public int ouBlockRank;

    public ThreadStackList(int tid) {
        this.tid = tid;
    }

    @Override
    public long cpuDuration() {
        if (main.isEmpty()) {
            return 0;
        } else {
            return main.get(main.size() - 1).nanoCPUTime - main.get(0).nanoCPUTime;
        }
    }

    @Override
    public long beginTime() {
        if (main.isEmpty()) {
            return 0;
        }
        return main.get(0).nanoTime;
    }

    @Override
    public long endTime() {
        if (main.isEmpty()) {
            return 0;
        }
        return main.get(main.size() - 1).nanoTime;
    }

    @Override
    public long wallDuration() {
        return endTime() - beginTime();
    }

    public CallNode asCallNode() {
        return StackListEncoder.decodeThread(main);
    }
}
