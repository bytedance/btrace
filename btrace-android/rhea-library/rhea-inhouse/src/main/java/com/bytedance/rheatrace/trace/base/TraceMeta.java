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
package com.bytedance.rheatrace.trace.base;

import com.bytedance.rheatrace.trace.sampling.SamplingConfigCreator;
import com.bytedance.rheatrace.trace.sampling.SamplingTrace;

/**
 * Holding the meta information of every currently supported kinds of traceable perf data.
 */
public enum TraceMeta {

    Sampling("sampling",true, 0, SamplingTrace.class, SamplingConfigCreator.class),
    Max("invalid", false, 1, null, null);


    private final String name;
    // extra information will be added only into core perf data dump file.
    private final boolean core;
    // offset of this meta kind in TraceAbilityCenter.abilities array.
    private final int offset;
    private final Class<? extends TraceAbility<? extends TraceConfig>> type;
    private final Class<? extends TraceConfigCreator<? extends TraceConfig>> creator;

    TraceMeta(String name, boolean core, int offset,
              Class<? extends TraceAbility<? extends TraceConfig>> type,
              Class<? extends TraceConfigCreator<? extends TraceConfig>> creator) {
        this.name = name;
        this.core = core;
        this.offset = offset;
        this.type = type;
        this.creator = creator;
    }

    public String getName() {
        return name;
    }

    public boolean isCore() {
        return core;
    }

    public int getOffset() {
        return offset;
    }

    public Class<? extends TraceAbility<? extends TraceConfig>> getType() {
        return type;
    }

    public Class<? extends TraceConfigCreator<? extends TraceConfig>> getCreatorType() {
        return creator;
    }
}