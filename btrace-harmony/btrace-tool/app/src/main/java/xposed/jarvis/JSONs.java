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

package xposed.jarvis;

import android.annotation.SuppressLint;

import org.json.JSONTokener;

import java.lang.reflect.Field;

@SuppressLint("DiscouragedPrivateApi")
public class JSONs {

    private static Field inField = null;

    static {
        try {
            inField = JSONTokener.class.getDeclaredField("in");
            inField.setAccessible(true);
        } catch (Exception ignore) {
        }
    }

    public static String asString(JSONTokener token) {
        if (inField == null) {
            return null;
        }
        try {
            Object in = inField.get(token);
            if (in instanceof String) {
                return (String) in;
            }
        } catch (IllegalAccessException ignore) {
        }
        return null;
    }
}
