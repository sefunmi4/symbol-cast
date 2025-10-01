#import "ServiceDelegate.h"

static NSString *const kSymbolCastServiceName = @"com.symbolcast.desktop.service";
static NSString *const kPresentWindowNotification = @"com.symbolcast.desktop.presentWindow";

@implementation ServiceDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
    NSRegisterServicesProvider(self, kSymbolCastServiceName);
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    NSUnregisterServicesProvider(kSymbolCastServiceName);
}

- (void)presentSymbolCast:(NSPasteboard *)pboard
                 userData:(NSString *)userData
                    error:(NSString *__autoreleasing  _Nullable *)error {
    (void)pboard;
    (void)userData;
    if (error) {
        *error = nil;
    }
    [[NSDistributedNotificationCenter defaultCenter]
        postNotificationName:kPresentWindowNotification
                      object:nil
                    userInfo:nil
          deliverImmediately:YES];
    [NSApp terminate:nil];
}

@end
