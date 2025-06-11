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
//  BTraceMonitorConfig.h
//  Pods
//
//  Created by ByteDance on 2024/10/14.
//

#ifndef BTraceMonitorConfig_h
#define BTraceMonitorConfig_h

#import <Foundation/Foundation.h>
#import "BTracePluginConfig.h"

@interface CPUUsageConfig : BTracePluginConfig

@property(nonatomic, assign, direct) uint32_t period;
@property(nonatomic, assign, direct) uint32_t window;
@property(nonatomic, assign, direct) float threshold;
@property(nonatomic, assign, direct) bool single_thread;

@end

@interface CPULevelConfig : BTracePluginConfig

@property(nonatomic, assign, direct) uint32_t duration;

@end

@interface MemorySpikeConfig : BTracePluginConfig

@property(nonatomic, assign, direct) uint32_t period;
@property(nonatomic, assign, direct) uint32_t window;
@property(nonatomic, assign, direct) float threshold;

@end

@interface HangConfig : BTracePluginConfig

@property(nonatomic, assign, direct) uint32_t hitch;  // ms
@property(nonatomic, assign, direct) uint32_t hang;  // ms
@property(nonatomic, assign, direct) uint32_t anr;  // s
@property(nonatomic, assign, direct) uint32_t anr_bg;  // s

@end

@interface BTraceMonitorConfig : BTracePluginConfig

@property(nonatomic, strong, direct, nullable) CPUUsageConfig *cpu_usage;
@property(nonatomic, strong, direct, nullable) CPULevelConfig *cpu_level;
@property(nonatomic, strong, direct, nullable) MemorySpikeConfig *mem_spike;
@property(nonatomic, strong, direct, nullable) HangConfig *hang;

+ (nonnull instancetype)defaultConfig;

@end

#endif /* BTraceMonitorConfig_h */

