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
//  BTrace.mm
//  BTrace
//
//  Created by Bytedance.
//

#import <Foundation/Foundation.h>
#import <zlib.h>
#import <stdint.h>
#import <objc/runtime.h>
#import <sys/utsname.h>

#import "BTrace.h"
#import "InternalUtils.h"

#import "BTraceProfilerConfig.h"
#import "BTraceProfilerPlugin.h"

#import "BTraceTimeSeriesConfig.h"
#import "BTraceTimeSeriesPlugin.h"

#import "BTraceMemProfilerConfig.h"
#import "BTraceMemProfilerPlugin.h"

#import "BTraceDispatchProfilerConfig.h"
#import "BTraceDispatchProfilerPlugin.h"

#import "BTraceDateProfilerConfig.h"
#import "BTraceDateProfilerPlugin.h"

#import "BTraceLockProfilerConfig.h"
#import "BTraceLockProfilerPlugin.h"

#import "BTraceMonitorConfig.h"
#import "BTraceMonitorPlugin.h"

#include "BTraceRecord.hpp"
#include "BTraceDataBase.hpp"
#include "ImageInfo.hpp"

#include "BTraceModel.hpp"
#include "ThreadInfoModel.hpp"
#include "TimeSeriesModel.hpp"
#include "ImageInfoModel.hpp"
#include "CallStackTableModel.hpp"

#include "reflection.hpp"
#include "utils.hpp"
#include "zone.hpp"
#include "monitor.hpp"
#include "os_thread.hpp"
#include "memory.hpp"
#include "callstack_table.hpp"
#include "event_loop.hpp"

static const int MAX_RECORDS = 1000;
static const int MAX_DURATION = 12 * 3600;
static const int MAX_FILE_SIZE = 20 * (1 << 20);
static const int GZIP_CHUNK_SIZE = 16 * (1 << 10);
static NSString *DATABASE_NAME = @"btrace.sqlite";
static NSString *RECORD_NUMBER_KEY = @"BTRACE_RECORD_NUMBER";
static NSString *LAST_RECORD_TIME_KEY = @"BTRACE_LAST_RECORD_TIME";
static NSMutableSet<NSString *> *METHOD_RECORD = nil;

using namespace btrace;

namespace btrace {

uint8_t trace_id = 0;
int64_t start_time = 0;
int64_t start_mach_time = 0;

void image_info_callback(ImageInfo *image_info);

static BTrace *s_btrace = nullptr;
static dispatch_queue_t s_queue = nullptr;

}

static void btrace_at_exit() {
    [[BTrace shared] stop];
}

@interface BTrace ()

@property(nonatomic, assign, direct) bool zip;
@property(nonatomic, assign, direct) int type;
@property(nonatomic, assign, direct) int timeout;
@property(nonatomic, assign, direct) int maxRecords;
@property(atomic, assign, direct) int toBeUploaded;
@property(nonatomic, strong, direct) NSString *workingDir;
@property(nonatomic, strong, direct) NSString *osVersion;
@property(nonatomic, strong, direct) NSString *deviceModel;
@property(nonatomic, strong, direct) NSString *bundleId;
@property(nonatomic, strong, direct) NSDictionary *dumpSampling;
@property(nonatomic, strong, direct) NSMutableSet<BTracePlugin *> *tracers;
@property(nonatomic, strong, direct) NSMutableArray<MethodPair *> *methodPairList;
@property(nonatomic, strong, direct) BTraceCallback callback;

@end

@implementation BTrace

//+ (void)load {
//
//    NSDictionary *envDict = [[NSProcessInfo processInfo] environment];
//
//    int period = [envDict[@"BTrace_period"] intValue] ?: 1;     // unit: ms
//    int duration = [envDict[@"BTrace_duration"] intValue] ?: 1 * 60; // unit: s
//    bool enable_pthread_hook;
//    id enable_pthread_hook_oc = envDict[@"BTrace_enable_pthread_hook"];
//    if (enable_pthread_hook_oc == nil) {
//        enable_pthread_hook = true;
//    } else {
//        enable_pthread_hook = [enable_pthread_hook_oc boolValue];
//    }
//    bool enable_thread_map;
//    id enable_thread_map_oc = envDict[@"BTrace_enable_thread_map"];
//    if (enable_thread_map_oc == nil) {
//        enable_thread_map = true;
//    } else {
//        enable_thread_map = [enable_thread_map_oc boolValue];
//    }
//    BOOL main_thread_only = NO;
//    BOOL active_thread_only = YES;
//
//    NSArray *plugin_configs = @[
//        @{
//            @"name" : @"timeseries",
//            @"config" : @{
//                @"app_state": @{},
//                @"cell": @{},
//                @"scroll": @{},
//                @"vc": @{},
//                @"timer": @{
//                    @"config": @{
//                        @"link": @(1),
//                        @"nstimer": @(1),
//                        @"dispatch": @(1)
//                    }
//                },
//                @"launch": @{},
//                @"first_frame": @{},
//                @"lynx": @{}
//            }
//        },
//        @{
//            @"name" : @"memoryprofiler",
//            @"config" : @{
//                @"lite_mode" : @(0),
//                @"interval" : @(1),
//                @"period" : @(period)
//            }
//        },
//        @{
//            @"name" : @"dispatchprofiler",
//            @"config" : @{
//                @"period" : @(0)
//            }
//        },
//        @{
//            @"name" : @"dateprofiler",
//            @"config" : @{
//                @"period" : @(period)
//            }
//        },
//        @{
//            @"name" : @"lockprofiler",
//            @"config" : @{
//                @"period" : @(period)
//            }
//        },
//        @{
//            @"name" : @"timeprofiler",
//            @"config" : @{
//                @"period" : @(period),
//                @"merge_stack" : @(0),
//                @"max_duration" : @(duration),
//                @"main_thread_only" : @(main_thread_only),
//                @"active_thread_only" : @(active_thread_only),
//                @"fast_unwind": @(1)
//            }
//        },
//        @{
//            @"name" : @"monitor",
//            @"config" : @{
//                @"cpu_level": @{},
//                @"hang": @{
//                    @"config": @{
//                        @"hitch": @(100),
//                        @"hang": @(250),
//                        @"anr": @(8),
//                    }
//                }
//            }
//        }
//    ];
//
//    NSDictionary *config = @{
//        @"enable": @(YES),
//        @"timeout": @(duration),
//        @"max_records": @(1000),
//        @"buffer_size": @(8),
//        @"enable_pthread_hook": @(enable_pthread_hook),
//        @"enable_thread_map": @(enable_thread_map),
//        @"auto_startup": @[
//            @{
//                @"B": @"",
////                @"E": @""
//            }
//        ],
//        @"dump_sampling": @{
//           @"hitch": @(4),
//           @"hang": @(4),
//       },
//        @"plugins": plugin_configs
//    };
//
//    [[self shared] setupWithConfig:config];
//    [[self shared] start];
//}

+ (instancetype)shared {
    static BTrace *instance = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      instance = [[self alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];

    if (self) {
        struct utsname systemInfo;
        uname(&systemInfo);
        _running = NO;
        _enable = NO;
        _timeout = 0;
        _bufferSize = 0;
        _maxRecords = 0; // record number limits per day
        _workingDir = [self workingDir];
        _tracers = [[NSMutableSet alloc] init];
        _queue = s_queue = dispatch_queue_create("btrace.async_queue",
                                       DISPATCH_QUEUE_SERIAL);
        _sdkInitTime = [[NSDate new] timeIntervalSince1970] * 1000;
        _osVersion = [[UIDevice currentDevice] systemVersion];
        _deviceModel = [NSString stringWithCString:systemInfo.machine
                                          encoding:NSUTF8StringEncoding];
        _bundleId = [[NSBundle mainBundle] bundleIdentifier];
        _methodPairList = [NSMutableArray array];
        _db = nil;
        
        Zone::Init();
        OSThread::Init();
        EventLoop::Init();
        ImageInfo::Init();
        VirtualMemory::Init();
        Utils::MachTimeInit();
        
        METHOD_RECORD = [NSMutableSet set];
        
        s_btrace = self;

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(appActive:)
                                                     name:UIApplicationDidBecomeActiveNotification
                                                   object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(appWillEnterFg:)
                                                     name:UIApplicationWillEnterForegroundNotification
                                                   object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(appDidEnterBg:)
                                                     name: UIApplicationDidEnterBackgroundNotification
                                                   object:nil];
        
        atexit(btrace_at_exit);
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(stop)
          name:@"UIApplicationWillTerminateNotification" object:nil];
     }
    return self;
}

- (void)dealloc {
    delete _db;
    _db = nil;
}

- (NSString *)workingDir {
    NSString *cachePath = [NSSearchPathForDirectoriesInDomains(
        NSCachesDirectory, NSUserDomainMask, YES) firstObject];
    NSString *rootFolderPath =
        [cachePath stringByAppendingPathComponent:@"BTrace"];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager createDirectoryAtPath:rootFolderPath
           withIntermediateDirectories:YES
                            attributes:nil
                                 error:NULL];
    
    return rootFolderPath;
}

- (void)appActive:(NSNotification *)note {
    _appState = 0;
    _appStateTime = Utils::GetCurrMachMicros();
}

- (void)appWillEnterFg:(NSNotification *)note {
    _appState = 0;
    _appStateTime = Utils::GetCurrMachMicros();
}

- (void)appDidEnterBg:(NSNotification *)note {
    _appState = 1;
    _appStateTime = Utils::GetCurrMachMicros();
}

- (void)start {
    @synchronized(self) {
        if (!_enable) {
            NSLog(@"[BTrace] not enabled!");
            return;
        }

        if (_running) {
            NSLog(@"[BTrace] running, return");
            return;
        }

        if (_maxRecords < [self nextRecordNumber]) {
            NSLog(@"[BTrace] reach maxRecords, return");
            return;
        }
        
        if (!_db) {
            NSString *dbPath = [_workingDir stringByAppendingPathComponent:DATABASE_NAME];
            [[NSFileManager defaultManager] removeItemAtPath:dbPath error:nil];
            _db = new BTraceDataBase([dbPath UTF8String]);
        }

        Transaction transaction(_db->getHandle());
        
        if (![self initTable]) {
            NSLog(@"[BTrace] init table error, return");
            return;
        }

        transaction.commit();
                
        if (!g_callstack_table) {
            g_callstack_table = new CallstackTable();
        }
        
        if (0 < _bufferSize && !g_record_buffer) {
            g_record_buffer = new RecordBuffer(_bufferSize*MB);
        }

        OSThread::Start();
        ImageInfo::Start();

        [self recordStart];

        _running = YES;

        Utils::Print("Start!");

        auto p_tid = OSThread::PthreadSelf();
        bool is_main = (p_tid == OSThread::MainJoinId());
        
        OSThread::ScopedDisableLogging disable(is_main);

        for (BTracePlugin *tracer in _tracers) {
            Transaction transaction(_db->getHandle());
            [tracer start];
            transaction.commit();
        }
        
        EventLoop::Start();
        
        dispatch_after(
            dispatch_time(DISPATCH_TIME_NOW, (int64_t)(_timeout)*NSEC_PER_SEC),
            _queue, ^{
              uint8_t trace_id = btrace::trace_id;
              [self stopWithId:trace_id];
        });
    }
}

-(void)_dumpWithTag:(const char*)tag 
               Info:(const char*)info
          BeginTime:(int64_t)begin
            EndTime: (int64_t)end
                Tid:(mach_port_t)tid {
    if (!_enable) {
        return;
    }

    if (!_running) {
        return;
    }
    
    if (_dumpSampling && 0 < [_dumpSampling count]) {
        int64_t curr_time = Utils::GetCurrMachMicros();
        int64_t rate = MAX([[_dumpSampling objectForKey:[NSString stringWithUTF8String:tag]] intValue], 1) ;
        bool hit = Utils::HitSampling(curr_time, rate);
        
        if (!hit) {
            return;
        }
    }
    
    @synchronized(self) {
        if (!_enable) {
            return;
        }
        
        if (!_running) {
            return;
        }

        Utils::Print("Dump, tag: %s, info: %s", tag, info);
        
        auto monitor = EventLoop::monitor();
        monitor->Enter();
        
        for (BTracePlugin *tracer in _tracers) {
            [tracer dump];
        }
        
        if (g_callstack_table) {
            [self dumpCallstackTable];
        }
        
        if (g_record_buffer) {
            uint64_t rel_begin_time = MAX(begin - start_time, 0);
            uint64_t rel_end_time = MAX(end - start_time, 0);
            g_record_buffer->Dump(rel_begin_time, rel_end_time, tid);
        }
        
        [self dumpImageInfo];
        [self dumpThreadInfo];
        
        [self recordDumpWithTag:tag Info:info];
        
        _db->getHandle()->wal_checkpoint();
        [self invokeDataCompletionCallback];
        
        monitor->Exit();
    }
}

- (void)_stop {
    if (!_running) {
        return;
    }

    _running = NO;
    
    Utils::Print("Stop!");

    EventLoop::Stop();

    for (BTracePlugin *tracer in _tracers) {
        [tracer stop];
    }
    
    if (g_callstack_table) {
        [self dumpCallstackTable];
        delete g_callstack_table;
        g_callstack_table = nullptr;
    }
    
    if (g_record_buffer) {
        g_record_buffer->Dump();
        delete g_record_buffer;
        g_record_buffer = nullptr;
    }
    
    [self dumpImageInfo];
    [self dumpThreadInfo];

    OSThread::Stop();
    ImageInfo::Stop();

    [self recordStop];

    _db->getHandle()->wal_checkpoint();
    [self invokeDataCompletionCallback];
}

- (void)dumpCallstackTable {
    std::vector<CallStackNode> node_list;
    CallstackTableIterator it(g_callstack_table);
    
    while (it.HasNext()) {
        const CallstackTable::Node *node = it.Next();
        uint32_t stack_id = (uint32_t)(reinterpret_cast<uint64_t>(node)>>4);
        uint32_t parent = (uint32_t)(node->parent>>4);
        node_list.emplace_back(stack_id, parent, node->address);
    }
    
    Transaction transaction(_db->getHandle());
    
    [BTrace shared].db->delete_table<CallStackTableModel>();
    auto stacktable_record =
        CallStackTableModel(btrace::trace_id, std::move(node_list));

    _db->insert(stacktable_record);
    transaction.commit();
}

- (void)dumpImageInfo {
    std::vector<ImageInfo *> info_list;
    ImageInfoIterator it;
    
    while (it.HasNext()) {
        ImageInfo *info = it.Next();
        if (!info->dumped) {
            info_list.push_back(info);
            info->dumped = true;
        }
    }
    
    Transaction transaction(_db->getHandle());
    
    for (ImageInfo *info: info_list) {
        auto image_info_record =
        ImageInfoModel(trace_id, info->text_vmaddr, info->text_size,
                       info->type, info->name, info->uuid);
        
        s_btrace.db->insert(image_info_record);
    }

    transaction.commit();
}

- (void)dumpThreadInfo {
    std::vector<ThreadInfoModel> info_list;
    
    {
        OSThreadIterator it;
        
        while (it.HasNext())
        {
            OSThread *t = it.Next();
            
            if (t->dumped() ||
                t->name() == nullptr ||
                strlen(t->name()) == 0) {
                continue;
            }
            info_list.emplace_back(trace_id, t->id(), t->name());
            t->set_dumped(true);
        }
    }
    
    Transaction transaction(_db->getHandle());
    
    for (auto &info: info_list) {
        s_btrace.db->insert(info);
    }

    transaction.commit();
}

- (void)dumpWithTag:(const char *)tag
               Info:(const char *)info
          BeginTime:(int64_t)begin
            EndTime:(int64_t)end
                Tid:(mach_port_t)tid{
    [self _dumpWithTag:tag Info:info BeginTime:begin EndTime:end Tid:tid];
}

- (void)dumpWithTag:(const char *)tag 
               Info:(const char *)info
          BeginTime:(int64_t)begin
            EndTime:(int64_t)end {
    [self _dumpWithTag:tag Info:info BeginTime:begin EndTime:end Tid:0];
}

- (void)dumpWithTag:(const char*)tag Info:(const char*)info {
    [self _dumpWithTag:tag Info:info BeginTime:start_time EndTime:INT64_MAX Tid:0];
}

- (void)dumpWithTag:(const char*)tag {
    [self _dumpWithTag:tag Info:"" BeginTime:start_time EndTime:INT64_MAX Tid:0];
}

- (void)dump {
    [self _dumpWithTag:"" Info:"" BeginTime:start_time EndTime:INT64_MAX Tid:0];
}

- (void)stop {
    if (!_running) {
        return;
    }
    
    @synchronized(self) {
        [self _stop];
    }
}

- (void)stopWithCallback:(BTraceCallback)block {
    if (block == nil) {
        return;
    }
    
    @synchronized(self) {
        if (!_running) {
            block(nil);
            return;
        }
        
        [self _stop];
        
        const char *dbPath = _db->getHandle()->getPath().c_str();
        NSString *path = [NSString stringWithUTF8String:dbPath];
        
        NSData *raw = [NSData dataWithContentsOfFile:path];
        NSData *data = [self gzipData:raw];
        raw = nil;
        block(data);
    }
}

- (void)stopWithId:(int64_t)trace_id {
    if (!_enable) {
        return;
    }

    if (!_running) {
        return;
    }
    
    @synchronized(self) {
        if (btrace::trace_id != trace_id)
            return;

        [self _stop];
    }
}

- (void)setDataCompletionCallback:(BTraceCallback)block {
    _callback = block;
}

- (void)invokeDataCompletionCallback {
    if (_callback == nil) {
        return;
    }

    const char *dbPath = _db->getHandle()->getPath().c_str();
    NSString *path = [NSString stringWithUTF8String:dbPath];

    auto manager = [NSFileManager defaultManager];
    auto fileSize = [[manager attributesOfItemAtPath:path error:nil] fileSize];
    
    if (MAX_FILE_SIZE < fileSize) {
        return;
    }

    NSData *data = [NSData dataWithContentsOfFile:path];

    if (self.zip) {
        data = [self gzipData:data];
    }

    _callback(data);
}

- (NSData *)gzipData:(NSData *) data {
    if (data.length == 0) return data;
    
    z_stream stream;
    bzero(&stream, sizeof(stream));
    
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.next_in = (Bytef *) data.bytes;
    stream.avail_in = (uInt) data.length;
    stream.total_out = 0;
    
    OSStatus status;
    status = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS+16, 8, Z_DEFAULT_STRATEGY);
    
    if (status != Z_OK) {
        return nil;
    }
    
    NSMutableData *compressed_data = [NSMutableData dataWithLength:GZIP_CHUNK_SIZE];
    
    do {
        if ((status == Z_BUF_ERROR) || (stream.total_out == [compressed_data length])) {
            [compressed_data increaseLengthBy:GZIP_CHUNK_SIZE];
        }

        stream.next_out = (Bytef*)[compressed_data mutableBytes] + stream.total_out;
        stream.avail_out = (unsigned int)([compressed_data length] - stream.total_out);

        status = deflate(&stream, Z_FINISH);
    } while ((status == Z_OK) || (status == Z_BUF_ERROR));

    deflateEnd(&stream);

    if ((status != Z_OK) && (status != Z_STREAM_END)) {
        return nil;
    }

    [compressed_data setLength:stream.total_out];
    
    return compressed_data;
}

- (void)recordStart {

    const char *os_version = [_osVersion UTF8String];
    const char *device_model = [_deviceModel UTF8String];
    const char *bundle_id = [_bundleId UTF8String];
    auto main_tid = OSThread::MainThreadId();

    auto record = BTraceModel(btrace::trace_id, _startTime, 0, _sdkInitTime,
                                 main_tid, os_version, device_model, bundle_id, 3);
    _db->insert(record);
}

- (void)recordStop {
    _endTime = Utils::GetCurrentTimeMicros();
    auto table_name = std::string(BTraceModel::MetaInfo().GetName());
    auto sql = "UPDATE " + table_name + " SET end_time = ?, sdk_init_time = ?, dump_time = ?, tag = ?, info = ? WHERE trace_id = ?;";
    _db->bind_exec(sql.c_str(), _endTime, _sdkInitTime, 0, "", "", btrace::trace_id);
}

- (void)recordDumpWithTag:(const char*)tag Info:(const char*)info {
    int64_t dumpTime = Utils::GetCurrentTimeMicros();
    auto table_name = std::string(BTraceModel::MetaInfo().GetName());
    auto sql = "UPDATE " + table_name + " SET sdk_init_time = ?, dump_time = ?, tag = ?, info = ? WHERE trace_id = ?;";
    _db->bind_exec(sql.c_str(), _sdkInitTime, dumpTime, tag, info, btrace::trace_id);
}

- (int64_t)nextRecordNumber {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSInteger last_time = [defaults integerForKey:LAST_RECORD_TIME_KEY];

    int result;

    NSDate *date = [NSDate dateWithTimeIntervalSince1970:last_time];
    BOOL isToday = [[NSCalendar currentCalendar] isDateInToday:date];

    if (isToday) {
        result = [defaults integerForKey:RECORD_NUMBER_KEY] + 1;
    } else {
        result = 1;
    }
    
    start_time = _startTime = Utils::GetCurrentTimeMicros();
    start_mach_time = Utils::GetCurrMachMicros();

    btrace::trace_id += 1;

    [defaults setInteger:_startTime / 1000000 forKey:LAST_RECORD_TIME_KEY];
    [defaults setInteger:result forKey:RECORD_NUMBER_KEY];

    return result;
}

- (bool)initTable {
    bool result = false;

    result = _db->drop_table<BTraceModel>();

    if (!result) {
        return result;
    }
    
    result = _db->drop_table<ImageInfoModel>();

    if (!result) {
        return result;
    }
    
    result = _db->drop_table<TimeSeriesModel>();
    
    if (!result) {
        return result;
    }
    
    result = _db->drop_table<CallStackTableModel>();

    if (!result) {
        return result;
    }
    
    result = _db->drop_table<ThreadInfoModel>();

    if (!result) {
        return result;
    }
        
    result = _db->create_table<ThreadInfoModel>();

    if (!result) {
        return result;
    }
    
    result = _db->create_table<CallStackTableModel>();

    if (!result) {
        return result;
    }
    
    result = _db->create_table<TimeSeriesModel>();
    
    if (!result) {
        return result;
    }

    result = _db->create_table<ImageInfoModel>();

    if (!result) {
        return result;
    }

    result = _db->create_table<BTraceModel>();

    if (!result) {
        return result;
    }
    
    return result;
}

- (void)setupWithConfig:(NSDictionary *)config {
    if (!config || [config count] == 0) {
        return;
    }
    
    if (_running) {
        NSLog(@"[BTrace] running, will not update config");
        return;
    }

    _enable = [[config objectForKey:@"enable"] boolValue];
    _zip = [[config objectForKey:@"zip"] boolValue];
    _timeout = MIN([[config objectForKey:@"timeout"] intValue], MAX_DURATION);
#if DEBUG || INHOUSE_TARGET || TEST_MODE || READING_DEV
    _maxRecords = INT32_MAX;
#else
    _maxRecords = MIN([[config objectForKey:@"max_records"] intValue], MAX_RECORDS);
#endif
    _bufferSize = MAX([[config objectForKey:@"buffer_size"] intValue], 0);
    NSDictionary *dumpSampling = [config objectForKey:@"dump_sampling"];
    _dumpSampling = dumpSampling!=nil?dumpSampling:@{
        @"hitch": @(4),
        @"hang": @(4),
        @"log_hang": @(4)
    };
    id enable_thread_map_oc = [config objectForKey:@"enable_thread_map"];
    if (enable_thread_map_oc == nil) {
        OSThread::enable_thread_map_ = true;
    } else {
        OSThread::enable_thread_map_ = [enable_thread_map_oc boolValue];
    }
    id enable_pthread_hook_oc = [config objectForKey:@"enable_pthread_hook"];
    if (enable_pthread_hook_oc == nil) {
        OSThread::enable_pthread_hook_ = true;
    } else {
        OSThread::enable_pthread_hook_ = [enable_pthread_hook_oc boolValue];
    }
    [self setupTracers:[config objectForKey:@"plugins"]];
    [self setupAutoStartup:[config objectForKey:@"auto_startup"]];
}

- (void)setupTracers:(NSArray<NSDictionary *> *)plugins {
    if (!plugins) {
        return;
    }
    
    [_tracers removeAllObjects];

    for (NSDictionary *config in plugins) {
        NSString *plugin_name = [config objectForKey:@"name"];
        NSDictionary *plugin_config = [config objectForKey:@"config"];

        BTracePlugin *tracer = nil;

        if ([plugin_name isEqualToString:[BTraceProfilerPlugin name]]) {
            tracer = [BTraceProfilerPlugin shared];
            auto config = [[BTraceProfilerConfig alloc]
                                     initWithDictionary:plugin_config];
            [tracer updateConfig:config];
        } else if ([plugin_name isEqualToString:[BTraceTimeSeriesPlugin name]]) {
            tracer = [BTraceTimeSeriesPlugin shared];
            auto config = [[BTraceTimeSeriesConfig alloc]
                                       initWithDictionary:plugin_config];
            [tracer updateConfig:config];
        } else if ([plugin_name isEqualToString:[BTraceMemProfilerPlugin name]]) {
            tracer = [BTraceMemProfilerPlugin shared];
            auto config = [[BTraceMemProfilerConfig alloc]
                                           initWithDictionary:plugin_config];
            [tracer updateConfig:config];
        } else if ([plugin_name isEqualToString:[BTraceMonitorPlugin name]]) {
            tracer = [BTraceMonitorPlugin shared];
            auto config = [[BTraceMonitorConfig alloc]
                                   initWithDictionary:plugin_config];
            [tracer updateConfig:config];
        } else if ([plugin_name isEqualToString:[BTraceDispatchProfilerPlugin name]]) {
            tracer = [BTraceDispatchProfilerPlugin shared];
            auto config = [[BTraceDispatchProfilerConfig alloc]
                                   initWithDictionary:plugin_config];
            [tracer updateConfig:config];
        } else if ([plugin_name isEqualToString:[BTraceDateProfilerPlugin name]]) {
            tracer = [BTraceDateProfilerPlugin shared];
            auto config = [[BTraceDateProfilerConfig alloc]
                                   initWithDictionary:plugin_config];
            [tracer updateConfig:config];
        } else if ([plugin_name isEqualToString:[BTraceLockProfilerPlugin name]]) {
            tracer = [BTraceLockProfilerPlugin shared];
            auto config = [[BTraceLockProfilerConfig alloc]
                                   initWithDictionary:plugin_config];
            [tracer updateConfig:config];
        }

        if (tracer && ![_tracers containsObject:tracer]) {
            [_tracers addObject:tracer];
        }
    }
}

- (void)setupAutoStartup:(NSArray<NSDictionary *> *)config {
    if (!config) {
        return;
    }

    for (NSDictionary *method_info in config) {
        NSString *beginMethod = [method_info objectForKey:@"B"];
        NSString *endMethod = [method_info objectForKey:@"E"];
        
        MethodHookInfo *begin = parseMethod(beginMethod);
        MethodHookInfo *end = parseMethod(endMethod);
        
        bool flag = false;

        if (begin) {
            int err = autoHookMethod(begin, STOptionBefore | STOptionWeakCheckSignature, ^(id<StingerParams> params) {
                mach_port_t tid = OSThread::GetCurrentThreadId();
                int64_t timestamp = Utils::GetCurrentTimeMicros();
                
                dispatch_async(self->_queue, ^{
                    [self markBegin:begin withTid:tid Timestamp:timestamp];
                   });
                 });

            if (err != 0) {
                flag = true;
            }
        }
        
        if (end) {
            int err = autoHookMethod(end, STOptionAfter | STOptionWeakCheckSignature, ^(id<StingerParams> params) {
                mach_port_t tid = OSThread::GetCurrentThreadId();
                int64_t timestamp = Utils::GetCurrentTimeMicros();
                
                dispatch_async(self->_queue, ^{
                    [self markEnd:end withBegin:begin tid:tid Timestamp:timestamp];
                   });
                 });

            if (err != 0) {
                flag = true;
            }
        }
        
        if (flag) {
            MethodPair *methodPair = [[MethodPair alloc] initWithBegin:begin End:end];
            dispatch_async(_queue, ^{
                [self->_methodPairList addObject:methodPair];
            });
        }
    }
}

- (void)markBegin:(MethodHookInfo *)begin withTid: (mach_port_t)tid Timestamp:(int64_t)timestamp {
    [self start];
    
    if (begin.execTime) {
        return;
    }
    
    begin.tid = tid;
    begin.execTime = timestamp;
}

- (void)markEnd:(MethodHookInfo *)end withBegin:(MethodHookInfo *)begin tid: (mach_port_t)tid Timestamp:(int64_t)timestamp {
    int64_t beginTime = 0;
    
    if (!begin) {
        return;
    }
    
    beginTime = begin.execTime;
    begin.execTime = 0;
    
    if (!beginTime) {
        return;
    }
    
    NSString *beginName = [NSString stringWithFormat:@"%@[%@ %@]",
                             begin.flag, begin.cls, begin.sel];
    NSString *endName = [NSString stringWithFormat:@"%@[%@ %@]",
                           end.flag, end.cls, end.sel];
    NSDictionary *info = @{
        @"begin_time": @(beginTime),
        @"end_time": @(timestamp),
        @"begin_name": beginName,
        @"end_name": endName,
        @"begin_tid": @(begin.tid),
        @"end_tid": @(tid),
    };
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:info
                                                       options:0
                                                         error:nil];
    NSString *jsonString = [[NSString alloc] initWithData:jsonData
                                                 encoding:NSUTF8StringEncoding];
    [self dumpWithTag:"method_pair"
                 Info:[jsonString UTF8String]
            BeginTime:beginTime
              EndTime:timestamp];
}

- (void)markType:(int64_t)type {
    int64_t val = 1 << (type - 1);
    _type |= val;
}

@end

@implementation MethodHookInfo

-(instancetype)initWithFlag:(NSString * _Nullable)flag Cls:(NSString * _Nullable)cls Sel:(NSString * _Nullable)sel {
    self = [super init];
    
    if (self) {
        _flag = flag;
        _cls = cls;
        _sel = sel;
    }
    
    return self;
}

@end

@implementation MethodPair

-(instancetype)initWithBegin:(MethodHookInfo * _Nullable)b End:(MethodHookInfo * _Nullable)e {
    self = [super init];
    
    if (self) {
        _beginMethod = b;
        _endMethod = e;
    }
    
    return self;
}

@end

MethodHookInfo *parseMethod(NSString * _Nullable methodName) {
    if (methodName == nil) {
        return nil;
    }
    
    NSString *clsFlag = nil;
    NSString *clsName = nil;
    NSString *selName = nil;

    NSRegularExpression *regex = [NSRegularExpression
        regularExpressionWithPattern:
            @"([-|+])\\s*\\[\\s*(\\w+)\\s*([\\w:.]*)\\s*\\]"
                             options:NSRegularExpressionCaseInsensitive
                               error:NULL];
    NSTextCheckingResult *match =
        [regex firstMatchInString:methodName
                          options:0
                            range:NSMakeRange(0, [methodName length])];
    if (match) {
        clsFlag = [methodName substringWithRange:[match rangeAtIndex:1]];
        clsName = [methodName substringWithRange:[match rangeAtIndex:2]];
        selName = [methodName substringWithRange:[match rangeAtIndex:3]];
    } else {
        return nil;
    }
    
    return [[MethodHookInfo alloc] initWithFlag:clsFlag Cls:clsName Sel:selName];
}


int autoHookMethod(MethodHookInfo *methodInfo, STOption op, id block) {
    
    if (methodInfo == nil) {
        return -1;
    }
    
    if (methodInfo.hooked) {
        return 0;
    }
    
    NSString *recordkey = [NSString stringWithFormat:@"%@[%@ %@]_%d", methodInfo.flag, methodInfo.cls, methodInfo.sel, op];
    
    if ([METHOD_RECORD containsObject:recordkey]) {
        methodInfo.hooked = YES;
        return 0;
    }
    
    Class cls = NSClassFromString(methodInfo.cls);
    
    if (cls == nil) {
        return 1;
    }

    if ([methodInfo.flag isEqualToString:@"-"]) {
    } else if ([methodInfo.flag isEqualToString:@"+"]) {
        cls = object_getClass(cls);
    } else {
        return 2;
    }
    
    STHookResult token = [cls st_hookInstanceMethod:NSSelectorFromString(methodInfo.sel)
                                             option:op
                                    usingIdentifier:methodInfo.sel
                                          withBlock:block];
    
    if (token != STHookResultSuccess) {
        return 3;
    }
    
    methodInfo.hooked = YES;
    
    return 0;
}

namespace btrace {
void image_info_callback(ImageInfo *image_info) {
    if (s_queue == nullptr) {
        return;
    }
    
    if (s_btrace == nullptr) {
        return;
    }
    
    dispatch_async(s_queue, ^{
        NSMutableArray<MethodPair *> *tempMethodPairList = [NSMutableArray array];
        
        for (MethodPair *pair in s_btrace.methodPairList) {
            MethodHookInfo *begin = pair.beginMethod;
            MethodHookInfo *end = pair.endMethod;
            
            bool flag = false;
            
            if (begin && !begin.hooked) {
                int err = autoHookMethod(begin, STOptionBefore | STOptionWeakCheckSignature, ^(id<StingerParams> params) {
                    mach_port_t tid = OSThread::GetCurrentThreadId();
                    int64_t timestamp = Utils::GetCurrentTimeMicros();
                    
                    dispatch_async(s_queue, ^{
                        [s_btrace markBegin:begin withTid:tid Timestamp:timestamp];
                       });
                     });
                
                if (err != 0) {
                    flag = true;
                }
            }
            
            if (end && !end.hooked) {
                int err = autoHookMethod(end, STOptionAfter | STOptionWeakCheckSignature, ^(id<StingerParams> params) {
                    mach_port_t tid = OSThread::GetCurrentThreadId();
                    int64_t timestamp = Utils::GetCurrentTimeMicros();
                    
                    dispatch_async(s_queue, ^{
                        [s_btrace markEnd:end withBegin:begin tid:tid Timestamp:timestamp];
                       });
                     });

                if (err != 0) {
                    flag = true;
                }
            }
            
            if (flag) {
                [tempMethodPairList addObject:pair];
            }
        }
        
        s_btrace.methodPairList = tempMethodPairList;
    });
}
}
