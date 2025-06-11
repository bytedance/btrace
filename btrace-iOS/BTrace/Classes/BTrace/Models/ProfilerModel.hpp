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
//  ProfilerModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/1/19.
//

#ifndef ProfilerModel_h
#define ProfilerModel_h

#ifdef __cplusplus
#include "reflection.hpp"

using namespace btrace;

/* ORM base model
 * Only support:
 *  raw type, such as 'int', 'char',...
 *  raw pointer type, such as 'int*', 'char*'...,
 *  vector<raw type>
 */
namespace btrace {

struct ProfilerModel {
    int8_t trace_id;
    int period;
    int max_duration;
    bool main_thread_only;
    bool active_thread_only;

    ProfilerModel(int8_t trace_id, int period, int max_duration,
                  bool main_thread_only, bool active_thread_only)
        : trace_id(trace_id), period(period), max_duration(max_duration),
          main_thread_only(main_thread_only),
          active_thread_only(active_thread_only){};

    static constexpr std::string_view TableName() {
        return std::string_view("ProfilerModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<ProfilerModel>::Register(
            Field("trace_id", &ProfilerModel::trace_id),
            Field("period", &ProfilerModel::period),
            Field("max_duration", &ProfilerModel::max_duration),
            Field("main_thread_only", &ProfilerModel::main_thread_only),
            Field("active_thread_only", &ProfilerModel::active_thread_only));
        return reflection;
    };
};

}

#endif /* __cplusplus */

#endif /* ProfilerModel_h */
