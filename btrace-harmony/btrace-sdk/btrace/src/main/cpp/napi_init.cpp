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

#include "TraceConfigurations.h"
#include "napi/native_api.h"
#include <cstdint>
#include <deviceinfo.h>
#include "TraceAgent.h"
#include <cstddef>
#include <string_view>
#include <hilog/log.h>

#undef LOG_TAG
#define LOG_TAG "btrace:NativeEntry"

static napi_value initTracing(napi_env env, napi_callback_info info) {
    if (OH_GetSdkApiVersion() < 20) {
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    size_t argc = 1;
    napi_value args[1] = {nullptr};

    napi_get_cb_info(env, info, &argc, args , nullptr, nullptr);
    
    napi_value configArray = args[0];
    
    bool isArray = false;
    napi_is_array(env, configArray, &isArray);
    
    if (!isArray) {
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    uint32_t length = 0;
    napi_get_array_length(env, configArray, &length);
    
    if (length == 0) {
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    btrace::TraceConfigurations::get().update(env, configArray, length);
    
    bool result = btrace::TraceAgent::get().start();
    
    napi_value napiResult;
    napi_get_boolean(env, result, &napiResult);
    return napiResult;
}

static napi_value pause(napi_env env, napi_callback_info info) {
    if (OH_GetSdkApiVersion() < 20) {
        return NULL;
    }
    btrace::TraceAgent::get().pause();
    return NULL;
}

static napi_value resume(napi_env env, napi_callback_info info) {
    if (OH_GetSdkApiVersion() < 20) {
        return NULL;
    }
    btrace::TraceAgent::get().resume();
    return NULL;
}

static napi_value dumpTrace(napi_env env, napi_callback_info info) {
    if (OH_GetSdkApiVersion() < 20) {
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    size_t argc = 5;
    napi_value args[5] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    size_t dirLength = 0;
    if (napi_get_value_string_utf8(env, args[0], nullptr, 0, &dirLength) != napi_ok) {
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    char dirBuf[dirLength + 1];
    std::memset(dirBuf, 0, dirLength + 1);
    if (napi_get_value_string_utf8(env, args[0], dirBuf, dirLength + 1, &dirLength) != napi_ok) {
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    std::string traceDir(dirBuf);
    size_t extraLength = 0;
    if (napi_get_value_string_utf8(env, args[1], nullptr, 0, &extraLength)) {
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    char extraBuf[extraLength + 1];
    std::memset(extraBuf, 0, extraLength + 1);
    if (napi_get_value_string_utf8(env, args[1], extraBuf, extraLength + 1, &extraLength) != napi_ok) {
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    napi_value tidArray = args[2];
    
    bool isArray = false;
    napi_is_array(env, tidArray, &isArray);
    
    if (!isArray) {
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    uint32_t length = 0;
    napi_get_array_length(env, tidArray, &length);

    std::vector<pid_t> tids{};

    for (int i=0;i<length;++i) {
        napi_value ele = nullptr;
        int32_t val = 0;
        napi_get_element(env, tidArray, i, &ele);
        napi_get_value_int32(env, ele, &val);
        tids.push_back(val);
    }

    int64_t beginTimeMs = 0;
    if (napi_get_value_int64(env, args[3], &beginTimeMs) != napi_ok) {
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }

    int64_t endTimeMs = 0;
    if (napi_get_value_int64(env, args[4], &endTimeMs) != napi_ok) {
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }
    
    std::string extra(extraBuf);
    bool result = btrace::TraceAgent::get().dumpTrace(traceDir, extra, tids, beginTimeMs, endTimeMs);
    napi_value napiResult;
    napi_get_boolean(env, result, &napiResult);
    return napiResult;
}

static napi_value terminateTracing(napi_env env, napi_callback_info info) {
    if (OH_GetSdkApiVersion() >= 20) {
        btrace::TraceAgent::get().stop();
    }
    return NULL;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "initTracing", nullptr, initTracing, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "dumpTrace", nullptr, dumpTrace, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "terminateTracing", nullptr, terminateTracing, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "resume", nullptr, resume, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "pause", nullptr, pause, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "btrace",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterBtraceModule(void)
{
    napi_module_register(&demoModule);
}
