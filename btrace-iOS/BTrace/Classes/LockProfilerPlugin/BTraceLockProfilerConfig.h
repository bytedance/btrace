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
//  BTraceLockPluginConfig.h
//  Pods
//
//  Created by ByteDance on 2024/11/6.
//

#ifndef BTraceLockProfilerConfig_h
#define BTraceLockProfilerConfig_h

#import <Foundation/Foundation.h>
#import "BTracePluginConfig.h"

@interface BTraceLockProfilerConfig : BTracePluginConfig

@property(nonatomic, assign, direct) intptr_t period;
@property(nonatomic, assign, direct) intptr_t bg_period;
@property(nonatomic, assign, direct) bool mtx;
@property(nonatomic, assign, direct) bool rw;
@property(nonatomic, assign, direct) bool cond;
@property(nonatomic, assign, direct) bool unfair;

+ (nonnull instancetype)defaultConfig;

@end

#endif /* BTraceLockProfilerConfig_h */

