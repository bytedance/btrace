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
package com.bytedance.rheatrace.utils;

import android.app.ActivityManager;
import android.app.Application;
import android.content.Context;
import android.os.Build;
import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.List;

public class ProcessUtils {

    private volatile static String sProcessName;

    public static boolean isMainProcess(Context context) {
        return context.getPackageName().equals(getProcessName(context));
    }

    private static String getProcessName(Context context) {
        if (sProcessName != null) {
            return sProcessName;
        }
        String processName = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            processName = Application.getProcessName();
        }
        if (TextUtils.isEmpty(processName)) {
            try {
                processName = getProcessNameClassical(context);
            } catch (Exception ignored) {
                //may be occur DeadSystemException
            }
        }
        if (TextUtils.isEmpty(processName)) {
            processName = getProcessNameSecure();
        }
        sProcessName = processName;
        return processName;
    }

    private static String getProcessNameClassical(Context context) {
        String processName = "";
        int pid = android.os.Process.myPid();
        ActivityManager manager = (ActivityManager) context.getSystemService(
                Context.ACTIVITY_SERVICE);
        if (manager == null) {
            return processName;
        }
        List<ActivityManager.RunningAppProcessInfo> appProcessList = manager
                .getRunningAppProcesses();
        if (appProcessList == null) {
            return processName;
        }
        for (ActivityManager.RunningAppProcessInfo process : appProcessList) {
            if (process.pid == pid) {
                processName = process.processName;
            }
        }
        return processName;
    }

    private static String getProcessNameSecure() {
        String processName = "";
        try {
            File file = new File("/proc/" + android.os.Process.myPid() + "/" + "cmdline");
            BufferedReader bufferedReader = new BufferedReader(new FileReader(file));
            processName = bufferedReader.readLine().trim();
            bufferedReader.close();
        } catch (Exception e) {
            //
        }
        return processName;
    }
}