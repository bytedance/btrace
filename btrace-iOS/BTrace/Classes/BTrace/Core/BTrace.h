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
//  BTrace.h
//  BTrace
//
//  Created by Bytedance.
//

#import <Foundation/Foundation.h>

typedef struct BTraceDataBase BTraceDataBase;
typedef void (^BTraceCallback)(NSData *data);

@interface BTrace : NSObject

@property(nonatomic, assign, direct) BOOL enable;
@property(atomic, assign, direct) BOOL running;
@property(atomic, assign, direct) int appState;
@property(atomic, assign, direct) int64_t appStateTime;
@property(nonatomic, assign, direct) int bufferSize;
@property(nonatomic, assign, direct) int64_t startTime;
@property(nonatomic, assign, direct) int64_t endTime;
@property(nonatomic, assign, direct) double sdkInitTime;
@property(nonatomic, strong, direct) dispatch_queue_t queue;
@property(nonatomic, assign, direct, nonnull) BTraceDataBase * db;

+ (nonnull instancetype)shared __attribute__((objc_direct));

- (void)setupWithConfig:(NSDictionary * _Nullable)config __attribute__((objc_direct));

- (void)setDataCompletionCallback:(BTraceCallback _Nullable)block;

- (void)start;

- (void)dump __attribute__((objc_direct));

- (void)dumpWithTag:(const char * _Nullable)tag __attribute__((objc_direct));

- (void)dumpWithTag:(const char * _Nullable)tag Info:(const char * _Nullable)info __attribute__((objc_direct));

- (void)dumpWithTag:(const char * _Nullable)tag
               Info:(const char * _Nullable)info
          BeginTime:(int64_t)begin
            EndTime:(int64_t)end __attribute__((objc_direct));

- (void)dumpWithTag:(const char * _Nullable)tag
               Info:(const char * _Nullable)info
          BeginTime:(int64_t)begin
            EndTime:(int64_t)end
                Tid:(mach_port_t)tid __attribute__((objc_direct));

- (void)stop;

- (void)stopWithCallback:(BTraceCallback _Nullable)block;

- (void)markType:(int64_t)type __attribute__((objc_direct));

@end
