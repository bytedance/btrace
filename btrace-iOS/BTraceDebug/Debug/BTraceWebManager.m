//
//  BTraceWebManager.m
//  GCDWebServer
//
//

#import "BTrace.h"
#import "BTraceWebManager.h"
#import <GCDWebServer/GCDWebserver.h>
#import <GCDWebServer/GCDWebServerDataRequest.h>
#import <GCDWebServer/GCDWebServerDataResponse.h>

static BTraceWebManager *WebManager;
@interface BTraceWebManager ()

@property (nonatomic, strong) GCDWebServer *webServer;
@property (nonatomic, strong) dispatch_queue_t processingQueue;

@end

@implementation BTraceWebManager

+ (void)load {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        WebManager = [[BTraceWebManager alloc] init];
        [WebManager start];
        
        NSDictionary *envDict = [[NSProcessInfo processInfo] environment];
        NSString *jsonConfigStr = [envDict objectForKey: @"BTrace"];
        NSLog(@"[BTrace]: jsonConfigStr: %@", jsonConfigStr);
        NSData *jsonConfigData = [jsonConfigStr dataUsingEncoding:NSUTF8StringEncoding];
        
        if (jsonConfigData == nil) {
            jsonConfigData = [self loadLocalConfig];
        }
        
        if (jsonConfigData != nil) {
            NSError *error;
            NSDictionary *config = [NSJSONSerialization JSONObjectWithData:jsonConfigData
                                                                    options:kNilOptions
                                                                        error:&error];
            
            if (error != nil) {
                NSLog(@"[BTrace] error: %@", error.description);
            } else if (config != nil) {
                [BTraceWebManager startTraceWithConfig:config];
            }
        }
    });
}

+ (void)startTraceWithConfig:(NSDictionary * _Nullable)config {
    [[BTrace shared] setupWithConfig:config];
    [[BTrace shared] start];
}

+ (void)stopTraceWithCallback:(BTraceCallback _Nullable)callback {
    [[BTrace shared] stopWithCallback:callback];
}

- (void)stopWebServer
{
    [WebManager stop];
}

- (instancetype)init {
    if (self = [super init]) {
        self.processingQueue = dispatch_queue_create("bddp_web_processing_queue", DISPATCH_QUEUE_CONCURRENT);
        
        self.webServer = [GCDWebServer new];
        [self registerHandler];
    }
    return self;
}

- (void)start {
    NSDictionary *info = [[NSBundle mainBundle] infoDictionary];
    NSString *appName = [info objectForKey:@"CFBundleDisplayName"] ?: [info objectForKey:@"CFBundleName"];
    [self.webServer startWithPort:1128
                      bonjourName:[NSString stringWithFormat:@"%@.%@.BDDPPerformance", [[UIDevice currentDevice] name], appName]];
}

- (void)stop {
    [self.webServer stop];
}

- (NSString *)serverURL {
    return [self.webServer.serverURL absoluteString];
}

- (GCDWebServerResponse *)processingPath:(GCDWebServerRequest *)request {
    NSString *domain = @"";
    NSString *path = request.path;
    
    return [GCDWebServerResponse responseWithStatusCode:200];
}

- (void)registerHandler {
    __weak typeof(self) weakSelf = self;
    
    [self.webServer addHandlerForMethod:@"GET" pathRegex:@".*" 
                           requestClass:[GCDWebServerRequest class]
                      asyncProcessBlock:^(__kindof GCDWebServerRequest * _Nonnull request, 
                                          GCDWebServerCompletionBlock  _Nonnull completionBlock) {
        typeof(weakSelf) self = weakSelf;
        
        if (!self)
            return;
        
        dispatch_async(weakSelf.processingQueue, ^{
            GCDWebServerResponse *response = [self processingPath:request];
            completionBlock(response);
        });
    }];
    
    [self.webServer addHandlerForMethod:@"POST" pathRegex:@"/record/start"
                           requestClass:[GCDWebServerDataRequest class]
                      asyncProcessBlock:^(__kindof GCDWebServerDataRequest * _Nonnull request, 
                                          GCDWebServerCompletionBlock  _Nonnull completionBlock) {
        typeof(weakSelf) self = weakSelf;
        
        if (!self)
            return;
        
        NSError *error;
        NSDictionary *config = [NSJSONSerialization JSONObjectWithData:request.data
                                                               options:kNilOptions
                                                                 error:&error];
        
        if (error != nil) {
            GCDWebServerDataResponse *resp = [GCDWebServerDataResponse responseWithText:error.description];
            resp.statusCode = 500;
            completionBlock(resp);
            return;
        }
        
        dispatch_async(weakSelf.processingQueue, ^{
            [BTraceWebManager startTraceWithConfig:config];
            GCDWebServerResponse *resp = [GCDWebServerResponse response];
            completionBlock(resp);
        });
    }];

    [self.webServer addHandlerForMethod:@"POST" pathRegex:@"/record/stop"
                           requestClass:[GCDWebServerRequest class]
                      asyncProcessBlock:^(__kindof GCDWebServerRequest * _Nonnull request, 
                                          GCDWebServerCompletionBlock  _Nonnull completionBlock) {
        typeof(weakSelf) self = weakSelf;
        
        if (!self)
            return;
        
        dispatch_async(weakSelf.processingQueue, ^{
            [BTraceWebManager stopTraceWithCallback:^(NSData *data){
                GCDWebServerResponse *response = [GCDWebServerDataResponse responseWithData:data contentType:@"sqlite.gz"];
                completionBlock(response);
            }];
        });
    }];
    
    [self.webServer addHandlerForMethod:@"POST" pathRegex:@"/config/upload"
                           requestClass:[GCDWebServerDataRequest class]
                      asyncProcessBlock:^(__kindof GCDWebServerDataRequest * _Nonnull request,
                                          GCDWebServerCompletionBlock  _Nonnull completionBlock) {
        typeof(weakSelf) self = weakSelf;
        
        if (!self)
            return;
        
        NSData *data = request.data;
        NSString *configPath = [self.class configPath];
        
        dispatch_async(weakSelf.processingQueue, ^{
            BOOL res = NO;
            NSFileManager *fileManager = [NSFileManager defaultManager];
            
            if (![fileManager fileExistsAtPath:configPath]) {
                res = [fileManager createFileAtPath:configPath contents:data attributes:nil];
            } else {
                res = [data writeToFile:configPath atomically:NO];
            }
            
            GCDWebServerResponse *resp = [GCDWebServerResponse response];
            if (!res) {
                resp.statusCode = 500;
            }
            completionBlock(resp);
        });
    }];
}

+ (NSData *)loadLocalConfig {
    NSData *jsonConfigData = nil;
    NSString *configPath = [self configPath];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    
    if ([fileManager fileExistsAtPath:configPath]){
        jsonConfigData = [NSData dataWithContentsOfFile:configPath];
        [fileManager removeItemAtPath:configPath error:nil];
    }
    
    return jsonConfigData;
}

+ (NSString *)configPath {
    NSString *cachePath = [NSSearchPathForDirectoriesInDomains(
        NSCachesDirectory, NSUserDomainMask, YES) firstObject];
    NSString *rootFolderPath =
        [cachePath stringByAppendingPathComponent:@"BTrace"];
    NSString *configPath =
        [rootFolderPath stringByAppendingPathComponent:@"config.json"];
    
    return configPath;
}

@end
