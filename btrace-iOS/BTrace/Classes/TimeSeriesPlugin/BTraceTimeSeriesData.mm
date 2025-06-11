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

#import <pthread.h>

#import <Foundation/Foundation.h>
#import "BTraceTimeSeriesData.h"

#import "utils.hpp"
#import "os_thread.hpp"



@implementation BTraceTimeSeriesData

- (instancetype)initWithType: (NSString *)type Dictionary:(NSDictionary *)json {
    mach_port_t tid = btrace::OSThread::GetCurrentThreadId();
    int64_t timestamp = btrace::Utils::GetCurrentTimeMicros();

    return [self initWithTid:tid Timestamp:timestamp Type:type Dictionary:json];
}

- (instancetype)initWithTid:(mach_port_t)tid Timestamp:(int64_t)timestamp Type:(NSString *)type Dictionary:(NSDictionary *)json {
    self = [super init];
    
    if (self) {
        _tid = tid;
        _timestamp = timestamp;
        _type = type;
        _info = [json copy];
    }
    return self;
}

@end
