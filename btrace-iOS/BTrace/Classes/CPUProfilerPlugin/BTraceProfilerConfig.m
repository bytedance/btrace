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
//  BTracePluginConfig.m
//  Pods-PerfAnalysis_Core
//
//  Created by ByteDance on 2024/1/8.
//

#import <Foundation/Foundation.h>

#import "BTraceProfilerConfig.h"


@implementation BTraceProfilerConfig

+ (instancetype)defaultConfig {
    BTraceProfilerConfig *config = [[BTraceProfilerConfig alloc] init];
    config.period = 1;
    config.bg_period = 1;
    config.max_duration = 0;
    config.merge_stack = YES;
    config.main_thread_only = NO;
    config.active_thread_only = YES;
    config.fast_unwind = YES;
    return config;
}

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    _period = MAX([[data objectForKey:@"period"] intValue], 1);
    _bg_period = MAX([[data objectForKey:@"bg_period"] intValue], _period);
    _max_duration = [[data objectForKey:@"max_duration"] intValue];
    id merge_stack_oc = [data objectForKey:@"merge_stack"];
    if (merge_stack_oc == nil) {
        _merge_stack = true;
    } else {
        _merge_stack = [merge_stack_oc boolValue];
    }
    id main_thread_only_oc = [data objectForKey:@"main_thread_only"];
    if (main_thread_only_oc == nil) {
        _main_thread_only = false;
    } else {
        _main_thread_only = [main_thread_only_oc boolValue];
    }
    id active_thread_only_oc = [data objectForKey:@"active_thread_only"];
    if (active_thread_only_oc == nil) {
        _active_thread_only = true;
    } else {
        _active_thread_only = [active_thread_only_oc boolValue];
    }
    id fast_unwind_oc = [data objectForKey:@"fast_unwind"];
    if (fast_unwind_oc == nil) {
        _fast_unwind = true;
    } else {
        _fast_unwind = [fast_unwind_oc boolValue];
    }
    return self;
}

@end
