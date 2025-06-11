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
//  BTraceMemProfilerPlugin.m
//  Pods-PerfAnalysis_Core
//
//  Created by ByteDance on 2024/1/8.
//

#import <Foundation/Foundation.h>
#include <mach/mach.h>

#import "BTrace.h"
#import "BTraceMemProfilerConfig.h"
#import "BTraceMemProfilerPlugin.h"

#include "BTraceRecord.hpp"
#include "BTraceDataBase.hpp"
#include "CallStackTableModel.hpp"
#include "MemSampleModel.hpp"
#include "MemRegionInfoModel.hpp"

#include "memory_profiler.hpp"

using namespace btrace;

namespace btrace {

extern uint8_t trace_id;

void get_mem_region_info(std::vector<MemRegionInfoNode>& region_info_list);

}

@interface BTraceMemProfilerPlugin ()

@property(atomic, assign, direct) bool running;

@end


__attribute__((objc_direct_members))
@implementation BTraceMemProfilerPlugin

+ (NSString *)name {
    return @"memoryprofiler";
}

+ (instancetype)shared {
    static BTraceMemProfilerPlugin *instance = nil;
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
    
    if (![self initTable]) {
        return;
    }

    if (!self.config) {
        self.config = [BTraceMemProfilerConfig defaultConfig];
    }
    
    _running = true;

    BTraceMemProfilerConfig *config = (BTraceMemProfilerConfig *)self.config;
    btrace::MemoryProfiler::EnableProfiler(config.lite_mode, config.interval,
                                              config.period, config.bg_period);
}

- (void)stop {
    if (!self.btrace.enable) {
        return;
    }
    
    if (!_running) {
        return;
    }

    _running = false;
    
    MemoryProfiler::DisableProfiler();
}

- (void)saveResult {
    auto config = (BTraceMemProfilerConfig*)self.config;
    
    auto region_info_list = std::vector<MemRegionInfoNode>();
    get_mem_region_info(region_info_list);
    auto mem_region_info_record = MemRegionInfoModel(trace_id, 1, std::move(region_info_list));
    self.btrace.db->insert(mem_region_info_record);
}

- (bool)initTable {
    bool result = true;
    
    if (0 < self.btrace.bufferSize) {
        result = self.btrace.db->drop_table<MemBatchSampleModel>();
        
        if (!result) {
            return result;
        }
        
        result = self.btrace.db->create_table<MemBatchSampleModel>();

        if (!result) {
            return result;
        }
    } else {
        result = self.btrace.db->drop_table<MemSampleModel>();
        
        if (!result) {
            return result;
        }
        
        result = self.btrace.db->create_table<MemSampleModel>();

        if (!result) {
            return result;
        }
    }
    
    result = self.btrace.db->drop_table<MemRegionInfoModel>();
    
    if (!result) {
        return result;
    }
    
    result = self.btrace.db->create_table<MemRegionInfoModel>();

    if (!result) {
        return result;
    }

    return result;
}

- (void)dump {
    if (!self.btrace.enable) {
        return;
    }
    
    if (!_running) {
        return;
    }
    
    MemoryProfiler::ProcessSampleBuffer();
}

@end

namespace btrace {

void get_mem_region_info(std::vector<MemRegionInfoNode>& region_info_list) {
    kern_return_t krc = KERN_SUCCESS;
    vm_address_t address = 0;
    vm_size_t size = 0;
    uint32_t depth = 1;
    auto pagesize = vm_kernel_page_size;
    
    while(true) {
        struct vm_region_submap_info_64 info;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        krc = vm_region_recurse_64(mach_task_self(), &address, &size, &depth, (vm_region_info_64_t)&info, &count);
    
        if (krc == KERN_INVALID_ADDRESS){
            break;
        }
        
        if (!info.is_submap) {
            uint32_t dirty_size = (uint32_t)(info.pages_dirtied * pagesize);
            uint32_t swapped_size = (uint32_t)(info.pages_swapped_out * pagesize);
//            ASSERT(address <= (uint64_t)1 << 36);
            auto addr = (uint32_t)(address>>4);
            region_info_list.emplace_back(addr, dirty_size, swapped_size);
            address += size;
        } else {
            ++depth;
        }
    }
}

void mem_profile_callback(std::vector<MemRecord> &processed_buffer) {
    uint8_t trace_id = btrace::trace_id;
    auto overwritten_records = std::vector<Record>();
    
    Transaction transaction([BTrace shared].db->getHandle());
    
    for (int i=0; i<processed_buffer.size();++i) {
        auto &mem_record = processed_buffer[i];
        if (g_record_buffer) {
            auto overwritten_record = Record();
            auto record = Record(mem_record);
            g_record_buffer->OverWrittenRecord(record, overwritten_record);
            if (overwritten_record.record_type != RecordType::kNone) {
                overwritten_records.push_back(overwritten_record);
            }
        } else {
            auto mem_sample_record = MemSampleModel(trace_id, mem_record);
            [BTrace shared].db->insert(mem_sample_record);
        }
    }
    
    transaction.commit();
    
    RecordBuffer::ProcessOverWrittenRecord(overwritten_records);
}
}
