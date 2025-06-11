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
//  MemRegionInfoModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/7/10.
//

#ifndef MemRegionInfoModel_h
#define MemRegionInfoModel_h

#ifdef __cplusplus

#include <vector>

#include "globals.hpp"
#include "reflection.hpp"

using namespace btrace;

namespace btrace {

struct MemRegionInfoNode {
    uint32_t addr;
    uint32_t dirty_size;
    uint32_t swapped_size;
    
    MemRegionInfoNode(): addr(0), dirty_size(0), swapped_size(0) {};
    
    MemRegionInfoNode(uint32_t addr, uint32_t dirty_size, uint32_t swapped_size)
        : addr(addr), dirty_size(dirty_size), swapped_size(swapped_size) {};
};

struct MemRegionInfoModel {
    int8_t trace_id;
    uint32_t version;
    std::vector<MemRegionInfoNode> nodes;

    MemRegionInfoModel(int8_t trace_id, uint32_t version, std::vector<MemRegionInfoNode> &&nodes)
        : trace_id(trace_id), version(version), nodes(std::move(nodes)) {};

    static constexpr std::string_view TableName() {
        return std::string_view("MemRegionInfoModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<MemRegionInfoModel>::Register(
            Field("trace_id", &MemRegionInfoModel::trace_id),
            Field("version", &MemRegionInfoModel::version),
            Field("nodes", &MemRegionInfoModel::nodes));
        return reflection;
    };
};

}

#endif /* __cplusplus */

#endif /* MemRegionInfoModel_h */
