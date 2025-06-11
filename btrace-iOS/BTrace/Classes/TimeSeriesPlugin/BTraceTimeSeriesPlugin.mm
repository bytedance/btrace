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
//  BTraceProfilerPlugin.m
//  Pods-PerfAnalysis_Core
//
//  Created by ByteDance on 2024/1/8.
//

#import <dlfcn.h>
#include <unistd.h>
#include <sys/uio.h>

#import <mach-o/dyld.h>
#import <objc/runtime.h>
#import <Foundation/Foundation.h>

#include <atomic>

#import "BTrace.h"
#import "InternalUtils.h"
#import "BTraceTimeSeriesConfig.h"
#import "BTraceTimeSeriesPlugin.h"
#import "BTraceTimeSeriesData.h"

#include "utils.hpp"
#include "os_thread.hpp"
#include "event_loop.hpp"
#include "concurrentqueue.hpp"
#include "callstack_table.hpp"
#include "BTraceRecord.hpp"
#include "TimeSeriesModel.hpp"

#include "BTraceDataBase.hpp"

constexpr static int BUFFER_MAX_SIZE = 16 * KB;
constexpr static double INTERVAL_THRESHOLD = 0.5;
static const char *s_gcd_timer_interval_key = "btrace_gcd_timer_interval";

namespace btrace {
    extern uint8_t trace_id;
    extern int64_t start_mach_time;

    static BTraceTimeSeriesPlugin *s_plugin = nullptr;
    static std::atomic<bool> s_main_running = false;
    static std::atomic<uint32_t> *s_working_thread = nullptr;

    struct TimeSeriesData {
        uint32_t tid;
        int64_t timestamp;
        std::string type;
        std::string info;
        
        TimeSeriesData(uint32_t tid, int64_t timestamp,
                       const char *type, const char *info):
                       tid(tid), timestamp(timestamp), type(type),
                       info(info) {}
    };
    
    using TimeSeriesBuffer = moodycamel::ConcurrentQueue<std::shared_ptr<TimeSeriesData>>;

    static TimeSeriesBuffer *s_buffer = nullptr;

    void ProcessTimeSeriesBuffer();

#if DEBUG || INHOUSE_TARGET || TEST_MODE || READING_DEV
    void CPUUsageInfo();
    void MemUsageInfo();
#endif

    struct BlockLiteral {
        void *isa; // initialized to &_NSConcreteStackBlock or &_NSConcreteGlobalBlock
        int flags;
        int reserved;
        void (*invoke)(void *, ...);
        struct block_descriptor {
            unsigned long int reserved;    // NULL
            unsigned long int size;         // sizeof(struct Block_literal_1)
            // optional helper functions
            void (*copy_helper)(void *dst, void *src);     // IFF (1<<25)
            void (*dispose_helper)(void *src);             // IFF (1<<25)
            // required ABI.2010.3.16
            const char *signature;                         // IFF (1<<30)
        } *descriptor;
        // imported variables
    };
}

using namespace btrace;

@interface BTraceTimeSeriesPlugin ()

- (void) btrace_viewDidAppear:(BOOL)animated;

- (void) btrace_viewDidDisappear:(BOOL)animated;

- (void) btrace_viewWillAppear:(BOOL)animated;

- (void) btrace_viewWillDisappear:(BOOL)animated;

- (id)initWithFrame_btrace:(CGRect)frame;

- (void)btrace_layoutSubviews;

- (void)btrace_drawRect:(CGRect)rect;

- (void)btrace_willMoveToSuperview:(nullable UIView *)newSuperview;

- (void)btrace_willMoveToWindow:(nullable UIWindow *)newWindow;

- (void)btrace_setFrame:(CGRect)rect;

- (void)btrace_setHidden:(BOOL)hidden;

- (void)btrace_setAlpha:(CGFloat)alpha;

- (void)btrace_setClipsToBounds:(BOOL)clipsToBounds;

- (void) applicationBecomeActive:(NSNotification *) note;

- (void) applicationWillEnterForeground:(NSNotification *) note;

- (void) applicationDidEnterBackground:(NSNotification *) note;

- (void) btrace_collectionView:(UICollectionView *)collectionView willDisplayCell:(UITableViewCell *)cell forRowAtIndexPath:(NSIndexPath *)indexPath;

- (void) btrace_collectionView:(UICollectionView *)collectionView didEndDisplayingCell:(UITableViewCell *)cell forRowAtIndexPath:(NSIndexPath *)indexPath;

- (void) btrace_tableView:(UITableView *)tableView willDisplayCell:(UITableViewCell *)cell forRowAtIndexPath:(NSIndexPath *)indexPath;

- (void) btrace_tableView:(UITableView *)tableView didEndDisplayingCell:(UITableViewCell *)cell forRowAtIndexPath:(NSIndexPath *)indexPath;

+ (CADisplayLink *)btrace_displayLinkWithTarget:(id)target selector:(SEL)sel;

- (void)btrace_displayLinkAddToRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode;

- (void)btrace_displayLinkSetPaused:(BOOL)paused;

- (void)btrace_displayLinkDealloc;

+ (NSTimer *)btrace_scheduledTimerWithTimeInterval:(NSTimeInterval)ti target:(id)aTarget selector:(SEL)aSelector userInfo:(nullable id)userInfo repeats:(BOOL)yesOrNo;

+ (NSTimer *)btrace_timerWithTimeInterval:(NSTimeInterval)ti target:(id)aTarget selector:(SEL)aSelector userInfo:(nullable id)userInfo repeats:(BOOL)yesOrNo;

+ (NSTimer *)btrace_timerWithTimeInterval:(NSTimeInterval)ti invocation:(NSInvocation *)invocation repeats:(BOOL)yesOrNo;

+ (NSTimer *)btrace_scheduledTimerWithTimeInterval:(NSTimeInterval)ti invocation:(NSInvocation *)invocation repeats:(BOOL)yesOrNo;

+ (NSTimer *)btrace_timerWithTimeInterval:(NSTimeInterval)interval repeats:(BOOL)repeats block:(void (^)(NSTimer *timer))block;

+ (NSTimer *)btrace_scheduledTimerWithTimeInterval:(NSTimeInterval)interval repeats:(BOOL)repeats block:(void (^)(NSTimer *timer))block;

- (void)btrace_NSTimerInvalidate;

- (void)btrace_NSTimerSetFireDate:(NSDate *)date;

- (void)btrace_addTimer:(NSTimer *)timer forMode:(NSRunLoopMode)mode;

+ (nullable NSData *)btrace_dataWithJSONObject:(id)obj options:(NSJSONWritingOptions)opt error:(NSError **)error;
+ (nullable id)btrace_JSONObjectWithData:(NSData *)jsonObject options:(NSJSONReadingOptions)opt error:(NSError **)error;
+ (nullable NSData *)btrace_archivedDataWithRootObject:(id)object requiringSecureCoding:(BOOL)requiresSecureCoding error:(NSError **)error;
+ (nullable id)btrace_unarchivedObjectOfClass:(Class)cls fromData:(NSData *)data error:(NSError **)error;

@end


BTRACE_HOOK(dispatch_source_t, dispatch_source_create, dispatch_source_type_t type, uintptr_t handle, uintptr_t mask, dispatch_queue_t _Nullable queue) {
    dispatch_source_t result = ORI(dispatch_source_create)(type, handle, mask, queue);
    
    if (type == DISPATCH_SOURCE_TYPE_TIMER && s_plugin && s_plugin.running) {
        uintptr_t objId = (uintptr_t)result;
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.dispatch) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:@"timer" Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"dispatch",
                @"action": @"create"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    return result;
}

BTRACE_HOOK(void, dispatch_source_set_timer, dispatch_source_t source, dispatch_time_t start, uint64_t interval, uint64_t leeway) {
    ORI(dispatch_source_set_timer)(source, start, interval, leeway);
    
    uintptr_t objId = (uintptr_t)source;
    double interval_s = (double)interval/NSEC_PER_SEC;
    if (interval_s < INTERVAL_THRESHOLD && s_plugin && s_plugin.running) {
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        NSString *dataType = @"timer";
        if (config.timer.dispatch) {
            objc_setAssociatedObject(source, s_gcd_timer_interval_key, @(interval_s), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"dispatch",
                @"action": @"setup",
                @"interval": @(interval_s)
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

BTRACE_HOOK(void, dispatch_source_set_event_handler, dispatch_source_t source, dispatch_block_t handler) {
    ORI(dispatch_source_set_event_handler)(source, handler);
    
    uintptr_t objId = (uintptr_t)source;

    if (s_plugin && s_plugin.running) {
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        double interval = INT32_MAX;
        id interval_oc = objc_getAssociatedObject(source, s_gcd_timer_interval_key);
        
        if ([interval_oc isKindOfClass:[NSNumber class]]) {
            interval = [interval_oc doubleValue];
        }
        
        if (config.timer.dispatch && interval < INTERVAL_THRESHOLD) {
            struct BlockLiteral *blockRef = (__bridge struct BlockLiteral *)handler;
            void *invoke = (void *)blockRef->invoke;
            
            NSString *name = [NSString stringWithFormat:@"%p", invoke];
            
            #if DEBUG || TEST_MODE
            Dl_info info;
            if (dladdr(invoke, &info) != 0 && info.dli_sname) {
                name = [NSString stringWithUTF8String:info.dli_sname];
            }
            #endif
            
            NSString *dataType = @"timer";
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"dispatch",
                @"action": @"create",
                @"name": name,
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

BTRACE_HOOK(void, dispatch_resume, dispatch_object_t object) {
    if (s_plugin && s_plugin.running) {
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        double interval = INT32_MAX;
        id interval_oc = objc_getAssociatedObject(object, s_gcd_timer_interval_key);
        
        if ([interval_oc isKindOfClass:[NSNumber class]]) {
            interval = [interval_oc doubleValue];
        }
        
        if (config.timer.dispatch && interval < INTERVAL_THRESHOLD) {
            NSString *dataType = @"timer";
            uintptr_t objId = (uintptr_t)object;
            
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"dispatch",
                @"action": @"start"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    ORI(dispatch_resume)(object);
}

BTRACE_HOOK(void, dispatch_suspend, dispatch_object_t object) {
    if (s_plugin && s_plugin.running) {
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        double interval = INT32_MAX;
        id interval_oc = objc_getAssociatedObject(object, s_gcd_timer_interval_key);
        
        if ([interval_oc isKindOfClass:[NSNumber class]]) {
            interval = [interval_oc doubleValue];
        }
        
        if (config.timer.dispatch && interval < INTERVAL_THRESHOLD) {
            NSString *dataType = @"timer";
            uintptr_t objId = (uintptr_t)object;
            
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"dispatch",
                @"action": @"stop"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    ORI(dispatch_suspend)(object);
}

BTRACE_HOOK(void, dispatch_source_cancel, dispatch_source_t source) {
    if (s_plugin && s_plugin.running) {
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        double interval = INT32_MAX;
        id interval_oc = objc_getAssociatedObject(source, s_gcd_timer_interval_key);
        
        if ([interval_oc isKindOfClass:[NSNumber class]]) {
            interval = [interval_oc doubleValue];
        }
        
        if (config.timer.dispatch && interval < INTERVAL_THRESHOLD) {
            NSString *dataType = @"timer";
            uintptr_t objId = (uintptr_t)source;
            
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"dispatch",
                @"action": @"destroy"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    ORI(dispatch_source_cancel)(source);
}


__attribute__((objc_direct_members))
@implementation BTraceTimeSeriesPlugin

+ (NSString *)name {
    return @"timeseries";
}

+ (instancetype)shared {
    static BTraceTimeSeriesPlugin *instance = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        instance = [[self alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];
    
    if (self) {
        _running = NO;
        
        if (s_working_thread == nullptr) {
            s_working_thread = new std::atomic<uint32_t>();
        }
        
        s_plugin = self;
    }
    return self;
}

- (void)updateConfig:(BTracePluginConfig *)config {
    self.config = config;
    
    BTraceTimeSeriesConfig *timeSeriesConfig = (BTraceTimeSeriesConfig *)self.config;

    if (timeSeriesConfig.vc) {
        [self monitorViewController];
    }
    if (timeSeriesConfig.view) {
        [self monitorView];
    }
    if (timeSeriesConfig.view.viewProperty) {
        [self monitorViewProperty];
    }
    if (timeSeriesConfig.appState) {
        [self monitorAppStatus];
    }
    if (timeSeriesConfig.timer) {
        [self monitorTimer];
    }
    if (timeSeriesConfig.data) {
        [self monitorData];
    }
}

- (void)start {
    if (!self.btrace.enable) {
        return;
    }
    
    if (_running) {
        return;
    }
    
    if (![self initTable]) {
        return;
    }
    
    if (!self.config) {
        self.config = [BTraceTimeSeriesConfig defaultConfig];
    }

    if (s_buffer == nullptr) {
        s_buffer = new TimeSeriesBuffer();
    }
    
    _running = YES;
    
    auto config = (BTraceTimeSeriesConfig *)self.config;
    
    EventLoop::Register(10 * kMillisPerSec, ProcessTimeSeriesBuffer);
    
#if DEBUG || INHOUSE_TARGET || TEST_MODE || READING_DEV
    EventLoop::Register(500, CPUUsageInfo);
    EventLoop::Register(500, MemUsageInfo);
#endif
    
    [self saveVCArrayForAction:@"start"];
}

- (void)stop {
    if (!self.btrace.enable) {
        return;
    }
    
    if (!_running) {
        return;
    }
    
    _running = NO;
    
    while (s_working_thread->load() || s_main_running) {
        Utils::WaitFor(1);
    }
    
    ProcessTimeSeriesBuffer();
    delete s_buffer;
    s_buffer = nullptr;
}

- (void)SaveTimeSeriesData:(BTraceTimeSeriesData * _Nullable)data {
    if (!self.running) {
        return;
    }
        
    ScopedCounter counter(*s_working_thread, s_main_running, false);
        
    if (!self.running) {
        return;
    }
        
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:data.info
                                                            options:0
                                                                error:nil];
    NSString *jsonString = [[NSString alloc] initWithData:jsonData
                                encoding:NSUTF8StringEncoding];

    auto time_series_data = std::make_shared<TimeSeriesData>(data.tid, data.timestamp, [data.type UTF8String], [jsonString UTF8String]);

    s_buffer->enqueue(time_series_data);
}

- (bool)initTable {
    bool result = true;
    
    return result;
}

- (void)monitorViewController {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        Class ViewController = NSClassFromString(@"UIViewController");
        
        swizzleMethod(ViewController,
                      @selector(viewWillAppear:),
                      self.class,
                      @selector(btrace_viewWillAppear:),
                      YES);
        
        swizzleMethod(ViewController,
                      @selector(viewWillDisappear:),
                      self.class,
                      @selector(btrace_viewWillDisappear:),
                      YES);
    });
}

- (void)monitorView {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        Class UIView = NSClassFromString(@"UIView");
        if (!UIView) return;
        
        swizzleMethod(UIView,
                      NSSelectorFromString(@"initWithFrame:"),
                      self.class,
                      NSSelectorFromString(@"initWithFrame_btrace:"),
                      YES);
        swizzleMethod(UIView,
                      NSSelectorFromString(@"layoutSubviews"),
                      self.class,
                      NSSelectorFromString(@"btrace_layoutSubviews"),
                      YES);
        swizzleMethod(UIView,
                      NSSelectorFromString(@"drawRect:"),
                      self.class,
                      NSSelectorFromString(@"btrace_drawRect:"),
                      YES);
        swizzleMethod(UIView,
                      NSSelectorFromString(@"willMoveToSuperview:"),
                      self.class,
                      NSSelectorFromString(@"btrace_willMoveToSuperview:"),
                      YES);
        swizzleMethod(UIView,
                      NSSelectorFromString(@"willMoveToWindow:"),
                      self.class,
                      NSSelectorFromString(@"btrace_willMoveToWindow:"),
                      YES);
    });
}

- (void)monitorData {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        Class NSJSONSerialization = NSClassFromString(@"NSJSONSerialization");
        if (NSJSONSerialization) {
            swizzleMethod(NSJSONSerialization,
                          NSSelectorFromString(@"dataWithJSONObject:options:error:"),
                          self.class,
                          NSSelectorFromString(@"btrace_dataWithJSONObject:options:error:"),
                          NO);
            swizzleMethod(NSJSONSerialization,
                          NSSelectorFromString(@"JSONObjectWithData:options:error:"),
                          self.class,
                          NSSelectorFromString(@"btrace_JSONObjectWithData:options:error:"),
                          NO);
        }
        
        Class NSKeyedArchiver = NSClassFromString(@"NSKeyedArchiver");
        if (NSKeyedArchiver) {
            swizzleMethod(NSKeyedArchiver,
                          NSSelectorFromString(@"archivedDataWithRootObject:requiringSecureCoding:error:"),
                          self.class,
                          NSSelectorFromString(@"btrace_archivedDataWithRootObject:requiringSecureCoding:error:"),
                          NO);
        }
        Class NSKeyedUnarchiver = NSClassFromString(@"NSKeyedUnarchiver");
        if (NSKeyedUnarchiver) {
            swizzleMethod(NSKeyedUnarchiver,
                          NSSelectorFromString(@"unarchivedObjectOfClass:fromData:error:"),
                          self.class,
                          NSSelectorFromString(@"btrace_unarchivedObjectOfClass:fromData:error:"),
                          NO);
        }
    });
}

- (void)monitorViewProperty {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        Class UIView = NSClassFromString(@"UIView");
        if (!UIView) return;
        
        swizzleMethod(UIView,
                      NSSelectorFromString(@"setFrame:"),
                      self.class,
                      NSSelectorFromString(@"btrace_setFrame:"),
                      YES);
        swizzleMethod(UIView,
                      NSSelectorFromString(@"setHidden:"),
                      self.class,
                      NSSelectorFromString(@"btrace_setHidden:"),
                      YES);
        swizzleMethod(UIView,
                      NSSelectorFromString(@"setAlpha:"),
                      self.class,
                      NSSelectorFromString(@"btrace_setAlpha:"),
                      YES);
        swizzleMethod(UIView,
                      NSSelectorFromString(@"setClipsToBounds:"),
                      self.class,
                      NSSelectorFromString(@"btrace_setClipsToBounds:"),
                      YES);
    });
}

- (void)saveVCArrayForAction:(NSString *)action {
    NSString *dataType = @"vc";
    
    BTraceTimeSeriesConfig *config = (BTraceTimeSeriesConfig *)self.config;
    
    if (config.vc) {
        
        NSArray<UIViewController *> *vcArray = [self currentVCArray];
    
        for(UIViewController *vc in vcArray) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(vc.class),
                @"id": @((uintptr_t)vc),
                @"parent": @((uintptr_t)vc.parentViewController),
                @"action": action
            }];

            [self SaveTimeSeriesData:data];
        }
    }
}

- (NSArray<UIViewController *> *)currentVCArray
{
    
    NSMutableArray<UIViewController *> *result = [NSMutableArray array];
    
    UIWindow *window = [[UIApplication sharedApplication] keyWindow];

    UIViewController *controller = window.rootViewController;
    if (!controller) {
        return result;
    }
    
    [result addObject:controller];

    while (YES) {
        if (controller.presentedViewController) {
            controller = controller.presentedViewController;
        } else {
            if ([controller isKindOfClass:[UINavigationController class]]) {
                controller = [(UINavigationController *)controller visibleViewController];
            } else if ([controller isKindOfClass:[UITabBarController class]]) {
                controller = ((UITabBarController *)controller).selectedViewController;
            } else {
                if (controller.childViewControllers.count > 0) {
                    controller = [controller.childViewControllers lastObject];
                } else {
                    break;
                }
            }
        }
        if (!controller) {
            break;
        }
        [result addObject:controller];
    }

    return result;
}

- (void)btrace_viewDidAppear:(BOOL)animated {
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"vc";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIViewController *vc = (UIViewController *)self;
        if (config.vc) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(vc.class),
                @"id": @((uintptr_t)vc),
                @"parent": @((uintptr_t)vc.parentViewController),
                @"action": @"appear"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    
    [self btrace_viewDidAppear:animated];
}

- (void)btrace_viewDidDisappear:(BOOL)animated {
    [self btrace_viewDidDisappear:animated];
    
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"vc";
    
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIViewController *vc = (UIViewController *)self;
        if (config.vc) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(vc.class),
                @"id": @((uintptr_t)vc),
                @"parent": @((uintptr_t)vc.parentViewController),
                @"action": @"disappear"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void)btrace_viewWillAppear:(BOOL)animated {
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"vc";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIViewController *vc = (UIViewController *)self;
        if (config.vc) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(vc.class),
                @"id": @((uintptr_t)vc),
                @"view_id": @((uintptr_t)vc.view),
                @"parent": @((uintptr_t)vc.parentViewController),
                @"action": @"appear"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    
    [self btrace_viewWillAppear:animated];
}

- (void)btrace_viewWillDisappear:(BOOL)animated {
    [self btrace_viewWillDisappear:animated];
    
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"vc";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIViewController *vc = (UIViewController *)self;
        if (config.vc) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(vc.class),
                @"id": @((uintptr_t)vc),
                @"parent": @((uintptr_t)vc.parentViewController),
                @"action": @"disappear",
                @"view_name": NSStringFromClass(vc.view.class),
                @"view_id": @((uintptr_t)vc.view),
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (id)initWithFrame_btrace:(CGRect)frame {
    auto start = Utils::GetCurrentTimeMicros();
    id result = [self initWithFrame_btrace:frame];
    auto end = Utils::GetCurrentTimeMicros();

    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"view";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIView *view = (UIView *)self;
        if (config.view) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(view.class),
                @"id": @((uintptr_t)view),
                @"parent": @((uintptr_t)view.superview),
                @"action": @"initialize",
                @"start": @(start),
                @"end": @(end),
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    return result;
}

- (void)btrace_layoutSubviews {
    auto start = Utils::GetCurrentTimeMicros();
    [self btrace_layoutSubviews];
    auto end = Utils::GetCurrentTimeMicros();

    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"view";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIView *view = (UIView *)self;
        if (config.view) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(view.class),
                @"id": @((uintptr_t)view),
                @"parent": @((uintptr_t)view.superview),
                @"action": @"layout",
                @"start": @(start),
                @"end": @(end),
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void)btrace_drawRect:(CGRect)rect {
    auto start = Utils::GetCurrentTimeMicros();
    [self btrace_drawRect:rect];
    auto end = Utils::GetCurrentTimeMicros();

    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"view";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIView *view = (UIView *)self;
        if (config.view) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(view.class),
                @"id": @((uintptr_t)view),
                @"parent": @((uintptr_t)view.superview),
                @"action": @"draw",
                @"start": @(start),
                @"end": @(end),
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void)btrace_willMoveToSuperview:(nullable UIView *)newSuperview {
    auto start = Utils::GetCurrentTimeMicros();
    [self btrace_willMoveToSuperview:newSuperview];
    auto end = Utils::GetCurrentTimeMicros();

    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"view";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIView *view = (UIView *)self;
        if (config.view) {
            NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithDictionary:@{
                @"name": NSStringFromClass(view.class),
                @"id": @((uintptr_t)view),
                @"parent": @((uintptr_t)view.superview),
                @"container": @((uintptr_t)newSuperview),
                @"action": @"moveToSuperView",
                @"start": @(start),
                @"end": @(end),
            }];
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary:dict.copy];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void)btrace_willMoveToWindow:(nullable UIWindow *)newWindow {
    auto start = Utils::GetCurrentTimeMicros();
    [self btrace_willMoveToWindow:newWindow];
    auto end = Utils::GetCurrentTimeMicros();

    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"view";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIView *view = (UIView *)self;
        if (config.view) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(view.class),
                @"id": @((uintptr_t)view),
                @"parent": @((uintptr_t)view.superview),
                @"container": @((uintptr_t)newWindow),
                @"action": @"moveToWindow",
                @"start": @(start),
                @"end": @(end),
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void)btrace_setFrame:(CGRect)rect {
    [self btrace_setFrame:rect];

    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"view";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIView *view = (UIView *)self;
        if (config.view && config.view.viewProperty) {
            if (![self checkIsInvalidFrame:rect]) {
                auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                    @"name": NSStringFromClass(view.class),
                    @"id": @((uintptr_t)view),
                    @"parent": @((uintptr_t)view.superview),
                    @"action": @"setFrame",
                    @"value": @[@(rect.origin.x), @(rect.origin.y), @(rect.size.width), @(rect.size.height)],
                }];
                [s_plugin SaveTimeSeriesData:data];
            }
        }
    }
}

- (bool)checkIsInvalidFrame:(CGRect)rect {
    bool isNull = CGRectIsNull(rect);
    if (isNull) {
        return true;
    }
    bool isNan = CGRectIsInfinite(rect);
    if (isNan) {
        return true;
    }
    bool xInvalid = isnan(rect.origin.x);
    bool yInvalid = isnan(rect.origin.y);
    bool wInvalid = isnan(rect.size.width);
    bool hInvalid = isnan(rect.size.height);
    
    if (xInvalid || yInvalid || wInvalid || hInvalid) {
        return true;
    }
    return false;
}

- (void)btrace_setHidden:(BOOL)hidden {
    [self btrace_setHidden:hidden];
    
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"view";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIView *view = (UIView *)self;
        if (config.view && config.view.viewProperty) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(view.class),
                @"id": @((uintptr_t)view),
                @"parent": @((uintptr_t)view.superview),
                @"value" : @(hidden),
                @"action": @"setHidden",
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void)btrace_setAlpha:(CGFloat)alpha {
    [self btrace_setAlpha:alpha];
    
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"view";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIView *view = (UIView *)self;
        if (config.view && config.view.viewProperty) {
            if ((uintptr_t)view == (uintptr_t)NULL || (uintptr_t)view.superview == (uintptr_t)NULL) {
                return;
            }
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(view.class),
                @"id": @((uintptr_t)view),
                @"parent": @((uintptr_t)view.superview),
                @"value" : @(alpha),
                @"action": @"setAlpha",
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void)btrace_setClipsToBounds:(BOOL)clipsToBounds {
    [self btrace_setClipsToBounds:clipsToBounds];
    
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"view";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        UIView *view = (UIView *)self;
        if (config.view && config.view.viewProperty) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(view.class),
                @"id": @((uintptr_t)view),
                @"parent": @((uintptr_t)view.superview),
                @"value" : @(clipsToBounds),
                @"action": @"setClipsToBounds",
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

static __thread unsigned depth = 0;

+ (nullable NSData *)btrace_dataWithJSONObject:(id)obj options:(NSJSONWritingOptions)opt error:(NSError **)error {
    NSData *jsonData = [self btrace_dataWithJSONObject:obj options:opt error:error];
    
    if (s_plugin && s_plugin.running && depth == 0) {
        depth++;
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        if (config.data && config.data.jsonSerializeSize > 0 && jsonData.length > config.data.jsonSerializeSize) {
            NSString *dataType = @"data";
            NSString *className = @"";
            if (object_isClass(obj)) {
                className = NSStringFromClass([obj class]);
            }
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": className,
                @"id": @((uintptr_t)obj),
                @"value" : @(jsonData.length),
                @"action": @(1),
            }];
            
            [s_plugin SaveTimeSeriesData:data];
        }
        depth--;
    }
    return jsonData;
}

+ (nullable id)btrace_JSONObjectWithData:(NSData *)jsonObject options:(NSJSONReadingOptions)opt error:(NSError **)error {
    id jsonObj = [self btrace_JSONObjectWithData:jsonObject options:opt error:error];
    
    if (s_plugin && s_plugin.running) {
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        if (config.data && config.data.jsonDeserializeSize > 0 && jsonObject.length > config.data.jsonDeserializeSize) {
            NSString *dataType = @"data";
            NSString *className = @"";
            if (object_isClass(jsonObj)) {
                className = NSStringFromClass([jsonObj class]);
            }
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": className,
                @"id": @((uintptr_t)jsonObject),
                @"value" : @(jsonObject.length),
                @"action": @(2),
            }];
            
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    return jsonObj;
}

+ (nullable NSData *)btrace_archivedDataWithRootObject:(id)object requiringSecureCoding:(BOOL)requiresSecureCoding error:(NSError **)error {
    NSData *archivedData = [self btrace_archivedDataWithRootObject:object requiringSecureCoding:requiresSecureCoding error:error];
    
    if (s_plugin && s_plugin.running) {
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        if (config.data && config.data.archiveSize > 0 && archivedData.length > config.data.archiveSize) {
            NSString *dataType = @"data";
            NSString *className = @"";
            if (object_isClass(object)) {
                className = NSStringFromClass([object class]);
            }
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": className,
                @"id": @((uintptr_t)object),
                @"value" : @(archivedData.length),
                @"action": @(3),
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    return archivedData;
}

+ (nullable id)btrace_unarchivedObjectOfClass:(Class)cls fromData:(NSData *)data error:(NSError **)error {
    id unarchiveObject = [self btrace_unarchivedObjectOfClass:cls fromData:data error:error];
    
    if (s_plugin && s_plugin.running) {
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        if (config.data && config.data.unarchiveSize > 0 && data.length > config.data.unarchiveSize) {
            NSString *dataType = @"data";
            BTraceTimeSeriesData *seriesData = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"name": NSStringFromClass(cls),
                @"id": @((uintptr_t)unarchiveObject),
                @"value" : @(data.length),
                @"action": @(4),
            }];
            [s_plugin SaveTimeSeriesData:seriesData];
        }
    }
    return unarchiveObject;
}

- (void)monitorAppStatus {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        // app启动或者app从后台进入前台都会调用这个方法
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:nil];
        // app从后台进入前台都会调用这个方法
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationWillEnterForeground:) name:UIApplicationWillEnterForegroundNotification object:nil];
        // 添加检测app进入后台的观察者
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidEnterBackground:) name: UIApplicationDidEnterBackgroundNotification object:nil];
    });
}

- (void) applicationBecomeActive:(NSNotification *) note {
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"app_state";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.appState) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"state": @"active"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void) applicationWillEnterForeground:(NSNotification *) note {
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"app_state";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.appState) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"state": @"fore"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void) applicationDidEnterBackground:(NSNotification *) note {
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"app_state";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.appState) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"state": @"back"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void) btrace_collectionView:(UICollectionView *)collectionView willDisplayCell:(UITableViewCell *)cell forRowAtIndexPath:(NSIndexPath *)indexPath {
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"cell";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.cell) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"sub_type": @"collection",
                @"name": NSStringFromClass(cell.class),
                @"id": @((uintptr_t)cell),
                @"action": @"appear"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void) btrace_collectionView:(UICollectionView *)collectionView didEndDisplayingCell:(UITableViewCell *)cell forRowAtIndexPath:(NSIndexPath *)indexPath {
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"cell";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.cell) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"sub_type": @"collection",
                @"name": NSStringFromClass(cell.class),
                @"id": @((uintptr_t)cell),
                @"action": @"disappear"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void) btrace_tableView:(UITableView *)tableView willDisplayCell:(UITableViewCell *)cell forRowAtIndexPath:(NSIndexPath *)indexPath {
    if (tableView.pagingEnabled && s_plugin && s_plugin.running ) {
        NSString *dataType = @"cell";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.cell) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"sub_type": @"table",
                @"name": NSStringFromClass(cell.class),
                @"id": @((uintptr_t)cell),
                @"action": @"appear"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void) btrace_tableView:(UITableView *)tableView didEndDisplayingCell:(UITableViewCell *)cell forRowAtIndexPath:(NSIndexPath *)indexPath {
    if (tableView.pagingEnabled && s_plugin && s_plugin.running) {
        NSString *dataType = @"cell";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.cell) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"sub_type": @"table",
                @"name": NSStringFromClass(cell.class),
                @"id": @((uintptr_t)cell),
                @"action": @"disappear"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void)onScrollDidBegin:(UIScrollView *)scrollView
{
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"scroll";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.scroll) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @((uintptr_t)scrollView),
                @"name": NSStringFromClass(scrollView.class),
                @"action": @"start"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void)onScrollDidEnd:(UIScrollView *)scrollView
{
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"scroll";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.scroll) {
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @((uintptr_t)scrollView),
                @"name": NSStringFromClass(scrollView.class),
                @"action": @"stop"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
}

- (void)monitorDisplayLink {
    Class CADisplayLink = NSClassFromString(@"CADisplayLink");
    swizzleMethod(CADisplayLink,
                  @selector(displayLinkWithTarget:selector:),
                  self.class,
                  @selector(btrace_displayLinkWithTarget:selector:),
                  NO);
    
    swizzleMethod(CADisplayLink,
                  @selector(addToRunLoop:forMode:),
                  self.class,
                  @selector(btrace_displayLinkAddToRunLoop:forMode:),
                  YES);
    
    swizzleMethod(CADisplayLink,
                  @selector(setPaused:),
                  self.class,
                  @selector(btrace_displayLinkSetPaused:),
                  YES);
    
    swizzleMethod(CADisplayLink,
                  NSSelectorFromString(@"dealloc"),
                  self.class, @selector(btrace_displayLinkDealloc),
                  YES);
}

+ (CADisplayLink *)btrace_displayLinkWithTarget:(id)target selector:(SEL)sel {
    CADisplayLink *displayLink = [self btrace_displayLinkWithTarget:target selector:sel];
    
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.link) {
            uintptr_t objId = (uintptr_t)displayLink;
            NSString *flag = [target isMemberOfClass:[target class]]?@"-":@"+";
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"link",
                @"action": @"create",
                @"name": [NSString stringWithFormat:@"%@[%@ %s]", flag, [target class], sel]
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    return displayLink;
}

- (void)btrace_displayLinkAddToRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode {
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.link) {
            uintptr_t objId = (uintptr_t)self;
            CADisplayLink *link = (CADisplayLink *)self;
            NSString *action = link.paused?@"stop":@"start";
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"link",
                @"action": action
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    [self btrace_displayLinkAddToRunLoop:runloop forMode:mode];
}

- (void)btrace_displayLinkSetPaused:(BOOL)paused {
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.link) {
            NSString *action = paused?@"stop":@"start";
            uintptr_t objId = (uintptr_t)self;
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"link",
                @"action": action
            }];
            [s_plugin SaveTimeSeriesData:data];
        }

    }
    [self btrace_displayLinkSetPaused:paused];
}

- (void)btrace_displayLinkDealloc {
    if (s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.link) {
            uintptr_t objId = (uintptr_t)self;
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"link",
                @"action": @"destroy"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }

    }
    [self btrace_displayLinkDealloc];
}

- (void)monitorTimer {
    BTraceTimeSeriesConfig *config = (BTraceTimeSeriesConfig *)self.config;
    
    static dispatch_once_t linkOnce;
    static dispatch_once_t nstimerOnce;
    static dispatch_once_t dispatchOnce;
    
    if (config.timer.nstimer) {
        dispatch_once(&nstimerOnce, ^{
            [self monitorNSTimer];
        });
    }
    if (config.timer.link) {
        dispatch_once(&linkOnce, ^{
            [self monitorDisplayLink];
        });
    }
    if (config.timer.dispatch) {
        dispatch_once(&dispatchOnce, ^{
            [self monitorDispatchSourceTimer];
        });
    }
}

- (void)monitorNSTimer {
    bool result;
    
    Class NSTimer = NSClassFromString(@"NSTimer");
    
    result = swizzleMethod(NSTimer,
                           @selector(scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:),
                           self.class,
                           @selector(btrace_scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:),
                           NO);
    
    result = swizzleMethod(NSTimer,
                           @selector(timerWithTimeInterval:target:selector:userInfo:repeats:),
                           self.class,
                           @selector(btrace_timerWithTimeInterval:target:selector:userInfo:repeats:),
                           NO);
    
    result = swizzleMethod(NSTimer,
                           @selector(timerWithTimeInterval:invocation:repeats:),
                           self.class,
                           @selector(btrace_timerWithTimeInterval:invocation:repeats:),
                           NO);
    
    result = swizzleMethod(NSTimer,
                           @selector(scheduledTimerWithTimeInterval:invocation:repeats:),
                           self.class,
                           @selector(btrace_scheduledTimerWithTimeInterval:invocation:repeats:),
                           NO);
    
    result = swizzleMethod(NSTimer,
                           @selector(timerWithTimeInterval:repeats:block:),
                           self.class,
                           @selector(btrace_timerWithTimeInterval:repeats:block:),
                           NO);
    
    result = swizzleMethod(NSTimer,
                           @selector(scheduledTimerWithTimeInterval:repeats:block:),
                           self.class,
                           @selector(btrace_scheduledTimerWithTimeInterval:repeats:block:),
                           NO);
    
    Class __NSCFTimer = NSClassFromString(@"__NSCFTimer");
    result = swizzleMethod(__NSCFTimer,
                           @selector(invalidate),
                           self.class,
                           @selector(btrace_NSTimerInvalidate),
                           YES);
    
    result = swizzleMethod(__NSCFTimer,
                           @selector(setFireDate:),
                           self.class,
                           @selector(btrace_NSTimerSetFireDate:),
                           YES);
    
    Class NSRunLoop = NSClassFromString(@"NSRunLoop");
    result = swizzleMethod(NSRunLoop,
                           @selector(addTimer:forMode:),
                           self.class, @selector(btrace_addTimer:forMode:),
                           YES);
}

+ (NSTimer *)btrace_scheduledTimerWithTimeInterval:(NSTimeInterval)ti target:(id)aTarget selector:(SEL)aSelector userInfo:(nullable id)userInfo repeats:(BOOL)yesOrNo {
    NSTimer *timer = [self btrace_scheduledTimerWithTimeInterval:ti target:aTarget selector:aSelector userInfo:userInfo repeats:yesOrNo];
    
    if (yesOrNo && ti < INTERVAL_THRESHOLD && s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.nstimer) {
            uintptr_t objId = (uintptr_t)timer;
            
            NSString *name = @"";

            if (timer.userInfo && ([timer.userInfo isKindOfClass:NSClassFromString(@"__NSGlobalBlock__")] ||
                                   [timer.userInfo isKindOfClass:NSClassFromString(@"__NSStackBlock__")] ||
                                   [timer.userInfo isKindOfClass:NSClassFromString(@"__NSMallocBlock__")])) {
                struct BlockLiteral *blockRef = (__bridge struct BlockLiteral *)timer.userInfo;
                name = [NSString stringWithFormat:@"%p", blockRef->invoke];
                #if DEBUG || TEST_MODE
                Dl_info info;
                if (dladdr((void *)blockRef->invoke, &info) != 0 && info.dli_sname) {
                    name = [NSString stringWithUTF8String:info.dli_sname];
                }
                #endif
            } else {
                if ([aTarget respondsToSelector:@selector(target)] && [aTarget respondsToSelector:@selector(selector)]) {
                    id realTarget = [aTarget performSelector:@selector(target)];
                    SEL realSelector = (SEL)(__bridge void *)[aTarget performSelector:@selector(selector)];
                    aTarget = realTarget;
                    aSelector = realSelector;
                }
                NSString *flag = [aTarget isMemberOfClass:[aTarget class]]?@"-":@"+";
                name = [NSString stringWithFormat:@"%@[%@ %s]", flag, [aTarget class], aSelector];
            }
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": @"create",
                @"interval": @(ti),
                @"name": name
            }];
            [s_plugin SaveTimeSeriesData:data];
            data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": @"start"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    
    return timer;
}

+ (NSTimer *)btrace_timerWithTimeInterval:(NSTimeInterval)ti target:(id)aTarget selector:(SEL)aSelector userInfo:(nullable id)userInfo repeats:(BOOL)yesOrNo {
    NSTimer *timer = [self btrace_timerWithTimeInterval:ti target:aTarget selector:aSelector userInfo:userInfo repeats:yesOrNo];
    
    if (yesOrNo && ti < INTERVAL_THRESHOLD && s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.nstimer) {
            uintptr_t objId = (uintptr_t)timer;
            
            NSString *name = @"";
            if (timer.userInfo && ([timer.userInfo isKindOfClass:NSClassFromString(@"__NSGlobalBlock__")] ||
                                   [timer.userInfo isKindOfClass:NSClassFromString(@"__NSStackBlock__")] ||
                                   [timer.userInfo isKindOfClass:NSClassFromString(@"__NSMallocBlock__")])) {
                struct BlockLiteral *blockRef = (__bridge struct BlockLiteral *)timer.userInfo;
                name = [NSString stringWithFormat:@"%p", blockRef->invoke];
                #if DEBUG || TEST_MODE
                Dl_info info;
                if (dladdr((void *)blockRef->invoke, &info) != 0 && info.dli_sname) {
                    name = [NSString stringWithUTF8String:info.dli_sname];
                }
                #endif
            } else {
                if ([aTarget respondsToSelector:@selector(target)] && [aTarget respondsToSelector:@selector(selector)]) {
                    id realTarget = [aTarget performSelector:@selector(target)];
                    SEL realSelector = (SEL)(__bridge void *)[aTarget performSelector:@selector(selector)];
                    aTarget = realTarget;
                    aSelector = realSelector;
                }
                NSString *flag = [aTarget isMemberOfClass:[aTarget class]]?@"-":@"+";
                name = [NSString stringWithFormat:@"%@[%@ %s]", flag, [aTarget class], aSelector];
            }
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": @"create",
                @"interval": @(ti),
                @"name": name
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    return timer;
}

+ (NSTimer *)btrace_timerWithTimeInterval:(NSTimeInterval)ti invocation:(NSInvocation *)invocation repeats:(BOOL)yesOrNo {
    NSTimer *timer = [self btrace_timerWithTimeInterval:ti invocation:invocation repeats:yesOrNo];
    
    if (yesOrNo && ti < INTERVAL_THRESHOLD && s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.nstimer && invocation.target && invocation.selector) {
            uintptr_t objId = (uintptr_t)timer;
            NSString *flag = [invocation.target isMemberOfClass:[invocation.target class]]?@"-":@"+";
            NSString *name = [NSString stringWithFormat:@"%@[%@ %s]", flag, [invocation.target class], invocation.selector];
            
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": @"create",
                @"interval": @(ti),
                @"name": name
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    return timer;
}

+ (NSTimer *)btrace_scheduledTimerWithTimeInterval:(NSTimeInterval)ti invocation:(NSInvocation *)invocation repeats:(BOOL)yesOrNo {
    NSTimer *timer = [self btrace_scheduledTimerWithTimeInterval:ti invocation:invocation repeats:yesOrNo];
    
    if (yesOrNo && ti < INTERVAL_THRESHOLD && s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.nstimer && invocation.target && invocation.selector) {
            uintptr_t objId = (uintptr_t)timer;
            NSString *flag = [invocation.target isMemberOfClass:[invocation.target class]]?@"-":@"+";
            NSString *name = [NSString stringWithFormat:@"%@[%@ %s]", flag, [invocation.target class], invocation.selector];
            
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": @"create",
                @"interval": @(ti),
                @"name": name
            }];
            [s_plugin SaveTimeSeriesData:data];
            data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": @"start"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    return timer;
}

+ (NSTimer *)btrace_timerWithTimeInterval:(NSTimeInterval)interval repeats:(BOOL)repeats block:(void (^)(NSTimer *timer))block {
    NSTimer *timer = [self btrace_timerWithTimeInterval:interval repeats:repeats block:block];
    
    if (repeats && interval < INTERVAL_THRESHOLD && s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.nstimer) {
            uintptr_t objId = (uintptr_t)timer;
            
            NSString *name = @"";
            struct BlockLiteral *blockRef = (__bridge struct BlockLiteral *)block;
            name = [NSString stringWithFormat:@"%p", blockRef->invoke];
            #if DEBUG || TEST_MODE
            Dl_info info;
            if (dladdr((void *)blockRef->invoke, &info) != 0 && info.dli_sname) {
                name = [NSString stringWithUTF8String:info.dli_sname];
            }
            #endif
            
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": @"create",
                @"interval": @(interval),
                @"name": name
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    return timer;
}

+ (NSTimer *)btrace_scheduledTimerWithTimeInterval:(NSTimeInterval)interval repeats:(BOOL)repeats block:(void (^)(NSTimer *timer))block {
    NSTimer *timer = [self btrace_scheduledTimerWithTimeInterval:interval repeats:repeats block:block];
    
    if (repeats && interval < INTERVAL_THRESHOLD && s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.nstimer) {
            uintptr_t objId = (uintptr_t)timer;
            
            NSString *name = @"";
            struct BlockLiteral *blockRef = (__bridge struct BlockLiteral *)block;
            name = [NSString stringWithFormat:@"%p", blockRef->invoke];
            #if DEBUG || TEST_MODE
            Dl_info info;
            if (dladdr((void *)blockRef->invoke, &info) != 0 && info.dli_sname) {
                name = [NSString stringWithUTF8String:info.dli_sname];
            }
            #endif
            
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": @"create",
                @"interval": @(interval),
                @"name": name
            }];
            [s_plugin SaveTimeSeriesData:data];
            data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": @"start"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    return timer;
}

- (void)btrace_NSTimerInvalidate {
    NSTimer *timer = (NSTimer *)self;
    NSTimeInterval interval = timer.timeInterval;
    BOOL repeats = (0 < interval);
    
    if (repeats && interval < INTERVAL_THRESHOLD && s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.nstimer) {
            uintptr_t objId = (uintptr_t)self;
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": @"destroy"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    [self btrace_NSTimerInvalidate];
}

- (void)btrace_NSTimerSetFireDate:(NSDate *)date {
    NSTimer *timer = (NSTimer *)self;
    NSTimeInterval interval = timer.timeInterval;
    BOOL repeats = (0 < interval);
    
    if (repeats && interval < INTERVAL_THRESHOLD && s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.nstimer) {
            NSString *action = [date isEqualToDate:[NSDate distantFuture]]?@"stop":@"start";
            uintptr_t objId = (uintptr_t)self;
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": action
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    [self btrace_NSTimerSetFireDate: date];
}

- (void)btrace_addTimer:(NSTimer *)timer forMode:(NSRunLoopMode)mode {
    NSTimeInterval interval = timer.timeInterval;
    BOOL repeats = (0 < interval);
    
    if (repeats && interval < INTERVAL_THRESHOLD && s_plugin && s_plugin.running) {
        NSString *dataType = @"timer";
        
        auto *config = (BTraceTimeSeriesConfig *)s_plugin.config;
        
        if (config.timer.nstimer) {
            uintptr_t objId = (uintptr_t)self;
            auto data = [[BTraceTimeSeriesData alloc] initWithType:dataType Dictionary: @{
                @"id": @(objId),
                @"sub_type": @"nstimer",
                @"action": @"start"
            }];
            [s_plugin SaveTimeSeriesData:data];
        }
    }
    [self btrace_addTimer:timer forMode:mode];
}

- (void)monitorDispatchSourceTimer {
    if (@available(iOS 16.0, *)) {
        [self fishhook];
    }
}

-(void)fishhook {
    dispatch_async(self.btrace.queue, ^{
        struct rebinding r[] = {
            REBINDING(dispatch_source_create),
            REBINDING(dispatch_source_set_event_handler),
            REBINDING(dispatch_source_set_timer),
            REBINDING(dispatch_resume),
            REBINDING(dispatch_suspend),
            REBINDING(dispatch_source_cancel)
        };
        int result = rebind_symbols(r, sizeof(r)/sizeof(struct rebinding));
    });
}

- (void)dump {
    if (!self.btrace.enable) {
        return;
    }

    ProcessTimeSeriesBuffer();
}

@end

namespace btrace {

void ProcessTimeSeriesBuffer() {
    auto size = s_buffer->size_approx();
    
    auto transaction = Transaction([BTrace shared].db->getHandle());
    
    for (int i=0; i<size;++i) {
        std::shared_ptr<TimeSeriesData> data;
        bool res = s_buffer->try_dequeue(data);
        if (!res) {
            break;
        }
        
        auto record = TimeSeriesModel(btrace::trace_id, data->tid, data->timestamp,
                                      data->type.c_str(), data->info.c_str());
        [BTrace shared].db->insert(record);
    }
    
    transaction.commit();
}

#if DEBUG || INHOUSE_TARGET || TEST_MODE || READING_DEV
void CPUUsageInfo() {
    if (s_plugin == nullptr) {
        return;
    }
    
    ThreadCPUUsageVisitor visitor;
    OSThread::VisitThreads(&visitor);
    
    float total_usage = visitor.total_usage();
    auto data = [[BTraceTimeSeriesData alloc] initWithType:@"cpu_usage" Dictionary:@{
        @"val": @(total_usage),
        @"perfetto_type": @"counter"
    }];
    [s_plugin SaveTimeSeriesData:data];
}

void MemUsageInfo() {
    if (s_plugin == nullptr) {
        return;
    }
    
    uint64_t curr_usage = Utils::MemoryUsage();
    auto data = [[BTraceTimeSeriesData alloc] initWithType:@"mem_usage" Dictionary:@{
        @"val": @(curr_usage),
        @"perfetto_type": @"counter"
    }];
    [s_plugin SaveTimeSeriesData:data];
    
}
#endif

}
