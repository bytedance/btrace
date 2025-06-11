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

#import <Foundation/Foundation.h>

#import "BTraceTimeSeriesConfig.h"

@implementation FirstFrameConfig

@end

@implementation LaunchConfig

@end

@implementation AppStateConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    if (data) {
        
    }
    
    return self;
}

@end

@implementation CellConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    if (data) {
        
    }
    
    return self;
}

@end


@implementation ScrollConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    NSMutableSet *defaultBlocklist = [NSMutableSet setWithArray:@[@"WKScrollView"]];
    
    if (data) {
        NSArray *remoteList = [data objectForKey:@"blocklist"];
        [defaultBlocklist unionSet:[NSSet setWithArray:remoteList]];
    }
    
    _blocklist = defaultBlocklist;
    
    return self;
}

@end

@implementation TimerConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    if (data) {
        id link_oc = [data objectForKey:@"link"];
        if (link_oc == nil) {
            _link = true;
        } else {
            _link = [link_oc boolValue];
        }
        id nstimer_oc = [data objectForKey:@"nstimer"];
        if (nstimer_oc == nil) {
            _nstimer = true;
        } else {
            _nstimer = [nstimer_oc boolValue];
        }
        id dispatch_oc = [data objectForKey:@"dispatch"];
        if (dispatch_oc == nil) {
            _dispatch = true;
        } else {
            _dispatch = [dispatch_oc boolValue];
        }
    }
    
    return self;
}

@end

@implementation ViewControllerConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    if (data) {
    }
    
    return self;
}

@end

@implementation ViewConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    if (data) {
        _viewProperty = [[data objectForKey:@"viewProperty"] boolValue];
        _ivarName = [[data objectForKey:@"ivarName"] boolValue];
    }
    
    return self;
}

@end

@implementation DataConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    if (data) {
        _jsonSerializeSize = [[data objectForKey:@"jsonSerializeSize"] intValue];
        _jsonDeserializeSize = [[data objectForKey:@"jsonDeserializeSize"] intValue];
        _archiveSize = [[data objectForKey:@"archiveSize"] intValue];
        _unarchiveSize = [[data objectForKey:@"unarchiveSize"] intValue];
    }
    
    return self;
}

@end

@implementation BTraceLynxConfig

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    if (data) {
    }
    
    return self;
}

@end

@implementation BTraceTimeSeriesConfig

+ (instancetype)defaultConfig {
    BTraceTimeSeriesConfig *config = [[BTraceTimeSeriesConfig alloc] init];
    config.appState = nil;
    config.cell = nil;
    config.scroll = nil;
    config.timer = nil;
    config.vc = nil;
    config.view = nil;
    config.data = nil;
    config.launch = nil;
    config.firstFrame = nil;
    config.lynx = nil;
    
    return config;
}

- (instancetype)initWithDictionary:(NSDictionary *)data {
    self = [super init];
    
    if (data) {
        NSDictionary *appStateConfig = [data objectForKey:@"app_state"];
        if (appStateConfig) {
            self.appState = [[AppStateConfig alloc] initWithDictionary:[appStateConfig objectForKey:@"config"]];
        }
        NSDictionary *cellConfig = [data objectForKey:@"cell"];
        if (cellConfig) {
            self.cell = [[CellConfig alloc] initWithDictionary:[cellConfig objectForKey:@"config"]];
        }
        NSDictionary *scrollConfig = [data objectForKey:@"scroll"];
        if (scrollConfig) {
            self.scroll = [[ScrollConfig alloc] initWithDictionary:[scrollConfig objectForKey:@"config"]];
        }
        NSDictionary *timerConfig = [data objectForKey:@"timer"];
        if (timerConfig) {
            self.timer = [[TimerConfig alloc] initWithDictionary:[timerConfig objectForKey:@"config"]];
        }
        NSDictionary *vcConfig = [data objectForKey:@"vc"];
        if (vcConfig) {
            self.vc = [[ViewControllerConfig alloc] initWithDictionary:[vcConfig objectForKey:@"config"]];
        }
        NSDictionary *viewConfig = [data objectForKey:@"view"];
        if (viewConfig) {
            self.view = [[ViewConfig alloc] initWithDictionary:[viewConfig objectForKey:@"config"]];
        }
        NSDictionary *dataConfig = [data objectForKey:@"data"];
        if (dataConfig) {
            self.data = [[DataConfig alloc] initWithDictionary:[dataConfig objectForKey:@"config"]];
        }
        NSDictionary *launchConfig = [data objectForKey:@"launch"];
        if (launchConfig) {
            self.launch = [[LaunchConfig alloc] initWithDictionary:[launchConfig objectForKey:@"config"]];
        }
        NSDictionary *firstFrameConfig = [data objectForKey:@"first_frame"];
        if (firstFrameConfig) {
            self.firstFrame = [[FirstFrameConfig alloc] initWithDictionary:[firstFrameConfig objectForKey:@"config"]];
        }
        NSDictionary *lynxConfig = [data objectForKey:@"lynx"];
        if (lynxConfig) {
            self.lynx = [[BTraceLynxConfig alloc] initWithDictionary:[lynxConfig objectForKey:@"config"]];
        }
    }

    return self;
}

@end
