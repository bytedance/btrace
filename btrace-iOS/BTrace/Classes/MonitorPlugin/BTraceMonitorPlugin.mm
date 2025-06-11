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
//  BTraceMonitorPlugin.m
//  Pods-PerfAnalysis_Core
//
//  Created by ByteDance on 2024/10/14.
//

#import <Foundation/Foundation.h>
#import <mach/mach.h>

#include <list>
#include <cstdint>
#include <thread>
#include <memory>
#include <numeric>
#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>

#import "BTrace.h"
#import "BTraceMonitorPlugin.h"
#import "BTraceMonitorConfig.h"
#import "BTraceTimeSeriesPlugin.h"

#include "BTraceDataBase.hpp"

#include "event_loop.hpp"

using namespace btrace;

namespace btrace {

extern int64_t start_time;
extern int64_t start_mach_time;

class CPUUsageMonitor {
public:
    static void Init(uint32_t period=3, uint32_t window=60,
                    float threshold=0.5, bool single_thread=true) {
        if (cpu_concurrency_ == 0) {
            cpu_concurrency_ = std::thread::hardware_concurrency();
        }
        
        period_ = std::max(period, (uint32_t)1);
        interval_ = std::max(window / period_, (uint32_t)1);
        threshold_ = threshold;
        single_thread_ = single_thread;

        if (single_thread_) {
            if (cpu_usage_history_map_ == nullptr) {
                cpu_usage_history_map_ = new std::unordered_map<ThreadId, std::list<float>>();
            }
        } else {
            threshold_ *= cpu_concurrency_;
        }
    }
    
    static void CleanUp() {
        total_usage_history_.clear();

        if (cpu_usage_history_map_ != nullptr) {
            delete cpu_usage_history_map_;
            cpu_usage_history_map_ = nullptr;
        }
    }
    
    static void Run() {        
        if (single_thread_) {
            auto alive_thread_set = std::unordered_set<ThreadId>();
            
            OSThreadIterator it;

            while (it.HasNext())
            {
                OSThread *t = it.Next();
                ThreadId tid = t->id();
                alive_thread_set.insert(tid);
                float curr_usage = t->cpu_usage();
                auto iter = cpu_usage_history_map_->find(tid);

                if (iter == cpu_usage_history_map_->end()) {
                    bool inserted;
                    std::tie(iter, inserted) = cpu_usage_history_map_->emplace(tid, std::list<float>());
                }

                auto &curr_cpu_usage_history = iter->second;
                check(curr_cpu_usage_history, curr_usage, tid);
            }
            
            for (auto it = cpu_usage_history_map_->begin(); it != cpu_usage_history_map_->end();)
            {
                ThreadId tid = it->first;
                
                if (alive_thread_set.count(tid) == 0)
                {
                    it = cpu_usage_history_map_->erase(it);
                }
                else {
                    ++it;
                }
            }
        } else {
            float total_usage = 0;
            OSThreadIterator it;

            while (it.HasNext())
            {
                OSThread *t = it.Next();
                total_usage += t->cpu_usage();
            }

            check(total_usage_history_, total_usage, 0);
        }
    }
    
    static uint32_t period() { return period_; }
    
private:
    static void check(std::list<float> &usage_history, float usage, ThreadId tid) {
        if (interval_ <= usage_history.size()) {
            
            float prev_total_usage = std::accumulate(usage_history.begin(),
                                                     usage_history.end(), 0.0);
            
            float avg_usage = prev_total_usage / usage_history.size();
            
            if (threshold_ < avg_usage) {
                usage_history.clear();
                dispatch_async([[BTrace shared] queue], ^{
                    uint64_t duration = period_ * interval_ * kMicrosPerSec;
                    uint64_t end_time = Utils::GetCurrentTimeMicros();
                    uint64_t begin_time = end_time - duration;
                    uint64_t rel_begin_time = begin_time - start_time;;
                    
                    NSDictionary *info = @{
                        @"begin_time": @(rel_begin_time),
                        @"duration": @(duration),
                        @"tid": @(tid)
                    };
                    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:info
                                                                       options:0
                                                                         error:nil];
                    NSString *jsonString = [[NSString alloc] initWithData:jsonData
                                                encoding:NSUTF8StringEncoding];
                    
                    [[BTrace shared] dumpWithTag:"cpu_usage"
                                               Info:[jsonString UTF8String]
                                          BeginTime:begin_time
                                            EndTime:end_time
                                                Tid:tid];
                });
            } else {
                usage_history.pop_front();
            }
        }
        
        usage_history.push_back(usage);
    }
    
    static inline uint32_t period_ = 3;
    static inline float threshold_ = 0.5;
    static inline uint32_t interval_ = 20;
    static inline bool single_thread_ = true;
    static inline uint32_t cpu_concurrency_ = 0;
    static inline std::list<float> total_usage_history_;
    static inline std::unordered_map<ThreadId, std::list<float>> *cpu_usage_history_map_ = nullptr;
};

class MemorySpikeMonitor {
public:
    static void Init(uint32_t period, uint32_t window, float threshold) {
        period_ = std::max(period, (uint32_t)1);
        interval_ = std::max(window / period_, (uint32_t)1);
        threshold_ = threshold;
    }
    
    static void Run() {
        uint64_t curr_usage = Utils::MemoryUsage();
        
        if (interval_ <= memory_usage_history_.size()) {
            uint64_t front = memory_usage_history_.front();
            
            float percent = (curr_usage - front) / (float)front;
            if (threshold_ < percent) {
                // Todo: dump data
                memory_usage_history_.clear();
            } else {
                memory_usage_history_.pop_front();
            }
        }
        
        memory_usage_history_.push_back(curr_usage);
    }
    
    static uint32_t period() { return period_; }
    
private:
    static inline uint32_t period_;
    static inline uint32_t interval_;
    static inline float threshold_;
    static inline std::list<uint64_t> memory_usage_history_;
};

class HangMonitor {
public:
    
    static void Init(int32_t hitch, int32_t hang, int32_t anr, int32_t anr_bg) {
        hitch_ = std::max(hitch, 0) * kMicrosPerMilli;
        hang_ = std::max(hang, 0) * kMicrosPerMilli;
        anr_ = std::max(anr, 0) * kMicrosPerSec;
        anr_bg_ = std::max(anr_bg, 0) * kMicrosPerSec;
        [[BTrace shared] markType:22];
    }
    
    static void RegisterObserver() {
        begin_observer_ = CFRunLoopObserverCreateWithHandler(kCFAllocatorDefault,
                                                            kCFRunLoopEntry | kCFRunLoopAfterWaiting,
                                                            true, LONG_MIN /* 优先级，越小越高, CATranstion 默认2000000 */,
                                                            ^(CFRunLoopObserverRef ob, CFRunLoopActivity activity) {
            begin_time_ = Utils::GetCurrMachMicros();
        });
        CFRunLoopAddObserver(CFRunLoopGetMain(), begin_observer_, kCFRunLoopCommonModes);
        
        end_observer_ = CFRunLoopObserverCreateWithHandler(kCFAllocatorDefault,
                                                          kCFRunLoopBeforeWaiting | kCFRunLoopExit,
                                                          true,
                                                          LONG_MAX /* 优先级，越小越高, CATranstion 默认2000000 */,
                                                          ^(CFRunLoopObserverRef ob, CFRunLoopActivity activity) {
            end_time_ = Utils::GetCurrMachMicros();
            
            if (begin_time_ < end_time_) {
                const char *tag = nullptr;
                
                uint64_t diff_time = end_time_ - begin_time_;
                
                if (hang_ && hang_ < diff_time) {
                    tag = "hang";
                }
                else if (hitch_ && hitch_ < diff_time) {
                    tag = "hitch";
                }
                
                if (!tag) {
                    return;
                }
                
                int64_t rel_begin_time = begin_time_ - start_mach_time;
                int64_t rel_end_time = end_time_ - start_mach_time;
                
                dispatch_async([[BTrace shared] queue], ^{
#if DEBUG || INHOUSE_TARGET || TEST_MODE || READING_DEV
                    BTraceTimeSeriesData *data = [[BTraceTimeSeriesData alloc] initWithType:@"hitch" Dictionary:@{
                        @"begin_time": @(rel_begin_time),
                        @"end_time": @(rel_end_time),
                        @"perfetto_type": @"slice",
//                        @"scene": scene
                    }];
                    [[BTraceTimeSeriesPlugin shared] SaveTimeSeriesData:data];
#else
                    NSDictionary *info = @{
                        @"begin_time": @(rel_begin_time),
                        @"duration": @(diff_time),
                        // @"scene": scene
                    };
                    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:info
                                                                            options:0
                                                                                error:nil];
                    NSString *jsonString = [[NSString alloc] initWithData:jsonData
                                                encoding:NSUTF8StringEncoding];
                    
                    uint64_t begin_time = rel_begin_time + start_time;
                    uint64_t end_time = rel_end_time + start_time;
                    
                    [[BTrace shared] dumpWithTag:tag
                                               Info:[jsonString UTF8String]
                                          BeginTime:begin_time
                                            EndTime:end_time];
#endif
                });
            }
        });
        CFRunLoopAddObserver(CFRunLoopGetMain(), end_observer_, kCFRunLoopCommonModes);
    }
    
    static void RemoveObserver() {
        CFRunLoopRemoveObserver(CFRunLoopGetMain(), begin_observer_, kCFRunLoopCommonModes);
        CFRunLoopRemoveObserver(CFRunLoopGetMain(), end_observer_, kCFRunLoopCommonModes);
        CFRelease(begin_observer_);
        CFRelease(end_observer_);
        begin_observer_ = nullptr;
        end_observer_ = nullptr;
    }
    
    static void Run() {
        int app_state = [BTrace shared].appState;
        int64_t thres = anr_;
        
        if (app_state == 1) {
            thres = anr_bg_;
        }
        
        if (thres && end_time_ < begin_time_) {
            int64_t curr_time = Utils::GetCurrMachMicros();
            int64_t diff_time = curr_time - begin_time_;
            
            if (diff_time < thres) {
                return;
            }
            
            int64_t rel_begin_time = begin_time_ - start_mach_time;
            int64_t rel_app_state_time = [BTrace shared].appStateTime - start_mach_time;

            begin_time_ = curr_time;

            dispatch_async([[BTrace shared] queue], ^{
                NSDictionary *info = @{
                    @"begin_time": @(rel_begin_time),
                    @"duration": @(diff_time),
                    // @"scene": scene,
                    @"app_state": @(app_state),
                    @"app_state_time": @(rel_app_state_time)
                };
                NSData *json_data = [NSJSONSerialization dataWithJSONObject:info
                                                                       options:0
                                                                         error:nil];
                NSString *json_string = [[NSString alloc] initWithData:json_data
                                          encoding:NSUTF8StringEncoding];
                [[BTrace shared] dumpWithTag:"anr" Info:[json_string UTF8String]];
            });
        }
    }
    
private:
    static inline int32_t hitch_;
    static inline int32_t hang_;
    static inline int32_t anr_;
    static inline int32_t anr_bg_;
    static inline std::atomic<int64_t> begin_time_ = INT64_MAX;
    static inline std::atomic<int64_t> end_time_ = 0;
    static inline CFRunLoopObserverRef begin_observer_ = nullptr;
    static inline CFRunLoopObserverRef end_observer_ = nullptr;
};

}

@interface BTraceMonitorPlugin ()
@end

__attribute__((objc_direct_members))
@implementation BTraceMonitorPlugin

+ (NSString *)name {
    return @"monitor";
}

+ (instancetype)shared {
    static BTraceMonitorPlugin *instance = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      instance = [[self alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];

    return self;
}

- (void)start {
    if (!self.btrace.enable) {
        return;
    }

    if (!self.config) {
        self.config = [BTraceMonitorConfig defaultConfig];
    }

    auto config = (BTraceMonitorConfig *)self.config;
    
    if (config.cpu_usage) {
        auto cpu_usage = config.cpu_usage;
        CPUUsageMonitor::Init(cpu_usage.period, cpu_usage.window,
                              cpu_usage.threshold, cpu_usage.single_thread);
        EventLoop::Register(CPUUsageMonitor::period()*kMillisPerSec, CPUUsageMonitor::Run);
    }
    
    // if (config.cpu_level) {
    //     CPULevelMonitor::Init(config.cpu_level.duration);
    //     CPULevelMonitor::RegisterObserver();
    // }
    
    if (config.mem_spike) {
        auto mem_spike = config.mem_spike;
        MemorySpikeMonitor::Init(mem_spike.period, mem_spike.window, mem_spike.threshold);
        EventLoop::Register(MemorySpikeMonitor::period()*kMillisPerSec, MemorySpikeMonitor::Run);
    }
    
    if (config.hang) {
        auto hang = config.hang;
        HangMonitor::Init(hang.hitch, hang.hang, hang.anr, hang.anr_bg);
        HangMonitor::RegisterObserver();
        EventLoop::Register(1 * kMillisPerSec, HangMonitor::Run);
    }
}

- (void)stop {
    if (!self.btrace.enable) {
        return;
    }
    
    auto config = (BTraceMonitorConfig *)self.config;
    
//    if (s_cpu_level_observer) {
//        CPULevelMonitor::RemoveObserver();
//    }
    
    if (config.hang) {
        HangMonitor::RemoveObserver();
    }

    if (config.cpu_usage) {
        CPUUsageMonitor::CleanUp();
    }
}

@end
