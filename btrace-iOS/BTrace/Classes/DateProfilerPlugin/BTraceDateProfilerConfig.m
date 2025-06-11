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
//  BTraceDateProfilerConfig.m
//  Pods-PerfAnalysis_Core
//
//  Created by ByteDance on 2024/11/6.
//

#import <Foundation/Foundation.h>

#import "BTraceDateProfilerConfig.h"


@implementation BTraceDateProfilerConfig

+ (instancetype)defaultConfig {
    BTraceDateProfilerConfig *config = [[BTraceDateProfilerConfig alloc] init];
    config.period = 1 * 1000;
    config.bg_period = 1 * 1000;
    return config;
}

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    _period = MAX([[data objectForKey:@"period"] intValue], 1);
    _bg_period = MAX([[data objectForKey:@"bg_period"] intValue], _period);
    _period *= 1000;
    _bg_period *= 1000;
    
    return self;
}

@end
