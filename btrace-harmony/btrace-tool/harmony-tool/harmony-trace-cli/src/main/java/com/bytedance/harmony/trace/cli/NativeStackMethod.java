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

public interface NativeStackMethod {

    class Helper {
        private static boolean isJavaMethodSo(String soName, String methodSignature) {
            return soName.contains(".oat") || soName.contains(".vdex") || soName.contains(".odex") ||
                    soName.contains(".dex") || soName.endsWith(".jar") || soName.contains(".art") ||
                    soName.contains("memfd:jit-cache") || methodSignature.equals("art_jni_trampoline") ||
                    methodSignature.equals("art_quick_generic_jni_trampoline");
        }
    }

    static NativeStackMethod parse(String module, String methodSignature) {
        if (Helper.isJavaMethodSo(module, methodSignature)) {
            return new JavaMethod(module, methodSignature);
        } else {
            return new CppMethod(module, methodSignature);
        }
    }

    boolean isJavaMethod();

    boolean needRetrace();

    String getMethodName();

    class JavaMethod implements NativeStackMethod {
        private final String module;
        private final String methodSignature;

        JavaMethod(String module, String methodSignature) {
            this.module = module;
            this.methodSignature = methodSignature;
        }

        @Override
        public boolean isJavaMethod() {
            return true;
        }

        @Override
        public boolean needRetrace() {
            return true;
        }

        @Override
        public String getMethodName() {
            return methodSignature;
        }
    }

    class CppMethod implements NativeStackMethod {
        private final String module;
        private final String methodSignature;

        CppMethod(String module, String methodName) {
            this.module = module;
            this.methodSignature = methodName;
        }

        @Override
        public boolean isJavaMethod() {
            return false;
        }

        @Override
        public boolean needRetrace() {
            return false;
        }

        @Override
        public String getMethodName() {
            return methodSignature;
        }
    }
}
