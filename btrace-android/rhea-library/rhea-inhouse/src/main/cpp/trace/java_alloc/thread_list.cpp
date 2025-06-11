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

#include "thread_list.h"
#include <shadowhook.h>
#include "java_alloc_common.h"
#include "../../RheaContext.h"
#include "../../utils/log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "RheaTrace:ThreadList"

namespace rheatrace {

static void* sThreadInitHookStub = nullptr;
static void* sFinalizerStub = nullptr;
static OnGetThreadList sThreadListCallback = nullptr;

bool proxyThreadInit(void* thread, void* thread_list, void* java_vm_ext,
                     void* jni_env_ext) {
    SHADOWHOOK_STACK_SCOPE();
    ALOGI("got thread list %p", thread_list);
    RheaContext::threadList = thread_list;
    bool result = SHADOWHOOK_CALL_PREV(proxyThreadInit, thread, thread_list, java_vm_ext, jni_env_ext);
    if (RheaContext::heap != nullptr && sThreadListCallback!= nullptr) {
        sThreadListCallback(thread_list);
    }
    shadowhook_unhook(sThreadInitHookStub);
    sThreadInitHookStub = nullptr;
    return result;
}

static void MyAddFinalizerReference(void* heap, void* self, void* object) {
    SHADOWHOOK_STACK_SCOPE();
    ALOGI("got heap %p", heap);
    RheaContext::heap = heap;
    shadowhook_unhook(sFinalizerStub);
    sFinalizerStub = nullptr;
    if (RheaContext::threadList != nullptr && sThreadListCallback!= nullptr) {
        sThreadListCallback(RheaContext::threadList);
    }
    SHADOWHOOK_CALL_PREV(MyAddFinalizerReference, heap, self, object);
}


bool init_thread_list() {
    if (RheaContext::threadList != nullptr || (sThreadInitHookStub!= nullptr && sFinalizerStub!= nullptr)) {
        return true;
    }
    sThreadInitHookStub = shadowhook_hook_sym_name("libart.so", THREAD_INIT,
                                                   reinterpret_cast<void*>(proxyThreadInit),
                                                   nullptr);
    if (sThreadInitHookStub== nullptr) {
        ALOGE("failed to hook %s", THREAD_INIT);
        return false;
    }
    sFinalizerStub = shadowhook_hook_sym_name("libart.so",
                                             "_ZN3art2gc4Heap21AddFinalizerReferenceEPNS_6ThreadEPNS_6ObjPtrINS_6mirror6ObjectEEE",
                                             reinterpret_cast<void*>(MyAddFinalizerReference),
                                             nullptr);
    if (sFinalizerStub== nullptr) {
        ALOGE("failed to hook %s", "_ZN3art2gc4Heap21AddFinalizerReferenceEPNS_6ThreadEPNS_6ObjPtrINS_6mirror6ObjectEEE");
        return false;
    }
    return true;
}

void set_thread_list_callback(OnGetThreadList callback) {
    sThreadListCallback = callback;
}

}