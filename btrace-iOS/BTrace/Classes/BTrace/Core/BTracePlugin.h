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
//  BTracePlugin.h
//  Pods
//
//  Created by ByteDance on 2024/1/8.
//

#ifndef BTracePlugin_h
#define BTracePlugin_h

#import <Foundation/Foundation.h>

#import "BTracePluginConfig.h"


@class BTrace;

@interface BTracePlugin : NSObject

@property (nonatomic, strong, direct, nullable) BTracePluginConfig *config;
@property (nonatomic, strong, direct, nonnull) BTrace *btrace;

+ (nonnull instancetype)shared;

+ (nonnull NSString *)name;

- (void)updateConfig:(BTracePluginConfig * _Nullable)config;

- (void)start;

- (void)dump;

- (void)stop;

@end


#endif /* BTracePlugin_h */
