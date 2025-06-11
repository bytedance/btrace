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
//  ImageInfoModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/1/19.
//

#ifndef ImageInfoModel_h
#define ImageInfoModel_h

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

struct ImageInfoModel {
    int8_t trace_id;
    uintptr_t address;
    uintptr_t len;
    int type;
    const char *name;
    const char *uuid;

    ImageInfoModel(int8_t trace_id, uintptr_t address, int64_t len, int type,
                   const char *name, const char *uuid)
        : trace_id(trace_id), address(address), len(len), type(type),
          name(name), uuid(uuid){};

    static constexpr std::string_view TableName() {
        return std::string_view("ImageInfoModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<ImageInfoModel>::Register(
            Field("trace_id", &ImageInfoModel::trace_id),
            Field("address", &ImageInfoModel::address),
            Field("len", &ImageInfoModel::len),
            Field("type", &ImageInfoModel::type),
            Field("name", &ImageInfoModel::name),
            Field("uuid", &ImageInfoModel::uuid));
        return reflection;
    };
};

}

#endif /* __cplusplus */

#endif /* ImageInfoModel_h */
