/*
 * Copyright (C) 2025 ByteDance Inc.
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
//  BTraceModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/1/19.
//

#ifndef BTraceModel_h
#define BTraceModel_h

#ifdef __cplusplus

#include "globals.hpp"
#include "reflection.hpp"
#include <vector>

using namespace btrace;

/* ORM base model
 * Only support:
 *  raw type, such as 'int', 'char',...
 *  raw pointer type, such as 'int*', 'char*'...,
 *  vector<raw type>
 */
namespace btrace {

struct BTraceModel {
    int8_t trace_id;
    int64_t start_time;
    int64_t end_time;
    double sdk_init_time;
    int64_t dump_time = 0;
    int64_t main_tid = 0;
    const char *os_version = "";
    const char *device_model = "";
    const char *bundle_id = "";
    const char *tag = "";
    const char *info = "";
    /*
     * version 1: first version
     * version 2: add alloc size and count info for all types of samples
     * version 3: change unit of time and cpu time of all types of samples from us to 10us
     */
    int64_t version = 0;

    BTraceModel() = default;

    BTraceModel(int8_t trace_id, int64_t start_time, int64_t end_time,
                   double sdk_init_time, int64_t main_tid, const char *os_version,
                   const char *device_model, const char *bundle_id,
                   int64_t version)
        : trace_id(trace_id), start_time(start_time), end_time(end_time),
          sdk_init_time(sdk_init_time), main_tid(main_tid), os_version(os_version),
          device_model(device_model), bundle_id(bundle_id),
          version(version) {}

    static constexpr std::string_view TableName() {
        return std::string_view("BTraceModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<BTraceModel>::Register(
            Field("trace_id", &BTraceModel::trace_id),
            Field("start_time", &BTraceModel::start_time),
            Field("end_time", &BTraceModel::end_time),
            Field("sdk_init_time", &BTraceModel::sdk_init_time),
            Field("os_version", &BTraceModel::os_version),
            Field("bundle_id", &BTraceModel::bundle_id),
            Field("dump_time", &BTraceModel::dump_time),
            Field("tag", &BTraceModel::tag),
            Field("info", &BTraceModel::info),
            Field("version", &BTraceModel::version),
            Field("main_tid", &BTraceModel::main_tid));
        return reflection;
    };
};

}

#endif /* __cplusplus */

#endif /* BTraceModel_h */
