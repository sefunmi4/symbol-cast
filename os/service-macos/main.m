#import <Cocoa/Cocoa.h>
#import "ServiceDelegate.h"

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        ServiceDelegate *delegate = [[ServiceDelegate alloc] init];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
