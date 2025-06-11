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

import com.bytedance.rheatrace.trace.base.TraceConfig;
import com.bytedance.rheatrace.trace.base.TraceConfigCreator;
import com.bytedance.rheatrace.trace.base.TraceMeta;

public class TraceConfigurations {
    private static final TraceConfig[] configurations = new TraceConfig[TraceMeta.Max.getOffset()];


    @SuppressWarnings("unchecked")
    public static  <T extends TraceConfig> T getConfig(TraceMeta meta) {
        T config = (T) configurations[meta.getOffset()];
        if (config == null) {
            try {
                TraceConfigCreator<T> creator = (TraceConfigCreator<T>) meta.getCreatorType().newInstance();
                config = creator.create();
                configurations[meta.getOffset()] = config;
            } catch (Exception e) {
                return null;
            }
        } else {
            TraceConfigCreator<T> creator = config.getCreator();
            creator.update(config);
        }
        return config;
    }
}