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
//  CallStackTableModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/2/18.
//

#ifndef CallStackTableRecord_h
#define CallStackTableRecord_h

#ifdef __cplusplus

#include <vector>

#include "globals.hpp"
#include "reflection.hpp"

using namespace btrace;

namespace btrace {

#pragma pack(push, 4)
struct CallStackNode {
    uint32_t stack_id;
    uint32_t parent;
    uint64_t address;
    
    CallStackNode(uint32_t stack_id, uint32_t parent, uint64_t address): stack_id(stack_id), parent(parent), address(address) {}
};
#pragma pack(pop)

struct CallStackTableModel {
    int8_t trace_id;
    std::vector<CallStackNode> nodes;

    CallStackTableModel(int8_t trace_id, std::vector<CallStackNode> &&nodes)
        : trace_id(trace_id), nodes(nodes) {};

    static constexpr std::string_view TableName() {
        return std::string_view("CallStackTableModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<CallStackTableModel>::Register(
            Field("trace_id", &CallStackTableModel::trace_id),
            Field("nodes", &CallStackTableModel::nodes));
        return reflection;
    };
};

}

#endif /* __cplusplus */

#endif /* CallStackTableRecord_h */
