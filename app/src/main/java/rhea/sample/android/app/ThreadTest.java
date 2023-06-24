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

import android.content.Context;

import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;

public class ThreadTest {
    private static Context sContext;

    public static void test(Context context) {
        sContext = context;
        new Thread(ThreadTest::extracted).start();
    }

    private static void extracted() {
        while (true) {
            doTest();
            LockSupport.parkNanos(TimeUnit.SECONDS.toNanos(1));
        }
    }

    public static void doTest() {
        if (sContext == null) {
            return;
        }
        try {
            for (int i = 0; i < 10; i++) {
                try {
                    String[] list = sContext.getAssets().list("/");
                    if (list != null) {
                        for (String s : list) {
                            System.out.println(s);
                        }
                    }
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            }
        } finally {
            System.out.println("done");
            LockSupport.parkNanos(TimeUnit.SECONDS.toNanos(3));
        }
    }
}
