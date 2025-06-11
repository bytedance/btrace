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

#ifndef BTraceTimeSeriesConfig_h
#define BTraceTimeSeriesConfig_h

#import <Foundation/Foundation.h>
#import "BTracePluginConfig.h"

@interface AppStateConfig : BTracePluginConfig

@end

@interface CellConfig : BTracePluginConfig

@end


@interface FirstFrameConfig : BTracePluginConfig

@end

@interface LaunchConfig : BTracePluginConfig

@end

@interface ScrollConfig : BTracePluginConfig

@property(nonatomic, strong, direct, nullable) NSSet<NSString *> *blocklist;

@end

@interface TimerConfig : BTracePluginConfig

@property(nonatomic, assign, direct) bool link;
@property(nonatomic, assign, direct) bool nstimer;
@property(nonatomic, assign, direct) bool dispatch;

@end

@interface ViewControllerConfig : BTracePluginConfig

@end

@interface ViewConfig : BTracePluginConfig

@property(nonatomic, assign, direct) bool viewProperty;
@property(nonatomic, assign, direct) bool ivarName;

@end

@interface DataConfig : BTracePluginConfig

@property(nonatomic, assign, direct) int jsonSerializeSize;
@property(nonatomic, assign, direct) int jsonDeserializeSize;
@property(nonatomic, assign, direct) int archiveSize;
@property(nonatomic, assign, direct) int unarchiveSize;

@end

@interface BTraceLynxConfig : BTracePluginConfig

@end

@interface BTraceTimeSeriesConfig : BTracePluginConfig

@property(nonatomic, strong, direct, nullable) AppStateConfig *appState;
@property(nonatomic, strong, direct, nullable) CellConfig *cell;
@property(nonatomic, strong, direct, nullable) ScrollConfig *scroll;
@property(nonatomic, strong, direct, nullable) TimerConfig *timer;
@property(nonatomic, strong, direct, nullable) ViewControllerConfig *vc;
@property(nonatomic, strong, direct, nullable) ViewConfig *view;
@property(nonatomic, strong, direct, nullable) DataConfig *data;
@property(nonatomic, strong, direct, nullable) LaunchConfig *launch;
@property(nonatomic, strong, direct, nullable) FirstFrameConfig *firstFrame;
@property(nonatomic, strong, direct, nullable) BTraceLynxConfig *lynx;

+ (nonnull instancetype)defaultConfig;

@end

#endif /* BTraceTimeSeriesConfig_h */

