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

import com.bytedance.rheatrace.processor.Main;

import java.util.jar.Manifest;

public class Version {
    public static final String NAME = resolveVersion();

    private static String resolveVersion() {
        try {
            Manifest mf = new Manifest();
            mf.read(Main.class.getClassLoader().getResourceAsStream("META-INF/MANIFEST.MF"));
            return mf.getMainAttributes().getValue("Manifest-Version");
        } catch (Throwable e) {
            return "-1";
        }
    }
}
