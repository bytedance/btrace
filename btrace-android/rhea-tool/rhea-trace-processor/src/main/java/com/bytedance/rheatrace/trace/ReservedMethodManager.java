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
package com.bytedance.rheatrace.trace;

public class ReservedMethodManager {
    public static final String METHOD_GC = "void Heap.WaitForGcToCompleteLocked()";
    public static final String METHOD_LOCK = "void Monitor:Lock()";
    private static final MethodSymbol gc;
    private static final MethodSymbol lock;

    static {
        long ptr = Long.MAX_VALUE;
        gc = new MethodSymbol(ptr--, 0, METHOD_GC);
        lock = new MethodSymbol(ptr, 0, METHOD_LOCK);
    }


    public static MethodSymbol gc() {
        return gc;
    }

    public static MethodSymbol lock() {
        return lock;
    }
}