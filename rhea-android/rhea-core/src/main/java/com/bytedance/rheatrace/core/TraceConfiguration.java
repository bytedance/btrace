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

package com.bytedance.rheatrace.core;

public final class TraceConfiguration {

    static final long MIN_BUFFER_SIZE = 10000;

    static final long MAX_BUFFER_SIZE = 5000000;

    static final long DEFAULT_BUFFER_SIZE = 100000;

    final boolean enableIO;

    final boolean mainThreadOnly;

    final boolean enableMemory;

    final boolean enableClassLoad;

    final boolean startWhenAppLaunch;

    final long atraceBufferSize;

    final String blockHookLibs;

    public TraceConfiguration(Builder builder) {
        this.enableIO = builder.enableIO;
        this.mainThreadOnly = builder.mainThreadOnly;
        this.enableMemory = builder.enableMemory;
        this.enableClassLoad = builder.enableClassLoad;
        this.startWhenAppLaunch = builder.startWhenAppLaunch;
        this.atraceBufferSize = builder.atraceBufferSize;
        this.blockHookLibs = builder.blockHookLibs;
    }

    public void checkConfig() {
        if (atraceBufferSize < MIN_BUFFER_SIZE) {
            throw new IllegalArgumentException("atraceBufferSize must be greater than 10000");
        }
        if (atraceBufferSize > MAX_BUFFER_SIZE) {
            throw new IllegalArgumentException("atraceBufferSize must be less than 5000000");
        }
    }

    @Override
    public String toString() {
        return "RheaConfig{" +
                "io=" + enableIO +
                ", mainThreadOnly=" + mainThreadOnly +
                ", memory=" + enableMemory +
                ", classLoad=" + enableClassLoad +
                ", startWhenAppLaunch=" + startWhenAppLaunch +
                ", atraceBufferSize=" + atraceBufferSize +
                ", blockHookLibs='" + blockHookLibs + '\'' +
                '}';
    }

    public static final class Builder {

        private boolean enableIO = true;

        private boolean mainThreadOnly = false;

        private boolean enableMemory = false;

        private boolean enableClassLoad = false;

        private boolean startWhenAppLaunch = true;

        private long atraceBufferSize = DEFAULT_BUFFER_SIZE;

        private String blockHookLibs;

        public Builder enableIO(boolean enableIO) {
            this.enableIO = enableIO;
            return this;
        }

        public Builder enableMemory(boolean enableMemory) {
            this.enableMemory = enableMemory;
            return this;
        }

        public Builder enableClassLoad(boolean enableClassLoad) {
            this.enableClassLoad = enableClassLoad;
            return this;
        }

        public Builder setMainThreadOnly(boolean mainThreadOnly) {
            this.mainThreadOnly = mainThreadOnly;
            return this;
        }

        public Builder startWhenAppLaunch(boolean startWhenAppLaunch) {
            this.startWhenAppLaunch = startWhenAppLaunch;
            return this;
        }

        public Builder setATraceBufferSize(long atraceBufferSize) {
            if (atraceBufferSize > 0) {
                this.atraceBufferSize = atraceBufferSize;
            }
            return this;
        }

        public Builder setBlockHookLibs(String blockHookLibs) {
            this.blockHookLibs = blockHookLibs;
            return this;
        }

        public TraceConfiguration build() {
            return new TraceConfiguration(this);
        }
    }
}
