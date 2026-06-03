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

#include "napi/native_api.h"
#include "hilog/log.h"
#include <vector>
#include "deviceinfo.h"

#undef LOG_TAG
#define LOG_TAG "btrace:entry"

std::vector<void*> ptrs;

napi_value TestMalloc(napi_env env, napi_callback_info info) {
    ptrs.emplace_back(malloc(32));
    return nullptr;
}

napi_value TestFree(napi_env env, napi_callback_info info) {
    for(void* ptr : ptrs) {
        free(ptr);
    }
    ptrs.clear();
    return nullptr;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {\
      {"testMalloc", nullptr, TestMalloc, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"testFree", nullptr, TestFree, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    int apiVersion = OH_GetSdkApiVersion();
    OH_LOG_INFO(LOG_APP, "OH SDK API Version: %{public}d", apiVersion);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&demoModule);
}
