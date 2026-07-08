// GamepadHaptics.mm — iOS gamepad + haptics subsystem.

#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include <SDL3/SDL.h>

#include "common/Console.h"
#include "pcsx2/Config.h"          // EmuConfig, GSConfig
#include "pcsx2/SIO/Pad/Pad.h"
#include "pcsx2/SIO/Pad/PadDualshock2.h"

#include "IOSRuntime.h"
#import "ARMSX2Bridge.h"

#pragma mark - Gamepad state
SDL_Gamepad* s_gamepads[ARMSX2_MAX_IOS_GAMEPADS] = {};
static std::atomic<u32> s_pendingGamepadRumble[ARMSX2_MAX_IOS_GAMEPADS];
static std::atomic<u32> s_gamepadRumbleStopGeneration[ARMSX2_MAX_IOS_GAMEPADS];
static u32 s_appliedGamepadRumble[ARMSX2_MAX_IOS_GAMEPADS] = {};
static bool s_appliedGamepadRumbleValid[ARMSX2_MAX_IOS_GAMEPADS] = {};
static bool s_loggedGamepadRumbleFailure = false;
static bool s_loggedSDLGamepadRumble = false;
static bool s_loggedSDLGamepadRumbleForceStop = false;
static std::atomic<u32> s_loggedPadRumbleCommandCount{0};
static std::atomic<u32> s_loggedIgnoredPadRumbleCount{0};
static bool s_loggedMultitapRestartNeeded = false;
static constexpr u32 ARMSX2_GAMEPAD_RUMBLE_DURATION_MS = 220;
static constexpr double ARMSX2_GAMEPAD_RUMBLE_FORCE_STOP_SECONDS = 0.30;
static constexpr u16 ARMSX2_GAMEPAD_RUMBLE_MAX_INTENSITY = 0x7000;

static CHHapticEngine* s_nativePulseHapticEngine[ARMSX2_MAX_IOS_GAMEPADS] = {};
static std::atomic<u32> s_nativePulseHapticStopGeneration[ARMSX2_MAX_IOS_GAMEPADS];
static std::atomic<u32> s_loggedNativePulseHapticEvents{0};
static GCController* s_nativeHapticController = nil;
static CHHapticEngine* s_nativeHapticEngine = nil;
static id<CHHapticAdvancedPatternPlayer> s_nativeHapticPlayer = nil;
static u32 s_nativeAppliedGamepadRumble = 0;
static bool s_nativeAppliedGamepadRumbleValid = false;
static bool s_loggedNativeGamepadRumbleReady = false;
static bool s_loggedNativeGamepadRumbleUnavailable = false;
static std::atomic<u8> s_nativeGamepadDpadMask[ARMSX2_MAX_IOS_GAMEPADS];
static std::atomic<u8> s_nativeGamepadDpadLatchedMask[ARMSX2_MAX_IOS_GAMEPADS];
static std::atomic<u8> s_nativeGamepadAnyDpadMask{0};
static std::atomic<u8> s_nativeGamepadAnyDpadLatchedMask{0};
static std::atomic<u32> s_loggedNativeGamepadDpadEvents{0};
static std::atomic<u32> s_loggedNativeGamepadDpadApplyEvents{0};
static std::atomic<u32> s_loggedJoyConRumbleSkipped{0};
static id s_nativeGamepadConnectObserver = nil;
static id s_nativeGamepadDisconnectObserver = nil;

#pragma mark - Native D-pad
enum : u8
{
    ARMSX2_NATIVE_DPAD_UP = 1 << 0,
    ARMSX2_NATIVE_DPAD_DOWN = 1 << 1,
    ARMSX2_NATIVE_DPAD_LEFT = 1 << 2,
    ARMSX2_NATIVE_DPAD_RIGHT = 1 << 3,
};

static u8 ARMSX2NativeDpadBitForPS2Button(u32 ps2_button)
{
    switch (ps2_button)
    {
        case PadDualshock2::Inputs::PAD_UP:
            return ARMSX2_NATIVE_DPAD_UP;
        case PadDualshock2::Inputs::PAD_DOWN:
            return ARMSX2_NATIVE_DPAD_DOWN;
        case PadDualshock2::Inputs::PAD_LEFT:
            return ARMSX2_NATIVE_DPAD_LEFT;
        case PadDualshock2::Inputs::PAD_RIGHT:
            return ARMSX2_NATIVE_DPAD_RIGHT;
        default:
            return 0;
    }
}

static void ARMSX2RecomputeNativeGamepadAnyDpadMask()
{
    u8 any_mask = 0;
    for (u32 slot = 0; slot < ARMSX2_MAX_IOS_GAMEPADS; slot++)
        any_mask |= s_nativeGamepadDpadMask[slot].load(std::memory_order_relaxed);

    s_nativeGamepadAnyDpadMask.store(any_mask, std::memory_order_relaxed);
}

static void ARMSX2RecomputeNativeGamepadAnyDpadLatchedMask()
{
    u8 any_mask = 0;
    for (u32 slot = 0; slot < ARMSX2_MAX_IOS_GAMEPADS; slot++)
        any_mask |= s_nativeGamepadDpadLatchedMask[slot].load(std::memory_order_relaxed);

    s_nativeGamepadAnyDpadLatchedMask.store(any_mask, std::memory_order_relaxed);
}

static u8 ARMSX2NativeDpadMaskForDirectionPad(GCControllerDirectionPad* dpad)
{
    if (!dpad)
        return 0;

    u8 mask = 0;
    if (dpad.up.pressed || dpad.yAxis.value > 0.35f)
        mask |= ARMSX2_NATIVE_DPAD_UP;
    if (dpad.down.pressed || dpad.yAxis.value < -0.35f)
        mask |= ARMSX2_NATIVE_DPAD_DOWN;
    if (dpad.left.pressed || dpad.xAxis.value < -0.35f)
        mask |= ARMSX2_NATIVE_DPAD_LEFT;
    if (dpad.right.pressed || dpad.xAxis.value > 0.35f)
        mask |= ARMSX2_NATIVE_DPAD_RIGHT;

    return mask;
}

static GCControllerDirectionPad* ARMSX2NativeDpadForController(GCController* controller)
{
    if (!controller)
        return nil;

    GCExtendedGamepad* extended = controller.extendedGamepad;
    if (extended && extended.dpad)
        return extended.dpad;

    GCPhysicalInputProfile* profile = controller.physicalInputProfile;
    if (profile && [profile respondsToSelector:@selector(dpads)]) {
        NSDictionary<NSString*, GCControllerDirectionPad*>* dpads = profile.dpads;
        for (NSString* key in dpads) {
            GCControllerDirectionPad* dpad = dpads[key];
            if (dpad)
                return dpad;
        }
    }

    return nil;
}

static void ARMSX2SetNativeGamepadDpadBit(u32 slot, u8 bit, bool pressed, const char* direction)
{
    if (slot >= ARMSX2_MAX_IOS_GAMEPADS || bit == 0)
        return;

    if (pressed) {
        s_nativeGamepadDpadLatchedMask[slot].fetch_or(bit, std::memory_order_relaxed);
        ARMSX2RecomputeNativeGamepadAnyDpadLatchedMask();
    }

    const u8 old_mask = s_nativeGamepadDpadMask[slot].load(std::memory_order_relaxed);
    const u8 new_mask = pressed ? (old_mask | bit) : (old_mask & ~bit);
    if (new_mask == old_mask)
        return;

    s_nativeGamepadDpadMask[slot].store(new_mask, std::memory_order_relaxed);
    ARMSX2RecomputeNativeGamepadAnyDpadMask();
    const u32 log_index = s_loggedNativeGamepadDpadEvents.fetch_add(1, std::memory_order_relaxed);
    if (log_index < 24) {
        Console.WriteLn("[ARMSX2 iOS Gamepad] Native dpad slot=%u dir=%s pressed=%u mask=0x%02x",
            slot + 1, direction ? direction : "unknown", pressed ? 1 : 0, new_mask);
    }
}

static void ARMSX2PollNativeGamepadDpadMasks(const char* reason)
{
    NSArray<GCController*>* controllers = [GCController controllers];
    u8 any_mask = 0;
    u32 slot = 0;
    for (GCController* controller in controllers) {
        if (slot >= ARMSX2_MAX_IOS_GAMEPADS)
            break;

        const u8 mask = ARMSX2NativeDpadMaskForDirectionPad(ARMSX2NativeDpadForController(controller));
        s_nativeGamepadDpadMask[slot].store(mask, std::memory_order_relaxed);
        if (mask != 0)
            s_nativeGamepadDpadLatchedMask[slot].fetch_or(mask, std::memory_order_relaxed);
        any_mask |= mask;
        slot++;
    }

    for (; slot < ARMSX2_MAX_IOS_GAMEPADS; slot++) {
        s_nativeGamepadDpadMask[slot].store(0, std::memory_order_relaxed);
        s_nativeGamepadDpadLatchedMask[slot].store(0, std::memory_order_relaxed);
    }

    s_nativeGamepadAnyDpadMask.store(any_mask, std::memory_order_relaxed);
    ARMSX2RecomputeNativeGamepadAnyDpadLatchedMask();

    static std::atomic<u32> s_loggedNativeGamepadPolls{0};
    const u32 log_index = s_loggedNativeGamepadPolls.fetch_add(1, std::memory_order_relaxed);
    if (log_index < 8) {
        Console.WriteLn("[ARMSX2 iOS Gamepad] Native dpad poll reason=%s controllers=%u any=0x%02x",
            reason ? reason : "poll", static_cast<unsigned>(controllers.count), any_mask);
    }
}

static void ARMSX2RefreshNativeGamepadDpadHandlersOnMain(const char* reason)
{
    for (u32 slot = 0; slot < ARMSX2_MAX_IOS_GAMEPADS; slot++) {
        s_nativeGamepadDpadMask[slot].store(0, std::memory_order_relaxed);
        s_nativeGamepadDpadLatchedMask[slot].store(0, std::memory_order_relaxed);
    }
    s_nativeGamepadAnyDpadMask.store(0, std::memory_order_relaxed);
    s_nativeGamepadAnyDpadLatchedMask.store(0, std::memory_order_relaxed);

    NSArray<GCController*>* controllers = [GCController controllers];
    u32 slot = 0;
    for (GCController* controller in controllers) {
        if (slot >= ARMSX2_MAX_IOS_GAMEPADS)
            break;

        GCControllerDirectionPad* dpad = ARMSX2NativeDpadForController(controller);
        if (!dpad) {
            slot++;
            continue;
        }

        const u32 controller_slot = slot;
        const u8 initial_mask = ARMSX2NativeDpadMaskForDirectionPad(dpad);
        s_nativeGamepadDpadMask[controller_slot].store(initial_mask, std::memory_order_relaxed);
        if (initial_mask != 0)
            s_nativeGamepadDpadLatchedMask[controller_slot].store(initial_mask, std::memory_order_relaxed);

        dpad.up.pressedChangedHandler = ^(GCControllerButtonInput* button, float value, BOOL pressed) {
            ARMSX2SetNativeGamepadDpadBit(controller_slot, ARMSX2_NATIVE_DPAD_UP, pressed, "up");
        };
        dpad.down.pressedChangedHandler = ^(GCControllerButtonInput* button, float value, BOOL pressed) {
            ARMSX2SetNativeGamepadDpadBit(controller_slot, ARMSX2_NATIVE_DPAD_DOWN, pressed, "down");
        };
        dpad.left.pressedChangedHandler = ^(GCControllerButtonInput* button, float value, BOOL pressed) {
            ARMSX2SetNativeGamepadDpadBit(controller_slot, ARMSX2_NATIVE_DPAD_LEFT, pressed, "left");
        };
        dpad.right.pressedChangedHandler = ^(GCControllerButtonInput* button, float value, BOOL pressed) {
            ARMSX2SetNativeGamepadDpadBit(controller_slot, ARMSX2_NATIVE_DPAD_RIGHT, pressed, "right");
        };
        dpad.valueChangedHandler = ^(GCControllerDirectionPad* directionPad, float xValue, float yValue) {
            ARMSX2SetNativeGamepadDpadBit(controller_slot, ARMSX2_NATIVE_DPAD_UP, yValue > 0.35f, "up-axis");
            ARMSX2SetNativeGamepadDpadBit(controller_slot, ARMSX2_NATIVE_DPAD_DOWN, yValue < -0.35f, "down-axis");
            ARMSX2SetNativeGamepadDpadBit(controller_slot, ARMSX2_NATIVE_DPAD_LEFT, xValue < -0.35f, "left-axis");
            ARMSX2SetNativeGamepadDpadBit(controller_slot, ARMSX2_NATIVE_DPAD_RIGHT, xValue > 0.35f, "right-axis");
        };

        NSString* vendor = controller.vendorName ?: @"unknown";
        NSString* product = @"";
        if ([controller respondsToSelector:@selector(productCategory)])
            product = controller.productCategory ?: @"";
        Console.WriteLn("[ARMSX2 iOS Gamepad] Native dpad fallback slot=%u vendor=%s category=%s reason=%s",
            controller_slot + 1, vendor.UTF8String, product.UTF8String, reason ? reason : "refresh");

        slot++;
    }

    ARMSX2RecomputeNativeGamepadAnyDpadMask();
    ARMSX2RecomputeNativeGamepadAnyDpadLatchedMask();
}

void ARMSX2InstallNativeGamepadDpadObserversOnMain()
{
    if (s_nativeGamepadConnectObserver)
        return;

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    s_nativeGamepadConnectObserver = [center addObserverForName:GCControllerDidConnectNotification
                                                         object:nil
                                                          queue:[NSOperationQueue mainQueue]
                                                     usingBlock:^(NSNotification* notification) {
        ARMSX2RefreshNativeGamepadDpadHandlersOnMain("native-connect");
    }];
    s_nativeGamepadDisconnectObserver = [center addObserverForName:GCControllerDidDisconnectNotification
                                                            object:nil
                                                             queue:[NSOperationQueue mainQueue]
                                                        usingBlock:^(NSNotification* notification) {
        ARMSX2RefreshNativeGamepadDpadHandlersOnMain("native-disconnect");
    }];
    ARMSX2RefreshNativeGamepadDpadHandlersOnMain("observer-install");
}

#pragma mark - Multitap
enum class ARMSX2IOSMultitapMode : int
{
    Auto = 0,
    Disabled = 1,
    Port1 = 2,
    Port2 = 3,
    Both = 4,
};

static ARMSX2IOSMultitapMode ARMSX2GetIOSMultitapMode()
{
    if (!s_settings_interface)
        return ARMSX2IOSMultitapMode::Auto;

    const int value = s_settings_interface->GetIntValue("ARMSX2iOS/Gamepad", "MultitapMode", 0);
    switch (value)
    {
        case 1:
            return ARMSX2IOSMultitapMode::Disabled;
        case 2:
            return ARMSX2IOSMultitapMode::Port1;
        case 3:
            return ARMSX2IOSMultitapMode::Port2;
        case 4:
            return ARMSX2IOSMultitapMode::Both;
        default:
            return ARMSX2IOSMultitapMode::Auto;
    }
}

static const char* ARMSX2IOSMultitapModeName(ARMSX2IOSMultitapMode mode)
{
    switch (mode)
    {
        case ARMSX2IOSMultitapMode::Disabled:
            return "Disabled";
        case ARMSX2IOSMultitapMode::Port1:
            return "Port 1";
        case ARMSX2IOSMultitapMode::Port2:
            return "Port 2";
        case ARMSX2IOSMultitapMode::Both:
            return "Port 1 + Port 2";
        case ARMSX2IOSMultitapMode::Auto:
        default:
            return "Auto";
    }
}

static bool ARMSX2IOSMultitapUsesPort1(ARMSX2IOSMultitapMode mode, u32 detected_controllers)
{
    switch (mode)
    {
        case ARMSX2IOSMultitapMode::Auto:
            return detected_controllers > 2;
        case ARMSX2IOSMultitapMode::Port1:
        case ARMSX2IOSMultitapMode::Both:
            return true;
        default:
            return false;
    }
}

static bool ARMSX2IOSMultitapUsesPort2(ARMSX2IOSMultitapMode mode)
{
    return mode == ARMSX2IOSMultitapMode::Port2 || mode == ARMSX2IOSMultitapMode::Both;
}

static bool ARMSX2IOSMapsPort1Multitap(ARMSX2IOSMultitapMode mode)
{
    if (mode == ARMSX2IOSMultitapMode::Auto)
        return EmuConfig.Pad.MultitapPort0_Enabled;

    return mode == ARMSX2IOSMultitapMode::Port1 || mode == ARMSX2IOSMultitapMode::Both;
}

static bool ARMSX2IOSMapsPort2Multitap(ARMSX2IOSMultitapMode mode)
{
    if (mode == ARMSX2IOSMultitapMode::Auto)
        return false;

    return ARMSX2IOSMultitapUsesPort2(mode);
}

static void ARMSX2EnsureIOSPadType(u32 unified_slot)
{
    if (!s_settings_interface || unified_slot >= Pad::NUM_CONTROLLER_PORTS)
        return;

    const std::string section = Pad::GetConfigSection(unified_slot);
    const std::string type = s_settings_interface->GetStringValue(section.c_str(), "Type", "");
    if (type.empty() || type == "None" || type == "NotConnected")
        s_settings_interface->SetStringValue(section.c_str(), "Type", "DualShock2");
}

static u32 ARMSX2DetectedSDLGamepadCount()
{
    SDL_PumpEvents();
    SDL_UpdateGamepads();
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    SDL_free(ids);
    return static_cast<u32>(std::max(count, 0));
}

void ARMSX2ApplyIOSMultitapConfig(const char* reason)
{
    if (!s_settings_interface)
        return;

    const ARMSX2IOSMultitapMode mode = ARMSX2GetIOSMultitapMode();
    const u32 detected = ARMSX2DetectedSDLGamepadCount();
    const bool port1 = ARMSX2IOSMultitapUsesPort1(mode, detected);
    const bool port2 = ARMSX2IOSMultitapUsesPort2(mode);

    s_settings_interface->SetBoolValue("Pad", "MultitapPort1", port1);
    s_settings_interface->SetBoolValue("Pad", "MultitapPort2", port2);
    EmuConfig.Pad.MultitapPort0_Enabled = port1;
    EmuConfig.Pad.MultitapPort1_Enabled = port2;
    s_loggedMultitapRestartNeeded = false;

    for (u32 controller = 0; controller < std::min<u32>(detected, ARMSX2_MAX_IOS_GAMEPADS); controller++) {
        u32 unified_slot = controller;
        if (port1) {
            unified_slot = (controller == 0) ? 0 : controller + 1;
        } else if (port2) {
            unified_slot = (controller <= 1) ? controller : controller + 3;
        } else if (controller > 1) {
            continue;
        }

        ARMSX2EnsureIOSPadType(unified_slot);
    }

    s_settings_interface->Save();
    Console.WriteLn("[ARMSX2 iOS Gamepad] Multitap mode=%s detected=%u port1=%d port2=%d reason=%s",
        ARMSX2IOSMultitapModeName(mode), detected, port1 ? 1 : 0, port2 ? 1 : 0, reason ? reason : "unknown");
}

#pragma mark - CoreHaptics rumble
static u32 ARMSX2PackGamepadRumble(float large_intensity, float small_intensity)
{
    const u32 large = static_cast<u32>(std::clamp(large_intensity, 0.0f, 1.0f) * 65535.0f);
    const u32 small = static_cast<u32>(std::clamp(small_intensity, 0.0f, 1.0f) * 65535.0f);
    return ((large & 0xffffu) << 16) | (small & 0xffffu);
}

static float ARMSX2RumbleLargeIntensity(u32 packed)
{
    return static_cast<float>((packed >> 16) & 0xffffu) / 65535.0f;
}

static float ARMSX2RumbleSmallIntensity(u32 packed)
{
    return static_cast<float>(packed & 0xffffu) / 65535.0f;
}

static u32 ARMSX2ConnectedGamepadCount()
{
    u32 count = 0;
    for (SDL_Gamepad* gamepad : s_gamepads)
    {
        if (gamepad && SDL_GamepadConnected(gamepad))
            count++;
    }
    return count;
}

unsigned int ARMSX2PadSlotForGamepadIndex(unsigned int gamepad_index)
{
    if (gamepad_index == 0)
        return 0;

    const ARMSX2IOSMultitapMode mode = ARMSX2GetIOSMultitapMode();

    // Two controllers should behave like normal PS2 ports 1/2. Three or four
    // controllers default to Port 1 multitap, which maps to 1A/1B/1C/1D.
    if (ARMSX2IOSMapsPort1Multitap(mode))
        return gamepad_index + 1;

    // Port 2 multitap is an escape hatch for games that look there instead:
    // controller 2 remains 2A, controller 3/4 become 2B/2C.
    if (ARMSX2IOSMapsPort2Multitap(mode)) {
        if (gamepad_index == 1)
            return 1;
        return gamepad_index + 3;
    }

    return (gamepad_index <= 1) ? gamepad_index : 0xffffffffu;
}

static int ARMSX2GamepadIndexForPadSlot(u32 pad_index)
{
    if (pad_index == 0)
        return 0;

    const ARMSX2IOSMultitapMode mode = ARMSX2GetIOSMultitapMode();
    if (ARMSX2IOSMapsPort1Multitap(mode)) {
        if (pad_index >= 2 && pad_index <= 4)
            return static_cast<int>(pad_index - 1);
        return -1;
    }

    if (ARMSX2IOSMapsPort2Multitap(mode)) {
        if (pad_index == 1)
            return 1;
        if (pad_index >= 5 && pad_index <= 6)
            return static_cast<int>(pad_index - 3);
        return -1;
    }

    return (pad_index == 1) ? 1 : -1;
}

extern "C" void ARMSX2_iOSUpdatePadVibration(u32 pad_index, float large_intensity, float small_intensity)
{
    const int gamepad_index = ARMSX2GamepadIndexForPadSlot(pad_index);
    if (gamepad_index < 0 || static_cast<u32>(gamepad_index) >= ARMSX2_MAX_IOS_GAMEPADS) {
        const u32 count = s_loggedIgnoredPadRumbleCount.fetch_add(1, std::memory_order_relaxed);
        if (count < 4)
            Console.WriteLn("[ARMSX2 iOS Gamepad] Ignoring rumble for unmapped pad=%u large=%.3f small=%.3f", pad_index, large_intensity, small_intensity);
        return;
    }

    const u32 packed = ARMSX2PackGamepadRumble(large_intensity, small_intensity);
    if (packed != 0) {
        const u32 count = s_loggedPadRumbleCommandCount.fetch_add(1, std::memory_order_relaxed);
        if (count < 12)
            Console.WriteLn("[ARMSX2 iOS Gamepad] Queued rumble pad=%u controller=%d large=%.3f small=%.3f",
                pad_index, gamepad_index + 1, large_intensity, small_intensity);
    }

    s_pendingGamepadRumble[gamepad_index].store(packed, std::memory_order_relaxed);
}

static void ARMSX2StopNativeGamepadRumbleOnMain()
{
    if (s_nativeHapticPlayer) {
        NSError* error = nil;
        [s_nativeHapticPlayer stopAtTime:CHHapticTimeImmediate error:&error];
        if (error)
            Console.WriteLn("[ARMSX2 iOS Gamepad] Native rumble stop failed: %s", error.localizedDescription.UTF8String ?: "unknown");
        s_nativeHapticPlayer = nil;
    }
}

static void ARMSX2ResetNativeGamepadRumbleOnMain()
{
    ARMSX2StopNativeGamepadRumbleOnMain();
    if (s_nativeHapticEngine) {
        [s_nativeHapticEngine stopWithCompletionHandler:nil];
        s_nativeHapticEngine = nil;
    }
    s_nativeHapticController = nil;
    s_nativeAppliedGamepadRumble = 0;
    s_nativeAppliedGamepadRumbleValid = false;
    s_loggedNativeGamepadRumbleReady = false;
}

static GCController* ARMSX2FindNativeHapticController()
{
    for (GCController* controller in [GCController controllers]) {
        if (controller.haptics)
            return controller;
    }

    return nil;
}

static bool ARMSX2EnsureNativeGamepadRumbleOnMain(float intensity, float sharpness)
{
    if (@available(iOS 14.0, *)) {
    } else {
        return false;
    }

    GCController* controller = ARMSX2FindNativeHapticController();
    if (!controller) {
        if (!s_loggedNativeGamepadRumbleUnavailable) {
            Console.WriteLn("[ARMSX2 iOS Gamepad] Native controller haptics unavailable");
            s_loggedNativeGamepadRumbleUnavailable = true;
        }
        ARMSX2ResetNativeGamepadRumbleOnMain();
        return false;
    }

    if (s_nativeHapticController != controller) {
        ARMSX2ResetNativeGamepadRumbleOnMain();
        s_nativeHapticController = controller;
    }

    if (!s_nativeHapticEngine) {
        s_nativeHapticEngine = [controller.haptics createEngineWithLocality:GCHapticsLocalityAll];
        if (!s_nativeHapticEngine) {
            if (!s_loggedNativeGamepadRumbleUnavailable) {
                Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic engine creation failed");
                s_loggedNativeGamepadRumbleUnavailable = true;
            }
            return false;
        }

        s_nativeHapticEngine.playsHapticsOnly = YES;
        s_nativeHapticEngine.autoShutdownEnabled = YES;
        s_nativeHapticEngine.stoppedHandler = ^(CHHapticEngineStoppedReason reason) {
            s_nativeHapticPlayer = nil;
            s_nativeAppliedGamepadRumble = 0;
            s_nativeAppliedGamepadRumbleValid = false;
            Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic engine stopped reason=%ld", static_cast<long>(reason));
        };
        s_nativeHapticEngine.resetHandler = ^{
            s_nativeHapticPlayer = nil;
            s_nativeAppliedGamepadRumble = 0;
            s_nativeAppliedGamepadRumbleValid = false;
            Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic engine reset");
        };
    }

    NSError* error = nil;
    if (![s_nativeHapticEngine startAndReturnError:&error]) {
        Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic engine start failed: %s", error.localizedDescription.UTF8String ?: "unknown");
        return false;
    }

    if (!s_nativeHapticPlayer) {
        NSArray<CHHapticEventParameter*>* params = @[
            [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:intensity],
            [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:sharpness]
        ];
        CHHapticEvent* event = [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
                                                              parameters:params
                                                            relativeTime:0.0
                                                                 duration:1.0];
        CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[event] parameters:@[] error:&error];
        if (!pattern) {
            Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic pattern failed: %s", error.localizedDescription.UTF8String ?: "unknown");
            return false;
        }

        s_nativeHapticPlayer = [s_nativeHapticEngine createAdvancedPlayerWithPattern:pattern error:&error];
        if (!s_nativeHapticPlayer) {
            Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic player failed: %s", error.localizedDescription.UTF8String ?: "unknown");
            return false;
        }

        s_nativeHapticPlayer.loopEnabled = YES;
        s_nativeHapticPlayer.loopEnd = 1.0;
        if (![s_nativeHapticPlayer startAtTime:CHHapticTimeImmediate error:&error]) {
            Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic player start failed: %s", error.localizedDescription.UTF8String ?: "unknown");
            s_nativeHapticPlayer = nil;
            return false;
        }

        if (!s_loggedNativeGamepadRumbleReady) {
            NSString* vendor = controller.vendorName ?: @"unknown controller";
            Console.WriteLn("[ARMSX2 iOS Gamepad] Native controller rumble active: %s", vendor.UTF8String);
            s_loggedNativeGamepadRumbleReady = true;
        }
    }

    const float sharpnessControl = std::clamp((sharpness * 2.0f) - 1.0f, -1.0f, 1.0f);
    NSArray<CHHapticDynamicParameter*>* dynamicParams = @[
        [[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl value:intensity relativeTime:0.0],
        [[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticSharpnessControl value:sharpnessControl relativeTime:0.0]
    ];

    if (![s_nativeHapticPlayer sendParameters:dynamicParams atTime:CHHapticTimeImmediate error:&error]) {
        Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic update failed: %s", error.localizedDescription.UTF8String ?: "unknown");
        ARMSX2StopNativeGamepadRumbleOnMain();
        return false;
    }

    return true;
}

static void ARMSX2ApplyNativeGamepadRumbleOnMain(u32 packed)
{
	if (s_nativeAppliedGamepadRumbleValid && packed == s_nativeAppliedGamepadRumble)
		return;

    const float large = ARMSX2RumbleLargeIntensity(packed);
    const float small = ARMSX2RumbleSmallIntensity(packed);
    const float intensity = std::clamp(std::max(large, small), 0.0f, 1.0f);
    if (intensity <= 0.01f) {
        ARMSX2StopNativeGamepadRumbleOnMain();
        s_nativeAppliedGamepadRumble = packed;
        s_nativeAppliedGamepadRumbleValid = true;
        return;
    }

    const float sharpness = std::clamp(0.20f + (small * 0.65f) - (large * 0.10f), 0.0f, 1.0f);
    if (ARMSX2EnsureNativeGamepadRumbleOnMain(intensity, sharpness)) {
        s_nativeAppliedGamepadRumble = packed;
        s_nativeAppliedGamepadRumbleValid = true;
	}
}

static void ARMSX2StopNativeGamepadRumblePulseOnMain(u32 slot)
{
	if (slot >= ARMSX2_MAX_IOS_GAMEPADS)
		return;

	s_nativePulseHapticStopGeneration[slot].fetch_add(1, std::memory_order_relaxed);
	if (s_nativePulseHapticEngine[slot]) {
		@try {
			[s_nativePulseHapticEngine[slot] stopWithCompletionHandler:nil];
		} @catch (NSException* exception) {
			Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic stop exception slot=%u name=%s reason=%s",
				slot + 1,
				exception.name.UTF8String ?: "unknown",
				exception.reason.UTF8String ?: "unknown");
		}
		s_nativePulseHapticEngine[slot] = nil;
	}
}

static GCController* ARMSX2FindNativeHapticControllerForSlot(u32 slot)
{
	NSArray<GCController*>* controllers = [GCController controllers];
	if (slot < controllers.count) {
		GCController* controller = controllers[slot];
		if (controller.haptics)
			return controller;
	}

	if (controllers.count == 1) {
		GCController* controller = controllers.firstObject;
		if (controller.haptics)
			return controller;
	}

	for (GCController* controller in controllers) {
		if (controller.haptics)
			return controller;
	}

	return nil;
}

static GCController* ARMSX2FindNativeControllerForSlot(u32 slot)
{
	NSArray<GCController*>* controllers = [GCController controllers];
	if (slot < controllers.count)
		return controllers[slot];

	if (controllers.count == 1)
		return controllers.firstObject;

	return nil;
}

static bool ARMSX2NativeControllerLooksLikeJoyCon(GCController* controller)
{
	if (!controller)
		return false;

	NSString* vendor = controller.vendorName ?: @"";
	NSString* category = @"";
	if ([controller respondsToSelector:@selector(productCategory)])
		category = controller.productCategory ?: @"";

	NSString* descriptor = [[NSString stringWithFormat:@"%@ %@", vendor, category] lowercaseString];
	return [descriptor containsString:@"joy-con"] ||
	       [descriptor containsString:@"joycon"] ||
	       [descriptor containsString:@"joy con"];
}

static bool ARMSX2CStringLooksLikeJoyCon(const char* value)
{
	if (!value || !*value)
		return false;

	NSString* descriptor = [NSString stringWithUTF8String:value];
	if (!descriptor)
		return false;

	descriptor = descriptor.lowercaseString;
	return [descriptor containsString:@"joy-con"] ||
	       [descriptor containsString:@"joycon"] ||
	       [descriptor containsString:@"joy con"];
}

static bool ARMSX2SDLGamepadLooksLikeJoyCon(SDL_Gamepad* gamepad)
{
	return gamepad && ARMSX2CStringLooksLikeJoyCon(SDL_GetGamepadName(gamepad));
}

static bool ARMSX2NativeControllerSlotLooksLikeJoyCon(u32 slot)
{
	return ARMSX2NativeControllerLooksLikeJoyCon(ARMSX2FindNativeControllerForSlot(slot));
}

static bool ARMSX2GamepadSlotLooksLikeJoyCon(u32 slot)
{
	if (ARMSX2NativeControllerSlotLooksLikeJoyCon(slot))
		return true;

	return slot < ARMSX2_MAX_IOS_GAMEPADS && ARMSX2SDLGamepadLooksLikeJoyCon(s_gamepads[slot]);
}

static bool ARMSX2ApplyNativeGamepadRumblePulseOnMain(u32 slot, u32 packed, const char* reason)
{
	if (slot >= ARMSX2_MAX_IOS_GAMEPADS)
		return false;

	if (@available(iOS 14.0, *)) {
	} else {
		return false;
	}

	if (ARMSX2GamepadSlotLooksLikeJoyCon(slot)) {
		const u32 log_index = s_loggedJoyConRumbleSkipped.fetch_add(1, std::memory_order_relaxed);
		if (log_index < 16)
			Console.WriteLn("[ARMSX2 iOS Gamepad] Joy-Con native rumble hard-disabled slot=%u reason=%s",
				slot + 1, reason ? reason : "unknown");
		return false;
	}

	const float large = ARMSX2RumbleLargeIntensity(packed);
	const float small = ARMSX2RumbleSmallIntensity(packed);
	const float raw_intensity = std::max(large, small);
	if (raw_intensity <= 0.01f) {
		ARMSX2StopNativeGamepadRumblePulseOnMain(slot);
		return true;
	}

	GCController* controller = ARMSX2FindNativeHapticControllerForSlot(slot);
	if (ARMSX2NativeControllerLooksLikeJoyCon(controller)) {
		const u32 log_index = s_loggedJoyConRumbleSkipped.fetch_add(1, std::memory_order_relaxed);
		if (log_index < 16) {
			NSString* vendor = controller.vendorName ?: @"unknown";
			NSString* product = @"";
			if ([controller respondsToSelector:@selector(productCategory)])
				product = controller.productCategory ?: @"";
			Console.WriteLn("[ARMSX2 iOS Gamepad] Joy-Con native rumble skipped slot=%u controller=%s category=%s reason=%s",
				slot + 1, vendor.UTF8String, product.UTF8String, reason ? reason : "unknown");
		}
		return false;
	}

	if (!controller || !controller.haptics) {
		const u32 log_index = s_loggedNativePulseHapticEvents.fetch_add(1, std::memory_order_relaxed);
		if (log_index < 16) {
			Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic pulse unavailable slot=%u reason=%s controllers=%u",
				slot + 1, reason ? reason : "unknown", static_cast<unsigned>([GCController controllers].count));
		}
		return false;
	}

	ARMSX2StopNativeGamepadRumblePulseOnMain(slot);

	GCHapticsLocality locality = GCHapticsLocalityDefault;
	NSSet<GCHapticsLocality>* localities = controller.haptics.supportedLocalities;
	if ([localities containsObject:GCHapticsLocalityAll])
		locality = GCHapticsLocalityAll;

	CHHapticEngine* engine = [controller.haptics createEngineWithLocality:locality];
	if (!engine) {
		const u32 log_index = s_loggedNativePulseHapticEvents.fetch_add(1, std::memory_order_relaxed);
		if (log_index < 16) {
			NSString* vendor = controller.vendorName ?: @"unknown";
			Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic pulse engine failed slot=%u controller=%s locality=%s reason=%s",
				slot + 1, vendor.UTF8String, locality.UTF8String, reason ? reason : "unknown");
		}
		return false;
	}

	engine.playsHapticsOnly = YES;
	engine.autoShutdownEnabled = YES;

	NSError* error = nil;
	if (![engine startAndReturnError:&error]) {
		NSString* vendor = controller.vendorName ?: @"unknown";
		Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic pulse start failed slot=%u controller=%s: %s",
			slot + 1, vendor.UTF8String, error.localizedDescription.UTF8String ?: "unknown");
		return false;
	}

	const float intensity = std::clamp(raw_intensity, 0.10f, 0.55f);
	const float sharpness = std::clamp(0.25f + (small * 0.45f), 0.20f, 0.65f);
	NSArray<CHHapticEventParameter*>* params = @[
		[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:intensity],
		[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:sharpness]
	];
	CHHapticEvent* event = [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
	                                                     parameters:params
	                                                   relativeTime:0.0
	                                                        duration:0.18];
	CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[event] parameters:@[] error:&error];
	if (!pattern) {
		Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic pulse pattern failed slot=%u: %s",
			slot + 1, error.localizedDescription.UTF8String ?: "unknown");
		[engine stopWithCompletionHandler:nil];
		return false;
	}

	id<CHHapticPatternPlayer> player = [engine createPlayerWithPattern:pattern error:&error];
	if (!player || ![player startAtTime:CHHapticTimeImmediate error:&error]) {
		Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic pulse player failed slot=%u: %s",
			slot + 1, error.localizedDescription.UTF8String ?: "unknown");
		[engine stopWithCompletionHandler:nil];
		return false;
	}

	s_nativePulseHapticEngine[slot] = engine;
	const u32 stop_generation = s_nativePulseHapticStopGeneration[slot].fetch_add(1, std::memory_order_relaxed) + 1;
	const u32 log_index = s_loggedNativePulseHapticEvents.fetch_add(1, std::memory_order_relaxed);
	if (log_index < 16) {
		NSString* vendor = controller.vendorName ?: @"unknown";
		NSString* product = @"";
		if ([controller respondsToSelector:@selector(productCategory)])
			product = controller.productCategory ?: @"";
		Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic pulse accepted slot=%u controller=%s category=%s locality=%s reason=%s intensity=%.2f",
			slot + 1, vendor.UTF8String, product.UTF8String, locality.UTF8String, reason ? reason : "unknown", intensity);
	}

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(ARMSX2_GAMEPAD_RUMBLE_FORCE_STOP_SECONDS * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
		if (slot >= ARMSX2_MAX_IOS_GAMEPADS ||
			s_nativePulseHapticStopGeneration[slot].load(std::memory_order_relaxed) != stop_generation)
			return;

		if (s_nativePulseHapticEngine[slot]) {
			@try {
				[s_nativePulseHapticEngine[slot] stopWithCompletionHandler:nil];
			} @catch (NSException* exception) {
				Console.WriteLn("[ARMSX2 iOS Gamepad] Native haptic delayed stop exception slot=%u name=%s reason=%s",
					slot + 1,
					exception.name.UTF8String ?: "unknown",
					exception.reason.UTF8String ?: "unknown");
			}
			s_nativePulseHapticEngine[slot] = nil;
		}
	});

	return true;
}

static bool ARMSX2ApplyNativeGamepadRumblePulseForJoyConOnMain(u32 slot, u32 packed, const char* reason)
{
	GCController* controller = ARMSX2FindNativeControllerForSlot(slot);
	if (!ARMSX2NativeControllerLooksLikeJoyCon(controller))
		return false;

	const u32 log_index = s_loggedJoyConRumbleSkipped.fetch_add(1, std::memory_order_relaxed);
	if (log_index < 16) {
		NSString* vendor = controller.vendorName ?: @"unknown";
		NSString* product = @"";
		if ([controller respondsToSelector:@selector(productCategory)])
			product = controller.productCategory ?: @"";
		Console.WriteLn("[ARMSX2 iOS Gamepad] Joy-Con rumble skipped slot=%u controller=%s category=%s reason=%s",
			slot + 1, vendor.UTF8String, product.UTF8String, reason ? reason : "unknown");
	}
	return false;
}

void ARMSX2ApplyPendingGamepadRumble(unsigned int gamepad_index)
{
	if (gamepad_index >= ARMSX2_MAX_IOS_GAMEPADS)
		return;

    const u32 packed = s_pendingGamepadRumble[gamepad_index].load(std::memory_order_relaxed);
    if (s_appliedGamepadRumbleValid[gamepad_index] && packed == s_appliedGamepadRumble[gamepad_index])
        return;

    const u16 large = std::min<u16>(static_cast<u16>((packed >> 16) & 0xffffu), ARMSX2_GAMEPAD_RUMBLE_MAX_INTENSITY);
    const u16 small = std::min<u16>(static_cast<u16>(packed & 0xffffu), ARMSX2_GAMEPAD_RUMBLE_MAX_INTENSITY);
    const bool wants_rumble = (large != 0 || small != 0);
    const u32 stop_generation = s_gamepadRumbleStopGeneration[gamepad_index].fetch_add(1, std::memory_order_relaxed) + 1;

    if (!wants_rumble) {
        const u32 slot = gamepad_index;
        dispatch_async(dispatch_get_main_queue(), ^{
            ARMSX2StopNativeGamepadRumblePulseOnMain(slot);
        });
    }

	if (wants_rumble && ARMSX2NativeControllerSlotLooksLikeJoyCon(gamepad_index)) {
		const u32 log_index = s_loggedJoyConRumbleSkipped.fetch_add(1, std::memory_order_relaxed);
		if (log_index < 16)
			Console.WriteLn("[ARMSX2 iOS Gamepad] Joy-Con rumble request ignored safely slot=%u", gamepad_index + 1);
		s_appliedGamepadRumble[gamepad_index] = packed;
		s_appliedGamepadRumbleValid[gamepad_index] = true;
		return;
	}

	if (wants_rumble && ARMSX2GamepadSlotLooksLikeJoyCon(gamepad_index)) {
		const u32 log_index = s_loggedJoyConRumbleSkipped.fetch_add(1, std::memory_order_relaxed);
		if (log_index < 16)
			Console.WriteLn("[ARMSX2 iOS Gamepad] Joy-Con SDL rumble request ignored safely slot=%u name=%s",
				gamepad_index + 1,
				s_gamepads[gamepad_index] ? (SDL_GetGamepadName(s_gamepads[gamepad_index]) ?: "unknown") : "unknown");
		s_appliedGamepadRumble[gamepad_index] = packed;
		s_appliedGamepadRumbleValid[gamepad_index] = true;
		return;
	}

    if (s_gamepads[gamepad_index]) {
        if (SDL_RumbleGamepad(s_gamepads[gamepad_index], large, small, ARMSX2_GAMEPAD_RUMBLE_DURATION_MS)) {
            if (!s_loggedSDLGamepadRumble) {
                Console.WriteLn("[ARMSX2 iOS Gamepad] SDL controller rumble accepted");
                s_loggedSDLGamepadRumble = true;
            }
            if (wants_rumble) {
                const u32 slot = gamepad_index;
                const u32 native_packed = packed;
                dispatch_async(dispatch_get_main_queue(), ^{
                    ARMSX2ApplyNativeGamepadRumblePulseForJoyConOnMain(slot, native_packed, "joycon-sdl-mirror");
                });
            }
            if (wants_rumble) {
                const u32 slot = gamepad_index;
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(ARMSX2_GAMEPAD_RUMBLE_FORCE_STOP_SECONDS * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                    if (slot >= ARMSX2_MAX_IOS_GAMEPADS ||
                        s_gamepadRumbleStopGeneration[slot].load(std::memory_order_relaxed) != stop_generation)
                        return;

                    if (s_gamepads[slot]) {
                        SDL_RumbleGamepad(s_gamepads[slot], 0, 0, 0);
                        SDL_RumbleGamepadTriggers(s_gamepads[slot], 0, 0, 0);
                    }
                    ARMSX2StopNativeGamepadRumblePulseOnMain(slot);
                    if (!s_loggedSDLGamepadRumbleForceStop) {
                        Console.WriteLn("[ARMSX2 iOS Gamepad] SDL controller rumble force-stopped");
                        s_loggedSDLGamepadRumbleForceStop = true;
                    }
                });
            }
        } else {
            if (!s_loggedGamepadRumbleFailure) {
                Console.WriteLn("[ARMSX2 iOS Gamepad] SDL controller %u rumble unavailable: %s", gamepad_index + 1, SDL_GetError());
                s_loggedGamepadRumbleFailure = true;
            }
            if (wants_rumble) {
                const u32 slot = gamepad_index;
                const u32 native_packed = packed;
                dispatch_async(dispatch_get_main_queue(), ^{
                    ARMSX2ApplyNativeGamepadRumblePulseOnMain(slot, native_packed, "sdl-fallback");
                });
            }
        }
    } else if (wants_rumble) {
        const u32 slot = gamepad_index;
        const u32 native_packed = packed;
        dispatch_async(dispatch_get_main_queue(), ^{
            ARMSX2ApplyNativeGamepadRumblePulseOnMain(slot, native_packed, "no-sdl-gamepad");
        });
        // No SDL gamepad and no native haptic controller: fall back to the
        // device taptic engine so rumble is felt on phones (e.g. Kishi 3).
        if (!ARMSX2FindNativeHapticController()) {
            [ARMSX2Bridge triggerDeviceHapticLarge:large small:small];
        }
    }

    s_appliedGamepadRumble[gamepad_index] = packed;
    s_appliedGamepadRumbleValid[gamepad_index] = true;
}

extern "C" void ARMSX2_iOSTestGamepadRumble(void)
{
    Console.WriteLn("[ARMSX2 iOS Gamepad] Test controller rumble requested");

    SDL_PumpEvents();
    SDL_UpdateGamepads();
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    Console.WriteLn("[ARMSX2 iOS Gamepad] Test SDL detected=%d", count);
    if (ids) {
        for (int id_index = 0; id_index < count; id_index++) {
            bool already_open = false;
            for (SDL_Gamepad* gamepad : s_gamepads) {
                if (gamepad && SDL_GetGamepadID(gamepad) == ids[id_index]) {
                    already_open = true;
                    break;
                }
            }
            if (already_open)
                continue;

            for (u32 slot = 0; slot < ARMSX2_MAX_IOS_GAMEPADS; slot++) {
                if (!s_gamepads[slot]) {
                    s_gamepads[slot] = SDL_OpenGamepad(ids[id_index]);
                    Console.WriteLn("[ARMSX2 iOS Gamepad] Test SDL open slot=%u id=%d result=%s",
                        slot + 1, ids[id_index],
                        s_gamepads[slot] ? (SDL_GetGamepadName(s_gamepads[slot]) ?: "unknown") : SDL_GetError());
                    break;
                }
            }
        }
        SDL_free(ids);
    }

    bool anySDLGamepad = false;
    bool anyNativeFallback = false;
    const u32 test_packed = ARMSX2PackGamepadRumble(0.55f, 0.55f);
    for (u32 slot = 0; slot < ARMSX2_MAX_IOS_GAMEPADS; slot++) {
        if (!s_gamepads[slot])
            continue;

		if (ARMSX2GamepadSlotLooksLikeJoyCon(slot)) {
			const u32 log_index = s_loggedJoyConRumbleSkipped.fetch_add(1, std::memory_order_relaxed);
			if (log_index < 16)
				Console.WriteLn("[ARMSX2 iOS Gamepad] Test Joy-Con rumble hard-disabled slot=%u name=%s",
					slot + 1, SDL_GetGamepadName(s_gamepads[slot]) ?: "unknown");
			continue;
		}

        anySDLGamepad = true;
        const bool ok = SDL_RumbleGamepad(s_gamepads[slot], ARMSX2_GAMEPAD_RUMBLE_MAX_INTENSITY, ARMSX2_GAMEPAD_RUMBLE_MAX_INTENSITY, 250);
        Console.WriteLn("[ARMSX2 iOS Gamepad] Test SDL controller %u rumble %s%s%s",
            slot + 1, ok ? "accepted" : "failed", ok ? "" : ": ", ok ? "" : SDL_GetError());
        if (!ok) {
            const u32 native_slot = slot;
            dispatch_async(dispatch_get_main_queue(), ^{
                ARMSX2ApplyNativeGamepadRumblePulseOnMain(native_slot, test_packed, "test-sdl-fallback");
            });
            anyNativeFallback = true;
        }
    }
    if (!anySDLGamepad) {
        Console.WriteLn("[ARMSX2 iOS Gamepad] Test SDL rumble skipped: no SDL gamepad open");
        for (u32 slot = 0; slot < ARMSX2_MAX_IOS_GAMEPADS; slot++) {
            if (ARMSX2NativeControllerSlotLooksLikeJoyCon(slot)) {
                const u32 log_index = s_loggedJoyConRumbleSkipped.fetch_add(1, std::memory_order_relaxed);
                if (log_index < 16)
                    Console.WriteLn("[ARMSX2 iOS Gamepad] Test native Joy-Con rumble skipped slot=%u", slot + 1);
                continue;
            }
            const u32 native_slot = slot;
            dispatch_async(dispatch_get_main_queue(), ^{
                ARMSX2ApplyNativeGamepadRumblePulseOnMain(native_slot, test_packed, "test-no-sdl-gamepad");
            });
        }
        anyNativeFallback = true;
    }

    for (u32 slot = 0; slot < ARMSX2_MAX_IOS_GAMEPADS; slot++) {
        s_pendingGamepadRumble[slot].store(0, std::memory_order_relaxed);
        s_appliedGamepadRumble[slot] = 0;
        s_appliedGamepadRumbleValid[slot] = false;
    }

    Console.WriteLn("[ARMSX2 iOS Gamepad] Native CoreHaptics pulse fallback %s", anyNativeFallback ? "queued when needed" : "not queued");

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(0.30 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        for (u32 slot = 0; slot < ARMSX2_MAX_IOS_GAMEPADS; slot++) {
            if (s_gamepads[slot]) {
                SDL_RumbleGamepad(s_gamepads[slot], 0, 0, 0);
                SDL_RumbleGamepadTriggers(s_gamepads[slot], 0, 0, 0);
            }
            ARMSX2StopNativeGamepadRumblePulseOnMain(slot);
            s_pendingGamepadRumble[slot].store(0, std::memory_order_relaxed);
            s_appliedGamepadRumble[slot] = 0;
            s_appliedGamepadRumbleValid[slot] = false;
        }
        Console.WriteLn("[ARMSX2 iOS Gamepad] Test controller rumble stopped");
    });
}

#pragma mark - SDL gamepad refresh
void ARMSX2RefreshIOSGamepads()
{
    SDL_PumpEvents();
    SDL_UpdateGamepads();
    ARMSX2PollNativeGamepadDpadMasks("sdl-refresh");

    for (u32 slot = 0; slot < ARMSX2_MAX_IOS_GAMEPADS; slot++) {
        SDL_Gamepad* gamepad = s_gamepads[slot];
        if (!gamepad)
            continue;

        if (!SDL_GamepadConnected(gamepad)) {
            Console.WriteLn("[Files] MFi gamepad %u disconnected", slot + 1);
            s_pendingGamepadRumble[slot].store(0, std::memory_order_relaxed);
            s_gamepadRumbleStopGeneration[slot].fetch_add(1, std::memory_order_relaxed);
            s_appliedGamepadRumble[slot] = 0;
            s_appliedGamepadRumbleValid[slot] = false;
            s_nativeGamepadDpadMask[slot].store(0, std::memory_order_relaxed);
            s_nativeGamepadDpadLatchedMask[slot].store(0, std::memory_order_relaxed);
            ARMSX2RecomputeNativeGamepadAnyDpadMask();
            ARMSX2RecomputeNativeGamepadAnyDpadLatchedMask();
            const u32 disconnected_slot = slot;
            dispatch_async(dispatch_get_main_queue(), ^{
                ARMSX2StopNativeGamepadRumblePulseOnMain(disconnected_slot);
            });
            if (slot == 0) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    ARMSX2ResetNativeGamepadRumbleOnMain();
                });
            }
            SDL_CloseGamepad(gamepad);
            s_gamepads[slot] = nullptr;
        }
    }

    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids)
        return;

    for (int id_index = 0; id_index < count; id_index++) {
        bool already_open = false;
        for (SDL_Gamepad* gamepad : s_gamepads) {
            if (gamepad && SDL_GetGamepadID(gamepad) == ids[id_index]) {
                already_open = true;
                break;
            }
        }
        if (already_open)
            continue;

        for (u32 slot = 0; slot < ARMSX2_MAX_IOS_GAMEPADS; slot++) {
            if (s_gamepads[slot])
                continue;

            s_gamepads[slot] = SDL_OpenGamepad(ids[id_index]);
            if (s_gamepads[slot]) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    ARMSX2RefreshNativeGamepadDpadHandlersOnMain("sdl-open");
                });
                const u32 pad_slot = ARMSX2PadSlotForGamepadIndex(slot);
                if (pad_slot == 0xffffffffu || pad_slot >= Pad::NUM_CONTROLLER_PORTS) {
                    Console.WriteLn("[Files] MFi gamepad %u connected but ignored by current multitap mode: %s",
                        slot + 1, SDL_GetGamepadName(s_gamepads[slot]));
                } else {
                    Console.WriteLn("[Files] MFi gamepad %u connected to PS2 pad slot %u: %s",
                        slot + 1, pad_slot + 1, SDL_GetGamepadName(s_gamepads[slot]));
                }
                if (!s_loggedMultitapRestartNeeded && s_vmThreadActive.load() &&
                    ARMSX2GetIOSMultitapMode() == ARMSX2IOSMultitapMode::Auto &&
                    ARMSX2ConnectedGamepadCount() > 2 && !EmuConfig.Pad.MultitapPort0_Enabled) {
                    Console.Warning("[ARMSX2 iOS Gamepad] 3+ controllers connected after boot; restart/reset with controllers connected to enable multitap.");
                    s_loggedMultitapRestartNeeded = true;
                }
            }
            break;
        }
    }

    SDL_free(ids);
}

static bool ARMSX2ShouldPreserveTouchState(u32 ps2_button, bool preserve_touch)
{
    return preserve_touch && ps2_button < (sizeof(g_touchPadState) / sizeof(g_touchPadState[0])) && g_touchPadState[ps2_button];
}

void ARMSX2ApplyIOSGamepadInput(unsigned int gamepad_index, SDL_Gamepad* gamepad, PadBase* pad, bool preserve_touch)
{
    if (!gamepad || !pad)
        return;

    if (s_captureMode.load()) {
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
            if (SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(b))) {
                s_capturedButton.store(b);
                break;
            }
        }
    }

    static const u32 ps2Buttons[] = {
        PadDualshock2::Inputs::PAD_UP, PadDualshock2::Inputs::PAD_DOWN,
        PadDualshock2::Inputs::PAD_LEFT, PadDualshock2::Inputs::PAD_RIGHT,
        PadDualshock2::Inputs::PAD_CROSS, PadDualshock2::Inputs::PAD_CIRCLE,
        PadDualshock2::Inputs::PAD_SQUARE, PadDualshock2::Inputs::PAD_TRIANGLE,
        PadDualshock2::Inputs::PAD_L1, PadDualshock2::Inputs::PAD_R1,
        0, 0, // L2/R2 handled as analog
        PadDualshock2::Inputs::PAD_START, PadDualshock2::Inputs::PAD_SELECT,
        PadDualshock2::Inputs::PAD_L3, PadDualshock2::Inputs::PAD_R3,
    };

    for (int i = 0; i < 16; i++) {
        const int sdlBtn = s_buttonMap[i];
        if (sdlBtn < 0)
            continue;

        const u32 ps2Button = ps2Buttons[i];
        if (ps2Button == 0)
            continue;

        if (ARMSX2NativeDpadBitForPS2Button(ps2Button) != 0)
            continue;

        bool pressed = SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(sdlBtn));

        if (pressed)
            pad->Set(ps2Button, 1.0f);
        else if (!ARMSX2ShouldPreserveTouchState(ps2Button, preserve_touch))
            pad->Set(ps2Button, 0.0f);
    }

    struct DpadBinding
    {
        int map_index;
        u32 ps2_button;
        u8 native_bit;
    };
    static constexpr DpadBinding dpad_bindings[] = {
        {0, PadDualshock2::Inputs::PAD_UP, ARMSX2_NATIVE_DPAD_UP},
        {1, PadDualshock2::Inputs::PAD_DOWN, ARMSX2_NATIVE_DPAD_DOWN},
        {2, PadDualshock2::Inputs::PAD_LEFT, ARMSX2_NATIVE_DPAD_LEFT},
        {3, PadDualshock2::Inputs::PAD_RIGHT, ARMSX2_NATIVE_DPAD_RIGHT},
    };

    u8 native_dpad_mask = 0;
    u8 slot_latched_mask = 0;
    u8 any_latched_mask = 0;
    if (gamepad_index < ARMSX2_MAX_IOS_GAMEPADS) {
        slot_latched_mask = s_nativeGamepadDpadLatchedMask[gamepad_index].exchange(0, std::memory_order_relaxed);
        native_dpad_mask = s_nativeGamepadDpadMask[gamepad_index].load(std::memory_order_relaxed) | slot_latched_mask;
        if (gamepad_index == 0 && ARMSX2ConnectedGamepadCount() <= 1) {
            any_latched_mask = s_nativeGamepadAnyDpadLatchedMask.exchange(0, std::memory_order_relaxed);
            native_dpad_mask |= s_nativeGamepadAnyDpadMask.load(std::memory_order_relaxed) | any_latched_mask;
        }
        ARMSX2RecomputeNativeGamepadAnyDpadLatchedMask();
    }

    for (const DpadBinding& binding : dpad_bindings) {
        bool pressed = false;
        const int sdlBtn = s_buttonMap[binding.map_index];
        if (sdlBtn >= 0)
            pressed = SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(sdlBtn));

        const bool native_pressed = ((native_dpad_mask & binding.native_bit) != 0);
        pressed = pressed || native_pressed;

        if (native_pressed) {
            const u32 log_index = s_loggedNativeGamepadDpadApplyEvents.fetch_add(1, std::memory_order_relaxed);
            if (log_index < 48) {
                Console.WriteLn("[ARMSX2 iOS Gamepad] Native dpad applied gamepad=%u ps2=0x%08x slot_mask=0x%02x slot_latched=0x%02x any_mask=0x%02x any_latched=0x%02x",
                    gamepad_index + 1, binding.ps2_button,
                    s_nativeGamepadDpadMask[gamepad_index].load(std::memory_order_relaxed),
                    slot_latched_mask,
                    s_nativeGamepadAnyDpadMask.load(std::memory_order_relaxed),
                    any_latched_mask);
            }
        }

        if (pressed)
            pad->Set(binding.ps2_button, 1.0f);
        else if (!ARMSX2ShouldPreserveTouchState(binding.ps2_button, preserve_touch))
            pad->Set(binding.ps2_button, 0.0f);
    }

    const float l2 = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
    const float r2 = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    if (l2 > 0.1f || !ARMSX2ShouldPreserveTouchState(PadDualshock2::Inputs::PAD_L2, preserve_touch))
        pad->Set(PadDualshock2::Inputs::PAD_L2, l2 > 0.1f ? l2 : 0.0f);
    if (r2 > 0.1f || !ARMSX2ShouldPreserveTouchState(PadDualshock2::Inputs::PAD_R2, preserve_touch))
        pad->Set(PadDualshock2::Inputs::PAD_R2, r2 > 0.1f ? r2 : 0.0f);

    auto axis = [&](SDL_GamepadAxis a) -> float {
        const float v = SDL_GetGamepadAxis(gamepad, a) / 32767.0f;
        return (v > 0.15f || v < -0.15f) ? v : 0.0f;
    };
    const float lx = axis(SDL_GAMEPAD_AXIS_LEFTX);
    const float ly = axis(SDL_GAMEPAD_AXIS_LEFTY);
    const float rx = axis(SDL_GAMEPAD_AXIS_RIGHTX);
    const float ry = axis(SDL_GAMEPAD_AXIS_RIGHTY);
    auto set_axis = [&](u32 input, float value) {
        if (value > 0.0f || !ARMSX2ShouldPreserveTouchState(input, preserve_touch))
            pad->Set(input, value);
    };
    set_axis(PadDualshock2::Inputs::PAD_L_RIGHT, lx > 0 ? lx : 0.0f);
    set_axis(PadDualshock2::Inputs::PAD_L_LEFT,  lx < 0 ? -lx : 0.0f);
    set_axis(PadDualshock2::Inputs::PAD_L_DOWN,  ly > 0 ? ly : 0.0f);
    set_axis(PadDualshock2::Inputs::PAD_L_UP,    ly < 0 ? -ly : 0.0f);
    set_axis(PadDualshock2::Inputs::PAD_R_RIGHT, rx > 0 ? rx : 0.0f);
    set_axis(PadDualshock2::Inputs::PAD_R_LEFT,  rx < 0 ? -rx : 0.0f);
    set_axis(PadDualshock2::Inputs::PAD_R_DOWN,  ry > 0 ? ry : 0.0f);
    set_axis(PadDualshock2::Inputs::PAD_R_UP,    ry < 0 ? -ry : 0.0f);
}
