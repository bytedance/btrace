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

public class NativeStackElement {

    private final String module;
    private final NativeStackMethod method;

    public NativeStackElement(String module, NativeStackMethod method) {
        this.module = module;
        this.method = method;
    }

    public static NativeStackElement parse(String symbol) {
        int soEndIndex = symbol.indexOf('>');
        String soName = symbol.substring(1, soEndIndex);
        String methodName = symbol.substring(soEndIndex + 2, symbol.length() - 1);
        return new NativeStackElement(soName, NativeStackMethod.parse(soName, methodName));
    }

    public boolean isJavaMethod() {
        return method.isJavaMethod();
    }

    public boolean needRetrace() {
        return method.needRetrace();
    }

    public String getMethodName() {
        return method.getMethodName();
    }

    public String getModule() {
        return module;
    }
}
