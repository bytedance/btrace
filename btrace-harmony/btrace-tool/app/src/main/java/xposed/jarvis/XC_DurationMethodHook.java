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

import android.os.SystemClock;

import de.robv.android.xposed.XC_MethodHook;

public abstract class XC_DurationMethodHook extends XC_MethodHook {
    @Override
    protected final void beforeHookedMethod(MethodHookParam param) throws Throwable {
        param.setObjectExtra("time", SystemClock.uptimeMillis());
    }

    @Override
    protected final void afterHookedMethod(MethodHookParam param) throws Throwable {
        long duration = SystemClock.uptimeMillis() - (long) param.getObjectExtra("time");
        afterHookedMethod(param, duration);
    }

    protected abstract void afterHookedMethod(MethodHookParam param, long duration) throws Throwable;
}
