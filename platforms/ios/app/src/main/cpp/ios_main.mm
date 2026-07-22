// Set SDL_MAIN_HANDLED to prevent SDL from redefining main()
#define SDL_MAIN_HANDLED


#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_metal.h>

// SwiftUI integration — Xcode names the generated header after the Swift module.
#if __has_include("ARMSX2iOS-Swift.h")
#import "ARMSX2iOS-Swift.h"
#define ARMSX2_HAS_SWIFTUI 1
#elif __has_include("ARMSX2-Swift.h")
#import "ARMSX2-Swift.h"
#define ARMSX2_HAS_SWIFTUI 1
#else
#define ARMSX2_HAS_SWIFTUI 0
#endif
#import <SwiftUI/SwiftUI.h>

// ... other includes ...
#include <unistd.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <chrono>
#include <cstdio>

#include "common/ProgressCallback.h"
#include "common/Error.h"
#include "pcsx2/Input/InputManager.h"
#include "pcsx2/SIO/Pad/Pad.h"
#include "pcsx2/SIO/Pad/PadDualshock2.h"
#include "pcsx2/Counters.h" // g_FrameCount
#include "pcsx2/PerformanceMetrics.h"
#include "pcsx2/R5900.h"
#include "pcsx2/Achievements.h"
#include "pcsx2/CDVD/CDVDdiscReader.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "pcsx2/VMManager.h"
#include "pcsx2/GameList.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/Config.h"
#include "pcsx2/CDVD/CDVD.h"
#include "pcsx2/CDVD/CDVDcommon.h"
#include "pcsx2/ps2/BiosTools.h"
#include "pcsx2/SIO/Memcard/MemoryCardFile.h"

#include "pcsx2/DEV9/pcap_io.h"
#include "pcsx2/DEV9/net.h"

#include "pcsx2/Host.h"
#include "pcsx2/Host/AudioStreamTypes.h"
#include "pcsx2/INISettingsInterface.h"

#include "common/WindowInfo.h"
#include "common/HTTPDownloader.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cmath>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <vector>
#include <sys/stat.h> // For mkdir

struct rc_client_event_t;
struct rc_client_t;

// iOS specific headers
#import <UIKit/UIKit.h>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>
#import <AVFoundation/AVFoundation.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include "common/Darwin/DarwinMisc.h"

#include "IOS/IOSRuntime.h"
#import "IOS/ARMSX2GameView.h"

// Global Log View
UITextView* g_logView = nil;

// Game render view — frame-based layout, portrait=50% / landscape=full safe area
// Game render view with CAMetalLayer as backing layer (like MTKView)
// Game render view — backing layer (CAMetalLayer), manual landscape toggle
#include "pcsx2/MTGS.h"
extern void GSResizeDisplayWindow(u32 width, u32 height, float scale);

#pragma mark - Init helpers
static std::vector<u8> s_imguiStandardFontData;

bool ARMSX2ShouldEnableMTVUByDefault(u32* physical_cores)
{
    u32 total_physical_cores = 0;
    for (const DarwinMisc::CPUClass& cpu_class : DarwinMisc::GetCPUClasses())
        total_physical_cores += cpu_class.num_physical;

    if (physical_cores)
        *physical_cores = total_physical_cores;

    return total_physical_cores >= 3;
}

void ARMSX2EnsureIOSSpeedhackDefaults(SettingsInterface* si, const char* reason)
{
    if (!si)
        return;

    bool changed = false;
    if (!si->ContainsValue("EmuCore/Speedhacks", "WaitLoop")) {
        si->SetBoolValue("EmuCore/Speedhacks", "WaitLoop", true);
        changed = true;
    }
    if (!si->ContainsValue("EmuCore/Speedhacks", "IntcStat")) {
        si->SetBoolValue("EmuCore/Speedhacks", "IntcStat", true);
        changed = true;
    }
    if (!si->ContainsValue("EmuCore/Speedhacks", "vuFlagHack")) {
        si->SetBoolValue("EmuCore/Speedhacks", "vuFlagHack", true);
        changed = true;
    }
    if (!si->ContainsValue("EmuCore/Speedhacks", "vu1Instant")) {
        si->SetBoolValue("EmuCore/Speedhacks", "vu1Instant", true);
        changed = true;
    }

    u32 physical_cores = 0;
    const bool default_mtvu = ARMSX2ShouldEnableMTVUByDefault(&physical_cores);
    const bool has_vu_thread = si->ContainsValue("EmuCore/Speedhacks", "vuThread");
    const bool has_legacy_mtvu = si->ContainsValue("EmuCore/Speedhacks", "MTVU");
    const bool migrated = si->GetBoolValue("ARMSX2iOS/Migrations", "SpeedhackDefaultsV2", false);

    if (!migrated) {
        const bool current_vu_thread = si->GetBoolValue("EmuCore/Speedhacks", "vuThread", default_mtvu);
        const bool legacy_false_default =
            has_legacy_mtvu && !si->GetBoolValue("EmuCore/Speedhacks", "MTVU", false) && !current_vu_thread;
        const bool mtvu_value = (!has_vu_thread || legacy_false_default) ? default_mtvu : current_vu_thread;
        si->SetBoolValue("EmuCore/Speedhacks", "vuThread", mtvu_value);
        si->SetBoolValue("ARMSX2iOS/Migrations", "SpeedhackDefaultsV2", true);
        std::fprintf(stderr,
            "@@MTVU_DEFAULT@@ reason=%s physical=%u default=%d had_vuThread=%d had_legacy=%d legacy_false=%d value=%d\n",
            reason, physical_cores, default_mtvu ? 1 : 0, has_vu_thread ? 1 : 0, has_legacy_mtvu ? 1 : 0,
            legacy_false_default ? 1 : 0, mtvu_value ? 1 : 0);
        changed = true;
    } else {
        const bool current_vu_thread = si->GetBoolValue("EmuCore/Speedhacks", "vuThread", default_mtvu);
        std::fprintf(stderr,
            "@@MTVU_DEFAULT@@ reason=%s physical=%u default=%d migrated=1 value=%d\n",
            reason, physical_cores, default_mtvu ? 1 : 0, current_vu_thread ? 1 : 0);
    }
    std::fflush(stderr);

    if (has_legacy_mtvu) {
        si->DeleteValue("EmuCore/Speedhacks", "MTVU");
        changed = true;
    }

    if (changed)
        si->Save();
}

bool ARMSX2RepairIOSARM64JITSettings(SettingsInterface* si, const char* reason)
{
    if (!si)
        return false;

    const int coreType = si->GetIntValue("EmuCore/CPU", "CoreType", 2);
    const bool useArm64 = si->GetBoolValue("EmuCore/CPU", "UseArm64Dynarec", coreType == 2);
    const bool enableEE = si->GetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", true);
    const bool enableIOP = si->GetBoolValue("EmuCore/CPU/Recompiler", "EnableIOP", true);
    const bool enableVU0 = si->GetBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", true);
    const bool enableVU1 = si->GetBoolValue("EmuCore/CPU/Recompiler", "EnableVU1", true);
    const bool enableFastmem = si->GetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", true);
    u32 physicalCores = 0;
    const bool defaultMTVU = ARMSX2ShouldEnableMTVUByDefault(&physicalCores);
    const bool manualFastmem = si->GetBoolValue("ARMSX2iOS/Speedhacks", "ManualFastmem", false);
    const bool manualMTVU = si->GetBoolValue("ARMSX2iOS/Speedhacks", "ManualMTVU", false);
    const int manualMTVUVersion = si->GetIntValue("ARMSX2iOS/Speedhacks", "ManualMTVUVersion", 0);
    const bool mtvu = si->GetBoolValue("EmuCore/Speedhacks", "vuThread", defaultMTVU);
    const bool staleManualMTVUOff = defaultMTVU && manualMTVU && !mtvu && manualMTVUVersion < 3;
#if TARGET_OS_SIMULATOR
    const bool jitAvailable = false;
#else
    const bool jitAvailable = DarwinMisc::IsJITAvailable();
#endif
    NSString* systemVersion = [[UIDevice currentDevice] systemVersion] ?: @"unknown";
    NSString* deviceModel = [[UIDevice currentDevice] model] ?: @"unknown";
    std::fprintf(stderr,
        "@@IOS_JIT_POLICY@@ reason=%s ios=\"%s\" device=\"%s\" jit_probe=%d core=%d use_arm64=%d ee=%d iop=%d vu0=%d vu1=%d fastmem=%d manual_fastmem=%d mtvu=%d manual_mtvu=%d manual_mtvu_version=%d stale_manual_mtvu=%d physical=%u\n",
        reason ? reason : "unknown", systemVersion.UTF8String, deviceModel.UTF8String, jitAvailable ? 1 : 0,
        coreType, useArm64 ? 1 : 0, enableEE ? 1 : 0, enableIOP ? 1 : 0,
        enableVU0 ? 1 : 0, enableVU1 ? 1 : 0, enableFastmem ? 1 : 0,
        manualFastmem ? 1 : 0, mtvu ? 1 : 0, manualMTVU ? 1 : 0, manualMTVUVersion,
        staleManualMTVUOff ? 1 : 0, physicalCores);
    std::fflush(stderr);

    if (!jitAvailable || coreType == 1)
        return false;

    bool changed = false;
    auto setBoolIfNeeded = [&](const char* section, const char* key, bool value) {
        if (si->GetBoolValue(section, key, !value) != value) {
            si->SetBoolValue(section, key, value);
            changed = true;
        }
    };
    auto setIntIfNeeded = [&](const char* section, const char* key, int value) {
        if (si->GetIntValue(section, key, value == 2 ? 0 : 2) != value) {
            si->SetIntValue(section, key, value);
            changed = true;
        }
    };

    setIntIfNeeded("EmuCore/CPU", "CoreType", 2);
    setBoolIfNeeded("EmuCore/CPU", "UseArm64Dynarec", true);
    setBoolIfNeeded("EmuCore/CPU/Recompiler", "EnableEE", true);
    setBoolIfNeeded("EmuCore/CPU/Recompiler", "EnableIOP", true);
    setBoolIfNeeded("EmuCore/CPU/Recompiler", "EnableVU0", true);
    setBoolIfNeeded("EmuCore/CPU/Recompiler", "EnableVU1", true);
    if (!manualFastmem)
        setBoolIfNeeded("EmuCore/CPU/Recompiler", "EnableFastmem", true);
    if (staleManualMTVUOff) {
        si->DeleteValue("ARMSX2iOS/Speedhacks", "ManualMTVU");
        si->DeleteValue("ARMSX2iOS/Speedhacks", "ManualMTVUVersion");
        changed = true;
        std::fprintf(stderr, "@@IOS_STALE_MTVU_REPAIR@@ reason=%s old_mtvu=0 manual_version=%d action=enable_default\n",
            reason ? reason : "unknown", manualMTVUVersion);
        std::fflush(stderr);
    }
    if (defaultMTVU && (!manualMTVU || staleManualMTVUOff))
        setBoolIfNeeded("EmuCore/Speedhacks", "vuThread", true);

    if (changed) {
        si->Save();
        std::fprintf(stderr,
            "@@CPU_DEFAULT_FIX@@ reason=%s old_core=%d old_arm64=%d old_ee=%d old_iop=%d old_vu0=%d old_vu1=%d old_fastmem=%d old_mtvu=%d new_core=2 new_arm64=1 fastmem=%d mtvu=%d\n",
            reason ? reason : "unknown", coreType, useArm64 ? 1 : 0, enableEE ? 1 : 0, enableIOP ? 1 : 0,
            enableVU0 ? 1 : 0, enableVU1 ? 1 : 0, enableFastmem ? 1 : 0, mtvu ? 1 : 0,
            si->GetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", true) ? 1 : 0,
            si->GetBoolValue("EmuCore/Speedhacks", "vuThread", defaultMTVU) ? 1 : 0);
        std::fflush(stderr);
    }
    return changed;
}

void ARMSX2ConfigureImGuiFonts(const char* reason)
{
    const std::string fontPath =
        EmuFolders::GetOverridableResourcePath("fonts" FS_OSPATH_SEPARATOR_STR "Roboto-Regular.ttf");
    std::optional<std::vector<u8>> fontData = FileSystem::ReadBinaryFile(fontPath.c_str());
    if (!fontData.has_value()) {
        std::fprintf(stderr, "@@IMGUI_FONT@@ ok=0 reason=%s path=\"%s\"\n", reason, fontPath.c_str());
        std::fflush(stderr);
        ImGuiManager::SetFonts({});
        return;
    }

    s_imguiStandardFontData = std::move(fontData.value());
    std::vector<ImGuiManager::FontInfo> fonts;
    fonts.push_back({
        std::span<const u8>(s_imguiStandardFontData.data(), s_imguiStandardFontData.size()),
        std::span<const u32>(),
        nullptr,
        false,
    });
    ImGuiManager::SetFonts(std::move(fonts));

    std::fprintf(stderr, "@@IMGUI_FONT@@ ok=1 reason=%s path=\"%s\" size=%zu\n",
        reason, fontPath.c_str(), s_imguiStandardFontData.size());
    std::fflush(stderr);
}

#pragma mark - ARMSX2GameView
@implementation ARMSX2GameView
+ (Class)layerClass { return [CAMetalLayer class]; }
- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self)
        [self armsx2ApplyNativeContentScale];
    return self;
}
- (void)didMoveToWindow {
    [super didMoveToWindow];
    [self armsx2ApplyNativeContentScale];
    [self setNeedsLayout];
}
- (CGFloat)armsx2NativeContentScale {
    UIScreen* screen = self.window.screen ?: UIScreen.mainScreen;
    CGFloat scale = screen.nativeScale > 0.0 ? screen.nativeScale : screen.scale;
    return scale > 0.0 ? scale : 1.0;
}
- (void)armsx2ApplyNativeContentScale {
    const CGFloat scale = [self armsx2NativeContentScale];
    self.contentScaleFactor = scale;
    self.layer.contentsScale = scale;
    ((CAMetalLayer*)self.layer).contentsScale = scale;
}
- (void)layoutSubviews {
    [super layoutSubviews];
    [self armsx2ApplyNativeContentScale];
    if (self.bounds.size.width <= 0.0 || self.bounds.size.height <= 0.0)
        return;

    CGFloat scale = self.contentScaleFactor;
    CAMetalLayer *mtl = (CAMetalLayer *)self.layer;
    mtl.drawableSize = CGSizeMake(self.bounds.size.width * scale,
                                   self.bounds.size.height * scale);
    int w = std::max(1, (int)(self.bounds.size.width * scale + 0.5));
    int h = std::max(1, (int)(self.bounds.size.height * scale + 0.5));
    float s = (float)scale;

    // Indent corner-anchored OSD by a small fixed clearance so it isn't clipped by the display's rounded corners
    constexpr double kOsdCornerInsetPt = 18.0;
    const float osd_inset = (float)(kOsdCornerInsetPt * scale);
    MTGS::RunOnGSThread([w, h, s, osd_inset]() {
        GSResizeDisplayWindow(w, h, s);
        ImGuiManager::SetOSDSafeAreaInsets(osd_inset, osd_inset, osd_inset, osd_inset);
    });
}
@end

#pragma mark - Statics & device stats
ARMSX2GameView* g_gameRenderView = nil;  // non-static: accessed from ARMSX2Bridge.mm
INISettingsInterface* s_settings_interface = nullptr;
INISettingsInterface* s_secrets_settings_interface = nullptr;

bool ARMSX2GetConfiguredFastBoot()
{
    if (!s_settings_interface)
        return false;

    return s_settings_interface->GetBoolValue(
        "GameISO", "FastBoot",
        s_settings_interface->GetBoolValue("EmuCore", "EnableFastBoot", false));
}

// Resolves fast boot for an ISO that is about to boot. A per-game override
// (EmuCore/EnableFastBoot in the game's settings INI) takes precedence;
// otherwise the configured global value is used. Global settings are never
// mutated, so a per-game override cannot leak into the global configuration.
bool ARMSX2ResolveFastBootForISO(const std::string& isoPath)
{
    const bool globalFastBoot = ARMSX2GetConfiguredFastBoot();
    if (isoPath.empty())
        return globalFastBoot;

    GameList::Entry entry;
    if (!GameList::PopulateEntryFromPath(isoPath, &entry) || entry.crc == 0)
        return globalFastBoot;

    const std::string serial = (entry.type == GameList::EntryType::ELF) ? std::string() : entry.serial;
    INISettingsInterface si(VMManager::GetGameSettingsPath(serial, entry.crc));
    if (!si.Load())
        return globalFastBoot;

    if (si.ContainsValue("EmuCore", "EnableFastBoot"))
        return si.GetBoolValue("EmuCore", "EnableFastBoot", globalFastBoot);

    return globalFastBoot;
}

int ARMSX2GetIOSMajorVersion()
{
    NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
    return static_cast<int>(version.majorVersion);
}

const char* ARMSX2DefaultJITScriptProtocol()
{
    return ARMSX2GetIOSMajorVersion() >= 26 ? "universal" : "legacy";
}

std::string ARMSX2NormalizeJITScriptProtocol(std::string jitProtocol)
{
    std::transform(jitProtocol.begin(), jitProtocol.end(), jitProtocol.begin(), ::tolower);
    if (jitProtocol == "utm-dolphin" || jitProtocol == "utm_dolphin")
        return "legacy";
    if (jitProtocol == "legacy" || jitProtocol == "universal")
        return jitProtocol;
    return {};
}

void ARMSX2MigrateJITScriptProtocolForIOS(SettingsInterface* si, const char* reason)
{
    if (!si)
        return;

    const int iosMajor = ARMSX2GetIOSMajorVersion();
    const char* defaultProtocol = ARMSX2DefaultJITScriptProtocol();
    const bool hadProtocol = si->ContainsValue("ARMSX2iOS/JIT", "ScriptProtocol");
    const bool migrated = si->GetBoolValue("ARMSX2iOS/Migrations", "JITScriptProtocolByOSV1", false);
    const std::string currentProtocol = ARMSX2NormalizeJITScriptProtocol(
        si->GetStringValue("ARMSX2iOS/JIT", "ScriptProtocol", defaultProtocol));

    if (!hadProtocol || (!migrated && iosMajor > 0 && iosMajor < 26 && currentProtocol == "universal")) {
        si->SetStringValue("ARMSX2iOS/JIT", "ScriptProtocol", defaultProtocol);
        si->SetBoolValue("ARMSX2iOS/Migrations", "JITScriptProtocolByOSV1", true);
        si->Save();
        std::fprintf(stderr,
            "@@JIT_PROTOCOL_MIGRATE@@ reason=%s ios_major=%d had=%d from=%s to=%s\n",
            reason ? reason : "unknown", iosMajor, hadProtocol ? 1 : 0, currentProtocol.c_str(), defaultProtocol);
        std::fflush(stderr);
        return;
    }

    if (!migrated) {
        si->SetBoolValue("ARMSX2iOS/Migrations", "JITScriptProtocolByOSV1", true);
        si->Save();
    }
}

std::string ARMSX2ResolveJITScriptProtocol()
{
    std::string jitProtocol = ARMSX2DefaultJITScriptProtocol();
    if (s_settings_interface)
    {
        jitProtocol = ARMSX2NormalizeJITScriptProtocol(
            s_settings_interface->GetStringValue("ARMSX2iOS/JIT", "ScriptProtocol", ARMSX2DefaultJITScriptProtocol()));
        if (jitProtocol != "legacy" && jitProtocol != "universal")
            jitProtocol = s_settings_interface->GetBoolValue(
                "ARMSX2iOS/JIT", "UseUniversalJITScript", ARMSX2GetIOSMajorVersion() >= 26) ? "universal" : "legacy";
    }
    return jitProtocol;
}

void ARMSX2ApplyJITScriptProtocol(const char* reason)
{
    const std::string jitProtocol = ARMSX2ResolveJITScriptProtocol();
    setenv("ARMSX2_JIT_PROTOCOL", jitProtocol.c_str(), 1);
    std::fprintf(stderr, "@@JIT_PROTOCOL_SELECTED@@ reason=%s ios_major=%d protocol=%s\n",
        reason ? reason : "unknown", ARMSX2GetIOSMajorVersion(), jitProtocol.c_str());
    std::fflush(stderr);
}

bool ARMSX2IOSRuntimeTelemetryEnabled()
{
    static std::atomic<int> s_enabled{-1};
    int enabled = s_enabled.load(std::memory_order_acquire);
    if (enabled >= 0)
        return enabled == 1;

    const char* value = std::getenv("iPSX2_IOS_PERF_TELEMETRY");
    if (!value || !value[0])
        value = std::getenv("SIMCTL_CHILD_iPSX2_IOS_PERF_TELEMETRY");

    enabled = (value && std::strcmp(value, "1") == 0) ? 1 : 0;
    int expected = -1;
    if (!s_enabled.compare_exchange_strong(expected, enabled, std::memory_order_acq_rel))
        enabled = expected;
    return enabled == 1;
}

double ARMSX2IOSGetAppRAMGB()
{
    task_vm_info_data_t vm_info = {};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&vm_info), &count) != KERN_SUCCESS)
        return 0.0;

    return static_cast<double>(vm_info.phys_footprint) / (1024.0 * 1024.0 * 1024.0);
}

const char* ARMSX2IOSHeatStateName(NSProcessInfoThermalState state)
{
    switch (state) {
    case NSProcessInfoThermalStateNominal:
        return "OK";
    case NSProcessInfoThermalStateFair:
        return "Warm";
    case NSProcessInfoThermalStateSerious:
        return "Hot";
    case NSProcessInfoThermalStateCritical:
        return "Critical";
    default:
        return "Unknown";
    }
}

void ARMSX2DisableRetroAchievementsHardcoreForIOS(SettingsInterface* si, const char* reason)
{
    if (!si)
        return;

    const bool was_hardcore = si->GetBoolValue("Achievements", "ChallengeMode", false);
    si->SetBoolValue("Achievements", "ChallengeMode", false);

    if (was_hardcore) {
        Console.Warning("@@RA_IOS_HARDCORE_DISABLED@@ reason=%s",
            reason ? reason : "unknown");
    }
}

void ARMSX2IOSApplyRetroAchievementsOverlayDefaults(SettingsInterface* si, const char* reason)
{
    if (!si)
        return;

    // iOS overlays share the screen with touch controls and the perf OSD. Keep
    // gameplay trackers left, but show achievement notifications at top-center.
    si->SetBoolValue("Achievements", "Overlays", true);
    si->SetBoolValue("Achievements", "LBOverlays", true);
    si->SetBoolValue("Achievements", "Notifications", true);
    si->SetBoolValue("Achievements", "LeaderboardNotifications", true);
    si->SetIntValue("Achievements", "OverlayPosition", static_cast<int>(AchievementOverlayPosition::TopLeft));
    si->SetIntValue("Achievements", "NotificationPosition", static_cast<int>(OsdOverlayPos::TopCenter));

    EmuConfig.Achievements.Overlays = true;
    EmuConfig.Achievements.LBOverlays = true;
    EmuConfig.Achievements.Notifications = true;
    EmuConfig.Achievements.LeaderboardNotifications = true;
    EmuConfig.Achievements.OverlayPosition = AchievementOverlayPosition::TopLeft;
    EmuConfig.Achievements.NotificationPosition = OsdOverlayPos::TopCenter;

    Console.WriteLn("iOS RetroAchievements overlay defaults applied (reason: %s)",
        reason ? reason : "unknown");
}

bool ARMSX2IOSPathStartsWith(const std::string& value, const std::string& prefix)
{
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool ARMSX2IOSPathIsInsideRoot(const std::string& path, const std::string& root)
{
    return path == root || ARMSX2IOSPathStartsWith(path, root + "/");
}

bool ARMSX2IOSPathContainsContainerFragment(const std::string& path)
{
    return path.find("Data/Application/") != std::string::npos ||
           path.find("/Containers/Data/Application/") != std::string::npos ||
           path.find("/var/mobile/Containers/Data/Application/") != std::string::npos ||
           path.find("/private/var/mobile/Containers/Data/Application/") != std::string::npos;
}

std::string ARMSX2IOSResolveFolderPath(const std::string& root, const std::string& value)
{
    return Path::IsAbsolute(value) ? value : Path::Combine(root, value);
}

NSInteger ARMSX2IOSCopyDirectoryContentsIfPresent(const std::string& source, const std::string& destination)
{
    if (source.empty() || destination.empty() || source == destination)
        return 0;

    @autoreleasepool {
        NSFileManager* fm = [NSFileManager defaultManager];
        NSString* sourcePath = [NSString stringWithUTF8String:source.c_str()];
        NSString* destinationPath = [NSString stringWithUTF8String:destination.c_str()];
        if (!sourcePath || !destinationPath)
            return 0;

        BOOL sourceIsDirectory = NO;
        if (![fm fileExistsAtPath:sourcePath isDirectory:&sourceIsDirectory] || !sourceIsDirectory)
            return 0;

        NSError* createError = nil;
        [fm createDirectoryAtPath:destinationPath withIntermediateDirectories:YES attributes:nil error:&createError];
        if (createError)
            NSLog(@"[ARMSX2 iOS FolderSanitize] destination create failed: %@ -> %@ error=%@",
                  sourcePath, destinationPath, createError.localizedDescription ?: @"unknown");

        NSInteger copied = 0;
        NSDirectoryEnumerator<NSString*>* enumerator = [fm enumeratorAtPath:sourcePath];
        for (NSString* relativePath in enumerator) {
            NSString* itemSource = [sourcePath stringByAppendingPathComponent:relativePath];
            NSString* itemDestination = [destinationPath stringByAppendingPathComponent:relativePath];

            BOOL itemIsDirectory = NO;
            if (![fm fileExistsAtPath:itemSource isDirectory:&itemIsDirectory])
                continue;

            if (itemIsDirectory) {
                [fm createDirectoryAtPath:itemDestination withIntermediateDirectories:YES attributes:nil error:nil];
                continue;
            }

            if ([fm fileExistsAtPath:itemDestination])
                continue;

            [fm createDirectoryAtPath:itemDestination.stringByDeletingLastPathComponent
          withIntermediateDirectories:YES
                           attributes:nil
                                error:nil];

            NSError* copyError = nil;
            if ([fm copyItemAtPath:itemSource toPath:itemDestination error:&copyError]) {
                copied++;
            } else {
                NSLog(@"[ARMSX2 iOS FolderSanitize] copy failed: %@ -> %@ error=%@",
                      itemSource, itemDestination, copyError.localizedDescription ?: @"unknown");
            }
        }

        return copied;
    }
}

void ARMSX2IOSSanitizeFolderSettings(SettingsInterface* si, const std::string& dataRoot, const char* reason)
{
    if (!si)
        return;

    struct FolderDefault
    {
        const char* key;
        const char* value;
        bool settingsRoot;
    };

    static constexpr FolderDefault defaults[] = {
        {"Bios", "bios", false},
        {"Snapshots", "snaps", false},
        {"Savestates", "sstates", false},
        {"MemoryCards", "memcards", false},
        {"Logs", "logs", false},
        {"Cheats", "cheats", false},
        {"Patches", "patches", false},
        {"Covers", "covers", false},
        {"GameSettings", "gamesettings", false},
        {"UserResources", "resources", false},
        {"Cache", "cache", false},
        {"Textures", "textures", false},
        {"InputProfiles", "inputprofiles", false},
        {"Videos", "videos", false},
        {"DebuggerLayouts", "debuggerlayouts", true},
        {"DebuggerSettings", "debuggersettings", true},
    };

    const std::string settingsRoot = Path::Combine(dataRoot, "inis");
    bool changed = false;

    for (const FolderDefault& entry : defaults) {
        std::string rawValue;
        if (!si->GetStringValue("Folders", entry.key, &rawValue) || rawValue.empty())
            continue;

        const std::string& root = entry.settingsRoot ? settingsRoot : dataRoot;
        const std::string resolved = ARMSX2IOSResolveFolderPath(root, rawValue);
        const bool rawAbsolute = Path::IsAbsolute(rawValue);
        const bool containerPath = ARMSX2IOSPathContainsContainerFragment(rawValue);
        const bool stale = (!rawAbsolute && containerPath) ||
            (rawAbsolute && containerPath && !ARMSX2IOSPathIsInsideRoot(resolved, root));

        if (!stale)
            continue;

        const std::string migratedPath = Path::Combine(root, entry.value);
        const NSInteger copied = ARMSX2IOSCopyDirectoryContentsIfPresent(resolved, migratedPath);
        si->SetStringValue("Folders", entry.key, entry.value);
        changed = true;

        std::fprintf(stderr,
            "@@IOS_FOLDER_SANITIZE@@ reason=%s key=%s old=\"%s\" resolved=\"%s\" default=\"%s\" copied=%ld\n",
            reason ? reason : "unknown", entry.key, rawValue.c_str(), resolved.c_str(),
            entry.value, static_cast<long>(copied));
        std::fflush(stderr);
    }

    if (changed)
        si->Save();
}

void ARMSX2IOSLogMemoryCardConfig(const char* reason)
{
    std::fprintf(stderr, "@@IOS_MEMCARD_DIR@@ reason=%s path=\"%s\"\n",
        reason ? reason : "unknown", EmuFolders::MemoryCards.c_str());

    constexpr size_t numMemoryCardSlots = sizeof(EmuConfig.Mcd) / sizeof(EmuConfig.Mcd[0]);
    for (size_t slot = 0; slot < numMemoryCardSlots; slot++) {
        const auto& card = EmuConfig.Mcd[slot];
        const std::string path = card.Filename.empty() ? std::string() : EmuConfig.FullpathToMcd(static_cast<uint>(slot));
        const bool existsFile = !path.empty() && FileSystem::FileExists(path.c_str());
        const bool existsDirectory = !path.empty() && FileSystem::DirectoryExists(path.c_str());
        const s64 size = existsFile ? FileSystem::GetPathFileSize(path.c_str()) : -1;
        const bool formatted = existsFile ? FileMcd_IsMemoryCardFormatted(path) : false;

        std::fprintf(stderr,
            "@@IOS_MEMCARD_SLOT@@ reason=%s slot=%zu enabled=%d type=%d name=\"%s\" path=\"%s\" file=%d dir=%d size=%lld formatted=%d hardcore_pref=%d hardcore_active=%d\n",
            reason ? reason : "unknown",
            slot + 1,
            card.Enabled ? 1 : 0,
            static_cast<int>(card.Type),
            card.Filename.c_str(),
            path.c_str(),
            existsFile ? 1 : 0,
            existsDirectory ? 1 : 0,
            static_cast<long long>(size),
            formatted ? 1 : 0,
            EmuConfig.Achievements.HardcoreMode ? 1 : 0,
            Achievements::IsHardcoreModeActive() ? 1 : 0);
    }

    std::fflush(stderr);
}

struct ARMSX2IOSDeviceStatsCache
{
    bool show = true;
    int severity = 0;
    std::string line;
    std::chrono::steady_clock::time_point last_update;
};

static std::mutex s_device_stats_mutex;
static ARMSX2IOSDeviceStatsCache s_device_stats_cache;

static const ARMSX2IOSDeviceStatsCache& ARMSX2IOSRefreshDeviceStatsCacheLocked()
{
    const auto now = std::chrono::steady_clock::now();
    if (!s_device_stats_cache.line.empty() &&
        (now - s_device_stats_cache.last_update) < std::chrono::seconds(1))
    {
        return s_device_stats_cache;
    }

    s_device_stats_cache.show = s_settings_interface ?
        s_settings_interface->GetBoolValue("ARMSX2iOS/UI", "OsdShowDeviceStats", true) : true;

    @autoreleasepool {
        UIDevice* device = [UIDevice currentDevice];
        const float battery = [device batteryLevel];
        const int battery_percent = (battery >= 0.0f) ? static_cast<int>(std::round(battery * 100.0f)) : -1;
        const NSProcessInfoThermalState thermal_state = [[NSProcessInfo processInfo] thermalState];
        const double app_ram_gb = ARMSX2IOSGetAppRAMGB();
        const bool low_power = [[NSProcessInfo processInfo] isLowPowerModeEnabled];

        if (thermal_state >= NSProcessInfoThermalStateSerious || (battery >= 0.0f && battery <= 0.15f))
            s_device_stats_cache.severity = 2;
        else if (thermal_state == NSProcessInfoThermalStateFair || (battery >= 0.0f && battery <= 0.30f))
            s_device_stats_cache.severity = 1;
        else
            s_device_stats_cache.severity = 0;

        char buffer[192];
        if (battery_percent >= 0) {
            std::snprintf(buffer, sizeof(buffer), "Battery: %d%% | Heat: %s | RAM: %.1f GB%s",
                battery_percent, ARMSX2IOSHeatStateName(thermal_state), app_ram_gb, low_power ? " | Low Power" : "");
        } else {
            std::snprintf(buffer, sizeof(buffer), "Battery: -- | Heat: %s | RAM: %.1f GB%s",
                ARMSX2IOSHeatStateName(thermal_state), app_ram_gb, low_power ? " | Low Power" : "");
        }

        s_device_stats_cache.line = buffer;
    }

    s_device_stats_cache.last_update = now;
    return s_device_stats_cache;
}

extern "C" bool ARMSX2_iOSShouldShowDeviceStatsOverlay()
{
    std::lock_guard<std::mutex> lock(s_device_stats_mutex);
    return ARMSX2IOSRefreshDeviceStatsCacheLocked().show;
}

extern "C" int ARMSX2_iOSGetDeviceStatsOverlaySeverity()
{
    std::lock_guard<std::mutex> lock(s_device_stats_mutex);
    return ARMSX2IOSRefreshDeviceStatsCacheLocked().severity;
}

extern "C" const char* ARMSX2_iOSGetDeviceStatsOverlayLine()
{
    std::lock_guard<std::mutex> lock(s_device_stats_mutex);
    return ARMSX2IOSRefreshDeviceStatsCacheLocked().line.c_str();
}

// Structured device stats for the SwiftUI VoiceOver HUD mirror. Reads the same
// cache as ARMSX2_iOSGetDeviceStatsOverlayLine but exposes individual fields.
// TODO: FPS - the overlay FPS lives in the PerformanceOverlay path with no
// thread-safe getter surfaced here yet; omitted for now.
extern "C" void ARMSX2_iOSCopyDeviceStats(int* outBatteryPercent, int* outThermalState,
                                          double* outRamGB, bool* outLowPower)
{
    std::lock_guard<std::mutex> lock(s_device_stats_mutex);
    const auto& stats = ARMSX2IOSRefreshDeviceStatsCacheLocked();
    if (outBatteryPercent) {
        UIDevice* device = [UIDevice currentDevice];
        const float battery = [device batteryLevel];
        *outBatteryPercent = (battery >= 0.0f) ? static_cast<int>(std::round(battery * 100.0f)) : -1;
    }
    if (outThermalState) *outThermalState = stats.severity;
    if (outRamGB) *outRamGB = ARMSX2IOSGetAppRAMGB();
    if (outLowPower) *outLowPower = [[NSProcessInfo processInfo] isLowPowerModeEnabled];
}

float ARMSX2SanitizedNominalScalar(float scalar)
{
    if (!std::isfinite(scalar))
        return 1.0f;

    return std::clamp(scalar, 0.05f, 10.0f);
}

void ARMSX2SanitizeFrameLimiterConfig(const char* reason)
{
    if (!s_settings_interface)
        return;

    const float raw = s_settings_interface->GetFloatValue("Framerate", "NominalScalar", 1.0f);
    const float sanitized = ARMSX2SanitizedNominalScalar(raw);
    if (std::fabs(raw - sanitized) > 0.001f) {
        Console.Warning("@@FRAMELIMIT@@ clamping unsupported NominalScalar %.3f -> %.3f reason=%s",
            raw, sanitized, reason ? reason : "unknown");
        s_settings_interface->SetFloatValue("Framerate", "NominalScalar", sanitized);
        s_settings_interface->Save();
    }

    EmuConfig.EmulationSpeed.NominalScalar = sanitized;
}

void ARMSX2SetIOSOsdFlags(bool show_fps, bool show_vps, bool show_speed, bool show_cpu,
    bool show_gpu, bool show_resolution, bool show_gs_stats, bool show_indicators,
    bool show_settings, bool show_inputs, bool show_frame_times, bool show_version,
    bool show_hardware_info)
{
    EmuConfig.GS.OsdShowFPS = show_fps;
    GSConfig.OsdShowFPS = show_fps;
    EmuConfig.GS.OsdShowVPS = show_vps;
    GSConfig.OsdShowVPS = show_vps;
    EmuConfig.GS.OsdShowSpeed = show_speed;
    GSConfig.OsdShowSpeed = show_speed;
    EmuConfig.GS.OsdShowCPU = show_cpu;
    GSConfig.OsdShowCPU = show_cpu;
    EmuConfig.GS.OsdShowGPU = show_gpu;
    GSConfig.OsdShowGPU = show_gpu;
    EmuConfig.GS.OsdShowResolution = show_resolution;
    GSConfig.OsdShowResolution = show_resolution;
    EmuConfig.GS.OsdShowGSStats = show_gs_stats;
    GSConfig.OsdShowGSStats = show_gs_stats;
    EmuConfig.GS.OsdShowIndicators = show_indicators;
    GSConfig.OsdShowIndicators = show_indicators;
    EmuConfig.GS.OsdShowSettings = show_settings;
    GSConfig.OsdShowSettings = show_settings;
    EmuConfig.GS.OsdShowInputs = show_inputs;
    GSConfig.OsdShowInputs = show_inputs;
    EmuConfig.GS.OsdShowFrameTimes = show_frame_times;
    GSConfig.OsdShowFrameTimes = show_frame_times;
    EmuConfig.GS.OsdShowVersion = show_version;
    GSConfig.OsdShowVersion = show_version;
    EmuConfig.GS.OsdShowHardwareInfo = show_hardware_info;
    GSConfig.OsdShowHardwareInfo = show_hardware_info;
    EmuConfig.GS.OsdShowVideoCapture = false;
    GSConfig.OsdShowVideoCapture = false;
    EmuConfig.GS.OsdShowInputRec = false;
    GSConfig.OsdShowInputRec = false;
}

void ARMSX2WriteIOSOsdFlagsToSettings()
{
    if (!s_settings_interface)
        return;

    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowFPS", EmuConfig.GS.OsdShowFPS);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowVPS", EmuConfig.GS.OsdShowVPS);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowSpeed", EmuConfig.GS.OsdShowSpeed);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowCPU", EmuConfig.GS.OsdShowCPU);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowGPU", EmuConfig.GS.OsdShowGPU);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowResolution", EmuConfig.GS.OsdShowResolution);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowGSStats", EmuConfig.GS.OsdShowGSStats);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowIndicators", EmuConfig.GS.OsdShowIndicators);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowSettings", EmuConfig.GS.OsdShowSettings);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowInputs", EmuConfig.GS.OsdShowInputs);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowFrameTimes", EmuConfig.GS.OsdShowFrameTimes);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowVersion", EmuConfig.GS.OsdShowVersion);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowHardwareInfo", EmuConfig.GS.OsdShowHardwareInfo);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowVideoCapture", false);
    s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowInputRec", false);
}

void ARMSX2ApplyIOSOsdPresetFromConfig(const char* reason)
{
    if (!s_settings_interface)
        return;

    ARMSX2SetIOSOsdFlags(
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowFPS", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowVPS", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowSpeed", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowCPU", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowGPU", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowResolution", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowGSStats", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowIndicators", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowSettings", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowInputs", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowFrameTimes", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowVersion", false),
        s_settings_interface->GetBoolValue("EmuCore/GS", "OsdShowHardwareInfo", false));

    int position = s_settings_interface->GetIntValue("EmuCore/GS", "OsdPerformancePos", static_cast<int>(OsdOverlayPos::TopRight));
    if (position == static_cast<int>(OsdOverlayPos::TopCenter))
        position = static_cast<int>(OsdOverlayPos::TopRight);
    EmuConfig.GS.OsdPerformancePos = static_cast<OsdOverlayPos>(position);
    GSConfig.OsdPerformancePos = static_cast<OsdOverlayPos>(position);

    Console.WriteLn("@@OSD@@ preset=%d position=%d reason=%s fps=%d vps=%d speed=%d gpu=%d device_stats=%d frame_times=%d version=%d hardware=%d",
        s_settings_interface->GetIntValue("ARMSX2iOS/UI", "OsdPreset", 0), position, reason ? reason : "unknown",
        EmuConfig.GS.OsdShowFPS ? 1 : 0,
        EmuConfig.GS.OsdShowVPS ? 1 : 0,
        EmuConfig.GS.OsdShowSpeed ? 1 : 0,
        EmuConfig.GS.OsdShowGPU ? 1 : 0,
        ARMSX2_iOSShouldShowDeviceStatsOverlay() ? 1 : 0,
        EmuConfig.GS.OsdShowFrameTimes ? 1 : 0,
        EmuConfig.GS.OsdShowVersion ? 1 : 0,
        EmuConfig.GS.OsdShowHardwareInfo ? 1 : 0);
}


#pragma mark - VM thread & CPU task queue
// Touch pad state
bool g_touchPadState[64] = {};

// Persistent VM thread lifecycle
std::atomic<bool> s_vmThreadActive{false};   // true while VM is executing
std::atomic<unsigned int> s_vmHeartbeatGeneration{0};
std::atomic<bool> s_requestVMStop{false};     // signal VM to stop from UI (extern for ARMSX2Bridge)
std::atomic<bool> s_requestVMBoot{false};     // signal VM thread to boot
std::mutex s_vmMutex;
std::condition_variable s_vmCV;
bool s_vmThreadCreated = false;               // guarded by s_vmMutex

std::thread::id s_cpuThreadId;
std::atomic<unsigned long long> s_cpuTaskNextId{1};
std::mutex s_cpuTaskMutex;
std::deque<std::shared_ptr<CPUThreadTask>> s_cpuTasks;

void ARMSX2DrainCPUThreadTasks()
{
    for (;;) {
        std::shared_ptr<CPUThreadTask> task;
        {
            std::lock_guard<std::mutex> lock(s_cpuTaskMutex);
            if (s_cpuTasks.empty())
                break;

            task = std::move(s_cpuTasks.front());
            s_cpuTasks.pop_front();
        }

        if (task && task->function) {
            std::fprintf(stderr, "@@CPU_TASK_RUN@@ id=%llu\n", task->id);
            std::fflush(stderr);
            task->function();
        }

        {
            std::lock_guard<std::mutex> lock(task->mutex);
            task->complete = true;
        }
        task->cv.notify_all();
    }
}

extern "C" bool ARMSX2_StartExternalGameDirectoryAccess(const char* path);
extern "C" void ARMSX2_PostRuntimeMenuStateChanged(void)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2iOSRuntimeMenuStateChanged" object:nil];
    });
}

// Gamepad button mapping — 16 PS2 buttons → SDL_GamepadButton
std::atomic<bool> s_captureMode{false};
std::atomic<int>  s_capturedButton{-1};

// Default mapping: PS2 index → SDL_GamepadButton
int s_buttonMap[16] = {
    SDL_GAMEPAD_BUTTON_DPAD_UP,        // 0  PAD_UP
    SDL_GAMEPAD_BUTTON_DPAD_DOWN,      // 1  PAD_DOWN
    SDL_GAMEPAD_BUTTON_DPAD_LEFT,      // 2  PAD_LEFT
    SDL_GAMEPAD_BUTTON_DPAD_RIGHT,     // 3  PAD_RIGHT
    SDL_GAMEPAD_BUTTON_SOUTH,          // 4  PAD_CROSS
    SDL_GAMEPAD_BUTTON_EAST,           // 5  PAD_CIRCLE
    SDL_GAMEPAD_BUTTON_WEST,           // 6  PAD_SQUARE
    SDL_GAMEPAD_BUTTON_NORTH,          // 7  PAD_TRIANGLE
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  // 8  PAD_L1
    SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, // 9  PAD_R1
    -1,                                // 10 PAD_L2 (analog trigger)
    -1,                                // 11 PAD_R2 (analog trigger)
    SDL_GAMEPAD_BUTTON_START,          // 12 PAD_START
    SDL_GAMEPAD_BUTTON_BACK,           // 13 PAD_SELECT
    SDL_GAMEPAD_BUTTON_LEFT_STICK,     // 14 PAD_L3
    SDL_GAMEPAD_BUTTON_RIGHT_STICK,    // 15 PAD_R3
};
const int s_defaultMap[16] = {
    SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
    SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    -1, -1,
    SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_BACK,
    SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
};

#pragma mark - View controllers & render-view glue
// View controller references for background color switching.
// __unsafe_unretained (not __weak): this TU is built under manual reference
// counting (no ARC), where __weak is illegal. The menu/root controllers are
// owned by UIKit/SwiftUI and outlive these unretained references.
UIViewController* __unsafe_unretained s_menuVC = nil;
UIViewController* __unsafe_unretained s_rootVC = nil;

static void ARMSX2EnsureGameRenderViewOnMain(const char* reason) {
    if (g_gameRenderView)
        return;

    g_gameRenderView = [[ARMSX2GameView alloc] initWithFrame:CGRectZero];
    g_gameRenderView.backgroundColor = [UIColor blackColor];
    g_gameRenderView.clipsToBounds = YES;
    [g_gameRenderView setNeedsLayout];
    Console.WriteLn("[Layout] Game render view prepared for SwiftUI (reason=%s)",
        reason ? reason : "unknown");
}

extern "C" void ARMSX2_PrepareGameRenderViewForCurrentRenderer(const char* reason) {
    if ([NSThread isMainThread]) {
        ARMSX2EnsureGameRenderViewOnMain(reason);
    } else {
        dispatch_sync(dispatch_get_main_queue(), ^{
            ARMSX2EnsureGameRenderViewOnMain(reason);
        });
    }
}

#pragma mark - Bridge settings handle
// Expose to ARMSX2Bridge.mm via extern
INISettingsInterface* g_p44_settings_interface = nullptr;
