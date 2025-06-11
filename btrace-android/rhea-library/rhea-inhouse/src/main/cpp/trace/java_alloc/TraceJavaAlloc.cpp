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
//
// @author lupengfei
//

#include "TraceJavaAlloc.h"

#include <memory>
#include "../../utils/npth_dl.h"
#include <shadowhook.h>
#include <mutex>
#include "java_alloc_common.h"
#include "checkpoint.h"
#include "thread_list.h"
#include "../../stat/JavaObjectStat.h"
#include "../../sampling/SamplingCollector.h"
#include "../../RheaContext.h"
#include "../../utils/log.h"
#include "../../utils/misc.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "RheaTrace:JavaAlloc"

namespace rheatrace {

typedef void (*SetQuickAllocEntryPointsInstrumented_)(bool instrumented);
static SetPtr_ SetAllocationListenerFunc = nullptr;
static CallVoid_ RemoveAllocationListenerFunc = nullptr;

static void *Thread_ResetQuickAllocEntryPointsForThreadFunc = nullptr;
static SetQuickAllocEntryPointsInstrumented_ SetQuickAllocEntryPointsInstrumentedFunc = nullptr;

static void *s_stub_SetEntrypointsInstrumented = nullptr;
static bool s_use_thread_ResetQuickAllocEntryPointsForThread_bool = false;
static thread_local bool s_in_self_set = false;

bool SetEntrypointsInstrumentedWithCheckpoint(void *_this, bool enable);
void onObjectAllocated(void* self, void* obj, size_t byte_count);

/**
 * android 8到android10版本
 */
class AllocationListener8 {
public:
    virtual ~AllocationListener8() = default;

    virtual void ObjectAllocated(void *self, void **obj, size_t byte_count) {}
};

/**
 * android 11 以上版本
 */
class AllocationListener11 {
public:
    virtual ~AllocationListener11() = default;

    // An event to allow a listener to intercept and modify an allocation before it takes place.
    // The listener can change the byte_count and type as they see fit. Extreme caution should be used
    // when doing so. This can also be used to control allocation occurring on another thread.
    //
    // Concurrency guarantees: This might be called multiple times for each single allocation. It's
    // guaranteed that, between the final call to the callback and the object being visible to
    // heap-walks there are no suspensions. If a suspension was allowed between these events the
    // callback will be invoked again after passing the suspend point.
    //
    // If the alloc succeeds it is guaranteed there are no suspend-points between the last return of
    // PreObjectAlloc and the newly allocated object being visible to heap-walks.
    //
    // This can also be used to make any last-minute changes to the type or size of the allocation.
    virtual void PreObjectAllocated(void *self,
                                    void *type,
                                    size_t *byte_count) {}

    // Fast check if we want to get the PreObjectAllocated callback, to avoid the expense of creating
    // handles. Defaults to false.
    virtual bool HasPreAlloc() const { return false; }

    virtual void ObjectAllocated(void *self, void **obj, size_t byte_count) {}
};

class MyAllocationListener11 : AllocationListener11 {
public:
    bool HasPreAlloc() const override {
        return false;
    }

    void PreObjectAllocated(void *self, void *type, size_t *byte_count) override {
    }

    void ObjectAllocated(void *self, void **obj, size_t byte_count) override {
        onObjectAllocated(self, obj, byte_count);
    }
};

class MyAllocationListener8 : AllocationListener8 {
public:
    void ObjectAllocated(void *self, void **obj, size_t byte_count) override {
        onObjectAllocated(self, obj, byte_count);
    }
};

void onObjectAllocated(void* self, void* obj, size_t byte_count) {
    JavaObjectStat::onObjectAllocated(byte_count);
    SamplingCollector::request(SamplingType::kObjectAllocation, self);
}

bool hookSelfSetEntrypoints() {
    if (s_stub_SetEntrypointsInstrumented != nullptr) {
        return true;
    }
    void *stub = shadowhook_hook_sym_name("libart.so",
                                          "_ZN3art15instrumentation15Instrumentation26SetEntrypointsInstrumentedEb",
                                          (void *) (SetEntrypointsInstrumentedWithCheckpoint),
                                          nullptr);
    if (stub == nullptr) {
        int error_num = shadowhook_get_errno();
        const char *error_msg = shadowhook_to_errmsg(error_num);
        ALOGE("hook SetEntrypointsInstrumented return: %p, %d - %s", stub, error_num, error_msg);
        return false;
    }

    s_stub_SetEntrypointsInstrumented = stub;
    return true;
}

bool SetQuickAllocEntryPointsInstrumented(bool enable) {
    if (SetQuickAllocEntryPointsInstrumentedFunc != nullptr) {
        SetQuickAllocEntryPointsInstrumentedFunc(enable);
        return true;
    }
    return false;
}


class CheckPointClosure : Closure {
private:
    CheckPointClosure() : count(0), target(0) {
    }

public:
    static CheckPointClosure *create() {
        return new CheckPointClosure();
    }

    virtual void Run(void *self) {
        if (Thread_ResetQuickAllocEntryPointsForThreadFunc != nullptr) {
            if (s_use_thread_ResetQuickAllocEntryPointsForThread_bool) {
                ((CallVoidBool_) (Thread_ResetQuickAllocEntryPointsForThreadFunc))(self, false);
            } else {
                ((CallVoid_) (Thread_ResetQuickAllocEntryPointsForThreadFunc))(self);
            }
        }
        incre();
    }
    virtual ~CheckPointClosure() {
    }

    void incre() {
        bool finished;
        {
            std::unique_lock<std::mutex> lock(mutex);
            count += 1;
            ALOGD("ResetQuickAllocEntryPointsForThread %d, %zu", gettid(), count);
            finished = count >= target && target > 0;
        }
        if (finished) {
            delete this;
        }
    }

    void setTarget(size_t target) {
        std::unique_lock<std::mutex> lk(mutex);
        this->target = target;
    }

private:
    std::mutex mutex;
    std::condition_variable cv;
    size_t count;
    size_t target;
};

bool SetEntrypointsInstrumentedWithCheckpoint(void *_this, bool enable) {
    SHADOWHOOK_STACK_SCOPE();
    if (!s_in_self_set) {
        return SHADOWHOOK_CALL_PREV(SetEntrypointsInstrumentedWithCheckpoint, _this, enable);
    }
    if (SetQuickAllocEntryPointsInstrumented(enable)) {
        CheckPointClosure *closure = CheckPointClosure::create();

        size_t target = run_checkpoint(closure);
        if (target > 0) {
            closure->setTarget(target);
            ALOGD("run checkpoint success %zu", target);
            return true;
        } else {
            ALOGD("run checkpoint failed %zu", target);
            delete closure;
        }
    }
    return false;
}

void unhookSelfSetEntrypoints() {
    if (s_stub_SetEntrypointsInstrumented == nullptr) {
        return;
    }
    shadowhook_unhook(s_stub_SetEntrypointsInstrumented);
    s_stub_SetEntrypointsInstrumented = nullptr;
}

const char *getThread_ResetQuickAllocEntryPointsForThreadFuncName() {
    return s_use_thread_ResetQuickAllocEntryPointsForThread_bool
           ? Thread_RESET_QUICK_ALLOC_ENTRY_POINTS_FOR_THREAD_BOOL
           : Thread_RESET_QUICK_ALLOC_ENTRY_POINTS_FOR_THREAD;
}

void registerAllocationListener() {
    if (RheaContext::heap == nullptr) {
        ALOGE("heap is null");
        return;
    }
    auto api_level = get_android_sdk_version();
    void *alloc_listener = nullptr;
    if (api_level >= __ANDROID_API_R__) {
        alloc_listener = new MyAllocationListener11();
    } else if (api_level >= __ANDROID_API_O__) {
        alloc_listener = new MyAllocationListener8();
    }
    if (alloc_listener == nullptr) {
        ALOGE("Unsupported android api level: %d!", api_level);
        return;
    }
    s_in_self_set = true;
    SetAllocationListenerFunc(RheaContext::heap, alloc_listener);
    s_in_self_set = false;
    ALOGI("register allocation listener success");
}

bool TraceJavaAlloc::init() {
    std::shared_ptr<void> scope = std::shared_ptr<void>(npth_dlopen("libart.so"), npth_dlclose);
    if (scope.get() == nullptr) {
        ALOGE("Cannot open libart.so");
        return false;
    }

    void* art_lib = scope.get();
    if(!init_checkpoint(art_lib)) {
        return false;
    }

    SetAllocationListenerFunc = (SetPtr_) npth_dlsym(art_lib,
                                                            HEAP_SET_ALLOC_LISTENER);
    if(SetAllocationListenerFunc == nullptr) {
        ALOGE("Cannot find SetAllocationListener");
        return false;
    }
    RemoveAllocationListenerFunc = (CallVoid_) npth_dlsym(art_lib,
                                                                 HEAP_REMOVE_ALLOC_LISTENER);
    if(RemoveAllocationListenerFunc == nullptr) {
        ALOGE("Cannot find RemoveAllocationListener");
        return false;
    }

    s_use_thread_ResetQuickAllocEntryPointsForThread_bool = false;
    Thread_ResetQuickAllocEntryPointsForThreadFunc = npth_dlsym(art_lib,
                                                                       Thread_RESET_QUICK_ALLOC_ENTRY_POINTS_FOR_THREAD);
    if(Thread_ResetQuickAllocEntryPointsForThreadFunc == nullptr) {
        s_use_thread_ResetQuickAllocEntryPointsForThread_bool = true;
        Thread_ResetQuickAllocEntryPointsForThreadFunc = npth_dlsym(art_lib,
                                                                           Thread_RESET_QUICK_ALLOC_ENTRY_POINTS_FOR_THREAD_BOOL);
    }
    if(Thread_ResetQuickAllocEntryPointsForThreadFunc == nullptr) {
        ALOGE("Cannot find ResetQuickAllocEntryPointsForThread");
        return false;
    }

    SetQuickAllocEntryPointsInstrumentedFunc = (SetQuickAllocEntryPointsInstrumented_) npth_dlsym(
            art_lib, SET_QUICK_ALLOC_ENTRY_POINTS_INSTRUMENTED);
    if(SetQuickAllocEntryPointsInstrumentedFunc == nullptr) {
        ALOGE("Cannot find SetQuickAllocEntryPointsInstrumented");
        return false;
    }

    if(!hookSelfSetEntrypoints()) {
        ALOGE("hook SetEntrypointsInstrumented failed");
        return false;
    }

    if (RheaContext::threadList != nullptr) {
        registerAllocationListener();
    } else {
        set_thread_list_callback([](void* thread_list) {
            registerAllocationListener();
        });
    }

    return true;
}

void TraceJavaAlloc::destroy() {

}

}

