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
#include "StackVisitor.h"
#include "../utils/npth_dl.h"

#include <string>


namespace rheatrace {

static constexpr const char* THREAD_CURRENT_FROM_GDB = "_ZN3art6Thread14CurrentFromGdbEv";
static constexpr const char* STACK_VISITOR_CTOR = "_ZN3art12StackVisitorC2EPNS_6ThreadEPNS_7ContextENS0_13StackWalkKindEb";
static constexpr const char* STACK_VISITOR_DTOR = "_ZN3art12StackVisitorD2Ev";
static constexpr const char* CONTEXT_CREATE = "_ZN3art7Context6CreateEv";
static constexpr const char* STACK_VISITOR_GET_METHOD = "_ZNK3art12StackVisitor9GetMethodEv";
static constexpr const char* STACK_VISITOR_WALK_STACK = "_ZN3art12StackVisitor9WalkStackILNS0_16CountTransitionsE0EEEvb";

bool StackVisitor::sInited = false;
StackVisitor::CurrentThread StackVisitor::sCurrentThreadCall = nullptr;
StackVisitor::Construct StackVisitor::sConstructCall = nullptr;
StackVisitor::Destruct StackVisitor::sDestructCall = nullptr;
StackVisitor::CreateContext StackVisitor::sCreateContextCall = nullptr;
StackVisitor::GetMethod StackVisitor::sGetMethodCall = nullptr;
StackVisitor::WalkStack StackVisitor::sWalkStackCall = nullptr;

static void destructContext(void *context) {
    if (context == nullptr) {
        return;
    }
    using ContextDestructor = void (*)(void *);
    void **vptr = *reinterpret_cast<void ***>(context);
    if (vptr != nullptr) {
        auto deletingDestructorCall = reinterpret_cast<ContextDestructor>(vptr[1]);
        if (deletingDestructorCall != nullptr) {
            deletingDestructorCall(context);
        }
    }
}

bool StackVisitor::init() {
    if (sInited) {
        return true;
    }
    auto *handle = npth_dlopen("libart.so");
    if (handle == nullptr) {
        return false;
    }
    sCurrentThreadCall = reinterpret_cast<CurrentThread>(npth_dlsym(handle, THREAD_CURRENT_FROM_GDB));
    sConstructCall = reinterpret_cast<Construct>(npth_dlsym(handle, STACK_VISITOR_CTOR));
    sDestructCall = reinterpret_cast<Destruct>(npth_dlsym(handle, STACK_VISITOR_DTOR));
    sCreateContextCall = reinterpret_cast<CreateContext>(npth_dlsym(handle, CONTEXT_CREATE));
    sGetMethodCall = reinterpret_cast<GetMethod>(npth_dlsym(handle, STACK_VISITOR_GET_METHOD));
    sWalkStackCall = reinterpret_cast<WalkStack>(npth_dlsym(handle, STACK_VISITOR_WALK_STACK));

    if (sConstructCall != nullptr && sCreateContextCall != nullptr && sGetMethodCall != nullptr && sWalkStackCall != nullptr) {
        sInited = Stack::init(handle);
    }

    npth_dlclose(handle);

    return sInited;
}

bool StackVisitor::visitOnce(Stack &stack, void *threadSelf, StackWalkKind kind) {
    if (!sInited) {
        return false;
    }
    if (threadSelf == nullptr && sCurrentThreadCall != nullptr) {
        threadSelf = sCurrentThreadCall();
    }
    if (threadSelf != nullptr) {
        return innerVisitOnce(stack, threadSelf, kind);
    }
    return false;
}

bool StackVisitor::innerVisitOnce(Stack &stack, void *thread, StackWalkKind kind) {
    StackVisitor visitor(stack);

    void *vptr = *reinterpret_cast<void **>(&visitor);
    auto *context = sCreateContextCall();
    sConstructCall(reinterpret_cast<void *>(&visitor), thread, context, kind, false);
    *reinterpret_cast<void **>(&visitor) = vptr;

    visitor.walk();

    visitor.mStack.mSavedDepth = std::min(visitor.mCurIndex, MAX_STACK_DEPTH);
    visitor.mStack.mActualDepth = visitor.mCurIndex;

    if (sDestructCall != nullptr) {
        sDestructCall(reinterpret_cast<void *>(&visitor));
    }

    destructContext(context);

    return true;
}

[[maybe_unused]] bool StackVisitor::VisitFrame() {
    auto *method = sGetMethodCall(reinterpret_cast<void *>(this));
    if (method != nullptr) {
        if (mCurIndex < MAX_STACK_DEPTH) {
            mStack.mStackMethods[mCurIndex] = uint64_t(method);
        }
        mCurIndex++;
    }
    return true;
}

void StackVisitor::walk() {
    sWalkStackCall(reinterpret_cast<void *>(this), false);
}

} // namespace rheatrace