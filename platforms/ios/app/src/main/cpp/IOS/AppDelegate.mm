// AppDelegate.mm — iOS app delegate, directory bootstrap, and main().

#define SDL_MAIN_HANDLED

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mach-o/dyld.h>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

#include "common/Console.h"
#include "common/Path.h"

#include "pcsx2/Config.h"            // EmuFolders
#include "pcsx2/ImGui/ImGuiManager.h"

#include "common/Darwin/DarwinMisc.h"

#import <UIKit/UIKit.h>

#include "IOSRuntime.h"
#import "IOS/PCSX2AppDelegate.h"
#import "IOS/PCSX2SceneDelegate.h"

#pragma mark - SetupIOSDirectories
static void SetupIOSDirectories(const std::string& dataRoot)
{
    const char* dirs[] = {"bios", "iso", "logs", "memcards", "savestates",
                          "snaps", "cheats", "patches", "cache", "covers",
                          "gamesettings", "textures", "inputprofiles", "videos",
                          "inis", "resources"};
    mkdir(dataRoot.c_str(), 0755);
    for (auto d : dirs)
        mkdir((dataRoot + "/" + d).c_str(), 0755);

    EmuFolders::DataRoot = dataRoot;
    EmuFolders::Settings = dataRoot + "/inis";
    EmuFolders::Bios = dataRoot + "/bios";
    EmuFolders::Logs = dataRoot + "/logs";
    EmuFolders::Savestates = dataRoot + "/savestates";
    EmuFolders::MemoryCards = dataRoot + "/memcards";
    EmuFolders::Snapshots = dataRoot + "/snaps";
    EmuFolders::Cheats = dataRoot + "/cheats";
    EmuFolders::Patches = dataRoot + "/patches";
    EmuFolders::Cache = dataRoot + "/cache";
    EmuFolders::Covers = dataRoot + "/covers";
    EmuFolders::GameSettings = dataRoot + "/gamesettings";
    EmuFolders::Textures = dataRoot + "/textures";
    EmuFolders::InputProfiles = dataRoot + "/inputprofiles";
    EmuFolders::Videos = dataRoot + "/videos";
    EmuFolders::UserResources = dataRoot + "/resources";
}


@implementation PCSX2AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Override point for customization after application launch.
    [UIDevice currentDevice].batteryMonitoringEnabled = YES;
    
    // --- Setup PCSX2 Environment (Moved from SceneDelegate) ---
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
    
    std::string dataRoot = [documentsDirectory UTF8String];
    SetupIOSDirectories(dataRoot);
    EmuFolders::AppRoot = [resourcePath UTF8String];
    EmuFolders::Resources = [resourcePath UTF8String];

    // --- Unified Logging Redirection ---
    // Force stderr and stdout to pcsx2_log.txt
    std::string logPath = dataRoot + "/pcsx2_log.txt";
    
    // Redirect stderr to file
    if (freopen(logPath.c_str(), "w", stderr) == NULL) { // "w" clears old logs
        printf("Reopen stderr failed\n");
    }
    
    // Redirect stdout to stderr
    if (dup2(fileno(stderr), fileno(stdout)) == -1) {
        fprintf(stderr, "Redirection of stdout failed\n");
    }
    
    // Disable buffering
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    
    // [iPSX2] Register File Descriptor for Signal Handler
    // We use the raw file descriptor of stderr (which is now our log file)
    DarwinMisc::SetCrashLogFD(fileno(stderr));
    
    // Log Proof Tag
    fprintf(stderr, "@@LOG_SINK@@ unified=1 path=%s pid=%d\n", logPath.c_str(), getpid());
    NSString* bundleID = [[NSBundle mainBundle] bundleIdentifier];
    fprintf(stderr, "@@BUNDLE_ID@@ %s\n", bundleID ? [bundleID UTF8String] : "(null)");
#ifndef ARMSX2_VERSION_STR
#define ARMSX2_VERSION_STR "dev"
#endif
#ifndef ARMSX2_GIT_HASH
#define ARMSX2_GIT_HASH "unknown"
#endif
#ifndef ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
#define ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS 0
#endif
    fprintf(stderr, "@@BUILD_ID@@ ARMSX2_iOS v%s %s %s %s\n",
        ARMSX2_VERSION_STR, ARMSX2_GIT_HASH, __DATE__, __TIME__);
    fprintf(stderr, "@@TEST_MARKER@@ armsx2_ios_43_v232_metadata_v1\n");
    fprintf(stderr, "@@FF_FIX@@ offspeed_present_skip=1 present_cap60=1 adaptive_backoff=1 drawable_wait_probe=1 vm_pace_probe=1 turbo_only_toggle=1\n");
    fprintf(stderr, "@@DIAG_MODE@@ ee_hotpath=%d\n", ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS);
    
    // [iPSX2] Unification Validation
    // @@BIOS_GATE@@ build_id=2026-01-14_13-30-00 bundle=(from_nsbundle)
    NSString* bID = [[NSBundle mainBundle] bundleIdentifier];
    const char* cBundle = bID ? [bID UTF8String] : "(null)";
    fprintf(stderr, "@@BIOS_GATE@@ build_id=2026-01-17_PROBE bundle=%s\n", cBundle);
    fprintf(stderr, "@@LOG_UNIFIED@@ pcsx2_log.txt includes emulog output; emulog.txt disabled=1\n");
    ARMSX2ConfigureImGuiFonts("app-launch");
    
// DYLD Map — debug builds only
#if DEBUG
    {
        fprintf(stderr, "@@CFG@@ iPSX2_CRASH_DIAG=1 (DYLD Dump Enabled)\n");
        uint32_t count = _dyld_image_count();
        for (uint32_t i = 0; i < count; i++) {
            const char* name = _dyld_get_image_name(i);
            intptr_t slide = _dyld_get_image_vmaddr_slide(i);
            const struct mach_header* hdr = _dyld_get_image_header(i);
            fprintf(stderr, "@@DYLD_MAP@@ idx=%u addr=%p slide=%p path=%s\n", i, hdr, (void*)slide, name);
        }
    }
#endif
    fflush(stderr);

    // Enable PCSX2 Console Output only (std::cout/cerr will now go to file)
    Log::SetConsoleOutputLevel(LOGLEVEL::LOGLEVEL_INFO);
    
    Console.WriteLn("PCSX2 iOS: AppDelegate didFinishLaunching.");
    
    return YES;
}

- (UISceneConfiguration *)application:(UIApplication *)application configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession options:(UISceneConnectionOptions *)options {
    // Called when a new scene session is being created.
    // Use this method to select a configuration to create the new scene with.
    UISceneConfiguration *config = [[UISceneConfiguration alloc] initWithName:@"Default Configuration" sessionRole:connectingSceneSession.role];
    config.delegateClass = [PCSX2SceneDelegate class];
    return config;
}

- (void)application:(UIApplication *)application didDiscardSceneSessions:(NSSet<UISceneSession *> *)sceneSessions {
}

@end


int main(int argc, char * argv[]) {
    @autoreleasepool {
        // SDL_MAIN_HANDLED is set, so we use standard main()
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([PCSX2AppDelegate class]));
    }
}
