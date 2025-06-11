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
package com.bytedance.rheatrace.trace;

import androidx.annotation.NonNull;

import com.bytedance.rheatrace.trace.base.TraceAbility;
import com.bytedance.rheatrace.trace.base.TraceMeta;

import java.util.ArrayList;
import java.util.List;

public class TraceAbilityCenter {

    private static final TraceAbility<?>[] abilities = new TraceAbility[TraceMeta.Max.getOffset()];

    @NonNull
    public synchronized static List<TraceAbility<?>> getAbilities(List<TraceMeta> flags) {
        ArrayList<TraceAbility<?>> results = new ArrayList<>();
        for (TraceMeta flag : flags) {
            TraceAbility<?> ability = abilities[flag.getOffset()];
            if (ability == null) {
                try {
                    ability = flag.getType().newInstance();
                } catch (IllegalAccessException | InstantiationException e) {
                    throw new RuntimeException(e);
                }
                abilities[flag.getOffset()] = ability;
            }
            results.add(ability);
        }
        return results;
    }

}