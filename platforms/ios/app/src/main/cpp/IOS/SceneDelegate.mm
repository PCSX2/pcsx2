// SceneDelegate.mm — iOS scene delegate: SDL/PCSX2 bootstrap, window setup,
// SwiftUI menu attachment, BIOS discovery, and the persistent VM thread.

#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mach-o/dyld.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "common/Console.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"

#include "pcsx2/CDVD/CDVD.h"          // cdvdSaveNVRAM
#include "pcsx2/ps2/BiosTools.h"      // BiosPath
#include "pcsx2/CDVD/CDVDcommon.h"
#include "pcsx2/Config.h"             // EmuConfig, GSConfig
#include "pcsx2/Counters.h"           // g_FrameCount
#include "pcsx2/GameList.h"
#include "pcsx2/Host.h"
#include "pcsx2/PerformanceMetrics.h"
#include "pcsx2/R5900.h"              // cpuRegs
#include "pcsx2/VMManager.h"
#include "pcsx2/vtlb.h"               // vtlb_FastmemAreaUnavailable
#include "pcsx2/SIO/Memcard/MemoryCardFile.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "IconsFontAwesome.h"

#include "common/Darwin/DarwinMisc.h"

#import <UIKit/UIKit.h>
#import <SwiftUI/SwiftUI.h>

// Xcode names the generated Swift bridge header after the Swift module.
#if __has_include("ARMSX2iOS-Swift.h")
#import "ARMSX2iOS-Swift.h"
#define ARMSX2_HAS_SWIFTUI 1
#elif __has_include("ARMSX2-Swift.h")
#import "ARMSX2-Swift.h"
#define ARMSX2_HAS_SWIFTUI 1
#else
#define ARMSX2_HAS_SWIFTUI 0
#endif

#include "IOSRuntime.h"
#import "IOS/ARMSX2GameView.h"
#import "IOS/PCSX2SceneDelegate.h"

@implementation PCSX2SceneDelegate

#pragma mark - Scene connection & bootstrap
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)connectionOptions {
    if (![scene isKindOfClass:[UIWindowScene class]]) return;
    
    UIWindowScene *windowScene = (UIWindowScene *)scene;
    
    // --- SDL Initialization ---
    static bool s_initialized = false;
    if (!s_initialized) {
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) < 0) {
            NSLog(@"SDL_Init failed: %s", SDL_GetError());
            return;
        }
        s_initialized = true;
    }
    ARMSX2InstallNativeGamepadDpadObserversOnMain();
    
    // --- Setup PCSX2 Environment ---
    // (Moved to AppDelegate)
    // We still need local variables if used below, but EmuFolders are global.
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    std::string dataRoot = [documentsDirectory UTF8String];
    
    // Re-ensure EmuFolders (idempotent)
    EmuFolders::DataRoot = dataRoot;
    EmuFolders::Bios = dataRoot + "/bios";
    // ...
    
    Console.WriteLn("PCSX2 iOS: Initializing logic in SceneDelegate...");
    
    // Settings Initialization
    if (!s_settings_interface) {
        std::string iniPath = dataRoot + "/PCSX2-iOS.ini";
        s_settings_interface = new INISettingsInterface(iniPath);
        if (!static_cast<INISettingsInterface*>(s_settings_interface)->Load()) {
            Console.WriteLn("Creating new config at %s", iniPath.c_str());
            
            // [iPSX2] Standard Defaults: JIT Enabled (if supported), EE/IOP/VU Recompilers ON
            s_settings_interface->SetIntValue("EmuCore/CPU", "CoreType", 2); // ARM64 JIT in the Swift UI model
            s_settings_interface->SetBoolValue("EmuCore/CPU", "UseArm64Dynarec", true);
            s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", true);
            s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableIOP", true);
            s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", true);
            s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU1", true);
            s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", true);
            s_settings_interface->SetBoolValue("EmuCore/CPU", "EnableSparseMemory", true);
            s_settings_interface->SetBoolValue("EmuCore/CPU", "ExtraMemory", false);

            // Audio
            s_settings_interface->SetStringValue("SPU2/Output", "Backend", "SDL");

            // GS
            s_settings_interface->SetIntValue("EmuCore/GS", "VsyncQueueSize", 8);

            // Normal console-speed frame limiter. Do not save reduced nominal
            // speed here; that is not a safe FPS cap for iOS.
            s_settings_interface->SetFloatValue("Framerate", "NominalScalar", 1.0f);
            s_settings_interface->SetBoolValue("GameISO", "FastBoot", false);
            s_settings_interface->SetBoolValue("EmuCore", "EnableFastBoot", false);

            // Speedhacks
            s_settings_interface->SetBoolValue("EmuCore/Speedhacks", "vuThread", ARMSX2ShouldEnableMTVUByDefault(nullptr));

            // RetroAchievements
            s_settings_interface->SetBoolValue("Achievements", "Enabled", false);
            s_settings_interface->SetBoolValue("Achievements", "ChallengeMode", false);

            // iOS controller defaults.
            s_settings_interface->SetIntValue("ARMSX2iOS/Gamepad", "MultitapMode", 0);
            
            Console.WriteLn("@@CFG_DEFAULTS@@ created=1 CoreType=2 UseArm64Dynarec=true EnableEE=1 FastBoot=1");
            s_settings_interface->Save();
        }
        Host::Internal::SetBaseSettingsLayer(s_settings_interface);
        std::string secretsPath = dataRoot + "/PCSX2-iOS-secrets.ini";
        s_secrets_settings_interface = new INISettingsInterface(secretsPath);
        s_secrets_settings_interface->Load();
        Host::Internal::SetSecretsSettingsLayer(s_secrets_settings_interface);
        g_p44_settings_interface = s_settings_interface; // expose to Bridge
	// Load gamepad button mapping from INI
        for (int i = 0; i < 16; i++) {
            char key[32]; snprintf(key, sizeof(key), "Button%d", i);
            int val = s_settings_interface->GetIntValue("ARMSX2iOS/GamepadMapping", key, s_defaultMap[i]);
            s_buttonMap[i] = val;
        }
    }
    ARMSX2RepairIOSARM64JITSettings(s_settings_interface, "scene-connect");
    ARMSX2MigrateJITScriptProtocolForIOS(s_settings_interface, "scene-connect");
    // One-time migration for existing INI (runs once, then conditions are false)
    if (!s_settings_interface->ContainsValue("SPU2/Output", "Backend")) {
        s_settings_interface->SetStringValue("SPU2/Output", "Backend", "SDL");
    }
    if (!s_settings_interface->ContainsValue("EmuCore/CPU", "ExtraMemory")) {
        s_settings_interface->SetBoolValue("EmuCore/CPU", "ExtraMemory", false);
    }
    if (!s_settings_interface->ContainsValue("EmuCore/CPU/Recompiler", "EnableFastmem")) {
        s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", true);
    }
    if (!s_settings_interface->ContainsValue("Achievements", "Enabled")) {
        s_settings_interface->SetBoolValue("Achievements", "Enabled", false);
    }
    if (!s_settings_interface->ContainsValue("Achievements", "ChallengeMode")) {
        s_settings_interface->SetBoolValue("Achievements", "ChallengeMode", false);
    }
    if (!ARMSX2IOSRetroAchievementsHardcoreAvailable) {
        ARMSX2DisableRetroAchievementsHardcoreForIOS(s_settings_interface, "scene-connect");
    }
    if (!s_settings_interface->ContainsValue("ARMSX2iOS/Gamepad", "MultitapMode")) {
        s_settings_interface->SetIntValue("ARMSX2iOS/Gamepad", "MultitapMode", 0);
    }
    ARMSX2IOSSanitizeFolderSettings(s_settings_interface, dataRoot, "scene-connect");
    ARMSX2EnsureIOSSpeedhackDefaults(s_settings_interface, "scene-connect");
    ARMSX2SanitizeFrameLimiterConfig("scene-connect");
    ARMSX2ApplyIOSMultitapConfig("scene-connect");
    s_settings_interface->Save();
    [self checkAndConfigureBIOS];

    // GS Renderer: Metal fixed on iOS. Only override if not already Metal.
#if DEBUG
    if (const char* null_gs_env = getenv("ARMSX2_NULL_GS"); null_gs_env && atoi(null_gs_env)) {
        EmuConfig.GS.Renderer = GSRendererType::Null;
        Console.WriteLn("@@CFG@@ GS Renderer: Null (DEBUG)");
    } else
#endif
    {
        EmuConfig.GS.Renderer = GSRendererType::Metal;
    }
    s_settings_interface->Save();

    VMManager::Internal::LoadStartupSettings();
    if (!ARMSX2IOSRetroAchievementsHardcoreAvailable) {
        EmuConfig.Achievements.HardcoreMode = false;
    }
    ARMSX2SanitizeFrameLimiterConfig("after-startup-settings");
    ARMSX2ApplyIOSOsdPresetFromConfig("after-startup-settings");
    ARMSX2IOSApplyRetroAchievementsOverlayDefaults(s_settings_interface, "after-startup-settings");
    s_settings_interface->Save();
    VMManager::ApplySettings();
    ARMSX2IOSLogMemoryCardConfig("scene-connect-after-apply-settings");
    ARMSX2_PostRetroAchievementsStateChanged();
    
    // --- Create SDL Window ---
    Host::g_sdl_window = SDL_CreateWindow("PCSX2 iOS", 1280, 720, SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE);
    if (!Host::g_sdl_window) {
        Console.Error("Failed to create SDL window: %s", SDL_GetError());
        return;
    }
    
    // --- Attach UIWindow ---
    UIWindow *uiWindow = (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(Host::g_sdl_window), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
    if (uiWindow) {
        Console.WriteLn("Attaching UIWindow to Scene...");
        uiWindow.windowScene = windowScene;
        self.window = uiWindow;
        self.window.backgroundColor = [UIColor systemGroupedBackgroundColor];
        [self.window makeKeyAndVisible];

// Create game render view — SwiftUI MetalGameView (UIViewRepresentable) manages placement
        g_gameRenderView = [[ARMSX2GameView alloc] initWithFrame:CGRectZero];
        g_gameRenderView.backgroundColor = [UIColor blackColor];
        g_gameRenderView.clipsToBounds = YES;
        // Do NOT addSubview here — SwiftUI's MetalGameView handles view hierarchy
        Console.WriteLn("[Layout] Game render view created (SwiftUI-managed)");
        
// Debug-only UI elements
#if DEBUG
        if (rootVC) {
            g_logView = [[UITextView alloc] initWithFrame:CGRectMake(10, 50, 600, 300)];
            g_logView.backgroundColor = [UIColor colorWithWhite:0.0 alpha:0.5];
            g_logView.textColor = [UIColor whiteColor];
            g_logView.font = [UIFont fontWithName:@"Courier" size:10];
            g_logView.editable = NO;
            g_logView.hidden = YES;
            [rootVC.view addSubview:g_logView];
        }
#endif
    }
    
    // --- [UI] Startup Logic: Show menu first, boot on user action ---
#if ARMSX2_HAS_SWIFTUI
    {
        UIViewController *rootVC = self.window.rootViewController;
        if (rootVC) {
            rootVC.view.backgroundColor = [UIColor systemGroupedBackgroundColor];

            UIViewController *menuVC = [SwiftUIHost createMenuController];
            menuVC.view.translatesAutoresizingMaskIntoConstraints = NO;
            menuVC.view.userInteractionEnabled = YES;
// Keep hosting controller always clear — SwiftUI RootView handles its own background
            menuVC.view.backgroundColor = [UIColor clearColor];
            [rootVC.view addSubview:menuVC.view];
            [NSLayoutConstraint activateConstraints:@[
                [menuVC.view.topAnchor constraintEqualToAnchor:rootVC.view.topAnchor],
                [menuVC.view.bottomAnchor constraintEqualToAnchor:rootVC.view.bottomAnchor],
                [menuVC.view.leadingAnchor constraintEqualToAnchor:rootVC.view.leadingAnchor],
                [menuVC.view.trailingAnchor constraintEqualToAnchor:rootVC.view.trailingAnchor],
            ]];
            [rootVC addChildViewController:menuVC];
            [menuVC didMoveToParentViewController:rootVC];
            s_menuVC = menuVC;
            s_rootVC = rootVC;

            if (g_logView) {
                g_logView.hidden = YES;
                g_logView.userInteractionEnabled = NO;
            }
            Console.WriteLn("[UI] SwiftUI menu attached (screen: %.0fx%.0f)",
                rootVC.view.bounds.size.width, rootVC.view.bounds.size.height);

}
    }

    // Listen for VM boot request from SwiftUI
    // queue:nil = synchronous delivery, so background colors are set BEFORE
    // SwiftUI re-renders the game overlay (avoids gray flash)
    [[NSNotificationCenter defaultCenter] addObserverForName:@"ARMSX2iOSRequestVMBoot"
                                                      object:nil
                                                       queue:nil
                                                  usingBlock:^(NSNotification * _Nonnull note) {
        std::string bootISO;
        if (s_settings_interface)
            bootISO = s_settings_interface->GetStringValue("GameISO", "BootISO", "");
        const std::string biosPath = Path::Combine(EmuFolders::Bios, EmuConfig.BaseFilenames.Bios);
        std::fprintf(stderr, "@@BOOT_NOTIFY@@ received=1 has_bios=%d bios=\"%s\" boot_iso=\"%s\"\n",
            (!EmuConfig.BaseFilenames.Bios.empty() && FileSystem::FileExists(biosPath.c_str())) ? 1 : 0,
            EmuConfig.BaseFilenames.Bios.c_str(), bootISO.c_str());
        std::fflush(stderr);
        Console.WriteLn("[UI] VM boot requested from UI (rootVC=%p)", s_rootVC);
        ARMSX2ApplyIOSMultitapConfig("boot-request");
        if (s_rootVC) s_rootVC.view.backgroundColor = [UIColor blackColor];
#if TARGET_OS_SIMULATOR
        [self startVMThread];
#else
        [self checkJITAndStartVM];
#endif
    }];

    [[NSNotificationCenter defaultCenter] addObserverForName:@"ARMSX2iOSRequestVMShutdown"
                                                      object:nil
                                                       queue:nil
                                                  usingBlock:^(NSNotification * _Nonnull note) {
        Console.WriteLn("[UI] VM shutdown requested from UI");
        s_requestVMStop.store(true);
    }];

    // ARMSX2iOSVMDidShutdown / ARMSX2iOSReturnToMenu: no rootVC background change needed.
    // SwiftUI RootView handles menu background via Color(systemGroupedBackground).ignoresSafeArea().
    // rootVC stays black after first boot — eliminates white flash during VM restart.

    [[NSNotificationCenter defaultCenter] addObserverForName:@"ARMSX2iOSVMDidShutdown"
                                                      object:nil
                                                       queue:nil
                                                  usingBlock:^(NSNotification * _Nonnull note) {
        // No rootVC background change — SwiftUI handles menu bg
    }];

    [[NSNotificationCenter defaultCenter] addObserverForName:@"ARMSX2iOSReturnToMenu"
                                                      object:nil
                                                       queue:nil
                                                  usingBlock:^(NSNotification * _Nonnull note) {
        // No rootVC background change — SwiftUI handles menu bg
    }];

    [[NSNotificationCenter defaultCenter] addObserverForName:@"ARMSX2iOSEnterGameScreen"
                                                      object:nil
                                                       queue:nil
                                                  usingBlock:^(NSNotification * _Nonnull note) {
        if (s_rootVC) s_rootVC.view.backgroundColor = [UIColor blackColor];
    }];

// Auto-boot — debug/simulator only
#if DEBUG || TARGET_OS_SIMULATOR
    if (getenv("ARMSX2_AUTO_BOOT") && atoi(getenv("ARMSX2_AUTO_BOOT")) == 1) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            Console.WriteLn("[AutoBoot] @@AUTO_BOOT@@ posting ARMSX2iOSRequestVMBoot + AutoBootDidStart");
            [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2iOSRequestVMBoot" object:nil];
            [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2iOSAutoBootDidStart" object:nil];
        });
    }
#endif // DEBUG || TARGET_OS_SIMULATOR — AUTO_BOOT
    // ps2autotests: auto-boot VM when ELF env var is set (always enabled)
    if (getenv("ARMSX2_BOOT_ELF")) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2iOSRequestVMBoot" object:nil];
        });
    }

    // Cold-launch deep link: iOS delivers the launch URL through connectionOptions,
    NSSet<UIOpenURLContext *> *launchURLContexts = connectionOptions.URLContexts;
    if (launchURLContexts.count > 0) {
        dispatch_async(dispatch_get_main_queue(), ^{
            for (UIOpenURLContext *ctx in launchURLContexts) {
                NSLog(@"[ARMSX2 iOS DeepLink] cold-launch url=%@", ctx.URL.absoluteString);
                [DeepLinkBridge handle:ctx.URL];
            }
        });
    }
#else
    // Fallback: no SwiftUI — auto-boot like before
    if (!EmuConfig.BaseFilenames.Bios.empty() && FileSystem::FileExists(Path::Combine(EmuFolders::Bios, EmuConfig.BaseFilenames.Bios).c_str())) {
#if TARGET_OS_SIMULATOR
        [self startVMThread];
#else
        [self checkJITAndStartVM];
#endif
    } else {
        Console.Warning("No valid BIOS found. Showing selection UI.");
        if (self.startBiosButton) {
            self.startBiosButton.hidden = NO;
            [self.window bringSubviewToFront:self.startBiosButton];
        }
    }
#endif
}

#pragma mark - Deep-link handling
- (void)scene:(UIScene *)scene openURLContexts:(NSSet<UIOpenURLContext *> *)URLContexts {
    for (UIOpenURLContext *context in URLContexts) {
        NSURL *url = context.URL;
        [DeepLinkBridge handle:url];
    }
}

#pragma mark - BIOS discovery
- (void)checkAndConfigureBIOS {
    std::string dataRoot = EmuFolders::DataRoot;
    std::string biosDir = dataRoot + "/bios";
    
    // 0. [iPSX2] Check Env Var Override (ARMSX2_BIOS_PATH)
    const char* envBios = getenv("ARMSX2_BIOS_PATH");
    // Simulator: use ARMSX2_BIOS_PATH env var or auto-scan Documents/bios/
    // Real device: BIOS must be placed in Documents/bios/ via Files app
    Console.WriteLn("@@BIOS_DIR@@ path=\"%s\"", biosDir.c_str());
    
    if (envBios) {
        bool exists = FileSystem::FileExists(envBios);
        Console.WriteLn("@@BIOS_ENV@@ exists=%d", exists ? 1 : 0);
        
        if (exists) {
            // Copy to EmuFolders::Bios to ensure sandbox compliance
            struct stat st = {0};
            if (stat(biosDir.c_str(), &st) == -1) mkdir(biosDir.c_str(), 0755);
            
            std::string fileName(Path::GetFileName(envBios));
            std::string destPath = Path::Combine(biosDir, fileName);
            
            // Only copy if source != dest
            if (std::string(envBios) != destPath) {
                FILE *src = fopen(envBios, "rb");
                FILE *dst = fopen(destPath.c_str(), "wb");
                if (src && dst) {
                     char buffer[4096];
                     size_t bytes;
                     while ((bytes = fread(buffer, 1, 4096, src)) > 0) fwrite(buffer, 1, bytes, dst);
                     fclose(src); fclose(dst);
                     Console.WriteLn("Copied env-var BIOS to: %s", destPath.c_str());
                } else {
                     Console.Error("Failed to copy env-var BIOS. src=%p dst=%p", src, dst);
                     if (src) fclose(src);
                     if (dst) fclose(dst);
                }
            }
            
            EmuConfig.BaseFilenames.Bios = fileName;
            if (s_settings_interface) {
                s_settings_interface->SetStringValue("Filenames", "BIOS", EmuConfig.BaseFilenames.Bios.c_str());
                s_settings_interface->Save();
            }
            Console.WriteLn("@@BIOS_PICK@@ result=\"%s\" source=env", EmuConfig.BaseFilenames.Bios.c_str());
            return;
        }
    } else {
        Console.WriteLn("@@BIOS_ENV@@ exists=0");
    }

    // 1. Check existing config
    if (!EmuConfig.BaseFilenames.Bios.empty() && FileSystem::FileExists(Path::Combine(EmuFolders::Bios, EmuConfig.BaseFilenames.Bios).c_str())) {
        Console.WriteLn("@@BIOS_PICK@@ result=\"%s\" source=config", EmuConfig.BaseFilenames.Bios.c_str());
        return;
    }

    // 1b. Auto-move BIOS files from Documents/ root to bios/ subfolder
    {
        FileSystem::FindResultsArray rootResults;
        if (FileSystem::FindFiles(dataRoot.c_str(), "*", FILESYSTEM_FIND_FILES, &rootResults)) {
            for (const auto& fd : rootResults) {
                if (fd.Size >= 1024*1024 && fd.Size <= 50*1024*1024) {
                    std::string fn = std::string(Path::GetFileName(fd.FileName));
                    std::string ext = fn.size() >= 4 ? fn.substr(fn.size() - 4) : "";
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".bin" || ext == ".rom") {
                        std::string src = Path::Combine(dataRoot, fn);
                        std::string dst = Path::Combine(biosDir, fn);
                        if (!FileSystem::FileExists(dst.c_str())) {
                            if (rename(src.c_str(), dst.c_str()) == 0)
                                Console.WriteLn("[Files] Moved BIOS to bios/: %s", fn.c_str());
                            else
                                Console.WriteLn("[Files] Failed to move BIOS: %s (errno=%d)", fn.c_str(), errno);
                        }
                    }
                }
            }
        }
    }

    // 2. Scan Documents/bios
    FileSystem::FindResultsArray results;
    int foundCount = 0;
    if (FileSystem::FindFiles(biosDir.c_str(), "*", FILESYSTEM_FIND_FILES, &results)) {
        for (const auto& fd : results) {
            foundCount++;
            if (fd.Size >= 1024*1024 && (fd.FileName.find(".bin") != std::string::npos || fd.FileName.find(".BIN") != std::string::npos)) {
                // Found a candidate
                std::string currentName = std::string(Path::GetFileName(fd.FileName));
                EmuConfig.BaseFilenames.Bios = currentName;
                Console.WriteLn("Auto-detected BIOS (name only): %s", EmuConfig.BaseFilenames.Bios.c_str());
                if (s_settings_interface) {
                    s_settings_interface->SetStringValue("Filenames", "BIOS", EmuConfig.BaseFilenames.Bios.c_str());
                    s_settings_interface->Save();
                }
                Console.WriteLn("@@BIOS_PICK@@ result=\"%s\" source=scan", EmuConfig.BaseFilenames.Bios.c_str());
                return;
            }
        }
    }
    Console.WriteLn("@@BIOS_SCAN@@ found=%d", foundCount);
    Console.WriteLn("@@BIOS_PICK@@ result=\"(none)\" source=none");

    // 3. Check Bundle Resources (Fallback)
    NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
    std::string bundleDir = [resourcePath UTF8String];
    FileSystem::FindResultsArray bundleResults;

    // [iPSX2] Support "BiosFiles" folder reference
    std::string bfDir = bundleDir + "/BiosFiles";
    if (FileSystem::FindFiles(bfDir.c_str(), "*", FILESYSTEM_FIND_FILES, &bundleResults)) {
        for (const auto& fd : bundleResults) {
             if (fd.Size >= 1024*1024 && (fd.FileName.find(".bin") != std::string::npos || fd.FileName.find(".BIN") != std::string::npos)) {
                 Console.WriteLn("Found BIOS in BiosFiles: %s", fd.FileName.c_str());
                 struct stat st = {0};
                 if (stat(biosDir.c_str(), &st) == -1) mkdir(biosDir.c_str(), 0755);

                 std::string src = bfDir + "/" + fd.FileName;
                 std::string dst = biosDir + "/" + fd.FileName;
                 FILE *s=fopen(src.c_str(),"rb"), *d=fopen(dst.c_str(),"wb");
                 if(s && d) { char b[4096]; size_t n; while((n=fread(b,1,4096,s))>0) fwrite(b,1,n,d); }
                 if(s) fclose(s); if(d) fclose(d);
                 EmuConfig.BaseFilenames.Bios = fd.FileName;
                 return;
             }
        }
    }
    if (FileSystem::FindFiles(bundleDir.c_str(), "*", FILESYSTEM_FIND_FILES, &bundleResults)) {
        for (const auto& fd : bundleResults) {
             if (fd.Size >= 1024*1024 && (fd.FileName.find(".bin") != std::string::npos || fd.FileName.find(".BIN") != std::string::npos)) {
                 Console.WriteLn("Found BIOS in Bundle: %s. Copying...", fd.FileName.c_str());
                 std::string srcPath = bundleDir + "/" + fd.FileName;
                 std::string destPath = biosDir + "/" + fd.FileName;
                 
                 struct stat st = {0};
                 if (stat(biosDir.c_str(), &st) == -1) mkdir(biosDir.c_str(), 0755);

                 FILE *src = fopen(srcPath.c_str(), "rb");
                 FILE *dst = fopen(destPath.c_str(), "wb");
                 if (src && dst) {
                     char buffer[4096];
                     size_t bytes;
                     while ((bytes = fread(buffer, 1, 4096, src)) > 0) fwrite(buffer, 1, bytes, dst);
                     fclose(src); fclose(dst);
                     EmuConfig.BaseFilenames.Bios = fd.FileName;
                     Console.WriteLn("Copy and set successful.");
                     return;
                 }
                 if(src) fclose(src);
                 if(dst) fclose(dst);
             }
        }
    }
    
    Console.Warning("No BIOS found automatically.");
    EmuConfig.BaseFilenames.Bios.clear();
}

#pragma mark - BIOS picker
- (void)showBiosPicker {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    UIDocumentPickerViewController *documentPicker = [[UIDocumentPickerViewController alloc] initWithDocumentTypes:@[@"public.data"] inMode:UIDocumentPickerModeImport];
#pragma clang diagnostic pop
    documentPicker.delegate = self;
    documentPicker.allowsMultipleSelection = NO;
    [self.window.rootViewController presentViewController:documentPicker animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    if (urls.count == 0) return;
    
    NSURL *url = urls.firstObject;
    Console.WriteLn("User picked file: %s", [[url path] UTF8String]);
    
    // Copy to Documents/bios
    std::string biosDir = EmuFolders::DataRoot + "/bios";
    struct stat st = {0};
    if (stat(biosDir.c_str(), &st) == -1) mkdir(biosDir.c_str(), 0755);
    
    NSString *destPath = [NSString stringWithFormat:@"%s/%@", biosDir.c_str(), [url lastPathComponent]];
    NSError *error = nil;
    
    // Remove if exists
    [[NSFileManager defaultManager] removeItemAtPath:destPath error:nil];
    
    if ([[NSFileManager defaultManager] copyItemAtURL:url toURL:[NSURL fileURLWithPath:destPath] error:&error]) {
        Console.WriteLn("Imported BIOS to: %s", [destPath UTF8String]);
        
        std::string fileName = [[destPath lastPathComponent] UTF8String];
        EmuConfig.BaseFilenames.Bios = fileName;
        
        // Hide button and start VM
        dispatch_async(dispatch_get_main_queue(), ^{
            self.startBiosButton.hidden = YES;
        });
        
#if TARGET_OS_SIMULATOR
        [self startVMThread];
#else
        [self checkJITAndStartVM];
#endif

    } else {
        Console.Error("Failed to import BIOS: %s", [[error localizedDescription] UTF8String]);
        Host::ReportErrorAsync("Import Failed", [[error localizedDescription] UTF8String]);
    }
}

// ---------------------------------------------------------------------------
// JIT keepalive timer (Component 2: idle-period grant validation)
// ---------------------------------------------------------------------------
// Fires every 12 seconds while the VM is idle, re-validating the JIT grant
// via DarwinMisc::ValidateJITAlive(). iOS can revoke CS_DEBUGGED ~30-60s after
// the app becomes inactive; this catches the revocation early and posts a
// notification so the UI can react before the next boot attempt.
// ---------------------------------------------------------------------------
static dispatch_source_t s_jitKeepaliveTimer = nil;
static std::atomic<bool> s_jitExpired{false};

// VM init watchdog (Component 4a) and re-boot thread-exit signal (Component 5).
// s_vmInitComplete is reset to false before CPUThreadInitialize and set to true
// once it returns; the watchdog thread polls it for 15 seconds.
// s_vmThreadShouldExit wakes the persistent boot loop so it can exit cleanly
// when revalidation decides to tear the old thread down and create a new one.
static std::atomic<bool> s_vmInitComplete{false};
static std::atomic<bool> s_vmThreadShouldExit{false};

static void ARMSX2StopJITKeepalive()
{
    if (s_jitKeepaliveTimer)
    {
        dispatch_source_cancel(s_jitKeepaliveTimer);
        s_jitKeepaliveTimer = nil;
        NSLog(@"@@JIT_KEEPALIVE@@ timer_stopped");
    }
}

static void ARMSX2StartJITKeepalive()
{
    if (s_jitKeepaliveTimer) return;
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0);
    s_jitKeepaliveTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    dispatch_source_set_timer(s_jitKeepaliveTimer,
                              dispatch_time(DISPATCH_TIME_NOW, 12 * NSEC_PER_SEC),
                              12 * NSEC_PER_SEC, 0);
    dispatch_source_set_event_handler(s_jitKeepaliveTimer, ^{
        // Check JIT on every interval, even during active gameplay.
        // iOS can revoke CS_DEBUGGED at any time — including mid-frame —
        // which flips protection on code and data pages and crashes the
        // CPU, GS, and MTVU threads simultaneously. The canary write is
        // cheap (one byte) and the csops check is a single syscall.
        if (!DarwinMisc::ValidateJITAlive())
        {
            s_jitExpired.store(true);
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSNotificationCenter defaultCenter]
                    postNotificationName:@"ARMSX2iOSJITExpired" object:nil];
            });
            ARMSX2StopJITKeepalive();
        }
    });
    dispatch_resume(s_jitKeepaliveTimer);
    NSLog(@"@@JIT_KEEPALIVE@@ timer_started interval=12s");
}

#pragma mark - JIT gate & VM launch
// JIT availability check for real devices. The allocation path can use MAP_JIT,
// dual-map, or legacy mprotect depending on what iOS/LiveContainer permits.
- (void)checkJITAndStartVM {
#if !TARGET_OS_SIMULATOR
    ARMSX2ApplyJITScriptProtocol("jit-gate");

    // Re-validate JIT even if it was available at launch. iOS can revoke
    // CS_DEBUGGED after ~30-60s of inactivity.
    const bool jitAlive = DarwinMisc::IsJITAvailable() && DarwinMisc::ValidateJITAlive();

    if (jitAlive) {
        std::fprintf(stderr, "@@BOOT_JIT_GATE@@ available=1 mode=jit_alloc\n");
        std::fflush(stderr);
        Console.WriteLn("@@JIT_GATE@@ JIT channel available; starting VM");
        DarwinMisc::iPSX2_FORCE_EE_INTERP = 0;
        // Restore recompiler settings if we previously forced interpreter.
        // Restore fastmem too (it was disabled during interpreter fallback).
        // The sticky vtlb_FastmemAreaUnavailable() check in the pre-VM-sync logic
        // will re-disable it if the 4GB reservation can't be made.
        s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", true);
        s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableIOP", true);
        s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", true);
        s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU1", true);
        s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", true);
        s_settings_interface->Save();
        [self startVMThread];
        return;
    }

    // JIT is dead. Fall back to interpreter instead of blocking boot.
    std::fprintf(stderr, "@@BOOT_JIT_GATE@@ available=0 fallback=interpreter\n");
    std::fflush(stderr);

    DarwinMisc::iPSX2_FORCE_EE_INTERP = 1;
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", false);
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableIOP", false);
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", false);
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU1", false);
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", false);
    s_settings_interface->Save();

    Host::AddIconOSDMessage("JITExpired", ICON_FA_TRIANGLE_EXCLAMATION,
        "JIT session expired — booting in interpreter mode (much slower). "
        "Relaunch the app to re-enable JIT.",
        15.0f);

    [self startVMThread];
#else
    [self startVMThread];
#endif
}

#pragma mark - Persistent VM thread
- (void)startVMThread {
    ARMSX2ApplyJITScriptProtocol("start-vm-thread");
    // Set inside the lock below when the JIT-dead re-boot path tears the old
    // thread down; consumed after the lock is released so the 200ms sleep does
    // NOT happen while holding s_vmMutex.
    bool needRebootAfterJITTeardown = false;
    {
        std::lock_guard<std::mutex> lk(s_vmMutex);
        if (s_vmThreadActive.load()) {
            std::fprintf(stderr, "@@BOOT_START_THREAD@@ active=1 action=ignored\n");
            std::fflush(stderr);
            Console.WriteLn("[VM] startVMThread: VM already active, ignoring");
            return;
        }

        // Signal the persistent thread to boot
        s_requestVMBoot.store(true);
        s_requestVMStop.store(false);

        if (s_vmThreadCreated) {
            // Re-validate JIT before signaling the existing thread.
            // The persistent thread bypasses CPUThreadInitialize, so it reuses
            // the JIT memory allocated at first boot. If iOS revoked the grant,
            // that memory is dead and the recompiler would write into a void.
            if (!DarwinMisc::iPSX2_FORCE_EE_INTERP && !DarwinMisc::ValidateJITAlive())
            {
                std::fprintf(stderr, "@@BOOT_JIT_GATE@@ revalidate=0 fallback=interpreter\n");
                std::fflush(stderr);
                DarwinMisc::iPSX2_FORCE_EE_INTERP = 1;
                s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", false);
                s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableIOP", false);
                s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", false);
                s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU1", false);
                s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", false);
                s_settings_interface->Save();

                Host::AddIconOSDMessage("JITExpired", ICON_FA_TRIANGLE_EXCLAMATION,
                    "JIT session expired — booting in interpreter mode (much slower). "
                    "Relaunch the app to re-enable JIT.",
                    15.0f);

                // Tell the old thread to exit, then fall through to create a new one.
                // We deliberately do NOT sleep here: std::condition_variable::wait
                // re-acquires the mutex before evaluating its predicate, so the old
                // thread cannot observe s_vmThreadShouldExit until this scope ends and
                // s_vmMutex is released. Sleeping under the lock would serialize the
                // 200ms and defeat the whole point of the grace period. Record the need
                // to sleep + respawn and perform it outside the lock below.
                s_vmThreadShouldExit.store(true);
                s_vmCV.notify_one(); // wake the old thread so it can check and exit
                needRebootAfterJITTeardown = true;
                // Fall out of this scope — do NOT return.
            }
            else
            {
                // JIT is alive — normal re-boot path
                std::fprintf(stderr, "@@BOOT_START_THREAD@@ active=0 created=1 action=signal\n");
                std::fflush(stderr);
                Console.WriteLn("[VM] startVMThread: signaling existing VM thread");
                s_vmCV.notify_one();
                return;
            }
        }
    }
    // <-- s_vmMutex is released here.

    if (needRebootAfterJITTeardown) {
        // Now that the lock is released, the old thread can wake, re-acquire
        // s_vmMutex, observe s_vmThreadShouldExit, clear it, and break out of
        // its wait loop. Give it a brief grace period to exit before we spawn a
        // replacement.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Ensure s_vmThreadCreated is set for the thread we're about to create.
    // First call: flips false->true. Re-boot after JIT teardown: it may already
    // be true (the old thread does not clear it on exit) -- that's fine; this
    // just lets the first-call and re-boot paths share the same tail.
    {
        std::lock_guard<std::mutex> lk(s_vmMutex);
        s_vmThreadCreated = true;
    }

    std::fprintf(stderr, "@@BOOT_START_THREAD@@ active=0 created=0 action=create\n");
    std::fflush(stderr);
    Console.WriteLn("[VM] Creating persistent VM thread...");

    std::thread vmThread([]() {
        // === ONE-TIME INIT (runs once per app lifetime) ===
        s_cpuThreadId = std::this_thread::get_id();
        std::fprintf(stderr, "@@BOOT_THREAD_INIT@@ begin=1\n");
        std::fflush(stderr);
        ARMSX2ConfigureImGuiFonts("vm-thread");
        Console.WriteLn("[VM] VM Thread: CPUThreadInitialize (once)...");
        // VM init watchdog (Component 4a): if CPUThreadInitialize hangs (e.g. a
        // Universal TXM prepare that never traps), surface an error and return
        // to the menu instead of hanging on a permanent black screen.
        s_vmInitComplete.store(false, std::memory_order_relaxed);
        std::thread watchdog([]() {
            for (int i = 0; i < 150; i++) // 15 seconds at 100ms intervals
            {
                if (s_vmInitComplete.load(std::memory_order_relaxed))
                    return;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (!s_vmInitComplete.load(std::memory_order_relaxed))
            {
                std::fprintf(stderr, "@@BOOT_FAIL@@ reason=vm_init_timeout stage=cpu_thread_initialize\n");
                std::fflush(stderr);
                dispatch_async(dispatch_get_main_queue(), ^{
                    Host::ReportErrorAsync("JIT Init Timeout",
                        "JIT memory setup took too long. This is a known issue with the Universal TXM "
                        "protocol on iOS 26. Try Settings → Emulator → JIT Script → Legacy, or relaunch "
                        "via StikDebug.");
                    [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2iOSReturnToMenu" object:nil];
                });
                std::lock_guard<std::mutex> lk(s_vmMutex);
                s_vmThreadCreated = false;
            }
        });
        watchdog.detach();
        const bool cpuInitOk = VMManager::Internal::CPUThreadInitialize();
        s_vmInitComplete.store(true, std::memory_order_relaxed);
        // NOTE (Issue 2, benign race): there is a TOCTOU window here. If the
        // watchdog fires between CPUThreadInitialize() completing and this point,
        // it will have already reset s_vmThreadCreated=false (and posted the
        // "JIT Init Timeout" error + ReturnToMenu notification) even though init
        // actually succeeded. We do not guard against this: the worst case is a
        // redundant error dialog the user dismisses, and the persistent thread
        // simply blocks on its wait predicate. Over-engineering a fix (e.g. a
        // second handshake) is not worth it. This is intentionally left as-is.
        if (!cpuInitOk) {
            std::fprintf(stderr, "@@BOOT_THREAD_INIT@@ ok=0\n");
            std::fflush(stderr);
            Console.Error("VM Thread: CPUThreadInitialize failed.");
            std::lock_guard<std::mutex> lk(s_vmMutex);
            s_vmThreadCreated = false;
            return;
        }
        std::fprintf(stderr, "@@BOOT_THREAD_INIT@@ ok=1\n");
        std::fflush(stderr);
        ARMSX2StartJITKeepalive(); // JIT acquired — start idle-period validation

        // === PERSISTENT BOOT LOOP ===
        bool auto_boot_first = (getenv("ARMSX2_AUTO_BOOT") && atoi(getenv("ARMSX2_AUTO_BOOT")) == 1)
                            || (getenv("ARMSX2_BOOT_ELF") != nullptr);
        while (true) {
            // Wait for boot signal (or auto-boot on first iteration)
            {
                std::unique_lock<std::mutex> lk(s_vmMutex);
                if (auto_boot_first) {
                    Console.WriteLn("[AutoBoot] @@AUTO_BOOT@@ skipping UI wait, auto-boot enabled");
                    auto_boot_first = false;
                } else {
                    std::fprintf(stderr, "@@BOOT_THREAD_WAIT@@ waiting=1\n");
                    std::fflush(stderr);
                    Console.WriteLn("[VM] VM Thread: waiting for boot request...");
                    s_vmCV.wait(lk, [] { return s_requestVMBoot.load() || s_vmThreadShouldExit.load(); });
                    if (s_vmThreadShouldExit.load(std::memory_order_relaxed))
                    {
                        s_vmThreadShouldExit.store(false, std::memory_order_relaxed);
                        std::fprintf(stderr, "@@BOOT_THREAD_EXIT@@ reason=should_exit\n");
                        std::fflush(stderr);
                        Console.WriteLn("[VM] VM Thread: exit requested, ending persistent loop.");
                        // The persistent-thread design assumes CPUThreadInitialize runs once.
                        // When we tear down to create a new thread (e.g. for interpreter fallback),
                        // we must pair the init with a shutdown so the new thread can re-allocate
                        // without duplicating the ~161MB SysMemory reservation.
                        VMManager::Internal::CPUThreadShutdown();
                        break; // exit the while(true) loop — thread ends
                    }
                }
                s_requestVMBoot.store(false);
            }

            std::fprintf(stderr, "@@BOOT_THREAD_SIGNAL@@ received=1\n");
            std::fflush(stderr);
            Console.WriteLn("[VM] VM Thread: boot signal received, preparing boot params...");
            s_vmThreadActive.store(true);
            const unsigned int heartbeat_generation =
                s_vmHeartbeatGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;

            // --- Build boot parameters from INI ---
            VMBootParameters boot_params;
            boot_params.fast_boot = false;
            {
                std::string isoDir = EmuFolders::DataRoot + "/iso";
                std::string defaultISO = "";
                std::string isoFilename = s_settings_interface->GetStringValue("GameISO", "BootISO", defaultISO.c_str());
                s_settings_interface->SetStringValue("GameISO", "BootISO", isoFilename.c_str());
                s_settings_interface->Save();
                std::string isoPath = (!isoFilename.empty() && isoFilename.front() == '/') ? isoFilename : (isoDir + "/" + isoFilename);
                // Fallback: check Documents/ root if not found in iso/.
                if (!isoFilename.empty() && isoFilename.front() != '/' && !FileSystem::FileExists(isoPath.c_str())) {
                    std::string rootPath = EmuFolders::DataRoot + "/" + isoFilename;
                    if (FileSystem::FileExists(rootPath.c_str())) {
                        isoPath = rootPath;
                        Console.WriteLn("ISO found in Documents/ root: %s", isoPath.c_str());
                    }
                }
                // Resolve fast boot from the per-game override if present, otherwise the
                // configured global value. Global settings are not mutated here.
                const bool fastBoot = ARMSX2ResolveFastBootForISO(isoPath);
                std::fprintf(stderr, "@@BOOT_FASTBOOT_READ@@ selected=%d\n", fastBoot ? 1 : 0);
                std::fflush(stderr);
                const bool isoExists = !isoFilename.empty() && FileSystem::FileExists(isoPath.c_str());
                std::fprintf(stderr, "@@BOOT_PARAMS@@ ini_iso=\"%s\" resolved=\"%s\" exists=%d fast_boot=%d\n",
                    isoFilename.c_str(), isoPath.c_str(), isoExists ? 1 : 0, fastBoot ? 1 : 0);
                std::fflush(stderr);
                if (isoExists) {
                    std::string suffix = isoFilename.size() >= 4 ? isoFilename.substr(isoFilename.size() - 4) : "";
                    std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);
                    bool isElf = (suffix == ".elf");
                    if (isElf) {
                        boot_params.elf_override = isoPath;
                        boot_params.source_type = CDVD_SourceType::NoDisc;
                        boot_params.fast_boot = true;
                        std::string discPath = VMManager::GetDiscOverrideFromGameSettings(isoPath);
                        if (!discPath.empty()) {
                            if (FileSystem::FileExists(discPath.c_str())) {
                                boot_params.filename = discPath;
                                boot_params.source_type = CDVD_SourceType::Iso;
                            } else {
                                Host::ReportErrorAsync("Linked disc not found",
                                    "This ELF has a linked disc, but the disc file is missing. Booting without it. Re-link it in the game's Disc Path menu.");
                            }
                        }
                        std::fprintf(stderr, "@@ISO_BOOT@@ path=%s fast_boot=1 mode=ELF INI=\"%s\"\n",
                            isoPath.c_str(), isoFilename.c_str());
                        std::fflush(stderr);
                        Console.WriteLn("@@ISO_BOOT@@ path=%s fast_boot=1 mode=ELF (INI: %s)", isoPath.c_str(), isoFilename.c_str());
                    } else {
                        boot_params.filename = isoPath;
                        boot_params.source_type = CDVD_SourceType::Iso;
                        boot_params.fast_boot = fastBoot;
                        std::fprintf(stderr, "@@ISO_BOOT@@ path=%s fast_boot=%d mode=ISO INI=\"%s\"\n",
                            isoPath.c_str(), fastBoot ? 1 : 0, isoFilename.c_str());
                        std::fflush(stderr);
                        Console.WriteLn("@@ISO_BOOT@@ path=%s fast_boot=%d (INI: %s)", isoPath.c_str(), fastBoot ? 1 : 0, isoFilename.c_str());
                    }
                } else {
                    std::fprintf(stderr, "@@ISO_BOOT_MISSING@@ ini_iso=\"%s\" attempted=\"%s\"\n",
                        isoFilename.c_str(), isoPath.c_str());
                    std::fflush(stderr);
                    Console.WriteLn("@@ISO_BOOT@@ no ISO='%s', falling back to BIOS only", isoFilename.c_str());
                }
            }

            if (getenv("ARMSX2_AUTO_BOOT_BIOS")) {
                Console.WriteLn("@@AUTO_BOOT_BIOS@@ enabled=1 action=triggered");
                boot_params.fast_boot = false;
            }
            // ps2autotests: boot ELF directly via env var
            if (const char* testElf = getenv("ARMSX2_BOOT_ELF")) {
                boot_params.elf_override = testElf;
                boot_params.source_type = CDVD_SourceType::NoDisc;
                boot_params.fast_boot = true;
                Console.WriteLn("@@BOOT_ELF@@ elf=%s", testElf);
            }

            // BIOS sanity check
            const std::string biosPath = Path::Combine(EmuFolders::Bios, EmuConfig.BaseFilenames.Bios);
            const bool biosExists = !EmuConfig.BaseFilenames.Bios.empty() && FileSystem::FileExists(biosPath.c_str());
            std::fprintf(stderr, "@@BOOT_BIOS_CHECK@@ bios=\"%s\" path=\"%s\" exists=%d\n",
                EmuConfig.BaseFilenames.Bios.c_str(), biosPath.c_str(), biosExists ? 1 : 0);
            std::fflush(stderr);
            if (!biosExists) {
                std::fprintf(stderr, "@@BOOT_BIOS_FAIL@@ action=abort_to_menu\n");
                std::fflush(stderr);
                Console.Error("CRITICAL: BIOS verification failed inside VM thread.");
                Host::ReportErrorAsync("BIOS Error", "Validation failed.");
                s_vmThreadActive.store(false);
                s_vmHeartbeatGeneration.fetch_add(1, std::memory_order_acq_rel);
                dispatch_async(dispatch_get_main_queue(), ^{
                    [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2iOSVMDidShutdown" object:nil];
                });
                continue; // back to wait loop
            }

            ARMSX2SanitizeFrameLimiterConfig("pre-vm-initialize");
            ARMSX2EnsureIOSSpeedhackDefaults(s_settings_interface, "pre-vm-initialize");
            ARMSX2RepairIOSARM64JITSettings(s_settings_interface, "pre-vm-initialize");
            ARMSX2MigrateJITScriptProtocolForIOS(s_settings_interface, "pre-vm-initialize");
            ARMSX2IOSSanitizeFolderSettings(s_settings_interface, EmuFolders::DataRoot, "pre-vm-initialize");
            VMManager::Internal::LoadStartupSettings();
            ARMSX2ApplyIOSOsdPresetFromConfig("pre-vm-initialize");
            EmuConfig.Speedhacks.vuThread =
                s_settings_interface->GetBoolValue("EmuCore/Speedhacks", "vuThread", EmuConfig.Speedhacks.vuThread);
            EmuConfig.Cpu.Recompiler.EnableFastmem =
                s_settings_interface->GetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", EmuConfig.Cpu.Recompiler.EnableFastmem);
            // Re-apply the sticky fastmem-area-unavailable disable. The INI read above
            // can re-enable fastmem (the default is true), but if the 4 GB reservation
            // failed at boot the recompiler would emit load/store against fastmem_base=0
            // and SIGSEGV on the first memory access.
            if (vtlb_FastmemAreaUnavailable() && EmuConfig.Cpu.Recompiler.EnableFastmem)
                EmuConfig.Cpu.Recompiler.EnableFastmem = false;
            std::fprintf(stderr, "@@IOS_PREVM_SYNC_SETTINGS@@ mtvu=%d fastmem=%d\n",
                EmuConfig.Speedhacks.vuThread ? 1 : 0,
                EmuConfig.Cpu.Recompiler.EnableFastmem ? 1 : 0);
            std::fflush(stderr);
            const int configuredCoreType = s_settings_interface->GetIntValue("EmuCore/CPU", "CoreType", 2);
            const bool configuredUseArm64 = s_settings_interface->GetBoolValue("EmuCore/CPU", "UseArm64Dynarec", configuredCoreType == 2);
            std::fprintf(stderr,
                "@@CPU_CONFIG@@ core=%d use_arm64=%d ee=%d iop=%d vu0=%d vu1=%d fastmem=%d mtvu=%d forced_interp=%d\n",
                configuredCoreType, configuredUseArm64 ? 1 : 0,
                EmuConfig.Cpu.Recompiler.EnableEE ? 1 : 0,
                EmuConfig.Cpu.Recompiler.EnableIOP ? 1 : 0,
                EmuConfig.Cpu.Recompiler.EnableVU0 ? 1 : 0,
                EmuConfig.Cpu.Recompiler.EnableVU1 ? 1 : 0,
                EmuConfig.Cpu.Recompiler.EnableFastmem ? 1 : 0,
                EmuConfig.Speedhacks.vuThread ? 1 : 0,
                DarwinMisc::iPSX2_FORCE_EE_INTERP ? 1 : 0);
            std::fflush(stderr);
            ARMSX2IOSLogMemoryCardConfig("pre-vm-initialize");
            Console.WriteLn("@@FRAMELIMIT@@ boot nominal=%.3f turbo=%.3f slomo=%.3f ntsc=%.3f pal=%.3f",
                EmuConfig.EmulationSpeed.NominalScalar,
                EmuConfig.EmulationSpeed.TurboScalar,
                EmuConfig.EmulationSpeed.SlomoScalar,
                EmuConfig.GS.FramerateNTSC,
                EmuConfig.GS.FrameratePAL);

            ARMSX2ApplyJITScriptProtocol("pre-vm-initialize");

            // --- Initialize & Execute VM ---
            // Keep the keepalive timer running during gameplay. iOS can
            // revoke CS_DEBUGGED mid-frame, and the timer detects it before
            // a protection fault crashes the CPU/GS/MTVU threads.
            Error bootError;
            const VMBootResult bootResult = VMManager::Initialize(boot_params, &bootError);
            const std::string bootErrorText = bootError.GetDescription();
            std::fprintf(stderr, "@@BOOT_VM_INIT@@ result=%d success=%d error=\"%s\"\n",
                static_cast<int>(bootResult), bootResult == VMBootResult::StartupSuccess ? 1 : 0,
                bootErrorText.c_str());
            std::fflush(stderr);
            if (bootResult == VMBootResult::StartupSuccess) {
                ARMSX2IOSLogMemoryCardConfig("post-vm-initialize");
                std::fprintf(stderr, "@@BOOT_POST_INIT@@ stage=before_osd state=%d frame=%u\n",
                    static_cast<int>(VMManager::GetState()), ::g_FrameCount);
                std::fflush(stderr);
                ARMSX2ApplyIOSOsdPresetFromConfig("post-vm-initialize");
                std::fprintf(stderr, "@@BOOT_POST_INIT@@ stage=after_osd state=%d frame=%u\n",
                    static_cast<int>(VMManager::GetState()), ::g_FrameCount);
                std::fflush(stderr);
                Console.WriteLn("[VM] VM initialized successfully");
                VMManager::SetState(VMState::Running);
                std::fprintf(stderr, "@@BOOT_POST_INIT@@ stage=after_set_running state=%d frame=%u\n",
                    static_cast<int>(VMManager::GetState()), ::g_FrameCount);
                std::fflush(stderr);

                if (ARMSX2IOSRuntimeTelemetryEnabled()) {
                    std::thread([heartbeat_generation]() {
                        for (int sec = 1; sec <= 180; sec++) {
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                            if (!s_vmThreadActive.load(std::memory_order_relaxed) ||
                                s_vmHeartbeatGeneration.load(std::memory_order_acquire) != heartbeat_generation)
                                break;
                            if (sec != 1 && (sec % 5) != 0)
                                continue;
                            std::fprintf(stderr,
                                "@@VM_HEARTBEAT@@ sec=%d state=%d frame=%u pm_frame=%llu fps=%.2f internal_fps=%.2f speed=%.2f cpu=%.2f vu=%.2f gs=%.2f gpu=%.2f ee_pc=0x%08x ee_cycle=%lld ee_next=%lld\n",
                                sec,
                                static_cast<int>(VMManager::GetState()),
                                ::g_FrameCount,
                                static_cast<unsigned long long>(PerformanceMetrics::GetFrameNumber()),
                                PerformanceMetrics::GetFPS(),
                                PerformanceMetrics::GetInternalFPS(),
                                PerformanceMetrics::GetSpeed(),
                                PerformanceMetrics::GetCPUThreadUsage(),
                                PerformanceMetrics::GetVUThreadUsage(),
                                PerformanceMetrics::GetGSThreadUsage(),
                                PerformanceMetrics::GetGPUUsage(),
                                cpuRegs.pc,
                                static_cast<long long>(cpuRegs.cycle),
                                static_cast<long long>(cpuRegs.nextEventCycle));
                        }
                    }).detach();
                }

                while (true) {
                    Host::PumpMessagesOnCPUThread();

                    if (s_requestVMStop.load()) {
                        Console.WriteLn("[VM] VM Thread: stop requested from UI.");
                        break;
                    }
                    VMState state = VMManager::GetState();
                    if (state == VMState::Stopping || state == VMState::Shutdown) {
                        Console.WriteLn("[VM] VM Thread: shutdown signal received.");
                        break;
                    } else if (state == VMState::Running) {
                        std::fprintf(stderr, "@@BOOT_EXEC_ENTER@@ state=%d frame=%u\n",
                            static_cast<int>(state), ::g_FrameCount);
                        std::fflush(stderr);
                        VMManager::Execute();
                    } else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }

                Console.WriteLn("[VM] VM Thread: shutting down VM...");
                VMManager::Shutdown(false);
            } else {
                Console.Error("VM Thread: VMManager::Initialize failed!");
                Host::ReportErrorAsync("Startup Error", "VM Initialization Failed.");
            }

            // --- Post-shutdown: reset state, notify UI ---
            s_vmThreadActive.store(false);
            ARMSX2StartJITKeepalive(); // VM stopped — JIT idle, restart monitoring
            s_vmHeartbeatGeneration.fetch_add(1, std::memory_order_acq_rel);
            s_requestVMStop.store(false);
            Console.WriteLn("[VM] VM Thread: shutdown complete, posting notification");
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2iOSVMDidShutdown" object:nil];
            });
        } // end while(true) boot loop

        // Note: CPUThreadShutdown() is never reached because the thread persists.
        // It would only be needed if we added an app-termination signal.
    });
    vmThread.detach();
}

#pragma mark - Scene lifecycle
- (void)sceneDidDisconnect:(UIScene *)scene {
}

- (void)sceneDidBecomeActive:(UIScene *)scene {
}

- (void)sceneWillResignActive:(UIScene *)scene {
// NVM save when app loses focus
    if (s_vmThreadActive.load(std::memory_order_relaxed) && !BiosPath.empty()) {
        extern void cdvdSaveNVRAM();
        cdvdSaveNVRAM();
        Console.WriteLn("[NVM] NVM saved on sceneWillResignActive");
    } else {
        Console.WriteLn("[NVM] Skipped save on sceneWillResignActive active=%d biosPath=%d",
            s_vmThreadActive.load(std::memory_order_relaxed) ? 1 : 0, BiosPath.empty() ? 0 : 1);
    }
}

- (void)sceneWillEnterForeground:(UIScene *)scene {
}

- (void)sceneDidEnterBackground:(UIScene *)scene {
// Save NVM + memory cards when app goes to background.
    // Without this, BIOS settings (language/date) are lost on every restart
    // because cdvdSaveNVRAM() is only called at VM shutdown, which never
    // happens when iOS terminates the app via SIGTERM.
    if (s_vmThreadActive.load(std::memory_order_relaxed) && !BiosPath.empty()) {
        extern void cdvdSaveNVRAM();
        cdvdSaveNVRAM();
        Console.WriteLn("[NVM] NVM saved on sceneDidEnterBackground");
    } else {
        Console.WriteLn("[NVM] Skipped save on sceneDidEnterBackground active=%d biosPath=%d",
            s_vmThreadActive.load(std::memory_order_relaxed) ? 1 : 0, BiosPath.empty() ? 0 : 1);
    }
}

@end
