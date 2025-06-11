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

import java.io.IOException;

public interface SystemLevelCapture {
    void start(String[] sysArgs) throws IOException;

    void stop();

    void print(boolean withError, PrintListener listener) throws IOException;

    void waitForExit() throws InterruptedException;

    void process() throws IOException, InterruptedException;

    void cleanup();

    interface PrintListener {
        void onPrint(String msg);
    }
}