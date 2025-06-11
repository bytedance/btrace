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
//  BTraceMemProfilerConfig.h
//  Pods
//
//  Created by ByteDance on 2024/7/8.
//

#ifndef BTraceMemProfilerConfig_h
#define BTraceMemProfilerConfig_h

#import <Foundation/Foundation.h>
#import "BTracePluginConfig.h"

@interface BTraceMemProfilerConfig : BTracePluginConfig

@property(assign, nonatomic, direct) BOOL lite_mode;
@property(assign, nonatomic, direct) uint32_t interval;
@property(assign, nonatomic, direct) intptr_t period;
@property(assign, nonatomic, direct) intptr_t bg_period;

+ (nonnull instancetype)defaultConfig;

@end

#endif /* BTraceMemProfilerConfig_h */

