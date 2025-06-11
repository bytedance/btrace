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
//  BTracePluginConfig.h
//  Pods
//
//  Created by ByteDance on 2024/1/8.
//

#ifndef BTraceProfilerConfig_h
#define BTraceProfilerConfig_h

#import <Foundation/Foundation.h>
#import "BTracePluginConfig.h"

@interface BTraceProfilerConfig : BTracePluginConfig

@property(nonatomic, assign, direct) intptr_t period;
@property(nonatomic, assign, direct) intptr_t bg_period;
@property(nonatomic, assign, direct) intptr_t max_duration;
@property(nonatomic, assign, direct) bool merge_stack;
@property(nonatomic, assign, direct) bool main_thread_only;
@property(nonatomic, assign, direct) bool active_thread_only;
@property(nonatomic, assign, direct) bool fast_unwind;

+ (nonnull instancetype)defaultConfig;

@end

#endif /* BTraceProfilerConfig_h */

