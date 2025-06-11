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
//  CPUSampleModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/9/20.
//

#ifndef CPUSampleModel_h
#define CPUSampleModel_h

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

struct CPUNode {
    uint32_t start_time;  // relative to trace start time
    uint32_t start_cpu_time;
    uint32_t stack_id;
    uint32_t alloc_size; // MB
    uint32_t alloc_count;
    
    CPUNode() {};
    CPUNode(uint32_t start_time, uint32_t start_cpu_time, uint32_t stack_id,
            uint32_t alloc_size, uint32_t alloc_count)
        : start_time(start_time), start_cpu_time(start_cpu_time), stack_id(stack_id),
        alloc_size(alloc_size), alloc_count(alloc_count) {}
};

struct CPUIntervalNode {
    uint32_t start_time;  // relative to trace start time
    uint32_t start_cpu_time;
    uint32_t stack_id;
    uint32_t end_time;    // relative to trace start time
    uint32_t end_cpu_time;
    uint32_t alloc_size;
    uint32_t alloc_count;
    
    CPUIntervalNode(): start_time(0), start_cpu_time(0), stack_id(0),
                     end_time(0), end_cpu_time(0), alloc_size(0), alloc_count(0) {};
    
    CPUIntervalNode(uint32_t start_time, uint32_t end_time,
              uint32_t start_cpu_time, uint32_t end_cpu_time, 
              uint32_t stack_id, uint32_t alloc_size, uint32_t alloc_count)
        : start_time(start_time), start_cpu_time(start_cpu_time), stack_id(stack_id),
        end_time(end_time), end_cpu_time(end_cpu_time),
        alloc_size(alloc_size), alloc_count(alloc_count)  {};
};

struct CPURecord: CPUIntervalNode {
    uint32_t tid;
    
    CPURecord(): tid(0), CPUIntervalNode(0, 0, 0, 0, 0, 0, 0) {};
    
    CPURecord(uint32_t tid, uint32_t start_time, uint32_t end_time,
              uint32_t start_cpu_time, uint32_t end_cpu_time, uint32_t stack_id,
              uint32_t alloc_size, uint32_t alloc_count)
        : tid(tid), CPUIntervalNode(start_time, end_time, start_cpu_time, end_cpu_time,
                                    stack_id, alloc_size, alloc_count) {};
};

struct CPUIntervalSampleModel: CPURecord {
    int8_t trace_id;
    
    CPUIntervalSampleModel(): CPURecord() {};
    CPUIntervalSampleModel(int8_t trace_id, uint32_t tid, uint32_t start_time,
                           uint32_t end_time, uint32_t start_cpu_time,
                           uint32_t end_cpu_time, uint32_t stack_id,
                           uint32_t alloc_size, uint32_t alloc_count)
        : trace_id(trace_id), CPURecord(tid, start_time, end_time,
        start_cpu_time, end_cpu_time, stack_id, alloc_size, alloc_count) {};

    static constexpr std::string_view TableName() {
        return std::string_view("CPUIntervalSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<CPUIntervalSampleModel>::Register(
            Field("trace_id", &CPUIntervalSampleModel::trace_id),
            Field("tid", &CPUIntervalSampleModel::tid),
            Field("start_time", &CPUIntervalSampleModel::start_time),
            Field("end_time", &CPUIntervalSampleModel::end_time),
            Field("start_cpu_time", &CPUIntervalSampleModel::start_cpu_time),
            Field("end_cpu_time", &CPUIntervalSampleModel::end_cpu_time),
            Field("stack_id", &CPUIntervalSampleModel::stack_id),
            Field("alloc_size", &CPUIntervalSampleModel::alloc_size),
            Field("alloc_count", &CPUIntervalSampleModel::alloc_count));
        return reflection;
    };
};

struct CPUBatchIntervalSampleModel {
    int8_t trace_id;
    uint32_t tid;
    std::vector<CPUIntervalNode> nodes;
    
    CPUBatchIntervalSampleModel() {};
    CPUBatchIntervalSampleModel(int8_t trace_id, uint32_t tid, std::vector<CPUIntervalNode> &&nodes)
        : trace_id(trace_id), tid(tid), nodes(nodes) {};

    static constexpr std::string_view TableName() {
        return std::string_view("CPUBatchIntervalSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<CPUBatchIntervalSampleModel>::Register(
            Field("trace_id", &CPUBatchIntervalSampleModel::trace_id),
            Field("tid", &CPUBatchIntervalSampleModel::tid),
            Field("nodes", &CPUBatchIntervalSampleModel::nodes));
        return reflection;
    };
};

struct CPUSampleModel: CPUNode {
    int8_t trace_id;
    uint32_t tid;
    
    CPUSampleModel() {};
    CPUSampleModel(int8_t trace_id, uint32_t tid, uint32_t start_time,
                   uint32_t start_cpu_time, uint32_t stack_id,
                   uint32_t alloc_size, uint32_t alloc_count)
        : trace_id(trace_id), tid(tid), CPUNode(start_time, start_cpu_time,
        stack_id, alloc_size, alloc_count) {};

    static constexpr std::string_view TableName() {
        return std::string_view("CPUSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<CPUSampleModel>::Register(
            Field("trace_id", &CPUSampleModel::trace_id),
            Field("tid", &CPUSampleModel::tid),
            Field("start_time", &CPUSampleModel::start_time),
            Field("start_cpu_time", &CPUSampleModel::start_cpu_time),
            Field("stack_id", &CPUSampleModel::stack_id),
            Field("alloc_size", &CPUSampleModel::alloc_size),
            Field("alloc_count", &CPUSampleModel::alloc_count));
        return reflection;
    };
};

struct CPUBatchSampleModel {
    int8_t trace_id;
    uint32_t tid;
    std::vector<CPUNode> nodes;
    
    CPUBatchSampleModel() {};
    CPUBatchSampleModel(int8_t trace_id, uint32_t tid, std::vector<CPUNode> &&nodes)
        : trace_id(trace_id), tid(tid), nodes(nodes) {};

    static constexpr std::string_view TableName() {
        return std::string_view("CPUBatchSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<CPUBatchSampleModel>::Register(
            Field("trace_id", &CPUBatchSampleModel::trace_id),
            Field("tid", &CPUBatchSampleModel::tid),
            Field("nodes", &CPUBatchSampleModel::nodes));
        return reflection;
    };
};


}

#endif /* __cplusplus */

#endif /* CPUSampleModel_h */
