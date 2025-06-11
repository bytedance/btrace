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

#include "checkpoint.h"
#include <stdint.h>
#include <shadowhook.h>
#include "../../utils/npth_dl.h"
#include "java_alloc_common.h"
#include "../../RheaContext.h"
#include "../../utils/log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "RheaTrace:JavaAlloc"

namespace rheatrace {

typedef size_t (*ThreadList_RunCheckpoint_)(void *thread_list, void *checkpoint_function,
                                            void *callback);
typedef size_t (*ThreadList_RunCheckpoint_15_)(void *thread_list, void *checkpoint_function,
                                               void *callback, bool allow_lock_checking);
ThreadList_RunCheckpoint_ ThreadList_RunCheckpointFunc = nullptr;
ThreadList_RunCheckpoint_15_ ThreadList_RunCheckpointFunc_15 = nullptr;


size_t run_checkpoint(void *closure) {
    if (RheaContext::threadList == nullptr || (ThreadList_RunCheckpointFunc == nullptr && ThreadList_RunCheckpointFunc_15 == nullptr)) {
        return 0;
    }
    if (ThreadList_RunCheckpointFunc != nullptr) {
        return ThreadList_RunCheckpointFunc(RheaContext::threadList, closure, nullptr);
    } else {
        return ThreadList_RunCheckpointFunc_15(RheaContext::threadList, closure, nullptr, false);
    }
}

bool init_checkpoint(void* lib) {

    ThreadList_RunCheckpointFunc = (ThreadList_RunCheckpoint_) npth_dlsym(lib,
                                                                                 ThreadList_RUN_CHECKPOINT);
    if(ThreadList_RunCheckpointFunc == nullptr) {
        ThreadList_RunCheckpointFunc_15 = (ThreadList_RunCheckpoint_15_) npth_dlsym(lib,
                                                                                     ThreadList_RUN_CHECKPOINT_15);
    }
    if (ThreadList_RunCheckpointFunc == nullptr && ThreadList_RunCheckpointFunc_15 == nullptr) {
        ALOGE("Cannot get ThreadList_RunCheckpoint");
        return false;
    }
    return true;
}

}