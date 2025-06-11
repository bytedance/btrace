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
//  TimeSeriesModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/1/19.
//

#ifndef TimeSeriesModel_h
#define TimeSeriesModel_h

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

struct TimeSeriesModel {
    int8_t trace_id;
    mach_port_t tid;
    int64_t timestamp;
    const char *type;
    const char *info;

    TimeSeriesModel(int8_t trace_id, mach_port_t tid, int64_t timestamp,
                     const char *type, const char *info)
        : trace_id(trace_id), tid(tid), timestamp(timestamp), type(type),
          info(info) {}

    static constexpr std::string_view TableName() {
        return std::string_view("TimeSeriesModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<TimeSeriesModel>::Register(
            Field("trace_id", &TimeSeriesModel::trace_id),
            Field("tid", &TimeSeriesModel::tid),
            Field("timestamp", &TimeSeriesModel::timestamp),
            Field("type", &TimeSeriesModel::type),
            Field("info", &TimeSeriesModel::info));
        return reflection;
    };
};

}

#endif /* __cplusplus */

#endif /* TimeSeriesModel_h */
