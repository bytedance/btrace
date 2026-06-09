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

public class StackParseFactory {
    public static final StackParseFactory JAVA_AND_NATIVE = new StackParseFactory(new IStackParse() {
        @Override
        public boolean needKeepStack(StackListParser parser, boolean isNativeStack, int threadId) {
            return true;
        }
    }, new IStackParse() {
        @Override
        public boolean needKeepStack(StackListParser parser, boolean isNativeStack, int threadId) {
            return true;
        }
    });

    private final IStackParse defaultParse;
    private final IStackParse playParse;

    private StackParseFactory(IStackParse defaultParse, IStackParse playParse) {
        this.defaultParse = defaultParse;
        this.playParse = playParse;
    }

    public IStackParse get(int sceneId) {
        switch (sceneId) {
            case 130:
            case 131:
                return playParse;
            default:
                return defaultParse;
        }
    }
}
