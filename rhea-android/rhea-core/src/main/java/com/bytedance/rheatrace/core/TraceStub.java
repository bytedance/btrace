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

import java.util.Arrays;

import androidx.annotation.Keep;

@Keep
@SuppressWarnings("unused")
public class TraceStub {

    /**
     * the method would be injected in the enter of app methods.
     *
     * @param methodId method id.
     */
    public static void i(String methodId) {
        RheaTrace.t(methodId);
    }

    /**
     * the method would be injected in the exits of app methods.
     *
     * @param methodId method id.
     */
    public static void o(String methodId) {
        RheaTrace.t(methodId);
    }

    /**
     * Supports passing in the specified parameters at the entry method
     *
     * @param methodId method id
     * @param object   array of params values
     */
    public static void i(String methodId, Object... object) {
        RheaTrace.t(methodId + Arrays.toString(object));
    }

    /**
     * Supports passing in the specified parameters at the exit method
     *
     * @param methodId method id
     * @param object   array of params values
     */
    public static void o(String methodId, Object... object) {
        RheaTrace.t(methodId + Arrays.toString(object));
    }
}
