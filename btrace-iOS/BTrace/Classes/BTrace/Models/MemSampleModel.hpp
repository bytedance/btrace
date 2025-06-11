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
//  MemSampleModel.hpp
//  Pods
//
//  Created by ByteDance on 2024/7/10.
//

#ifndef MemSampleModel_h
#define MemSampleModel_h

#ifdef __cplusplus

#include <vector>

#include "globals.hpp"
#include "reflection.hpp"

namespace btrace {

struct MemNode {
    uint32_t addr;
    uint32_t size;
    uint32_t time;
    uint32_t cpu_time;
    uint32_t stack_id;
    uint32_t alloc_size;
    uint32_t alloc_count;
    
    MemNode(): addr(0), size(0), time(0), cpu_time(0), stack_id(0) {};
    
    MemNode(uint32_t addr, uint32_t size, uint32_t time, uint32_t cpu_time, uint32_t stack_id,
            uint32_t alloc_size, uint32_t alloc_count)
        : addr(addr), size(size), time(time), cpu_time(cpu_time), stack_id(stack_id),
        alloc_size(alloc_size), alloc_count(alloc_count) {};
};

struct MemRecord: MemNode {
    uint32_t tid;
    
    MemRecord(): tid(0), MemNode(0, 0, 0, 0, 0, 0, 0) {};
    
    MemRecord(uint32_t tid, uint32_t addr,
              uint32_t size, uint32_t time, uint32_t cpu_time, uint32_t stack_id,
              uint32_t alloc_size, uint32_t alloc_count)
        : tid(tid), MemNode(addr, size, time, cpu_time, stack_id,
        alloc_size, alloc_count) {};
};

struct MemSampleModel: MemRecord {
    int8_t trace_id;

    MemSampleModel(int8_t trace_id, const MemRecord& record)
        : trace_id(trace_id), MemRecord(record.tid, record.addr, record.size,
        record.time, record.cpu_time, record.stack_id,
        record.alloc_size, record.alloc_count) {};
    
    MemSampleModel(int8_t trace_id, uint32_t tid, uint32_t addr,
                   uint32_t size, uint32_t time, uint32_t cpu_time,
                   uint32_t stack_id, uint32_t alloc_size, uint32_t alloc_count)
        : trace_id(trace_id), MemRecord(tid, addr, size, time, cpu_time, stack_id,
        alloc_size, alloc_count) {};

    static constexpr std::string_view TableName() {
        return std::string_view("MemSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<MemSampleModel>::Register(
            Field("trace_id", &MemSampleModel::trace_id),
            Field("tid", &MemSampleModel::tid),
            Field("addr", &MemSampleModel::addr),
            Field("size", &MemSampleModel::size),
            Field("time", &MemSampleModel::time),
            Field("cpu_time", &MemSampleModel::cpu_time),
            Field("stack_id", &MemSampleModel::stack_id),
            Field("alloc_size", &MemSampleModel::alloc_size),
            Field("alloc_count", &MemSampleModel::alloc_count));
        return reflection;
    };
};

struct MemBatchSampleModel {
    int8_t trace_id;
    uint32_t tid;
    std::vector<MemNode> nodes;
    
    MemBatchSampleModel(int8_t trace_id, uint32_t tid, std::vector<MemNode> &&nodes)
        : trace_id(trace_id), tid(tid), nodes(nodes) {};

    static constexpr std::string_view TableName() {
        return std::string_view("MemBatchSampleModel");
    }

    static auto MetaInfo() {
        static auto reflection = Reflection<MemBatchSampleModel>::Register(
            Field("trace_id", &MemBatchSampleModel::trace_id),
            Field("tid", &MemBatchSampleModel::tid),
            Field("nodes", &MemBatchSampleModel::nodes));
        return reflection;
    };
};

}

#endif /* __cplusplus */

#endif /* MemSampleModel_h */
