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

package com.bytedance.rheatrace.atrace;

import android.os.Trace;
import android.util.Log;

import com.bytedance.rheatrace.common.ReflectUtil;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

final class TraceEnableTagsHelper {

    static final String TAG = "rhea:enable_tags";

    static Method nativeGetEnabledTags_method = null;

    static Field sEnabledTags = null;

    static boolean updateEnableTags() {
        if (nativeGetEnabledTags_method == null) {
            try {
                nativeGetEnabledTags_method = ReflectUtil.INSTANCE.getDeclaredMethodRecursive(Trace.class, "nativeGetEnabledTags");
            } catch (Throwable e) {
                Log.d(TAG, "failed to reflect method nativeGetEnabledTags of Trace.class!");
                return false;
            }
        }
        if (sEnabledTags == null) {
            try {
                sEnabledTags = ReflectUtil.INSTANCE.getDeclaredFieldRecursive(Trace.class, "sEnabledTags");
            } catch (Throwable e) {
                Log.d(TAG, "failed to reflect filed sEnabledTags of Trace.class!");
                return false;
            }
        }
        try {
            sEnabledTags.set(null, nativeGetEnabledTags_method.invoke(null));
        } catch (Throwable e) {
            Log.d(TAG, "failed to update enable tags for Trace.class!");
            return false;
        }
        return true;
    }
}
