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
//  DateSampleModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/10/28.
//

#ifndef DateSampleModel_h
#define DateSampleModel_h

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

struct DateNode {
    uint32_t time;  // relative to trace start time
    uint32_t cpu_time;
    uint32_t stack_id;
    uint32_t alloc_size;
    uint32_t alloc_count;
    
    DateNode() {};
    DateNode(uint32_t time, uint32_t cpu_time, uint32_t stack_id,
             uint32_t alloc_size, uint32_t alloc_count)
        : time(time), cpu_time(cpu_time), stack_id(stack_id),
        alloc_size(alloc_size), alloc_count(alloc_count) {}
};

struct DateRecord: DateNode {
    uint32_t tid;
    
    DateRecord(): tid(0), DateNode(0, 0, 0, 0, 0) {};
    
    DateRecord(uint32_t tid, uint32_t time, uint32_t cpu_time, uint32_t stack_id,
               uint32_t alloc_size, uint32_t alloc_count)
        : tid(tid), DateNode(time, cpu_time, stack_id,
        alloc_size, alloc_count) {};
};

struct DateSampleModel: DateRecord {
    int8_t trace_id;
    
    DateSampleModel() {};
    DateSampleModel(int8_t trace_id, uint32_t tid, uint32_t time,
                        uint32_t cpu_time, uint32_t stack_id,
                        uint32_t alloc_size, uint32_t alloc_count)
        : trace_id(trace_id), DateRecord(tid, time, cpu_time, stack_id,
        alloc_size, alloc_count) {};

    static constexpr std::string_view TableName() {
        return std::string_view("DateSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<DateSampleModel>::Register(
            Field("trace_id", &DateSampleModel::trace_id),
            Field("tid", &DateSampleModel::tid),
            Field("time", &DateSampleModel::time),
            Field("cpu_time", &DateSampleModel::cpu_time),
            Field("stack_id", &DateSampleModel::stack_id),
            Field("alloc_size", &DateSampleModel::alloc_size),
            Field("alloc_count", &DateSampleModel::alloc_count));
        return reflection;
    };
};

struct DateBatchSampleModel {
    int8_t trace_id;
    uint32_t tid;
    std::vector<DateNode> nodes;
    
    DateBatchSampleModel() {};
    DateBatchSampleModel(int8_t trace_id, uint32_t tid, std::vector<DateNode> &&nodes)
        : trace_id(trace_id), tid(tid), nodes(nodes) {};

    static constexpr std::string_view TableName() {
        return std::string_view("DateBatchSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<DateBatchSampleModel>::Register(
            Field("trace_id", &DateBatchSampleModel::trace_id),
            Field("tid", &DateBatchSampleModel::tid),
            Field("nodes", &DateBatchSampleModel::nodes));
        return reflection;
    };
};


}

#endif /* __cplusplus */

#endif /* DateSampleModel_h */
