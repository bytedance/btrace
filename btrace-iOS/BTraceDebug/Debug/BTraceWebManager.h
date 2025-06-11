//
//  BTraceWebManager.h
//  GCDWebServer
//
//

#import <Foundation/Foundation.h>

@interface BTraceWebManager : NSObject

- (void)start;
- (void)stop;

- (nullable NSString *)serverURL;

@end
