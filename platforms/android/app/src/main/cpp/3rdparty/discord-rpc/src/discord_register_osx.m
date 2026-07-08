#include <stdio.h>
#include <sys/stat.h>

#import <AppKit/AppKit.h>

#include "discord_register.h"

static void RegisterCommand(const char* applicationId, const char* command)
{
    // There does not appear to be a way to register arbitrary commands on OSX, so instead we'll save the command
    // to a file in the Discord config path, and when it is needed, Discord can try to load the file there, open
    // the command therein (will pass to js's window.open, so requires a url-like thing)

    // Note: will not work for sandboxed apps
  	NSString *home = NSHomeDirectory();
    if (!home) {
        return;
    }

    NSString *path = [[[[[[home stringByAppendingPathComponent:@"Library"]
                                stringByAppendingPathComponent:@"Application Support"]
                                stringByAppendingPathComponent:@"discord"]
                                stringByAppendingPathComponent:@"games"]
                                stringByAppendingPathComponent:[NSString stringWithUTF8String:applicationId]]
                                stringByAppendingPathExtension:@"json"];
    [[NSFileManager defaultManager] createDirectoryAtPath:[path stringByDeletingLastPathComponent] withIntermediateDirectories:YES attributes:nil error:nil];

    NSString *jsonBuffer = [NSString stringWithFormat:@"{\"command\": \"%s\"}", command];
    [jsonBuffer writeToFile:path atomically:NO encoding:NSUTF8StringEncoding error:nil];
}

static void RegisterURL(const char* applicationId)
{
    char url[256];
    snprintf(url, sizeof(url), "discord-%s", applicationId);
    CFStringRef cfURL = CFStringCreateWithCString(NULL, url, kCFStringEncodingUTF8);

    NSString* myBundleId = [[NSBundle mainBundle] bundleIdentifier];
    if (!myBundleId) {
        fprintf(stderr, "No bundle id found\n");
        return;
    }

    NSURL* myURL = [[NSBundle mainBundle] bundleURL];
    if (!myURL) {
        fprintf(stderr, "No bundle url found\n");
        return;
    }

    OSStatus status = LSSetDefaultHandlerForURLScheme(cfURL, (__bridge CFStringRef)myBundleId);
    if (status != noErr) {
        fprintf(stderr, "Error in LSSetDefaultHandlerForURLScheme: %d\n", (int)status);
        return;
    }

    status = LSRegisterURL((__bridge CFURLRef)myURL, true);
    if (status != noErr) {
        fprintf(stderr, "Error in LSRegisterURL: %d\n", (int)status);
    }
}

void Discord_Register(const char* applicationId, const char* command)
{
    if (command) {
        RegisterCommand(applicationId, command);
    }
    else {
        // raii lite
        @autoreleasepool {
            RegisterURL(applicationId);
        }
    }
}

void Discord_RegisterSteamGame(const char* applicationId, const char* steamId)
{
    char command[256];
    snprintf(command, 256, "steam://rungameid/%s", steamId);
    Discord_Register(applicationId, command);
}
