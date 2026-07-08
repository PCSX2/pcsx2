// HostImpls.mm — PCSX2 Host interface implementations for iOS.

#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common/Console.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/HTTPDownloader.h"
#include "common/Path.h"
#include "common/ProgressCallback.h"
#include "common/WindowInfo.h"

#include "pcsx2/Counters.h"          // g_FrameCount
#include "pcsx2/Config.h"            // EmuConfig, GSConfig
#include "pcsx2/Host.h"
#include "pcsx2/Host/AudioStreamTypes.h"
#include "pcsx2/INISettingsInterface.h"
#include "pcsx2/PerformanceMetrics.h"
#include "pcsx2/R5900.h"
#include "pcsx2/SIO/Pad/Pad.h"
#include "pcsx2/SIO/Pad/PadDualshock2.h"
#include "pcsx2/VMManager.h"
#include "pcsx2/Achievements.h"

#include "pcsx2/CDVD/CDVDdiscReader.h"
#include "pcsx2/DEV9/pcap_io.h"
#include "pcsx2/DEV9/net.h"
#include "pcsx2/Input/InputManager.h"

#import <UIKit/UIKit.h>

#include "IOSRuntime.h"

#pragma mark - namespace Host
namespace Host
{
    SDL_Window* g_sdl_window = nullptr;

    void RequestShutdown() {
        SDL_Event event;
        event.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&event);
    }

    void RunOnMainThread(std::function<void()> func, bool wait) {
        if (wait) {
            dispatch_sync(dispatch_get_main_queue(), ^{ func(); });
        } else {
            dispatch_async(dispatch_get_main_queue(), ^{ func(); });
        }
    }

    // Only needed stubs for linking
    bool CopyTextToClipboard(const std::string_view text)
    {
        NSString* nsText = [[NSString alloc] initWithBytes:text.data()
                                                    length:text.size()
                                                  encoding:NSUTF8StringEncoding];
        if (!nsText)
            return false;

        void (^copyBlock)(void) = ^{
            UIPasteboard.generalPasteboard.string = nsText;
        };

        if ([NSThread isMainThread])
            copyBlock();
        else
            dispatch_sync(dispatch_get_main_queue(), copyBlock);

        return true;
    }

    std::string GetTextFromClipboard()
    {
        __block NSString* nsText = nil;
        void (^pasteBlock)(void) = ^{
            nsText = UIPasteboard.generalPasteboard.string;
        };

        if ([NSThread isMainThread])
            pasteBlock();
        else
            dispatch_sync(dispatch_get_main_queue(), pasteBlock);

        const char* utf8 = nsText ? [nsText UTF8String] : nullptr;
        return utf8 ? std::string(utf8) : std::string();
    }

    void OnOSDMessage(const std::string&, float, u32) {}
    void ReportError(const char*, const char*) {}
    bool ConfirmAction(const char*, const char*, const char*) { return true; }
    std::optional<std::string> OpenFileSelectionDialog(const char*, const char*, const char*, const char*) { return std::nullopt; }
    std::optional<std::string> OpenDirectorySelectionDialog(const char*, const char*) { return std::nullopt; }
    void SysLog(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
    void LoadSettings(SettingsInterface&, std::unique_lock<std::mutex>&) {} 
    void RequestResetSettings(bool) {} 
    const char* GetTranslatedStringImpl(const char* key) { return key; }
    u32 GetDisplayRefreshRate() { return 60; }
    std::optional<WindowInfo> AcquireRenderWindow(bool recreate_window) {
        Console.WriteLn("Host::AcquireRenderWindow(recreate=%d) called.", recreate_window);
        if (!g_sdl_window) {
            Console.Error("Host::AcquireRenderWindow: g_sdl_window is NULL");
            return std::nullopt;
        }
        
        __block WindowInfo wi = {};
        wi.type = WindowInfo::Type::MacOS;
        
        // SDL calls that interact with UIKit must run on the main thread
        dispatch_sync(dispatch_get_main_queue(), ^{
            // SDL3 properties for UIKit
            SDL_PropertiesID props = SDL_GetWindowProperties(g_sdl_window);
            UIWindow* window = (__bridge UIWindow*)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
            
            if (window) {
                // Use dedicated game render view if available (sized for portrait).
                if (g_gameRenderView) {
                    wi.window_handle = (__bridge void*)g_gameRenderView;
                } else {
                    wi.window_handle = (__bridge void*)[window rootViewController].view;
                }
            }

            if (!wi.window_handle) {
                 Console.Error("Host::AcquireRenderWindow: Failed to get UIKit View (UIWindow=%p)", window);
                 // Last resort: some older SDL versions might put the view in the window property or vice versa
                 if (!wi.window_handle) wi.window_handle = (__bridge void*)window;
            }

            // Get render size from the actual render view.
            UIView* renderView = (__bridge UIView*)wi.window_handle;
            CGFloat scale = renderView.contentScaleFactor > 0.0 ? renderView.contentScaleFactor : UIScreen.mainScreen.nativeScale;
            if (scale <= 0.0)
                scale = 1.0;
            wi.surface_width = static_cast<u32>(std::max<CGFloat>(1.0, renderView.bounds.size.width * scale));
            wi.surface_height = static_cast<u32>(std::max<CGFloat>(1.0, renderView.bounds.size.height * scale));
            wi.surface_scale = static_cast<float>(scale);
            
            SDL_DisplayID display = SDL_GetDisplayForWindow(g_sdl_window);
            const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
            if (mode)
                wi.surface_refresh_rate = mode->refresh_rate;
            else
                wi.surface_refresh_rate = 60.0f;
        });

        Console.WriteLn("Host::AcquireRenderWindow: Returning WindowInfo (Type=%d, View=%p, Size=%ux%u, Scale=%.2f)",
            (int)wi.type, wi.window_handle, wi.surface_width, wi.surface_height, wi.surface_scale);

        return wi;
    }
    void ReleaseRenderWindow() {}
    bool InNoGUIMode() { return false; }
    void OnVMPaused() {}
    void OnVMResumed() {}
    void OnVMStarted() {}
    void OnVMStarting() {}
    void EndTextInput() {}
    bool IsFullscreen() { return true; }
    void SetMouseMode(bool, bool) {}
    void OnGameChanged(const std::string&, const std::string&, const std::string&, const std::string&, unsigned int, unsigned int)
    {
        ARMSX2_PostRuntimeMenuStateChanged();
    }
    void OnVMDestroyed() {}
    void SetFullscreen(bool) {}
    void BeginTextInput() {}
    bool ConfirmMessage(std::string_view, std::string_view) { return true; }
    void RunOnCPUThread(std::function<void()> function, bool block)
    {
        if (!function)
            return;

        if (std::this_thread::get_id() == s_cpuThreadId) {
            function();
            return;
        }

        if (!s_vmThreadActive.load()) {
            Console.Warning("[ARMSX2 iOS] CPU-thread task requested while VM is inactive; running inline");
            function();
            return;
        }

        auto task = std::make_shared<CPUThreadTask>();
        task->id = s_cpuTaskNextId.fetch_add(1, std::memory_order_relaxed);
        task->function = std::move(function);

        {
            std::lock_guard<std::mutex> lock(s_cpuTaskMutex);
            s_cpuTasks.push_back(task);
        }

        if (!block)
            return;

        std::unique_lock<std::mutex> lock(task->mutex);
        std::fprintf(stderr, "@@CPU_TASK_WAIT@@ id=%llu state=%d\n",
            task->id, static_cast<int>(VMManager::GetState()));
        std::fflush(stderr);
        if (!task->cv.wait_for(lock, std::chrono::seconds(1), [&task] { return task->complete; })) {
            std::fprintf(stderr, "@@CPU_TASK_TIMEOUT@@ id=%llu state=%d queued=1\n",
                task->id, static_cast<int>(VMManager::GetState()));
            std::fflush(stderr);
            return;
        }
        std::fprintf(stderr, "@@CPU_TASK_WAIT_OK@@ id=%llu\n", task->id);
        std::fflush(stderr);
    }
    void ReportInfoAsync(std::string_view, std::string_view) {}
    void ReportErrorAsync(std::string_view title, std::string_view msg) {
        Console.Error("Host::ReportErrorAsync: %s - %s", std::string(title).c_str(), std::string(msg).c_str());
    }
    void OnSaveStateSaved(std::string_view) { ARMSX2_PostRuntimeMenuStateChanged(); }
    void OnSaveStateLoaded(std::string_view, bool) { ARMSX2_PostRuntimeMenuStateChanged(); }
    void BeginPresentFrame() {}
    void OnSaveStateLoading(std::string_view) {}
    bool LocaleCircleConfirm() { return false; }

    void RefreshGameListAsync(bool) {}
    bool RequestResetSettings(bool, bool, bool, bool, bool) { return true; }
    void CancelGameListRefresh() {}
    void RequestVMShutdown(bool, bool, bool) {}
    void RequestExitBigPicture() {}
    void OnInputDeviceConnected(std::string_view, std::string_view) {}
    void RequestExitApplication(bool) {}
    void CheckForSettingsChanges(const Pcsx2Config&) {}
    void OnAchievementsRefreshed()
    {
        ARMSX2_PostRetroAchievementsStateChanged();
    }
    void PumpMessagesOnCPUThread()
    {
        ARMSX2DrainCPUThreadTasks();

// Check for VM shutdown request (safe: runs on CPU thread)
        if (s_requestVMStop.load()) {
            Console.WriteLn("[UI] PumpMessages: setting VM state to Stopping");
            VMManager::SetState(VMState::Stopping);
            return;
        }

        PadBase* pad = Pad::GetPad(0, 0);
        if (!pad) return;

#if TARGET_OS_SIMULATOR
        const bool* keys = SDL_GetKeyboardState(nullptr);
        if (!keys) return;

        static const struct { SDL_Scancode sc; u32 idx; } mapping[] = {
            { SDL_SCANCODE_UP,     PadDualshock2::Inputs::PAD_UP },
            { SDL_SCANCODE_DOWN,   PadDualshock2::Inputs::PAD_DOWN },
            { SDL_SCANCODE_LEFT,   PadDualshock2::Inputs::PAD_LEFT },
            { SDL_SCANCODE_RIGHT,  PadDualshock2::Inputs::PAD_RIGHT },
            { SDL_SCANCODE_Z,      PadDualshock2::Inputs::PAD_CIRCLE },
            { SDL_SCANCODE_X,      PadDualshock2::Inputs::PAD_CROSS },
            { SDL_SCANCODE_A,      PadDualshock2::Inputs::PAD_SQUARE },
            { SDL_SCANCODE_S,      PadDualshock2::Inputs::PAD_TRIANGLE },
            { SDL_SCANCODE_Q,      PadDualshock2::Inputs::PAD_L1 },
            { SDL_SCANCODE_W,      PadDualshock2::Inputs::PAD_R1 },
            { SDL_SCANCODE_1,      PadDualshock2::Inputs::PAD_L2 },
            { SDL_SCANCODE_2,      PadDualshock2::Inputs::PAD_R2 },
            { SDL_SCANCODE_RETURN, PadDualshock2::Inputs::PAD_START },
            { SDL_SCANCODE_SPACE,  PadDualshock2::Inputs::PAD_SELECT },
        };

        // Merge keyboard + touch input: only override with keyboard if
        // touch is not currently pressing the same button.
        for (const auto& m : mapping) {
            if (keys[m.sc])
                pad->Set(m.idx, 1.0f);
            else if (!g_touchPadState[m.idx])
                pad->Set(m.idx, 0.0f);
            // If touch is holding the button (g_touchPadState), don't reset it
        }
#endif // TARGET_OS_SIMULATOR — keyboard mapping

        // MFi / External gamepad support via SDL3
        {
            ARMSX2RefreshIOSGamepads();
            for (u32 gamepad_index = 0; gamepad_index < ARMSX2_MAX_IOS_GAMEPADS; gamepad_index++) {
                SDL_Gamepad* gamepad = s_gamepads[gamepad_index];
                if (!gamepad)
                    continue;

                const u32 pad_slot = ARMSX2PadSlotForGamepadIndex(gamepad_index);
                if (pad_slot == 0xffffffffu || pad_slot >= Pad::NUM_CONTROLLER_PORTS)
                    continue;

                PadBase* gamepad_pad = Pad::GetPad(static_cast<u8>(pad_slot));
                if (!gamepad_pad)
                    continue;

                ARMSX2ApplyPendingGamepadRumble(gamepad_index);
                ARMSX2ApplyIOSGamepadInput(gamepad_index, gamepad, gamepad_pad, gamepad_index == 0);
            }
        }

        // [BIOS_NAV] Auto-navigate BIOS — debug only
#if DEBUG
        if (const char* nav = getenv("ARMSX2_BIOS_NAV"); nav && atoi(nav))
        {
            unsigned int fc = ::g_FrameCount;
            auto press = [&](u32 btn, unsigned int at) {
                if (fc >= at && fc <= at + 1) pad->Set(btn, 1.0f);
                else if (fc == at + 2) pad->Set(btn, 0.0f);
            };
            // BIOS nav: ↓ → ○ → ○ → ○ → ← → ○ → ○...
            // The exact screen order varies. Try multiple ←+○ combos.
            press(PadDualshock2::Inputs::PAD_DOWN, 600);
            press(PadDualshock2::Inputs::PAD_CIRCLE, 750);
            // After entering System Configuration, each screen needs ○ to advance.
            // The "initialization" dialog needs ← first to select "Yes".
            // Try ← before each ○ to handle wherever the dialog appears.
            unsigned int seq[] = {
                950,  0,  // ○ language
                1150, 0,  // ○ clock
                1350, 1,  // ← then ○ (init dialog attempt 1)
                1550, 1,  // ← then ○ (init dialog attempt 2)
                1750, 0,  // ○
                1950, 0,  // ○
                2150, 1,  // ← then ○ (attempt 3)
                2350, 0,  // ○
                2550, 0, 2750, 0, 2950, 0, 3150, 0, 3350, 0, 3550, 0,
            };
            for (int i = 0; i < (int)(sizeof(seq)/sizeof(seq[0])); i += 2) {
                unsigned int t = seq[i];
                if (seq[i+1]) // needs LEFT first
                    press(PadDualshock2::Inputs::PAD_LEFT, t);
                press(PadDualshock2::Inputs::PAD_CIRCLE, t + (seq[i+1] ? 100 : 0));
            }

            // Log after each step
            static const unsigned int cps[] = {650, 770, 950, 1130, 1300, 1500, 1800, 2100, 2400, 2700, 3000};
            for (auto cp : cps) {
                if (fc == cp) {
                    Console.WriteLn(Color_Yellow, "[BIOS_NAV] checkpoint f=%u", fc);
                }
            }
        }
#endif // DEBUG — BIOS_NAV
    }
    std::string TranslatePluralToString(const char*, const char* msg, const char*, int count)
    {
        std::string result = msg ? msg : "";
        const std::string count_string = std::to_string(count);
        size_t pos = 0;
        while ((pos = result.find("%n", pos)) != std::string::npos)
        {
            result.replace(pos, 2, count_string);
            pos += count_string.size();
        }
        return result;
    }
    void CommitBaseSettingChanges()
    {
        if (s_settings_interface)
            s_settings_interface->Save();
        if (s_secrets_settings_interface)
            s_secrets_settings_interface->Save();
    }
    void OnInputDeviceDisconnected(InputBindingKey, std::string_view) {}
    void OpenHostFileSelectorAsync(std::string_view, bool, std::function<void(const std::string&)>, std::vector<std::string>, std::string_view) {}
    std::unique_ptr<ProgressCallback> CreateHostProgressCallback() { return nullptr; }
    void OnAchievementsLoginSuccess(char const*, u32, u32, u32) { ARMSX2_PostRetroAchievementsStateChanged(); }
    void OnPerformanceMetricsUpdated()
    {
        if (!ARMSX2IOSRuntimeTelemetryEnabled())
            return;

        static std::atomic<uint> s_last_metrics_frame{0};
        const uint frame = ::g_FrameCount;
        const float fps = PerformanceMetrics::GetFPS();
        const float internal_fps = PerformanceMetrics::GetInternalFPS();
        const float speed = PerformanceMetrics::GetSpeed();
        const float cpu_usage = PerformanceMetrics::GetCPUThreadUsage();
        const float vu_usage = PerformanceMetrics::GetVUThreadUsage();
        const float gs_usage = PerformanceMetrics::GetGSThreadUsage();
        const float gpu_usage = PerformanceMetrics::GetGPUUsage();
        const bool hot_sample =
            (frame > 300 && fps < 58.0f) ||
            cpu_usage >= 85.0f ||
            vu_usage >= 35.0f ||
            gs_usage >= 25.0f ||
            gpu_usage >= 25.0f;
        const uint min_frame_delta = hot_sample ? 60 : 300;
        uint last = s_last_metrics_frame.load(std::memory_order_relaxed);
        if (last != 0 && frame < last + min_frame_delta)
            return;
        if (!s_last_metrics_frame.compare_exchange_strong(last, frame, std::memory_order_relaxed))
            return;

        std::fprintf(stderr,
            "@@PERF@@ frame=%u pm_frame=%llu fps=%.2f internal_fps=%.2f speed=%.2f cpu=%.2f vu=%.2f gs=%.2f gpu=%.2f state=%d\n",
            frame,
            static_cast<unsigned long long>(PerformanceMetrics::GetFrameNumber()),
            fps,
            internal_fps,
            speed,
            cpu_usage,
            vu_usage,
            gs_usage,
            gpu_usage,
            static_cast<int>(VMManager::GetState()));
    }
    void OnAchievementsLoginRequested(Achievements::LoginRequestReason) { ARMSX2_PostRetroAchievementsStateChanged(); }
    bool ShouldPreferHostFileSelector() { return false; }
    void OnCoverDownloaderOpenRequested() {}
    void OnCreateMemoryCardOpenRequested() {}
    void OnAchievementsHardcoreModeChanged(bool) {
        ARMSX2_PostRetroAchievementsStateChanged();
        // Re-evaluate .pnach enable lists now: ReloadEnabledLists gates cheats and patches on
        // Hardcore, so previously-enabled entries stop applying (or resume) the moment Hardcore
        // toggles, without waiting for the next patch reload.
        if (VMManager::HasValidVM()) {
            RunOnCPUThread([]() {
                VMManager::ReloadPatches(false, true, false, true);
            }, false);
        }
    }
    void SetMouseLock(bool) {}
    int LocaleSensitiveCompare(std::string_view lhs, std::string_view rhs) { return lhs.compare(rhs); }
    void OpenURL(std::string_view) {}
}

#pragma mark - namespace Host::Internal
namespace Host::Internal
{
    s32 GetTranslatedStringImpl(const std::string_view, const std::string_view msg, char* tbuf, size_t tbuf_space)
    {
        if (msg.size() > tbuf_space)
            return -1;

        if (!msg.empty())
            std::memcpy(tbuf, msg.data(), msg.size());

        return static_cast<s32>(msg.size());
    }
}

#pragma mark - SDL fullscreen hooks
// Called from ARMSX2Bridge to toggle SDL fullscreen (controls status bar visibility)
extern "C" void ARMSX2_SetSDLFullscreen(bool enabled) {
    if (Host::g_sdl_window)
        SDL_SetWindowFullscreen(Host::g_sdl_window, enabled);
}

extern "C" bool ARMSX2_IsSDLFullscreen() {
    if (!Host::g_sdl_window)
        return false;
    return (SDL_GetWindowFlags(Host::g_sdl_window) & SDL_WINDOW_FULLSCREEN) != 0;
}

// IOCtlSrc Stubs
#pragma mark - IOCtlSrc stubs
IOCtlSrc::IOCtlSrc(std::string filename) : m_filename(std::move(filename)) {}
IOCtlSrc::~IOCtlSrc() {}
bool IOCtlSrc::Reopen(Error*) { return false; }
u32 IOCtlSrc::GetSectorCount() const { return 0; }
const std::vector<toc_entry>& IOCtlSrc::ReadTOC() const { static std::vector<toc_entry> empty; return empty; }
bool IOCtlSrc::ReadSectors2048(u32, u32, u8*) const { return false; }
bool IOCtlSrc::ReadSectors2352(u32, u32, u8*) const { return false; }
bool IOCtlSrc::ReadTrackSubQ(cdvdSubQ*) const { return false; }
u32 IOCtlSrc::GetLayerBreakAddress() const { return 0; }
s32 IOCtlSrc::GetMediaType() const { return 0; }
void IOCtlSrc::SetSpindleSpeed(bool) const {}
bool IOCtlSrc::DiscReady() { return false; }

// ... InputManager Stubs ...
#pragma mark - InputManager stubs
namespace InputManager {
    void Initialize() {}
    void Shutdown() {}
    void Update() {}
    void SetRumble(int, u8, u8) {}
    const char* ConvertHostKeyboardCodeToIcon(unsigned int) { return ""; }
    std::optional<std::string> ConvertHostKeyboardCodeToString(unsigned int) { return std::nullopt; }
    std::optional<unsigned int> ConvertHostKeyboardStringToCode(std::string_view) { return std::nullopt; }
}

// ... HTTP ...
#pragma mark - HTTP downloader
class IOSHTTPDownloader final : public HTTPDownloader
{
public:
    explicit IOSHTTPDownloader(std::string user_agent)
        : m_user_agent(std::move(user_agent))
    {
        @autoreleasepool {
            NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
            configuration.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
            configuration.URLCache = nil;
            configuration.HTTPShouldSetCookies = NO;
            NSURLSession* session = [NSURLSession sessionWithConfiguration:configuration];
#if __has_feature(objc_arc)
            m_session = (__bridge_retained void*)session;
#else
            m_session = (void*)[session retain];
#endif
            NSLog(@"[ARMSX2 iOS HTTP] NSURLSession created: %@", session);
        }
    }

    ~IOSHTTPDownloader() override
    {
        if (m_session)
        {
#if __has_feature(objc_arc)
            NSURLSession* session = (__bridge_transfer NSURLSession*)m_session;
#else
            NSURLSession* session = (NSURLSession*)m_session;
#endif
            m_session = nullptr;
            [session invalidateAndCancel];
#if !__has_feature(objc_arc)
            [session release];
#endif
        }
    }

protected:
    struct IOSRequest final : Request
    {
        NSURLSessionDataTask* task = nil;
        std::mutex completion_mutex;
        bool completion_ready = false;
        s32 completed_status_code = HTTP_STATUS_ERROR;
        u32 completed_content_length = 0;
        std::string completed_content_type;
        Request::Data completed_data;

#if !__has_feature(objc_arc)
        ~IOSRequest()
        {
            [task release];
        }
#endif
    };

    Request* InternalCreateRequest() override
    {
        return new IOSRequest();
    }

    void InternalPollRequests() override
    {
        for (Request* request : m_pending_http_requests)
        {
            IOSRequest* native_request = static_cast<IOSRequest*>(request);
            std::lock_guard<std::mutex> completion_lock(native_request->completion_mutex);
            if (!native_request->completion_ready)
                continue;

            native_request->status_code = native_request->completed_status_code;
            native_request->content_length = native_request->completed_content_length;
            native_request->content_type = std::move(native_request->completed_content_type);
            native_request->data = std::move(native_request->completed_data);
            native_request->completion_ready = false;
            native_request->state.store(Request::State::Complete, std::memory_order_release);
        }
    }

    bool StartRequest(Request* request) override
    {
        IOSRequest* native_request = static_cast<IOSRequest*>(request);

        @autoreleasepool {
            NSString* url_string = [NSString stringWithUTF8String:request->url.c_str()];
            NSURL* url = url_string ? [NSURL URLWithString:url_string] : nil;
            if (!url)
            {
                request->status_code = HTTP_STATUS_ERROR;
                request->state.store(Request::State::Complete);
                NSLog(@"[ARMSX2 iOS HTTP] Invalid URL: %@", url_string ?: @"<nil>");
                return true;
            }

            NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
            url_request.timeoutInterval = m_timeout;
            url_request.cachePolicy = NSURLRequestReloadIgnoringLocalCacheData;

            NSString* user_agent = [NSString stringWithUTF8String:m_user_agent.c_str()];
            if (user_agent.length > 0)
                [url_request setValue:user_agent forHTTPHeaderField:@"User-Agent"];

            if (request->type == Request::Type::Post)
            {
                url_request.HTTPMethod = @"POST";
                url_request.HTTPBody = [NSData dataWithBytes:request->post_data.data() length:request->post_data.size()];
                [url_request setValue:@"application/x-www-form-urlencoded" forHTTPHeaderField:@"Content-Type"];
            }
            else
            {
                url_request.HTTPMethod = @"GET";
            }

            NSString* debug_url = url.absoluteString ?: @"";
#if __has_feature(objc_arc)
            NSURLSession* session = (__bridge NSURLSession*)m_session;
#else
            NSURLSession* session = (NSURLSession*)m_session;
#endif
            if (!session || ![session respondsToSelector:@selector(dataTaskWithRequest:completionHandler:)])
            {
                request->status_code = HTTP_STATUS_ERROR;
                request->state.store(Request::State::Complete);
                NSLog(@"[ARMSX2 iOS HTTP] Invalid NSURLSession while starting %@", debug_url);
                return true;
            }
            request->state.store(Request::State::Started);

            NSURLSessionDataTask* task = [session dataTaskWithRequest:url_request completionHandler:
                ^(NSData* data, NSURLResponse* response, NSError* error) {
                    s32 status_code = HTTP_STATUS_ERROR;
                    u32 content_length = 0;
                    std::string content_type;
                    Request::Data response_data;

                    if (error)
                    {
                        if (error.code == NSURLErrorCancelled)
                            status_code = HTTP_STATUS_CANCELLED;
                        else if (error.code == NSURLErrorTimedOut)
                            status_code = HTTP_STATUS_TIMEOUT;

                        NSLog(@"[ARMSX2 iOS HTTP] %@ failed: %@", debug_url, error.localizedDescription);
                    }
                    else
                    {
                        NSHTTPURLResponse* http_response = [response isKindOfClass:[NSHTTPURLResponse class]] ?
                            (NSHTTPURLResponse*)response : nil;
                        status_code = http_response ? static_cast<s32>(http_response.statusCode) : HTTP_STATUS_ERROR;

                        NSString* mime_type = response.MIMEType;
                        if (mime_type.length > 0)
                            content_type = mime_type.UTF8String;

                        const long long expected_length = response.expectedContentLength;
                        if (expected_length > 0)
                            content_length = static_cast<u32>(std::min<long long>(expected_length, UINT32_MAX));

                        if (data.length > 0)
                        {
                            response_data.resize(data.length);
                            std::memcpy(response_data.data(), data.bytes, data.length);
                        }

                        NSLog(@"[ARMSX2 iOS HTTP] %@ -> %d (%lu bytes)", debug_url, status_code,
                            static_cast<unsigned long>(data.length));
                    }

                    {
                        std::lock_guard<std::mutex> completion_lock(native_request->completion_mutex);
                        native_request->completed_status_code = status_code;
                        native_request->completed_content_length = content_length;
                        native_request->completed_content_type = std::move(content_type);
                        native_request->completed_data = std::move(response_data);
                        native_request->completion_ready = true;
                    }
                }];

#if __has_feature(objc_arc)
            native_request->task = task;
#else
            native_request->task = [task retain];
#endif
            [native_request->task resume];
        }

        return true;
    }

    void CloseRequest(Request* request) override
    {
        IOSRequest* native_request = static_cast<IOSRequest*>(request);
        const Request::State state = native_request->state.load();

        if (state == Request::State::Complete)
        {
            delete native_request;
            return;
        }

        // NSURLSession can still deliver a completion after cancellation. Keep the tiny
        // request object alive in that rare timeout/cancel path to avoid a use-after-free.
        [native_request->task cancel];
#if !__has_feature(objc_arc)
        [native_request->task release];
#endif
        native_request->task = nil;
    }

private:
    std::string m_user_agent;
    void* m_session = nullptr;
};

std::unique_ptr<HTTPDownloader> HTTPDownloader::Create(std::string user_agent)
{
    return std::make_unique<IOSHTTPDownloader>(std::move(user_agent));
}

// Global stubs for DISCopen
void GetValidDrive(std::string&) {  }
std::vector<std::string> GetOpticalDriveList() { return {}; }

#pragma mark - namespace FileSystem
namespace FileSystem {
    int OpenFDFileContent(const char*) { return -1; } // Added overload
    bool OpenFDFileContent(const std::string&, int, s64, s64) { return false; }
    std::string GetValidDrive(const std::string&) { return ""; }
    std::vector<std::string> GetOpticalDriveList() { return {}; }
}


// ... CocoaTools Stub ...
#pragma mark - namespace CocoaTools
namespace CocoaTools {
    void InhibitAppNap(const std::string&) {}
    void UninhibitAppNap() {}
    std::string GetBundlePath() { return [[NSBundle mainBundle].bundlePath UTF8String]; }
    
    void* CreateMetalLayer(WindowInfo* wi) {
        if (!Host::g_sdl_window) return nullptr;
        
        // Return existing layer if we already have it
        if (wi->surface_handle) {
            return SDL_Metal_GetLayer((SDL_MetalView)wi->surface_handle);
        }
        
        // Create the Metal view
        SDL_MetalView view = SDL_Metal_CreateView(Host::g_sdl_window);
        if (!view) {
            Console.Error("SDL_Metal_CreateView failed: %s", SDL_GetError());
            return nullptr;
        }
        
        void* layer = SDL_Metal_GetLayer(view);
        wi->surface_handle = view; // Store view handle to destroy later
        Console.WriteLn("Created Metal Layer: %p from View: %p", layer, view);
        return layer;
    }
    
    void DestroyMetalLayer(WindowInfo* wi) {
        if (wi->surface_handle) {
            Console.WriteLn("Destroying Metal View: %p", wi->surface_handle);
            SDL_Metal_DestroyView((SDL_MetalView)wi->surface_handle);
            wi->surface_handle = nullptr;
        }
    }
}

// ... AudioStream Stub ...
#include "pcsx2/Host/AudioStream.h"
// ... PCAP Stub ...
PCAPAdapter::PCAPAdapter() {}
PCAPAdapter::~PCAPAdapter() {}
bool PCAPAdapter::blocks() { return false; }
bool PCAPAdapter::isInitialised() { return false; }
bool PCAPAdapter::recv(NetPacket*) { return false; }
bool PCAPAdapter::send(NetPacket*) { return false; }
void PCAPAdapter::reloadSettings() {}
std::vector<AdapterEntry> PCAPAdapter::GetAdapters() { return {}; }
AdapterOptions PCAPAdapter::GetAdapterOptions() { return {}; }
bool PCAPAdapter::InitPCAP(const std::string&, bool) { return false; }
bool PCAPAdapter::SetMACSwitchedFilter(PacketReader::MAC_Address) { return false; }
void PCAPAdapter::SetMACBridgedRecv(NetPacket*) {}
void PCAPAdapter::SetMACBridgedSend(NetPacket*) {}
void PCAPAdapter::HandleFrameCheckSequence(NetPacket*) {}
bool PCAPAdapter::ValidateEtherFrame(NetPacket*) { return false; }
