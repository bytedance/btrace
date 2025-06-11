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

public class MethodSymbol {
    public final long ptr;
    public final long globalID;
    public String symbol;

    public MethodSymbol(long ptr, long globalID, String symbol) {
        this.ptr = ptr;
        this.globalID = globalID;
        this.symbol = symbol;
    }

    public String symbol() {
        return symbol;
    }
}