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
//  DispatchSampleModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/8/27.
//

#ifndef DispatchSampleModel_h
#define DispatchSampleModel_h

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

struct DispatchNode {
    uint32_t source_time;
    uint32_t source_cpu_time;
    uint32_t target_tid;
    uint32_t target_time;
    uint32_t stack_id;
    uint32_t alloc_size;
    uint32_t alloc_count;
    
    DispatchNode() {}
    
    DispatchNode(uint32_t source_time, uint32_t source_cpu_time, uint32_t target_tid,
              uint32_t target_time, uint32_t stack_id,
              uint32_t alloc_size, uint32_t alloc_count)
            : source_time(source_time), source_cpu_time(source_cpu_time),
            target_tid(target_tid), target_time(target_time), stack_id(stack_id),
            alloc_size(alloc_size), alloc_count(alloc_count) {}
};

struct DispatchRecord: DispatchNode {
    uint32_t source_tid;
    
    DispatchRecord() {}
    
    DispatchRecord(uint32_t source_tid, uint32_t source_time, uint32_t source_cpu_time, 
                uint32_t target_tid, uint32_t target_time, uint32_t stack_id,
                uint32_t alloc_size, uint32_t alloc_count)
                : source_tid(source_tid), DispatchNode(source_time, source_cpu_time,
                target_tid, target_time, stack_id, alloc_size, alloc_count) {}
    
    DispatchRecord(uint32_t source_tid, DispatchNode &dispatch_node): source_tid(source_tid), DispatchNode(dispatch_node) {}
};

struct DispatchSampleModel: DispatchRecord {
    int8_t trace_id;

    DispatchSampleModel() = default;
    
    DispatchSampleModel(int8_t trace_id, DispatchRecord &dispatch_record)
        : trace_id(trace_id), DispatchRecord(dispatch_record) {}

    DispatchSampleModel(int8_t trace_id, uint32_t source_time, uint32_t source_cpu_time,
                     uint32_t source_tid, uint32_t target_time, uint32_t target_tid,
                     uint32_t stack_id, uint32_t alloc_size, uint32_t alloc_count)
        : trace_id(trace_id), DispatchRecord(source_tid, source_time, source_cpu_time,
        target_tid, target_time, stack_id, alloc_size, alloc_count) {}

    static constexpr std::string_view TableName() {
        return std::string_view("DispatchSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<DispatchSampleModel>::Register(
            Field("trace_id", &DispatchSampleModel::trace_id),
            Field("source_tid", &DispatchSampleModel::source_tid),
            Field("source_time", &DispatchSampleModel::source_time),
            Field("source_cpu_time", &DispatchSampleModel::source_cpu_time),
            Field("target_tid", &DispatchSampleModel::target_tid),
            Field("target_time", &DispatchSampleModel::target_time),
            Field("stack_id", &DispatchSampleModel::stack_id),
            Field("alloc_size", &DispatchSampleModel::alloc_size),
            Field("alloc_count", &DispatchSampleModel::alloc_count));
        return reflection;
    };
};

struct DispatchBatchSampleModel {
    int8_t trace_id;
    uint32_t tid;
    std::vector<DispatchNode> nodes;
    
    DispatchBatchSampleModel() {};
    DispatchBatchSampleModel(int8_t trace_id, uint32_t tid, std::vector<DispatchNode> &&nodes)
        : trace_id(trace_id), tid(tid), nodes(nodes) {};

    static constexpr std::string_view TableName() {
        return std::string_view("DispatchBatchSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<DispatchBatchSampleModel>::Register(
            Field("trace_id", &DispatchBatchSampleModel::trace_id),
            Field("tid", &DispatchBatchSampleModel::tid),
            Field("nodes", &DispatchBatchSampleModel::nodes));
        return reflection;
    };
};

}

#endif /* __cplusplus */

#endif /* DispatchSampleModel_h */
