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
//  BTraceLockPluginConfig.m
//  Pods-PerfAnalysis_Core
//
//  Created by ByteDance on 2024/11/6.
//

#import <Foundation/Foundation.h>

#import "BTraceLockProfilerConfig.h"


@implementation BTraceLockProfilerConfig

+ (instancetype)defaultConfig {
    BTraceLockProfilerConfig *config = [[BTraceLockProfilerConfig alloc] init];
    config.period = 1 * 1000;
    config.bg_period = 1 * 1000;
    config.mtx = true;
    config.rw = true;
    config.cond = true;
    config.unfair = true;
    return config;
}

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    _period = MAX([[data objectForKey:@"period"] intValue], 1);
    _bg_period = MAX([[data objectForKey:@"bg_period"] intValue], _period);
    _period *= 1000;
    _bg_period *= 1000;
    id mtx_oc = [data objectForKey:@"mtx"];
    if (mtx_oc == nil) {
        _mtx = YES;
    } else {
        _mtx = [mtx_oc boolValue];
    }
    id rw_oc = [data objectForKey:@"rw"];
    if (rw_oc == nil) {
        _rw = YES;
    } else {
        _rw = [rw_oc boolValue];
    }
    id cond_oc = [data objectForKey:@"cond"];
    if (cond_oc == nil) {
        _cond = YES;
    } else {
        _cond = [cond_oc boolValue];
    }
    id unfair_oc = [data objectForKey:@"unfair"];
    if (unfair_oc == nil) {
        _unfair = YES;
    } else {
        _unfair = [unfair_oc boolValue];
    }
    
    return self;
}

@end
