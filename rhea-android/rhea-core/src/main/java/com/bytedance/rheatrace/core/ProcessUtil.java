/*
 * MIT License
 *
 * Copyright (c) 2019-present, iQIYI, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
//from https://github.com/iqiyi/Qigsaw

//------ modify by RheaTrace ------

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

import android.app.ActivityManager;
import android.app.Application;
import android.content.Context;
import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.List;

class ProcessUtil {

    static String getProcessName(Context context) {
        String processName = null;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.P) {
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
}
