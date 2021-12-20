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

import android.app.Application;
import android.content.Context;
import android.os.Build;

import androidx.annotation.Keep;

@Keep
@SuppressWarnings("unused")
public final class TraceApplicationLike {

    /**
     * the method would be injected in {@link Application#attachBaseContext(Context)} of your app application.
     *
     * @param context base context of application.
     */
    public static void attachBaseContext(Context context) {
        if (Build.VERSION.SDK_INT < 18) {
            throw new RuntimeException("rhea trace don't support api level lower than 18");
        }
        TraceSwitch.init(context);
    }

    /**
     * the method would be injected in {@link Application#onCreate()} of your app application.
     *
     * @param context  application context.
     */
    public static void onCreate(Context context) {
        //Todo::this method would be used for another purpose, so stay tuned.
    }
}
