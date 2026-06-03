/*
 * Copyright (c) 2026 Bytedance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.bytedance.harmony.trace.cli;

import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.function.Function;

public class Statistics {
    private static Function<String, Void> reporter;
    private final boolean success;
    private final String userEmail;
    private final String bundleId;

    public static class MobileInfo {
        private static String osVersionKey = "const.ohos.fullname";
        private static String deviceTypeKey = "const.product.model";
        private static String buildVersionKey = "const.product.software.version";

        private final String deviceId;
        private final String deviceType;
        private final String osVersion;
        private final String buildVersion;

        public MobileInfo(String deviceId, String deviceType, String osVersion, String buildVersion) {
            this.deviceId = deviceId;
            this.deviceType = deviceType;
            this.osVersion = osVersion;
            this.buildVersion = buildVersion;
        }
    }

    private final MobileInfo mobileInfo;

    private Statistics(MobileInfo mobileInfo, String userEmail, String bundleId, boolean success) {
        this.userEmail = userEmail;
        this.success = success;
        this.mobileInfo = mobileInfo;
        this.bundleId = bundleId;
    }

    public static void setReporter(Function<String, Void> reporter) {
        Statistics.reporter = reporter;
    }

    private static String getUserEmail() {
        StringBuilder result = new StringBuilder();

        try {
            ProcessBuilder pb = new ProcessBuilder("git", "config", "--global", "user.email");// ignore_security_alert [ByDesign12.1]UsingProcessBuilder
            pb.redirectErrorStream(true);

            Process process = pb.start();
            BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
            String line;

            while ((line = reader.readLine()) != null) {
                result.append(line);
            }

            int exitCode = process.waitFor();

        } catch (IOException | InterruptedException e) {
        }

        return result.toString();
    }

    private static MobileInfo getMobileInfo() {
        try {
            String osVersion = "";
            String deviceType = "";
            String deviceId = "";
            String buildVersion = "";

            String deviceUUidStr = Hdc.callString("shell", "bm", "get", "-u");
            String[] deviceUUidStrList = deviceUUidStr.split("\n");

            if (0 < deviceUUidStrList.length) {
                deviceId = deviceUUidStrList[deviceUUidStrList.length-1];
            }

            String deviceParamStr = Hdc.callString("shell", "param", "get");
            String[] deviceParamStrList = deviceParamStr.split("\n");

            for (String paramInfoStr : deviceParamStrList) {
                String[] paramInfoList = paramInfoStr.split("=");

                if (paramInfoList.length != 2) {
                    continue;
                }

                String paramKey = paramInfoList[0].strip();
                String paramVal = paramInfoList[1].strip();

                if (paramKey.equals(MobileInfo.osVersionKey)) {
                    osVersion = paramVal;
                } else if (paramKey.equals(MobileInfo.deviceTypeKey)) {
                    deviceType = paramVal;
                } else if (paramKey.equals(MobileInfo.buildVersionKey)) {
                    buildVersion = paramVal;
                }
            }
            return new MobileInfo(deviceId, deviceType, osVersion, buildVersion);
        } catch (Throwable ignore) {
        }
        return null;
    }

    public static void safeSend(String bundldId, boolean success) {
        try {
            String body = new Statistics(getMobileInfo(), getUserEmail(), bundldId, success).toJSON();

            if (reporter != null) {
                reporter.apply(body);
            }
        } catch (Throwable e) {
//            if (Debug.isDebug()) {
//                e.printStackTrace();
//            }
        }
    }

    public String toJSON() {
        JSONObject json = new JSONObject();
        json.put("success", success);
        json.put("bundle_id", bundleId);
        json.put("user_email", userEmail);
        json.put("os_type", "harmony");

        if (mobileInfo != null) {
            json.put("device_id", mobileInfo.deviceId);
            json.put("device_type", mobileInfo.deviceType);
            json.put("os_version", mobileInfo.osVersion);
            json.put("build_version", mobileInfo.buildVersion);
        }

        return json.toString();
    }
}
