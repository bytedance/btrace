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
//  LockSampleModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/11/6.
//

#ifndef LockSampleModel_h
#define LockSampleModel_h

#ifdef __cplusplus

#include <array>
#include <vector>

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

#pragma pack(push, 4)
struct LockNode {
    uint64_t id : 48;
    uint64_t action : 16;
    uint32_t time; // relative to trace start time
    uint32_t cpu_time;
    uint32_t stack_id;
    uint32_t alloc_size;
    uint32_t alloc_count;

    LockNode(){};
    LockNode(uint64_t id, uint64_t action, uint32_t time, uint32_t cpu_time,
             uint32_t stack_id, uint32_t alloc_size, uint32_t alloc_count)
        : id(id), action(action), time(time), cpu_time(cpu_time),
          stack_id(stack_id), alloc_size(alloc_size), alloc_count(alloc_count) {}
};
#pragma pack(pop)

#pragma pack(push, 4)
struct LockRecord {
    uint64_t id : 48;
    uint64_t action : 16;
    uint32_t time; // relative to trace start time
    uint32_t cpu_time;
    uint32_t stack_id;
    uint32_t tid;
    uint32_t alloc_size;
    uint32_t alloc_count;

    LockRecord(): tid(0), id(0), action(0), time(0), cpu_time(0),
                stack_id(0), alloc_size(0), alloc_count(0) {};

    LockRecord(uint32_t tid, uint64_t id, uint64_t action, uint32_t time,
               uint32_t cpu_time, uint32_t stack_id, uint32_t alloc_size,
               uint32_t alloc_count)
        : tid(tid), id(id), action(action), time(time), cpu_time(cpu_time),
        stack_id(stack_id), alloc_size(alloc_size), alloc_count(alloc_count) {};
};
#pragma pack(pop)

struct LockSampleModel {
    int8_t trace_id;
    std::array<LockRecord, 1> sample;

    LockSampleModel(){};
    LockSampleModel(int8_t trace_id, const LockRecord &record)
        : trace_id(trace_id), sample{record} {};
    LockSampleModel(int8_t trace_id, uint32_t tid, uint64_t id, uint64_t action,
                    uint32_t time, uint32_t cpu_time, uint32_t stack_id, 
                    uint32_t alloc_size, uint32_t alloc_count)
        : trace_id(trace_id),
          sample{LockRecord(tid, id, action, time, cpu_time, stack_id,
          alloc_size, alloc_count)} {};

    static constexpr std::string_view TableName() {
        return std::string_view("LockSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<LockSampleModel>::Register(
            Field("trace_id", &LockSampleModel::trace_id),
            Field("sample", &LockSampleModel::sample));
        return reflection;
    };
};

struct LockBatchSampleModel {
    int8_t trace_id;
    uint32_t tid;
    std::vector<LockNode> nodes;

    LockBatchSampleModel() {};
    LockBatchSampleModel(int8_t trace_id, uint32_t tid,
                         const std::vector<LockNode> &&nodes)
        : trace_id(trace_id), tid(tid), nodes(nodes) {};

    static constexpr std::string_view TableName() {
        return std::string_view("LockBatchSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<LockBatchSampleModel>::Register(
            Field("trace_id", &LockBatchSampleModel::trace_id),
            Field("tid", &LockBatchSampleModel::tid),
            Field("nodes", &LockBatchSampleModel::nodes));
        return reflection;
    };
};

} // namespace btrace

#endif /* __cplusplus */

#endif /* LockSampleModel_h */
