// [P63] Stubs for macOS native build — symbols not available without full UI
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE

#include "Host.h"
#include "VMManager.h"
#include "Input/InputManager.h"
#include "DEV9/pcap_io.h"
#include "Achievements.h"
#include "common/HTTPDownloader.h"
#include "common/ProgressCallback.h"
#include "common/FileSystem.h"
#include "Host/AudioStream.h"
#include "INISettingsInterface.h"

// --- PNG (already in common/PNGStub.cpp) ---

// --- FileSystem ---
int FileSystem::OpenFDFileContent(const char* path) { return -1; }

// --- x86 emitter symbols (never called on ARM64, but referenced by JIT stubs) ---
// Non-const → external linkage; Itanium ABI mangles by name only, type doesn't matter
namespace x86Emitter {
    char xmm0[16]={}, xmm1[16]={}, xmm2[16]={}, xmm3[16]={}, xmm4[16]={}, xmm5[16]={};
    char xmm6[16]={}, xmm7[16]={}, xmm8[16]={}, xmm9[16]={}, xmm10[16]={};
    char xmm11[16]={}, xmm12[16]={}, xmm13[16]={}, xmm14[16]={}, xmm15[16]={};
}

// --- g_xmmtypes (thread-local, referenced by iCore.o) ---
enum { _XMMT_INT = 0 };
thread_local int g_xmmtypes[16] = {_XMMT_INT};

// --- Audio: Oboe is Android-only ---
std::unique_ptr<AudioStream> AudioStream::CreateOboeAudioStream(u32 sr, const AudioStreamParameters& p, bool s, Error* e) { return nullptr; }

// --- Network: PCAP not available ---
PCAPAdapter::PCAPAdapter() : NetAdapter() {}

// --- HTTP: curl not linked ---
std::unique_ptr<HTTPDownloader> HTTPDownloader::Create(std::string ua) { return nullptr; }

// --- Input: keyboard mapping ---
std::optional<std::string> InputManager::ConvertHostKeyboardCodeToString(u32 c) { return std::nullopt; }
const char* InputManager::ConvertHostKeyboardCodeToIcon(u32 c) { return nullptr; }
std::optional<u32> InputManager::ConvertHostKeyboardStringToCode(std::string_view s) { return std::nullopt; }

// --- Settings ---
INISettingsInterface* g_p44_settings_interface = nullptr;

// --- Host functions (declared in Host.h) ---
bool Host::InNoGUIMode() { return true; }
void Host::RunOnCPUThread(std::function<void()> f, bool b) { if (f) f(); }
void Host::RequestVMShutdown(bool a, bool b, bool c) {}
bool Host::RequestResetSettings(bool a, bool b, bool c, bool d, bool e) { return false; }
void Host::CancelGameListRefresh() {}
void Host::RefreshGameListAsync(bool i) {}
void Host::CommitBaseSettingChanges() {}
bool Host::ConfirmMessage(std::string_view t, std::string_view m) { return true; }
void Host::ReportErrorAsync(std::string_view t, std::string_view m) { fprintf(stderr, "[Error] %.*s: %.*s\n", (int)t.size(), t.data(), (int)m.size(), m.data()); }
void Host::ReportInfoAsync(std::string_view t, std::string_view m) {}
void Host::OpenURL(std::string_view u) {}
bool Host::CopyTextToClipboard(std::string_view t) { return false; }
std::unique_ptr<ProgressCallback> Host::CreateHostProgressCallback() { return nullptr; }
std::string Host::TranslatePluralToString(const char* ctx, const char* msg, const char* mpl, int n) { return msg; }
s32 Host::Internal::GetTranslatedStringImpl(std::string_view ctx, std::string_view msg, char* buf, size_t sz) {
    s32 len = std::min((s32)msg.size(), (s32)(sz - 1));
    std::memcpy(buf, msg.data(), len);
    buf[len] = 0;
    return len;
}

// --- Host functions (declared in VMManager.h / other headers) ---
namespace Host {
    void BeginPresentFrame() {}
    // AcquireRenderWindow is in ios_main.mm (macOS section) with correct return type
    void ReleaseRenderWindow() {}
    void BeginTextInput() {}
    void EndTextInput() {}
    bool IsFullscreen() { return false; }
    void SetFullscreen(bool f) {}
    void SetMouseMode(bool r, bool h) {}
    void OnVMStarting() {}
    void OnVMStarted() {}
    void OnVMDestroyed() {}
    void OnVMPaused() {}
    void OnVMResumed() {}
    void OnGameChanged(const std::string& d, const std::string& e, const std::string& t, const std::string& s, u32 c, u32 r) {}
    void OnPerformanceMetricsUpdated() {}
    void OnSaveStateLoading(std::string_view f) {}
    void OnSaveStateLoaded(std::string_view f, bool w) {}
    void OnSaveStateSaved(std::string_view f) {}
    void OnAchievementsHardcoreModeChanged(bool e) {}
    void OnAchievementsLoginRequested(Achievements::LoginRequestReason r) {}
    void OnAchievementsLoginSuccess(const char* u, u32 p, u32 sc, u32 us) {}
    void OnAchievementsRefreshed() {}
    bool HasNativeAchievementNotifications() { return false; }
    void OnAchievementNotification(const char*, float, const char*, const char*, const char*) {}
    void OnCoverDownloaderOpenRequested() {}
    void OnCreateMemoryCardOpenRequested() {}
    void OnInputDeviceConnected(std::string_view i, std::string_view d) {}
    void OnInputDeviceDisconnected(InputBindingKey k, std::string_view i) {}
    void PumpMessagesOnCPUThread() {}
    void RequestExitApplication(bool a) {}
    void RequestExitBigPicture() {}
    void CheckForSettingsChanges(const Pcsx2Config& c) {}
    void LoadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock) {}
    bool ShouldPreferHostFileSelector() { return false; }
    void OpenHostFileSelectorAsync(std::string_view t, bool d, std::function<void(const std::string&)> cb, std::vector<std::string> f, std::string_view i) {}
    bool LocaleCircleConfirm() { return false; }
}

// iOS-specific globals (referenced by iPSX2Bridge.mm)
#include <map>
#include <string>
int g_touchPadState = 0;
bool s_captureMode = false;
int s_capturedButton = -1;
bool s_requestVMStop = false;
extern "C" void iPSX2_SetSDLFullscreen(bool) {}
std::map<std::string, int> s_buttonMap;

// PCAPAdapter vtable
PCAPAdapter::~PCAPAdapter() {}
bool PCAPAdapter::blocks() { return false; }
bool PCAPAdapter::isInitialised() { return false; }
bool PCAPAdapter::recv(NetPacket* p) { return false; }
bool PCAPAdapter::send(NetPacket* p) { return false; }
void PCAPAdapter::reloadSettings() {}

#else // TARGET_OS_IPHONE — iOS stubs for CocoaTools (CocoaTools.mm is macOS-only)

// On iOS, CocoaTools.mm is excluded from the build. Provide stubs for the
// functions referenced by iOS-compiled core code (DynamicLibrary, WindowInfo,
// Pcsx2Config, etc.). iOS uses UIKit/Foundation, not Cocoa/AppKit.
#include "common/CocoaTools.h"
#include "common/WindowInfo.h"
#include <optional>
#include <string>

namespace CocoaTools
{
	bool CreateMetalLayer(WindowInfo* wi) { return false; }
	void DestroyMetalLayer(WindowInfo* wi) {}
	std::optional<float> GetViewRefreshRate(const WindowInfo& wi) { return std::nullopt; }
	void MarkHelpMenu(void* menu) {}
	std::optional<std::string> GetBundlePath() { return std::nullopt; }
	std::optional<std::string> GetNonTranslocatedBundlePath() { return std::nullopt; }
	std::optional<std::string> MoveToTrash(std::string_view file) { return std::nullopt; }
	bool DelayedLaunch(std::string_view file) { return false; }
	bool ShowInFinder(std::string_view file) { return false; }
	std::optional<std::string> GetResourcePath() { return std::nullopt; }
	void* CreateWindow(std::string_view title, uint32_t width, uint32_t height) { return nullptr; }
	void DestroyWindow(void* window) {}
	void GetWindowInfoFromWindow(WindowInfo* wi, void* window) {}
	void RunCocoaEventLoop(bool wait_forever) {}
	void StopMainThreadEventLoop() {}
}

// --- Discord Register stubs (discord_register_osx.m excluded on iOS) ---
extern "C" {
void Discord_Register(const char* applicationId, const char* command) {}
void Discord_RegisterSteamGame(const char* applicationId, const char* steamId) {}
}

// --- Host capture + hotkey stubs (frontend callbacks not yet wired) ---
#include "Host.h"
#include "GS/GS.h"
#include "Input/InputManager.h"
void Host::OnCaptureStarted(const std::string& filename) {}
void Host::OnCaptureStopped() {}

// g_host_hotkeys - normally defined in pcsx2-qt, empty on iOS
BEGIN_HOTKEY_LIST(g_host_hotkeys)
END_HOTKEY_LIST()

#endif // !TARGET_OS_IPHONE

