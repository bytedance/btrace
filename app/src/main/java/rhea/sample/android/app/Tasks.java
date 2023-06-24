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
package rhea.sample.android.app;

import android.os.Trace;

import java.util.concurrent.locks.LockSupport;

/**
 * Created by caodongping on 2023/2/10
 *
 * @author caodongping@bytedance.com
 */
public class Tasks {
    public static void doTasks() {
        new Thread(() -> {
            Trace.beginSection("doTask-thread-thread");
            doTask0();
            doTask1();
            Trace.endSection();
            new Thread(() -> {
                Trace.beginSection("doTask-thread-thread");
                doTask2();
                doTask3();
                Trace.endSection();
            }).start();
        }).start();
    }

    private static void doTask0() {
        Trace.beginSection("doTask0");
        LockSupport.parkNanos(100000000);
        Trace.endSection();
    }

    private static void doTask1() {
        Trace.beginSection("doTask1");
        LockSupport.parkNanos(200000000);
        Trace.endSection();
    }

    private static void doTask2() {
        Trace.beginSection("doTask2");
        LockSupport.parkNanos(200000000);
        ThreadTest.doTest();
        Trace.endSection();
    }

    private static void doTask3() {
        Trace.beginSection("doTask3");
        LockSupport.parkNanos(200000000);
        Trace.endSection();
    }
}
