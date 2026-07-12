// ARMSX2Bridge.mm — ObjC bridge implementation
// SPDX-License-Identifier: GPL-3.0+

#import "ARMSX2Bridge.h"

// Xcode names the generated Swift bridge header after the Swift module.
#if __has_include("ARMSX2iOS-Swift.h")
#import "ARMSX2iOS-Swift.h"
#define ARMSX2_HAS_SWIFTUI_HOST 1
#elif __has_include("ARMSX2-Swift.h")
#import "ARMSX2-Swift.h"
#define ARMSX2_HAS_SWIFTUI_HOST 1
#else
#define ARMSX2_HAS_SWIFTUI_HOST 0
#endif

// MetalFX spatial upscaler is iOS 16+ and weak-linked (see PCSX2 CMake). Both
// headers are pulled in here so isMetalFXSupported can probe device capability.
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>

#include "common/Darwin/DarwinMisc.h"
#include <SDL3/SDL.h>

extern "C" void ARMSX2_SetSDLFullscreen(bool enabled);
extern "C" bool ARMSX2_IsSDLFullscreen();
extern "C" void ARMSX2_iOSCopyDeviceStats(int* outBatteryPercent, int* outThermalState,
                                          double* outRamGB, bool* outLowPower);
#include "Common.h"
#include "Config.h"
#include "CDVD/CDVD.h"
#include "CDVD/CDVDcommon.h"
#include "VMManager.h"
#include "pcsx2/MTGS.h"
#include "Patch.h"
#include "Achievements.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Pad/PadDualshock2.h"
#include "SIO/Memcard/MemoryCardFile.h"
#include "SIO/Sio.h"
#include "Counters.h"
#include "GS/GSState.h"
#include "SPU2/spu2.h"
#include "GameList.h"
#include "ps2/BiosTools.h"
#include "pcsx2/Host.h"
#include "pcsx2/INISettingsInterface.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/ZipHelpers.h"
#include "common/Error.h"
#include "common/MRCHelpers.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>
#include <ifaddrs.h>
#include <net/if.h>

// Access the global settings interface from ios_main.mm
extern INISettingsInterface* g_p44_settings_interface;
extern "C" void ARMSX2_PrepareGameRenderViewForCurrentRenderer(const char* reason);
extern "C" void ARMSX2_PostRuntimeMenuStateChanged(void);
extern "C" void ARMSX2_iOSTestGamepadRumble(void);

// Coalesce base-settings INI writes so rapid changes (slider drags, preset bursts,
// repeated toggles) persist to disk once per short window instead of once per call.
// The in-memory interface is updated immediately, so reads always observe the latest
// value; only disk persistence is deferred. ARMSX2FlushINISave() forces a write now.
static BOOL s_ini_save_scheduled = NO;
static NSUInteger s_ini_save_generation = 0;

static void ARMSX2ScheduleINISave()
{
    if (!g_p44_settings_interface || s_ini_save_scheduled)
        return;
    s_ini_save_scheduled = YES;
    s_ini_save_generation++;
    const NSUInteger scheduled_generation = s_ini_save_generation;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.25 * NSEC_PER_SEC)),
        dispatch_get_main_queue(),
        ^{
            s_ini_save_scheduled = NO;
            if (scheduled_generation == s_ini_save_generation && g_p44_settings_interface)
                g_p44_settings_interface->Save();
        });
}

static void ARMSX2FlushINISave()
{
    s_ini_save_generation++;
    s_ini_save_scheduled = NO;
    if (g_p44_settings_interface)
        g_p44_settings_interface->Save();
}

static NSDate* s_lastNVMSaveDate = nil;
static NSDictionary<NSString*, id>* s_pendingRetroAchievementsNotification = nil;

@implementation ARMSX2SaveStateSlotInfo
@end

@implementation ARMSX2BIOSInfo
@end

static NSString* const ARMSX2CompatibilityProfileOff = @"off";
static NSString* const ARMSX2CompatibilityProfileCOP1 = @"cop1";
static NSString* const ARMSX2CompatibilityProfileLoadStore = @"loadstore";
static NSString* const ARMSX2CompatibilityProfileMMI = @"mmi";
static NSString* const ARMSX2CompatibilityProfileCOP2VU = @"cop2vu";
static NSString* const ARMSX2CompatibilityProfileMultDiv = @"multdiv";
static NSString* const ARMSX2CompatibilityProfileShifts = @"shifts";
static NSString* const ARMSX2CompatibilityProfileMoves = @"moves";
static NSString* const ARMSX2CompatibilityProfileIntegerALU = @"integeralu";
static NSString* const ARMSX2CompatibilityProfileBranches = @"branches";
static NSString* const ARMSX2CompatibilityProfileCustom = @"custom";
static constexpr int ARMSX2UseGlobalIntSentinel = -1;
static constexpr int ARMSX2TriFilterUseGlobalSentinel = std::numeric_limits<int>::min();
static constexpr int ARMSX2DefaultAudioVolumePercent = 100;

static int ARMSX2ClampInt(int value, int minValue, int maxValue)
{
    return std::min(std::max(value, minValue), maxValue);
}

static NSString* ARMSX2NSStringFromStdString(const std::string& value);

static NSString* ARMSX2RegionFallbackForSerial(const std::string& serial)
{
    std::string normalized;
    normalized.reserve(serial.size());
    for (const char ch : serial)
    {
        if (ch != '-' && ch != '_' && ch != ' ')
            normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }

    auto startsWith = [&normalized](const char* prefix) {
        return normalized.rfind(prefix, 0) == 0;
    };

    if (startsWith("SLUS") || startsWith("SCUS") || startsWith("PBPX"))
        return @"NTSC-U";
    if (startsWith("SLES") || startsWith("SCES") || startsWith("SLED") || startsWith("SCED"))
        return @"PAL";
    if (startsWith("SLPS") || startsWith("SLPM") || startsWith("SCPS") || startsWith("PCPX") || startsWith("SCAJ"))
        return @"NTSC-J";
    if (startsWith("SLKA") || startsWith("SCKA"))
        return @"NTSC-K";
    if (startsWith("SCCS"))
        return @"NTSC-C";
    if (startsWith("SLAJ"))
        return @"NTSC-HK";

    return nil;
}

static NSString* ARMSX2BIOSDisplayRegionForZone(NSString* zone)
{
    if ([zone isEqualToString:@"USA"])
        return @"North America";
    if ([zone length] > 0)
        return zone;
    return @"Unknown Region";
}

static NSString* ARMSX2BIOSCountryCodeForZone(NSString* zone)
{
    if ([zone isEqualToString:@"Japan"])
        return @"JP";
    if ([zone isEqualToString:@"USA"])
        return @"US";
    if ([zone isEqualToString:@"Europe"])
        return @"EU";
    if ([zone isEqualToString:@"Asia"])
        return @"HK";
    if ([zone isEqualToString:@"China"])
        return @"CN";
    return @"";
}

static ARMSX2BIOSInfo* ARMSX2MakeBIOSInfo(NSString* fileName, NSString* directory)
{
    ARMSX2BIOSInfo* info = [ARMSX2BIOSInfo new];
    info.fileName = fileName ?: @"";
    info.filePath = directory ? [directory stringByAppendingPathComponent:fileName ?: @""] : @"";
    info.regionName = @"Unknown Region";
    info.countryCode = @"";
    info.descriptionText = @"Region unavailable";
    info.regionCode = -1;
    info.valid = NO;

    u32 version = 0;
    u32 region = 0;
    std::string description;
    std::string zone;
    if (IsBIOS(info.filePath.UTF8String, version, description, region, zone)) {
        NSString* zoneString = ARMSX2NSStringFromStdString(zone);
        info.valid = YES;
        info.regionCode = static_cast<NSInteger>(region);
        info.regionName = ARMSX2BIOSDisplayRegionForZone(zoneString);
        info.countryCode = ARMSX2BIOSCountryCodeForZone(zoneString);
        info.descriptionText = ARMSX2NSStringFromStdString(description);
    }

    return info;
}

static int* ARMSX2JITBisectFlagPtr(NSString* key)
{
    if ([key isEqualToString:@"COP1EverythingOnly"]) return &DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_ONLY;
    if ([key isEqualToString:@"COP1EverythingPlusLoadStore"]) return &DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_LOADSTORE;
    if ([key isEqualToString:@"COP1EverythingPlusMMI"]) return &DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MMI;
    if ([key isEqualToString:@"COP1EverythingPlusCOP2VU"]) return &DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_COP2_VU;
    if ([key isEqualToString:@"COP1EverythingPlusMultDiv"]) return &DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MULTDIV;
    if ([key isEqualToString:@"COP1EverythingPlusShifts"]) return &DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_SHIFTS;
    if ([key isEqualToString:@"COP1EverythingPlusMoves"]) return &DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MOVES;
    if ([key isEqualToString:@"COP1EverythingPlusIntegerALU"]) return &DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_INTEGER_ALU;
    if ([key isEqualToString:@"COP1EverythingPlusBranches"]) return &DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_BRANCHES;
    return nullptr;
}

static void ARMSX2ApplyJITBisectFlag(NSString* key, BOOL enabled)
{
    if (int* flag = ARMSX2JITBisectFlagPtr(key))
        *flag = enabled ? 1 : 0;
}

static NSArray<NSString*>* ARMSX2JITBisectFlagKeys()
{
    static NSArray<NSString*>* keys;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        keys = @[
            @"COP1EverythingOnly",
            @"COP1EverythingPlusLoadStore",
            @"COP1EverythingPlusMMI",
            @"COP1EverythingPlusCOP2VU",
            @"COP1EverythingPlusMultDiv",
            @"COP1EverythingPlusShifts",
            @"COP1EverythingPlusMoves",
            @"COP1EverythingPlusIntegerALU",
            @"COP1EverythingPlusBranches",
        ];
    });
    return keys;
}

static NSString* ARMSX2CompatibilityProfileFlagKey(NSString* profile)
{
    if ([profile isEqualToString:ARMSX2CompatibilityProfileCOP1]) return @"COP1EverythingOnly";
    if ([profile isEqualToString:ARMSX2CompatibilityProfileLoadStore]) return @"COP1EverythingPlusLoadStore";
    if ([profile isEqualToString:ARMSX2CompatibilityProfileMMI]) return @"COP1EverythingPlusMMI";
    if ([profile isEqualToString:ARMSX2CompatibilityProfileCOP2VU]) return @"COP1EverythingPlusCOP2VU";
    if ([profile isEqualToString:ARMSX2CompatibilityProfileMultDiv]) return @"COP1EverythingPlusMultDiv";
    if ([profile isEqualToString:ARMSX2CompatibilityProfileShifts]) return @"COP1EverythingPlusShifts";
    if ([profile isEqualToString:ARMSX2CompatibilityProfileMoves]) return @"COP1EverythingPlusMoves";
    if ([profile isEqualToString:ARMSX2CompatibilityProfileIntegerALU]) return @"COP1EverythingPlusIntegerALU";
    if ([profile isEqualToString:ARMSX2CompatibilityProfileBranches]) return @"COP1EverythingPlusBranches";
    return @"";
}

static NSString* ARMSX2NormalizeCompatibilityProfile(NSString* profile)
{
    NSString* normalized = [profile.lowercaseString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if ([normalized isEqualToString:ARMSX2CompatibilityProfileCOP1] ||
        [normalized isEqualToString:ARMSX2CompatibilityProfileLoadStore] ||
        [normalized isEqualToString:ARMSX2CompatibilityProfileMMI] ||
        [normalized isEqualToString:ARMSX2CompatibilityProfileCOP2VU] ||
        [normalized isEqualToString:ARMSX2CompatibilityProfileMultDiv] ||
        [normalized isEqualToString:ARMSX2CompatibilityProfileShifts] ||
        [normalized isEqualToString:ARMSX2CompatibilityProfileMoves] ||
        [normalized isEqualToString:ARMSX2CompatibilityProfileIntegerALU] ||
        [normalized isEqualToString:ARMSX2CompatibilityProfileBranches] ||
        [normalized isEqualToString:ARMSX2CompatibilityProfileCustom])
        return normalized;

    return ARMSX2CompatibilityProfileOff;
}

static NSString* ARMSX2CurrentCompatibilityProfileFromSettings()
{
    if (!g_p44_settings_interface)
        return ARMSX2CompatibilityProfileOff;

    std::string stored = g_p44_settings_interface->GetStringValue("ARMSX2/JITBisect", "Profile", "");
    NSString* storedProfile = ARMSX2NormalizeCompatibilityProfile(ARMSX2NSStringFromStdString(stored));
    if (![storedProfile isEqualToString:ARMSX2CompatibilityProfileOff] && ![storedProfile isEqualToString:ARMSX2CompatibilityProfileCustom])
        return storedProfile;

    NSString* activeProfile = ARMSX2CompatibilityProfileOff;
    int activeCount = 0;
    for (NSString* key in ARMSX2JITBisectFlagKeys()) {
        if (g_p44_settings_interface->GetBoolValue("ARMSX2/JITBisect", key.UTF8String, false)) {
            activeCount++;
            NSString* profile = ARMSX2CompatibilityProfileOff;
            if ([key isEqualToString:@"COP1EverythingOnly"]) profile = ARMSX2CompatibilityProfileCOP1;
            else if ([key isEqualToString:@"COP1EverythingPlusLoadStore"]) profile = ARMSX2CompatibilityProfileLoadStore;
            else if ([key isEqualToString:@"COP1EverythingPlusMMI"]) profile = ARMSX2CompatibilityProfileMMI;
            else if ([key isEqualToString:@"COP1EverythingPlusCOP2VU"]) profile = ARMSX2CompatibilityProfileCOP2VU;
            else if ([key isEqualToString:@"COP1EverythingPlusMultDiv"]) profile = ARMSX2CompatibilityProfileMultDiv;
            else if ([key isEqualToString:@"COP1EverythingPlusShifts"]) profile = ARMSX2CompatibilityProfileShifts;
            else if ([key isEqualToString:@"COP1EverythingPlusMoves"]) profile = ARMSX2CompatibilityProfileMoves;
            else if ([key isEqualToString:@"COP1EverythingPlusIntegerALU"]) profile = ARMSX2CompatibilityProfileIntegerALU;
            else if ([key isEqualToString:@"COP1EverythingPlusBranches"]) profile = ARMSX2CompatibilityProfileBranches;
            activeProfile = profile;
        }
    }

    return activeCount == 0 ? ARMSX2CompatibilityProfileOff : (activeCount == 1 ? activeProfile : ARMSX2CompatibilityProfileCustom);
}

static void ARMSX2ApplyCompatibilityProfile(NSString* profile, BOOL persistSettings, NSString* reason)
{
    NSString* normalized = ARMSX2NormalizeCompatibilityProfile(profile);
    if ([normalized isEqualToString:ARMSX2CompatibilityProfileCustom]) {
        if (persistSettings && g_p44_settings_interface) {
            g_p44_settings_interface->SetStringValue("ARMSX2/JITBisect", "Profile", normalized.UTF8String);
            g_p44_settings_interface->Save();
        }

        NSLog(@"[ARMSX2Bridge] Compatibility preset=custom reason=%@ flags preserved", reason ?: @"manual");
        std::fprintf(stderr, "@@IOS_JIT_PROFILE_APPLY@@ profile=custom reason=\"%s\" persisted=%d flags_preserved=1\n",
            reason ? reason.UTF8String : "manual", persistSettings ? 1 : 0);
        std::fflush(stderr);
        return;
    }

    NSString* activeFlag = ARMSX2CompatibilityProfileFlagKey(normalized);

    for (NSString* key in ARMSX2JITBisectFlagKeys()) {
        const BOOL enabled = activeFlag.length > 0 && [key isEqualToString:activeFlag];
        ARMSX2ApplyJITBisectFlag(key, enabled);
        if (persistSettings && g_p44_settings_interface)
            g_p44_settings_interface->SetBoolValue("ARMSX2/JITBisect", key.UTF8String, enabled);
    }

    if (persistSettings && g_p44_settings_interface) {
        g_p44_settings_interface->SetStringValue("ARMSX2/JITBisect", "Profile", normalized.UTF8String);
        g_p44_settings_interface->Save();
    }

    NSLog(@"[ARMSX2Bridge] Compatibility preset=%@ reason=%@", normalized, reason ?: @"manual");
    std::fprintf(stderr,
        "@@IOS_JIT_PROFILE_APPLY@@ profile=%s flag=%s reason=\"%s\" persisted=%d cop1=%d ls=%d mmi=%d cop2vu=%d multdiv=%d shifts=%d moves=%d ialu=%d branches=%d\n",
        normalized.UTF8String, activeFlag.UTF8String, reason ? reason.UTF8String : "manual", persistSettings ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_ONLY ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_LOADSTORE ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MMI ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_COP2_VU ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MULTDIV ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_SHIFTS ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MOVES ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_INTEGER_ALU ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_BRANCHES ? 1 : 0);
    std::fflush(stderr);
}

static NSString* ARMSX2CompatibilityCustomFlagSection(NSString* identity)
{
    return [NSString stringWithFormat:@"ARMSX2/JITBisectGamePresetFlags/%@", identity ?: @""];
}

static void ARMSX2SaveCompatibilityCustomFlagsForIdentity(NSString* identity)
{
    if (!g_p44_settings_interface || identity.length == 0)
        return;

    NSString* section = ARMSX2CompatibilityCustomFlagSection(identity);
    g_p44_settings_interface->SetStringValue("ARMSX2/JITBisectGamePresets", identity.UTF8String, ARMSX2CompatibilityProfileCustom.UTF8String);
    for (NSString* key in ARMSX2JITBisectFlagKeys()) {
        BOOL enabled = NO;
        if (int* flag = ARMSX2JITBisectFlagPtr(key))
            enabled = (*flag != 0) ? YES : NO;
        else
            enabled = g_p44_settings_interface->GetBoolValue("ARMSX2/JITBisect", key.UTF8String, false) ? YES : NO;

        g_p44_settings_interface->SetBoolValue(section.UTF8String, key.UTF8String, enabled ? true : false);
    }
    g_p44_settings_interface->Save();
    NSLog(@"[ARMSX2Bridge] Compatibility custom flags saved identity=%@", identity);
}

static BOOL ARMSX2LoadCompatibilityCustomFlagsForIdentity(NSString* identity)
{
    if (!g_p44_settings_interface || identity.length == 0)
        return NO;

    NSString* section = ARMSX2CompatibilityCustomFlagSection(identity);
    bool foundAny = false;
    bool anyEnabled = false;

    for (NSString* key in ARMSX2JITBisectFlagKeys()) {
        bool enabled = false;
        if (g_p44_settings_interface->GetBoolValue(section.UTF8String, key.UTF8String, &enabled))
            foundAny = true;
        if (enabled)
            anyEnabled = true;

        ARMSX2ApplyJITBisectFlag(key, enabled ? YES : NO);
        g_p44_settings_interface->SetBoolValue("ARMSX2/JITBisect", key.UTF8String, enabled);
    }

    if (!foundAny || !anyEnabled)
    {
        std::fprintf(stderr, "@@IOS_JIT_PROFILE_CUSTOM@@ identity=\"%s\" found=%d enabled=0 action=ignore_empty_custom\n",
            identity ? identity.UTF8String : "", foundAny ? 1 : 0);
        std::fflush(stderr);
        return NO;
    }

    g_p44_settings_interface->SetStringValue("ARMSX2/JITBisect", "Profile", ARMSX2CompatibilityProfileCustom.UTF8String);
    g_p44_settings_interface->Save();
    NSLog(@"[ARMSX2Bridge] Compatibility custom flags loaded identity=%@", identity);
    std::fprintf(stderr,
        "@@IOS_JIT_PROFILE_CUSTOM@@ identity=\"%s\" found=1 enabled=1 cop1=%d ls=%d mmi=%d cop2vu=%d multdiv=%d shifts=%d moves=%d ialu=%d branches=%d\n",
        identity ? identity.UTF8String : "",
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_ONLY ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_LOADSTORE ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MMI ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_COP2_VU ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MULTDIV ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_SHIFTS ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MOVES ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_INTEGER_ALU ? 1 : 0,
        DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_BRANCHES ? 1 : 0);
    std::fflush(stderr);
    return YES;
}

static void ARMSX2ClearCompatibilityCustomFlagsForIdentity(NSString* identity)
{
    if (!g_p44_settings_interface || identity.length == 0)
        return;

    NSString* section = ARMSX2CompatibilityCustomFlagSection(identity);
    g_p44_settings_interface->ClearSection(section.UTF8String);
}

static NSString* ARMSX2NSStringFromStdString(const std::string& value)
{
    if (value.empty())
        return @"";

    NSString* string = [NSString stringWithUTF8String:value.c_str()];
    return string ?: @"";
}

static NSString* ARMSX2NSStringFromStringView(std::string_view value)
{
    if (value.empty())
        return @"";

    NSString* string = [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
    return string ?: @"";
}

extern "C" void ARMSX2_PostRetroAchievementsStateChanged(void)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2RetroAchievementsStateChanged" object:nil];
    });
}

extern "C" void ARMSX2_PostRetroAchievementsNotification(const char* title, const char* message, const char* badgePath)
{
    NSString* titleString = title ? [NSString stringWithUTF8String:title] : nil;
    if (titleString.length == 0)
        return;

    NSString* messageString = message ? [NSString stringWithUTF8String:message] : nil;
    NSString* badgePathString = badgePath ? [NSString stringWithUTF8String:badgePath] : nil;
    if (!messageString)
        messageString = @"";
    if (!badgePathString)
        badgePathString = @"";

    std::fprintf(stderr, "@@RA_NOTIFY@@ title_len=%lu message_len=%lu badge=%d hardcore=%d notifications=%d overlays=%d\n",
        static_cast<unsigned long>(titleString.length),
        static_cast<unsigned long>(messageString.length),
        badgePathString.length > 0 ? 1 : 0,
        Achievements::IsHardcoreModeActive() ? 1 : 0,
        EmuConfig.Achievements.Notifications ? 1 : 0,
        EmuConfig.Achievements.Overlays ? 1 : 0);
    std::fflush(stderr);

    dispatch_async(dispatch_get_main_queue(), ^{
        NSDictionary* userInfo = @{
            @"title": titleString,
            @"message": messageString,
            @"badgePath": badgePathString,
            @"handledByUIKit": @NO,
        };
        s_pendingRetroAchievementsNotification = userInfo;
        std::fprintf(stderr, "@@RA_NOTIFY_QUEUED@@ title_len=%lu pending=1\n",
            static_cast<unsigned long>(titleString.length));
        std::fflush(stderr);
        [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2RetroAchievementsNotification" object:nil userInfo:userInfo];
    });
}

static dispatch_queue_t ARMSX2RetroAchievementsQueue()
{
    static dispatch_queue_t queue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        queue = dispatch_queue_create("org.armsx2.ios.retroachievements", DISPATCH_QUEUE_SERIAL);
    });
    return queue;
}

static constexpr bool ARMSX2RetroAchievementsAvailable = true;
static constexpr bool ARMSX2RetroAchievementsHardcoreAvailable = true;

static NSString* ARMSX2RetroAchievementsUnavailableMessage()
{
    return @"RetroAchievements is temporarily unavailable in this build.";
}

static void ARMSX2SaveBaseSettingBool(const char* section, const char* key, bool value);

static void ARMSX2ForceRetroAchievementsHardcoreOff()
{
    Pcsx2Config::AchievementsOptions old_config = EmuConfig.Achievements;
    EmuConfig.Achievements.HardcoreMode = false;
    ARMSX2SaveBaseSettingBool("Achievements", "ChallengeMode", false);

    if (Achievements::IsActive())
        Achievements::UpdateSettings(old_config);
}

static bool ARMSX2EnsureAchievementsClientInitialized()
{
    if (!EmuConfig.Achievements.Enabled)
        return false;

    if (!Achievements::IsActive())
        return Achievements::Initialize();

    return true;
}

static bool ARMSX2RetroAchievementsHardcoreActive()
{
    return EmuConfig.Achievements.Enabled && Achievements::IsHardcoreModeActive();
}

static void ARMSX2LogRetroAchievementsHardcoreBlock(const char* action)
{
    std::fprintf(stderr, "@@RA_HARDCORE_BLOCK@@ action=%s\n", action ? action : "unknown");
    std::fflush(stderr);
    NSLog(@"[ARMSX2Bridge] RetroAchievements Hardcore blocked action=%s", action ? action : "unknown");
}

static void ARMSX2SaveBaseSettingBool(const char* section, const char* key, bool value)
{
    Host::SetBaseBoolSettingValue(section, key, value);
    if (g_p44_settings_interface) {
        g_p44_settings_interface->SetBoolValue(section, key, value);
        g_p44_settings_interface->Save();
    }
}

static void ARMSX2UpdateAchievementsSettings(void (^mutate)())
{
    Pcsx2Config::AchievementsOptions old_config = EmuConfig.Achievements;
    mutate();
    Achievements::UpdateSettings(old_config);
    ARMSX2_PostRetroAchievementsStateChanged();
}

static BOOL ARMSX2GetCurrentSaveStateIdentity(std::string* serial, u32* crc)
{
    if (!VMManager::HasValidVM())
        return NO;

    const std::string currentSerial = VMManager::GetDiscSerial();
    const u32 currentCRC = VMManager::GetDiscCRC();
    if (currentSerial.empty() || currentCRC == 0)
        return NO;

    if (serial)
        *serial = currentSerial;
    if (crc)
        *crc = currentCRC;
	return YES;
}

static NSString* const ARMSX2ExternalGameDirectoriesDefaultsKey = @"ARMSX2iOSExternalGameDirectories";

static BOOL ARMSX2IsPathInsideDirectory(NSString* path, NSString* directory)
{
	if (path.length == 0 || directory.length == 0)
		return NO;

	NSString* normalizedPath = path.stringByStandardizingPath;
	NSString* normalizedDirectory = directory.stringByStandardizingPath;
	if ([normalizedPath isEqualToString:normalizedDirectory])
		return YES;

	NSString* prefix = [normalizedDirectory hasSuffix:@"/"] ? normalizedDirectory : [normalizedDirectory stringByAppendingString:@"/"];
	return [normalizedPath hasPrefix:prefix];
}

static NSMutableArray<NSURL*>* ARMSX2ActiveExternalGameAccessURLs()
{
	static NSMutableArray<NSURL*>* activeAccess = nil;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		activeAccess = [[NSMutableArray alloc] init];
	});
	return activeAccess;
}

static BOOL ARMSX2ExternalGameAccessAlreadyActive(NSString* path)
{
	NSMutableArray<NSURL*>* activeAccess = ARMSX2ActiveExternalGameAccessURLs();
	@synchronized(activeAccess) {
		for (NSURL* activeURL in activeAccess) {
			if (![activeURL isKindOfClass:NSURL.class])
				continue;

			NSString* activePath = activeURL.path;
			if (activePath.length > 0 && ARMSX2IsPathInsideDirectory(path, activePath))
				return YES;
		}
	}

	return NO;
}

static void ARMSX2RememberExternalGameAccess(NSURL* url)
{
	if (!url)
		return;

	NSString* normalizedPath = url.path.stringByStandardizingPath;
	if (normalizedPath.length == 0)
		return;

	NSMutableArray<NSURL*>* activeAccess = ARMSX2ActiveExternalGameAccessURLs();
	@synchronized(activeAccess) {
		for (NSURL* activeURL in activeAccess) {
			if (![activeURL isKindOfClass:NSURL.class])
				continue;
			if ([activeURL.path.stringByStandardizingPath isEqualToString:normalizedPath])
				return;
		}

		[activeAccess addObject:url];
	}
}

static BOOL ARMSX2IsSupportedGameImageAtPath(NSString* path);

static NSArray<NSDictionary*>* ARMSX2ExternalGameDirectoryRecords()
{
	id rawRecords = [[NSUserDefaults standardUserDefaults] objectForKey:ARMSX2ExternalGameDirectoriesDefaultsKey];
	if (![rawRecords isKindOfClass:NSArray.class]) {
		if (rawRecords)
			NSLog(@"[ARMSX2Bridge] External game folder records ignored unexpectedClass=%@", [rawRecords class]);
		return @[];
	}

	NSMutableArray<NSDictionary*>* records = [NSMutableArray array];
	for (id rawRecord in (NSArray*)rawRecords) {
		if ([rawRecord isKindOfClass:NSDictionary.class])
			[records addObject:(NSDictionary*)rawRecord];
		else
			NSLog(@"[ARMSX2Bridge] External game folder record ignored unexpectedClass=%@", [rawRecord class]);
	}

	return records;
}

static BOOL ARMSX2ExternalGameRecordIsDirectory(NSDictionary* record, NSURL* url)
{
	id isDirectoryValue = record[@"isDirectory"];
	if ([isDirectoryValue isKindOfClass:NSNumber.class])
		return [(NSNumber*)isDirectoryValue boolValue];

	NSString* kind = [record[@"kind"] isKindOfClass:NSString.class] ? record[@"kind"] : nil;
	if ([kind isEqualToString:@"file"])
		return NO;
	if ([kind isEqualToString:@"folder"])
		return YES;

	NSNumber* isDirectory = nil;
	if ([url getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil])
		return isDirectory.boolValue;

	return !ARMSX2IsSupportedGameImageAtPath(url.path);
}

static BOOL ARMSX2ExternalGameRecordIsCloudProvider(NSDictionary* record, NSURL* url)
{
	NSString* path = (url.path ?: @"").lowercaseString;
	NSString* displayName = [record[@"displayName"] isKindOfClass:NSString.class] ? [(NSString*)record[@"displayName"] lowercaseString] : @"";
	return [path containsString:@"google"] || [displayName containsString:@"google"];
}

static BOOL ARMSX2ExternalGameRecordScanDisabled(NSDictionary* record, NSURL* url)
{
	if (ARMSX2ExternalGameRecordIsCloudProvider(record, url))
		return YES;

	id scanDisabledValue = record[@"scanDisabled"];
	if ([scanDisabledValue isKindOfClass:NSNumber.class] && [(NSNumber*)scanDisabledValue boolValue])
		NSLog(@"[ARMSX2Bridge] Ignoring stale external scanDisabled flag for non-cloud path=%@", url.path);

	return NO;
}

static NSURL* ARMSX2ResolveExternalGameDirectoryRecord(NSDictionary* record)
{
	NSString* path = [record[@"path"] isKindOfClass:NSString.class] ? record[@"path"] : nil;
	NSData* bookmarkData = [record[@"bookmarkData"] isKindOfClass:NSData.class] ? record[@"bookmarkData"] : nil;
	if (bookmarkData.length > 0) {
		BOOL stale = NO;
		NSError* error = nil;
		NSURL* url = [NSURL URLByResolvingBookmarkData:bookmarkData
		                                       options:0
		                                 relativeToURL:nil
		                           bookmarkDataIsStale:&stale
		                                         error:&error];
		if (url) {
			if (stale)
				NSLog(@"[ARMSX2Bridge] External game folder bookmark is stale path=%@", url.path);
			return url;
		}

		NSLog(@"[ARMSX2Bridge] External game folder bookmark failed path=%@ error=%@",
		      path ?: @"", error.localizedDescription ?: @"");
	}

	if (path.length > 0) {
		BOOL isDirectory = YES;
		id isDirectoryValue = record[@"isDirectory"];
		NSString* kind = [record[@"kind"] isKindOfClass:NSString.class] ? record[@"kind"] : nil;
		if ([isDirectoryValue isKindOfClass:NSNumber.class])
			isDirectory = [(NSNumber*)isDirectoryValue boolValue];
		else if ([kind isEqualToString:@"file"])
			isDirectory = NO;
		return [NSURL fileURLWithPath:path isDirectory:isDirectory];
	}

	return nil;
}

static BOOL ARMSX2StartExternalGameDirectoryAccessForPath(NSString* path)
{
	if (path.length == 0 || !path.isAbsolutePath)
		return NO;

	if (ARMSX2ExternalGameAccessAlreadyActive(path))
		return YES;

	for (NSDictionary* record in ARMSX2ExternalGameDirectoryRecords()) {
		NSURL* directoryURL = ARMSX2ResolveExternalGameDirectoryRecord(record);
		if (!directoryURL)
			continue;
		if (ARMSX2ExternalGameRecordIsCloudProvider(record, directoryURL)) {
			NSLog(@"[ARMSX2Bridge] External game cloud provider direct access skipped path=%@", directoryURL.path);
			continue;
		}

		BOOL isDirectory = ARMSX2ExternalGameRecordIsDirectory(record, directoryURL);
		NSString* normalizedRecordPath = directoryURL.path.stringByStandardizingPath;
		NSString* normalizedPath = path.stringByStandardizingPath;
		BOOL matches = isDirectory ? ARMSX2IsPathInsideDirectory(path, directoryURL.path) : [normalizedPath isEqualToString:normalizedRecordPath];
		if (!matches)
			continue;

		BOOL granted = [directoryURL startAccessingSecurityScopedResource];
		if (granted) {
			ARMSX2RememberExternalGameAccess(directoryURL);
			NSLog(@"[ARMSX2Bridge] External game %@ access active path=%@", isDirectory ? @"folder" : @"file", directoryURL.path);
		} else {
			NSLog(@"[ARMSX2Bridge] External game %@ access not granted path=%@", isDirectory ? @"folder" : @"file", directoryURL.path);
		}
		return granted;
	}

	return NO;
}

static BOOL ARMSX2StartExternalGameDirectoryAccessForPathSafe(NSString* path)
{
	@try {
		return ARMSX2StartExternalGameDirectoryAccessForPath(path);
	} @catch (NSException* exception) {
		NSLog(@"[ARMSX2Bridge] External game folder access exception path=%@ name=%@ reason=%@",
		      path ?: @"", exception.name ?: @"", exception.reason ?: @"");
		return NO;
	}
}

extern "C" bool ARMSX2_StartExternalGameDirectoryAccess(const char* path)
{
	if (!path || path[0] == '\0')
		return false;

	NSString* nsPath = [NSString stringWithUTF8String:path];
	return ARMSX2StartExternalGameDirectoryAccessForPathSafe(nsPath) ? true : false;
}

static BOOL ARMSX2IsSupportedGameImageAtPath(NSString* path)
{
	NSString* ext = path.pathExtension.lowercaseString;
	if ([ext isEqualToString:@"iso"] || [ext isEqualToString:@"img"] || [ext isEqualToString:@"chd"] ||
	    [ext isEqualToString:@"cso"] || [ext isEqualToString:@"zso"] || [ext isEqualToString:@"gz"] ||
	    [ext isEqualToString:@"elf"])
		return YES;

	if ([ext isEqualToString:@"bin"]) {
		NSDictionary* attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:nil];
		return [attrs fileSize] > 50 * 1024 * 1024;
	}

	return NO;
}

static void ARMSX2EnumerateLocalGameImages(NSString* root, void (^block)(NSString* absolutePath, NSString* relativeName))
{
	NSFileManager* fm = [NSFileManager defaultManager];
	BOOL isDir = NO;
	if (root.length == 0 || block == nil || ![fm fileExistsAtPath:root isDirectory:&isDir] || !isDir)
		return;

	NSString* prefix = [root.stringByStandardizingPath stringByAppendingString:@"/"];
	for (NSURL* url in [fm enumeratorAtURL:[NSURL fileURLWithPath:root isDirectory:YES]
	               includingPropertiesForKeys:nil
	                                  options:NSDirectoryEnumerationSkipsHiddenFiles
	                             errorHandler:nil]) {
		NSString* path = url.path;
		if (!ARMSX2IsSupportedGameImageAtPath(path))
			continue;

		NSString* full = path.stringByStandardizingPath;
		NSString* rel = [full hasPrefix:prefix] ? [full substringFromIndex:prefix.length] : full.lastPathComponent;
		if ([rel containsString:@"/"] && ![rel.pathExtension.lowercaseString isEqualToString:@"elf"])
			continue;

		block(path, rel);
	}
}

static NSString* ARMSX2ResolveISOPath(NSString* isoName)
{
	if (isoName.length == 0)
		return nil;

	NSFileManager* fm = [NSFileManager defaultManager];
	if (isoName.isAbsolutePath) {
		if ([fm fileExistsAtPath:isoName])
			return isoName;

		if (ARMSX2StartExternalGameDirectoryAccessForPathSafe(isoName) && [fm fileExistsAtPath:isoName])
			return isoName;
	}

	NSString* isoPath = [[ARMSX2Bridge isoDirectory] stringByAppendingPathComponent:isoName];
	if ([fm fileExistsAtPath:isoPath])
		return isoPath;

    NSString* docsPath = [[ARMSX2Bridge documentsDirectory] stringByAppendingPathComponent:isoName];
    if ([fm fileExistsAtPath:docsPath])
        return docsPath;

    return nil;
}

static BOOL ARMSX2PopulateGameListEntryForISO(NSString* isoName, GameList::Entry* entry, NSString** resolvedPath)
{
    NSString* path = ARMSX2ResolveISOPath(isoName);
    if (resolvedPath)
        *resolvedPath = path;

    if (path.length == 0 || !entry)
        return NO;

    return GameList::PopulateEntryFromPath(path.UTF8String, entry) ? YES : NO;
}

static NSString* ARMSX2CompatibilityIdentityKey(NSString* serial, u32 crc);

static NSArray<NSString*>* ARMSX2GameDataTokensForEntry(NSString* isoName, const GameList::Entry& entry)
{
    NSMutableOrderedSet<NSString*>* tokens = [NSMutableOrderedSet orderedSet];
    NSString* baseName = isoName.stringByDeletingPathExtension ?: isoName;
    if (baseName.length > 0)
        [tokens addObject:baseName.lowercaseString];
    if (!entry.serial.empty())
        [tokens addObject:ARMSX2NSStringFromStdString(entry.serial).lowercaseString];
    if (entry.crc != 0)
        [tokens addObject:[[NSString stringWithFormat:@"%08X", entry.crc] lowercaseString]];
    return tokens.array;
}

static NSInteger ARMSX2RemoveMatchingGeneratedFiles(NSString* directory, NSArray<NSString*>* tokens)
{
    if (directory.length == 0 || tokens.count == 0)
        return 0;

    NSFileManager* fm = [NSFileManager defaultManager];
    BOOL isDirectory = NO;
    if (![fm fileExistsAtPath:directory isDirectory:&isDirectory] || !isDirectory)
        return 0;

    NSMutableArray<NSURL*>* matches = [NSMutableArray array];
    NSURL* rootURL = [NSURL fileURLWithPath:directory isDirectory:YES];
    NSDirectoryEnumerator<NSURL*>* enumerator =
        [fm enumeratorAtURL:rootURL
 includingPropertiesForKeys:@[NSURLIsDirectoryKey]
                    options:NSDirectoryEnumerationSkipsHiddenFiles
               errorHandler:^BOOL(NSURL* url, NSError* error) {
                   NSLog(@"[ARMSX2Bridge] Game data scan skipped %@ error=%@", url.path, error.localizedDescription);
                   return YES;
               }];

    for (NSURL* url in enumerator) {
        NSString* name = url.lastPathComponent.lowercaseString;
        for (NSString* token in tokens) {
            if (token.length > 3 && [name containsString:token]) {
                [matches addObject:url];
                [enumerator skipDescendants];
                break;
            }
        }
    }

    [matches sortUsingComparator:^NSComparisonResult(NSURL* lhs, NSURL* rhs) {
        return lhs.path.length > rhs.path.length ? NSOrderedAscending : NSOrderedDescending;
    }];

    NSInteger removed = 0;
    for (NSURL* url in matches) {
        NSError* error = nil;
        if ([fm removeItemAtURL:url error:&error]) {
            removed++;
            NSLog(@"[ARMSX2Bridge] Removed generated game file %@", url.path);
        } else {
            NSLog(@"[ARMSX2Bridge] Failed removing generated game file %@ error=%@", url.path, error.localizedDescription);
        }
    }
    return removed;
}

static NSString* ARMSX2CompatibilityIdentityForISOName(NSString* isoName, GameList::Entry* entryOut = nullptr)
{
    GameList::Entry entry;
    NSString* resolvedPath = nil;
    if (!ARMSX2PopulateGameListEntryForISO(isoName, &entry, &resolvedPath) || entry.crc == 0)
        return @"";

    if (entryOut)
        *entryOut = entry;

    return ARMSX2CompatibilityIdentityKey(ARMSX2NSStringFromStdString(entry.serial), entry.crc);
}

static NSString* ARMSX2CompatibilityIdentityKey(NSString* serial, u32 crc)
{
    NSString* normalizedSerial = [[serial ?: @"" stringByReplacingOccurrencesOfString:@"_" withString:@"-"] uppercaseString];
    normalizedSerial = [normalizedSerial stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (normalizedSerial.length > 0)
        return normalizedSerial;

    if (crc != 0)
        return [NSString stringWithFormat:@"CRC-%08X", crc];

    return @"";
}

static NSString* ARMSX2CurrentCompatibilityIdentityKey()
{
    if (!VMManager::HasValidVM())
        return @"";

    return ARMSX2CompatibilityIdentityKey(ARMSX2NSStringFromStdString(VMManager::GetDiscSerial()), VMManager::GetDiscCRC());
}

static NSString* ARMSX2CompatibilityBuiltInPreset(NSString* title, NSString* serial)
{
    return ARMSX2CompatibilityProfileOff;
}

static NSString* ARMSX2SavedCompatibilityPreset(NSString* identity)
{
    if (!g_p44_settings_interface || identity.length == 0)
        return @"";

    std::string value = g_p44_settings_interface->GetStringValue("ARMSX2/JITBisectGamePresets", identity.UTF8String, "");
    if (value.empty())
        return @"";

    return ARMSX2NormalizeCompatibilityProfile(ARMSX2NSStringFromStdString(value));
}

static NSString* ARMSX2ResolvedCompatibilityPreset(NSString* identity, NSString* title)
{
    if (!g_p44_settings_interface)
        return ARMSX2CompatibilityProfileOff;

    const bool autoPresets = g_p44_settings_interface->GetBoolValue("ARMSX2/JITBisect", "AutoGamePresets", true);
    if (!autoPresets)
        return ARMSX2CurrentCompatibilityProfileFromSettings();

    NSString* saved = ARMSX2SavedCompatibilityPreset(identity);
    if (saved.length > 0)
        return saved;

    NSString* builtIn = ARMSX2CompatibilityBuiltInPreset(title, identity);
    if (builtIn.length > 0)
        return builtIn;

    return ARMSX2CompatibilityProfileOff;
}

static void ARMSX2ApplyCompatibilityPresetForISOName(NSString* isoName)
{
    NSString* identity = @"";
    NSString* title = isoName.stringByDeletingPathExtension ?: isoName;
    NSString* path = ARMSX2ResolveISOPath(isoName);

    if (path.length > 0) {
        GameList::Entry entry;
        if (GameList::PopulateEntryFromPath(path.UTF8String, &entry)) {
            identity = ARMSX2CompatibilityIdentityKey(ARMSX2NSStringFromStdString(entry.serial), entry.crc);
            title = ARMSX2NSStringFromStdString(entry.GetTitle(false));
            if (title.length == 0)
                title = isoName.stringByDeletingPathExtension ?: isoName;
        }
    }

    const bool autoPresets = g_p44_settings_interface ?
        g_p44_settings_interface->GetBoolValue("ARMSX2/JITBisect", "AutoGamePresets", true) : false;
    NSString* saved = ARMSX2SavedCompatibilityPreset(identity);
    NSString* builtIn = ARMSX2CompatibilityBuiltInPreset(title, identity);
    NSString* profile = ARMSX2ResolvedCompatibilityPreset(identity, title);
    std::fprintf(stderr,
        "@@IOS_JIT_PRESET_RESOLVE@@ iso=\"%s\" path=\"%s\" identity=\"%s\" title=\"%s\" auto=%d saved=\"%s\" builtin=\"%s\" profile=\"%s\"\n",
        isoName ? isoName.UTF8String : "", path ? path.UTF8String : "", identity ? identity.UTF8String : "",
        title ? title.UTF8String : "", autoPresets ? 1 : 0, saved ? saved.UTF8String : "",
        builtIn ? builtIn.UTF8String : "", profile ? profile.UTF8String : "");
    std::fflush(stderr);
    if ([profile isEqualToString:ARMSX2CompatibilityProfileCustom]) {
        if (ARMSX2LoadCompatibilityCustomFlagsForIdentity(identity)) {
            NSLog(@"[ARMSX2Bridge] Compatibility preset=custom identity=%@ reason=boot %@", identity ?: @"", title ?: @"");
            std::fprintf(stderr,
                "@@IOS_JIT_PROFILE_APPLY@@ profile=custom reason=\"boot %s %s\" persisted=1 flags_preserved=0 loaded_custom=1\n",
                identity ? identity.UTF8String : "", title ? title.UTF8String : "");
            std::fflush(stderr);
            return;
        }

        if (builtIn.length > 0) {
            g_p44_settings_interface->DeleteValue("ARMSX2/JITBisectGamePresets", identity.UTF8String);
            ARMSX2ClearCompatibilityCustomFlagsForIdentity(identity);
            profile = builtIn;
            std::fprintf(stderr,
                "@@IOS_JIT_PROFILE_STALE_CUSTOM_IGNORED@@ identity=\"%s\" title=\"%s\" fallback=\"%s\"\n",
                identity ? identity.UTF8String : "", title ? title.UTF8String : "", profile.UTF8String);
            std::fflush(stderr);
        } else {
            profile = ARMSX2CompatibilityProfileOff;
        }
    }
    ARMSX2ApplyCompatibilityProfile(profile, YES, [NSString stringWithFormat:@"boot %@ %@", identity ?: @"", title ?: @""]);
}

static NSString* ARMSX2SanitizedMemoryCardName(NSString* name)
{
    NSString* trimmed = [name stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (trimmed.length == 0)
        return @"";

    NSMutableString* sanitized = [NSMutableString stringWithCapacity:trimmed.length];
    NSCharacterSet* invalid = [NSCharacterSet characterSetWithCharactersInString:@"/\\:?%*|\"<>"];
    for (NSUInteger i = 0; i < trimmed.length; i++) {
        unichar ch = [trimmed characterAtIndex:i];
        [sanitized appendString:[invalid characterIsMember:ch] ? @"_" : [NSString stringWithCharacters:&ch length:1]];
    }

    while ([sanitized containsString:@".."])
        [sanitized replaceOccurrencesOfString:@".." withString:@"_" options:0 range:NSMakeRange(0, sanitized.length)];

    if (sanitized.pathExtension.length == 0)
        [sanitized appendString:@".ps2"];

    return sanitized;
}

static MemoryCardFileType ARMSX2MemoryCardFileTypeForSizeMB(NSInteger sizeMB)
{
    switch (sizeMB) {
    case 8:
        return MemoryCardFileType::PS2_8MB;
    case 16:
        return MemoryCardFileType::PS2_16MB;
    case 32:
        return MemoryCardFileType::PS2_32MB;
    case 64:
        return MemoryCardFileType::PS2_64MB;
    default:
        return MemoryCardFileType::Unknown;
    }
}

static NSData* ARMSX2ReadSaveStatePreviewPNG(const std::string& path)
{
    if (path.empty())
        return nil;

    zip_error_t ze = {};
    auto zf = zip_open_managed(path.c_str(), ZIP_RDONLY, &ze);
    if (!zf)
        return nil;

    auto zff = zip_fopen_managed(zf.get(), "Screenshot.png", 0);
    if (!zff)
        return nil;

    std::optional<std::vector<u8>> data = ReadBinaryFileInZip(zff.get());
    if (!data.has_value() || data->empty())
        return nil;

    return [NSData dataWithBytes:data->data() length:data->size()];
}

static dispatch_queue_t ARMSX2SaveStateQueue()
{
    static dispatch_queue_t queue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        queue = dispatch_queue_create("org.armsx2.ios.savestates", DISPATCH_QUEUE_SERIAL);
    });
    return queue;
}

static NSString* ARMSX2SanitizedBackupPathComponent(NSString* value)
{
    if (value.length == 0)
        return @"unknown";

    NSMutableString* sanitized = [NSMutableString stringWithCapacity:value.length];
    NSCharacterSet* invalid = [NSCharacterSet characterSetWithCharactersInString:@"/\\:?%*|\"<> "];
    for (NSUInteger i = 0; i < value.length; i++) {
        const unichar ch = [value characterAtIndex:i];
        [sanitized appendString:[invalid characterIsMember:ch] ? @"_" : [NSString stringWithCharacters:&ch length:1]];
    }

    return sanitized.length > 0 ? sanitized : @"unknown";
}

static NSString* ARMSX2MemcardBackupRoot()
{
    const std::string root = Path::Combine(EmuFolders::DataRoot, "memcard-state-backups");
    return ARMSX2NSStringFromStdString(root);
}

static void ARMSX2PruneOldMemcardBackups(NSString* backupRoot, NSUInteger keepCount)
{
    if (backupRoot.length == 0 || keepCount == 0)
        return;

    NSFileManager* fm = [NSFileManager defaultManager];
    NSArray<NSURL*>* entries = [fm contentsOfDirectoryAtURL:[NSURL fileURLWithPath:backupRoot]
                                includingPropertiesForKeys:@[NSURLContentModificationDateKey]
                                                   options:NSDirectoryEnumerationSkipsHiddenFiles
                                                     error:nil];
    if (entries.count <= keepCount)
        return;

    NSArray<NSURL*>* sorted = [entries sortedArrayUsingComparator:^NSComparisonResult(NSURL* lhs, NSURL* rhs) {
        NSDate* leftDate = nil;
        NSDate* rightDate = nil;
        [lhs getResourceValue:&leftDate forKey:NSURLContentModificationDateKey error:nil];
        [rhs getResourceValue:&rightDate forKey:NSURLContentModificationDateKey error:nil];
        NSDate* lhsDate = leftDate ?: [NSDate distantPast];
        NSDate* rhsDate = rightDate ?: [NSDate distantPast];
        return [rhsDate compare:lhsDate];
    }];

    for (NSUInteger i = keepCount; i < sorted.count; i++) {
        NSError* error = nil;
        if (![fm removeItemAtURL:sorted[i] error:&error]) {
            NSLog(@"[ARMSX2 iOS SaveState] memcard backup prune failed path=%@ error=%@",
                  sorted[i].path, error.localizedDescription ?: @"unknown");
        }
    }
}

static NSInteger ARMSX2BackupAssignedMemoryCards(const char* reason, s32 stateSlot, const std::string& serial, u32 crc)
{
    NSString* backupRoot = ARMSX2MemcardBackupRoot();
    if (backupRoot.length == 0)
        return 0;

    NSFileManager* fm = [NSFileManager defaultManager];
    NSError* mkdirError = nil;
    if (![fm createDirectoryAtPath:backupRoot withIntermediateDirectories:YES attributes:nil error:&mkdirError]) {
        NSLog(@"[ARMSX2 iOS SaveState] memcard backup root failed path=%@ error=%@",
              backupRoot, mkdirError.localizedDescription ?: @"unknown");
        return 0;
    }

    NSString* safeSerial = ARMSX2SanitizedBackupPathComponent(ARMSX2NSStringFromStdString(serial));
    const long long timestamp = static_cast<long long>(llround([[NSDate date] timeIntervalSince1970] * 1000.0));
    NSString* backupDirName = [NSString stringWithFormat:@"%lld-%@-%08X-slot%02d-%s",
                                                        timestamp, safeSerial, crc, stateSlot, reason ? reason : "state"];
    NSString* backupDir = [backupRoot stringByAppendingPathComponent:backupDirName];
    if (![fm createDirectoryAtPath:backupDir withIntermediateDirectories:YES attributes:nil error:&mkdirError]) {
        NSLog(@"[ARMSX2 iOS SaveState] memcard backup directory failed path=%@ error=%@",
              backupDir, mkdirError.localizedDescription ?: @"unknown");
        return 0;
    }

    NSInteger copied = 0;
    constexpr size_t numMemoryCardSlots = sizeof(EmuConfig.Mcd) / sizeof(EmuConfig.Mcd[0]);
    for (size_t i = 0; i < numMemoryCardSlots; i++) {
        if (!EmuConfig.Mcd[i].Enabled || EmuConfig.Mcd[i].Filename.empty())
            continue;

        const std::string source = EmuConfig.FullpathToMcd(static_cast<uint>(i));
        NSString* sourcePath = ARMSX2NSStringFromStdString(source);
        BOOL isDirectory = NO;
        if (sourcePath.length == 0 || ![fm fileExistsAtPath:sourcePath isDirectory:&isDirectory])
            continue;

        NSString* sourceName = ARMSX2SanitizedBackupPathComponent(sourcePath.lastPathComponent);
        NSString* targetName = [NSString stringWithFormat:@"slot%zu-%@", i + 1, sourceName];
        NSString* targetPath = [backupDir stringByAppendingPathComponent:targetName];
        NSError* copyError = nil;
        if ([fm copyItemAtPath:sourcePath toPath:targetPath error:&copyError]) {
            copied++;
            NSLog(@"[ARMSX2 iOS SaveState] memcard backup copied slot=%zu path=%@",
                  i + 1, targetPath);
        } else {
            NSLog(@"[ARMSX2 iOS SaveState] memcard backup failed slot=%zu source=%@ error=%@",
                  i + 1, sourcePath, copyError.localizedDescription ?: @"unknown");
        }
    }

    if (copied == 0) {
        [fm removeItemAtPath:backupDir error:nil];
    } else {
        ARMSX2PruneOldMemcardBackups(backupRoot, 6);
        NSLog(@"[ARMSX2 iOS SaveState] memcard backup complete reason=%s slot=%d copied=%ld dir=%@",
              reason ? reason : "state", stateSlot, static_cast<long>(copied), backupDir);
    }

    return copied;
}

static bool ARMSX2FlushNVRAMAndMemoryCards(const char* reason)
{
    cdvdSaveNVRAM();
    s_lastNVMSaveDate = [NSDate date];

    if (!VMManager::HasValidVM()) {
        NSLog(@"[ARMSX2Bridge] Save-state flush skipped memory cards reason=%s validVM=0",
              reason ? reason : "unknown");
        return true;
    }

    if (MemcardBusy::IsBusy()) {
        NSLog(@"[ARMSX2Bridge] Save-state flush blocked reason=%s memoryCardBusy=1",
              reason ? reason : "unknown");
        return false;
    }

    FileMcd_EmuClose();
    FileMcd_EmuOpen();
    NSLog(@"[ARMSX2Bridge] Save-state flush complete reason=%s nvmDate=%@",
          reason ? reason : "unknown", s_lastNVMSaveDate);
    return true;
}

static BOOL ARMSX2IsControllerSkinImageName(NSString* name)
{
    NSString* ext = name.pathExtension.lowercaseString;
    return [ext isEqualToString:@"png"] || [ext isEqualToString:@"jpg"] ||
           [ext isEqualToString:@"jpeg"] || [ext isEqualToString:@"webp"];
}

static NSString* ARMSX2ControllerSkinJSONImportKey(NSString* name)
{
    NSString* last = name.lastPathComponent.lowercaseString;
    if (last.length == 0 || ![last.pathExtension.lowercaseString isEqualToString:@"json"])
        return nil;

    return last;
}

static BOOL ARMSX2IsControllerSkinImportName(NSString* name, NSSet<NSString*>* allowedJSONNames)
{
    if (ARMSX2IsControllerSkinImageName(name))
        return YES;

    NSString* key = ARMSX2ControllerSkinJSONImportKey(name);
    return key.length > 0 && [allowedJSONNames containsObject:key];
}

static NSMutableSet<NSString*>* ARMSX2AllowedControllerSkinJSONNames(zip_t* zf, zip_int64_t count)
{
    NSMutableSet<NSString*>* allowedJSONNames = [NSMutableSet setWithObject:@"manifest.json"];
    for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(std::max<zip_int64_t>(count, 0)); i++) {
        zip_stat_t stat = {};
        if (zip_stat_index(zf, i, ZIP_FL_ENC_GUESS, &stat) != 0 || !stat.name)
            continue;

        NSString* entryName = [NSString stringWithUTF8String:stat.name];
        if (![ARMSX2ControllerSkinJSONImportKey(entryName) isEqualToString:@"manifest.json"])
            continue;

        auto file = zip_fopen_index_managed(zf, i, ZIP_FL_ENC_GUESS);
        if (!file)
            continue;

        std::optional<std::vector<u8>> data = ReadBinaryFileInZip(file.get());
        if (!data.has_value() || data->empty())
            continue;

        NSData* manifestData = [NSData dataWithBytes:data->data() length:data->size()];
        id manifestObject = [NSJSONSerialization JSONObjectWithData:manifestData options:0 error:nil];
        if (![manifestObject isKindOfClass:NSDictionary.class])
            continue;

        id layoutValue = [(NSDictionary*)manifestObject objectForKey:@"layout"];
        if (![layoutValue isKindOfClass:NSString.class])
            continue;

        NSString* layoutKey = ARMSX2ControllerSkinJSONImportKey((NSString*)layoutValue);
        if (layoutKey.length > 0)
            [allowedJSONNames addObject:layoutKey];
    }
    return allowedJSONNames;
}

static NSString* ARMSX2SanitizedSkinFileName(NSString* name)
{
    NSString* last = name.lastPathComponent;
    if (last.length == 0)
        return nil;

    NSMutableString* sanitized = [NSMutableString stringWithCapacity:last.length];
    NSCharacterSet* allowed = [NSCharacterSet characterSetWithCharactersInString:
        @"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-"];
    for (NSUInteger i = 0; i < last.length; i++) {
        unichar ch = [last characterAtIndex:i];
        [sanitized appendString:[allowed characterIsMember:ch] ? [NSString stringWithCharacters:&ch length:1] : @"_"];
    }
    return sanitized;
}

static void ARMSX2ApplyLiveGSBoolSetting(const char* section, const char* key, bool value)
{
    if (std::strcmp(section, "EmuCore/GS") != 0)
        return;

#define APPLY_OSD_BOOL(name) \
    do { \
        if (std::strcmp(key, #name) == 0) { \
            EmuConfig.GS.name = value; \
            GSConfig.name = value; \
            return; \
        } \
    } while (0)

    APPLY_OSD_BOOL(OsdShowFPS);
    APPLY_OSD_BOOL(OsdShowVPS);
    APPLY_OSD_BOOL(OsdShowSpeed);
    APPLY_OSD_BOOL(OsdShowCPU);
    APPLY_OSD_BOOL(OsdShowGPU);
    APPLY_OSD_BOOL(OsdShowResolution);
    APPLY_OSD_BOOL(OsdShowGSStats);
    APPLY_OSD_BOOL(OsdShowIndicators);
    APPLY_OSD_BOOL(OsdShowSettings);
    APPLY_OSD_BOOL(OsdShowInputs);
    APPLY_OSD_BOOL(OsdShowFrameTimes);
    APPLY_OSD_BOOL(OsdShowVersion);
    APPLY_OSD_BOOL(OsdShowHardwareInfo);
    APPLY_OSD_BOOL(OsdShowVideoCapture);
    APPLY_OSD_BOOL(OsdShowInputRec);
    APPLY_OSD_BOOL(DumpReplaceableTextures);
    APPLY_OSD_BOOL(DumpReplaceableMipmaps);
    APPLY_OSD_BOOL(DumpTexturesWithFMVActive);
    APPLY_OSD_BOOL(DumpDirectTextures);
    APPLY_OSD_BOOL(DumpPaletteTextures);
    APPLY_OSD_BOOL(LoadTextureReplacements);
    APPLY_OSD_BOOL(LoadTextureReplacementsAsync);
    APPLY_OSD_BOOL(PrecacheTextureReplacements);

    if (std::strcmp(key, "hw_mipmap") == 0) {
        EmuConfig.GS.HWMipmap = value;
        GSConfig.HWMipmap = value;
        return;
    }

#undef APPLY_OSD_BOOL
}

static void ARMSX2ApplyLiveGSIntSetting(const char* section, const char* key, int value)
{
    if (std::strcmp(section, "EmuCore/GS") != 0)
        return;

    if (std::strcmp(key, "OsdPerformancePos") == 0) {
        const int clamped = std::clamp(value, static_cast<int>(OsdOverlayPos::None), static_cast<int>(OsdOverlayPos::TopRight));
        EmuConfig.GS.OsdPerformancePos = static_cast<OsdOverlayPos>(clamped);
        GSConfig.OsdPerformancePos = static_cast<OsdOverlayPos>(clamped);
    } else if (std::strcmp(key, "OsdMessagesPos") == 0) {
        // Toggles the transient OSD message queue (shader-compilation, save,
        // settings-applied, etc.) without touching performance counters or the
        // separate SwiftUI alert path used for critical errors.
        const int clamped = std::clamp(value, static_cast<int>(OsdOverlayPos::None), static_cast<int>(OsdOverlayPos::TopRight));
        EmuConfig.GS.OsdMessagesPos = static_cast<OsdOverlayPos>(clamped);
        GSConfig.OsdMessagesPos = static_cast<OsdOverlayPos>(clamped);
    } else if (std::strcmp(key, "texture_preloading") == 0) {
        const int clamped = std::clamp(value, 0, static_cast<int>(TexturePreloadingLevel::Full));
        EmuConfig.GS.TexturePreloading = static_cast<TexturePreloadingLevel>(clamped);
        GSConfig.TexturePreloading = static_cast<TexturePreloadingLevel>(clamped);
    } else if (std::strcmp(key, "UserHacks_SkipDraw_Start") == 0) {
        EmuConfig.GS.SkipDrawStart = value;
        GSConfig.SkipDrawStart = value;
    } else if (std::strcmp(key, "UserHacks_SkipDraw_End") == 0) {
        EmuConfig.GS.SkipDrawEnd = std::max(EmuConfig.GS.SkipDrawStart, value);
        GSConfig.SkipDrawEnd = EmuConfig.GS.SkipDrawEnd;
    }
}

static void ARMSX2ApplyLiveTargetSpeedSetting(std::function<void()> update, const char* section, const char* key, float value)
{
    const std::string sectionName(section ? section : "");
    const std::string keyName(key ? key : "");

    if (!VMManager::HasValidVM()) {
        update();
        NSLog(@"[ARMSX2Bridge] target speed setting stored for next boot %s/%s=%0.3f",
              sectionName.c_str(), keyName.c_str(), value);
        return;
    }

    Host::RunOnCPUThread([update = std::move(update), sectionName, keyName, value]() mutable {
        update();
        VMManager::UpdateTargetSpeed();
        NSLog(@"[ARMSX2Bridge] target speed updated on CPU thread %s/%s=%0.3f",
              sectionName.c_str(), keyName.c_str(), value);
    }, false);
}

static float ARMSX2NormalizeIOSNominalScalar(float value)
{
    return std::isfinite(value) ? std::clamp(value, 0.05f, 10.0f) : 1.0f;
}

static float ARMSX2EnforceRetroAchievementsHardcoreFloatSetting(const char* section, const char* key, float value)
{
    if (!ARMSX2RetroAchievementsHardcoreActive())
        return value;

    if (std::strcmp(section, "Framerate") == 0) {
        if (std::strcmp(key, "NominalScalar") == 0 && value < 1.0f) {
            ARMSX2LogRetroAchievementsHardcoreBlock("slowdown_nominal_scalar");
            return 1.0f;
        }
        if (std::strcmp(key, "SlomoScalar") == 0 && value < 1.0f) {
            ARMSX2LogRetroAchievementsHardcoreBlock("slowdown_slomo_scalar");
            return 1.0f;
        }
    } else if (std::strcmp(section, "EmuCore/GS") == 0) {
        if (std::strcmp(key, "FramerateNTSC") == 0 &&
            std::fabs(value - Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_NTSC) > 0.001f) {
            ARMSX2LogRetroAchievementsHardcoreBlock("framerate_ntsc_override");
            return Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_NTSC;
        }
        if (std::strcmp(key, "FrameratePAL") == 0 &&
            std::fabs(value - Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_PAL) > 0.001f) {
            ARMSX2LogRetroAchievementsHardcoreBlock("framerate_pal_override");
            return Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_PAL;
        }
    }

    return value;
}

static bool ARMSX2ShouldBlockRetroAchievementsHardcoreBoolSetting(const char* section, const char* key, bool value)
{
    if (!value || !ARMSX2RetroAchievementsHardcoreActive())
        return false;

    if (std::strcmp(section, "EmuCore") == 0) {
        if (std::strcmp(key, "EnableCheats") == 0) {
            ARMSX2LogRetroAchievementsHardcoreBlock("enable_cheats");
            return true;
        }
        if (std::strcmp(key, "EnableRecordingTools") == 0 || std::strcmp(key, "EnablePINE") == 0) {
            ARMSX2LogRetroAchievementsHardcoreBlock(key);
            return true;
        }
    }

    return false;
}

static void ARMSX2ApplyLiveFloatSetting(const char* section, const char* key, float value)
{
    if (std::strcmp(section, "Framerate") == 0) {
        const float clamped = std::isfinite(value) ? std::clamp(value, 0.05f, 10.0f) : 1.0f;
        if (std::strcmp(key, "NominalScalar") == 0) {
            const float normalized = ARMSX2NormalizeIOSNominalScalar(value);
            if (std::fabs(normalized - clamped) > 0.001f)
                NSLog(@"[ARMSX2Bridge] clamping unsupported NominalScalar %.3f -> %.3f", clamped, normalized);
            ARMSX2ApplyLiveTargetSpeedSetting([normalized]() { EmuConfig.EmulationSpeed.NominalScalar = normalized; }, section, key, normalized);
        } else if (std::strcmp(key, "TurboScalar") == 0)
            ARMSX2ApplyLiveTargetSpeedSetting([clamped]() { EmuConfig.EmulationSpeed.TurboScalar = clamped; }, section, key, clamped);
        else if (std::strcmp(key, "SlomoScalar") == 0)
            ARMSX2ApplyLiveTargetSpeedSetting([clamped]() { EmuConfig.EmulationSpeed.SlomoScalar = clamped; }, section, key, clamped);
        else
            return;
        return;
    }

    if (std::strcmp(section, "EmuCore/GS") != 0)
        return;

    if (std::strcmp(key, "FramerateNTSC") == 0) {
        ARMSX2ApplyLiveTargetSpeedSetting([value]() { EmuConfig.GS.FramerateNTSC = value; }, section, key, value);
        return;
    }
    if (std::strcmp(key, "FrameratePAL") == 0) {
        ARMSX2ApplyLiveTargetSpeedSetting([value]() { EmuConfig.GS.FrameratePAL = value; }, section, key, value);
        return;
    }
    if (std::strcmp(key, "upscale_multiplier") == 0) {
        const float clamped = std::clamp(value, 0.25f, 8.0f);
        EmuConfig.GS.UpscaleMultiplier = clamped;
        GSConfig.UpscaleMultiplier = clamped;
        return;
    }
}

// Builds the per-game settings dictionary seeded with the current global values.
static NSMutableDictionary<NSString*, id>* ARMSX2BuildGlobalGameSettingsResult()
{
    const float globalUpscale = g_p44_settings_interface ? g_p44_settings_interface->GetFloatValue("EmuCore/GS", "upscale_multiplier", 1.0f) : 1.0f;
    const std::string globalAspect = g_p44_settings_interface ? g_p44_settings_interface->GetStringValue("EmuCore/GS", "AspectRatio", "Auto 4:3/3:2") : std::string("Auto 4:3/3:2");
    const int globalTextureFiltering = g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/GS", "filter", 2) : 2;
    const bool globalHardwareMipmapping = g_p44_settings_interface ? g_p44_settings_interface->GetBoolValue("EmuCore/GS", "hw_mipmap", true) : true;
    const int globalBlendingAccuracy = g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/GS", "accurate_blending_unit", 1) : 1;
    const int globalInterlaceMode = g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/GS", "deinterlace_mode", 7) : 7;
    const int globalTrilinearFiltering = g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/GS", "TriFilter", -1) : -1;
    const int globalHalfPixelOffset = g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/GS", "UserHacks_HalfPixelOffset", 0) : 0;
    const int globalRoundSprite = g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/GS", "UserHacks_round_sprite_offset", 0) : 0;
    const bool globalAlignSprite = g_p44_settings_interface ? g_p44_settings_interface->GetBoolValue("EmuCore/GS", "UserHacks_align_sprite_X", false) : false;
    const bool globalMergeSprite = g_p44_settings_interface ? g_p44_settings_interface->GetBoolValue("EmuCore/GS", "UserHacks_merge_pp_sprite", false) : false;
    const bool globalWildArmsOffset = g_p44_settings_interface ? g_p44_settings_interface->GetBoolValue("EmuCore/GS", "UserHacks_ForceEvenSpritePosition", false) : false;
    const int globalTextureOffsetX = g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/GS", "UserHacks_TCOffsetX", 0) : 0;
    const int globalTextureOffsetY = g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/GS", "UserHacks_TCOffsetY", 0) : 0;
    const int globalSkipDrawStart = g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/GS", "UserHacks_SkipDraw_Start", 0) : 0;
    const int globalSkipDrawEnd = g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/GS", "UserHacks_SkipDraw_End", 0) : 0;
    const bool globalEnableCheats = g_p44_settings_interface ? g_p44_settings_interface->GetBoolValue("EmuCore", "EnableCheats", false) : false;
    const bool globalEnablePatches = g_p44_settings_interface ? g_p44_settings_interface->GetBoolValue("EmuCore", "EnablePatches", true) : true;
    const bool globalEnableGameFixes = g_p44_settings_interface ? g_p44_settings_interface->GetBoolValue("EmuCore", "EnableGameFixes", true) : true;
    const bool globalEnableGameDBHardwareFixes = g_p44_settings_interface ? !g_p44_settings_interface->GetBoolValue("EmuCore/GS", "UserHacks", false) : true;
    const int globalEECoreType = g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/CPU", "CoreType", 2) : 2;
    const bool globalMTVU = g_p44_settings_interface ? g_p44_settings_interface->GetBoolValue("EmuCore/Speedhacks", "vuThread", true) : true;
    const int globalEECycleRate = ARMSX2ClampInt(
        g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("EmuCore/Speedhacks", "EECycleRate", 0) : 0,
        -3,
        3);
    const bool globalFastBoot = g_p44_settings_interface ?
        g_p44_settings_interface->GetBoolValue(
            "EmuCore", "EnableFastBoot",
            g_p44_settings_interface->GetBoolValue("GameISO", "FastBoot", false)) : false;
    const int globalVolumePercent = ARMSX2ClampInt(
        g_p44_settings_interface ? g_p44_settings_interface->GetIntValue("SPU2/Output", "StandardVolume", ARMSX2DefaultAudioVolumePercent) : ARMSX2DefaultAudioVolumePercent,
        0,
        ARMSX2DefaultAudioVolumePercent);
    return [@{
        @"enabled": @NO,
        @"path": @"",
        @"serial": @"",
        @"crc": @"",
        @"upscaleMultiplier": @(globalUpscale),
        @"aspectRatio": ARMSX2NSStringFromStdString(globalAspect),
        @"textureFiltering": @(globalTextureFiltering),
        @"hardwareMipmapping": @(globalHardwareMipmapping),
        @"blendingAccuracy": @(globalBlendingAccuracy),
        @"interlaceMode": @(globalInterlaceMode),
        @"trilinearFiltering": @(globalTrilinearFiltering),
        @"hasTrilinearFilteringOverride": @NO,
        @"halfPixelOffset": @(globalHalfPixelOffset),
        @"hasHalfPixelOffsetOverride": @NO,
        @"roundSprite": @(globalRoundSprite),
        @"hasRoundSpriteOverride": @NO,
        @"alignSprite": @(globalAlignSprite),
        @"hasAlignSpriteOverride": @NO,
        @"mergeSprite": @(globalMergeSprite),
        @"hasMergeSpriteOverride": @NO,
        @"wildArmsOffset": @(globalWildArmsOffset),
        @"hasWildArmsOffsetOverride": @NO,
        @"textureOffsetX": @(ARMSX2ClampInt(globalTextureOffsetX, -4096, 4096)),
        @"hasTextureOffsetXOverride": @NO,
        @"textureOffsetY": @(ARMSX2ClampInt(globalTextureOffsetY, -4096, 4096)),
        @"hasTextureOffsetYOverride": @NO,
        @"skipDrawStart": @(ARMSX2ClampInt(globalSkipDrawStart, 0, 5000)),
        @"hasSkipDrawStartOverride": @NO,
        @"skipDrawEnd": @(ARMSX2ClampInt(globalSkipDrawEnd, 0, 5000)),
        @"hasSkipDrawEndOverride": @NO,
        @"enableCheats": @(globalEnableCheats),
        @"enablePatches": @(globalEnablePatches),
        @"enableGameFixes": @(globalEnableGameFixes),
        @"enableGameDBHardwareFixes": @(globalEnableGameDBHardwareFixes),
        @"eeCoreType": @(globalEECoreType),
        @"mtvu": @(globalMTVU),
        @"globalEECycleRate": @(globalEECycleRate),
        @"eeCycleRate": @(globalEECycleRate),
        @"hasEECycleRateOverride": @NO,
        @"globalFastBoot": @(globalFastBoot),
        @"fastBoot": @(globalFastBoot),
        @"hasFastBootOverride": @NO,
        @"globalVolumePercent": @(globalVolumePercent),
        @"volumePercent": @(globalVolumePercent),
        @"hasVolumeOverride": @NO,
    } mutableCopy];
}

// Overlays per-game INI overrides for the given serial/crc onto a globals-seeded result.
// Sourcing serial/crc from the caller avoids re-scanning the disc image (which is unsafe
// while the VM is actively reading the same disc).
static void ARMSX2ApplyPerGameSettingsOverrides(NSMutableDictionary<NSString*, id>* result, const std::string& serial, u32 crc)
{
    const std::string settingsPath = VMManager::GetGameSettingsPath(serial, crc);
    result[@"path"] = ARMSX2NSStringFromStdString(settingsPath);
    result[@"serial"] = ARMSX2NSStringFromStdString(serial);
    result[@"crc"] = [NSString stringWithFormat:@"%08X", crc];

    INISettingsInterface si(settingsPath);
    if (!si.Load())
        return;

    if (si.ContainsValue("EmuCore/Speedhacks", "vuThread") &&
        !si.GetBoolValue("EmuCore/Speedhacks", "vuThread", true) &&
        (!si.GetBoolValue("ARMSX2iOS/PerGame", "ManualMTVU", false) ||
            si.GetIntValue("ARMSX2iOS/PerGame", "ManualMTVUVersion", 0) < 3)) {
        si.DeleteValue("ARMSX2iOS/PerGame", "ManualMTVU");
        si.DeleteValue("ARMSX2iOS/PerGame", "ManualMTVUVersion");
        si.DeleteValue("EmuCore/Speedhacks", "vuThread");
        Error saveError;
        const bool saved = si.Save(&saveError);
        std::fprintf(stderr, "@@IOS_PERGAME_MTVU_REPAIR@@ file=\"%s\" ui_read=1 removed_stale_false=1 saved=%d error=\"%s\"\n",
            settingsPath.c_str(), saved ? 1 : 0, saveError.GetDescription().c_str());
        std::fflush(stderr);
    }

    const bool hasKnownOverride =
        si.GetBoolValue("ARMSX2iOS/PerGame", "Enabled", false) ||
        si.ContainsValue("EmuCore/GS", "upscale_multiplier") ||
        si.ContainsValue("EmuCore/GS", "AspectRatio") ||
        si.ContainsValue("EmuCore/GS", "filter") ||
        si.ContainsValue("EmuCore/GS", "hw_mipmap") ||
        si.ContainsValue("EmuCore/GS", "accurate_blending_unit") ||
        si.ContainsValue("EmuCore/GS", "deinterlace_mode") ||
        si.ContainsValue("EmuCore/GS", "TriFilter") ||
        si.ContainsValue("EmuCore/GS", "UserHacks_HalfPixelOffset") ||
        si.ContainsValue("EmuCore/GS", "UserHacks_round_sprite_offset") ||
        si.ContainsValue("EmuCore/GS", "UserHacks_align_sprite_X") ||
        si.ContainsValue("EmuCore/GS", "UserHacks_merge_pp_sprite") ||
        si.ContainsValue("EmuCore/GS", "UserHacks_ForceEvenSpritePosition") ||
        si.ContainsValue("EmuCore/GS", "UserHacks_TCOffsetX") ||
        si.ContainsValue("EmuCore/GS", "UserHacks_TCOffsetY") ||
        si.ContainsValue("EmuCore/GS", "UserHacks_SkipDraw_Start") ||
        si.ContainsValue("EmuCore/GS", "UserHacks_SkipDraw_End") ||
        si.ContainsValue("EmuCore", "EnableCheats") ||
        si.ContainsValue("EmuCore", "EnablePatches") ||
        si.ContainsValue("EmuCore", "EnableGameFixes") ||
        si.ContainsValue("EmuCore/GS", "UserHacks") ||
        si.ContainsValue("EmuCore/CPU", "CoreType") ||
        si.ContainsValue("EmuCore/CPU", "UseArm64Dynarec") ||
        si.ContainsValue("EmuCore/Speedhacks", "vuThread") ||
        si.ContainsValue("EmuCore/Speedhacks", "EECycleRate") ||
        si.ContainsValue("EmuCore", "EnableFastBoot") ||
        si.ContainsValue("SPU2/Output", "StandardVolume") ||
        si.ContainsValue("SPU2/Output", "FastForwardVolume");

    result[@"enabled"] = @(hasKnownOverride);
    const bool hasStandardVolumeOverride = si.ContainsValue("SPU2/Output", "StandardVolume");
    const bool hasFastForwardVolumeOverride = si.ContainsValue("SPU2/Output", "FastForwardVolume");
    const bool hasVolumeOverride = hasStandardVolumeOverride || hasFastForwardVolumeOverride;
    const int inheritedVolumePercent = [result[@"volumePercent"] intValue];
    const int volumePercent = hasStandardVolumeOverride ?
        si.GetIntValue("SPU2/Output", "StandardVolume", inheritedVolumePercent) :
        (hasFastForwardVolumeOverride ? si.GetIntValue("SPU2/Output", "FastForwardVolume", inheritedVolumePercent) : inheritedVolumePercent);
    result[@"hasVolumeOverride"] = @(hasVolumeOverride);
    result[@"volumePercent"] = @(ARMSX2ClampInt(volumePercent, 0, ARMSX2DefaultAudioVolumePercent));
    NSString* currentAspect = [result[@"aspectRatio"] isKindOfClass:NSString.class] ? result[@"aspectRatio"] : @"Auto 4:3/3:2";
    result[@"upscaleMultiplier"] = @(si.GetFloatValue("EmuCore/GS", "upscale_multiplier", [result[@"upscaleMultiplier"] floatValue]));
    result[@"aspectRatio"] = ARMSX2NSStringFromStdString(si.GetStringValue("EmuCore/GS", "AspectRatio", currentAspect.UTF8String));
    result[@"textureFiltering"] = @(si.GetIntValue("EmuCore/GS", "filter", [result[@"textureFiltering"] intValue]));
    result[@"hardwareMipmapping"] = @(si.GetBoolValue("EmuCore/GS", "hw_mipmap", [result[@"hardwareMipmapping"] boolValue]));
    result[@"blendingAccuracy"] = @(si.GetIntValue("EmuCore/GS", "accurate_blending_unit", [result[@"blendingAccuracy"] intValue]));
    result[@"interlaceMode"] = @(si.GetIntValue("EmuCore/GS", "deinterlace_mode", [result[@"interlaceMode"] intValue]));
    result[@"hasTrilinearFilteringOverride"] = @(si.ContainsValue("EmuCore/GS", "TriFilter"));
    result[@"trilinearFiltering"] = @(ARMSX2ClampInt(si.GetIntValue("EmuCore/GS", "TriFilter", [result[@"trilinearFiltering"] intValue]), -1, 2));
    result[@"hasHalfPixelOffsetOverride"] = @(si.ContainsValue("EmuCore/GS", "UserHacks_HalfPixelOffset"));
    result[@"halfPixelOffset"] = @(ARMSX2ClampInt(si.GetIntValue("EmuCore/GS", "UserHacks_HalfPixelOffset", [result[@"halfPixelOffset"] intValue]), 0, 5));
    result[@"hasRoundSpriteOverride"] = @(si.ContainsValue("EmuCore/GS", "UserHacks_round_sprite_offset"));
    result[@"roundSprite"] = @(ARMSX2ClampInt(si.GetIntValue("EmuCore/GS", "UserHacks_round_sprite_offset", [result[@"roundSprite"] intValue]), 0, 2));
    result[@"hasAlignSpriteOverride"] = @(si.ContainsValue("EmuCore/GS", "UserHacks_align_sprite_X"));
    result[@"alignSprite"] = @(si.GetBoolValue("EmuCore/GS", "UserHacks_align_sprite_X", [result[@"alignSprite"] boolValue]));
    result[@"hasMergeSpriteOverride"] = @(si.ContainsValue("EmuCore/GS", "UserHacks_merge_pp_sprite"));
    result[@"mergeSprite"] = @(si.GetBoolValue("EmuCore/GS", "UserHacks_merge_pp_sprite", [result[@"mergeSprite"] boolValue]));
    result[@"hasWildArmsOffsetOverride"] = @(si.ContainsValue("EmuCore/GS", "UserHacks_ForceEvenSpritePosition"));
    result[@"wildArmsOffset"] = @(si.GetBoolValue("EmuCore/GS", "UserHacks_ForceEvenSpritePosition", [result[@"wildArmsOffset"] boolValue]));
    result[@"hasTextureOffsetXOverride"] = @(si.ContainsValue("EmuCore/GS", "UserHacks_TCOffsetX"));
    result[@"textureOffsetX"] = @(ARMSX2ClampInt(si.GetIntValue("EmuCore/GS", "UserHacks_TCOffsetX", [result[@"textureOffsetX"] intValue]), -4096, 4096));
    result[@"hasTextureOffsetYOverride"] = @(si.ContainsValue("EmuCore/GS", "UserHacks_TCOffsetY"));
    result[@"textureOffsetY"] = @(ARMSX2ClampInt(si.GetIntValue("EmuCore/GS", "UserHacks_TCOffsetY", [result[@"textureOffsetY"] intValue]), -4096, 4096));
    result[@"hasSkipDrawStartOverride"] = @(si.ContainsValue("EmuCore/GS", "UserHacks_SkipDraw_Start"));
    result[@"skipDrawStart"] = @(ARMSX2ClampInt(si.GetIntValue("EmuCore/GS", "UserHacks_SkipDraw_Start", [result[@"skipDrawStart"] intValue]), 0, 5000));
    result[@"hasSkipDrawEndOverride"] = @(si.ContainsValue("EmuCore/GS", "UserHacks_SkipDraw_End"));
    result[@"skipDrawEnd"] = @(ARMSX2ClampInt(si.GetIntValue("EmuCore/GS", "UserHacks_SkipDraw_End", [result[@"skipDrawEnd"] intValue]), 0, 5000));
    result[@"enableCheats"] = @(si.GetBoolValue("EmuCore", "EnableCheats", [result[@"enableCheats"] boolValue]));
    result[@"enablePatches"] = @(si.GetBoolValue("EmuCore", "EnablePatches", [result[@"enablePatches"] boolValue]));
    result[@"enableGameFixes"] = @(si.GetBoolValue("EmuCore", "EnableGameFixes", [result[@"enableGameFixes"] boolValue]));
    result[@"enableGameDBHardwareFixes"] = @(!si.GetBoolValue("EmuCore/GS", "UserHacks", ![result[@"enableGameDBHardwareFixes"] boolValue]));
    result[@"eeCoreType"] = @(si.GetIntValue("EmuCore/CPU", "CoreType", [result[@"eeCoreType"] intValue]));
    result[@"mtvu"] = @(si.GetBoolValue("EmuCore/Speedhacks", "vuThread", [result[@"mtvu"] boolValue]));
    result[@"hasEECycleRateOverride"] = @(si.ContainsValue("EmuCore/Speedhacks", "EECycleRate"));
    result[@"eeCycleRate"] = @(ARMSX2ClampInt(si.GetIntValue("EmuCore/Speedhacks", "EECycleRate", [result[@"eeCycleRate"] intValue]), -3, 3));
    result[@"hasFastBootOverride"] = @(si.ContainsValue("EmuCore", "EnableFastBoot"));
    result[@"fastBoot"] = @(si.GetBoolValue("EmuCore", "EnableFastBoot", [result[@"fastBoot"] boolValue]));

    // Per-game compatibility overrides. Returning these from the single INI that is
    // already loaded here lets the settings panel open without re-parsing the file
    // once per override key. Only present overrides are included; absent keys fall
    // back to the global value on the caller side.
    {
        NSMutableDictionary<NSString*, NSNumber*>* perGameFixes = [NSMutableDictionary dictionary];
        static constexpr const char* kARMSX2GameFixKeys[] = {
            "VuAddSubHack", "FpuMulHack", "XgKickHack", "EETimingHack", "InstantDMAHack",
            "SoftwareRendererFMVHack", "SkipMPEGHack", "OPHFlagHack", "DMABusyHack",
            "VIF1StallHack", "GIFFIFOHack", "GoemonTlbHack", "IbitHack", "VUSyncHack",
            "VUOverflowHack", "BlitInternalFPSHack", "FullVU0SyncHack"
        };
        for (const char* gameFixKey : kARMSX2GameFixKeys)
        {
            if (si.ContainsValue("EmuCore/Gamefixes", gameFixKey))
            {
                perGameFixes[[NSString stringWithUTF8String:gameFixKey]] =
                    @(si.GetBoolValue("EmuCore/Gamefixes", gameFixKey, false) ? 1 : 0);
            }
        }
        result[@"perGameFixes"] = perGameFixes;

        const bool hasPerGameAAT = si.ContainsValue("EmuCore/GS", "HWAccurateAlphaTest");
        result[@"hasPerGameAAT"] = @(hasPerGameAAT);
        result[@"perGameAAT"] = @((hasPerGameAAT && si.GetBoolValue("EmuCore/GS", "HWAccurateAlphaTest", false)) ? 1 : 0);

        const bool hasPerGameTextureInsideRt = si.ContainsValue("EmuCore/GS", "UserHacks_TextureInsideRt");
        result[@"hasPerGameTextureInsideRt"] = @(hasPerGameTextureInsideRt);
        result[@"perGameTextureInsideRt"] =
            @(hasPerGameTextureInsideRt ? si.GetIntValue("EmuCore/GS", "UserHacks_TextureInsideRt", 0) : 0);

        const bool hasPerGameRenderer = si.ContainsValue("EmuCore/GS", "Renderer");
        result[@"hasPerGameRenderer"] = @(hasPerGameRenderer);
        result[@"perGameRenderer"] = @(hasPerGameRenderer ? si.GetIntValue("EmuCore/GS", "Renderer", 17) : 17);

        const bool hasPerGameFXAA = si.ContainsValue("EmuCore/GS", "fxaa");
        result[@"hasPerGameFXAA"] = @(hasPerGameFXAA);
        result[@"perGameFXAA"] = @((hasPerGameFXAA && si.GetBoolValue("EmuCore/GS", "fxaa", false)) ? 1 : 0);

        const bool hasPerGameShadeBoost = si.ContainsValue("EmuCore/GS", "ShadeBoost");
        result[@"hasPerGameShadeBoost"] = @(hasPerGameShadeBoost);
        result[@"perGameShadeBoost"] = @((hasPerGameShadeBoost && si.GetBoolValue("EmuCore/GS", "ShadeBoost", false)) ? 1 : 0);

        const bool hasPerGameTVShader = si.ContainsValue("EmuCore/GS", "TVShader");
        result[@"hasPerGameTVShader"] = @(hasPerGameTVShader);
        result[@"perGameTVShader"] = @(hasPerGameTVShader ? si.GetIntValue("EmuCore/GS", "TVShader", 0) : 0);

        const bool hasPerGameCASMode = si.ContainsValue("EmuCore/GS", "CASMode");
        result[@"hasPerGameCASMode"] = @(hasPerGameCASMode);
        result[@"perGameCASMode"] = @(hasPerGameCASMode ? si.GetIntValue("EmuCore/GS", "CASMode", 0) : 0);

        // MetalFX Spatial upscaler (Off = 0, MetalFXSpatial = 1).
        const bool hasPerGameUpscaler = si.ContainsValue("EmuCore/GS", "Upscaler");
        result[@"hasPerGameUpscaler"] = @(hasPerGameUpscaler);
        result[@"perGameUpscaler"] = @(hasPerGameUpscaler ? si.GetIntValue("EmuCore/GS", "Upscaler", 0) : 0);

        const bool hasPerGameMaxAnisotropy = si.ContainsValue("EmuCore/GS", "MaxAnisotropy");
        result[@"hasPerGameMaxAnisotropy"] = @(hasPerGameMaxAnisotropy);
        result[@"perGameMaxAnisotropy"] = @(hasPerGameMaxAnisotropy ? si.GetIntValue("EmuCore/GS", "MaxAnisotropy", 0) : 0);

        const bool hasPerGameCASSharpness = si.ContainsValue("EmuCore/GS", "CASSharpness");
        result[@"hasPerGameCASSharpness"] = @(hasPerGameCASSharpness);
        result[@"perGameCASSharpness"] = @(hasPerGameCASSharpness ? si.GetIntValue("EmuCore/GS", "CASSharpness", 50) : 50);

        const bool hasPerGamePCRTCOffsets = si.ContainsValue("EmuCore/GS", "pcrtc_offsets");
        result[@"hasPerGamePCRTCOffsets"] = @(hasPerGamePCRTCOffsets);
        result[@"perGamePCRTCOffsets"] = @((hasPerGamePCRTCOffsets && si.GetBoolValue("EmuCore/GS", "pcrtc_offsets", false)) ? 1 : 0);

        const bool hasPerGameIntegerScaling = si.ContainsValue("EmuCore/GS", "IntegerScaling");
        result[@"hasPerGameIntegerScaling"] = @(hasPerGameIntegerScaling);
        result[@"perGameIntegerScaling"] = @((hasPerGameIntegerScaling && si.GetBoolValue("EmuCore/GS", "IntegerScaling", false)) ? 1 : 0);

        const bool hasPerGameSkipDupFrames = si.ContainsValue("EmuCore/GS", "SkipDuplicateFrames");
        result[@"hasPerGameSkipDupFrames"] = @(hasPerGameSkipDupFrames);
        result[@"perGameSkipDupFrames"] = @((hasPerGameSkipDupFrames && si.GetBoolValue("EmuCore/GS", "SkipDuplicateFrames", true)) ? 1 : 0);

        const bool hasPerGamePCRTCOverscan = si.ContainsValue("EmuCore/GS", "pcrtc_overscan");
        result[@"hasPerGamePCRTCOverscan"] = @(hasPerGamePCRTCOverscan);
        result[@"perGamePCRTCOverscan"] = @((hasPerGamePCRTCOverscan && si.GetBoolValue("EmuCore/GS", "pcrtc_overscan", false)) ? 1 : 0);

        const bool hasPerGamePCRTCAntiBlur = si.ContainsValue("EmuCore/GS", "pcrtc_antiblur");
        result[@"hasPerGamePCRTCAntiBlur"] = @(hasPerGamePCRTCAntiBlur);
        result[@"perGamePCRTCAntiBlur"] = @((hasPerGamePCRTCAntiBlur && si.GetBoolValue("EmuCore/GS", "pcrtc_antiblur", true)) ? 1 : 0);

        const bool hasPerGameDisableInterlaceOffset = si.ContainsValue("EmuCore/GS", "disable_interlace_offset");
        result[@"hasPerGameDisableInterlaceOffset"] = @(hasPerGameDisableInterlaceOffset);
        result[@"perGameDisableInterlaceOffset"] = @((hasPerGameDisableInterlaceOffset && si.GetBoolValue("EmuCore/GS", "disable_interlace_offset", false)) ? 1 : 0);
    }
}

static void ARMSX2WriteGameSettingsForIdentity(const std::string& serial,
                                                u32 crc,
                                                BOOL enabled,
                                                float upscaleMultiplier,
                                                NSString* aspectRatio,
                                                int textureFiltering,
                                                BOOL hardwareMipmapping,
                                                int blendingAccuracy,
                                                int interlaceMode,
                                                int trilinearFiltering,
                                                int halfPixelOffset,
                                                int roundSprite,
                                                BOOL alignSpriteOverride,
                                                BOOL alignSprite,
                                                BOOL mergeSpriteOverride,
                                                BOOL mergeSprite,
                                                BOOL wildArmsOffsetOverride,
                                                BOOL wildArmsOffset,
                                                BOOL textureOffsetXOverride,
                                                int textureOffsetX,
                                                BOOL textureOffsetYOverride,
                                                int textureOffsetY,
                                                BOOL skipDrawStartOverride,
                                                int skipDrawStart,
                                                BOOL skipDrawEndOverride,
                                                int skipDrawEnd,
                                                BOOL volumeOverride,
                                                int volumePercent,
                                                int eeCoreType,
                                                BOOL mtvu,
                                                BOOL eeCycleRateOverride,
                                                int eeCycleRate,
                                                BOOL fastBootOverride,
                                                BOOL fastBoot,
                                                BOOL enableCheats,
                                                BOOL enablePatches,
                                                BOOL enableGameFixes,
                                                BOOL enableGameDBHardwareFixes)
{
    FileSystem::CreateDirectoryPath(EmuFolders::GameSettings.c_str(), false);
    if (enableCheats && (ARMSX2RetroAchievementsHardcoreActive() || EmuConfig.Achievements.HardcoreMode)) {
        ARMSX2LogRetroAchievementsHardcoreBlock("per_game_enable_cheats");
        enableCheats = NO;
    }

    const std::string settingsPath = VMManager::GetGameSettingsPath(serial, crc);
    INISettingsInterface si(settingsPath);
    si.Load();

    if (enabled) {
        si.SetBoolValue("ARMSX2iOS/PerGame", "Enabled", true);
        si.SetFloatValue("EmuCore/GS", "upscale_multiplier", upscaleMultiplier);
        si.SetStringValue("EmuCore/GS", "AspectRatio", aspectRatio.UTF8String ?: "Auto 4:3/3:2");
        si.SetIntValue("EmuCore/GS", "filter", textureFiltering);
        si.SetBoolValue("EmuCore/GS", "hw_mipmap", hardwareMipmapping);
        si.SetIntValue("EmuCore/GS", "accurate_blending_unit", blendingAccuracy);
        si.SetIntValue("EmuCore/GS", "deinterlace_mode", interlaceMode);
        if (trilinearFiltering == ARMSX2TriFilterUseGlobalSentinel)
            si.DeleteValue("EmuCore/GS", "TriFilter");
        else
            si.SetIntValue("EmuCore/GS", "TriFilter", ARMSX2ClampInt(trilinearFiltering, -1, 2));

        if (halfPixelOffset == ARMSX2UseGlobalIntSentinel)
            si.DeleteValue("EmuCore/GS", "UserHacks_HalfPixelOffset");
        else
            si.SetIntValue("EmuCore/GS", "UserHacks_HalfPixelOffset", ARMSX2ClampInt(halfPixelOffset, 0, 5));

        if (roundSprite == ARMSX2UseGlobalIntSentinel)
            si.DeleteValue("EmuCore/GS", "UserHacks_round_sprite_offset");
        else
            si.SetIntValue("EmuCore/GS", "UserHacks_round_sprite_offset", ARMSX2ClampInt(roundSprite, 0, 2));

        if (alignSpriteOverride)
            si.SetBoolValue("EmuCore/GS", "UserHacks_align_sprite_X", alignSprite);
        else
            si.DeleteValue("EmuCore/GS", "UserHacks_align_sprite_X");

        if (mergeSpriteOverride)
            si.SetBoolValue("EmuCore/GS", "UserHacks_merge_pp_sprite", mergeSprite);
        else
            si.DeleteValue("EmuCore/GS", "UserHacks_merge_pp_sprite");

        if (wildArmsOffsetOverride)
            si.SetBoolValue("EmuCore/GS", "UserHacks_ForceEvenSpritePosition", wildArmsOffset);
        else
            si.DeleteValue("EmuCore/GS", "UserHacks_ForceEvenSpritePosition");

        if (textureOffsetXOverride)
            si.SetIntValue("EmuCore/GS", "UserHacks_TCOffsetX", ARMSX2ClampInt(textureOffsetX, -4096, 4096));
        else
            si.DeleteValue("EmuCore/GS", "UserHacks_TCOffsetX");

        if (textureOffsetYOverride)
            si.SetIntValue("EmuCore/GS", "UserHacks_TCOffsetY", ARMSX2ClampInt(textureOffsetY, -4096, 4096));
        else
            si.DeleteValue("EmuCore/GS", "UserHacks_TCOffsetY");

        if (skipDrawStartOverride)
            si.SetIntValue("EmuCore/GS", "UserHacks_SkipDraw_Start", ARMSX2ClampInt(skipDrawStart, 0, 5000));
        else
            si.DeleteValue("EmuCore/GS", "UserHacks_SkipDraw_Start");

        if (skipDrawEndOverride)
            si.SetIntValue("EmuCore/GS", "UserHacks_SkipDraw_End", ARMSX2ClampInt(skipDrawEnd, 0, 5000));
        else
            si.DeleteValue("EmuCore/GS", "UserHacks_SkipDraw_End");

        if (volumeOverride) {
            const int clampedVolumePercent = ARMSX2ClampInt(volumePercent, 0, ARMSX2DefaultAudioVolumePercent);
            si.SetIntValue("SPU2/Output", "StandardVolume", clampedVolumePercent);
            si.SetIntValue("SPU2/Output", "FastForwardVolume", clampedVolumePercent);
        } else {
            si.DeleteValue("SPU2/Output", "StandardVolume");
            si.DeleteValue("SPU2/Output", "FastForwardVolume");
        }

        si.SetBoolValue("EmuCore", "EnableCheats", enableCheats);
        si.SetBoolValue("EmuCore", "EnablePatches", enablePatches);
        si.SetBoolValue("EmuCore", "EnableGameFixes", enableGameFixes);
        si.SetBoolValue("EmuCore/GS", "UserHacks", !enableGameDBHardwareFixes);
        si.SetIntValue("EmuCore/CPU", "CoreType", eeCoreType);
        si.SetBoolValue("EmuCore/CPU", "UseArm64Dynarec", eeCoreType == 2);
        const bool globalMTVU = g_p44_settings_interface ?
            g_p44_settings_interface->GetBoolValue("EmuCore/Speedhacks", "vuThread", true) : true;
        if (mtvu == globalMTVU) {
            si.DeleteValue("ARMSX2iOS/PerGame", "ManualMTVU");
            si.DeleteValue("ARMSX2iOS/PerGame", "ManualMTVUVersion");
            si.DeleteValue("EmuCore/Speedhacks", "vuThread");
        } else {
            si.SetBoolValue("ARMSX2iOS/PerGame", "ManualMTVU", true);
            si.SetIntValue("ARMSX2iOS/PerGame", "ManualMTVUVersion", 3);
            si.SetBoolValue("EmuCore/Speedhacks", "vuThread", mtvu);
        }

        if (eeCycleRateOverride) {
            int clampedEECycleRate = ARMSX2ClampInt(eeCycleRate, -3, 3);
            if (ARMSX2RetroAchievementsHardcoreActive() && clampedEECycleRate < 0) {
                ARMSX2LogRetroAchievementsHardcoreBlock("per_game_ee_underclock");
                clampedEECycleRate = 0;
            }
            si.SetIntValue("EmuCore/Speedhacks", "EECycleRate", clampedEECycleRate);
        } else {
            si.DeleteValue("EmuCore/Speedhacks", "EECycleRate");
        }

        if (fastBootOverride)
            si.SetBoolValue("EmuCore", "EnableFastBoot", fastBoot);
        else
            si.DeleteValue("EmuCore", "EnableFastBoot");
    } else {
        si.DeleteValue("ARMSX2iOS/PerGame", "Enabled");
        si.DeleteValue("EmuCore/GS", "upscale_multiplier");
        si.DeleteValue("EmuCore/GS", "AspectRatio");
        si.DeleteValue("EmuCore/GS", "filter");
        si.DeleteValue("EmuCore/GS", "hw_mipmap");
        si.DeleteValue("EmuCore/GS", "accurate_blending_unit");
        si.DeleteValue("EmuCore/GS", "deinterlace_mode");
        si.DeleteValue("EmuCore/GS", "TriFilter");
        si.DeleteValue("EmuCore/GS", "UserHacks_HalfPixelOffset");
        si.DeleteValue("EmuCore/GS", "UserHacks_round_sprite_offset");
        si.DeleteValue("EmuCore/GS", "UserHacks_align_sprite_X");
        si.DeleteValue("EmuCore/GS", "UserHacks_merge_pp_sprite");
        si.DeleteValue("EmuCore/GS", "UserHacks_ForceEvenSpritePosition");
        si.DeleteValue("EmuCore/GS", "UserHacks_TCOffsetX");
        si.DeleteValue("EmuCore/GS", "UserHacks_TCOffsetY");
        si.DeleteValue("EmuCore/GS", "UserHacks_SkipDraw_Start");
        si.DeleteValue("EmuCore/GS", "UserHacks_SkipDraw_End");
        si.DeleteValue("EmuCore", "EnableCheats");
        si.DeleteValue("EmuCore", "EnablePatches");
        si.DeleteValue("EmuCore", "EnableGameFixes");
        si.DeleteValue("EmuCore/GS", "UserHacks");
        si.DeleteValue("EmuCore/CPU", "CoreType");
        si.DeleteValue("EmuCore/CPU", "UseArm64Dynarec");
        si.DeleteValue("ARMSX2iOS/PerGame", "ManualMTVU");
        si.DeleteValue("ARMSX2iOS/PerGame", "ManualMTVUVersion");
        si.DeleteValue("EmuCore/Speedhacks", "vuThread");
        si.DeleteValue("EmuCore/Speedhacks", "EECycleRate");
        si.DeleteValue("EmuCore", "EnableFastBoot");
        si.DeleteValue("SPU2/Output", "StandardVolume");
        si.DeleteValue("SPU2/Output", "FastForwardVolume");
        si.RemoveEmptySections();
    }

    Error error;
    const bool saved = si.Save(&error);
    NSLog(@"[ARMSX2Bridge] Game settings %@ serial=%@ crc=%08X path=%@ result=%d",
          enabled ? @"saved" : @"cleared", ARMSX2NSStringFromStdString(serial),
          crc, ARMSX2NSStringFromStdString(settingsPath), saved ? 1 : 0);
    if (!saved)
        NSLog(@"[ARMSX2Bridge] Game settings save error: %@", ARMSX2NSStringFromStdString(error.GetDescription()));
}

static NSArray<NSString*>* ARMSX2PatchEnableListForIdentity(const std::string& serial, u32 crc,
                                                             NSString* section, NSString* key)
{
    if (serial.empty() || crc == 0)
        return @[];

    const std::string settingsPath = VMManager::GetGameSettingsPath(serial, crc);
    INISettingsInterface si(settingsPath);
    if (!si.Load())
        return @[];

    const std::vector<std::string> values = si.GetStringList(section.UTF8String ?: "", key.UTF8String ?: "");
    NSMutableArray<NSString*>* result = [NSMutableArray arrayWithCapacity:values.size()];
    for (const std::string& value : values)
    {
        NSString* name = ARMSX2NSStringFromStdString(value);
        if (name.length > 0)
            [result addObject:name];
    }
    return result;
}

static void ARMSX2SetPatchEnableListForIdentity(NSArray<NSString*>* values, const std::string& serial, u32 crc,
                                                NSString* section, NSString* key)
{
    if (serial.empty() || crc == 0)
        return;

    FileSystem::CreateDirectoryPath(EmuFolders::GameSettings.c_str(), false);
    const std::string settingsPath = VMManager::GetGameSettingsPath(serial, crc);
    INISettingsInterface si(settingsPath);
    si.Load();

    std::vector<std::string> list;
    list.reserve(values.count);
    for (NSString* value in values)
    {
        if (value.length > 0)
            list.push_back(value.UTF8String);
    }

    if (list.empty())
        si.DeleteValue(section.UTF8String ?: "", key.UTF8String ?: "");
    else
        si.SetStringList(section.UTF8String ?: "", key.UTF8String ?: "", list);

    Error error;
    si.Save(&error);
    // NSLog(@"[ARMSX2Bridge] Patch enable list saved serial=%@ crc=%08X section=%@ count=%lu",
    //       ARMSX2NSStringFromStdString(serial), (unsigned int)crc, section, (unsigned long)list.size());
}

// Resolve a per-game identity (serial, crc) for a library ISO. Returns NO if the ISO
// cannot be resolved or has no CRC. ELFs have no serial (empty), matching the
// game-settings writer.
static BOOL ARMSX2PerGameIdentityForISO(NSString* isoName, std::string* serial, u32* crc)
{
    GameList::Entry entry;
    NSString* resolvedPath = nil;
    if (!ARMSX2PopulateGameListEntryForISO(isoName, &entry, &resolvedPath) || entry.crc == 0)
        return NO;
    *serial = (entry.type == GameList::EntryType::ELF) ? std::string() : entry.serial;
    *crc = entry.crc;
    return YES;
}

// Resolve a per-game identity for the running game. Returns NO with no valid VM/CRC.
static BOOL ARMSX2PerGameIdentityForCurrentGame(std::string* serial, u32* crc)
{
    if (!VMManager::HasValidVM())
        return NO;
    *serial = VMManager::GetSerialForGameSettings();
    *crc = VMManager::GetDiscCRC();
    return *crc != 0;
}

static std::string ARMSX2PerGameSettingsPath(const std::string& serial, u32 crc)
{
    FileSystem::CreateDirectoryPath(EmuFolders::GameSettings.c_str(), false);
    return VMManager::GetGameSettingsPath(serial, crc);
}

@implementation ARMSX2Bridge

+ (UIView *)gameRenderView {
    extern UIView* g_gameRenderView;
    return g_gameRenderView;
}

+ (void)prepareGameRenderViewForCurrentRenderer {
    ARMSX2_PrepareGameRenderViewForCurrentRenderer("swift_preboot");
}

+ (void)saveNVRAM {
    cdvdSaveNVRAM();
    s_lastNVMSaveDate = [NSDate date];
    NSLog(@"[ARMSX2Bridge] NVM saved at %@", s_lastNVMSaveDate);
}

+ (void)saveMemoryCards {
    Host::RunOnCPUThread([]() {
        const bool flushed = ARMSX2FlushNVRAMAndMemoryCards("manual-save-memory-cards");
        NSLog(@"[ARMSX2Bridge] Memory card save requested result=%d", flushed ? 1 : 0);
    }, false);
}

+ (void)saveAllState {
    [self saveNVRAM];
    [self saveMemoryCards];
}

+ (BOOL)isRunning {
    return VMManager::GetState() == VMState::Running;
}

+ (void)setPadButton:(ARMSX2PadButton)button pressed:(BOOL)pressed {
    auto* pad = static_cast<PadDualshock2*>(Pad::GetPad(0, 0));
    if (!pad) return;

    static const u32 buttonMap[] = {
        PadDualshock2::Inputs::PAD_UP,       // Up
        PadDualshock2::Inputs::PAD_DOWN,     // Down
        PadDualshock2::Inputs::PAD_LEFT,     // Left
        PadDualshock2::Inputs::PAD_RIGHT,    // Right
        PadDualshock2::Inputs::PAD_CROSS,    // Cross
        PadDualshock2::Inputs::PAD_CIRCLE,   // Circle
        PadDualshock2::Inputs::PAD_SQUARE,   // Square
        PadDualshock2::Inputs::PAD_TRIANGLE, // Triangle
        PadDualshock2::Inputs::PAD_L1,       // L1
        PadDualshock2::Inputs::PAD_R1,       // R1
        PadDualshock2::Inputs::PAD_L2,       // L2
        PadDualshock2::Inputs::PAD_R2,       // R2
        PadDualshock2::Inputs::PAD_START,    // Start
        PadDualshock2::Inputs::PAD_SELECT,   // Select
        PadDualshock2::Inputs::PAD_L3,       // L3
        PadDualshock2::Inputs::PAD_R3,       // R3
    };

    if ((int)button < (int)(sizeof(buttonMap)/sizeof(buttonMap[0]))) {
        u32 idx = buttonMap[(int)button];
        pad->Set(idx, pressed ? 1.0f : 0.0f);
        // Update touch state so PumpMessagesOnCPUThread doesn't override
        extern bool g_touchPadState[64];
        if (idx < 64) g_touchPadState[idx] = pressed;
    }
}

+ (void)setLeftStickX:(float)x Y:(float)y {
    auto* pad = static_cast<PadDualshock2*>(Pad::GetPad(0, 0));
    if (!pad) return;
    // Convert axis (-1..+1) to individual direction values (0..1)
    const float right = x > 0 ? x : 0.0f;
    const float left = x < 0 ? -x : 0.0f;
    const float down = y > 0 ? y : 0.0f;
    const float up = y < 0 ? -y : 0.0f;
    pad->Set(PadDualshock2::Inputs::PAD_L_RIGHT, right);
    pad->Set(PadDualshock2::Inputs::PAD_L_LEFT, left);
    pad->Set(PadDualshock2::Inputs::PAD_L_DOWN, down);
    pad->Set(PadDualshock2::Inputs::PAD_L_UP, up);
    extern bool g_touchPadState[64];
    g_touchPadState[PadDualshock2::Inputs::PAD_L_RIGHT] = right > 0.01f;
    g_touchPadState[PadDualshock2::Inputs::PAD_L_LEFT] = left > 0.01f;
    g_touchPadState[PadDualshock2::Inputs::PAD_L_DOWN] = down > 0.01f;
    g_touchPadState[PadDualshock2::Inputs::PAD_L_UP] = up > 0.01f;
}

+ (void)setRightStickX:(float)x Y:(float)y {
    auto* pad = static_cast<PadDualshock2*>(Pad::GetPad(0, 0));
    if (!pad) return;
    const float right = x > 0 ? x : 0.0f;
    const float left = x < 0 ? -x : 0.0f;
    const float down = y > 0 ? y : 0.0f;
    const float up = y < 0 ? -y : 0.0f;
    pad->Set(PadDualshock2::Inputs::PAD_R_RIGHT, right);
    pad->Set(PadDualshock2::Inputs::PAD_R_LEFT, left);
    pad->Set(PadDualshock2::Inputs::PAD_R_DOWN, down);
    pad->Set(PadDualshock2::Inputs::PAD_R_UP, up);
    extern bool g_touchPadState[64];
    g_touchPadState[PadDualshock2::Inputs::PAD_R_RIGHT] = right > 0.01f;
    g_touchPadState[PadDualshock2::Inputs::PAD_R_LEFT] = left > 0.01f;
    g_touchPadState[PadDualshock2::Inputs::PAD_R_DOWN] = down > 0.01f;
    g_touchPadState[PadDualshock2::Inputs::PAD_R_UP] = up > 0.01f;
}

+ (nonnull NSString *)biosName {
    return @"PS2";
}

+ (void)requestVMStop {
    extern std::atomic<bool> s_requestVMStop;
    s_requestVMStop.store(true);
    NSLog(@"[ARMSX2Bridge] VM stop requested");
}

+ (void)setVMPaused:(BOOL)paused {
    const bool hasValidVM = VMManager::HasValidVM();
    std::fprintf(stderr, "@@IOS_VM_PAUSE_REQUEST@@ paused=%d valid=%d block=0 state=%d\n",
        paused ? 1 : 0, hasValidVM ? 1 : 0,
        hasValidVM ? static_cast<int>(VMManager::GetState()) : -1);
    std::fflush(stderr);

    if (!hasValidVM)
        return;

    Host::RunOnCPUThread([paused]() {
        if (!VMManager::HasValidVM())
            return;

        const VMState state = VMManager::GetState();
        if (paused && state == VMState::Running) {
            VMManager::SetPaused(true);
            Console.WriteLn("@@IOS_VM_PAUSE@@ paused=1 reason=swiftui-menu");
        } else if (!paused && state == VMState::Paused) {
            VMManager::SetPaused(false);
            Console.WriteLn("@@IOS_VM_PAUSE@@ paused=0 reason=swiftui-menu");
        }
        Console.WriteLn("@@IOS_VM_PAUSE_APPLY@@ requested=%d before=%d after=%d",
            paused ? 1 : 0, static_cast<int>(state), static_cast<int>(VMManager::GetState()));
    }, false);
}

+ (void)setFullScreen:(BOOL)enabled {
    ARMSX2_SetSDLFullscreen(enabled ? true : false);
}

+ (BOOL)isSDLFullscreen {
    return ARMSX2_IsSDLFullscreen() ? YES : NO;
}

+ (nonnull NSString *)buildVersion {
    NSString *ver = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"] ?: @"?";
    return [NSString stringWithFormat:@"ARMSX2 iOS v%@", ver];
}

+ (BOOL)isJITAvailable {
    return DarwinMisc::IsJITAvailable();
}

+ (BOOL)isNoJITFallbackActive {
    return DarwinMisc::iPSX2_FORCE_EE_INTERP != 0;
}

+ (nonnull NSArray<NSURL *> *)extractControllerSkinArchiveAtURL:(nonnull NSURL *)archiveURL
                                                    toDirectory:(nonnull NSURL *)destinationDirectory {
    static const zip_uint64_t kMaxSkinArchiveEntryBytes = 16 * 1024 * 1024;
    static const NSUInteger kMaxSkinArchiveEntries = 64;
    static const zip_int64_t kMaxSkinArchiveTotalEntries = 512;

    NSMutableArray<NSURL *> *extracted = [NSMutableArray array];
    if (!archiveURL.isFileURL || !destinationDirectory.isFileURL)
        return extracted;

    NSError *directoryError = nil;
    if (![[NSFileManager defaultManager] createDirectoryAtURL:destinationDirectory
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:&directoryError]) {
        NSLog(@"[ARMSX2 iOS Skins] Could not create extraction directory %@: %@",
              destinationDirectory.path, directoryError.localizedDescription);
        return extracted;
    }

    zip_error_t ze = {};
    auto zf = zip_open_managed(archiveURL.path.UTF8String, ZIP_RDONLY, &ze);
    if (!zf) {
        NSLog(@"[ARMSX2 iOS Skins] Could not open skin archive %@: %s",
              archiveURL.lastPathComponent, zip_error_strerror(&ze));
        return extracted;
    }

    const zip_int64_t count = zip_get_num_entries(zf.get(), 0);
    if (count > kMaxSkinArchiveTotalEntries) {
        NSLog(@"[ARMSX2 iOS Skins] Skin archive has too many entries (%lld); skipping %@.",
              static_cast<long long>(count), archiveURL.lastPathComponent);
        return extracted;
    }
    NSSet<NSString*>* allowedJSONNames = ARMSX2AllowedControllerSkinJSONNames(zf.get(), count);
    for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(std::max<zip_int64_t>(count, 0)); i++) {
        if (extracted.count >= kMaxSkinArchiveEntries)
            break;

        zip_stat_t stat = {};
        if (zip_stat_index(zf.get(), i, ZIP_FL_ENC_GUESS, &stat) != 0 || !stat.name)
            continue;
        if ((stat.valid & ZIP_STAT_SIZE) && stat.size > kMaxSkinArchiveEntryBytes)
            continue;

        NSString *entryName = [NSString stringWithUTF8String:stat.name];
        if (entryName.length == 0 || [entryName hasSuffix:@"/"] || !ARMSX2IsControllerSkinImportName(entryName, allowedJSONNames))
            continue;

        auto file = zip_fopen_index_managed(zf.get(), i, ZIP_FL_ENC_GUESS);
        if (!file)
            continue;

        std::optional<std::vector<u8>> data = ReadBinaryFileInZip(file.get());
        if (!data.has_value() || data->empty())
            continue;

        NSString *safeName = ARMSX2SanitizedSkinFileName(entryName);
        if (safeName.length == 0)
            continue;

        NSURL *destinationURL = [destinationDirectory URLByAppendingPathComponent:safeName];
        NSData *imageData = [NSData dataWithBytes:data->data() length:data->size()];
        if ([imageData writeToURL:destinationURL atomically:YES])
            [extracted addObject:destinationURL];
    }

    NSLog(@"[ARMSX2 iOS Skins] Extracted %lu skin file(s) from %@",
          static_cast<unsigned long>(extracted.count), archiveURL.lastPathComponent);
    return extracted;
}

+ (nullable NSData *)peekSkinManifestDataAtURL:(NSURL *)archiveURL {
    if (!archiveURL.isFileURL) {
        return nil;
    }

    zip_error_t ze = {};
    auto zf = zip_open_managed(archiveURL.path.UTF8String, ZIP_RDONLY, &ze);
    if (!zf) {
        return nil;
    }

    const zip_int64_t count = zip_get_num_entries(zf.get(), 0);
    if (count > 512) {
        return nil;
    }

    // Prefer info.json, then manifest.json. Read raw bytes without extracting.
    for (NSString* wanted in @[@"info.json", @"manifest.json"]) {
        for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(std::max<zip_int64_t>(count, 0)); i++) {
            zip_stat_t stat = {};
            if (zip_stat_index(zf.get(), i, ZIP_FL_ENC_GUESS, &stat) != 0 || !stat.name) {
                continue;
            }
            NSString* entryName = [NSString stringWithUTF8String:stat.name];
            if (entryName.length == 0 || [entryName hasSuffix:@"/"]) {
                continue;
            }
            if ([entryName containsString:@"__MACOSX"] || [entryName containsString:@".."]) {
                continue;
            }
            if (![entryName.lastPathComponent.lowercaseString isEqualToString:wanted]) {
                continue;
            }
            if ((stat.valid & ZIP_STAT_SIZE) && stat.size > 16 * 1024 * 1024) {
                continue;
            }
            auto file = zip_fopen_index_managed(zf.get(), i, ZIP_FL_ENC_GUESS);
            if (!file) {
                continue;
            }
            std::optional<std::vector<u8>> data = ReadBinaryFileInZip(file.get());
            if (!data.has_value() || data->empty()) {
                continue;
            }
            return [NSData dataWithBytes:data->data() length:data->size()];
        }
    }
    return nil;
}

+ (nonnull NSArray<NSURL *> *)extractSkinPackageArchiveAtURL:(NSURL *)archiveURL
                                                  toDirectory:(NSURL *)destinationDirectory {
    static const zip_uint64_t kMaxPackageEntryBytes = 16 * 1024 * 1024;
    static const NSUInteger kMaxPackageExtractedEntries = 128;
    static const zip_int64_t kMaxPackageTotalEntries = 512;
    static NSArray<NSString*>* kAllowedPackageExtensions;

    static dispatch_once_t once;
    dispatch_once(&once, ^{
        kAllowedPackageExtensions = @[@"png", @"jpg", @"jpeg", @"webp", @"pdf", @"json"];
    });

    NSMutableArray<NSURL*>* extracted = [NSMutableArray array];
    if (!archiveURL.isFileURL || !destinationDirectory.isFileURL) {
        return extracted;
    }

    NSError* directoryError = nil;
    if (![[NSFileManager defaultManager] createDirectoryAtURL:destinationDirectory
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:&directoryError]) {
        NSLog(@"[ARMSX2 iOS Skins] Could not create package directory %@: %@",
              destinationDirectory.path, directoryError.localizedDescription);
        return extracted;
    }

    zip_error_t ze = {};
    auto zf = zip_open_managed(archiveURL.path.UTF8String, ZIP_RDONLY, &ze);
    if (!zf) {
        NSLog(@"[ARMSX2 iOS Skins] Could not open skin package %@: %s",
              archiveURL.lastPathComponent, zip_error_strerror(&ze));
        return extracted;
    }

    const zip_int64_t count = zip_get_num_entries(zf.get(), 0);
    if (count > kMaxPackageTotalEntries) {
        NSLog(@"[ARMSX2 iOS Skins] Skin package has too many entries (%lld); skipping %@.",
              static_cast<long long>(count), archiveURL.lastPathComponent);
        return extracted;
    }

    NSString* basePath = destinationDirectory.path;
    NSCharacterSet* allowed = [NSCharacterSet characterSetWithCharactersInString:
        @"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-"];

    for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(std::max<zip_int64_t>(count, 0)); i++) {
        if (extracted.count >= kMaxPackageExtractedEntries) {
            break;
        }

        zip_stat_t stat = {};
        if (zip_stat_index(zf.get(), i, ZIP_FL_ENC_GUESS, &stat) != 0 || !stat.name) {
            continue;
        }
        if ((stat.valid & ZIP_STAT_SIZE) && stat.size > kMaxPackageEntryBytes) {
            continue;
        }

        NSString* entryName = [NSString stringWithUTF8String:stat.name];
        if (entryName.length == 0 || [entryName hasSuffix:@"/"]) {
            continue;
        }
        if ([entryName containsString:@"__MACOSX"]) {
            continue;
        }

        NSString* extension = entryName.pathExtension.lowercaseString;
        if (extension.length == 0 || ![kAllowedPackageExtensions containsObject:extension]) {
            continue;
        }

        // Build a safe relative path: reject any absolute/".."/"."/hidden
        // component, then sanitize each component to filesystem-safe characters.
        NSArray* components = [entryName componentsSeparatedByString:@"/"];
        BOOL rejected = NO;
        for (NSString* component in components) {
            if ([component isEqualToString:@".."] || [component isEqualToString:@"."] || [component hasPrefix:@"."]) {
                rejected = YES;
                break;
            }
        }
        if (rejected) {
            continue;
        }

        NSURL* destinationURL = destinationDirectory;
        for (NSString* component in components) {
            NSMutableString* sanitized = [NSMutableString stringWithCapacity:component.length];
            for (NSUInteger c = 0; c < component.length; c++) {
                unichar ch = [component characterAtIndex:c];
                [sanitized appendString:[allowed characterIsMember:ch] ? [NSString stringWithCharacters:&ch length:1] : @"_"];
            }
            if (sanitized.length > 0) {
                destinationURL = [destinationURL URLByAppendingPathComponent:sanitized];
            }
        }

        // Defense-in-depth against path traversal: the resolved path must
        // remain inside the destination directory.
        NSString* resolvedPath = destinationURL.path;
        if (![resolvedPath hasPrefix:[basePath stringByAppendingString:@"/"]]) {
            continue;
        }

        auto file = zip_fopen_index_managed(zf.get(), i, ZIP_FL_ENC_GUESS);
        if (!file) {
            continue;
        }
        std::optional<std::vector<u8>> data = ReadBinaryFileInZip(file.get());
        if (!data.has_value() || data->empty()) {
            continue;
        }

        NSString* parentPath = destinationURL.URLByDeletingLastPathComponent.path;
        [[NSFileManager defaultManager] createDirectoryAtPath:parentPath
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:nil];
        NSData* nsdata = [NSData dataWithBytes:data->data() length:data->size()];
        if ([nsdata writeToURL:destinationURL atomically:YES]) {
            [extracted addObject:destinationURL];
        }
    }

    NSLog(@"[ARMSX2 iOS Skins] Extracted %lu package file(s) from %@",
          static_cast<unsigned long>(extracted.count), archiveURL.lastPathComponent);
    return extracted;
}

+ (nullable NSString *)extractMemoryCardArchiveAtURL:(nonnull NSURL *)archiveURL {
    static const zip_uint64_t kMaxMemcardEntryBytes = 128 * 1024 * 1024;
    static const zip_int64_t kMaxMemcardTotalEntries = 64;

    if (!archiveURL.isFileURL) {
        return nil;
    }

    zip_error_t ze = {};
    auto zf = zip_open_managed(archiveURL.path.UTF8String, ZIP_RDONLY, &ze);
    if (!zf) {
        NSLog(@"[ARMSX2 iOS Memcards] Could not open archive %@: %s",
              archiveURL.lastPathComponent, zip_error_strerror(&ze));
        return nil;
    }

    const zip_int64_t count = zip_get_num_entries(zf.get(), 0);
    if (count > kMaxMemcardTotalEntries) {
        NSLog(@"[ARMSX2 iOS Memcards] Archive has too many entries (%lld); skipping %@.",
              static_cast<long long>(count), archiveURL.lastPathComponent);
        return nil;
    }

    NSString *memcardDir = [self memoryCardDirectory];
    for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(std::max<zip_int64_t>(count, 0)); i++) {
        zip_stat_t stat = {};
        if (zip_stat_index(zf.get(), i, ZIP_FL_ENC_GUESS, &stat) != 0 || !stat.name)
            continue;
        if ((stat.valid & ZIP_STAT_SIZE) && stat.size > kMaxMemcardEntryBytes)
            continue;

        NSString *entryName = [NSString stringWithUTF8String:stat.name];
        if (entryName.length == 0 || [entryName hasSuffix:@"/"])
            continue;
        if ([entryName containsString:@"__MACOSX"] || [entryName containsString:@".."])
            continue;
        if (![entryName.pathExtension.lowercaseString isEqualToString:@"ps2"])
            continue;

        NSString *safeName = entryName.lastPathComponent;
        if ([safeName hasPrefix:@"."])
            continue;

        auto file = zip_fopen_index_managed(zf.get(), i, ZIP_FL_ENC_GUESS);
        if (!file)
            continue;
        std::optional<std::vector<u8>> data = ReadBinaryFileInZip(file.get());
        if (!data.has_value() || data->empty())
            continue;

        NSString *destinationPath = [memcardDir stringByAppendingPathComponent:safeName];
        if (![destinationPath hasPrefix:[memcardDir stringByAppendingString:@"/"]])
            continue;

        NSData *nsdata = [NSData dataWithBytes:data->data() length:data->size()];
        if ([nsdata writeToFile:destinationPath atomically:YES]) {
            NSLog(@"[ARMSX2 iOS Memcards] Extracted %@ from %@", safeName, archiveURL.lastPathComponent);
            return safeName;
        }
    }

    NSLog(@"[ARMSX2 iOS Memcards] No .ps2 memory card found in %@",
          archiveURL.lastPathComponent);
    return nil;
}

+ (nullable NSString *)currentISOPath {
    NSString *docsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
    NSString *iniPath = [docsPath stringByAppendingPathComponent:@"ARMSX2-iOS.ini"];
    if (![[NSFileManager defaultManager] fileExistsAtPath:iniPath])
        iniPath = [docsPath stringByAppendingPathComponent:@"PCSX2-iOS.ini"];
    // Read BootISO from INI
    FILE *f = fopen(iniPath.UTF8String, "r");
    if (!f) return nil;
    char line[512];
    bool inSection = false;
    NSString *result = nil;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "[GameISO]")) { inSection = true; continue; }
        if (line[0] == '[') { inSection = false; continue; }
        if (inSection && strstr(line, "BootISO")) {
            char *eq = strchr(line, '=');
            if (eq) {
                eq++;
                while (*eq == ' ') eq++;
                // Remove trailing newline
                char *nl = strchr(eq, '\n'); if (nl) *nl = 0;
                char *cr = strchr(eq, '\r'); if (cr) *cr = 0;
                if (strlen(eq) > 0) result = [NSString stringWithUTF8String:eq];
            }
        }
    }
    fclose(f);
    return result;
}

+ (nullable NSString *)currentGameISOName {
    if (VMManager::HasValidVM()) {
        const std::string discPath = VMManager::GetDiscPath();
        if (!discPath.empty()) {
            NSString *fileName = ARMSX2NSStringFromStringView(Path::GetFileName(discPath));
            if (fileName.length > 0)
                return fileName;
        }
    }

    NSString *currentPath = [self currentISOPath];
    return currentPath.length > 0 ? currentPath.lastPathComponent : nil;
}

+ (nonnull NSString *)isoDirectory {
    NSString *docsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
    NSString *isoDir = [docsPath stringByAppendingPathComponent:@"iso"];
    [[NSFileManager defaultManager] createDirectoryAtPath:isoDir withIntermediateDirectories:YES attributes:nil error:nil];
    return isoDir;
}

+ (nonnull NSString *)documentsDirectory {
    return [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
}

+ (nonnull NSArray<NSString *> *)availableISOs {
	NSFileManager *fm = [NSFileManager defaultManager];
	NSMutableSet *seen = [NSMutableSet set];
	NSMutableArray *isos = [NSMutableArray array];

    // Helper block: scan a directory for ISO files
    void (^scanDir)(NSString *) = ^(NSString *dir) {
        NSArray *files = [fm contentsOfDirectoryAtPath:dir error:nil];
        for (NSString *file in files) {
            if ([seen containsObject:file]) continue;
            NSString *ext = file.pathExtension.lowercaseString;
            if ([ext isEqualToString:@"iso"] || [ext isEqualToString:@"img"] || [ext isEqualToString:@"chd"] ||
                [ext isEqualToString:@"cso"] || [ext isEqualToString:@"zso"] || [ext isEqualToString:@"gz"] ||
                [ext isEqualToString:@"elf"]) {
                [isos addObject:file];
                [seen addObject:file];
            } else if ([ext isEqualToString:@"bin"]) {
// .bin > 50MB treated as game image
                NSString *fullPath = [dir stringByAppendingPathComponent:file];
                NSDictionary *attrs = [fm attributesOfItemAtPath:fullPath error:nil];
                if ([attrs fileSize] > 50 * 1024 * 1024) {
                    [isos addObject:file];
                    [seen addObject:file];
                }
            }
        }
    };

    ARMSX2EnumerateLocalGameImages([self isoDirectory], ^(NSString* absolutePath, NSString* relativeName) {
        if ([seen containsObject:relativeName]) return;
        [isos addObject:relativeName];
        [seen addObject:relativeName];
    });
    NSString *docsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
    scanDir(docsPath);

	return isos;
}

+ (nonnull NSArray<NSDictionary<NSString *, id> *> *)availableISOEntries {
	NSFileManager* fm = [NSFileManager defaultManager];
	NSMutableSet<NSString*>* seenPaths = [NSMutableSet set];
	NSMutableArray<NSDictionary<NSString*, id>*>* entries = [NSMutableArray array];

	void (^addPathWithName)(NSString*, NSString*, BOOL, NSString*, BOOL) = ^(NSString* path, NSString* displayName, BOOL external, NSString* source, BOOL forceGameFile) {
		if (path.length == 0 || (!forceGameFile && !ARMSX2IsSupportedGameImageAtPath(path)))
			return;

		NSString* normalizedPath = path.stringByStandardizingPath;
		NSString* entryName = displayName.length > 0 ? displayName : normalizedPath.lastPathComponent;
		NSString* key = normalizedPath.lowercaseString;
		if ([seenPaths containsObject:key])
			return;

		[seenPaths addObject:key];
		[entries addObject:@{
			@"name": entryName ?: @"",
			@"path": normalizedPath,
			@"external": @(external),
			@"source": source ?: (external ? @"External" : @"On My iPhone"),
		}];
	};

	void (^addPath)(NSString*, BOOL, NSString*) = ^(NSString* path, BOOL external, NSString* source) {
		addPathWithName(path, nil, external, source, NO);
	};

	void (^scanLocalDir)(NSString*, NSString*) = ^(NSString* dir, NSString* source) {
		NSArray<NSString*>* files = [fm contentsOfDirectoryAtPath:dir error:nil];
		for (NSString* file in files) {
			addPath([dir stringByAppendingPathComponent:file], NO, source);
		}
	};

	ARMSX2EnumerateLocalGameImages([self isoDirectory], ^(NSString* absolutePath, NSString* relativeName) {
		addPathWithName(absolutePath, relativeName, NO, @"On My iPhone", YES);
	});
	scanLocalDir([self documentsDirectory], @"On My iPhone");

	for (NSDictionary* record in ARMSX2ExternalGameDirectoryRecords()) {
		NSURL* directoryURL = ARMSX2ResolveExternalGameDirectoryRecord(record);
		if (!directoryURL)
			continue;
		if (ARMSX2ExternalGameRecordIsCloudProvider(record, directoryURL)) {
			NSLog(@"[ARMSX2Bridge] External game cloud provider list entry skipped path=%@", directoryURL.path);
			continue;
		}

		BOOL isDirectory = ARMSX2ExternalGameRecordIsDirectory(record, directoryURL);
		if (isDirectory && ARMSX2ExternalGameRecordScanDisabled(record, directoryURL)) {
			NSLog(@"[ARMSX2Bridge] External game folder scan disabled path=%@", directoryURL.path);
			continue;
		}

		BOOL alreadyActive = ARMSX2ExternalGameAccessAlreadyActive(directoryURL.path);
		BOOL startedAccess = alreadyActive ? NO : [directoryURL startAccessingSecurityScopedResource];

		NSString* source = [record[@"displayName"] isKindOfClass:NSString.class] ? record[@"displayName"] : directoryURL.lastPathComponent;
		if (!isDirectory) {
			addPathWithName(directoryURL.path, source, YES, source, YES);
			if (startedAccess)
				[directoryURL stopAccessingSecurityScopedResource];
			continue;
		}

		NSError* contentsError = nil;
		NSArray<NSURL*>* urls = [fm contentsOfDirectoryAtURL:directoryURL
		                         includingPropertiesForKeys:@[NSURLIsRegularFileKey, NSURLIsDirectoryKey]
		                                            options:NSDirectoryEnumerationSkipsHiddenFiles
		                                              error:&contentsError];
		if (!urls) {
			NSLog(@"[ARMSX2Bridge] External game folder scan failed path=%@ error=%@",
			      directoryURL.path, contentsError.localizedDescription ?: @"");
			if (startedAccess)
				[directoryURL stopAccessingSecurityScopedResource];
			continue;
		}

		for (NSURL* url in urls) {
			NSNumber* isDirectory = nil;
			if ([url getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil] && isDirectory.boolValue)
				continue;

			addPath(url.path, YES, source);
		}

		if (startedAccess)
			[directoryURL stopAccessingSecurityScopedResource];
	}

	return entries;
}

+ (nonnull NSDictionary<NSString *, NSString *> *)gameMetadataForISO:(nonnull NSString *)isoName {
	if (isoName.length == 0)
		return @{};

	NSFileManager *fm = [NSFileManager defaultManager];
	NSString *path = ARMSX2ResolveISOPath(isoName);
	NSString *fileName = path.length > 0 ? path.lastPathComponent : isoName.lastPathComponent;

	NSMutableDictionary<NSString *, NSString *> *metadata = [NSMutableDictionary dictionary];
	metadata[@"fileTitle"] = fileName.stringByDeletingPathExtension ?: fileName;

	if (![fm fileExistsAtPath:path]) {
		return metadata;
	}

    GameList::Entry entry;
    if (GameList::PopulateEntryFromPath(path.UTF8String, &entry)) {
        NSString *title = ARMSX2NSStringFromStdString(entry.GetTitle(false));
        NSString *serial = ARMSX2NSStringFromStdString(entry.serial);
        const char *regionText = GameList::RegionToString(entry.region, false);
        NSString *region = (regionText && *regionText) ? @(regionText) : nil;
        if (!region || [region isEqualToString:@"Other"]) {
            NSString *fallbackRegion = ARMSX2RegionFallbackForSerial(entry.serial);
            if (fallbackRegion.length > 0)
                region = fallbackRegion;
        }

        if (title.length > 0)
            metadata[@"title"] = title;
        if (serial.length > 0)
            metadata[@"serial"] = serial;
        if (region.length > 0)
            metadata[@"region"] = region;
        if (entry.crc != 0)
            metadata[@"crc"] = [NSString stringWithFormat:@"%08X", entry.crc];

        NSLog(@"[ARMSX2 iOS Covers] metadata %@ title=%@ serial=%@ region=%@",
              isoName, metadata[@"title"] ?: @"", metadata[@"serial"] ?: @"", metadata[@"region"] ?: @"");
    } else {
        NSLog(@"[ARMSX2 iOS Covers] metadata unavailable %@", isoName);
    }

    return metadata;
}

+ (nonnull NSDictionary<NSString *, id> *)gameSettingsForISO:(nonnull NSString *)isoName {
    NSMutableDictionary<NSString*, id>* result = ARMSX2BuildGlobalGameSettingsResult();

    GameList::Entry entry;
    NSString* resolvedPath = nil;
    if (!ARMSX2PopulateGameListEntryForISO(isoName, &entry, &resolvedPath) || entry.crc == 0) {
        NSLog(@"[ARMSX2Bridge] Game settings unavailable for %@ path=%@", isoName, resolvedPath ?: @"");
        return result;
    }

    const std::string settingsSerial = (entry.type == GameList::EntryType::ELF) ? std::string() : entry.serial;
    ARMSX2ApplyPerGameSettingsOverrides(result, settingsSerial, entry.crc);
    return result;
}

// VM-safe per-game settings for the running title. Reads the serial/crc the VM already
// holds in memory instead of re-scanning the disc image, which is what previously
// disturbed audio/loading when the runtime panel was opened over an active game.
+ (nullable NSDictionary<NSString *, id> *)gameSettingsForCurrentGame {
    if (!VMManager::HasValidVM())
        return nil;

    NSMutableDictionary<NSString*, id>* result = ARMSX2BuildGlobalGameSettingsResult();
    const std::string serial = VMManager::GetSerialForGameSettings();
    const u32 crc = VMManager::GetDiscCRC();
    if (serial.empty() && crc == 0)
        return result;

    ARMSX2ApplyPerGameSettingsOverrides(result, serial, crc);
    return result;
}

+ (void)setGameSettingsForISO:(nonnull NSString *)isoName
                       enabled:(BOOL)enabled
             upscaleMultiplier:(float)upscaleMultiplier
                   aspectRatio:(nonnull NSString *)aspectRatio
              textureFiltering:(int)textureFiltering
            hardwareMipmapping:(BOOL)hardwareMipmapping
              blendingAccuracy:(int)blendingAccuracy
               interlaceMode:(int)interlaceMode
        trilinearFiltering:(int)trilinearFiltering
          halfPixelOffset:(int)halfPixelOffset
              roundSprite:(int)roundSprite
      alignSpriteOverride:(BOOL)alignSpriteOverride
              alignSprite:(BOOL)alignSprite
      mergeSpriteOverride:(BOOL)mergeSpriteOverride
              mergeSprite:(BOOL)mergeSprite
    wildArmsOffsetOverride:(BOOL)wildArmsOffsetOverride
           wildArmsOffset:(BOOL)wildArmsOffset
    textureOffsetXOverride:(BOOL)textureOffsetXOverride
           textureOffsetX:(int)textureOffsetX
    textureOffsetYOverride:(BOOL)textureOffsetYOverride
           textureOffsetY:(int)textureOffsetY
     skipDrawStartOverride:(BOOL)skipDrawStartOverride
            skipDrawStart:(int)skipDrawStart
       skipDrawEndOverride:(BOOL)skipDrawEndOverride
              skipDrawEnd:(int)skipDrawEnd
         volumeOverride:(BOOL)volumeOverride
           volumePercent:(int)volumePercent
                    eeCoreType:(int)eeCoreType
                          mtvu:(BOOL)mtvu
           eeCycleRateOverride:(BOOL)eeCycleRateOverride
                   eeCycleRate:(int)eeCycleRate
               fastBootOverride:(BOOL)fastBootOverride
                       fastBoot:(BOOL)fastBoot
                  enableCheats:(BOOL)enableCheats
                 enablePatches:(BOOL)enablePatches
              enableGameFixes:(BOOL)enableGameFixes
    enableGameDBHardwareFixes:(BOOL)enableGameDBHardwareFixes {
    GameList::Entry entry;
    NSString* resolvedPath = nil;
    if (!ARMSX2PopulateGameListEntryForISO(isoName, &entry, &resolvedPath) || entry.crc == 0) {
        NSLog(@"[ARMSX2Bridge] Game settings save rejected for %@ path=%@", isoName, resolvedPath ?: @"");
        return;
    }

    const std::string settingsSerial = (entry.type == GameList::EntryType::ELF) ? std::string() : entry.serial;
    ARMSX2WriteGameSettingsForIdentity(settingsSerial, entry.crc, enabled, upscaleMultiplier, aspectRatio,
                                        textureFiltering, hardwareMipmapping, blendingAccuracy, interlaceMode,
                                        trilinearFiltering, halfPixelOffset, roundSprite, alignSpriteOverride,
                                        alignSprite, mergeSpriteOverride, mergeSprite, wildArmsOffsetOverride,
                                        wildArmsOffset, textureOffsetXOverride, textureOffsetX,
                                        textureOffsetYOverride, textureOffsetY, skipDrawStartOverride,
                                        skipDrawStart, skipDrawEndOverride, skipDrawEnd,
                                        volumeOverride, volumePercent, eeCoreType, mtvu,
                                        eeCycleRateOverride, eeCycleRate, fastBootOverride, fastBoot,
                                        enableCheats, enablePatches, enableGameFixes, enableGameDBHardwareFixes);
}

+ (void)setGameSettingsForCurrentGameWithEnabled:(BOOL)enabled
                               upscaleMultiplier:(float)upscaleMultiplier
                                     aspectRatio:(nonnull NSString *)aspectRatio
                                textureFiltering:(int)textureFiltering
                              hardwareMipmapping:(BOOL)hardwareMipmapping
                                blendingAccuracy:(int)blendingAccuracy
                                   interlaceMode:(int)interlaceMode
                              trilinearFiltering:(int)trilinearFiltering
                                 halfPixelOffset:(int)halfPixelOffset
                                     roundSprite:(int)roundSprite
                             alignSpriteOverride:(BOOL)alignSpriteOverride
                                     alignSprite:(BOOL)alignSprite
                             mergeSpriteOverride:(BOOL)mergeSpriteOverride
                                     mergeSprite:(BOOL)mergeSprite
                           wildArmsOffsetOverride:(BOOL)wildArmsOffsetOverride
                                  wildArmsOffset:(BOOL)wildArmsOffset
                           textureOffsetXOverride:(BOOL)textureOffsetXOverride
                                  textureOffsetX:(int)textureOffsetX
                           textureOffsetYOverride:(BOOL)textureOffsetYOverride
                                  textureOffsetY:(int)textureOffsetY
                            skipDrawStartOverride:(BOOL)skipDrawStartOverride
                                   skipDrawStart:(int)skipDrawStart
                              skipDrawEndOverride:(BOOL)skipDrawEndOverride
                                     skipDrawEnd:(int)skipDrawEnd
                                   volumeOverride:(BOOL)volumeOverride
                                     volumePercent:(int)volumePercent
                                      eeCoreType:(int)eeCoreType
                                            mtvu:(BOOL)mtvu
                             eeCycleRateOverride:(BOOL)eeCycleRateOverride
                                     eeCycleRate:(int)eeCycleRate
                                 fastBootOverride:(BOOL)fastBootOverride
                                         fastBoot:(BOOL)fastBoot
                                    enableCheats:(BOOL)enableCheats
                                   enablePatches:(BOOL)enablePatches
                                 enableGameFixes:(BOOL)enableGameFixes
                      enableGameDBHardwareFixes:(BOOL)enableGameDBHardwareFixes {
    if (!VMManager::HasValidVM()) {
        NSLog(@"[ARMSX2Bridge] Current game settings save rejected: no valid VM");
        return;
    }

    const std::string serial = VMManager::GetSerialForGameSettings();
    const u32 crc = VMManager::GetDiscCRC();
    if (crc == 0) {
        NSLog(@"[ARMSX2Bridge] Current game settings save rejected serial=%@ crc=%08X",
              ARMSX2NSStringFromStdString(serial), crc);
        return;
    }

    ARMSX2WriteGameSettingsForIdentity(serial, crc, enabled, upscaleMultiplier, aspectRatio,
                                        textureFiltering, hardwareMipmapping, blendingAccuracy, interlaceMode,
                                        trilinearFiltering, halfPixelOffset, roundSprite, alignSpriteOverride,
                                        alignSprite, mergeSpriteOverride, mergeSprite, wildArmsOffsetOverride,
                                        wildArmsOffset, textureOffsetXOverride, textureOffsetX,
                                        textureOffsetYOverride, textureOffsetY, skipDrawStartOverride,
                                        skipDrawStart, skipDrawEndOverride, skipDrawEnd,
                                        volumeOverride, volumePercent, eeCoreType, mtvu,
                                        eeCycleRateOverride, eeCycleRate, fastBootOverride, fastBoot,
                                        enableCheats, enablePatches, enableGameFixes, enableGameDBHardwareFixes);

    if (VMManager::HasValidVM()) {
        VMManager::ReloadGameSettings();
        if (MTGS::IsOpen())
            MTGS::ApplySettings();
    }
}

+ (nullable NSString *)linkedDiscPathForELF:(nonnull NSString *)elfName {
    NSString* resolvedPath = ARMSX2ResolveISOPath(elfName);
    if (resolvedPath.length == 0)
        return nil;

    const std::string discPath = VMManager::GetDiscOverrideFromGameSettings(resolvedPath.UTF8String);
    return discPath.empty() ? nil : ARMSX2NSStringFromStdString(discPath);
}

+ (void)setLinkedDiscPath:(nullable NSString *)discPath forELF:(nonnull NSString *)elfName {
    GameList::Entry entry;
    if (!ARMSX2PopulateGameListEntryForISO(elfName, &entry, nil) || entry.crc == 0)
        return;

    FileSystem::CreateDirectoryPath(EmuFolders::GameSettings.c_str(), false);
    INISettingsInterface si(VMManager::GetGameSettingsPath(std::string_view(), entry.crc));
    si.Load();

    NSString* trimmed = [discPath stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (trimmed.length > 0)
    {
        NSString* root = [ARMSX2NSStringFromStdString(EmuFolders::DataRoot).stringByStandardizingPath stringByAppendingString:@"/"];
        NSString* full = trimmed.stringByStandardizingPath;
        NSString* rel = [full hasPrefix:root] ? [full substringFromIndex:root.length] : trimmed;
        si.SetStringValue("EmuCore", "DiscPath", rel.UTF8String);
    }
    else
    {
        si.DeleteValue("EmuCore", "DiscPath");
    }

    si.Save();
}

+ (nonnull NSString *)clearCacheForISO:(nonnull NSString *)isoName {
    GameList::Entry entry;
    NSString* resolvedPath = nil;
    if (!ARMSX2PopulateGameListEntryForISO(isoName, &entry, &resolvedPath) || entry.crc == 0) {
        NSLog(@"[ARMSX2Bridge] Clear cache unavailable for %@ path=%@", isoName, resolvedPath ?: @"");
        return @"Cache not cleared: game identity was not found.";
    }

    NSArray<NSString*>* tokens = ARMSX2GameDataTokensForEntry(isoName, entry);
    NSInteger removed = 0;
    removed += ARMSX2RemoveMatchingGeneratedFiles(ARMSX2NSStringFromStdString(EmuFolders::Cache), tokens);
    removed += ARMSX2RemoveMatchingGeneratedFiles(NSTemporaryDirectory(), tokens);

    NSLog(@"[ARMSX2Bridge] Clear cache iso=%@ serial=%@ crc=%08X removed=%ld",
          isoName, ARMSX2NSStringFromStdString(entry.serial), entry.crc, (long)removed);
    return [NSString stringWithFormat:@"Cleared %ld generated cache item%@ for %@.",
            (long)removed, removed == 1 ? @"" : @"s", isoName.stringByDeletingPathExtension ?: isoName];
}

+ (nonnull NSString *)deleteGameDataForISO:(nonnull NSString *)isoName {
    GameList::Entry entry;
    NSString* resolvedPath = nil;
    if (!ARMSX2PopulateGameListEntryForISO(isoName, &entry, &resolvedPath) || entry.crc == 0) {
        NSLog(@"[ARMSX2Bridge] Delete game data unavailable for %@ path=%@", isoName, resolvedPath ?: @"");
        return @"Game data was not deleted: game identity was not found.";
    }

    NSInteger removed = 0;
    auto removePath = [&removed](const std::string& path) {
        if (path.empty() || !FileSystem::FileExists(path.c_str()))
            return;
        if (FileSystem::DeleteFilePath(path.c_str()))
            removed++;
    };

    for (s32 slot = -1; slot <= VMManager::NUM_SAVE_STATE_SLOTS; slot++) {
        removePath(VMManager::GetSaveStateFileName(entry.serial.c_str(), entry.crc, slot));
        removePath(VMManager::GetSaveStateFileName(entry.serial.c_str(), entry.crc, slot, true));
    }

    removePath(Patch::GetPnachFilename(entry.serial, entry.crc, true));
    removePath(Patch::GetPnachFilename(entry.serial, entry.crc, false));
    removePath(VMManager::GetGameSettingsPath(entry.serial, entry.crc));

    NSString* identity = ARMSX2CompatibilityIdentityKey(ARMSX2NSStringFromStdString(entry.serial), entry.crc);
    if (g_p44_settings_interface && identity.length > 0) {
        g_p44_settings_interface->DeleteValue("ARMSX2/JITBisectGamePresets", identity.UTF8String);
        ARMSX2ClearCompatibilityCustomFlagsForIdentity(identity);
        g_p44_settings_interface->Save();
    }

    NSArray<NSString*>* tokens = ARMSX2GameDataTokensForEntry(isoName, entry);
    removed += ARMSX2RemoveMatchingGeneratedFiles(ARMSX2NSStringFromStdString(EmuFolders::Cache), tokens);
    removed += ARMSX2RemoveMatchingGeneratedFiles(NSTemporaryDirectory(), tokens);

    NSLog(@"[ARMSX2Bridge] Delete game data iso=%@ serial=%@ crc=%08X removed=%ld",
          isoName, ARMSX2NSStringFromStdString(entry.serial), entry.crc, (long)removed);
    return [NSString stringWithFormat:@"Deleted %ld game-data item%@ for %@. Memory card contents were left intact.",
            (long)removed, removed == 1 ? @"" : @"s", isoName.stringByDeletingPathExtension ?: isoName];
}

+ (BOOL)deleteISO:(nonnull NSString *)isoName deleteGameData:(BOOL)deleteGameData {
    NSString* isoPath = ARMSX2ResolveISOPath(isoName);
    if (isoPath.length == 0)
        return NO;

    if (deleteGameData)
        [self deleteGameDataForISO:isoName];

    NSError* error = nil;
    BOOL removed = [[NSFileManager defaultManager] removeItemAtPath:isoPath error:&error];
    NSLog(@"[ARMSX2Bridge] Delete ISO iso=%@ path=%@ result=%d error=%@",
          isoName, isoPath, removed ? 1 : 0, error.localizedDescription ?: @"");
    return removed;
}

+ (void)changeDiscToISO:(nonnull NSString *)isoName completion:(nullable ARMSX2SaveStateCompletion)completion {
    ARMSX2SaveStateCompletion callback = [completion copy];
    NSString* isoPath = ARMSX2ResolveISOPath(isoName);
    if (!isoPath || !VMManager::HasValidVM()) {
        NSLog(@"[ARMSX2Bridge] ChangeDisc rejected iso=%@ path=%@ validVM=%d", isoName, isoPath ?: @"", VMManager::HasValidVM() ? 1 : 0);
        if (callback)
            dispatch_async(dispatch_get_main_queue(), ^{ callback(NO); });
        return;
    }

    const std::string nativePath(isoPath.UTF8String ?: "");
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        bool ejectResult = false;
        bool result = false;
        Host::RunOnCPUThread([&result]() {
            const CDVD_SourceType oldSource = CDVDsys_GetSourceType();
            const std::string oldPathForLog = CDVDsys_GetFile(oldSource);
            NSLog(@"[ARMSX2Bridge] ChangeDisc eject phase oldSource=%d oldPath=%@",
                  static_cast<int>(oldSource), ARMSX2NSStringFromStdString(oldPathForLog));
            result = VMManager::ChangeDisc(CDVD_SourceType::NoDisc, {});
        }, true);
        ejectResult = result;

        [NSThread sleepForTimeInterval:1.25];

        Host::RunOnCPUThread([nativePath, &result]() {
            result = VMManager::ChangeDisc(CDVD_SourceType::Iso, nativePath);
            NSLog(@"[ARMSX2Bridge] ChangeDisc insert phase newSource=%d newPath=%@ result=%d",
                  static_cast<int>(CDVDsys_GetSourceType()),
                  ARMSX2NSStringFromStdString(CDVDsys_GetFile(CDVDsys_GetSourceType())),
                  result ? 1 : 0);
        }, true);

        if (result && g_p44_settings_interface) {
            g_p44_settings_interface->SetStringValue("GameISO", "BootISO", isoName.UTF8String);
            g_p44_settings_interface->Save();
        }

        ARMSX2_PostRuntimeMenuStateChanged();
        NSLog(@"[ARMSX2Bridge] ChangeDisc iso=%@ ejectResult=%d result=%d", isoName, ejectResult ? 1 : 0, result ? 1 : 0);
        if (callback)
            dispatch_async(dispatch_get_main_queue(), ^{ callback(result ? YES : NO); });
    });
}

+ (void)ejectDiscWithCompletion:(nullable ARMSX2SaveStateCompletion)completion {
    ARMSX2SaveStateCompletion callback = [completion copy];
    if (!VMManager::HasValidVM()) {
        NSLog(@"[ARMSX2Bridge] EjectDisc rejected validVM=0");
        if (callback)
            dispatch_async(dispatch_get_main_queue(), ^{ callback(NO); });
        return;
    }

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        bool result = false;
        Host::RunOnCPUThread([&result]() {
            result = VMManager::ChangeDisc(CDVD_SourceType::NoDisc, {});
        }, true);

        ARMSX2_PostRuntimeMenuStateChanged();
        NSLog(@"[ARMSX2Bridge] EjectDisc result=%d", result ? 1 : 0);
        if (callback)
            dispatch_async(dispatch_get_main_queue(), ^{ callback(result ? YES : NO); });
    });
}

// Toggle overlay visibility via position (None vs TopRight).
// Individual OSD flags are controlled by preset in SettingsStore, not here.
+ (void)setPerformanceOverlayVisible:(BOOL)visible {
    if (visible) {
        GSConfig.OsdPerformancePos = EmuConfig.GS.OsdPerformancePos;
        // If user had None in config, default to TopRight
        if (GSConfig.OsdPerformancePos == OsdOverlayPos::None) {
            GSConfig.OsdPerformancePos = OsdOverlayPos::TopRight;
            EmuConfig.GS.OsdPerformancePos = OsdOverlayPos::TopRight;
        }
    } else {
        GSConfig.OsdPerformancePos = OsdOverlayPos::None;
        EmuConfig.GS.OsdPerformancePos = OsdOverlayPos::None;
    }

    if (g_p44_settings_interface) {
        g_p44_settings_interface->SetIntValue("EmuCore/GS", "OsdPerformancePos",
            static_cast<int>(EmuConfig.GS.OsdPerformancePos));
        g_p44_settings_interface->Save();
    }
}

+ (BOOL)isPerformanceOverlayVisible {
    return GSConfig.OsdPerformancePos != OsdOverlayPos::None;
}

+ (nonnull NSDictionary<NSString *, id> *)deviceStatsForAccessibility {
    int battery = -1, severity = 0;
    double ramGB = 0.0;
    bool lowPower = false;
    ARMSX2_iOSCopyDeviceStats(&battery, &severity, &ramGB, &lowPower);
    NSString* thermal;
    switch (severity) {
        case 2:  thermal = @"Serious"; break;
        case 1:  thermal = @"Fair"; break;
        default: thermal = @"Nominal"; break;
    }
    return @{
        @"battery": @(battery),
        @"thermalState": thermal,
        @"ramGB": @(ramGB),
        @"lowPower": @(lowPower),
    };
}

+ (void)triggerDeviceHapticLarge:(NSUInteger)large small:(NSUInteger)small {
    // GameEventHaptics is @MainActor-isolated; dispatch to the main queue.
    dispatch_async(dispatch_get_main_queue(), ^{
#if ARMSX2_HAS_SWIFTUI_HOST
        [SwiftUIHost triggerDeviceHapticWithLarge:large small:small];
#else
        (void)large;
        (void)small;
#endif
    });
}

// Apply OSD preset — sets ALL GSConfig flags to match the preset
+ (void)applyOsdPreset:(int)preset {
    // Clear everything first
    GSConfig.OsdShowFPS = false;
    GSConfig.OsdShowSpeed = false;
    GSConfig.OsdShowVPS = false;
    GSConfig.OsdShowCPU = false;
    GSConfig.OsdShowGPU = false;
    GSConfig.OsdShowResolution = false;
    GSConfig.OsdShowGSStats = false;
    GSConfig.OsdShowFrameTimes = false;
    GSConfig.OsdShowVersion = false;
    GSConfig.OsdShowHardwareInfo = false;
    GSConfig.OsdShowIndicators = false;
    GSConfig.OsdShowSettings = false;
    GSConfig.OsdShowInputs = false;
    GSConfig.OsdShowVideoCapture = false;
    GSConfig.OsdShowInputRec = false;

    switch (preset) {
    case 1: // simple: clean player readout; device stats are Swift-side
        GSConfig.OsdShowFPS = true;
        GSConfig.OsdShowSpeed = true;
        GSConfig.OsdShowCPU = true;
        GSConfig.OsdShowVersion = true;
        break;
    case 2: // detail: performance and renderer diagnostics
        GSConfig.OsdShowFPS = true;
        GSConfig.OsdShowVPS = true;
        GSConfig.OsdShowSpeed = true;
        GSConfig.OsdShowCPU = true;
        GSConfig.OsdShowGPU = true;
        GSConfig.OsdShowResolution = true;
        GSConfig.OsdShowIndicators = true;
        GSConfig.OsdShowVersion = true;
        break;
    case 3: // full: closest to Android's full stats section
        GSConfig.OsdShowFPS = true;
        GSConfig.OsdShowVPS = true;
        GSConfig.OsdShowSpeed = true;
        GSConfig.OsdShowCPU = true;
        GSConfig.OsdShowGPU = true;
        GSConfig.OsdShowResolution = true;
        GSConfig.OsdShowGSStats = true;
        GSConfig.OsdShowFrameTimes = true;
        GSConfig.OsdShowVersion = true;
        GSConfig.OsdShowHardwareInfo = true;
        GSConfig.OsdShowIndicators = true;
        GSConfig.OsdShowSettings = true;
        GSConfig.OsdShowInputs = true;
        break;
    default: // 0 = off
        break;
    }

    EmuConfig.GS.OsdShowFPS = GSConfig.OsdShowFPS;
    EmuConfig.GS.OsdShowVPS = GSConfig.OsdShowVPS;
    EmuConfig.GS.OsdShowSpeed = GSConfig.OsdShowSpeed;
    EmuConfig.GS.OsdShowCPU = GSConfig.OsdShowCPU;
    EmuConfig.GS.OsdShowGPU = GSConfig.OsdShowGPU;
    EmuConfig.GS.OsdShowResolution = GSConfig.OsdShowResolution;
    EmuConfig.GS.OsdShowGSStats = GSConfig.OsdShowGSStats;
    EmuConfig.GS.OsdShowFrameTimes = GSConfig.OsdShowFrameTimes;
    EmuConfig.GS.OsdShowVersion = GSConfig.OsdShowVersion;
    EmuConfig.GS.OsdShowHardwareInfo = GSConfig.OsdShowHardwareInfo;
    EmuConfig.GS.OsdShowIndicators = GSConfig.OsdShowIndicators;
    EmuConfig.GS.OsdShowSettings = GSConfig.OsdShowSettings;
    EmuConfig.GS.OsdShowInputs = GSConfig.OsdShowInputs;
}

+ (int)emulatorVolumePercent {
    const int value = g_p44_settings_interface ?
        g_p44_settings_interface->GetIntValue("SPU2/Output", "StandardVolume", ARMSX2DefaultAudioVolumePercent) :
        ARMSX2DefaultAudioVolumePercent;
    return ARMSX2ClampInt(value, 0, ARMSX2DefaultAudioVolumePercent);
}

+ (void)setEmulatorVolumePercent:(int)value {
    if (!g_p44_settings_interface)
        return;

    const int clampedValue = ARMSX2ClampInt(value, 0, ARMSX2DefaultAudioVolumePercent);
    g_p44_settings_interface->SetIntValue("SPU2/Output", "StandardVolume", clampedValue);
    g_p44_settings_interface->SetIntValue("SPU2/Output", "FastForwardVolume", clampedValue);
    ARMSX2ScheduleINISave();

    if (!VMManager::HasValidVM())
        return;

    Host::RunOnCPUThread([clampedValue]() {
        if (!VMManager::HasValidVM())
            return;

        const std::string serial = VMManager::GetDiscSerial();
        const u32 crc = VMManager::GetDiscCRC();
        if (crc != 0) {
            INISettingsInterface si(VMManager::GetGameSettingsPath(serial, crc));
            if (si.Load() &&
                (si.ContainsValue("SPU2/Output", "StandardVolume") ||
                 si.ContainsValue("SPU2/Output", "FastForwardVolume"))) {
                return;
            }
        }

        const Pcsx2Config oldConfig(EmuConfig);
        EmuConfig.SPU2.StandardVolume = clampedValue;
        EmuConfig.SPU2.FastForwardVolume = clampedValue;
        SPU2::CheckForConfigChanges(oldConfig);
    }, false);
}

// ============================================================
// ISO / BIOS / Settings management
// ============================================================

#pragma mark - ISO boot

+ (BOOL)canResolveISO:(nonnull NSString *)isoName {
	return ARMSX2ResolveISOPath(isoName).length > 0 ? YES : NO;
}

+ (void)bootISO:(nonnull NSString *)isoName {
	if (!g_p44_settings_interface) {
		std::fprintf(stderr, "@@BOOT_SET_ISO@@ status=no_settings input=\"%s\"\n", isoName ? isoName.UTF8String : "");
		std::fflush(stderr);
		return;
	}
	ARMSX2FlushINISave(); // persist deferred base-setting writes before booting
	NSString* resolvedPath = ARMSX2ResolveISOPath(isoName);
	NSString* bootValue = isoName.isAbsolutePath ? (resolvedPath ?: isoName) : isoName;
	if (bootValue.isAbsolutePath) {
		BOOL accessActive = ARMSX2StartExternalGameDirectoryAccessForPathSafe(bootValue);
		NSLog(@"[ARMSX2Bridge] bootISO external access path=%@ active=%d", bootValue, accessActive ? 1 : 0);
	}
	std::fprintf(stderr, "@@BOOT_SET_ISO@@ input=\"%s\" boot=\"%s\" resolved=\"%s\" resolved_exists=%d absolute=%d\n",
		isoName ? isoName.UTF8String : "", bootValue ? bootValue.UTF8String : "", resolvedPath ? resolvedPath.UTF8String : "",
		resolvedPath.length > 0 ? 1 : 0, bootValue.isAbsolutePath ? 1 : 0);
	std::fflush(stderr);
	g_p44_settings_interface->SetStringValue("GameISO", "BootISO", bootValue.UTF8String);
	const bool fastBoot = g_p44_settings_interface->GetBoolValue(
		"GameISO", "FastBoot",
		g_p44_settings_interface->GetBoolValue("EmuCore", "EnableFastBoot", false));
	g_p44_settings_interface->SetBoolValue("GameISO", "FastBoot", fastBoot);
	g_p44_settings_interface->SetBoolValue("EmuCore", "EnableFastBoot", fastBoot);
	g_p44_settings_interface->Save();
	ARMSX2ApplyCompatibilityPresetForISOName(bootValue);
	std::fprintf(stderr, "@@BOOT_FASTBOOT_SET@@ value=%d source=settings\n", fastBoot ? 1 : 0);
	std::fflush(stderr);
	NSLog(@"bootISO: set BootISO=%@ resolved=%@", bootValue, resolvedPath ?: @"");
}

#pragma mark - BIOS management

+ (nonnull NSString *)biosDirectory {
    NSString *docsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
    NSString *biosDir = [docsPath stringByAppendingPathComponent:@"bios"];
    [[NSFileManager defaultManager] createDirectoryAtPath:biosDir withIntermediateDirectories:YES attributes:nil error:nil];
    return biosDir;
}

+ (nonnull NSArray<ARMSX2BIOSInfo *> *)availableBIOSInfos {
    NSFileManager *fm = [NSFileManager defaultManager];
    NSMutableSet *seen = [NSMutableSet set];
    NSMutableArray<ARMSX2BIOSInfo *> *bioses = [NSMutableArray array];

    // Helper block: list all imported BIOS candidates, including small companion ROMs.
    void (^scanDir)(NSString *) = ^(NSString *dir) {
        NSArray *files = [fm contentsOfDirectoryAtPath:dir error:nil];
        for (NSString *file in files) {
            if ([seen containsObject:file]) continue;
            NSString *ext = file.pathExtension.lowercaseString;
            if ([ext isEqualToString:@"bin"] || [ext isEqualToString:@"rom"]) {
                NSString *fullPath = [dir stringByAppendingPathComponent:file];
                NSDictionary *attrs = [fm attributesOfItemAtPath:fullPath error:nil];
                unsigned long long sz = [attrs fileSize];
                if (sz > 0 && sz <= 50 * 1024 * 1024) {
                    [bioses addObject:ARMSX2MakeBIOSInfo(file, dir)];
                    [seen addObject:file];
                }
            }
        }
    };

    scanDir([self biosDirectory]);
    [bioses sortUsingComparator:^NSComparisonResult(ARMSX2BIOSInfo *lhs, ARMSX2BIOSInfo *rhs) {
        if (lhs.valid != rhs.valid)
            return lhs.valid ? NSOrderedAscending : NSOrderedDescending;
        return [lhs.fileName localizedCaseInsensitiveCompare:rhs.fileName];
    }];
    return bioses;
}

+ (nonnull NSString *)defaultBIOSName {
    if (!g_p44_settings_interface) return @"";
    std::string val = g_p44_settings_interface->GetStringValue("Filenames", "BIOS", "");
    return [NSString stringWithUTF8String:val.c_str()];
}

+ (void)setDefaultBIOS:(nonnull NSString *)biosName {
    if (!g_p44_settings_interface) return;
    g_p44_settings_interface->SetStringValue("Filenames", "BIOS", biosName.UTF8String);
    g_p44_settings_interface->Save();
    EmuConfig.BaseFilenames.Bios = biosName.UTF8String;
    NSLog(@"setDefaultBIOS: %@", biosName);
}

#pragma mark - Favorites

+ (BOOL)isFavorite:(nonnull NSString *)isoName {
    if (!g_p44_settings_interface) return NO;
    return g_p44_settings_interface->GetBoolValue("Favorites", isoName.UTF8String, false);
}

+ (void)setFavorite:(nonnull NSString *)isoName favorite:(BOOL)favorite {
    if (!g_p44_settings_interface) return;
    g_p44_settings_interface->SetBoolValue("Favorites", isoName.UTF8String, favorite);
    g_p44_settings_interface->Save();
}

#pragma mark - INI generic getter/setter

+ (int)getINIInt:(nonnull NSString *)section key:(nonnull NSString *)key defaultValue:(int)def {
    if (!g_p44_settings_interface) return def;
    return g_p44_settings_interface->GetIntValue(section.UTF8String, key.UTF8String, def);
}

+ (BOOL)getINIBool:(nonnull NSString *)section key:(nonnull NSString *)key defaultValue:(BOOL)def {
    if (!g_p44_settings_interface) return def;
    return g_p44_settings_interface->GetBoolValue(section.UTF8String, key.UTF8String, def);
}

+ (float)getINIFloat:(nonnull NSString *)section key:(nonnull NSString *)key defaultValue:(float)def {
    if (!g_p44_settings_interface) return def;
    return g_p44_settings_interface->GetFloatValue(section.UTF8String, key.UTF8String, def);
}

+ (nonnull NSString *)getINIString:(nonnull NSString *)section key:(nonnull NSString *)key defaultValue:(nonnull NSString *)def {
    if (!g_p44_settings_interface) return def;
    std::string val = g_p44_settings_interface->GetStringValue(section.UTF8String, key.UTF8String, def.UTF8String);
    return [NSString stringWithUTF8String:val.c_str()];
}

+ (void)setINIInt:(nonnull NSString *)section key:(nonnull NSString *)key value:(int)value {
    if (!g_p44_settings_interface) return;
    if (ARMSX2RetroAchievementsHardcoreActive() &&
        std::strcmp(section.UTF8String, "EmuCore/Speedhacks") == 0 &&
        std::strcmp(key.UTF8String, "EECycleRate") == 0 && value < 0) {
        ARMSX2LogRetroAchievementsHardcoreBlock("ee_underclock");
        value = 0;
    }
    g_p44_settings_interface->SetIntValue(section.UTF8String, key.UTF8String, value);
    ARMSX2ScheduleINISave();
    ARMSX2ApplyLiveGSIntSetting(section.UTF8String, key.UTF8String, value);
}

+ (void)setINIBool:(nonnull NSString *)section key:(nonnull NSString *)key value:(BOOL)value {
    if (!g_p44_settings_interface) return;
    if (ARMSX2ShouldBlockRetroAchievementsHardcoreBoolSetting(section.UTF8String, key.UTF8String, value))
        value = NO;
    g_p44_settings_interface->SetBoolValue(section.UTF8String, key.UTF8String, value);
    ARMSX2ScheduleINISave();
    ARMSX2ApplyLiveGSBoolSetting(section.UTF8String, key.UTF8String, value);
}

+ (void)setINIFloat:(nonnull NSString *)section key:(nonnull NSString *)key value:(float)value {
    if (!g_p44_settings_interface) return;
    float valueToStore = value;
    valueToStore = ARMSX2EnforceRetroAchievementsHardcoreFloatSetting(section.UTF8String, key.UTF8String, valueToStore);
    if (std::strcmp(section.UTF8String, "Framerate") == 0 && std::strcmp(key.UTF8String, "NominalScalar") == 0)
        valueToStore = ARMSX2NormalizeIOSNominalScalar(valueToStore);

    g_p44_settings_interface->SetFloatValue(section.UTF8String, key.UTF8String, valueToStore);
    ARMSX2ScheduleINISave();
    ARMSX2ApplyLiveFloatSetting(section.UTF8String, key.UTF8String, valueToStore);
}

+ (void)setINIString:(nonnull NSString *)section key:(nonnull NSString *)key value:(nonnull NSString *)value {
    if (!g_p44_settings_interface) return;
    g_p44_settings_interface->SetStringValue(section.UTF8String, key.UTF8String, value.UTF8String);
    ARMSX2ScheduleINISave();
}

+ (void)clearINISection:(nonnull NSString *)section {
    if (!g_p44_settings_interface) return;
    g_p44_settings_interface->ClearSection(section.UTF8String);
    ARMSX2ScheduleINISave();
}

// Reloads settings into the running VM and pushes graphics options to the GS
// thread so visual changes take effect without restarting the game. Safe to call
// when no VM/GS is open (it returns without doing anything).
+ (void)applyGraphicsSettingsNow
{
    if (!VMManager::HasValidVM())
        return;

    VMManager::ApplySettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

// Force any deferred base-settings INI write to disk immediately.
+ (void)flushINISettings
{
    ARMSX2FlushINISave();
}

// Probes whether MetalFX Spatial upscaling is available on this device. This is
// a standalone check that works from the main menu before any GS device exists,
// so the settings UI can decide whether to show the Upscaler section at all. It
// returns NO on pre-iOS-16, the simulator, and any GPU that fails the framework
// capability probe.
+ (BOOL)isMetalFXSupported {
    if (@available(iOS 16.0, *)) {
        MRCOwned<id<MTLDevice>> device = MRCTransfer(MTLCreateSystemDefaultDevice());
        if (!device)
            return NO;
        return [MTLFXSpatialScalerDescriptor supportsDevice:device];
    }
    return NO;
}

#pragma mark - Per-game INI getter/setter
// Reads/writes the per-game INI at VMManager::GetGameSettingsPath(serial,crc), the
// same file the game-settings and patch-enable-list helpers use. The "for current
// game" write/delete variants live-apply via VMManager::ReloadGameSettings().

+ (BOOL)hasPerGameINIValue:(nonnull NSString *)section key:(nonnull NSString *)key forISO:(nonnull NSString *)isoName {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForISO(isoName, &serial, &crc))
        return NO;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    if (!si.Load())
        return NO;
    return si.ContainsValue(section.UTF8String, key.UTF8String);
}

+ (int)getPerGameINIInt:(nonnull NSString *)section key:(nonnull NSString *)key defaultValue:(int)def forISO:(nonnull NSString *)isoName {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForISO(isoName, &serial, &crc))
        return def;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    if (!si.Load())
        return def;
    return si.GetIntValue(section.UTF8String, key.UTF8String, def);
}

+ (BOOL)getPerGameINIBool:(nonnull NSString *)section key:(nonnull NSString *)key defaultValue:(BOOL)def forISO:(nonnull NSString *)isoName {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForISO(isoName, &serial, &crc))
        return def;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    if (!si.Load())
        return def;
    return si.GetBoolValue(section.UTF8String, key.UTF8String, def);
}

+ (void)setPerGameINIInt:(nonnull NSString *)section key:(nonnull NSString *)key value:(int)value forISO:(nonnull NSString *)isoName {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForISO(isoName, &serial, &crc))
        return;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    si.Load();
    si.SetIntValue(section.UTF8String, key.UTF8String, value);
    Error error;
    si.Save(&error);
}

+ (void)setPerGameINIBool:(nonnull NSString *)section key:(nonnull NSString *)key value:(BOOL)value forISO:(nonnull NSString *)isoName {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForISO(isoName, &serial, &crc))
        return;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    si.Load();
    si.SetBoolValue(section.UTF8String, key.UTF8String, value);
    Error error;
    si.Save(&error);
}

+ (void)deletePerGameINIValue:(nonnull NSString *)section key:(nonnull NSString *)key forISO:(nonnull NSString *)isoName {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForISO(isoName, &serial, &crc))
        return;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    if (!si.Load())
        return;
    si.DeleteValue(section.UTF8String, key.UTF8String);
    si.RemoveEmptySections();
    Error error;
    si.Save(&error);
}

+ (BOOL)hasPerGameINIValueForCurrentGame:(nonnull NSString *)section key:(nonnull NSString *)key {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForCurrentGame(&serial, &crc))
        return NO;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    if (!si.Load())
        return NO;
    return si.ContainsValue(section.UTF8String, key.UTF8String);
}

+ (int)getPerGameINIIntForCurrentGame:(nonnull NSString *)section key:(nonnull NSString *)key defaultValue:(int)def {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForCurrentGame(&serial, &crc))
        return def;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    if (!si.Load())
        return def;
    return si.GetIntValue(section.UTF8String, key.UTF8String, def);
}

+ (BOOL)getPerGameINIBoolForCurrentGame:(nonnull NSString *)section key:(nonnull NSString *)key defaultValue:(BOOL)def {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForCurrentGame(&serial, &crc))
        return def;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    if (!si.Load())
        return def;
    return si.GetBoolValue(section.UTF8String, key.UTF8String, def);
}

+ (void)setPerGameINIIntForCurrentGame:(nonnull NSString *)section key:(nonnull NSString *)key value:(int)value {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForCurrentGame(&serial, &crc))
        return;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    si.Load();
    si.SetIntValue(section.UTF8String, key.UTF8String, value);
    Error error;
    si.Save(&error);
    VMManager::ReloadGameSettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

+ (void)setPerGameINIBoolForCurrentGame:(nonnull NSString *)section key:(nonnull NSString *)key value:(BOOL)value {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForCurrentGame(&serial, &crc))
        return;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    si.Load();
    si.SetBoolValue(section.UTF8String, key.UTF8String, value);
    Error error;
    si.Save(&error);
    VMManager::ReloadGameSettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

+ (void)deletePerGameINIValueForCurrentGame:(nonnull NSString *)section key:(nonnull NSString *)key {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2PerGameIdentityForCurrentGame(&serial, &crc))
        return;
    INISettingsInterface si(ARMSX2PerGameSettingsPath(serial, crc));
    if (!si.Load())
        return;
    si.DeleteValue(section.UTF8String, key.UTF8String);
    si.RemoveEmptySections();
    Error error;
    si.Save(&error);
    VMManager::ReloadGameSettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

+ (int)limiterMode
{
    if (!VMManager::HasValidVM())
        return static_cast<int>(LimiterModeType::Nominal);

    return static_cast<int>(VMManager::GetLimiterMode());
}

+ (void)setLimiterMode:(int)mode
{
    const bool hasValidVM = VMManager::HasValidVM();
    std::fprintf(stderr, "@@LIMITER_MODE_REQUEST@@ mode=%d valid=%d state=%d\n",
        mode, hasValidVM ? 1 : 0, static_cast<int>(VMManager::GetState()));
    std::fflush(stderr);

    if (!hasValidVM)
        return;

    LimiterModeType limiterMode = LimiterModeType::Nominal;
    switch (mode) {
    case static_cast<int>(LimiterModeType::Turbo):
        limiterMode = LimiterModeType::Turbo;
        break;
    case static_cast<int>(LimiterModeType::Slomo):
        limiterMode = LimiterModeType::Slomo;
        break;
    case static_cast<int>(LimiterModeType::Unlimited):
        limiterMode = LimiterModeType::Unlimited;
        break;
    default:
        break;
    }

    if (ARMSX2RetroAchievementsHardcoreActive() && limiterMode == LimiterModeType::Slomo) {
        ARMSX2LogRetroAchievementsHardcoreBlock("slomo_limiter_mode");
        limiterMode = LimiterModeType::Nominal;
    }

    Host::RunOnCPUThread([limiterMode]() {
        if (!VMManager::HasValidVM())
            return;

        const LimiterModeType previousMode = VMManager::GetLimiterMode();
        VMManager::SetLimiterMode(limiterMode);
        const LimiterModeType appliedMode = VMManager::GetLimiterMode();
        std::fprintf(stderr,
            "@@LIMITER_MODE@@ before=%d after=%d target=%.3f nominal=%.3f turbo=%.3f slomo=%.3f\n",
            static_cast<int>(previousMode), static_cast<int>(appliedMode), VMManager::GetTargetSpeed(),
            EmuConfig.EmulationSpeed.NominalScalar, EmuConfig.EmulationSpeed.TurboScalar,
            EmuConfig.EmulationSpeed.SlomoScalar);
        std::fflush(stderr);
    }, false);
}

#pragma mark - Compatibility Lab

+ (BOOL)getJITBisectFlag:(nonnull NSString *)key defaultValue:(BOOL)def
{
    BOOL value = def;
    if (g_p44_settings_interface)
        value = g_p44_settings_interface->GetBoolValue("ARMSX2/JITBisect", key.UTF8String, def);

    ARMSX2ApplyJITBisectFlag(key, value);
    return value;
}

+ (void)setJITBisectFlag:(nonnull NSString *)key value:(BOOL)value
{
    ARMSX2ApplyJITBisectFlag(key, value);
    if (g_p44_settings_interface) {
        g_p44_settings_interface->SetBoolValue("ARMSX2/JITBisect", key.UTF8String, value);
        g_p44_settings_interface->SetStringValue("ARMSX2/JITBisect", "Profile", ARMSX2CompatibilityProfileCustom.UTF8String);
        g_p44_settings_interface->Save();
    }
    NSString* identity = ARMSX2CurrentCompatibilityIdentityKey();
    if (identity.length > 0)
        ARMSX2SaveCompatibilityCustomFlagsForIdentity(identity);
    NSLog(@"[ARMSX2Bridge] Compatibility Lab %@ %@", key, value ? @"ON" : @"OFF");
}

+ (nonnull NSString *)compatibilityPresetForCurrentGame
{
    return ARMSX2CurrentCompatibilityProfileFromSettings();
}

+ (nonnull NSString *)compatibilityIdentityForCurrentGame
{
    return ARMSX2CurrentCompatibilityIdentityKey();
}

+ (nonnull NSString *)compatibilityPresetForISO:(nonnull NSString *)isoName
{
    GameList::Entry entry;
    NSString* identity = ARMSX2CompatibilityIdentityForISOName(isoName, &entry);
    if (identity.length == 0)
        return ARMSX2CompatibilityProfileOff;

    NSString* title = ARMSX2NSStringFromStdString(entry.GetTitle(false));
    if (title.length == 0)
        title = isoName.stringByDeletingPathExtension ?: isoName;

    return ARMSX2ResolvedCompatibilityPreset(identity, title);
}

+ (nonnull NSString *)compatibilityIdentityForISO:(nonnull NSString *)isoName
{
    return ARMSX2CompatibilityIdentityForISOName(isoName);
}

+ (BOOL)isCompatibilityAutoGamePresetsEnabled
{
    if (!g_p44_settings_interface)
        return YES;
    return g_p44_settings_interface->GetBoolValue("ARMSX2/JITBisect", "AutoGamePresets", true) ? YES : NO;
}

+ (void)setCompatibilityAutoGamePresetsEnabled:(BOOL)enabled
{
    if (g_p44_settings_interface) {
        g_p44_settings_interface->SetBoolValue("ARMSX2/JITBisect", "AutoGamePresets", enabled ? true : false);
        g_p44_settings_interface->Save();
    }
    NSLog(@"[ARMSX2Bridge] Compatibility auto game presets %@", enabled ? @"ON" : @"OFF");
}

+ (void)setCompatibilityPreset:(nonnull NSString *)preset forISO:(nonnull NSString *)isoName
{
    if (!g_p44_settings_interface)
        return;

    NSString* normalized = ARMSX2NormalizeCompatibilityProfile(preset);
    NSString* identity = ARMSX2CompatibilityIdentityForISOName(isoName);
    if (identity.length == 0) {
        NSLog(@"[ARMSX2Bridge] Compatibility preset save rejected iso=%@", isoName);
        return;
    }

    g_p44_settings_interface->SetStringValue("ARMSX2/JITBisectGamePresets", identity.UTF8String, normalized.UTF8String);
    if (![normalized isEqualToString:ARMSX2CompatibilityProfileCustom])
        ARMSX2ClearCompatibilityCustomFlagsForIdentity(identity);
    g_p44_settings_interface->Save();
    NSLog(@"[ARMSX2Bridge] Compatibility saved preset=%@ identity=%@ iso=%@", normalized, identity, isoName);
}

+ (BOOL)compatibilityFlag:(nonnull NSString *)flag forISO:(nonnull NSString *)isoName
{
    if (!g_p44_settings_interface)
        return NO;

    NSString* identity = ARMSX2CompatibilityIdentityForISOName(isoName);
    NSString* key = ARMSX2CompatibilityProfileFlagKey(ARMSX2NormalizeCompatibilityProfile(flag));
    if (identity.length == 0 || key.length == 0)
        return NO;

    NSString* section = ARMSX2CompatibilityCustomFlagSection(identity);
    bool value = false;
    if (g_p44_settings_interface->GetBoolValue(section.UTF8String, key.UTF8String, &value))
        return value ? YES : NO;

    NSString* activeKey = ARMSX2CompatibilityProfileFlagKey(ARMSX2ResolvedCompatibilityPreset(identity, identity));
    return (activeKey.length > 0 && [activeKey isEqualToString:key]) ? YES : NO;
}

+ (void)setCompatibilityFlag:(nonnull NSString *)flag enabled:(BOOL)enabled forISO:(nonnull NSString *)isoName
{
    if (!g_p44_settings_interface)
        return;

    NSString* identity = ARMSX2CompatibilityIdentityForISOName(isoName);
    NSString* key = ARMSX2CompatibilityProfileFlagKey(ARMSX2NormalizeCompatibilityProfile(flag));
    if (identity.length == 0 || key.length == 0) {
        NSLog(@"[ARMSX2Bridge] Compatibility custom flag rejected flag=%@ iso=%@", flag, isoName);
        return;
    }

    NSString* section = ARMSX2CompatibilityCustomFlagSection(identity);
    g_p44_settings_interface->SetStringValue("ARMSX2/JITBisectGamePresets", identity.UTF8String, ARMSX2CompatibilityProfileCustom.UTF8String);
    g_p44_settings_interface->SetBoolValue(section.UTF8String, key.UTF8String, enabled ? true : false);

    NSString* currentIdentity = ARMSX2CurrentCompatibilityIdentityKey();
    if ([identity isEqualToString:currentIdentity]) {
        ARMSX2ApplyJITBisectFlag(key, enabled);
        g_p44_settings_interface->SetBoolValue("ARMSX2/JITBisect", key.UTF8String, enabled ? true : false);
        g_p44_settings_interface->SetStringValue("ARMSX2/JITBisect", "Profile", ARMSX2CompatibilityProfileCustom.UTF8String);
    }

    g_p44_settings_interface->Save();
    NSLog(@"[ARMSX2Bridge] Compatibility custom flag %@ %@ identity=%@ iso=%@", key, enabled ? @"ON" : @"OFF", identity, isoName);
}

+ (void)setCompatibilityPreset:(nonnull NSString *)preset rememberForCurrentGame:(BOOL)rememberForCurrentGame
{
    NSString* normalized = ARMSX2NormalizeCompatibilityProfile(preset);
    ARMSX2ApplyCompatibilityProfile(normalized, YES, rememberForCurrentGame ? @"remember current game" : @"manual preset");

    if (rememberForCurrentGame && g_p44_settings_interface) {
        NSString* identity = ARMSX2CurrentCompatibilityIdentityKey();
        if (identity.length > 0) {
            g_p44_settings_interface->SetStringValue("ARMSX2/JITBisectGamePresets", identity.UTF8String, normalized.UTF8String);
            g_p44_settings_interface->Save();
            NSLog(@"[ARMSX2Bridge] Compatibility remembered preset=%@ identity=%@", normalized, identity);
        }
    }
}

+ (void)forgetCompatibilityPresetForCurrentGame
{
    if (!g_p44_settings_interface)
        return;

    NSString* identity = ARMSX2CurrentCompatibilityIdentityKey();
    if (identity.length == 0)
        return;

    g_p44_settings_interface->DeleteValue("ARMSX2/JITBisectGamePresets", identity.UTF8String);
    ARMSX2ClearCompatibilityCustomFlagsForIdentity(identity);
    g_p44_settings_interface->Save();
    NSString* profile = ARMSX2ResolvedCompatibilityPreset(identity, identity);
    ARMSX2ApplyCompatibilityProfile(profile, YES, [NSString stringWithFormat:@"forget %@", identity]);
    NSLog(@"[ARMSX2Bridge] Compatibility forgot preset identity=%@", identity);
}

+ (void)forgetCompatibilityPresetForISO:(nonnull NSString *)isoName
{
    if (!g_p44_settings_interface)
        return;

    NSString* identity = ARMSX2CompatibilityIdentityForISOName(isoName);
    if (identity.length == 0)
        return;

    g_p44_settings_interface->DeleteValue("ARMSX2/JITBisectGamePresets", identity.UTF8String);
    ARMSX2ClearCompatibilityCustomFlagsForIdentity(identity);
    g_p44_settings_interface->Save();
    NSLog(@"[ARMSX2Bridge] Compatibility forgot preset identity=%@ iso=%@", identity, isoName);
}

#pragma mark - VM lifecycle

+ (BOOL)isVMRunning {
    VMState st = VMManager::GetState();
    return st == VMState::Running || st == VMState::Paused;
}

+ (BOOL)hasBIOS {
    if (EmuConfig.BaseFilenames.Bios.empty()) return NO;
    std::string fullPath = Path::Combine(EmuFolders::Bios, EmuConfig.BaseFilenames.Bios);
    return FileSystem::FileExists(fullPath.c_str());
}

+ (void)requestVMBoot {
	std::string bootISO;
	if (g_p44_settings_interface)
		bootISO = g_p44_settings_interface->GetStringValue("GameISO", "BootISO", "");
	const std::string biosPath = Path::Combine(EmuFolders::Bios, EmuConfig.BaseFilenames.Bios);
	std::fprintf(stderr, "@@BOOT_REQUEST@@ posted=1 has_bios=%d bios=\"%s\" boot_iso=\"%s\"\n",
		(!EmuConfig.BaseFilenames.Bios.empty() && FileSystem::FileExists(biosPath.c_str())) ? 1 : 0,
		EmuConfig.BaseFilenames.Bios.c_str(), bootISO.c_str());
	std::fflush(stderr);
    [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2iOSRequestVMBoot" object:nil];
}

+ (void)requestVMShutdown {
    [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2iOSRequestVMShutdown" object:nil];
}

+ (void)testControllerRumble {
    ARMSX2_iOSTestGamepadRumble();
}

#pragma mark - Save states

+ (BOOL)hasValidSaveStateGame {
    return ARMSX2GetCurrentSaveStateIdentity(nullptr, nullptr);
}

+ (nonnull NSArray<ARMSX2SaveStateSlotInfo *> *)saveStateSlots {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2GetCurrentSaveStateIdentity(&serial, &crc))
        return @[];

    NSMutableArray<ARMSX2SaveStateSlotInfo *> *slots = [NSMutableArray arrayWithCapacity:VMManager::NUM_SAVE_STATE_SLOTS];
    NSFileManager *fm = [NSFileManager defaultManager];

    for (s32 slot = 1; slot <= VMManager::NUM_SAVE_STATE_SLOTS; slot++) {
        const std::string path = VMManager::GetSaveStateFileName(serial.c_str(), crc, slot);
        const BOOL occupied = !path.empty() && FileSystem::FileExists(path.c_str());
        NSString *nsPath = ARMSX2NSStringFromStdString(path);

        ARMSX2SaveStateSlotInfo *info = [ARMSX2SaveStateSlotInfo new];
        info.slot = slot;
        info.occupied = occupied;
        info.filePath = nsPath;
        info.fileName = ARMSX2NSStringFromStringView(Path::GetFileName(path));

        if (occupied) {
            NSDictionary<NSFileAttributeKey, id> *attrs = [fm attributesOfItemAtPath:nsPath error:nil];
            info.modifiedDate = attrs[NSFileModificationDate];
            info.previewPNGData = ARMSX2ReadSaveStatePreviewPNG(path);
        }

        [slots addObject:info];
    }

    return slots;
}

+ (void)saveStateToSlot:(NSInteger)slot completion:(nullable ARMSX2SaveStateCompletion)completion {
    const s32 nativeSlot = static_cast<s32>(slot);
    ARMSX2SaveStateCompletion callback = [completion copy];
    std::string serial;
    u32 crc = 0;
    if (nativeSlot < 1 || nativeSlot > VMManager::NUM_SAVE_STATE_SLOTS || !ARMSX2GetCurrentSaveStateIdentity(&serial, &crc)) {
        NSLog(@"[ARMSX2 iOS SaveState] save rejected slot=%d validGame=0", nativeSlot);
        if (callback)
            dispatch_async(dispatch_get_main_queue(), ^{ callback(NO); });
        return;
    }

    const std::string targetPath = VMManager::GetSaveStateFileName(serial.c_str(), crc, nativeSlot);
    NSLog(@"[ARMSX2 iOS SaveState] save requested slot=%d path=%@", nativeSlot, ARMSX2NSStringFromStdString(targetPath));

    dispatch_async(ARMSX2SaveStateQueue(), ^{
        bool result = false;
        VMManager::WaitForSaveStateFlush();
        Host::RunOnCPUThread([nativeSlot, &result]() {
            NSLog(@"[ARMSX2 iOS SaveState] CPU save start slot=%d", nativeSlot);
            if (MemcardBusy::IsBusy()) {
                NSLog(@"[ARMSX2 iOS SaveState] CPU save rejected slot=%d reason=memory-card-busy", nativeSlot);
                result = false;
                return;
            }

            if (!ARMSX2FlushNVRAMAndMemoryCards("pre-save-state")) {
                NSLog(@"[ARMSX2 iOS SaveState] CPU save rejected slot=%d reason=pre-save-flush-failed", nativeSlot);
                result = false;
                return;
            }

            std::string saveError;
            VMManager::SaveStateToSlot(nativeSlot, false, [&saveError](const std::string& error) {
                saveError = error;
            });
            result = saveError.empty();
            if (result)
                ARMSX2FlushNVRAMAndMemoryCards("post-save-state");
            else
                NSLog(@"[ARMSX2 iOS SaveState] CPU save failed slot=%d error=%@", nativeSlot, ARMSX2NSStringFromStdString(saveError));
            NSLog(@"[ARMSX2 iOS SaveState] CPU save finished slot=%d result=%d", nativeSlot, result ? 1 : 0);
        }, true);

        if (result) {
            VMManager::WaitForSaveStateFlush();
            result = targetPath.empty() ? result : FileSystem::FileExists(targetPath.c_str());
        }

        NSLog(@"[ARMSX2 iOS SaveState] save finished slot=%d result=%d exists=%d",
              nativeSlot, result ? 1 : 0, (!targetPath.empty() && FileSystem::FileExists(targetPath.c_str())) ? 1 : 0);

        if (callback)
            dispatch_async(dispatch_get_main_queue(), ^{ callback(result ? YES : NO); });
    });
}

+ (void)loadStateFromSlot:(NSInteger)slot completion:(nullable ARMSX2SaveStateCompletion)completion {
    const s32 nativeSlot = static_cast<s32>(slot);
    ARMSX2SaveStateCompletion callback = [completion copy];
    std::string serial;
    u32 crc = 0;
    if (nativeSlot < 1 || nativeSlot > VMManager::NUM_SAVE_STATE_SLOTS || !ARMSX2GetCurrentSaveStateIdentity(&serial, &crc)) {
        NSLog(@"[ARMSX2 iOS SaveState] load rejected slot=%d validGame=0", nativeSlot);
        if (callback)
            dispatch_async(dispatch_get_main_queue(), ^{ callback(NO); });
        return;
    }

    const std::string targetPath = VMManager::GetSaveStateFileName(serial.c_str(), crc, nativeSlot);
    NSLog(@"[ARMSX2 iOS SaveState] load requested slot=%d path=%@ exists=%d",
          nativeSlot, ARMSX2NSStringFromStdString(targetPath), (!targetPath.empty() && FileSystem::FileExists(targetPath.c_str())) ? 1 : 0);

    dispatch_async(ARMSX2SaveStateQueue(), ^{
        if (ARMSX2RetroAchievementsHardcoreActive()) {
            NSLog(@"[ARMSX2 iOS SaveState] load rejected slot=%d reason=hardcore-active", nativeSlot);
            std::fprintf(stderr, "@@IOS_SAVESTATE_LOAD_BLOCKED@@ slot=%d reason=hardcore-active\n", nativeSlot);
            std::fflush(stderr);
            if (callback)
                dispatch_async(dispatch_get_main_queue(), ^{ callback(NO); });
            return;
        }

        bool result = false;
        bool flushResult = false;
        VMManager::WaitForSaveStateFlush();
        Host::RunOnCPUThread([&flushResult]() {
            flushResult = ARMSX2FlushNVRAMAndMemoryCards("pre-load-state");
        }, true);

        NSInteger backupCount = 0;
        if (flushResult)
            backupCount = ARMSX2BackupAssignedMemoryCards("pre-load-state", nativeSlot, serial, crc);

        Host::RunOnCPUThread([nativeSlot, flushResult, &result]() {
            NSLog(@"[ARMSX2 iOS SaveState] CPU load start slot=%d", nativeSlot);
            if (!flushResult) {
                NSLog(@"[ARMSX2 iOS SaveState] CPU load rejected slot=%d reason=pre-load-flush-failed", nativeSlot);
                result = false;
                return;
            }

            if (MemcardBusy::IsBusy()) {
                NSLog(@"[ARMSX2 iOS SaveState] CPU load rejected slot=%d reason=memory-card-busy", nativeSlot);
                result = false;
                return;
            }

            result = VMManager::LoadStateFromSlot(nativeSlot);
            NSLog(@"[ARMSX2 iOS SaveState] CPU load finished slot=%d result=%d", nativeSlot, result ? 1 : 0);
        }, true);

        NSLog(@"[ARMSX2 iOS SaveState] load callback slot=%d result=%d memcardBackups=%ld",
              nativeSlot, result ? 1 : 0, static_cast<long>(backupCount));

        if (callback)
            dispatch_async(dispatch_get_main_queue(), ^{ callback(result ? YES : NO); });
    });
}

#pragma mark - PNACH cheats/patches

+ (nullable NSString *)pnachPathForCurrentGameAsCheat:(BOOL)asCheat {
    // Note: Hardcore Mode does not block locating/creating cheat files here. Cheat
    // download/import only stores the file; the PCSX2 core refuses to apply cheats
    // while Hardcore is active, and the Swift toggle gates enabling them.

    std::string serial;
    u32 crc = 0;
    if (!ARMSX2GetCurrentSaveStateIdentity(&serial, &crc)) {
        NSLog(@"[ARMSX2Bridge] PNACH path unavailable: no current game identity");
        return nil;
    }

    return ARMSX2NSStringFromStdString(Patch::GetPnachFilename(serial, crc, asCheat));
}

+ (nullable NSString *)pnachPathForISO:(nonnull NSString *)isoName asCheat:(BOOL)asCheat {
    // Note: Hardcore Mode does not block locating/creating cheat files here. See
    // pnachPathForCurrentGameAsCheat: above; the core gates application, not storage.

    NSString* path = ARMSX2ResolveISOPath(isoName);
    if (path.length == 0) {
        NSLog(@"[ARMSX2Bridge] PNACH path unavailable for %@: ISO not found", isoName);
        return nil;
    }

    GameList::Entry entry;
    if (!GameList::PopulateEntryFromPath(path.UTF8String, &entry) || entry.crc == 0) {
        NSLog(@"[ARMSX2Bridge] PNACH path unavailable for %@: metadata missing", isoName);
        return nil;
    }

    return ARMSX2NSStringFromStdString(Patch::GetPnachFilename(entry.serial, entry.crc, asCheat));
}

+ (void)reloadPatches {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2GetCurrentSaveStateIdentity(&serial, &crc))
        return;

    Host::RunOnCPUThread([serial, crc]() {
        Patch::ReloadPatches(serial, crc, true, true, true, true);
        Patch::UpdateActivePatches(true, true, true, true);
    }, false);
}

+ (NSArray<NSString *> *)patchEnableListForISO:(NSString *)isoName section:(NSString *)section key:(NSString *)key {
    NSString* path = ARMSX2ResolveISOPath(isoName);
    if (path.length == 0) return @[];

    GameList::Entry entry;
    if (!GameList::PopulateEntryFromPath(path.UTF8String, &entry) || entry.crc == 0) return @[];
    return ARMSX2PatchEnableListForIdentity(entry.serial, entry.crc, section, key);
}

+ (NSArray<NSString *> *)patchEnableListForCurrentGameSection:(NSString *)section key:(NSString *)key {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2GetCurrentSaveStateIdentity(&serial, &crc)) return @[];
    return ARMSX2PatchEnableListForIdentity(serial, crc, section, key);
}

+ (void)setPatchEnableList:(NSArray<NSString *> *)values forISO:(NSString *)isoName section:(NSString *)section key:(NSString *)key {
    NSString* path = ARMSX2ResolveISOPath(isoName);
    if (path.length == 0) return;

    GameList::Entry entry;
    if (!GameList::PopulateEntryFromPath(path.UTF8String, &entry) || entry.crc == 0) return;
    ARMSX2SetPatchEnableListForIdentity(values, entry.serial, entry.crc, section, key);
}

+ (void)setPatchEnableListForCurrentGame:(NSArray<NSString *> *)values section:(NSString *)section key:(NSString *)key {
    std::string serial;
    u32 crc = 0;
    if (!ARMSX2GetCurrentSaveStateIdentity(&serial, &crc)) return;
    ARMSX2SetPatchEnableListForIdentity(values, serial, crc, section, key);
}

#pragma mark - Memory cards

+ (nonnull NSString *)memoryCardDirectory {
    FileSystem::CreateDirectoryPath(EmuFolders::MemoryCards.c_str(), false);
    return ARMSX2NSStringFromStdString(EmuFolders::MemoryCards);
}

+ (nonnull NSArray<NSString *> *)availableMemoryCards {
    [self memoryCardDirectory];

    std::vector<AvailableMcdInfo> cards = FileMcd_GetAvailableCards(true);
    NSMutableArray<NSString *> *names = [NSMutableArray arrayWithCapacity:cards.size()];
    for (const AvailableMcdInfo& card : cards) {
        NSString* name = ARMSX2NSStringFromStdString(card.name);
        if (name.length > 0)
            [names addObject:name];
    }

    return names;
}

+ (nullable NSString *)memoryCardNameForSlot:(NSInteger)slot {
    if (slot < 1 || slot > 8)
        return nil;

    const uint nativeSlot = static_cast<uint>(slot - 1);
    char key[32];
    std::snprintf(key, sizeof(key), "Slot%u_Filename", nativeSlot + 1);

    std::string value = EmuConfig.Mcd[nativeSlot].Filename;
    if (g_p44_settings_interface)
        value = g_p44_settings_interface->GetStringValue("MemoryCards", key, value.c_str());

    return ARMSX2NSStringFromStdString(value);
}

+ (void)setMemoryCardName:(nonnull NSString *)name forSlot:(NSInteger)slot enabled:(BOOL)enabled {
    if (slot < 1 || slot > 8)
        return;

    const uint nativeSlot = static_cast<uint>(slot - 1);
    const std::string nativeName(name.UTF8String ?: "");
    char enableKey[32];
    char fileKey[32];
    std::snprintf(enableKey, sizeof(enableKey), "Slot%u_Enable", nativeSlot + 1);
    std::snprintf(fileKey, sizeof(fileKey), "Slot%u_Filename", nativeSlot + 1);

    if (g_p44_settings_interface) {
        g_p44_settings_interface->SetBoolValue("MemoryCards", enableKey, enabled);
        g_p44_settings_interface->SetStringValue("MemoryCards", fileKey, nativeName.c_str());
        g_p44_settings_interface->Save();
    }

    EmuConfig.Mcd[nativeSlot].Enabled = enabled ? true : false;
    EmuConfig.Mcd[nativeSlot].Filename = nativeName;
    if (!enabled || nativeName.empty()) {
        EmuConfig.Mcd[nativeSlot].Type = MemoryCardType::Empty;
    } else if (const std::optional<AvailableMcdInfo> cardInfo = FileMcd_GetCardInfo(nativeName)) {
        EmuConfig.Mcd[nativeSlot].Type = cardInfo->type;
    } else {
        EmuConfig.Mcd[nativeSlot].Type = MemoryCardType::File;
    }

    NSLog(@"[ARMSX2Bridge] MemoryCard slot=%ld enabled=%d name=%@", static_cast<long>(slot), enabled ? 1 : 0, name);
}

+ (BOOL)createMemoryCardNamed:(nonnull NSString *)name sizeMB:(NSInteger)sizeMB folder:(BOOL)folder {
    [self memoryCardDirectory];

    NSString* sanitized = ARMSX2SanitizedMemoryCardName(name);
    if (sanitized.length == 0)
        return NO;

    const std::string nativeName(sanitized.UTF8String ?: "");
    const std::string fullPath(Path::Combine(EmuFolders::MemoryCards, nativeName));
    if (FileSystem::FileExists(fullPath.c_str()) || FileSystem::DirectoryExists(fullPath.c_str())) {
        NSLog(@"[ARMSX2Bridge] MemoryCard create refused, already exists: %@", sanitized);
        return NO;
    }

    const MemoryCardType cardType = folder ? MemoryCardType::Folder : MemoryCardType::File;
    const MemoryCardFileType fileType = folder ? MemoryCardFileType::Unknown : ARMSX2MemoryCardFileTypeForSizeMB(sizeMB);
    if (!folder && fileType == MemoryCardFileType::Unknown)
        return NO;

    const bool result = FileMcd_CreateNewCard(nativeName, cardType, fileType);
    NSLog(@"[ARMSX2Bridge] MemoryCard create name=%@ folder=%d size=%ld result=%d",
          sanitized, folder ? 1 : 0, static_cast<long>(sizeMB), result ? 1 : 0);
    return result ? YES : NO;
}

+ (BOOL)deleteMemoryCardNamed:(nonnull NSString *)name {
    [self memoryCardDirectory];

    const std::string nativeName(name.UTF8String ?: "");
    // Reject anything that looks like a path; names are single on-disk entries in the cards dir.
    if (nativeName.empty() ||
        nativeName.find('/') != std::string::npos ||
        nativeName.find('\\') != std::string::npos ||
        nativeName.find("..") != std::string::npos) {
        return NO;
    }

    const std::string fullPath(Path::Combine(EmuFolders::MemoryCards, nativeName));

    bool ok = false;
    if (FileSystem::FileExists(fullPath.c_str()))
        ok = FileSystem::DeleteFilePath(fullPath.c_str());
    else if (FileSystem::DirectoryExists(fullPath.c_str()))
        ok = FileSystem::RecursiveDeleteDirectory(fullPath.c_str());
    else
        return YES; // already gone — treat as success

    // Self-heal: clear any slot still pointing at the deleted card so it does not
    // reference a stale filename at the next VM boot.
    if (ok) {
        for (uint slot = 0; slot < sizeof(EmuConfig.Mcd) / sizeof(EmuConfig.Mcd[0]); slot++) {
            if (EmuConfig.Mcd[slot].Filename == nativeName)
                [self setMemoryCardName:@"" forSlot:static_cast<NSInteger>(slot + 1) enabled:NO];
        }
    }

    NSLog(@"[ARMSX2Bridge] MemoryCard delete name=%@ result=%d", name, ok ? 1 : 0);
    return ok ? YES : NO;
}

#pragma mark - DEV9 / Network

+ (nonnull NSArray<NSString *> *)dev9NetworkAdapters {
    NSMutableOrderedSet<NSString *> *adapters = [NSMutableOrderedSet orderedSetWithObject:@"Auto"];

    struct ifaddrs *interfaces = nullptr;
    if (getifaddrs(&interfaces) == 0) {
        for (struct ifaddrs *ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_name || !ifa->ifa_addr)
                continue;

            const sa_family_t family = ifa->ifa_addr->sa_family;
            if (family != AF_INET)
                continue;

            if ((ifa->ifa_flags & IFF_UP) == 0 || (ifa->ifa_flags & IFF_LOOPBACK) != 0)
                continue;

            [adapters addObject:[NSString stringWithUTF8String:ifa->ifa_name]];
        }
        freeifaddrs(interfaces);
    }

    return adapters.array;
}

#pragma mark - RetroAchievements

+ (nonnull NSDictionary<NSString *, id> *)retroAchievementsState {
    if (!ARMSX2RetroAchievementsHardcoreAvailable && EmuConfig.Achievements.HardcoreMode)
        ARMSX2ForceRetroAchievementsHardcoreOff();

    std::string username;
    std::string displayName;
    std::string avatarPath;
    std::string gameTitle;
    std::string richPresence;
    std::string gameIconPath;
    std::string gameIconURL;
    bool loggedIn = false;
    bool hasGame = false;
    bool active = false;
    bool hardcoreActive = false;
    bool hasAchievements = false;
    bool hasLeaderboards = false;
    bool hasRichPresence = false;
    u32 gameId = 0;
    u32 points = 0;
    u32 softcorePoints = 0;
    u32 unreadMessages = 0;
    u32 unlockedAchievements = 0;
    u32 totalAchievements = 0;
    u32 unlockedPoints = 0;
    u32 totalPoints = 0;
    const std::string savedUsernameValue = Host::GetBaseStringSettingValue("Achievements", "Username");
    const bool savedUsername = !savedUsernameValue.empty();
    const bool savedToken = !Host::GetStringSettingValue("Achievements", "Token").empty();

    Achievements::UserStats userStats;
    Achievements::GameStats gameStats;
    const bool haveUserStats = Achievements::GetCurrentUserStats(&userStats);
    const bool haveGameStats = Achievements::GetCurrentGameStats(&gameStats);

    {
        auto lock = Achievements::GetLock();
        active = Achievements::IsActive();
        if (haveUserStats) {
            username = userStats.username;
            displayName = userStats.display_name;
            avatarPath = userStats.avatar_path;
            points = userStats.points;
            softcorePoints = userStats.softcore_points;
            unreadMessages = userStats.unread_messages;
            loggedIn = !username.empty();
        } else if (active) {
            if (const char* loggedInUser = Achievements::GetLoggedInUserName()) {
                username = loggedInUser;
                displayName = username;
                loggedIn = !username.empty();
            }
        }
        if (active && loggedIn && avatarPath.empty())
            avatarPath = Achievements::GetLoggedInUserBadgePath();
        hasGame = Achievements::HasActiveGame();
        if (haveGameStats) {
            hasGame = true;
            gameTitle = gameStats.title;
            richPresence = gameStats.rich_presence;
            gameIconPath = gameStats.icon_path;
            gameIconURL = gameStats.icon_url;
            gameId = gameStats.game_id;
            unlockedAchievements = gameStats.unlocked_achievements;
            totalAchievements = gameStats.total_achievements;
            unlockedPoints = gameStats.unlocked_points;
            totalPoints = gameStats.total_points;
            hasAchievements = gameStats.has_achievements;
            hasLeaderboards = gameStats.has_leaderboards;
            hasRichPresence = gameStats.has_rich_presence;
        } else if (hasGame) {
            gameTitle = Achievements::GetGameTitle();
            richPresence = Achievements::GetRichPresenceString();
            gameIconURL = Achievements::GetGameIconURL();
            gameId = Achievements::GetGameID();
            hasAchievements = Achievements::HasAchievements();
            hasLeaderboards = Achievements::HasLeaderboards();
            hasRichPresence = Achievements::HasRichPresence();
        }
        hardcoreActive = Achievements::IsHardcoreModeActive();
    }

    const bool savedLogin = savedUsername && savedToken;
    const bool loginPending = EmuConfig.Achievements.Enabled && active && !loggedIn && savedLogin;
    if (username.empty() && savedUsername) {
        username = savedUsernameValue;
        displayName = savedUsernameValue;
    }

    std::fprintf(stderr, "@@RA_STATE@@ enabled=%d active=%d logged_in=%d saved_username=%d saved_token=%d login_pending=%d has_game=%d hardcore_pref=%d hardcore_active=%d\n",
        EmuConfig.Achievements.Enabled ? 1 : 0,
        active ? 1 : 0,
        loggedIn ? 1 : 0,
        savedUsername ? 1 : 0,
        savedToken ? 1 : 0,
        loginPending ? 1 : 0,
        hasGame ? 1 : 0,
        EmuConfig.Achievements.HardcoreMode ? 1 : 0,
        hardcoreActive ? 1 : 0);
    std::fflush(stderr);

    return @{
        @"supported": @(ARMSX2RetroAchievementsAvailable),
        @"hardcoreSupported": @(ARMSX2RetroAchievementsHardcoreAvailable),
        @"unavailableMessage": ARMSX2RetroAchievementsUnavailableMessage(),
        @"enabled": @(EmuConfig.Achievements.Enabled),
        @"active": @(active),
        @"loggedIn": @(loggedIn),
        @"savedLogin": @(savedLogin),
        @"loginPending": @(loginPending),
        @"username": ARMSX2NSStringFromStdString(username),
        @"displayName": ARMSX2NSStringFromStdString(displayName.empty() ? username : displayName),
        @"avatarPath": ARMSX2NSStringFromStdString(avatarPath),
        @"points": @(points),
        @"softcorePoints": @(softcorePoints),
        @"unreadMessages": @(unreadMessages),
        @"hardcorePreference": @(EmuConfig.Achievements.HardcoreMode),
        @"hardcoreActive": @(hardcoreActive),
        @"notifications": @(EmuConfig.Achievements.Notifications),
        @"leaderboardNotifications": @(EmuConfig.Achievements.LeaderboardNotifications),
        @"overlays": @(EmuConfig.Achievements.Overlays),
        @"hasActiveGame": @(hasGame),
        @"gameTitle": ARMSX2NSStringFromStdString(gameTitle),
        @"richPresence": ARMSX2NSStringFromStdString(richPresence),
        @"gameIconPath": ARMSX2NSStringFromStdString(gameIconPath),
        @"gameIconURL": ARMSX2NSStringFromStdString(gameIconURL),
        @"unlockedAchievements": @(unlockedAchievements),
        @"totalAchievements": @(totalAchievements),
        @"unlockedPoints": @(unlockedPoints),
        @"totalPoints": @(totalPoints),
        @"gameId": @(gameId),
        @"hasAchievements": @(hasAchievements),
        @"hasLeaderboards": @(hasLeaderboards),
        @"hasRichPresence": @(hasRichPresence),
    };
}

+ (nonnull NSArray<NSDictionary<NSString *, id> *> *)retroAchievementsForCurrentGame {
    std::vector<Achievements::AchievementInfo> achievements;
    if (!Achievements::GetCurrentAchievementList(&achievements))
        return @[];

    NSMutableArray<NSDictionary<NSString *, id> *> *result = [NSMutableArray arrayWithCapacity:achievements.size()];
    for (const Achievements::AchievementInfo& achievement : achievements) {
        [result addObject:@{
            @"id": @(achievement.id),
            @"title": ARMSX2NSStringFromStdString(achievement.title),
            @"description": ARMSX2NSStringFromStdString(achievement.description),
            @"badgePath": ARMSX2NSStringFromStdString(achievement.badge_path),
            @"measuredProgress": ARMSX2NSStringFromStdString(achievement.measured_progress),
            @"points": @(achievement.points),
            @"unlockTime": @(achievement.unlock_time),
            @"state": @(achievement.state),
            @"category": @(achievement.category),
            @"bucket": @(achievement.bucket),
            @"unlocked": @(achievement.unlocked),
            @"measuredPercent": @(achievement.measured_percent),
            @"rarity": @(achievement.rarity),
            @"rarityHardcore": @(achievement.rarity_hardcore),
        }];
    }

    return result;
}

+ (nullable NSDictionary<NSString *, id> *)consumePendingRetroAchievementsNotification {
    __block NSDictionary<NSString*, id>* pending = nil;
    void (^consume)(void) = ^{
        pending = s_pendingRetroAchievementsNotification;
        s_pendingRetroAchievementsNotification = nil;
    };

    if ([NSThread isMainThread]) {
        consume();
    } else {
        dispatch_sync(dispatch_get_main_queue(), consume);
    }

    std::fprintf(stderr, "@@RA_NOTIFY_CONSUME@@ pending=%d\n", pending ? 1 : 0);
    std::fflush(stderr);
    return pending;
}

+ (BOOL)isRetroAchievementsHardcoreActive {
    return ARMSX2RetroAchievementsHardcoreActive() ? YES : NO;
}

+ (void)setRetroAchievementsEnabled:(BOOL)enabled {
    const bool enable = enabled ? true : false;
    dispatch_async(ARMSX2RetroAchievementsQueue(), ^{
        if (EmuConfig.Achievements.Enabled == enable) {
            ARMSX2SaveBaseSettingBool("Achievements", "Enabled", enable);
            if (!enable || !ARMSX2RetroAchievementsHardcoreAvailable)
                ARMSX2SaveBaseSettingBool("Achievements", "ChallengeMode", false);
            ARMSX2_PostRetroAchievementsStateChanged();
            return;
        }

        ARMSX2UpdateAchievementsSettings(^{
            EmuConfig.Achievements.Enabled = enable;
            if (!enable || !ARMSX2RetroAchievementsHardcoreAvailable)
                EmuConfig.Achievements.HardcoreMode = false;
            ARMSX2SaveBaseSettingBool("Achievements", "Enabled", enable);
            if (!enable || !ARMSX2RetroAchievementsHardcoreAvailable)
                ARMSX2SaveBaseSettingBool("Achievements", "ChallengeMode", false);
        });
        NSLog(@"[ARMSX2Bridge] RetroAchievements enabled=%d", enable ? 1 : 0);
    });
}

+ (void)setRetroAchievementsHardcore:(BOOL)enabled {
    const bool enable = enabled ? true : false;
    dispatch_async(ARMSX2RetroAchievementsQueue(), ^{
        if (!ARMSX2RetroAchievementsHardcoreAvailable) {
            ARMSX2ForceRetroAchievementsHardcoreOff();
            ARMSX2_PostRetroAchievementsStateChanged();
            if (enable)
                NSLog(@"[ARMSX2Bridge] RetroAchievements hardcore rejected: unavailable");
            return;
        }

        if (EmuConfig.Achievements.HardcoreMode == enable) {
            ARMSX2SaveBaseSettingBool("Achievements", "ChallengeMode", enable);
            ARMSX2_PostRetroAchievementsStateChanged();
            return;
        }

        ARMSX2UpdateAchievementsSettings(^{
            EmuConfig.Achievements.HardcoreMode = enable;
            ARMSX2SaveBaseSettingBool("Achievements", "ChallengeMode", enable);
        });
        NSLog(@"[ARMSX2Bridge] RetroAchievements hardcore=%d", enable ? 1 : 0);
    });
}

+ (void)setRetroAchievementsNotifications:(BOOL)enabled {
    const bool enable = enabled ? true : false;
    dispatch_async(ARMSX2RetroAchievementsQueue(), ^{
        ARMSX2UpdateAchievementsSettings(^{
            EmuConfig.Achievements.Notifications = enable;
            ARMSX2SaveBaseSettingBool("Achievements", "Notifications", enable);
        });
    });
}

+ (void)setRetroAchievementsLeaderboards:(BOOL)enabled {
    const bool enable = enabled ? true : false;
    dispatch_async(ARMSX2RetroAchievementsQueue(), ^{
        ARMSX2UpdateAchievementsSettings(^{
            EmuConfig.Achievements.LeaderboardNotifications = enable;
            ARMSX2SaveBaseSettingBool("Achievements", "LeaderboardNotifications", enable);
        });
    });
}

+ (void)setRetroAchievementsOverlays:(BOOL)enabled {
    const bool enable = enabled ? true : false;
    dispatch_async(ARMSX2RetroAchievementsQueue(), ^{
        ARMSX2UpdateAchievementsSettings(^{
            EmuConfig.Achievements.Overlays = enable;
            ARMSX2SaveBaseSettingBool("Achievements", "Overlays", enable);
        });
    });
}

+ (void)loginRetroAchievementsWithUsername:(nonnull NSString *)username password:(nonnull NSString *)password completion:(nullable ARMSX2RetroAchievementsCompletion)completion {
    NSString* trimmedUsername = [username stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSString* nativePassword = password ?: @"";
    ARMSX2RetroAchievementsCompletion callback = [completion copy];

    if (trimmedUsername.length == 0 || nativePassword.length == 0) {
        if (callback)
            dispatch_async(dispatch_get_main_queue(), ^{ callback(NO, @"Enter your RetroAchievements username and password."); });
        return;
    }

    std::string user(trimmedUsername.UTF8String ?: "");
    std::string pass(nativePassword.UTF8String ?: "");

    NSLog(@"@@RA_LOGIN_NATIVE@@ requested username=%@ enabled=%d active=%d",
        trimmedUsername, EmuConfig.Achievements.Enabled ? 1 : 0, Achievements::IsActive() ? 1 : 0);

    dispatch_async(ARMSX2RetroAchievementsQueue(), ^{
        @autoreleasepool {
            if (!EmuConfig.Achievements.Enabled) {
                Pcsx2Config::AchievementsOptions old_config = EmuConfig.Achievements;
                EmuConfig.Achievements.Enabled = true;
                ARMSX2SaveBaseSettingBool("Achievements", "Enabled", true);
                Achievements::UpdateSettings(old_config);
            }

            if (!ARMSX2EnsureAchievementsClientInitialized()) {
                NSString* message = @"RetroAchievements could not initialize its network client.";
                NSLog(@"[ARMSX2Bridge] RetroAchievements login username=%@ result=0 message=%@", trimmedUsername, message);
                ARMSX2_PostRetroAchievementsStateChanged();
                if (callback)
                    dispatch_async(dispatch_get_main_queue(), ^{ callback(NO, message); });
                return;
            }

            Error error;
            const bool result = Achievements::Login(user.c_str(), pass.c_str(), &error);
            NSString* message = result ? @"RetroAchievements login successful." :
                (error.IsValid() ? ARMSX2NSStringFromStdString(error.GetDescription()) : @"RetroAchievements login failed.");

            if (result && g_p44_settings_interface)
                g_p44_settings_interface->Save();

            NSLog(@"@@RA_LOGIN_NATIVE@@ result=%d message=%@", result ? 1 : 0, message);
            NSLog(@"[ARMSX2Bridge] RetroAchievements login username=%@ result=%d message=%@", trimmedUsername, result ? 1 : 0, message);
            ARMSX2_PostRetroAchievementsStateChanged();

            if (callback)
                dispatch_async(dispatch_get_main_queue(), ^{ callback(result ? YES : NO, message); });
        }
    });
}

+ (void)logoutRetroAchievements {
    dispatch_async(ARMSX2RetroAchievementsQueue(), ^{
        Achievements::Logout();
        if (g_p44_settings_interface)
            g_p44_settings_interface->Save();
        NSLog(@"[ARMSX2Bridge] RetroAchievements logout");
        ARMSX2_PostRetroAchievementsStateChanged();
    });
}

// Gamepad button mapping
extern std::atomic<bool> s_captureMode;
extern std::atomic<int>  s_capturedButton;
extern int s_buttonMap[16];

+ (void)startButtonCapture {
    s_capturedButton.store(-1);
    s_captureMode.store(true);
}

+ (void)stopButtonCapture {
    s_captureMode.store(false);
}

// Poll SDL gamepad from main thread (for settings screen when VM is not running)
+ (void)pollGamepadForCapture {
    if (!s_captureMode.load()) return;
    SDL_UpdateGamepads();
    // Keep gamepad open across polls to avoid open/close overhead
    static SDL_Gamepad* s_settingsGP = nullptr;
    if (!s_settingsGP) {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (ids && count > 0) s_settingsGP = SDL_OpenGamepad(ids[0]);
        SDL_free(ids);
    }
    if (!s_settingsGP) return;
    if (!SDL_GamepadConnected(s_settingsGP)) {
        SDL_CloseGamepad(s_settingsGP);
        s_settingsGP = nullptr;
        return;
    }
    // SDL_PumpEvents required for GCController input to be processed
    SDL_PumpEvents();
    SDL_UpdateGamepads();
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        if (SDL_GetGamepadButton(s_settingsGP, (SDL_GamepadButton)b)) {
            s_capturedButton.store(b);
            break;
        }
    }
}

+ (int)capturedButton {
    return s_capturedButton.exchange(-1);
}

+ (void)setButtonMapping:(int)ps2Index toSDLButton:(int)sdlButton {
    if (ps2Index >= 0 && ps2Index < 16) {
        s_buttonMap[ps2Index] = sdlButton;
        // Persist to INI
        if (g_p44_settings_interface) {
            char key[32];
            snprintf(key, sizeof(key), "Button%d", ps2Index);
            g_p44_settings_interface->SetIntValue("ARMSX2iOS/GamepadMapping", key, sdlButton);
            g_p44_settings_interface->Save();
        }
    }
}

+ (int)getButtonMapping:(int)ps2Index {
    if (ps2Index >= 0 && ps2Index < 16) return s_buttonMap[ps2Index];
    return -1;
}

+ (void)resetButtonMappings {
    static const int defMap[16] = {
        SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN,
        SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        -1, -1,
        SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_BACK,
        SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    };
    for (int i = 0; i < 16; i++) s_buttonMap[i] = defMap[i];
    if (g_p44_settings_interface) {
        g_p44_settings_interface->RemoveSection("ARMSX2iOS/GamepadMapping");
        g_p44_settings_interface->Save();
    }
}

@end
