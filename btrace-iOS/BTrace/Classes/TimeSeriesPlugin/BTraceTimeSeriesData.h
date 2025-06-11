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
//  BTraceTimeSeriesConfig.h
//  Pods
//
//  Created by ByteDance on 2024/1/8.
//

#ifndef BTraceTimeSeriesData_h
#define BTraceTimeSeriesData_h

#import <Foundation/Foundation.h>

@interface BTraceTimeSeriesData : NSObject 

@property(nonatomic, assign, direct) mach_port_t tid;
@property(nonatomic, assign, direct) int64_t timestamp;
@property(nonatomic, strong, direct, nullable) NSString *type;
@property(nonatomic, strong, direct, nullable) NSDictionary *info;

- (nonnull instancetype)initWithType: (NSString * _Nullable)type Dictionary:(NSDictionary * _Nullable)json;

- (nonnull instancetype)initWithTid: (mach_port_t) tid Timestamp: (int64_t)timestamp Type: (NSString * _Nullable)type Dictionary:(NSDictionary * _Nullable)json;

@end

#endif /* BTraceTimeSeriesData_h */

