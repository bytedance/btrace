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
//  BTraceTimeSeriesPlugin.h
//  Pods
//
//  Created by ByteDance on 2024/1/8.
//

#ifndef BTraceTimeSeriesPlugin_h
#define BTraceTimeSeriesPlugin_h

#import <Foundation/Foundation.h>
#import "BTracePlugin.h"
#import "BTraceTimeSeriesData.h"

@interface BTraceTimeSeriesPlugin : BTracePlugin

@property(atomic, assign, direct) BOOL running;

- (void)SaveTimeSeriesData:(BTraceTimeSeriesData* _Nullable)data;

@end

#endif /* BTraceTimeSeriesPlugin_h */
