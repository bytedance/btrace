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


import java.util.ArrayList;
import java.util.List;

public class MethodSymbol {
    public static class NativeReTracedResult {
        public final String library;
        public final String symbol;
        public long address;
        public final List<String> retracedSymbol = new ArrayList<>();
        public final List<String> demangledSymbol = new ArrayList<>();

        public NativeReTracedResult(String library, String symbol) {
            this.library = library;
            this.symbol = symbol;
            if (isHex(symbol)) {
                try {
                    this.address = Long.parseLong(symbol, 16);
                } catch (Throwable e) {
                    this.address = 0;
                }
            } else {
                this.address = 0;
            }
        }
    }

    public static final int TYPE_PURE_JAVA = 0;
    public static final int TYPE_NATIVE_CPP = 1;
    public static final int TYPE_NATIVE_JNI = 2;
    public static final int TYPE_NATIVE_APP_JAVA = 3;
    public static final int TYPE_NATIVE_SYSTEM_JAVA = 4;
    public static final int TYPE_NATIVE_JAVA_UNKNOWN = 5;

    public final long ptr;
    public long globalID;
    public final boolean isReservedTypeSymbol;
    public String symbol;
    public int type;
    public int kind;
    public int methodCategory;
    public boolean symbolRevised = false;
    public String raw;
    public NativeReTracedResult nativeReTracedResult;

    public MethodSymbol(long ptr, long globalID, String symbol, short kind) {
        this(ptr, globalID, symbol, false);
        this.kind = kind;
    }

    public MethodSymbol(long ptr, long globalID, String symbol) {
        this(ptr, globalID, symbol, false);
    }

    public MethodSymbol(long ptr, long globalID, String symbol, boolean isReservedTypeSymbol) {
        this.ptr = ptr;
        try {
            int last = symbol.lastIndexOf("@");
            if (last < 0) {
                // v1 没有 @
                this.symbol = symbol;
                this.globalID = globalID;
                this.raw = symbol;
            } else {
                String maybeId = symbol.substring(last + 1);
                if (StringUtils.isNumber(maybeId)) {
                    // v2 符号@ID
                    this.symbol = symbol.substring(0, last);
                    this.globalID = Long.parseUnsignedLong(maybeId);
                    this.raw = this.symbol;
                } else {
                    // v3 符号@全局ID@原始符号，android ONLY，考虑 native trace <> 之间的 @ 忽略
                    boolean inBrackets = false;
                    int first = -1;
                    int second = -1;
                    for (int i = 0; i < symbol.length(); i++) {
                        char c = symbol.charAt(i);
                        if (c == '<') {
                            inBrackets = true;
                        } else if (c == '>') {
                            inBrackets = false;
                        } else if (c == '@' && !inBrackets) {
                            if (first < 0) {
                                first = i;
                            } else {
                                second = i;
                                break;
                            }
                        }
                    }
                    this.symbol = symbol.substring(0, first);
                    this.globalID = Long.parseUnsignedLong(symbol.substring(first + 1, second));
                    this.raw = symbol.substring(second + 1);
                }
            }
        } catch (Throwable e) {
            this.symbol = symbol;
            this.globalID = globalID;
            this.raw = symbol;
        }
        this.isReservedTypeSymbol = isReservedTypeSymbol;
        this.type = TYPE_PURE_JAVA;
        nativeReTracedResult = resolveNativeSymbol();
    }

    private NativeReTracedResult resolveNativeSymbol() {
        if (!isNativeSymbol()) {
            return null;
        }
        int middle = symbol.indexOf("><");
        try {
            String library = symbol.substring(1, middle);
            String symbol = this.symbol.substring(middle + 2, this.symbol.length() - 1);
            return new NativeReTracedResult(library, symbol);
        } catch (IndexOutOfBoundsException e) {
            throw new RuntimeException("invalid symbol " + symbol, e);
        }
    }

    public static boolean isHex(String input) {
        String hexPattern = "^[0-9A-Fa-f]+$";
        return input.matches(hexPattern);
    }
    public String symbol() {
        return symbol(false);
    }

    public String symbol(boolean trimReturnType) {
        if (!trimReturnType) {
            return symbol;
        }
        return MethodUtils.trimReturnType(symbol);
    }
    public String returnType(boolean trimReturnType) {
        if (!trimReturnType) {
            return null;
        }
        String after = symbol(true);
        if (after.length() == symbol.length()) {
            return null;
        }
        return symbol.substring(0, symbol.length() - after.length()).trim();
    }

    @Override
    public String toString() {
        return symbol;
    }

    public boolean isNativeSymbol() {
        return symbol.startsWith("<") && !symbol.startsWith("<runtime method>") && symbol.contains("><");
    }
}
