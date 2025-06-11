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
#pragma once

#include <jni.h>
#include "Stack.h"

namespace rheatrace {

class StackVisitor {
public:
    enum class StackWalkKind {
        kIncludeInlinedFrames,
        kSkipInlinedFrames [[maybe_unused]]
    };
private:
    using CurrentThread = void* (*)();
    using Construct = void (*)(void*, void*, void*, StackWalkKind, bool);
    using Destruct = void (*)(void*);
    using CreateContext = void* (*)();
    using GetMethod = void* (*)(void*);
    using WalkStack = void (*)(void*, bool);
    static bool sInited;
    static CurrentThread sCurrentThreadCall;
    static Construct sConstructCall;
    static Destruct sDestructCall;
    static CreateContext sCreateContextCall;
    static GetMethod sGetMethodCall;
    static WalkStack sWalkStackCall;

    [[maybe_unused]] char mSpaceHolder[2048]; // preserve for real StackVisitor's fields space
    uint32_t mCurIndex;
    Stack& mStack;
public:
    static bool init();

    static bool visitOnce(Stack& stack, void* threadSelf = nullptr, StackWalkKind kind = StackWalkKind::kSkipInlinedFrames);

private:
    static bool
    innerVisitOnce(Stack& stack, void* thread, StackWalkKind kind);

    StackVisitor(Stack& stack) : mCurIndex(0), mStack(stack) {}

    void walk();

    virtual ~StackVisitor() {}

    [[maybe_unused]] virtual bool VisitFrame();
};

} // namespace rheatrace