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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;

public final class TraceReceiver extends BroadcastReceiver {

    static final String ACTION_RHEA_TRACE_SWITCH = "com.bytedance.rheatrace.switch";

    static final String ACTION_RHEA_TRACE_SWITCH_START = "com.bytedance.rheatrace.switch.start";

    static final String ACTION_RHEA_TRACE_SWITCH_STOP = "com.bytedance.rheatrace.switch.stop";

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        if (action != null) {
            switch (action) {
                case ACTION_RHEA_TRACE_SWITCH:
                    startOrStopTracing(context);
                    break;
                case ACTION_RHEA_TRACE_SWITCH_START:
                    startTracing(context);
                    break;
                case ACTION_RHEA_TRACE_SWITCH_STOP:
                    stopTracing(context);
                    break;
            }
        }
    }

    private void startOrStopTracing(Context context) {
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                if (TraceSwitch.isStarted()) {
                    TraceSwitch.stop(context);
                } else {
                    TraceSwitch.start(context);
                }
            }
        });
    }

    private void startTracing(Context context) {
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                TraceSwitch.start(context);
            }
        });
    }

    private void stopTracing(Context context) {
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                TraceSwitch.stop(context);
            }
        });
    }

}
