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
//  BTraceMonitorConfig.m
//  Pods-PerfAnalysis_Core
//
//  Created by ByteDance on 2024/10/14.
//

#import <Foundation/Foundation.h>

#import "BTraceMonitorConfig.h"

@implementation CPUUsageConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    _period = MAX([[data valueForKey:@"period"] intValue], 1);
    _window = MIN(MAX([[data valueForKey:@"interval"] intValue], 1), 600);
    _threshold = MAX([[data valueForKey:@"threshold"] floatValue], 0.1);
    id single_thread_oc = [data objectForKey:@"single_thread"];
    if (single_thread_oc == nil) {
        _single_thread = YES;
    } else {
        _single_thread = [single_thread_oc boolValue];
    }
    
    return self;
}

@end

@implementation CPULevelConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    _duration = 10;

    return self;
}

@end

@implementation MemorySpikeConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    _period = MAX([[data valueForKey:@"period"] intValue], 1);
    _window = MIN(MAX([[data valueForKey:@"interval"] intValue], 1), 30);
    _threshold = MAX([[data valueForKey:@"threshold"] floatValue], 0.2);
    
    return self;
}

@end

@implementation HangConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    _hitch = MAX([[data objectForKey:@"hitch"] intValue], 0);
    _hang = MAX([[data objectForKey:@"hang"] intValue], 0);
    _anr = MAX([[data objectForKey:@"anr"] intValue], 0);
    _anr_bg = MAX([[data objectForKey:@"anr_bg"] intValue], 0);
    
    return self;
}

@end

@implementation BTraceMonitorConfig

+ (instancetype)defaultConfig {
    BTraceMonitorConfig *config = [[BTraceMonitorConfig alloc] init];
    config.cpu_usage = nil;
    config.mem_spike = nil;
    config.cpu_level = nil;
    config.hang = nil;
    return config;
}

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    if (data) {
        NSDictionary *cpu_usage_config = [data valueForKey:@"cpu_usage"];
        if (cpu_usage_config) {
            NSDictionary *config = [cpu_usage_config valueForKey:@"config"];
            _cpu_usage = [[CPUUsageConfig alloc] initWithDictionary:config];
        }
        NSDictionary *cpu_level_config = [data valueForKey:@"cpu_level"];
        if (cpu_level_config) {
            NSDictionary *config = [cpu_level_config valueForKey:@"config"];
            _cpu_level = [[CPULevelConfig alloc] initWithDictionary:config];
        }
        NSDictionary *mem_spike_config = [data valueForKey:@"mem_spike"];
        if (mem_spike_config) {
            NSDictionary *config = [mem_spike_config valueForKey:@"config"];
            _mem_spike = [[MemorySpikeConfig alloc] initWithDictionary:config];
        }
        NSDictionary *hang_config = [data valueForKey:@"hang"];
        if (hang_config) {
            NSDictionary *config = [hang_config valueForKey:@"config"];
            _hang = [[HangConfig alloc] initWithDictionary:config];
        }
    }
    return self;
}

@end
