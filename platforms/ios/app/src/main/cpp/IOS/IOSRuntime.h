// IOSRuntime.h — shared declarations for the split iOS runtime translation units.

#pragma once

#include <UIKit/UIKit.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "pcsx2/INISettingsInterface.h"

// Forward declarations to keep this header light.
struct SDL_Window;

// CPUThreadTask is defined here so HostImpls.mm can std::make_shared it and access
// its members.
struct CPUThreadTask
{
    unsigned long long id = 0;
    std::function<void()> function;
    std::mutex mutex;
    std::condition_variable cv;
    bool complete = false;
};

@class ARMSX2GameView;

// ---------------------------------------------------------------------------
// Host::g_sdl_window
// ---------------------------------------------------------------------------
namespace Host {
    extern SDL_Window* g_sdl_window;
}

// ---------------------------------------------------------------------------
// Settings interface pointers
// ---------------------------------------------------------------------------
extern INISettingsInterface* s_settings_interface;
extern INISettingsInterface* s_secrets_settings_interface;
extern INISettingsInterface* g_p44_settings_interface;

// ---------------------------------------------------------------------------
// On-screen debug log view
// ---------------------------------------------------------------------------
extern UITextView* g_logView;

// ---------------------------------------------------------------------------
// Game render view (CAMetalLayer-backed ARMSX2GameView)
// ---------------------------------------------------------------------------
extern ARMSX2GameView* g_gameRenderView;

// ---------------------------------------------------------------------------
// VM thread / boot coordination
// ---------------------------------------------------------------------------
extern std::atomic<bool> s_vmThreadActive;          // true while VM is executing
extern std::atomic<unsigned int> s_vmHeartbeatGeneration;
extern std::atomic<bool> s_requestVMBoot;           // signal VM thread to boot
extern std::atomic<bool> s_requestVMStop;           // signal VM to stop from UI
extern std::mutex s_vmMutex;
extern std::condition_variable s_vmCV;
extern bool s_vmThreadCreated;                       // guarded by s_vmMutex

// ---------------------------------------------------------------------------
// CPU task queue
// ---------------------------------------------------------------------------
extern std::thread::id s_cpuThreadId;
extern std::atomic<unsigned long long> s_cpuTaskNextId;
extern std::mutex s_cpuTaskMutex;
extern std::deque<std::shared_ptr<CPUThreadTask>> s_cpuTasks;

// Drains the CPU task queue.
void ARMSX2DrainCPUThreadTasks();

extern "C" void ARMSX2_PostRetroAchievementsStateChanged(void);
// Posts a RetroAchievements toast to the SwiftUI layer. `duration` is the on-screen
// time in seconds (<= 0 falls back to the SwiftUI default). Safe to call from any thread.
extern "C" void ARMSX2_PostRetroAchievementsNotification(const char* title, const char* message,
	const char* badgePath, float duration);
extern "C" void ARMSX2_PostRuntimeMenuStateChanged(void);
// Runtime telemetry gate (env-gated, cached).
bool ARMSX2IOSRuntimeTelemetryEnabled();

// ---------------------------------------------------------------------------
// View-controller refs
// __unsafe_unretained (not __weak): this translation unit is built under manual
// reference counting (no ARC), where __weak is illegal. The menu/root
// controllers are owned by UIKit/SwiftUI; the unretained refs here are kept
// valid for the lifetime of those owning objects.
// ---------------------------------------------------------------------------
extern UIViewController* __unsafe_unretained s_menuVC;
extern UIViewController* __unsafe_unretained s_rootVC;

// ---------------------------------------------------------------------------
// Gamepad / haptics subsystem
// ARMSX2_MAX_IOS_GAMEPADS is a header constant so every TU sees the same array bound.
// ---------------------------------------------------------------------------
inline constexpr unsigned int ARMSX2_MAX_IOS_GAMEPADS = 4;
struct SDL_Gamepad;
extern SDL_Gamepad* s_gamepads[ARMSX2_MAX_IOS_GAMEPADS];

extern bool g_touchPadState[64];
extern std::atomic<bool> s_captureMode;
extern std::atomic<int>  s_capturedButton;
extern int s_buttonMap[16];
extern const int s_defaultMap[16];

// Gamepad helpers.
void ARMSX2RefreshIOSGamepads();
unsigned int ARMSX2PadSlotForGamepadIndex(unsigned int gamepad_index);
void ARMSX2ApplyPendingGamepadRumble(unsigned int gamepad_index);
struct PadBase;
void ARMSX2ApplyIOSGamepadInput(unsigned int gamepad_index, struct SDL_Gamepad* gamepad,
                                PadBase* pad, bool preserve_touch);
void ARMSX2InstallNativeGamepadDpadObserversOnMain();

// ---------------------------------------------------------------------------
// Launch / settings helpers
// ---------------------------------------------------------------------------
inline constexpr bool ARMSX2IOSRetroAchievementsHardcoreAvailable = true;
class SettingsInterface;
void ARMSX2EnsureIOSSpeedhackDefaults(SettingsInterface* si, const char* reason);
bool ARMSX2RepairIOSARM64JITSettings(SettingsInterface* si, const char* reason);
void ARMSX2MigrateJITScriptProtocolForIOS(SettingsInterface* si, const char* reason);
void ARMSX2IOSSanitizeFolderSettings(SettingsInterface* si, const std::string& dataRoot,
                                     const char* reason);
void ARMSX2IOSApplyRetroAchievementsOverlayDefaults(SettingsInterface* si, const char* reason);
void ARMSX2ApplyIOSOsdPresetFromConfig(const char* reason);
void ARMSX2SanitizeFrameLimiterConfig(const char* reason);
void ARMSX2IOSLogMemoryCardConfig(const char* reason);
bool ARMSX2ResolveFastBootForISO(const std::string& isoPath);
void ARMSX2ConfigureImGuiFonts(const char* reason);
void ARMSX2ApplyJITScriptProtocol(const char* reason);
void ARMSX2ApplyIOSMultitapConfig(const char* reason);
void ARMSX2DisableRetroAchievementsHardcoreForIOS(SettingsInterface* si, const char* reason);
bool ARMSX2ShouldEnableMTVUByDefault(unsigned int* physical_cores);
