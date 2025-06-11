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
//  BTraceProfilerPlugin.m
//  Pods-PerfAnalysis_Core
//
//  Created by ByteDance on 2024/1/8.
//

#import <Foundation/Foundation.h>
#include <memory>
#include <vector>
#include <unordered_map>

#import "BTrace.h"
#import "BTraceProfilerConfig.h"
#import "BTraceProfilerPlugin.h"

#include "BTraceRecord.hpp"
#include "BTraceDataBase.hpp"

#include "CPUSampleModel.hpp"

#include "profiler.hpp"
#include "callstack_table.hpp"


namespace btrace {
    extern uint8_t trace_id;

    extern int64_t start_mach_time;

    static void profile_callback(btrace::Profile *profile);
    
    struct CPUSample {
        ProcessedSample *start_sample;
        ProcessedSample *end_sample;
        uint32_t stack_id;
        
        CPUSample() : start_sample(nullptr), end_sample(nullptr), stack_id(0) {}
        
        CPUSample(ProcessedSample *sample, uint32_t stack_id)
        : start_sample(sample), end_sample(sample), stack_id(stack_id) {}
    };

    static void save_cpu_sample(uint32_t tid, CPUSample &cpu_sample, std::vector<Record> &overwritten_stacks);
}

@interface BTraceProfilerPlugin ()

@property(atomic, assign, direct) bool running;

@end


__attribute__((objc_direct_members))
@implementation BTraceProfilerPlugin

+ (NSString *)name {
    return @"timeprofiler";
}

+ (instancetype)shared {
    static BTraceProfilerPlugin *instance = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      instance = [[self alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];
    
    _running = false;

    return self;
}

- (void)start {
    if (!self.btrace.enable) {
        return;
    }
    
    if (_running) {
        return;
    }

    if (!self.config) {
        self.config = [BTraceProfilerConfig defaultConfig];
    }
    
    if (![self initTable]) {
        return;
    }
    
    BTraceProfilerConfig *config = (BTraceProfilerConfig *)self.config;
    
    intptr_t period = config.period;
    intptr_t bg_period = config.bg_period;
    intptr_t max_duration = config.max_duration;
    bool main_thread_only = config.main_thread_only;
    bool active_thread_only = config.active_thread_only;
    bool fast_unwind = config.fast_unwind;
    
    _running = true;

    btrace::Profiler::SetProfileCallback(profile_callback);
    btrace::Profiler::EnableProfiler(period, bg_period, main_thread_only,
                                        active_thread_only, fast_unwind);
}

-(void)dump{
    if (!self.btrace.enable) {
        return;
    }
    
    if (!_running) {
        return;
    }
    
    Profiler::ProcessSampleBuffer();
}

- (void)stop {
    if (!self.btrace.enable) {
        return;
    }
    
    if (!_running) {
        return;
    }

    _running = false;
    
    btrace::Profiler::DisableProfiler();
    btrace::Profiler::SetProfileCallback(nullptr);
}


- (bool)initTable {
    bool result = false;
    
    if (0 < self.btrace.bufferSize) {
        result = self.btrace.db->drop_table<CPUBatchSampleModel>();
        
        if (!result) {
            return result;
        }
        
        result = self.btrace.db->create_table<CPUBatchSampleModel>();

        if (!result) {
            return result;
        }
        
        result = self.btrace.db->drop_table<CPUBatchIntervalSampleModel>();
        
        if (!result) {
            return result;
        }
        
        result = self.btrace.db->create_table<CPUBatchIntervalSampleModel>();

        if (!result) {
            return result;
        }
    } else {
        
        result = self.btrace.db->drop_table<CPUIntervalSampleModel>();
        
        if (!result) {
            return result;
        }
        
        result = self.btrace.db->create_table<CPUIntervalSampleModel>();

        if (!result) {
            return result;
        }
        
        result = self.btrace.db->drop_table<CPUSampleModel>();
        
        if (!result) {
            return result;
        }
        
        result = self.btrace.db->create_table<CPUSampleModel>();

        if (!result) {
            return result;
        }
    }

    return result;
}

@end

namespace btrace {

void profile_callback(btrace::Profile *profile) {
    if (!profile->sample_count()) {
        return;
    }
    
    auto config = (BTraceProfilerConfig *)[BTraceProfilerPlugin shared].config;
    
    using SampleVector = std::vector<btrace::ProcessedSample *>;
    using ThreadSampleTable = std::unordered_map<ThreadId, SampleVector>;
    auto thread_sample_table = ThreadSampleTable();
    
    for (intptr_t sample_index = 0; sample_index < profile->sample_count();
         sample_index++) {
        btrace::ProcessedSample *sample = profile->SampleAt(sample_index);
        ThreadId tid = sample->tid();
        
        if (thread_sample_table.find(tid) == thread_sample_table.end()) {
            thread_sample_table[tid] = SampleVector();
        }
        
        auto &sample_vector = thread_sample_table[tid];
        sample_vector.push_back(sample);
    }
    
    auto overwritten_records = std::vector<Record>();
    Transaction transaction([BTrace shared].db->getHandle());
    
    for (auto iter = thread_sample_table.begin();
         iter != thread_sample_table.end(); ++iter) {
        ThreadId tid = iter->first;
        auto &sample_vector = iter->second;
        
        ASSERT(std::is_sorted(sample_vector.begin(), sample_vector.end(),
        // stop until curr <= prev
        [](auto const curr, auto const prev) {
            return curr->timestamp() <= prev->timestamp();
        }));
        
        CPUSample cpu_sample;
        for (size_t idx = 0; idx < sample_vector.size(); ++idx) {
            auto sample = sample_vector[idx];
            
            uint32_t stack_id = sample->stack_id();
            
            if (config.merge_stack) {
                if (idx == 0) {
                    cpu_sample = CPUSample(sample, stack_id);
                }
                
                if (cpu_sample.stack_id == stack_id) {
                    cpu_sample.end_sample = sample;
                } else {
                    save_cpu_sample(tid, cpu_sample, overwritten_records);
                    cpu_sample = CPUSample(sample, stack_id);
                }
                
                if (idx == sample_vector.size() - 1) {
                    save_cpu_sample(tid, cpu_sample, overwritten_records);
                }
            } else {
                cpu_sample = CPUSample(sample, stack_id);
                save_cpu_sample(tid, cpu_sample, overwritten_records);
            }
        }
    }
    
    transaction.commit();
    
    RecordBuffer::ProcessOverWrittenRecord(overwritten_records);
}

void save_cpu_sample(uint32_t tid, CPUSample &cpu_sample, std::vector<Record>& overwritten_records) {
    uint8_t trace_id = btrace::trace_id;
    uint32_t start_time = (uint32_t)((cpu_sample.start_sample->timestamp() - start_mach_time)/10);
    uint32_t end_time = (uint32_t)((cpu_sample.end_sample->timestamp() - start_mach_time)/10);
    uint32_t start_cpu_time = cpu_sample.start_sample->cpu_time();
    uint32_t end_cpu_time = cpu_sample.end_sample->cpu_time();
    uint32_t alloc_size = cpu_sample.end_sample->alloc_size();
    uint32_t alloc_count = cpu_sample.end_sample->alloc_count();
    
    if (g_record_buffer) {
        auto overwritten_record = Record();
        auto cpu_record = CPURecord(tid, start_time, end_time, start_cpu_time,
                                    end_cpu_time, cpu_sample.stack_id, alloc_size,
                                    alloc_count);
        auto record = Record(cpu_record);
        g_record_buffer->OverWrittenRecord(record, overwritten_record);
        
        if (overwritten_record.record_type != RecordType::kNone) {
            overwritten_records.push_back(overwritten_record);
        }
    } else {
        if (start_time == end_time) {
            auto sample_record = CPUSampleModel(trace_id, tid, start_time,
                                                start_cpu_time, cpu_sample.stack_id,
                                                alloc_size, alloc_count);
            [BTrace shared].db->insert(sample_record);
        } else {
            auto interval_sample_record = CPUIntervalSampleModel(trace_id, tid, start_time, end_time,
                                                            start_cpu_time, end_cpu_time, cpu_sample.stack_id,
                                                            alloc_size, alloc_count);
            [BTrace shared].db->insert(interval_sample_record);
        }

    }
}
}
