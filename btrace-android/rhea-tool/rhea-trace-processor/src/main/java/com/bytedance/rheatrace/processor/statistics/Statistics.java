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
package com.bytedance.rheatrace.processor.statistics;

import com.bytedance.rheatrace.processor.Adb;
import com.bytedance.rheatrace.processor.core.Arguments;
import com.bytedance.rheatrace.processor.core.Debug;
import com.bytedance.rheatrace.processor.core.Version;

import org.json.JSONObject;

import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.function.Function;


/**
 * To use it for data statistics, you can set up the delivery interface by calling setReporter. By default, no data is collected.
 */
public class Statistics {
    private static Function<String, Integer> reporter;
    private final String hostDeviceId;
    private final String ip;
    private final String application;
    private final long bufferSize;
    private final String categories;
    private final int traceDuration;
    private final String cmdline;
    private final String traceMode;
    private final String result;
    private final String errorMessage;

    public static class MobileInfo {
        private final String deviceId;
        private final String deviceBrand;
        private final String deviceModel;
        private final int deviceOs;
        private final int deviceRam;
        private final String deviceArch;

        public MobileInfo(String deviceId, String deviceBrand, String deviceModel, int deviceOs, int deviceRam, String deviceArch) {
            this.deviceId = deviceId;
            this.deviceBrand = deviceBrand;
            this.deviceModel = deviceModel;
            this.deviceOs = deviceOs;
            this.deviceRam = deviceRam;
            this.deviceArch = deviceArch;
        }
    }

    private final MobileInfo mobileInfo;

    private final String extraMap;
    private final String version = Version.NAME;

    private Statistics(MobileInfo mobileInfo, boolean success, String errorMessage) throws UnknownHostException, SocketException {
        hostDeviceId = getMacAddress();
        ip = getIp();
        Arguments arg = Arguments.get();
        application = arg.appName;
        bufferSize = arg.maxAppTraceBufferSize;
        categories = String.join(",", arg.categories);
        traceDuration = arg.timeInSeconds;
        cmdline = arg.cmdLine;
        traceMode = arg.mode;
        result = success ? "success" : "error";
        this.errorMessage = errorMessage;
        extraMap = "{}";
        this.mobileInfo = mobileInfo;
    }

    public static void setReporter(Function<String, Integer> reporter) {
        Statistics.reporter = reporter;
    }

    private String getIp() throws UnknownHostException {
        try {
            return InetAddress.getLocalHost().getHostAddress();
        } catch (Throwable ignore) {
        }
        return null;
    }

    private static Statistics.MobileInfo getMobileInfo() {
        try {
            String text = Adb.Http.get("?name=deviceInfo");
            JSONObject json = new JSONObject(text);
            return new Statistics.MobileInfo(
                    json.optString("deviceId"),
                    json.optString("brand"),
                    json.optString("model"),
                    json.optInt("os"),
                    json.optInt("ram"),
                    json.optString("arch")
            );
        } catch (Throwable ignore) {
        }
        return null;
    }

    public static void safeSend(boolean success, String errorMessage) {
        try {
            String body = new Statistics(getMobileInfo(), success, errorMessage).toJSON();
            if (reporter != null) {
                reporter.apply(body);
            }
        } catch (Throwable e) {
            if (Debug.isDebug()) {
                e.printStackTrace();
            }
        }
    }

    private String getMacAddress() {
        try {
            InetAddress localHost = InetAddress.getLocalHost();
            NetworkInterface ni = NetworkInterface.getByInetAddress(localHost);
            byte[] hardwareAddress = ni.getHardwareAddress();
            String[] hexadecimal = new String[hardwareAddress.length];
            for (int i = 0; i < hardwareAddress.length; i++) {
                hexadecimal[i] = String.format("%02X", hardwareAddress[i]);
            }
            return String.join("-", hexadecimal);
        } catch (Throwable ignore) {
            return null;
        }
    }

    public String toJSON() {
        JSONObject json = new JSONObject();
        json.put("host_device_id", hostDeviceId);
        json.put("ip", ip);
        json.put("application", application);
        json.put("buffer_size", bufferSize);
        json.put("categories", categories);
        json.put("trace_duration", traceDuration);
        json.put("cmdline", cmdline);
        json.put("trace_mode", traceMode);
        json.put("result", result);
        json.put("error_message", errorMessage);
        json.put("extra_map", extraMap);
        json.put("version", version);
        if (mobileInfo != null) {
            json.put("mobile_device_id", mobileInfo.deviceId);
            json.put("mobile_device_brand", mobileInfo.deviceBrand);
            json.put("mobile_device_model", mobileInfo.deviceModel);
            json.put("mobile_device_os", mobileInfo.deviceOs);
            json.put("mobile_device_ram", mobileInfo.deviceRam);
            json.put("device_arch", mobileInfo.deviceArch);
        }
        return json.toString();
    }
}
