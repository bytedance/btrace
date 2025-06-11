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
//  ThreadInfoModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/12/26.
//

#ifndef ThreadInfoModel_h
#define ThreadInfoModel_h

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus

#include "globals.hpp"
#include "reflection.hpp"

using namespace btrace;

/* ORM base model
 * Only support:
 *  raw type, such as 'int', 'char',...
 *  raw pointer type, such as 'int*', 'char*'...,
 *  vector<raw type>
 */
namespace btrace {

struct ThreadInfoModel {
    int8_t trace_id;
    uint32_t tid;
    char *name = nullptr;

    ThreadInfoModel() = default;

    ThreadInfoModel(int8_t trace_id, uint32_t tid, const char *thread_name)
        : trace_id(trace_id), tid(tid) {
        name = strdup(thread_name);
    }

    ThreadInfoModel(ThreadInfoModel &&info)
        : trace_id(info.trace_id), tid(info.tid) {
        name = info.name;
        info.name = nullptr;
    }

    ~ThreadInfoModel() {
        if (name != nullptr) {
            free(name);
            name = nullptr;
        }
    }

    static constexpr std::string_view TableName() {
        return std::string_view("ThreadInfoModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<ThreadInfoModel>::Register(
            Field("trace_id", &ThreadInfoModel::trace_id),
            Field("tid", &ThreadInfoModel::tid),
            Field("name", &ThreadInfoModel::name));
        return reflection;
    };
};

}

#endif /* __cplusplus */

#endif /* ThreadInfoModel_h */
