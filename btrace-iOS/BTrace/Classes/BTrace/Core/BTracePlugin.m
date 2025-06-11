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
//  BTracePlugin.m
//  Pods-BTrace_Core
//
//  Created by ByteDance on 2024/1/8.
//

#import <Foundation/Foundation.h>
#import "BTracePlugin.h"
#import "BTrace.h"


@implementation BTracePlugin

+ (NSString *)name {
    return @"";
}

+ (instancetype)shared {
    static BTracePlugin *instance = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      instance = [[self alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];
    
    if (self) {
        _btrace = [BTrace shared];
    }
    return self;
}

- (void)updateConfig:(BTracePluginConfig *)config {
    _config = config;
}

- (void)start {
}

- (void)dump {
}

- (void)stop {
}

- (void)destroy {
}

@end
