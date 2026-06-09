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

//
// Created on 2026/6/1.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "hook_utils.h"
#include "native_hook.h"

#include <mutex>
#include <atomic>

static native_hooker *__hooker = nullptr;
static inline std::once_flag once_flag;

namespace btrace {
bool hook_init() {
    std::call_once(once_flag, [](){
        __hooker = new native_hooker();
        __hooker->phrase_proc_maps();
    });
    return true;
}

bool hook_single(const char *so_name, const char *func_name, void *pfn_new, void **ppfn_old) {
    __hooker->hook_module(so_name, func_name, pfn_new, ppfn_old, true);
    return true;
}
bool hook_all(const char *func_name, void *pfn_new, void **ppfn_old) {
    __hooker->hook_all_modules(func_name, pfn_new, ppfn_old, true);
    return true;
}
}