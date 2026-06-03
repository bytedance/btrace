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

import java.util.List;

public class StringUtils {
    public static String join(String[] ar, String j) {
        if (ar == null || ar.length == 0) {
            return "";
        }
        if (j == null) {
            j = "";
        }
        StringBuilder builder = new StringBuilder();
        for (String s : ar) {
            builder.append(s);
            builder.append(j);
        }
        builder.setLength(builder.length() - j.length());
        return builder.toString();
    }

    public static String join(List<String> ar, String j) {
        if (ar == null || ar.isEmpty()) {
            return "";
        }
        if (j == null) {
            j = "";
        }
        StringBuilder builder = new StringBuilder();
        for (String s : ar) {
            builder.append(s);
            builder.append(j);
        }
        builder.setLength(builder.length() - j.length());
        return builder.toString();
    }

    public static void reverse(String[] ss) {
        if (ss == null || ss.length <= 1) {
            return;
        }
        int n = ss.length >> 1;
        int tail = ss.length - 1;
        int i = 0;
        String p;
        while (i < n) {
            p = ss[i];
            ss[i] = ss[tail - i];
            ss[tail - i] = p;
            i++;
        }
    }


    public static String stackJoin(String a, String b) {
        if (a == null || "".equals(a)) {
            return b;
        }
        if (b == null || "".equals(b)) {
            return a;
        }
        return a + "\n" + b;
    }

    public static int size(String[] a) {
        return a == null ? 0 : a.length;
    }

    public static boolean isNumber(String s) {
        if (s == null || s.isEmpty()) return false;
        int length = s.length();
        int start = 0;
        char firstChar = s.charAt(0);
        // 处理符号位
        if (firstChar == '+' || firstChar == '-') {
            if (length == 1) return false; // 只有符号的情况
            start = 1;
        }
        // 逐字符检查数字
        for (int i = start; i < length; i++) {
            if (!isDigit(s.charAt(i))) {
                return false;
            }
        }
        return true;
    }

    private static boolean isDigit(char c) {
        return c >= '0' && c <= '9';
    }
}
