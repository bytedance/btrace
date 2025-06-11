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
//  BTraceRecord.mm
//  record
//
//  Created by Bytedance.
//

#include "BTrace.h"
#include "BTraceDataBase.hpp"
#include "BTraceRecord.hpp"

#include "callstack_table.hpp"
#include "ring_buffer.hpp"

namespace btrace {

extern uint8_t trace_id;

RecordBuffer *g_record_buffer = nullptr;

void RecordBuffer::ProcessOverWrittenRecord(
    std::vector<Record> &overwritten_records) {
    for (auto record : overwritten_records) {
        uint32_t stack_id = 0;
        switch (record.record_type) {
            case RecordType::kCPURecord: {
                stack_id = record.cpu_record.stack_id;
                break;
            }
            case RecordType::kMemRecord: {
                stack_id = record.mem_record.stack_id;
                break;
            }
            case RecordType::kDispatchRecord: {
                stack_id = record.dispatch_record.stack_id;
                break;
            }
            case RecordType::kDateRecord: {
                stack_id = record.date_record.stack_id;
                break;
            }
            case RecordType::kLockRecord: {
                stack_id = record.lock_record.stack_id;
                break;
            }
            default:
                break;
        }
        if (stack_id) {
            g_callstack_table->DecrementStackRef(stack_id);
        }
    }
}

void RecordBuffer::Dump(uint64_t begin_time, uint64_t end_time, ThreadId filter_tid) {
    using CPUIntervalVector = std::vector<CPUIntervalNode>;
    using CPUIntervalVectorTable =
        std::unordered_map<ThreadId, CPUIntervalVector>;
    auto cpu_interval_table = CPUIntervalVectorTable();

    using CPUVector = std::vector<CPUNode>;
    using CPUVectorTable = std::unordered_map<ThreadId, CPUVector>;
    auto cpu_table = CPUVectorTable();

    using MemVector = std::vector<MemNode>;
    using MemVectorTable = std::unordered_map<ThreadId, MemVector>;
    auto mem_table = MemVectorTable();
    
    using DispatchVector = std::vector<DispatchNode>;
    using DispatchVectorTable = std::unordered_map<ThreadId, DispatchVector>;
    auto dispatch_table = DispatchVectorTable();
    
    using DateVector = std::vector<DateNode>;
    using DateVectorTable = std::unordered_map<ThreadId, DateVector>;
    auto date_table = DateVectorTable();

    using LockVector = std::vector<LockNode>;
    using LockVectorTable = std::unordered_map<ThreadId, LockVector>;
    auto lock_table = LockVectorTable();

    begin_time = begin_time / 10;
    end_time = end_time / 10;

    {
        for (uint64_t pos = read_pos_; pos < write_pos_;) {
            auto record = Record();

            ViewAndAdvanceRecord(pos, record);

            if (record.record_type == RecordType::kCPURecord) {
                auto &cpu_record = record.cpu_record;
                auto tid = cpu_record.tid;
                
                if (cpu_record.end_time < begin_time ||
                    end_time < cpu_record.start_time) {
                    continue;
                }
                
                if (filter_tid && tid != filter_tid) {
                    continue;
                }
                
                if (cpu_record.start_time == cpu_record.end_time) {
                    auto it = cpu_table.find(tid);
                    if (it == cpu_table.end()) {
                        bool inserted;
                        std::tie(it, inserted) = cpu_table.emplace(tid, CPUVector());
                    }

                    auto &cpu_vector = it->second;
                    cpu_vector.emplace_back(cpu_record.start_time,
                                            cpu_record.start_cpu_time,
                                            cpu_record.stack_id,
                                            cpu_record.alloc_size,
                                            cpu_record.alloc_count);
                } else {
                    auto it = cpu_interval_table.find(tid);
                    if (it == cpu_interval_table.end()) {
                        bool inserted;
                        std::tie(it, inserted) = cpu_interval_table.emplace(
                                                    tid, CPUIntervalVector());
                    }

                    auto &cpu_interval_vector = it->second;
                    cpu_interval_vector.emplace_back(
                        cpu_record.start_time, cpu_record.end_time,
                        cpu_record.start_cpu_time, cpu_record.end_cpu_time,
                        cpu_record.stack_id, cpu_record.alloc_size,
                        cpu_record.alloc_count);
                }
            } else if (record.record_type == RecordType::kMemRecord) {
                auto &mem_record = record.mem_record;
                
                if (mem_record.time < begin_time ||
                    end_time < mem_record.time) {
                    continue;
                }
                
                auto tid = mem_record.tid;
                
                if (filter_tid && tid != filter_tid) {
                    continue;
                }

                auto it = mem_table.find(mem_record.tid);
                if (it == mem_table.end()) {
                    bool inserted;
                    std::tie(it, inserted) =
                        mem_table.emplace(mem_record.tid, MemVector());
                }

                auto &mem_node_vector = it->second;
                mem_node_vector.emplace_back(mem_record.addr, mem_record.size, mem_record.time,
                                             mem_record.cpu_time, mem_record.stack_id,
                                             mem_record.alloc_size, mem_record.alloc_count);
            } else if (record.record_type == RecordType::kDispatchRecord) {
                auto &dispatch_record = record.dispatch_record;
                
                if (dispatch_record.source_time < begin_time ||
                    end_time < dispatch_record.source_time) {
                    continue;
                }
                
                auto tid = dispatch_record.source_tid;
                
                if (filter_tid && tid != filter_tid) {
                    continue;
                }

                auto it = dispatch_table.find(dispatch_record.source_tid);
                if (it == dispatch_table.end()) {
                    bool inserted;
                    std::tie(it, inserted) =
                    dispatch_table.emplace(dispatch_record.source_tid, DispatchVector());
                }

                auto &dispatch_node_vector = it->second;
                dispatch_node_vector.emplace_back(dispatch_record.source_time, dispatch_record.source_cpu_time, 
                                               dispatch_record.target_tid, dispatch_record.target_time,
                                               dispatch_record.stack_id, dispatch_record.alloc_size, dispatch_record.alloc_count);
            } else if (record.record_type == RecordType::kDateRecord) {
                auto &date_record = record.date_record;
                
                if (date_record.time < begin_time ||
                    end_time < date_record.time) {
                    continue;
                }
                
                auto tid = date_record.tid;
                
                if (filter_tid && tid != filter_tid) {
                    continue;
                }

                auto it = date_table.find(date_record.tid);
                if (it == date_table.end()) {
                    bool inserted;
                    std::tie(it, inserted) =
                    date_table.emplace(date_record.tid, DateVector());
                }

                auto &date_node_vector = it->second;
                date_node_vector.emplace_back(date_record.time, date_record.cpu_time, date_record.stack_id,
                                              date_record.alloc_size, date_record.alloc_count);
            } else if (record.record_type == RecordType::kLockRecord) {
                const auto &lock_record = record.lock_record;
                
                if (lock_record.time < begin_time ||
                    end_time < lock_record.time) {
                    continue;
                }
                
                auto tid = lock_record.tid;
                
                if (filter_tid && tid != filter_tid) {
                    continue;
                }

                auto it = lock_table.find(lock_record.tid);
                if (it == lock_table.end()) {
                    bool inserted;
                    std::tie(it, inserted) =
                    lock_table.emplace(lock_record.tid, LockVector());
                }

                auto &lock_node_vector = it->second;
                lock_node_vector.emplace_back(lock_record.id, lock_record.action, lock_record.time,
                                              lock_record.cpu_time, lock_record.stack_id,
                                              lock_record.alloc_size, lock_record.alloc_count);
            }
        }
    }

    auto trace_id = btrace::trace_id;
    Transaction transaction([BTrace shared].db->getHandle());
    
    [BTrace shared].db->delete_table<CPUBatchSampleModel>();
    [BTrace shared].db->delete_table<CPUBatchIntervalSampleModel>();
    [BTrace shared].db->delete_table<MemBatchSampleModel>();
    [BTrace shared].db->delete_table<DispatchBatchSampleModel>();
    [BTrace shared].db->delete_table<DateBatchSampleModel>();
    [BTrace shared].db->delete_table<LockBatchSampleModel>();
    
    for (auto &node_info : cpu_table) {
        auto tid = node_info.first;
        auto &node_vector = node_info.second;
        auto batch_sample_record =
            CPUBatchSampleModel(trace_id, tid, std::move(node_vector));
        [BTrace shared].db->insert(batch_sample_record);
    }
    for (auto &node_info : cpu_interval_table) {
        auto tid = node_info.first;
        auto &node_vector = node_info.second;
        auto batch_interval_sample_record = CPUBatchIntervalSampleModel(
            trace_id, tid, std::move(node_vector));
        [BTrace shared].db->insert(batch_interval_sample_record);
    }
    for (auto &node_info : mem_table) {
        auto tid = node_info.first;
        auto &node_vector = node_info.second;
        auto batch_sample_record =
            MemBatchSampleModel(trace_id, tid, std::move(node_vector));
        [BTrace shared].db->insert(batch_sample_record);
    }
    for (auto &node_info : dispatch_table) {
        auto tid = node_info.first;
        auto &node_vector = node_info.second;
        auto batch_sample_record = DispatchBatchSampleModel(trace_id, tid, std::move(node_vector));
        [BTrace shared].db->insert(batch_sample_record);
    }
    for (auto &node_info : date_table) {
        auto tid = node_info.first;
        auto &node_vector = node_info.second;
        auto batch_sample_record = DateBatchSampleModel(trace_id, tid, std::move(node_vector));
        [BTrace shared].db->insert(batch_sample_record);
    }
    for (auto &node_info : lock_table) {
        auto tid = node_info.first;
        auto &node_vector = node_info.second;
        auto batch_sample_record = LockBatchSampleModel(trace_id, tid, std::move(node_vector));
        [BTrace shared].db->insert(batch_sample_record);
    }
    
    transaction.commit();
}

} // namespace btrace
