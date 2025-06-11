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
//  InternalUtils.h
//  InternalUtils
//
//  Created by Bytedance.
//

#import <objc/runtime.h>
#import <Foundation/Foundation.h>

#import <Stinger/Stinger.h>
#import <fishhook/fishhook.h>

static void noop(void) {};

inline BOOL swizzleMethod(Class oriCls, SEL oriSel, Class swiCls, SEL swiSel, BOOL isInsMeth)
{
    Method oriMeth, swiMeth;
    
    if (isInsMeth) {
        oriMeth = class_getInstanceMethod(oriCls, oriSel);
        swiMeth = class_getInstanceMethod(swiCls, swiSel);
        if (!oriMeth || !swiMeth) {
            return NO;
        }
        class_addMethod(oriCls, swiSel, method_getImplementation(swiMeth), method_getTypeEncoding(swiMeth));
        swiMeth = class_getInstanceMethod(oriCls, swiSel);
        if (!swiMeth) {
            return NO;
        }
        
        BOOL added = class_addMethod(oriCls,
                                     oriSel,
                                     method_getImplementation(swiMeth),
                                     method_getTypeEncoding(swiMeth));
        if (added) {
            // 子类没有实现
            method_setImplementation(swiMeth, method_getImplementation(oriMeth) ?: noop);
        } else {
            method_exchangeImplementations(oriMeth, swiMeth);
        }
    } else {
        oriMeth = class_getClassMethod(oriCls, oriSel);
        swiMeth = class_getClassMethod(swiCls, swiSel);
        if (!oriMeth || !swiMeth) {
            return NO;
        }
        class_addMethod(object_getClass(oriCls), swiSel, method_getImplementation(swiMeth), method_getTypeEncoding(swiMeth));
        swiMeth = class_getClassMethod(oriCls, swiSel);
        if (!swiMeth) {
            return NO;
        }
        
        BOOL added = class_addMethod(object_getClass(oriCls),
                                     oriSel,
                                     method_getImplementation(swiMeth),
                                     method_getTypeEncoding(swiMeth));
        if (added) {
            // 子类没有实现
            method_setImplementation(swiMeth, method_getImplementation(oriMeth) ?: noop);
        } else {
            method_exchangeImplementations(oriMeth, swiMeth);
        }
    }
    return YES;
}


@interface MethodHookInfo: NSObject

@property(nonatomic, assign, direct) BOOL hooked;
@property(nonatomic, assign, direct) mach_port_t tid;
@property(nonatomic, assign, direct) int64_t execTime;
@property(nonatomic, strong, direct, nullable) NSString *flag;
@property(nonatomic, strong, direct, nullable) NSString *cls;
@property(nonatomic, strong, direct, nullable) NSString *sel;

-(instancetype)initWithFlag:(NSString * _Nullable)flag Cls:(NSString * _Nullable)cls Sel:(NSString * _Nullable)sel;

@end

@interface MethodPair: NSObject

@property(nonatomic, strong, direct, nullable) MethodHookInfo *beginMethod;
@property(nonatomic, strong, direct, nullable) MethodHookInfo *endMethod;

-(instancetype)initWithBegin:(MethodHookInfo * _Nullable)b End:(MethodHookInfo * _Nullable)e;

@end

MethodHookInfo *parseMethod(NSString * _Nullable methodName);
int autoHookMethod(MethodHookInfo * _Nullable methodInfo, STOption op, id _Nullable block);

#define BTRACE(x) __btrace_##x
#define ORI(func) btrace_ori_##func
#define REBINDING(func) {#func, (void * _Nonnull)BTRACE(func), (void * _Nonnull * _Nullable)&ORI(func)}

#define BTRACE_HOOK(ret_type,func,...) \
static ret_type (*ORI(func))(__VA_ARGS__);\
static ret_type (BTRACE(func))(__VA_ARGS__)
