#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <mutex>
#include "PrecompiledHeader.h"
#include "tests/arm64/run_tests.h"
#include "tests/core/run_patch_tests.h"
#include "tests/mvu/run_mvu_tests.h"
#include "tests/ee/run_ee_tests.h"
#include "tests/ee/run_ee_seq_tests.h"
#include "tests/vif/run_vif_tests.h"
#include "common/StringUtil.h"
#include "common/FileSystem.h"
#include "common/ZipHelpers.h"
#include "pcsx2/GS.h"
#include "pcsx2/Counters.h"
#include "pcsx2/VMManager.h"
#include "pcsx2/CDVD/CDVDcommon.h"
#include "pcsx2/CDVD/CDVD.h" // cdvdSaveNVRAM (flush BIOS NVM on background)
#include "SIO/Memcard/MemoryCardFile.h"
#include "pcsx2/Patch.h"
#include "pcsx2/R5900.h"
#include "pcsx2/EEDiffVerify.h" // @@EEDIFF@@ diff-verifier toggle
#include <atomic>
#include <thread>
#include "PerformanceMetrics.h"
#include "GameList.h"
#include "GameDatabase.h"
#include "GS/GSPerfMon.h"
#include "GS/Renderers/Common/GSDevice.h" // GSDevice::SetShaderChainParams (shader chain params)
#include "GS/Renderers/Vulkan/VKShaderCache.h"
#include "GSDumpReplayer.h"
#include "ImGui/ImGuiManager.h"
#include "ImGui/ImGuiOverlays.h"
#include "common/Path.h"
#include "common/MemorySettingsInterface.h"
#include "common/SettingsWrapper.h"
#include "pcsx2/INISettingsInterface.h"
#include "SIO/Pad/Pad.h"
#include "Input/InputManager.h"
#include "USB/USB.h"
#include "USB/deviceproxy.h"
#include "USB/qemu-usb/hid.h"
#include "ImGui/ImGuiFullscreen.h"
#include "Achievements.h"
#include "common/Error.h"
#include "common/HTTPDownloaderAndroid.h"
#include "Host.h"
#include "ImGui/FullscreenUI.h"
#include "SIO/Pad/PadDualshock2.h"
#include "MTGS.h"
#include "SPU2/spu2.h"
#include "GS/Renderers/Vulkan/VKLoader.h"
#include "GS/Renderers/HW/GSTextureReplacements.h"
#include "GS/Renderers/Common/GSRenderer.h"
#include "SDL3/SDL.h"
#include "ps2/BiosTools.h"
#include "BuildVersion.h"
#include "native-lib.h"
#include "libchdr/chd.h"
#include <algorithm>
#include <cmath>

#include "common/HostSys.h"
#include <cctype>
#include <condition_variable>
#include <deque>
#include <future>
#include <functional>
#include <thread>
#include <regex>
#include <vector>


// Redirect stdout/stderr to Android logcat so Vixl/libc abort messages are visible.
static void* stdout_redirect_thread(void* fd_ptr)
{
    int fd = (int)(intptr_t)fd_ptr;
    char buf[512];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0)
    {
        buf[n] = '\0';
        __android_log_print(ANDROID_LOG_WARN, "STDOUT", "%s", buf);
    }
    close(fd);
    return nullptr;
}
static void redirect_stdout_to_logcat()
{
    int pfds[2];
    if (pipe(pfds) != 0) return;
    dup2(pfds[1], STDOUT_FILENO);
    dup2(pfds[1], STDERR_FILENO);
    close(pfds[1]);
    pthread_t t;
    pthread_create(&t, nullptr, stdout_redirect_thread, (void*)(intptr_t)pfds[0]);
    pthread_detach(t);
}

#include <atomic>

// librashader's preset API, used here ONLY to read a preset's tweakable parameters for the
// UI. No runtime is needed for that, so no LIBRA_RUNTIME_* opt-in: the chain itself lives in
// GSDeviceVK/GSDeviceOGL. emucore links PCSX2_FLAGS, which carries both the define and the
// header's include dir, so this compiles out cleanly on a cargo-less build.
#ifdef ARMSX2_HAS_LIBRASHADER
#include "librashader.h"
#endif

// True whenever the CPU thread is parked outside Cpu->Execute() (runVMThread's
// loop flips it around each Execute() call). Read cross-thread by the savestate
// JNI entry points to confirm the VM is actually quiescent, hence atomic.
std::atomic<bool> s_execute_exit{false};
// Latched the moment an Android shutdown is requested. The run loop honours
// this regardless of VMState, because the settings-overlay pause/resume tasks
// can race the async shutdown and flip s_state back to Running/Paused after
// SetState(Stopping) — which previously let the run loop re-enter Execute()
// forever (the exit-game hang: EE breaks out, loop re-enters, repeat). Reset
// at the top of runVMThread so a fresh launch starts clean.
std::atomic<bool> s_stop_requested{false};
static std::mutex s_cpu_thread_mutex;
static std::deque<std::function<void()>> s_cpu_thread_queue;
static std::thread::id s_cpu_thread_id;
int s_window_width = 0;
int s_window_height = 0;
// Display refresh rate (Hz) reported by the Kotlin surface layer via
// setDisplayRefreshRate(). 0 = unknown -> throttle/pacing falls back to 60Hz.
// Populated so high-refresh handhelds (90/120Hz) pace against the real panel
// instead of being 60Hz-blind. Guarded by s_window_mutex.
float s_window_refresh_rate = 0.0f;
ANativeWindow* s_window = nullptr;
// Guards s_window against the UI-thread surfaceChanged/surfaceDestroyed
// writers racing the GS thread's AcquireRenderWindow reader. Without it the
// GS thread can read s_window an instant before the UI thread releases the
// final reference — vkCreateAndroidSurfaceKHR then does RefBase::incStrong
// on freed memory (observed as recurring SIGSEGV fault_addr=0x4 in the GS
// thread). The lock is only held for pointer swaps and a refcount bump, so
// the UI thread never waits on GPU work.
static std::mutex s_window_mutex;
// The GS thread's own reference on the window it last acquired, released on
// the next acquire or via ReleaseRenderWindow. Keeps the window alive past
// the UI thread dropping its reference; surface creation on an abandoned
// (but live) window fails cleanly instead of crashing.
static ANativeWindow* s_acquired_window = nullptr;

// File-backed base settings store. V7 used MemorySettingsInterface here, which
// made UI writes such as memory card slots, OSD toggles, and DEV9 options vanish
// after a cold restart unless Kotlin also happened to mirror them.
static std::unique_ptr<INISettingsInterface> s_settings_interface;
static std::string s_settings_interface_path;
// File-backed RetroAchievements credentials store. Holds Token (written by
// rcheevos at Achievements.cpp:2018) AND Username (mirrored from BASE on
// login so it survives restart). Path resolved in Java_..._initialize once
// EmuFolders::DataRoot is known. Lazy-constructed std::unique_ptr because
// INISettingsInterface needs a path at construction.
static std::unique_ptr<INISettingsInterface> s_secrets_settings_interface;
static std::string s_secrets_settings_interface_path;

static JNIEnv env_main;

// Cached JVM + refs for callbacks originating on non-Java threads (e.g. vmSetPaused).
// Populated once in initialize() while we have a valid Java-thread env.
static JavaVM*    s_jvm              = nullptr;
static jclass     s_NativeApp_class  = nullptr;  // GlobalRef
static jmethodID  s_vmSetPaused_mid  = nullptr;
static jmethodID  s_onPadRumble_mid  = nullptr;
static jmethodID  s_playSound_mid    = nullptr;

////
std::string GetJavaString(JNIEnv *env, jstring jstr) {
    if (!jstr) {
        return "";
    }
    const char *str = env->GetStringUTFChars(jstr, nullptr);
    std::string cpp_string = std::string(str);
    env->ReleaseStringUTFChars(jstr, str);
    return cpp_string;
}

#ifdef ARMSX2_PGO_GENERATE
// compiler-rt profile runtime — present only in the -fprofile-generate build.
extern "C" void __llvm_profile_set_filename(const char*);
extern "C" int __llvm_profile_write_file(void);
#endif

// PGO instrument build: flush collected profile counters to the .profraw file
// (path set via __llvm_profile_set_filename in initialize()). Called from Kotlin
// onPause so a profiling run survives an Android process kill. No-op in normal
// builds (the profile runtime isn't linked).
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_dumpPgoProfile(JNIEnv*, jclass) {
#ifdef ARMSX2_PGO_GENERATE
    __llvm_profile_write_file();
#endif
}

// Save a GS dump (.gs) to EmuFolders::Snapshots — a replayable capture of the
// GPU command stream, for diagnosing rendering bugs (replay in desktop PCSX2).
// Mirrors the GSDumpSingleFrame/MultiFrame hotkeys (GS.cpp).
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_captureGsDump(JNIEnv*, jclass, jint frames) {
    const u32 n = (frames > 0) ? static_cast<u32>(frames) : 1u;
    MTGS::RunOnGSThread([n]() { GSQueueSnapshot(std::string(), n); });
}

// @@EEDIFF@@ Toggle the EE recompiler-vs-interpreter differential verifier (throwaway
// diagnostic — see EEDiffVerify.h). Sets the enable flag AND clears the EE block cache so
// blocks recompile WITH (enabled) or WITHOUT (disabled) the per-op verify hooks. With it
// on, load True Crime NYC and watch logcat for "@@EEDIFF@@ ... DIVERGE ..." — the first
// line names the exact guest instruction that miscompiles.
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setEeDiffVerify(JNIEnv*, jclass, jboolean enabled) {
    eeDiffSetEnabled(enabled == JNI_TRUE);
    Console.WriteLnFmt("EE diff verify {}", enabled == JNI_TRUE ? "ENABLED" : "disabled");
    // Reset the EE recompiler so every block recompiles with the new hook state. Cpu is
    // the active R5900 provider (the mac ARM64 rec on Android); Reset -> recResetEE, which
    // defers safely to the dispatcher if a block is currently executing.
    if (Cpu)
        Cpu->Reset();
}

// Read the real flag so the UI can reflect it. The toggle previously kept its
// state in a Compose `remember`, so navigating away reset the switch to off while the native
// flag stayed on — the switch was lying about whether the diagnostic was armed.
extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_isEeDiffVerify(JNIEnv*, jclass) {
    return eeDiffGetEnabled() ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_initialize(JNIEnv *env, jclass clazz,
                                                jstring p_szpath,
                                                jstring p_szbiosfolder,
                                                jint p_apiVer) {
    redirect_stdout_to_logcat();
    // p_szpath is the user's chosen system folder (memcards, savestates,
    // configs land here) when set up via the wizard; falls back to the
    // app's externalFilesDir when unset. p_szbiosfolder is always the
    // app's externalFilesDir/bios — the wizard copies the user-picked
    // BIOS there via private File APIs, which the chosen-systemDir path
    // can't necessarily host on Android 11+ scoped storage. Pinning
    // Folders/Bios separately keeps BIOS loading working regardless of
    // where DataRoot points.
    std::string _szPath = GetJavaString(env, p_szpath);
    std::string _szBiosFolder = GetJavaString(env, p_szbiosfolder);
    EmuFolders::AppRoot = _szPath;
    EmuFolders::DataRoot = _szPath;
    EmuFolders::SetResourcesDirectory();

#ifdef ARMSX2_PGO_GENERATE
    // PGO instrument build: redirect the .profraw output to an on-device writable
    // dir — the baked -fprofile-dir is the build machine's path. set_filename
    // overrides the env reliably (the runtime may have read LLVM_PROFILE_FILE at
    // load already). %p=pid, %m=binary signature so the 4k/16k cores stay separate
    // and mergeable. NativeApp.dumpPgoProfile() flushes it (called on app pause).
    {
        const std::string pgo_dir = Path::Combine(EmuFolders::DataRoot, "pgo");
        FileSystem::CreateDirectoryPath(pgo_dir.c_str(), true);
        const std::string pgo_pat = Path::Combine(pgo_dir, "armsx2-%p-%m.profraw");
        __llvm_profile_set_filename(pgo_pat.c_str());
    }
#endif

    Log::SetConsoleOutputLevel(LOGLEVEL_DEBUG);
    // Font loading is handled by ImGuiManager::LoadFontData() using s_font_path fallback

    const std::string settings_path =
        Path::Combine(EmuFolders::DataRoot, "PCSX2-Android.ini");
    if (!s_settings_interface || s_settings_interface_path != settings_path)
    {
        s_settings_interface_path = settings_path;
        s_settings_interface = std::make_unique<INISettingsInterface>(settings_path);
        s_settings_interface->Load();
        Host::Internal::SetBaseSettingsLayer(s_settings_interface.get());
    }

    const std::string secrets_path =
        Path::Combine(EmuFolders::DataRoot, "achievements.ini");
    if (!s_secrets_settings_interface || s_secrets_settings_interface_path != secrets_path)
    {
        // Build the secrets layer file-backed at <DataRoot>/achievements.ini.
        // Persists the RetroAchievements auth token across app launches so
        // the user doesn't have to log in every cold start. Path::Combine
        // handles the trailing-slash form for both system and app-private
        // DataRoot. Load() returns false when the file doesn't exist yet
        // (first launch) — that's fine, the file gets created on first Save().
        s_secrets_settings_interface_path = secrets_path;
        s_secrets_settings_interface =
            std::make_unique<INISettingsInterface>(secrets_path);
        s_secrets_settings_interface->Load();
        Host::Internal::SetSecretsSettingsLayer(s_secrets_settings_interface.get());
    }

    INISettingsInterface& si = *s_settings_interface;
    const bool _SettingsIsEmpty = si.IsEmpty();
    if(_SettingsIsEmpty) {
        VMManager::SetDefaultSettings(si, true, true, true, true, true);

        // FrameLimitEnable is inert in this fork (no read site outside a
        // commented MTGS check). Frame pacing is driven by SetLimiterMode at
        // runtime; the persisted bool is applied after Initialize succeeds in
        // runVMThread below. Don't pre-force it here — that just confuses the
        // overlay's saved-state display.
        si.SetIntValue("EmuCore/GS", "VsyncEnable", false);
        si.SetBoolValue("EmuCore", "EnableThreadPinning", true);
        si.SetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", true);

        // ensure all input sources are disabled, we're not using them
        si.SetBoolValue("InputSources", "SDL", true);
        si.SetBoolValue("InputSources", "XInput", false);

        si.SetStringValue("SPU2/Output", "Backend", "Oboe");
        si.SetBoolValue("EmuCore", "EnableFastBoot", false);

        // Enable RetroAchievements by default. Pcsx2Config defaults this to
        // false (privacy-conscious for desktop), but on Android the in-game
        // overlay's right-side panel + login form make it discoverable, and
        // without Enabled=true, Achievements::Initialize never runs →
        // s_client stays null → s_has_achievements stays false → the panel
        // permanently shows "No achievements" even with a logged-in user
        // and a recognised game. Users who don't want RA can flip it off
        // via a future settings toggle (or env override).
        si.SetBoolValue("Achievements", "Enabled", true);

        // Pin BIOS folder to the app's externalFilesDir/bios regardless
        // of where the user pointed DataRoot. The setup wizard's
        // finishBiosStep copies the chosen BIOS file there via java.io
        // (always writable), and this absolute path bypasses the
        // DataRoot/bios default that EmuFolders::LoadConfig would
        // compute otherwise. Path::Combine treats absolute second args
        // as-is, so EmuFolders::Bios resolves directly to this folder.
        if (!_szBiosFolder.empty())
            si.SetStringValue("Folders", "Bios", _szBiosFolder.c_str());

        // Renderer is left at Auto (Pcsx2Config::DEFAULT_HW_RENDERER) so
        // GSUtil::GetPreferredRenderer chooses at runtime — on Android that
        // resolves to OpenGL HW. SW + VK can still be picked via the
        // RenderModeButton (cycles VULKAN_SW ↔ OPENGL). VK HW is intentionally
        // not in the cycle while its blending bugs remain unresolved.

        // OpenGL HW: leave texture barriers on Auto (-1).
        //
        // With the Mali GPU profile restored (see GSGPUProfile + the Mali
        // block in GSDeviceOGL::CheckFeatures), Auto is the correct default:
        //   - Mali devices that report GL_ARM_shader_framebuffer_fetch use
        //     that as the texture-barrier substitute. Forcing `1` here
        //     skipped the Mali Auto branch and installed
        //     MemoryBarrierAsTextureBarrier instead, which is the wrong
        //     path for Mali.
        //   - Adreno + other GLES devices still fall through to the
        //     multidraw_fb_copy fallback (or the ARB barrier path on
        //     desktop) without the override.
        //
        // The earlier `= 1` (Force Enabled) was a diagnostic experiment for
        // SH2 pop-in. If that regression returns under Auto, expose the
        // override as a per-user toggle in the Renderer tab rather than
        // forcing it globally.
        si.SetIntValue("EmuCore/GS", "OverrideTextureBarriers", -1);

        // none of the bindings are going to resolve to anything
        Pad::ClearPortBindings(si, 0);
        si.ClearSection("Hotkeys");

        // force logging
        //si.SetBoolValue("Logging", "EnableSystemConsole", !s_no_console);
        si.SetBoolValue("Logging", "EnableSystemConsole", true);
        si.SetBoolValue("Logging", "EnableTimestamps", true);
        si.SetBoolValue("Logging", "EnableVerbose", true);

        // Perf OSD defaults OFF (it reads as clutter). The canonical GSOptions
        // bitfields default every OsdShow* to 1 (desktop PCSX2 shows FPS etc. by
        // default), so we MUST explicitly seed them false here — otherwise a fresh
        // install renders the overlay even though the UI toggle reads "off".
        // This block only runs when the INI IsEmpty() (first launch), so it never
        // clobbers a returning user who turned the OSD on; the overlay renderer
        // (ImGuiOverlays.cpp DrawPerformanceOverlay) reads EmuConfig.GS, which loads
        // exactly these seeded values. The in-game "On-screen display" toggle turns
        // them back on and persists true, which this block then skips.
        for (const char* k : {"OsdShowFPS", "OsdShowVPS", "OsdShowSpeed", "OsdShowResolution",
            "OsdShowGSStats", "OsdShowCPU", "OsdShowGPU", "OsdShowGPUStats", "OsdShowFrameTimes",
            "OsdShowHardwareInfo", "OsdShowVersion", "OsdShowSettings", "OsdShowInputs"})
        {
            si.SetBoolValue("EmuCore/GS", k, false);
        }
//        // remove memory cards, so we don't have sharing violations
//        for (u32 i = 0; i < 2; i++)
//        {
//            si.SetBoolValue("MemoryCards", fmt::format("Slot{}_Enable", i + 1).c_str(), false);
//            si.SetStringValue("MemoryCards", fmt::format("Slot{}_Filename", i + 1).c_str(), "");
//        }
    }

    if (!_szBiosFolder.empty())
        si.SetStringValue("Folders", "Bios", _szBiosFolder.c_str());

    // Mirror Username from secrets → BASE so Achievements::Initialize's
    // GetBaseStringSettingValue("Achievements","Username") finds it on a
    // returning user even after the base settings layer is loaded from disk.
    const std::string saved_user = s_secrets_settings_interface->GetStringValue(
        "Achievements", "Username", "");
    if (!saved_user.empty())
        si.SetStringValue("Achievements", "Username", saved_user.c_str());

    if (si.IsDirty())
        si.Save();

    VMManager::Internal::LoadStartupSettings();

    // Cache JavaVM + NativeApp refs for use from non-Java threads.
    env->GetJavaVM(&s_jvm);
    if (jclass local = env->FindClass("kr/co/iefriends/pcsx2/NativeApp")) {
        s_NativeApp_class = static_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);
        s_vmSetPaused_mid = env->GetStaticMethodID(s_NativeApp_class, "vmSetPaused", "(Z)V");
        s_onPadRumble_mid = env->GetStaticMethodID(s_NativeApp_class, "onPadRumble", "(III)V");
        s_playSound_mid   = env->GetStaticMethodID(s_NativeApp_class, "playSound", "(Ljava/lang/String;)V");
    }

    // Bind the JNI-backed HTTP downloader's class + method IDs while we
    // still have a Java-thread env. Worker threads spawned from
    // HTTPDownloaderAndroid::StartRequest don't have a class loader, so
    // FindClass would fail there — these globals must be cached up front.
    HTTPDownloaderAndroid::BindFromJNI(env);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getGameTitle(JNIEnv *env, jclass clazz,
                                                  jstring p_szpath) {
    std::string _szPath = GetJavaString(env, p_szpath);

    // The Android library is scanned in Kotlin, so the NATIVE game-list cache is
    // usually empty — GetEntryForPath misses and the info tab gets no CRC (the
    // title/serial come from the Kotlin GameInfo, which is why those showed but
    // CRC was blank). Fall back to an on-demand populate (reads the disc to
    // compute serial + CRC). Callers invoke getGameTitle off the UI thread.
    GameList::Entry temp_entry;
    const GameList::Entry *entry = GameList::GetEntryForPath(_szPath.c_str());
    if (!entry || entry->crc == 0)
    {
        if (GameList::PopulateEntryFromPath(_szPath, &temp_entry))
            entry = &temp_entry;
    }
    if (!entry)
        return env->NewStringUTF("");

    std::string ret;
    ret.append(entry->title);
    ret.append("|");
    ret.append(entry->serial);
    ret.append("|");
    ret.append(StringUtil::StdStringFromFormat("%s (%08X)", entry->serial.c_str(), entry->crc));

    return env->NewStringUTF(ret.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getGameSerial(JNIEnv *env, jclass clazz) {
    std::string ret = VMManager::GetDiscSerial();
    return env->NewStringUTF(ret.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getGameCRC(JNIEnv *env, jclass clazz) {
    std::string ret = StringUtil::StdStringFromFormat("%08X", VMManager::GetCurrentCRC());
    return env->NewStringUTF(ret.c_str());
}

// Build version string sourced from BuildVersion::GitRev. Format:
//   "GitTagHi.GitTagMid.GitTagLo.ARMSX2Build-SNAPSHOT"
// Used by the setup wizard + in-game overlay to show the build label
// without hardcoding the values on the Kotlin side.
extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getBuildVersion(JNIEnv *env, jclass clazz) {
    return env->NewStringUTF(BuildVersion::GitRev);
}

// Achievements snapshot for the in-game overlay's right-side panel.
// Format documented at Achievements::GetAchievementsAsJSON. Empty payload
// (`{"active":false,"loggedIn":false,"userName":"","items":[]}`) when no
// active game / no client / not logged in.
extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getAchievementsJSON(JNIEnv *env, jclass clazz) {
    const std::string json = Achievements::GetAchievementsAsJSON();
    return env->NewStringUTF(json.c_str());
}

// Live RetroAchievements rich-presence string — the rcheevos client
// recomputes this each second from the game's RAM (see Achievements.cpp
// UpdateRichPresence). On Android the AchievementsPanel polls this
// alongside the achievements JSON; rcheevos also auto-pings the RA
// server with the same string so the user's RA profile shows it. Returns
// empty string when no client / no game / RP not yet computed.
extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getRichPresence(JNIEnv *env, jclass clazz) {
    if (!Achievements::HasRichPresence())
        return env->NewStringUTF("");
    return env->NewStringUTF(Achievements::GetRichPresenceString().c_str());
}

// RetroAchievements password login. Synchronous — Achievements::Login waits
// for the HTTP request internally. Returns null on success, otherwise a
// human-readable error string (rcheevos message or "Failed to create
// client" / "Failed to create login request"). Callers should dispatch
// off the Main thread; the request is HTTP and may take a few seconds.
extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_loginAchievements(JNIEnv *env, jclass clazz,
                                                       jstring p_user, jstring p_pass) {
    const std::string user = GetJavaString(env, p_user);
    const std::string pass = GetJavaString(env, p_pass);
    Error error;
    const bool ok = Achievements::Login(user.c_str(), pass.c_str(), &error);
    if (!ok)
    {
        const std::string msg = error.GetDescription();
        return env->NewStringUTF(msg.empty() ? "Login failed." : msg.c_str());
    }

    // Login wrote Username to BASE (in-memory, lost on restart) and Token
    // to SECRETS (file-backed via INISettingsInterface — persists). Mirror
    // Username INTO secrets.ini too so the next app launch can re-push it
    // to BASE before Achievements::Initialize runs. Without this the user
    // re-logs in every launch even though the token survives.
    if (s_secrets_settings_interface)
    {
        s_secrets_settings_interface->SetStringValue("Achievements", "Username", user.c_str());
        s_secrets_settings_interface->Save();
    }

    // Achievements::Initialize is gated on EmuConfig.Achievements.Enabled —
    // a returning user with the old default-off config might still have it
    // off. Push Enabled=true and ApplySettings so UpdateSettings detects
    // the change and runs Initialize for any current/future VM. Initialize
    // reads the just-persisted Token and re-logs in on the persistent
    // s_client, then BeginLoadGame loads the running game's achievement
    // set.
    Host::SetBaseBoolSettingValue("Achievements", "Enabled", true);
    if (VMManager::HasValidVM())
        VMManager::ApplySettings();
    return nullptr;
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_logoutAchievements(JNIEnv *env, jclass clazz) {
    Achievements::Logout();
    // Achievements::Logout clears Token from SECRETS but leaves the
    // Username we co-stored there. Drop it too so a fresh launch doesn't
    // re-mirror a stale username back to BASE.
    if (s_secrets_settings_interface)
    {
        s_secrets_settings_interface->DeleteValue("Achievements", "Username");
        s_secrets_settings_interface->Save();
    }
}

// Enable / disable RetroAchievements hardcore mode. Persists the hardcore
// flag and applies it via VMManager::ApplySettings — the settings-diff path
// in Achievements::UpdateSettings() applies a turn-OFF live (DisableHardcoreMode),
// but a turn-ON on a running game is DEFERRED until the next system reset
// (upstream design: hardcore can only engage from a clean boot). The Kotlin
// side therefore resets the VM after calling this with `true`.
//
// CRITICAL: the INI key for hardcore is "ChallengeMode", NOT "HardcoreMode" —
// Pcsx2Config maps the field via SettingsWrapBitBoolEx(HardcoreMode,
// "ChallengeMode") and upstream FullscreenUI reads/writes "ChallengeMode".
// Writing the wrong key meant ApplySettings never saw the change (the toggle
// silently did nothing). We also Save() so the choice survives a process kill.
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setHardcoreMode(JNIEnv *env, jclass clazz, jboolean enabled) {
    Host::SetBaseBoolSettingValue("Achievements", "ChallengeMode", enabled == JNI_TRUE);
    if (s_settings_interface && s_settings_interface->IsDirty())
        s_settings_interface->Save();
    if (VMManager::HasValidVM())
        VMManager::ApplySettings();
}

// Returns the live hardcore-mode flag (rcheevos s_hardcore_mode), not the
// persisted EmuConfig setting — they can transiently differ while a
// hardcore-enable is waiting for the next boot.
extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_isHardcoreMode(JNIEnv *env, jclass clazz) {
    return Achievements::IsHardcoreModeActive() ? JNI_TRUE : JNI_FALSE;
}

// Returns the PERSISTED hardcore setting (Achievements/ChallengeMode) — what will take
// effect on the next game boot. Unlike isHardcoreMode() this is valid with NO game
// running, so the global (home-screen) toggle can show and drive the setting directly
// instead of reflecting the live rcheevos flag (which is always off with no game).
extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_isHardcorePersisted(JNIEnv *env, jclass clazz) {
    return Host::GetBaseBoolSettingValue("Achievements", "ChallengeMode", false) ? JNI_TRUE : JNI_FALSE;
}

// Toggle one of the RetroAchievements presentation options (notifications,
// leaderboard notifications, in-game overlays/indicators, leaderboard
// trackers, sound effects). `key` is a stable lowercase id from the Kotlin
// panel; it maps to the [Achievements] INI key. Persists + ApplySettings so
// Achievements::UpdateSettings picks the change up live (these don't require
// a reset). The current values are surfaced back in getAchievementsJSON.
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setAchievementsOption(JNIEnv *env, jclass clazz,
                                                           jstring p_key, jboolean enabled) {
    const std::string key = GetJavaString(env, p_key);
    const char* ini_key = nullptr;
    if (key == "notifications") ini_key = "Notifications";
    else if (key == "leaderboardNotifications") ini_key = "LeaderboardNotifications";
    else if (key == "overlays") ini_key = "Overlays";
    else if (key == "lbOverlays") ini_key = "LBOverlays";
    else if (key == "soundEffects") ini_key = "SoundEffects";
    // Achievement MODES. The native side already handles these (Achievements.cpp
    // CreateClient + UpdateSettings call rc_client_set_{encore,spectator,unofficial}_mode);
    // ApplySettings below reloads the RA session for them with no VM reset needed.
    else if (key == "encoreMode") ini_key = "EncoreMode";
    else if (key == "spectatorMode") ini_key = "SpectatorMode";
    else if (key == "unofficialTestMode") ini_key = "UnofficialTestMode";
    if (!ini_key)
        return;

    Host::SetBaseBoolSettingValue("Achievements", ini_key, enabled == JNI_TRUE);
    if (s_settings_interface && s_settings_interface->IsDirty())
        s_settings_interface->Save();
    if (VMManager::HasValidVM())
        VMManager::ApplySettings();
}

// Custom achievement-unlock sound. Writes the [Achievements] UnlockSoundName path
// (an app-private absolute file the MediaPlayer reads on unlock) and enables the
// specific-sound path. An empty path clears it, so PlayAchievementSound falls back
// to the bundled default. Persisted to the base INI so it survives restarts.
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setAchievementsUnlockSound(JNIEnv *env, jclass clazz,
                                                                jstring p_path) {
    const std::string path = GetJavaString(env, p_path);
    Host::SetBaseStringSettingValue("Achievements", "UnlockSoundName", path.c_str());
    Host::SetBaseBoolSettingValue("Achievements", "UnlockSound", true);
    if (s_settings_interface && s_settings_interface->IsDirty())
        s_settings_interface->Save();
    if (VMManager::HasValidVM())
        VMManager::ApplySettings();
}

// Rebuild the rc_client so CreateClient re-reads the [Achievements] Host
// setting. UpdateSettings' diff path never re-creates the client on a Host
// change, so a live host switch needs an explicit teardown/reinit. No-op
// unless achievements are enabled and active — otherwise the next
// Initialize() picks the host up on its own.
static void RestartAchievementsForHostChange() {
    if (!EmuConfig.Achievements.Enabled || !Achievements::IsActive())
        return;
    Achievements::Shutdown(false);
    Achievements::Initialize();
}

static void PersistAndApplyAchievementsSettings() {
    if (s_settings_interface && s_settings_interface->IsDirty())
        s_settings_interface->Save();
    if (VMManager::HasValidVM())
        VMManager::ApplySettings();
}

// Point the RetroAchievements client at a loopback proxy. Drives the same
// [Achievements] Host setting CreateClient reads, so the override survives a
// cold start. Hardcore mode is left untouched here so it can be exercised
// against the dev proxy; it stays under the user's own control. An empty
// host is ignored (use the clear path instead).
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setAchievementsHostOverride(JNIEnv *env, jclass clazz, jstring p_host) {
    const std::string host = GetJavaString(env, p_host);
    if (host.empty())
        return;

    Host::SetBaseStringSettingValue("Achievements", "Host", host.c_str());

    // Older builds saved/forced hardcore off while an override was active;
    // we no longer touch hardcore, so drop any leftover saved-state key.
    Host::RemoveBaseSettingValue("Achievements", "HostOverrideSavedHardcore");

    PersistAndApplyAchievementsSettings();
    RestartAchievementsForHostChange();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_clearAchievementsHostOverride(JNIEnv *env, jclass clazz) {
    Host::RemoveBaseSettingValue("Achievements", "Host");
    Host::RemoveBaseSettingValue("Achievements", "HostOverrideSavedHardcore");

    PersistAndApplyAchievementsSettings();
    RestartAchievementsForHostChange();
}

// Live HW/SW state from the GS thread's POV. The in-game overlay's renderer
// pill mirrors this on every poll so an emucore-driven swap (e.g. SoftwareRendererFMVHack
// flipping to SW during an FMV) doesn't desync the UI from the actual state.
extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_isHardwareRenderer(JNIEnv *env, jclass clazz) {
    return GSIsHardwareRenderer() ? JNI_TRUE : JNI_FALSE;
}

// Custom Vulkan driver pin. Called from Main.applyRendererPrefs BEFORE the
// VM starts so the first MTGS::Open (which triggers Vulkan::LoadVulkanLibrary)
// picks up the custom driver. Empty strings revert to the system loader.
// See Vulkan::SetCustomDriverPath in VKLoader.cpp for the splice.
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setCustomVulkanDriver(
    JNIEnv* env, jclass clazz,
    jstring driverDir, jstring driverName,
    jstring redirectDir, jstring hookLibDir) {
    const std::string dir   = GetJavaString(env, driverDir);
    const std::string name  = GetJavaString(env, driverName);
    const std::string redir = GetJavaString(env, redirectDir);
    const std::string hook  = GetJavaString(env, hookLibDir);
    Vulkan::SetCustomDriverPath(
        dir.c_str(), name.c_str(), redir.c_str(), hook.c_str());
}

extern "C"
JNIEXPORT jfloat JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getFPS(JNIEnv *env, jclass clazz) {
    return (jfloat)PerformanceMetrics::GetFPS();
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getPauseGameTitle(JNIEnv *env, jclass clazz) {
    std::string ret = VMManager::GetTitle(true);
    return env->NewStringUTF(ret.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getPauseGameSerial(JNIEnv *env, jclass clazz) {
    std::string ret = StringUtil::StdStringFromFormat("%s (%08X)", VMManager::GetDiscSerial().c_str(), VMManager::GetDiscCRC());
    return env->NewStringUTF(ret.c_str());
}


extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setPadVibration(JNIEnv *env, jclass clazz,
                                                     jboolean p_isOnOff) {
}


// Serializes pad input (applyPadButton, on the Android input thread) against the
// co-op hot-plug rebuild (enablePad2's Pad::LoadConfig, on a background thread),
// which reassigns s_controllers[] (make_unique). ScopedVMPause only parks the
// CPU/MTGS/MTVU threads, NOT the input thread, so without this a P2-join could
// use-after-free the controller being replaced. Uncontended on the input thread
// (all input is serial on the UI thread) except for the brief enablePad2 window.
static std::mutex s_pad_mutex;

static void applyPadButton(u32 port, jint p_key, jint p_range, jboolean p_keyPressed) {
    PadDualshock2::Inputs _key;
    switch (p_key) {
        case 19: _key = PadDualshock2::Inputs::PAD_UP; break;
        case 22: _key = PadDualshock2::Inputs::PAD_RIGHT; break;
        case 20: _key = PadDualshock2::Inputs::PAD_DOWN; break;
        case 21: _key = PadDualshock2::Inputs::PAD_LEFT; break;
        case 100: _key = PadDualshock2::Inputs::PAD_TRIANGLE; break;
        case 97: _key = PadDualshock2::Inputs::PAD_CIRCLE; break;
        case 96: _key = PadDualshock2::Inputs::PAD_CROSS; break;
        case 99: _key = PadDualshock2::Inputs::PAD_SQUARE; break;
        case 109: _key = PadDualshock2::Inputs::PAD_SELECT; break;
        case 108: _key = PadDualshock2::Inputs::PAD_START; break;
        case 102: _key = PadDualshock2::Inputs::PAD_L1; break;
        case 104: _key = PadDualshock2::Inputs::PAD_L2; break;
        case 103: _key = PadDualshock2::Inputs::PAD_R1; break;
        case 105: _key = PadDualshock2::Inputs::PAD_R2; break;
        case 106: _key = PadDualshock2::Inputs::PAD_L3; break;
        case 107: _key = PadDualshock2::Inputs::PAD_R3; break;
        case 110: _key = PadDualshock2::Inputs::PAD_L_UP; break;
        case 111: _key = PadDualshock2::Inputs::PAD_L_RIGHT; break;
        case 112: _key = PadDualshock2::Inputs::PAD_L_DOWN; break;
        case 113: _key = PadDualshock2::Inputs::PAD_L_LEFT; break;
        case 120: _key = PadDualshock2::Inputs::PAD_R_UP; break;
        case 121: _key = PadDualshock2::Inputs::PAD_R_RIGHT; break;
        case 122: _key = PadDualshock2::Inputs::PAD_R_DOWN; break;
        case 123: _key = PadDualshock2::Inputs::PAD_R_LEFT; break;
        // Custom target (ControllerMappings "analog" action) — the DualShock2
        // Analog/mode button. Toggles analog mode; some early games (e.g. Driving
        // Emotion Type-S) need it pressed before the sticks work at all. The
        // native PAD already handles the toggle (shows "Analog light is now ...").
        case 200: _key = PadDualshock2::Inputs::PAD_ANALOG; break;
        default: _key = PadDualshock2::Inputs::PAD_CROSS ; break;
    }

    // Analog axis inputs (keycodes 110-123) carry a 0-32767 magnitude in p_range.
    // Digital buttons always use 1.0/0.0.
    const float state = p_keyPressed
        ? ((p_range > 0) ? (p_range / 32767.0f) : 1.0f)
        : 0.0f;
    // Pad input can arrive with no VM running (e.g. the gyroscope overlay emits a neutral
    // release when it first composes in the library) — the pads don't exist yet, so drop it.
    if (!VMManager::HasValidVM())
        return;
    std::lock_guard<std::mutex> lk(s_pad_mutex);
    Pad::SetControllerState(port, static_cast<u32>(_key), state);
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setPadButton(JNIEnv *env, jclass clazz,
                                                  jint p_key, jint p_range, jboolean p_keyPressed) {
    applyPadButton(0, p_key, p_range, p_keyPressed);
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setPadButtonForPort(JNIEnv *env, jclass clazz,
                                                         jint p_port, jint p_key, jint p_range,
                                                         jboolean p_keyPressed) {
    // Local co-op: route to PS2 controller port 0 (P1) or 1 (P2). SetControllerState
    // ignores ports >= NUM_CONTROLLER_PORTS; a negative/unset port falls back to P1.
    applyPadButton(p_port < 0 ? 0u : static_cast<u32>(p_port), p_key, p_range, p_keyPressed);
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_resetKeyStatus(JNIEnv *env, jclass clazz) {
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setEnableCheats(JNIEnv *env, jclass clazz,
                                                     jboolean p_isonoff) {
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setAspectRatio(JNIEnv *env, jclass clazz,
                                                    jint p_type) {
    const int ratio = std::clamp(static_cast<int>(p_type), 0,
        static_cast<int>(AspectRatioType::MaxCount) - 1);
    const char* name = Pcsx2Config::GSOptions::AspectRatioNames[ratio];
    if (!name)
        return;

    Host::SetBaseStringSettingValue("EmuCore/GS", "AspectRatio", name);
    EmuConfig.GS.AspectRatio = static_cast<AspectRatioType>(ratio);
    EmuConfig.CurrentAspectRatio = static_cast<AspectRatioType>(ratio);
}

// FMV Aspect Ratio override — applied only while an FMV/MPEG is playing (Counters.cpp
// swaps EmuConfig.CurrentAspectRatio to this on FMV state transitions, restoring the
// generic AspectRatio when the FMV ends). 0 Off (use the generic aspect) · 1 Auto
// 4:3/3:2 · 2 4:3 · 3 16:9 · 4 10:7. Mirrors setAspectRatio; updates EmuConfig.GS live
// so the next FMV transition honours a change made mid-session.
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setFmvAspectRatio(JNIEnv *env, jclass clazz,
                                                       jint p_type) {
    const int ratio = std::clamp(static_cast<int>(p_type), 0,
        static_cast<int>(FMVAspectRatioSwitchType::MaxCount) - 1);
    const char* name = Pcsx2Config::GSOptions::FMVAspectRatioSwitchNames[ratio];
    if (!name)
        return;

    Host::SetBaseStringSettingValue("EmuCore/GS", "FMVAspectRatioSwitch", name);
    EmuConfig.GS.FMVAspectRatioSwitch = static_cast<FMVAspectRatioSwitchType>(ratio);
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_speedhackLimitermode(JNIEnv *env, jclass clazz,
                                                          jint p_value) {
    // Enum values match LimiterModeType (Config.h:267):
    //   0 Nominal, 1 Turbo, 2 Slomo, 3 Unlimited.
    // Called from the in-game overlay's Frame Limit toggle (0 vs 3).
    // SetLimiterMode is a small atomic update + UpdateTargetSpeed —
    // safe to call from the JNI thread without RunOnCPUThread (the
    // Android port doesn't have one wired anyway). No-op when no VM
    // is running so we don't poke stale state.
    if (!VMManager::HasValidVM())
        return;
    LimiterModeType mode;
    switch (p_value) {
        case 0: mode = LimiterModeType::Nominal; break;
        case 1: mode = LimiterModeType::Turbo; break;
        case 2: mode = LimiterModeType::Slomo; break;
        case 3: mode = LimiterModeType::Unlimited; break;
        default: return;
    }
    // RetroAchievements hardcore forbids slow motion. This JNI bypasses
    // VMManager::ApplySettings (so EnforceAchievementsChallengeModeSettings
    // never runs), so guard Slomo here. Turbo/Unlimited (fast-forward) stay
    // allowed — RA only bans slowdown.
    if (mode == LimiterModeType::Slomo && Achievements::IsHardcoreModeActive())
        mode = LimiterModeType::Nominal;
    VMManager::SetLimiterMode(mode);
    // Suspend the Android present-FPS cap while fast-forwarding (Turbo) so the
    // speed-up is visible instead of being held at the cap. Unlimited (the
    // frame-limit-off steady state) keeps the cap — there the user still wants a
    // bounded DISPLAY rate over uncapped emulation. Re-engages on Nominal.
    GSSetPresentCapSuspended(mode == LimiterModeType::Turbo);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_toggleTextureDumping(JNIEnv *env, jclass clazz) {
    // Runtime toggle of texture dumping — mirrors PCSX2's built-in
    // "ToggleTextureDumping" hotkey (GS.cpp). Lets users flip dumping off during
    // FMVs so prerendered cutscenes don't spew thousands of dumped frames.
    // Runtime-only (not persisted), matching the upstream hotkey. Returns the
    // new state so the UI can show ON/OFF.
    if (!VMManager::HasValidVM())
        return JNI_FALSE;
    const bool newval = !EmuConfig.GS.DumpReplaceableTextures;
    EmuConfig.GS.DumpReplaceableTextures = newval;
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
    return newval ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_createMemoryCard(JNIEnv *env, jclass clazz,
                                                      jstring p_name, jint p_type, jint p_fileType) {
    // Create a new PS2 memory card in EmuFolders::MemoryCards. type:
    //   1 = File (.ps2), 2 = Folder. fileType (File only):
    //   1 = 8MB, 2 = 16MB, 3 = 32MB, 4 = 64MB. Returns success.
    const char* name_c = env->GetStringUTFChars(p_name, nullptr);
    if (!name_c)
        return JNI_FALSE;
    std::string name(name_c);
    env->ReleaseStringUTFChars(p_name, name_c);
    if (name.empty())
        return JNI_FALSE;
    const bool ok = FileMcd_CreateNewCard(name,
        static_cast<MemoryCardType>(p_type),
        static_cast<MemoryCardFileType>(p_fileType));
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_isMemoryCard(JNIEnv *env, jclass clazz, jstring p_name) {
    const std::string name = GetJavaString(env, p_name);
    return (!name.empty() && FileMcd_GetCardInfo(name).has_value()) ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setNominalSpeed(JNIEnv *env, jclass clazz,
                                                     jint p_percent) {
    // Custom speed / FPS cap. Mirrors speedhackLimitermode's direct-apply
    // pattern: the "Framerate/NominalScalar" base-setting write (from
    // Settings.applyTo) persists the value for cold starts, but the live VM's
    // frame pacer only re-reads it on UpdateTargetSpeed — so push it straight
    // into EmuConfig and re-pace here. Without this, dragging the Speed Limit
    // slider in-game did nothing (the string round-trip through ApplySettings
    // didn't re-pace reliably). Clamp matches EmulationSpeedOptions::SanityCheck.
    float scalar = std::clamp(static_cast<float>(p_percent) / 100.0f, 0.05f, 10.0f);
    // RetroAchievements hardcore forbids slowdown. This direct-apply path skips
    // VMManager::ApplySettings (so EnforceAchievementsChallengeModeSettings,
    // which clamps NominalScalar to >=1.0, never runs) — enforce it here.
    // Fast-forward (>1.0) stays allowed.
    if (Achievements::IsHardcoreModeActive() && scalar < 1.0f)
        scalar = 1.0f;
    Host::SetBaseFloatSettingValue("Framerate", "NominalScalar", scalar);
    EmuConfig.EmulationSpeed.NominalScalar = scalar;
    if (VMManager::HasValidVM())
        VMManager::UpdateTargetSpeed();
    Console.WriteLnFmt("@@ANDROID_SPEED@@ nominal_scalar={}", scalar);
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setFpsCap(JNIEnv *env, jclass clazz,
                                               jint p_fps) {
    // Max presented-FPS cap. INDEPENDENT of the Speed Limit % — it caps the
    // DISPLAY frame rate by dropping presents on the GS thread (GSRenderer::VSync)
    // while emulation keeps running full speed. It never touches NominalScalar,
    // so it does not slow the game and does not fight the Speed Limit %. The cap
    // is adaptive (drops only when ahead of the target interval), so a game
    // already at/below the target is unaffected — no over-skip. 0 = off.
    //
    // No RetroAchievements guard needed: capping the present rate is not a
    // slowdown (game logic still advances in real time), so hardcore is fine.
    const u32 fps = (p_fps > 0) ? static_cast<u32>(std::min(p_fps, 1000)) : 0u;
    u64 interval = 0;
    if (fps > 0)
    {
        // Arbitrary present-rate cap: present at most once per (1/fps) seconds. The
        // GS-thread accumulator pacer (GSRenderer::VSync) holds this average rate
        // for ANY target (e.g. 47/55 for per-game golden-spot tuning), not just
        // whole divisions of the source. Capping at/above the game's own rate
        // can't drop frames (the source produces no more), so treat that as off.
        const double native = static_cast<double>(VMManager::GetFrameRate()); // ~59.94 / 50
        if (static_cast<double>(fps) < native - 0.5)
            interval = static_cast<u64>(static_cast<double>(GetTickFrequency()) / static_cast<double>(fps));
        // else interval stays 0 → off (no effective cap)
    }
    GSSetMaxPresentFps(fps, interval);
    Console.WriteLnFmt("@@ANDROID_FPSCAP@@ fps={} interval_ticks={}", fps, interval);
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setFrameSkip(JNIEnv *env, jclass clazz,
                                                  jint p_skip) {
    // Manual frameskip for low-end devices: present 1 of every (skip+1) frames.
    // Applied live on the GS thread via GSRenderer::VSync; emulation still runs
    // every frame (this is a present/GPU-side skip, not an emulation skip).
    const u32 skip = static_cast<u32>(std::clamp(static_cast<int>(p_skip), 0, 5));
    GSSetManualFrameSkip(skip);
    Console.WriteLnFmt("@@ANDROID_FRAMESKIP@@ frames={}", skip);
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setAudioVolume(JNIEnv *env, jclass clazz,
                                                    jint p_volume) {
    // SPU2 output volume (percent). Persist to the base layer for cold starts,
    // mirror into EmuConfig so a later ApplySettings diff doesn't fight it, and
    // push live to the open audio stream (no-op when none is open).
    const int vol = std::clamp(static_cast<int>(p_volume), 0, 200);
    Host::SetBaseIntSettingValue("SPU2/Output", "StandardVolume", vol);
    EmuConfig.SPU2.StandardVolume = vol;
    SPU2::SetOutputVolume(static_cast<u32>(vol));
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setAudioMuted(JNIEnv *env, jclass clazz,
                                                   jboolean p_muted) {
    const bool muted = (p_muted == JNI_TRUE);
    // Update EmuConfig BEFORE SetOutputMuted: its unmute path refuses to unmute
    // while EmuConfig.SPU2.OutputMuted is still true.
    EmuConfig.SPU2.OutputMuted = muted;
    Host::SetBaseBoolSettingValue("SPU2/Output", "OutputMuted", muted);
    SPU2::SetOutputMuted(muted);
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setAudioSwapChannels(JNIEnv *env, jclass clazz,
                                                          jboolean p_swap) {
    // Swap the final stereo output channels (L<->R). Persist to the base layer so it
    // survives cold starts, and push live to the running mixer (applied next sample).
    const bool swap = (p_swap == JNI_TRUE);
    Host::SetBaseBoolSettingValue("SPU2/Output", "SwapChannels", swap);
    SPU2::SetSwapChannels(swap);
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_speedhackEecyclerate(JNIEnv *env, jclass clazz,
                                                          jint p_value) {
    const int value = std::clamp(static_cast<int>(p_value), -3, 3);
    Host::SetBaseIntSettingValue("EmuCore/Speedhacks", "EECycleRate", value);
    EmuConfig.Speedhacks.EECycleRate = value;
    if (VMManager::HasValidVM())
        VMManager::ApplySettings();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_speedhackEecycleskip(JNIEnv *env, jclass clazz,
                                                          jint p_value) {
    const int value = std::clamp(static_cast<int>(p_value), 0, 3);
    Host::SetBaseIntSettingValue("EmuCore/Speedhacks", "EECycleSkip", value);
    EmuConfig.Speedhacks.EECycleSkip = value;
    if (VMManager::HasValidVM())
        VMManager::ApplySettings();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setInstantVU1(JNIEnv*, jclass, jboolean enabled) {
    const bool value = (enabled == JNI_TRUE);
    Host::SetBaseBoolSettingValue("EmuCore/Speedhacks", "vu1Instant", value);
    EmuConfig.Speedhacks.vu1Instant = value;
    Console.WriteLnFmt("@@ANDROID_SPEEDHACK@@ vu1Instant={}", value ? 1 : 0);
}

// Savestate save/load and live GS reconfiguration mutate VM state that the
// EE/MTVU/MTGS pipeline reads concurrently, and upstream only ever runs them
// on the CPU thread. Our JNI entry points run on UI / Dispatchers.IO threads
// and used to rely on the caller having paused the VM first — which not every
// UI flow guaranteed. A load racing a RUNNING VM corrupts it mid-overwrite:
// the EE rec executes blocks against half-loaded RAM, MTVU keeps VIF-unpacking
// while dVifReset clears the hash buckets, and the SPU2 mixer reads voice
// state mid-thaw (three distinct observed SIGSEGVs). Live upscale changes had
// the same class of bug: GSUpdateConfig reconfigured the renderer while MTVU
// was mid-XGKICK, corrupting the GIF path (GoW2: "GS packet size exceeded VU
// memory size!" storms, then random EE/MTVU SIGSEGVs).
//
// This guard pauses the VM, then waits for the CPU thread to actually park
// outside Cpu->Execute() (runVMThread flips s_execute_exit between Execute()
// calls). The destructor restores the previous run state. If the CPU thread
// fails to park, parked() stays false and the caller must skip the state op.
class ScopedVMPause {
public:
    // pause_audio=false keeps the SPU2 output device running across the park.
    // Used by the settings-apply paths (commitSettings / live GS): a heavy
    // gamefix can park the VM for seconds, and pausing the low-latency Android
    // audio stream that long let the OS reclaim it — audio then stayed muted
    // until a manual menu resume. The CPU/MTGS/MTVU threads are still parked
    // for the JIT/GS rebuild; only the audio pause edges are suppressed, and
    // the stream emits silence on underrun so there's no audible artifact.
    explicit ScopedVMPause(bool pause_audio = true) {
        m_was_running = (VMManager::GetState() == VMState::Running);
        m_was_paused = (VMManager::GetState() == VMState::Paused);
        if (m_was_running)
        {
            // Set BEFORE SetPaused(true) so the pause edge is suppressed; the
            // dtor clears it AFTER SetPaused(false) so the resume edge is too.
            if (!pause_audio)
            {
                m_audio_pause_suppressed = true;
                SPU2::SetOutputPauseSuppressed(true);
            }
            VMManager::SetPaused(true);
            if (!s_execute_exit.load(std::memory_order_acquire) && Cpu)
                Cpu->ExitExecution();
        }
        // A healthy VM exits Execute() within a frame of the state flip;
        // allow a generous 3s before declaring failure.
        for (int i = 0; i < 3000 && !s_execute_exit.load(std::memory_order_acquire); ++i)
            usleep(1000);
        m_parked = s_execute_exit.load(std::memory_order_acquire) || m_was_paused;
    }
    ~ScopedVMPause() {
        if (m_was_running && !s_stop_requested.load(std::memory_order_acquire))
            VMManager::SetPaused(false);
        if (m_audio_pause_suppressed)
            SPU2::SetOutputPauseSuppressed(false);
    }
    ScopedVMPause(const ScopedVMPause&) = delete;
    ScopedVMPause& operator=(const ScopedVMPause&) = delete;

    bool parked() const { return m_parked; }

private:
    bool m_was_running = false;
    bool m_was_paused = false;
    bool m_parked = false;
    bool m_audio_pause_suppressed = false;
};

static void LogAndroidGSSettings(const char* reason)
{
    Console.WriteLnFmt(
        "@@ANDROID_GS_SETTINGS@@ reason={} renderer={} ir={:.2f} "
        "mipmap={} blend={} filter={} preloading={} tv={} shade={} "
        "sb={}/{}/{}/{} userhacks={} af={} tri={} hpo={} atfl={} "
        "limit24={} texrt={} native_scaling={} bilinear={}",
        reason,
        static_cast<int>(EmuConfig.GS.Renderer),
        EmuConfig.GS.UpscaleMultiplier,
        +EmuConfig.GS.HWMipmap,
        static_cast<int>(EmuConfig.GS.AccurateBlendingUnit),
        static_cast<int>(EmuConfig.GS.TextureFiltering),
        static_cast<int>(EmuConfig.GS.TexturePreloading),
        +EmuConfig.GS.TVShader,
        +EmuConfig.GS.ShadeBoost,
        +EmuConfig.GS.ShadeBoost_Brightness,
        +EmuConfig.GS.ShadeBoost_Contrast,
        +EmuConfig.GS.ShadeBoost_Saturation,
        +EmuConfig.GS.ShadeBoost_Gamma,
        +EmuConfig.GS.ManualUserHacks,
        static_cast<unsigned>(EmuConfig.GS.MaxAnisotropy),
        static_cast<int>(EmuConfig.GS.TriFilter),
        static_cast<int>(EmuConfig.GS.UserHacks_HalfPixelOffset),
        static_cast<int>(EmuConfig.GS.UserHacks_AutoFlush),
        static_cast<int>(EmuConfig.GS.UserHacks_Limit24BitDepth),
        static_cast<int>(EmuConfig.GS.UserHacks_TextureInsideRt),
        static_cast<int>(EmuConfig.GS.UserHacks_NativeScaling),
        static_cast<int>(EmuConfig.GS.UserHacks_BilinearHack));
}

static bool ApplyLiveGSSettingsIfOpen(const char* reason)
{
    if (MTGS::IsOpen())
    {
        // pause_audio=false: keep the audio device alive across the park so a
        // live GS reconfigure can't mute audio (see ScopedVMPause).
        ScopedVMPause vm_pause(/*pause_audio=*/false);
        if (!vm_pause.parked())
        {
            Console.WriteLnFmt("@@ANDROID_GS_SETTINGS@@ reason={} skipped=cpu_not_parked", reason);
            return false;
        }
        MTGS::ApplySettings();
    }

    LogAndroidGSSettings(reason);
    return true;
}

// Generic setting writer — mirror of pcsx2-qt's settings save path.
// Writes flow into s_settings_interface (the MemorySettingsInterface
// installed in initialize); commitSettings flushes them through to the
// VM. Type comes as a string from Java to keep the JNI surface flat —
// only four primitives are supported (bool/int/float/string), enough
// for every EmuCore key the UI needs to push.
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setSetting(JNIEnv *env, jclass clazz,
                                                 jstring p_section, jstring p_key,
                                                 jstring p_type, jstring p_value) {
    const std::string section = GetJavaString(env, p_section);
    const std::string key     = GetJavaString(env, p_key);
    const std::string type    = GetJavaString(env, p_type);
    const std::string value   = GetJavaString(env, p_value);

    // Project builds with -fno-exceptions, so std::stoi / std::stof can't
    // be wrapped in try-catch. Use StringUtil::FromChars (the same parser
    // MemorySettingsInterface uses internally for GetIntValue/GetFloatValue
    // — guarantees parse-symmetry with whatever we write here).
    if (type == "bool")
    {
        const bool bval = (value == "true" || value == "1");
        Host::SetBaseBoolSettingValue(section.c_str(), key.c_str(), bval);
    }
    else if (type == "int")
    {
        if (auto parsed = StringUtil::FromChars<s32>(value, 10); parsed.has_value())
            Host::SetBaseIntSettingValue(section.c_str(), key.c_str(), parsed.value());
    }
    else if (type == "float")
    {
        if (auto parsed = StringUtil::FromChars<float>(value); parsed.has_value())
            Host::SetBaseFloatSettingValue(section.c_str(), key.c_str(), parsed.value());
    }
    else if (type == "string")
    {
        Host::SetBaseStringSettingValue(section.c_str(), key.c_str(), value.c_str());
    }
    else
    {
        Console.Warning("setSetting: unknown type '%s' for %s/%s", type.c_str(),
                        section.c_str(), key.c_str());
    }
}

// Push queued setSetting writes into the running VM. Idempotent — safe
// to call multiple times. Logs the resolved EmuCore.Speedhacks state
// for plumbing-verification (one-line check from logcat).
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_commitSettings(JNIEnv *env, jclass clazz) {
    if (VMManager::HasValidVM()) {
        // ApplySettings mutates state the EE/MTVU/MTGS pipeline reads
        // concurrently (JIT cache flushes, GS reconfig), so it must not race
        // a RUNNING VM. The pause overlay used to guarantee that by pausing
        // synchronously on the UI thread before any settings write could be
        // sent; pause is now dispatched to a background executor (it can
        // block for seconds when MTVU/MTGS drain slowly), so enforce
        // quiescence here instead of trusting caller ordering. Near-zero
        // cost when the VM is already parked. Skipped entirely pre-VM:
        // s_execute_exit is false before the first Execute(), so the guard
        // would spin its full 3s watchdog during setup-wizard commits.
        // pause_audio=false: a heavy gamefix can park the VM for seconds;
        // pausing the audio device that long lets Android reclaim it and the
        // game goes silent until a manual menu resume. Keep it running (it
        // fills with silence on underrun) while the JIT/GS caches rebuild.
        ScopedVMPause vm_pause(/*pause_audio=*/false);
        VMManager::ApplySettings();
        if (MTGS::IsOpen())
            MTGS::ApplySettings();
    } else if (MTGS::IsOpen()) {
        MTGS::ApplySettings();
    }
    if (s_settings_interface && s_settings_interface->IsDirty())
        s_settings_interface->Save();
    LogAndroidGSSettings("commit");

    // Plumbing roundtrip verifier — once the UI starts pushing real
    // settings, watch logcat for these to confirm the write landed.
    // Unary `+` promotes each bit-field to a plain int, since C++ doesn't
    // allow forwarding references (T&&) to bind to bit-fields.
    Console.WriteLnFmt(
        "Settings commit: vuThread={} EECycleRate={} EECycleSkip={} "
        "vu1Instant={} fastCDVD={} vuFlagHack={}",
        +EmuConfig.Speedhacks.vuThread,
        +EmuConfig.Speedhacks.EECycleRate,
        +EmuConfig.Speedhacks.EECycleSkip,
        +EmuConfig.Speedhacks.vu1Instant,
        +EmuConfig.Speedhacks.fastCDVD,
        +EmuConfig.Speedhacks.vuFlagHack);
}

// Generic live GS reconfigure. Reloads the whole EmuCore/GS section from the
// base settings layer into EmuConfig.GS, re-applies the user-hack masks the
// core applies on load, then pushes the change to the GS thread — WITHOUT the
// full VMManager::ApplySettings() CPU/JIT rebuild that commitSettings() does
// (that heavy path is what caused ANRs when settings were scrubbed live). This
// lets every renderer / hardware-fix / upscaling-fix setting apply mid-game,
// the same way the desktop pause menu's GS settings do. The UI writes the
// changed keys via setSetting() first, then calls this.
extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_applyGSSettingsLive(JNIEnv *env, jclass clazz) {
    // CRITICAL (Android stability): reloading the WHOLE EmuCore/GS section pulls
    // the device-identity fields (Renderer/Adapter/…) from the base layer too —
    // e.g. base "Auto" vs the already-resolved OpenGL the VM is running on. Any
    // mismatch makes GSUpdateConfig take the full device teardown/recreate path
    // (GSreopen(true,true), gated by GSOptions::RestartOptionsAreEqual), which is
    // the one GS operation that crashes mid-game here. The narrow live setters
    // (renderTvShader, …) are safe precisely because they never touch these.
    // So snapshot the live device fields, reload, then restore them — only the
    // safe in-place render / hardware-fix / upscaling-fix options end up changing.
    const auto saved_renderer        = EmuConfig.GS.Renderer;
    const auto saved_adapter         = EmuConfig.GS.Adapter;
    const auto saved_debug_device    = EmuConfig.GS.UseDebugDevice;
    const auto saved_blit_swap       = EmuConfig.GS.UseBlitSwapChain;
    const auto saved_no_shader_cache = EmuConfig.GS.DisableShaderCache;
    const auto saved_no_fb_fetch     = EmuConfig.GS.DisableFramebufferFetch;
    const auto saved_adreno_fbfetch  = EmuConfig.GS.EnableAdrenoFramebufferFetch;
    const auto saved_mali_fbfetch    = EmuConfig.GS.ForceMaliFramebufferFetch;
    const auto saved_no_vs_expand    = EmuConfig.GS.DisableVertexShaderExpand;
    const auto saved_tex_barriers    = EmuConfig.GS.OverrideTextureBarriers;
    const auto saved_depth_feedback  = EmuConfig.GS.DepthFeedbackMode;
    const auto saved_back_thread     = EmuConfig.GS.BackThreadMode;
    const auto saved_hwaa1           = EmuConfig.GS.HWAA1;
    const auto saved_exclusive_fs    = EmuConfig.GS.ExclusiveFullscreenControl;
    const auto saved_sw_threads      = EmuConfig.GS.SWExtraThreads;
    const auto saved_sw_threads_h    = EmuConfig.GS.SWExtraThreadsHeight;

    {
        auto lock = Host::GetSettingsLock();
        SettingsInterface* si = Host::GetSettingsInterface();
        if (!si)
            return JNI_FALSE;
        SettingsLoadWrapper slw(*si);
        EmuConfig.GS.LoadSave(slw);
    }

    // Restore everything RestartOptionsAreEqual() compares (+ the SW-thread quick-
    // reopen pair) so a live apply can NEVER trigger a device/renderer recreate.
    EmuConfig.GS.Renderer                   = saved_renderer;
    EmuConfig.GS.Adapter                    = saved_adapter;
    EmuConfig.GS.UseDebugDevice             = saved_debug_device;
    EmuConfig.GS.UseBlitSwapChain           = saved_blit_swap;
    EmuConfig.GS.DisableShaderCache         = saved_no_shader_cache;
    EmuConfig.GS.DisableFramebufferFetch    = saved_no_fb_fetch;
    EmuConfig.GS.EnableAdrenoFramebufferFetch = saved_adreno_fbfetch;
    EmuConfig.GS.ForceMaliFramebufferFetch  = saved_mali_fbfetch;
    EmuConfig.GS.DisableVertexShaderExpand  = saved_no_vs_expand;
    EmuConfig.GS.OverrideTextureBarriers    = saved_tex_barriers;
    EmuConfig.GS.DepthFeedbackMode          = saved_depth_feedback;
    EmuConfig.GS.BackThreadMode             = saved_back_thread;
    EmuConfig.GS.HWAA1                       = saved_hwaa1;
    EmuConfig.GS.ExclusiveFullscreenControl = saved_exclusive_fs;
    EmuConfig.GS.SWExtraThreads             = saved_sw_threads;
    EmuConfig.GS.SWExtraThreadsHeight       = saved_sw_threads_h;

    // Mirror VMManager::LoadCoreSettings: strip user/upscaling hacks when their
    // master toggles are off so stale keys can't leak through into the renderer.
    EmuConfig.GS.MaskUserHacks();
    EmuConfig.GS.MaskUpscalingHacks();

    // Re-apply the active game's GameDB GS hardware fixes. LoadSave above only
    // restored the user/base layer; per-game fixes (e.g. True Crime's
    // textureInsideRT) apply on TOP of it in VMManager::ApplyGameFixes. Without
    // this, a live GS settings change would wipe them and the game would break
    // until the next launch. Mirrors ApplyGameFixes' GS portion.
    if (const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(VMManager::GetDiscSerial()))
    {
        game->applyGSHardwareFixes(EmuConfig.GS);
        EmuConfig.GS.MaskUpscalingHacks();
    }
    return ApplyLiveGSSettingsIfOpen("ui_render_live") ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jint JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_reloadPatches(JNIEnv *env, jclass clazz) {
    if (!VMManager::HasValidVM())
        return static_cast<jint>(Patch::GetActiveCheatsCount());

    ScopedVMPause vm_pause;
    if (!vm_pause.parked())
    {
        Console.WriteLn("@@ANDROID_PNACH@@ reload skipped: cpu_not_parked");
        return -1;
    }

    VMManager::ReloadPatches(true, true, true, true);
    const u32 active_cheats = Patch::GetActiveCheatsCount();
    Console.WriteLnFmt("@@ANDROID_PNACH@@ reload active_cheats={}", active_cheats);
    return static_cast<jint>(active_cheats);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_reloadTextureReplacements(JNIEnv *env, jclass clazz) {
    if (!MTGS::IsOpen())
        return JNI_FALSE;
    MTGS::RunOnGSThread([]() {
        if (!g_gs_renderer)
            return;
        GSTextureReplacements::ReloadReplacementMap();
        g_gs_renderer->PurgeTextureCache(true, false, true);
    });
    return JNI_TRUE;
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_applyFramerateLive(JNIEnv *env, jclass clazz,
                                                        jfloat p_ntsc, jfloat p_pal) {
    // Per-region NTSC/PAL emulated vsync rate, applied LIVE (no restart) so the
    // Frame Rate sliders behave like NetherSX2. The pacer caches the rate in
    // vSyncInfo, so a plain EmuConfig write does nothing until UpdateVSyncRate
    // recomputes it — mirroring VMManager::CheckForGSConfigChanges' framerate
    // branch (UpdateVSyncRate + UpdateTargetSpeed).
    //
    // CRITICAL: UpdateVSyncRate rewrites the EE-thread hsync/vsync counters and
    // calls cpuRcntSet(), so it MUST run with the CPU/MTGS/MTVU threads parked —
    // a raw JNI-thread call would race the emulation loop. ScopedVMPause(false)
    // parks them but keeps the audio stream alive (avoids the low-latency-stream
    // reclaim that muted audio on longer parks). Invoked off the UI thread via
    // LiveGsApplyQueue, which also coalesces rapid slider drags.
    if (!VMManager::HasValidVM())
        return;
    ScopedVMPause vm_pause(false);
    if (!vm_pause.parked())
        return;
    EmuConfig.GS.FramerateNTSC = static_cast<float>(p_ntsc);
    EmuConfig.GS.FrameratePAL = static_cast<float>(p_pal);
    UpdateVSyncRate(true);
    VMManager::UpdateTargetSpeed();
    Console.WriteLnFmt("@@ANDROID_FRAMERATE@@ ntsc={} pal={}",
        EmuConfig.GS.FramerateNTSC, EmuConfig.GS.FrameratePAL);
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_enablePad2(JNIEnv *env, jclass clazz) {
    // Local co-op: hot-plug a second DualShock2 into PS2 port 2 the moment a 2nd
    // physical controller joins (PadRouter). By default GetDefaultPadType makes only
    // port 0 a DualShock2, so until this runs SetControllerState(1,...) lands on the
    // (valid, non-null) PadNotConnected slot 1 — a safe no-op. Persist [Pad2] to the
    // base layer so a later ApplySettings keeps it, set the live EmuConfig, then
    // Pad::LoadConfig rebuilds the pads (with eject ticks so the running game detects
    // the insertion).
    //
    // Threading: Pad::LoadConfig reassigns s_controllers[] (make_unique) and is read
    // by BOTH the EE/SIO (CPU) thread AND the Android input thread. ScopedVMPause
    // parks the CPU/MTGS/MTVU side; s_pad_mutex serializes the input side (applyPadButton).
    // Both are required to avoid a use-after-free on the replaced controller. Runs on a
    // background thread (see Main.onPlayer2Joined) so the input thread isn't blocked by
    // the up-to-3s park wait.
    if (!VMManager::HasValidVM())
        return;
    if (EmuConfig.Pad.Ports[1].Type == Pad::ControllerType::DualShock2)
        return; // already connected
    ScopedVMPause vm_pause(false);
    if (!vm_pause.parked())
        return;
    {
        auto lock = Host::GetSettingsLock();
        if (SettingsInterface* si = Host::GetSettingsInterface()) {
            si->SetStringValue("Pad2", "Type", "DualShock2");
            si->SetFloatValue("Pad2", "Deadzone", 0.0f);        // app shapes the stick (shapeStickMag)
            si->SetFloatValue("Pad2", "AxisScale", 1.33f);      // PCSX2 default
            si->SetFloatValue("Pad2", "ButtonDeadzone", 0.0f);
        }
    }
    {
        std::lock_guard<std::mutex> lk(s_pad_mutex);
        EmuConfig.Pad.Ports[1].Type = Pad::ControllerType::DualShock2;
        Pad::LoadConfig(*Host::GetSettingsInterface());
    }
    Console.WriteLn("@@ANDROID_COOP@@ Pad2 enabled (DualShock2)");
}

// PS2 Multitap: enable/disable the 3 extra "tap" slots on one physical PS2 port so a
// game can see up to 4 pads per port (8 total). Unified-slot layout (Sio.h): port 0 taps
// = unified slots 2,3,4 ([Pad3..Pad5]); port 1 taps = slots 5,6,7 ([Pad6..Pad8]). The
// on-disk multitap flag keys are OFF-BY-ONE: [Pad] "MultitapPort1" -> engine
// MultitapPort0_Enabled (physical port 0), "MultitapPort2" -> port 1. A tap only goes
// live when BOTH the flag is set AND Ports[slot].Type == DualShock2 (Pad::LoadConfig
// forces NotConnected otherwise). Sio2 reads the multitap flag + Pad::GetPad(port,slot)
// live every poll, so no SIO re-init is needed — Pad::LoadConfig sends eject ticks and
// the running game re-detects. Threading mirrors enablePad2 exactly: ScopedVMPause parks
// the CPU/MTGS/MTVU side and s_pad_mutex serializes the input thread against the
// s_controllers[] rebuild. MUST be called off the UI thread (the park can take up to 3s).
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setMultitap(JNIEnv *env, jclass clazz, jint p_port, jboolean p_enabled) {
    if (!VMManager::HasValidVM())
        return;
    const u32 port = (p_port <= 0) ? 0u : 1u;
    const bool enabled = (p_enabled == JNI_TRUE);
    const char* flagKey = (port == 0) ? "MultitapPort1" : "MultitapPort2";
    u32 taps[3];
    if (port == 0) { taps[0] = 2u; taps[1] = 3u; taps[2] = 4u; }
    else           { taps[0] = 5u; taps[1] = 6u; taps[2] = 7u; }

    ScopedVMPause vm_pause(false);
    if (!vm_pause.parked())
        return;
    {
        auto lock = Host::GetSettingsLock();
        if (SettingsInterface* si = Host::GetSettingsInterface()) {
            si->SetBoolValue("Pad", flagKey, enabled);
            for (int k = 0; k < 3; k++) {
                const std::string section = Pad::GetConfigSection(taps[k]); // [Pad3..Pad8]
                si->SetStringValue(section.c_str(), "Type", enabled ? "DualShock2" : "None");
                si->SetFloatValue(section.c_str(), "Deadzone", 0.0f);       // app shapes the stick
                si->SetFloatValue(section.c_str(), "AxisScale", 1.33f);     // PCSX2 default
                si->SetFloatValue(section.c_str(), "ButtonDeadzone", 0.0f);
            }
        }
    }
    {
        std::lock_guard<std::mutex> lk(s_pad_mutex);
        if (port == 0)
            EmuConfig.Pad.MultitapPort0_Enabled = enabled;
        else
            EmuConfig.Pad.MultitapPort1_Enabled = enabled;
        // Only ever touch the 3 tap slots for THIS port — never Ports[0]/Ports[1]
        // (the port mains) or we'd eject P1/P2 mid-game.
        for (int k = 0; k < 3; k++)
            EmuConfig.Pad.Ports[taps[k]].Type = enabled ? Pad::ControllerType::DualShock2
                                                        : Pad::ControllerType::NotConnected;
        Pad::LoadConfig(*Host::GetSettingsInterface()); // eject ticks -> running game re-detects
    }
    Console.WriteLn("@@ANDROID_MULTITAP@@ port=%u enabled=%d", port, (int)enabled);
}

// jobjectArray<String> -> std::vector<std::string>.
static std::vector<std::string> jStringArrayToVector(JNIEnv* env, jobjectArray arr) {
    std::vector<std::string> out;
    if (!arr)
        return out;
    const jsize n = env->GetArrayLength(arr);
    out.reserve(static_cast<size_t>(n));
    for (jsize i = 0; i < n; i++) {
        jstring s = static_cast<jstring>(env->GetObjectArrayElement(arr, i));
        out.push_back(GetJavaString(env, s));
        if (s)
            env->DeleteLocalRef(s);
    }
    return out;
}

// Set which named patches/cheats are ENABLED. PCSX2 only applies a patch whose
// name is in the base [Patches]/[Cheats] "Enable" string list (Patch.cpp reads
// it via Host::GetStringListSetting); writing the .pnach file alone does nothing.
// The browser passes ALL of the current game's entry names plus the user's
// selected subset: drop the game's names from the list then re-add the selected
// ones (exact per-game state without disturbing other games), and Save so it
// persists across reset/relaunch. Call reloadPatches() afterward to apply.
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setEnabledPatches(
    JNIEnv* env, jclass, jboolean cheats, jobjectArray allNames, jobjectArray enabledNames) {
    const std::vector<std::string> all = jStringArrayToVector(env, allNames);
    const std::vector<std::string> enabled = jStringArrayToVector(env, enabledNames);
    const char* section = (cheats == JNI_TRUE) ? "Cheats" : "Patches";

    auto lock = Host::GetSettingsLock();
    SettingsInterface* si = Host::Internal::GetBaseSettingsLayer();
    if (!si)
        return;
    for (const auto& n : all)
        si->RemoveFromStringList(section, "Enable", n.c_str());
    for (const auto& n : enabled)
        si->AddToStringList(section, "Enable", n.c_str());
    si->Save();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderUpscalemultiplier(JNIEnv *env, jclass clazz,
                                                             jfloat p_value) {
    // VMManager::ApplySettings (called inside Initialize) resets EmuConfig
    // from the persistent SettingsInterface, so writing only to EmuConfig
    // pre-launch gets clobbered. Push to the BASE layer so LoadCoreSettings
    // picks it up. Also update EmuConfig directly + nudge MTGS so a live
    // VM picks up the change without a settings file save round-trip.
    Host::SetBaseFloatSettingValue("EmuCore/GS", "upscale_multiplier", p_value);
    EmuConfig.GS.UpscaleMultiplier = p_value;
    ApplyLiveGSSettingsIfOpen("upscale");
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderMipmap(JNIEnv *env, jclass clazz,
                                                  jint p_value) {
    const bool enabled = (p_value != 0);
    Host::SetBaseBoolSettingValue("EmuCore/GS", "hw_mipmap", enabled);
    EmuConfig.GS.HWMipmap = enabled;
    ApplyLiveGSSettingsIfOpen("hw_mipmap");
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderHalfpixeloffset(JNIEnv *env, jclass clazz,
                                                           jint p_value) {
    const int value = std::clamp(static_cast<int>(p_value), 0,
        static_cast<int>(GSHalfPixelOffset::MaxCount) - 1);
    Host::SetBaseIntSettingValue("EmuCore/GS", "UserHacks_HalfPixelOffset", value);
    EmuConfig.GS.UserHacks_HalfPixelOffset = static_cast<GSHalfPixelOffset>(value);
    ApplyLiveGSSettingsIfOpen("half_pixel_offset");
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderTvShader(JNIEnv *env, jclass clazz,
                                                    jint p_value) {
    const int value = std::clamp(static_cast<int>(p_value), 0, 7);
    Host::SetBaseIntSettingValue("EmuCore/GS", "TVShader", value);
    EmuConfig.GS.TVShader = static_cast<u8>(value);
    ApplyLiveGSSettingsIfOpen("tv_shader");
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderShadeBoost(JNIEnv *env, jclass clazz,
                                                      jboolean p_enabled,
                                                      jint p_brightness,
                                                      jint p_contrast,
                                                      jint p_saturation,
                                                      jint p_gamma) {
    const bool enabled = (p_enabled == JNI_TRUE);
    const int brightness = std::clamp(static_cast<int>(p_brightness), 1, 100);
    const int contrast = std::clamp(static_cast<int>(p_contrast), 1, 100);
    const int saturation = std::clamp(static_cast<int>(p_saturation), 1, 100);
    const int gamma = std::clamp(static_cast<int>(p_gamma), 1, 100);

    Host::SetBaseBoolSettingValue("EmuCore/GS", "ShadeBoost", enabled);
    Host::SetBaseIntSettingValue("EmuCore/GS", "ShadeBoost_Brightness", brightness);
    Host::SetBaseIntSettingValue("EmuCore/GS", "ShadeBoost_Contrast", contrast);
    Host::SetBaseIntSettingValue("EmuCore/GS", "ShadeBoost_Saturation", saturation);
    Host::SetBaseIntSettingValue("EmuCore/GS", "ShadeBoost_Gamma", gamma);

    EmuConfig.GS.ShadeBoost = enabled;
    EmuConfig.GS.ShadeBoost_Brightness = static_cast<u8>(brightness);
    EmuConfig.GS.ShadeBoost_Contrast = static_cast<u8>(contrast);
    EmuConfig.GS.ShadeBoost_Saturation = static_cast<u8>(saturation);
    EmuConfig.GS.ShadeBoost_Gamma = static_cast<u8>(gamma);
    ApplyLiveGSSettingsIfOpen("shadeboost");
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderPreloading(JNIEnv *env, jclass clazz,
                                                      jint p_value) {
    const int value = std::clamp(static_cast<int>(p_value), 0,
        static_cast<int>(TexturePreloadingLevel::Full));
    Host::SetBaseIntSettingValue("EmuCore/GS", "texture_preloading", value);
    EmuConfig.GS.TexturePreloading = static_cast<TexturePreloadingLevel>(value);
    ApplyLiveGSSettingsIfOpen("texture_preloading");
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderSoftware(JNIEnv *env, jclass clazz) {
    // Don't go through MTGS::ApplySettings → GSUpdateConfig → GSreopen(true,true,SW)
    // here: GSreopen(recreate_device=true, SW) calls GetAPIForRenderer(SW), which
    // falls to the default branch and asks GSUtil::GetPreferredRenderer for an API.
    // On Android that resolves to OpenGL — so going SW after picking Vulkan in the
    // wizard would silently rebuild GSDeviceOGL and the SW renderer would present
    // via GL, not VK.
    //
    // SetSoftwareRendering preserves the existing GSDevice (VK stays VK, OGL stays
    // OGL) and only swaps the renderer to SW. The picked backend remains the host
    // display device.
    //
    // Persist SW to the base layer too (like renderOpenGL/renderAuto) so the
    // choice survives a cold boot — VMManager::ApplySettings reloads Renderer
    // from the base SettingsInterface at VM init, so without this a selected
    // "Software" would silently boot back into hardware.
    Host::SetBaseIntSettingValue("EmuCore/GS", "Renderer",
        static_cast<int>(GSRendererType::SW));
    EmuConfig.GS.Renderer = GSRendererType::SW;
    if(MTGS::IsOpen()) {
        MTGS::SetSoftwareRendering(true, EmuConfig.GS.InterlaceMode, false);
    }
}

// Auto = let GSUtil::GetPreferredRenderer pick at runtime based on what
// the device supports (Vulkan when available, OpenGL otherwise, SW as
// last resort). Matches Pcsx2Config::DEFAULT_HW_RENDERER and is the
// fresh-install default — the in-game overlay's renderer cycle still
// allows explicit OPENGL/SW override on top.
extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderAuto(JNIEnv *env, jclass clazz) {
    Host::SetBaseIntSettingValue("EmuCore/GS", "Renderer",
        static_cast<int>(GSRendererType::Auto));
    EmuConfig.GS.Renderer = GSRendererType::Auto;
    if(MTGS::IsOpen()) {
        MTGS::ApplySettings();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderOpenGL(JNIEnv *env, jclass clazz) {
    Host::SetBaseIntSettingValue("EmuCore/GS", "Renderer",
        static_cast<int>(GSRendererType::OGL));
    EmuConfig.GS.Renderer = GSRendererType::OGL;
    if(MTGS::IsOpen()) {
        // In-game pill SW→HW: keep the existing OGL device, swap renderer to HW.
        // ApplySettings would do a full teardown which is fine here (same backend),
        // but SetSoftwareRendering is cheaper and matches the symmetric path used
        // by renderSoftware.
        MTGS::SetSoftwareRendering(false, EmuConfig.GS.InterlaceMode, false);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderVulkan(JNIEnv *env, jclass clazz) {
    // Selecting "Vulkan" in the setup wizard means the host display device
    // is GSDeviceVK. We set Renderer=VK so MTGS::Open creates that device;
    // a subsequent renderSoftware() call then flips Renderer=SW but keeps
    // GSDeviceVK as the display, which is how the in-game HW/SW pill cycles
    // inside the user's chosen backend.
    //
    // VK HW had a known blending regression (BIOS pillars / SCEA text get
    // black boxes when AccBlendLevel = Full) — was masked here by a coerce
    // to SW. The coerce is removed because (a) the user explicitly picked
    // Vulkan, (b) without it SW couldn't get the VK display backend, and
    // (c) the AccBlendLevel default in the wizard is Full and the in-game
    // overlay has the toggle if the user hits the regression.
    Host::SetBaseIntSettingValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::VK));
    EmuConfig.GS.Renderer = GSRendererType::VK;
    if(MTGS::IsOpen()) {
        // In-game pill SW→HW with Vulkan backend: keep the existing VK device,
        // swap renderer to HW.
        MTGS::SetSoftwareRendering(false, EmuConfig.GS.InterlaceMode, false);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_onNativeSurfaceCreated(JNIEnv *env, jclass clazz) {
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setDisplayRefreshRate(JNIEnv *env, jclass clazz, jfloat p_hz) {
    // Called from the surface layer before onNativeSurfaceChanged so the value is
    // in place when AcquireRenderWindow() reads it during window (re)acquisition.
    std::lock_guard<std::mutex> lock(s_window_mutex);
    s_window_refresh_rate = (p_hz > 1.0f) ? (float)p_hz : 0.0f;
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_onNativeSurfaceChanged(JNIEnv *env, jclass clazz,
                                                            jobject p_surface, jint p_width, jint p_height) {
    {
        std::lock_guard<std::mutex> lock(s_window_mutex);
        if(s_window) {
            ANativeWindow_release(s_window);
            s_window = nullptr;
        }
        if(p_surface != nullptr) {
            s_window = ANativeWindow_fromSurface(env, p_surface);
        }
        if(p_width > 0 && p_height > 0) {
            s_window_width = p_width;
            s_window_height = p_height;
        }
    }

    if(p_width > 0 && p_height > 0 && MTGS::IsOpen()) {
        MTGS::UpdateDisplayWindow();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_onNativeSurfaceDestroyed(JNIEnv *env, jclass clazz) {
    {
        std::lock_guard<std::mutex> lock(s_window_mutex);
        if(s_window) {
            ANativeWindow_release(s_window);
            s_window = nullptr;
        }
    }
    // Tear the swapchain down now rather than letting the GS thread keep
    // presenting into the dead window until a failed present forces a
    // recreate. AcquireRenderWindow reports Surfaceless while s_window is
    // null, so the recreate path skips swapchain creation cleanly. Async
    // post to the GS thread — safe from the UI thread.
    if(MTGS::IsOpen()) {
        MTGS::UpdateDisplayWindow();
    }
}


std::optional<WindowInfo> Host::AcquireRenderWindow(bool recreate_window)
{
    WindowInfo _windowInfo;
    memset(&_windowInfo, 0, sizeof(_windowInfo));

    std::lock_guard<std::mutex> lock(s_window_mutex);

    // Drop the previous acquisition's reference — at most one outstanding.
    if (s_acquired_window) {
        ANativeWindow_release(s_acquired_window);
        s_acquired_window = nullptr;
    }

    // The Android surface dies and is reborn across overlay opens / resizes /
    // backgrounding: onNativeSurfaceChanged releases s_window before creating
    // the replacement, and onNativeSurfaceDestroyed leaves it null. Report
    // Surfaceless in that window instead of Type::Android with a null handle —
    // GSDeviceVK::UpdateWindow/GSDeviceOGL handle Surfaceless by skipping
    // swapchain creation, while a null handle reaches vkCreateAndroidSurfaceKHR
    // and SIGSEGVs inside the loader (RefBase::incStrong on null+4). The next
    // onNativeSurfaceChanged triggers MTGS::UpdateDisplayWindow and we
    // re-acquire the real surface.
    if (!s_window) {
        _windowInfo.type = WindowInfo::Type::Surfaceless;
        return _windowInfo;
    }

    // Take our own reference so the UI thread releasing its reference (next
    // surfaceChanged/Destroyed) can't free the window out from under the GS
    // thread. Surface creation on an abandoned-but-live window fails cleanly.
    ANativeWindow_acquire(s_window);
    s_acquired_window = s_window;

    float _fScale = 1.0;
    if (s_window_width > 0 && s_window_height > 0) {
        int _nSize = s_window_width;
        if (s_window_width <= s_window_height) {
            _nSize = s_window_height;
        }
        _fScale = (float)_nSize / 800.0f;
    }
    ////
    _windowInfo.type = WindowInfo::Type::Android;
    _windowInfo.surface_width = s_window_width;
    _windowInfo.surface_height = s_window_height;
    _windowInfo.surface_scale = _fScale;
    _windowInfo.surface_refresh_rate = s_window_refresh_rate;
    _windowInfo.window_handle = s_window;

    return _windowInfo;
}

void Host::ReleaseRenderWindow() {
    std::lock_guard<std::mutex> lock(s_window_mutex);
    if (s_acquired_window) {
        ANativeWindow_release(s_acquired_window);
        s_acquired_window = nullptr;
    }
}

static s32 s_loop_count = 1;

// Owned by the GS thread.
static u32 s_dump_frame_number = 0;
static u32 s_loop_number = s_loop_count;
static double s_last_internal_draws = 0;
static double s_last_draws = 0;
static double s_last_render_passes = 0;
static double s_last_barriers = 0;
static double s_last_copies = 0;
static double s_last_uploads = 0;
static double s_last_readbacks = 0;
static u64 s_total_internal_draws = 0;
static u64 s_total_draws = 0;
static u64 s_total_render_passes = 0;
static u64 s_total_barriers = 0;
static u64 s_total_copies = 0;
static u64 s_total_uploads = 0;
static u64 s_total_readbacks = 0;
static u32 s_total_frames = 0;
static u32 s_total_drawn_frames = 0;

void Host::BeginPresentFrame() {
    if (GSIsHardwareRenderer())
    {
        const u32 last_draws = s_total_internal_draws;
        const u32 last_uploads = s_total_uploads;

        static constexpr auto update_stat = [](GSPerfMon::counter_t counter, u64& dst, double& last) {
            // perfmon resets every 30 frames to zero
            const double val = g_perfmon.GetCounter(counter);
            dst += static_cast<u64>((val < last) ? val : (val - last));
            last = val;
        };

        update_stat(GSPerfMon::Draw, s_total_internal_draws, s_last_internal_draws);
        update_stat(GSPerfMon::DrawCalls, s_total_draws, s_last_draws);
        update_stat(GSPerfMon::RenderPasses, s_total_render_passes, s_last_render_passes);
        update_stat(GSPerfMon::Barriers, s_total_barriers, s_last_barriers);
        update_stat(GSPerfMon::TextureCopies, s_total_copies, s_last_copies);
        update_stat(GSPerfMon::TextureUploads, s_total_uploads, s_last_uploads);
        update_stat(GSPerfMon::Readbacks, s_total_readbacks, s_last_readbacks);

        const bool idle_frame = s_total_frames && (last_draws == s_total_internal_draws && last_uploads == s_total_uploads);

        if (!idle_frame)
            s_total_drawn_frames++;

        s_total_frames++;

        std::atomic_thread_fence(std::memory_order_release);
    }
}

void Host::OnGameChanged(const std::string& title, const std::string& elf_override, const std::string& disc_path,
                         const std::string& disc_serial, u32 disc_crc, u32 current_crc) {
}

void Host::PumpMessagesOnCPUThread() {
    std::deque<std::function<void()>> queue;
    {
        std::lock_guard lock(s_cpu_thread_mutex);
        queue.swap(s_cpu_thread_queue);
    }

    for (auto& function : queue)
        function();
}

int FileSystem::OpenFDFileContent(const char* filename)
{
    auto *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    if(env == nullptr) {
        return -1;
    }
    jclass NativeApp = env->FindClass("kr/co/iefriends/pcsx2/NativeApp");
    jmethodID openContentUri = env->GetStaticMethodID(NativeApp, "openContentUri", "(Ljava/lang/String;)I");

    jstring j_filename = env->NewStringUTF(filename);
    int fd = env->CallStaticIntMethod(NativeApp, openContentUri, j_filename);
    return fd;
}

bool FileSystem::CreateDirectoryViaJava(const char* path)
{
    // Bridges to NativeApp.createDirectoryPath (java.io.File.mkdirs). Used as a
    // fallback when libc mkdir() is denied on FUSE-emulated external storage,
    // which is what makes folder memory cards work on a custom data folder.
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    if (env == nullptr)
        return false;
    jclass NativeApp = env->FindClass("kr/co/iefriends/pcsx2/NativeApp");
    if (NativeApp == nullptr)
    {
        env->ExceptionClear();
        return false;
    }
    jmethodID mid = env->GetStaticMethodID(NativeApp, "createDirectoryPath", "(Ljava/lang/String;)Z");
    if (mid == nullptr)
    {
        env->ExceptionClear();
        env->DeleteLocalRef(NativeApp);
        return false;
    }
    // Called many times during folder-card use, so free every local ref and clear
    // any pending JNI exception on all paths — the Java side swallows its own, but
    // a JNI-layer throw must not leak a local ref or an exception onto the next call.
    bool ok = false;
    jstring j_path = env->NewStringUTF(path);
    if (j_path != nullptr)
    {
        ok = (env->CallStaticBooleanMethod(NativeApp, mid, j_path) == JNI_TRUE);
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            ok = false;
        }
        env->DeleteLocalRef(j_path);
    }
    env->DeleteLocalRef(NativeApp);
    return ok;
}

void ReportTestResults(const char* label, int passed, int total)
{
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    if (!env) return;
    jclass clazz = env->FindClass("kr/co/iefriends/pcsx2/NativeApp");
    if (!clazz) return;
    jmethodID mid = env->GetStaticMethodID(clazz, "onTestResults", "(Ljava/lang/String;II)V");
    if (!mid) { env->DeleteLocalRef(clazz); return; }
    jstring jlabel = env->NewStringUTF(label);
    env->CallStaticVoidMethod(clazz, mid, jlabel, (jint)passed, (jint)total);
    env->DeleteLocalRef(jlabel);
    env->DeleteLocalRef(clazz);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_runVMThread(JNIEnv *env, jclass clazz,
                                                 jstring p_szpath) {
    std::string _szPath = GetJavaString(env, p_szpath);
    Console.WriteLn("ARMSX2-START");
    /////////////////////////////

    s_execute_exit = false;
    s_stop_requested = false;
    {
        std::lock_guard lock(s_cpu_thread_mutex);
        s_cpu_thread_id = std::this_thread::get_id();
        s_cpu_thread_queue.clear();
    }

    const char* error;
    if (!VMManager::PerformEarlyHardwareChecks(&error)) {
        Console.Error("Early hardware check failed: %s", error ? error : "unknown error");
        return false;
    }

    // fast_boot : (false:bios->game, true:game)
    VMBootParameters boot_params;
    boot_params.filename = _szPath;
    boot_params.fast_boot = Host::GetBaseBoolSettingValue("EmuCore", "EnableFastBoot", false);
    Console.WriteLnFmt("@@ANDROID_RUNVM_PATH@@ empty={} path={}",
        _szPath.empty() ? 1 : 0, _szPath);
    Console.Error("Loading %s", _szPath.c_str());
    if (!VMManager::Internal::CPUThreadInitialize()) {
        Console.Error("@@ANDROID_CPU_THREAD_INIT_FAILED@@");
        VMManager::Internal::CPUThreadShutdown();
        return false;
    }

    // Wait for Android surface before opening GS
    while (!s_window)
        usleep(10000);

    VMManager::ApplySettings();
    Console.WriteLnFmt(
        "@@ANDROID_CPU_CONFIG@@ ee={} iop={} vu0={} vu1={} fastmem={} mtvu={} "
        "waitloop={} intc={} vuFlag={} vu1Instant={} fpuFull={} fpuOvf={} fpuExtraOvf={}",
        +EmuConfig.Cpu.Recompiler.EnableEE,
        +EmuConfig.Cpu.Recompiler.EnableIOP,
        +EmuConfig.Cpu.Recompiler.EnableVU0,
        +EmuConfig.Cpu.Recompiler.EnableVU1,
        +EmuConfig.Cpu.Recompiler.EnableFastmem,
        +EmuConfig.Speedhacks.vuThread,
        +EmuConfig.Speedhacks.WaitLoop,
        +EmuConfig.Speedhacks.IntcStat,
        +EmuConfig.Speedhacks.vuFlagHack,
        +EmuConfig.Speedhacks.vu1Instant,
        +EmuConfig.Cpu.Recompiler.fpuFullMode,
        +EmuConfig.Cpu.Recompiler.fpuOverflow,
        +EmuConfig.Cpu.Recompiler.fpuExtraOverflow);
    GSDumpReplayer::SetIsDumpRunner(false);

    Error boot_error;
    const VMBootResult boot_result = VMManager::Initialize(boot_params, &boot_error);
    if (boot_result == VMBootResult::StartupSuccess)
    {
        Console.Error("VM INIT");
        // Apply the persisted frame-limit preference now that the VM is up.
        // The overlay's Frame Limiter toggle stores into the base layer via
        // setSetting("EmuCore/GS","FrameLimitEnable") + speedhackLimitermode
        // for live-apply; the live-apply early-returns when no VM exists, so
        // on a cold start the saved preference would otherwise be ignored
        // until the user toggled it. Default is `true` (Nominal) to match
        // VMManager::SetDefaultSettings's behaviour.
        const bool frame_limit_on = Host::GetBaseBoolSettingValue(
            "EmuCore/GS", "FrameLimitEnable", true);
        VMManager::SetLimiterMode(frame_limit_on ? LimiterModeType::Nominal
                                                 : LimiterModeType::Unlimited);
        // The present-cap-suspend flag is process-global (it lives in GS.cpp), so a
        // game stopped mid-fast-forward could leave it set. Clear it on every boot
        // so a fresh game never starts with its display cap silently bypassed.
        GSSetPresentCapSuspended(false);
        VMState _vmState = VMState::Running;
        VMManager::SetState(_vmState);
        ////
        while (true) {
            if (s_stop_requested.load(std::memory_order_acquire)) {
                // Latched stop wins over any state flip caused by racing
                // pause/resume tasks; don't re-enter Execute().
                if (VMManager::GetState() != VMState::Stopping &&
                    VMManager::GetState() != VMState::Shutdown)
                    VMManager::SetState(VMState::Stopping);
                Console.WriteLn("@@ANDROID_RUNLOOP_BREAK@@ latched state=%d",
                    static_cast<int>(VMManager::GetState()));
                break;
            }
            _vmState = VMManager::GetState();
            if (_vmState == VMState::Stopping || _vmState == VMState::Shutdown) {
                break;
            } else if (_vmState == VMState::Running) {
                s_execute_exit = false;
                VMManager::Execute();
                s_execute_exit = true;
                Console.WriteLn("@@ANDROID_EXEC_RETURN@@ state=%d stop=%d",
                    static_cast<int>(VMManager::GetState()),
                    s_stop_requested.load(std::memory_order_acquire) ? 1 : 0);
                Host::PumpMessagesOnCPUThread();
            } else if (_vmState == VMState::Paused) {
                VMManager::IdlePollUpdate();
                Host::PumpMessagesOnCPUThread();
                usleep(16000);
            } else {
                usleep(250000);
            }
        }
        ////
        VMManager::Shutdown(false);
    }
    else
    {
        Console.Error("@@ANDROID_VM_INIT_FAILED@@ result=%d error=%s",
            static_cast<int>(boot_result), boot_error.GetDescription().c_str());
    }
    ////
    Host::PumpMessagesOnCPUThread();
    VMManager::Internal::CPUThreadShutdown();
    {
        std::lock_guard lock(s_cpu_thread_mutex);
        s_cpu_thread_id = std::thread::id();
        s_cpu_thread_queue.clear();
    }

    return true;
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_pause(JNIEnv *env, jclass clazz) {
    if (!VMManager::HasValidVM())
        return;

    const VMState state = VMManager::GetState();
    if (state == VMState::Running)
    {
        Host::RunOnCPUThread([]() {
            if (VMManager::HasValidVM() && VMManager::GetState() == VMState::Running)
                VMManager::SetPaused(true);
            // Persist the BIOS NVRAM (clock / language / console config) on every
            // background/pause, not only on a clean Shutdown. Android users background
            // or swipe the app far more than they cleanly Stop a game, and the process
            // is frequently killed while paused — so BIOS config written to the in-RAM
            // NVM buffer never reached disk, and the BIOS re-ran its first-boot setup
            // on every launch. Runs on the CPU thread (owns CDVD state); cdvdSaveNVRAM()
            // no-ops when the NVM is unchanged, so pausing repeatedly is cheap.
            if (VMManager::HasValidVM())
                cdvdSaveNVRAM();
        });

        if (!s_execute_exit.load(std::memory_order_acquire) && Cpu)
            Cpu->ExitExecution();

        Console.WriteLn("@@ANDROID_PAUSE@@ queued state=%d execute_exit=%d",
            static_cast<int>(state),
            s_execute_exit.load(std::memory_order_acquire) ? 1 : 0);
    }
    else if (state == VMState::Paused)
    {
        Console.WriteLn("@@ANDROID_PAUSE@@ already_paused");
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_resume(JNIEnv *env, jclass clazz) {
    if (!VMManager::HasValidVM())
        return;

    Host::RunOnCPUThread([]() {
        if (VMManager::HasValidVM() && VMManager::GetState() == VMState::Paused)
            VMManager::SetPaused(false);
    });
    Console.WriteLn("@@ANDROID_RESUME@@ queued state=%d", static_cast<int>(VMManager::GetState()));
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_flushShaderCache(JNIEnv *env, jclass clazz) {
    // Persist the Vulkan pipeline cache so cold restarts don't re-compile every
    // pipeline. Hooked from onPause so the typical background-then-swipe-kill
    // sequence on Android still writes the cache. The destructor in
    // VKShaderCache also flushes, but we can't rely on onDestroy running before
    // Android reaps the process. No-op for the OpenGL backend (GL backend
    // manages its own cache via GLShaderCache; this is Vulkan-specific).
    if (g_vulkan_shader_cache)
        g_vulkan_shader_cache->FlushPipelineCache();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_shutdown(JNIEnv *env, jclass clazz) {
    // Only signal Stopping when there's actually a VM to stop. Calling
    // SetState(Stopping) with no active VM leaves s_state stuck at Stopping,
    // which then makes the next VMManager::Initialize fail (it requires
    // s_state == Shutdown). Symptom was a "hang" on first card-tap launch.
    const VMState state = VMManager::GetState();
    const bool active = (state >= VMState::Running && state <= VMState::Stopping);
    Console.WriteLn("@@ANDROID_STOP@@ request active=%d state=%d execute_exit=%d",
        active ? 1 : 0, static_cast<int>(state),
        s_execute_exit.load(std::memory_order_acquire) ? 1 : 0);
    if (!active)
        return;

    // Latch the stop FIRST, before SetState — so even if a queued pause/resume
    // task flips s_state back to Running/Paused, the CPU thread's run loop will
    // still break out and shut down instead of re-entering Execute().
    s_stop_requested.store(true, std::memory_order_release);
    Console.WriteLn("@@STOP_LATCH_SET@@ val=%d",
        s_stop_requested.load(std::memory_order_acquire) ? 1 : 0);

    VMManager::SetLimiterMode(LimiterModeType::Nominal);
    if (VMManager::GetState() != VMState::Stopping)
        VMManager::SetState(VMState::Stopping);
    if (!s_execute_exit.load(std::memory_order_acquire) && Cpu)
        Cpu->ExitExecution();
    Host::RunOnCPUThread([]() { Host::RequestVMShutdown(false, false, false); });

    Console.WriteLn("@@ANDROID_STOP_SIGNAL@@ state=%d execute_exit=%d",
        static_cast<int>(VMManager::GetState()),
        s_execute_exit.load(std::memory_order_acquire) ? 1 : 0);

    for (int i = 0; i < 5000; ++i)
    {
        if (VMManager::GetState() == VMState::Shutdown)
        {
            Console.WriteLn("@@ANDROID_STOP_DONE@@ waited_ms=%d", i);
            return;
        }

        // If the EE is still executing, keep nudging it out of Execute() so
        // the run loop can observe the stop latch. This is intentionally
        // rate-limited; ExitExecution is cheap, but spamming it makes logs and
        // debugging harder.
        if ((i % 16) == 0 && !s_execute_exit.load(std::memory_order_acquire) && Cpu)
            Cpu->ExitExecution();

        if (i > 0 && (i % 1000) == 0)
            Console.WriteLn("@@ANDROID_STOP_WAIT@@ waited_ms=%d state=%d execute_exit=%d",
                i, static_cast<int>(VMManager::GetState()),
                s_execute_exit.load(std::memory_order_acquire) ? 1 : 0);
        usleep(1000);
    }

    Console.WriteLn("@@ANDROID_STOP_TIMEOUT@@ state=%d execute_exit=%d",
        static_cast<int>(VMManager::GetState()),
        s_execute_exit.load(std::memory_order_acquire) ? 1 : 0);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_hasActiveVM(JNIEnv *env, jclass clazz) {
    const VMState state = VMManager::GetState();
    return (state >= VMState::Running && state <= VMState::Stopping);
}


extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_saveStateToSlot(JNIEnv *env, jclass clazz, jint p_slot) {
    // Previous body was a TODO stub spinning on s_execute_exit with the
    // actual SaveStateToSlot call commented out — that's why save was
    // doing nothing. Now we just call straight through.
    //
    // Caller (Kotlin SaveStatePicker) dispatches this on a background
    // thread and pauses the VM beforehand (overlay path), so blocking
    // here is fine. zip_on_thread=false → the zip is finalized before
    // we return, so the slot's Screenshot.png is on disk by the time
    // the picker re-reads slot state. The screenshot is captured by
    // VMManager::SaveStateToSlot from the GS framebuffer automatically
    // — no separate GSQueueSnapshot needed.
    if (!VMManager::HasValidVM())
        return false;
    if (VMManager::GetDiscCRC() == 0)
        return false;
    const ScopedVMPause pause_guard;
    if (!pause_guard.parked()) {
        Console.Error("saveStateToSlot: CPU thread failed to park, refusing to save");
        return false;
    }
    std::string save_error;
    VMManager::SaveStateToSlot(p_slot, /*zip_on_thread=*/false,
        [&save_error](const std::string& error) { save_error = error; });
    if (!save_error.empty()) {
        Console.Error("saveStateToSlot: %s", save_error.c_str());
        return false;
    }
    const std::string filename = VMManager::GetSaveStateFileName(
        VMManager::GetDiscSerial().c_str(), VMManager::GetDiscCRC(), p_slot);
    return !filename.empty() && FileSystem::FileExists(filename.c_str());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_loadStateFromSlot(JNIEnv *env, jclass clazz, jint p_slot) {
    // ScopedVMPause below guarantees the CPU thread is parked before the
    // load runs — do not rely on the Kotlin caller having paused the VM
    // (not every UI flow does, and a load racing a running VM corrupts it).
    if (!VMManager::HasValidVM())
        return false;
    const u32 _crc = VMManager::GetDiscCRC();
    if (_crc == 0)
        return false;
    if (!VMManager::HasSaveStateInSlot(VMManager::GetDiscSerial().c_str(), _crc, p_slot))
        return false;
    const ScopedVMPause pause_guard;
    if (!pause_guard.parked()) {
        Console.Error("loadStateFromSlot: CPU thread failed to park, refusing to load");
        return false;
    }
    const bool loaded = VMManager::LoadStateFromSlot(p_slot);
    // A normal LoadState does not present (only the input-recording path does), so the restored
    // frame isn't shown until the game draws its next frame. When the game is already running
    // that's the next vsync (imperceptible), but a load early in boot — before the present loop
    // is flowing — otherwise leaves a black screen. Force the restored frame to display now.
    if (loaded)
        MTGS::PresentCurrentFrame();
    return loaded;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_changeDisc(JNIEnv *env, jclass clazz, jstring p_path) {
    // Hot-swap the CDVD image on the running VM (NetherSX2-style) instead of
    // rebooting. ChangeDisc cycles the tray so the game detects the new disc —
    // which is exactly what CodeBreaker / multi-disc hand-offs rely on — and
    // emits the on-screen "Disc changed to '...'" OSD. Booting a picked disc is
    // handled separately in Kotlin by stopping and restarting the VM.
    if (!VMManager::HasValidVM())
        return false;
    const char* c_path = (p_path != nullptr) ? env->GetStringUTFChars(p_path, nullptr) : nullptr;
    if (c_path == nullptr)
        return false;
    std::string path(c_path);
    env->ReleaseStringUTFChars(p_path, c_path);
    if (path.empty())
        return false;
    // ChangeDisc mutates live CDVD/IOP/tray state OWNED by the CPU thread, so it
    // must run THERE, not from JNI (doing it here races the emulator and hangs).
    // Park the CPU thread so the paused idle loop (runVMThread) drains the queue
    // within a frame; RunOnCPUThread(block) then waits for the swap to finish.
    // We deliberately do NOT resume here — the Kotlin caller unpauses afterward
    // (single resume authority), so the game runs and detects the new disc.
    const bool was_running = (VMManager::GetState() == VMState::Running);
    if (was_running) {
        VMManager::SetPaused(true);
        if (!s_execute_exit.load(std::memory_order_acquire) && Cpu)
            Cpu->ExitExecution();
        for (int i = 0; i < 3000 && !s_execute_exit.load(std::memory_order_acquire); ++i)
            usleep(1000);
    }
    bool ok = false;
    Host::RunOnCPUThread([&path, &ok]() {
        ok = VMManager::ChangeDisc(CDVD_SourceType::Iso, path);
    }, /*block=*/true);
    return ok;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getGamePathSlot(JNIEnv *env, jclass clazz, jint p_slot) {
    std::string _filename = VMManager::GetSaveStateFileName(VMManager::GetDiscSerial().c_str(), VMManager::GetDiscCRC(), p_slot);
    if(!_filename.empty()) {
        return env->NewStringUTF(_filename.c_str());
    }
    return nullptr;
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getImageSlot(JNIEnv *env, jclass clazz, jint p_slot) {
    jbyteArray retArr = nullptr;

    std::string _filename = VMManager::GetSaveStateFileName(VMManager::GetDiscSerial().c_str(), VMManager::GetDiscCRC(), p_slot);
    if(!_filename.empty())
    {
        zip_error_t ze = {};
        auto zf = zip_open_managed(_filename.c_str(), ZIP_RDONLY, &ze);
        if (zf) {
            auto zff = zip_fopen_managed(zf.get(), "Screenshot.png", 0);
            if(zff) {
                std::optional<std::vector<u8>> optdata(ReadBinaryFileInZip(zff.get()));
                if (optdata.has_value()) {
                    std::vector<u8> vec = std::move(optdata.value());
                    ////
                    auto length = static_cast<jsize>(vec.size());
                    retArr = env->NewByteArray(length);
                    if (retArr != nullptr) {
                        env->SetByteArrayRegion(retArr, 0, length,
                                                reinterpret_cast<const jbyte *>(vec.data()));
                    }
                }
            }
        }
    }

    return retArr;
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getSaveStateImage(JNIEnv *env, jclass clazz, jstring p_path) {
    const std::string filename = GetJavaString(env, p_path);
    if (filename.empty())
        return nullptr;

    zip_error_t ze = {};
    auto zf = zip_open_managed(filename.c_str(), ZIP_RDONLY, &ze);
    if (!zf)
        return nullptr;

    auto screenshot = zip_fopen_managed(zf.get(), "Screenshot.png", 0);
    if (!screenshot)
        return nullptr;

    std::optional<std::vector<u8>> data(ReadBinaryFileInZip(screenshot.get()));
    if (!data.has_value() || data->empty())
        return nullptr;

    const jsize length = static_cast<jsize>(data->size());
    jbyteArray result = env->NewByteArray(length);
    if (result)
        env->SetByteArrayRegion(result, 0, length, reinterpret_cast<const jbyte*>(data->data()));
    return result;
}

// =====================  Autosave-on-exit slot  =====================
// Backed by VMManager::SAVESTATE_SLOT_AUTOSAVE (s32 sentinel = -2),
// stored as `{serial} (CRC).autosave.p2s`. Lets "Save State And Exit"
// avoid clobbering user slot 0; the load picker surfaces the autosave
// tile only when hasAutosaveState() returns true.

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_saveAutosaveState(JNIEnv *env, jclass clazz) {
    if (!VMManager::HasValidVM())
        return false;
    if (VMManager::GetDiscCRC() == 0)
        return false;
    const ScopedVMPause pause_guard;
    if (!pause_guard.parked()) {
        Console.Error("saveAutosaveState: CPU thread failed to park, refusing to save");
        return false;
    }
    std::string save_error;
    VMManager::SaveStateToSlot(VMManager::SAVESTATE_SLOT_AUTOSAVE, /*zip_on_thread=*/false,
        [&save_error](const std::string& error) { save_error = error; });
    if (!save_error.empty()) {
        Console.Error("saveAutosaveState: %s", save_error.c_str());
        return false;
    }
    const std::string filename = VMManager::GetSaveStateFileName(
        VMManager::GetDiscSerial().c_str(), VMManager::GetDiscCRC(),
        VMManager::SAVESTATE_SLOT_AUTOSAVE);
    return !filename.empty() && FileSystem::FileExists(filename.c_str());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_loadAutosaveState(JNIEnv *env, jclass clazz) {
    if (!VMManager::HasValidVM())
        return false;
    const u32 _crc = VMManager::GetDiscCRC();
    if (_crc == 0)
        return false;
    if (!VMManager::HasSaveStateInSlot(VMManager::GetDiscSerial().c_str(), _crc,
                                       VMManager::SAVESTATE_SLOT_AUTOSAVE))
        return false;
    const ScopedVMPause pause_guard;
    if (!pause_guard.parked()) {
        Console.Error("loadAutosaveState: CPU thread failed to park, refusing to load");
        return false;
    }
    const bool loaded = VMManager::LoadStateFromSlot(VMManager::SAVESTATE_SLOT_AUTOSAVE);
    // Force the restored frame to display — this load fires during boot (auto-load / Save+Quit
    // resume), before the game has drawn its first frame, so without an explicit present the
    // screen stays black until the game happens to redraw. See loadStateFromSlot.
    if (loaded)
        MTGS::PresentCurrentFrame();
    return loaded;
}

// Host-side count of frames the GS has presented since it opened (g_perfmon frame counter, NOT
// part of the savestate). The auto-load-on-boot path polls this so it only restores the state
// once the renderer is actually presenting frames — loading before the present loop is flowing
// leaves a black screen (the restored frame never reaches the surface).
extern "C"
JNIEXPORT jint JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getPresentedFrameCount(JNIEnv *env, jclass clazz) {
    if (!VMManager::HasValidVM())
        return 0;
    return static_cast<jint>(g_perfmon.GetFrame());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_hasAutosaveState(JNIEnv *env, jclass clazz) {
    if (!VMManager::HasValidVM())
        return false;
    const u32 _crc = VMManager::GetDiscCRC();
    if (_crc == 0)
        return false;
    return VMManager::HasSaveStateInSlot(VMManager::GetDiscSerial().c_str(), _crc,
                                         VMManager::SAVESTATE_SLOT_AUTOSAVE);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getAutosaveGamePath(JNIEnv *env, jclass clazz) {
    std::string _filename = VMManager::GetSaveStateFileName(VMManager::GetDiscSerial().c_str(),
                                                            VMManager::GetDiscCRC(),
                                                            VMManager::SAVESTATE_SLOT_AUTOSAVE);
    if (!_filename.empty())
        return env->NewStringUTF(_filename.c_str());
    return nullptr;
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getAutosaveImage(JNIEnv *env, jclass clazz) {
    jbyteArray retArr = nullptr;

    std::string _filename = VMManager::GetSaveStateFileName(VMManager::GetDiscSerial().c_str(),
                                                            VMManager::GetDiscCRC(),
                                                            VMManager::SAVESTATE_SLOT_AUTOSAVE);
    if (!_filename.empty())
    {
        zip_error_t ze = {};
        auto zf = zip_open_managed(_filename.c_str(), ZIP_RDONLY, &ze);
        if (zf) {
            auto zff = zip_fopen_managed(zf.get(), "Screenshot.png", 0);
            if (zff) {
                std::optional<std::vector<u8>> optdata(ReadBinaryFileInZip(zff.get()));
                if (optdata.has_value()) {
                    std::vector<u8> vec = std::move(optdata.value());
                    auto length = static_cast<jsize>(vec.size());
                    retArr = env->NewByteArray(length);
                    if (retArr != nullptr) {
                        env->SetByteArrayRegion(retArr, 0, length,
                                                reinterpret_cast<const jbyte *>(vec.data()));
                    }
                }
            }
        }
    }

    return retArr;
}


void Host::CommitBaseSettingChanges()
{
    // nothing to save, we're all in memory
}

void Host::LoadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock)
{
}

void Host::CheckForSettingsChanges(const Pcsx2Config& old_config)
{
}

bool Host::RequestResetSettings(bool folders, bool core, bool controllers, bool hotkeys, bool ui)
{
    // not running any UI, so no settings requests will come in
    return false;
}

void Host::SetDefaultUISettings(SettingsInterface& si)
{
    // nothing
}

std::unique_ptr<ProgressCallback> Host::CreateHostProgressCallback()
{
    return nullptr;
}

void Host::ReportErrorAsync(const std::string_view title, const std::string_view message)
{
    if (!title.empty() && !message.empty())
        ERROR_LOG("ReportErrorAsync: {}: {}", title, message);
    else if (!message.empty())
        ERROR_LOG("ReportErrorAsync: {}", message);
}

//TODO
/*bool Host::ConfirmMessage(const std::string_view title, const std::string_view message)
{
    if (!title.empty() && !message.empty())
        ERROR_LOG("ConfirmMessage: {}: {}", title, message);
    else if (!message.empty())
        ERROR_LOG("ConfirmMessage: {}", message);

    return true;
}*/

void Host::OpenURL(const std::string_view url)
{
    // noop
}

bool Host::CopyTextToClipboard(const std::string_view text)
{
    return false;
}

std::string Host::GetTextFromClipboard()
{
    return {};
}

void Host::BeginTextInput()
{
    // noop
}

void Host::EndTextInput()
{
    // noop
}

std::optional<WindowInfo> Host::GetTopLevelWindowInfo()
{
    return std::nullopt;
}

void Host::OnInputDeviceConnected(const std::string_view identifier, const std::string_view device_name)
{
}

void Host::OnInputDeviceDisconnected(const InputBindingKey key, const std::string_view identifier)
{
}

void Host::SetMouseMode(bool relative_mode, bool hide_cursor)
{
}

void Host::RequestResizeHostDisplay(s32 width, s32 height)
{
}

void Host::OnVMStarting()
{
}

void Host::OnVMStarted()
{
}

void Host::OnVMDestroyed()
{
}

void Host::OnVMPaused()
{
    Native::vmSetPaused(true);
}

void Host::OnVMResumed()
{
    Native::vmSetPaused(false);
}

void Host::OnPerformanceMetricsUpdated()
{
}

void Host::OnSaveStateLoading(const std::string_view filename)
{
}

void Host::OnSaveStateLoaded(const std::string_view filename, bool was_successful)
{
}

void Host::OnSaveStateSaved(const std::string_view filename)
{
}

void Host::RunOnCPUThread(std::function<void()> function, bool block /* = false */)
{
    const std::thread::id current_thread = std::this_thread::get_id();
    bool run_inline = false;
    {
        std::lock_guard lock(s_cpu_thread_mutex);
        run_inline = (s_cpu_thread_id == std::thread::id() || s_cpu_thread_id == current_thread);
    }
    if (run_inline)
    {
        function();
        return;
    }

    if (block)
    {
        std::mutex wait_mutex;
        std::condition_variable wait_cv;
        bool done = false;
        {
            std::lock_guard lock(s_cpu_thread_mutex);
            s_cpu_thread_queue.push_back([&]() {
                function();
                {
                    std::lock_guard wait_lock(wait_mutex);
                    done = true;
                }
                wait_cv.notify_one();
            });
        }

        std::unique_lock wait_lock(wait_mutex);
        wait_cv.wait(wait_lock, [&]() { return done; });
    }
    else
    {
        std::lock_guard lock(s_cpu_thread_mutex);
        s_cpu_thread_queue.push_back(std::move(function));
    }
}

void Host::RefreshGameListAsync(bool invalidate_cache)
{
}

void Host::CancelGameListRefresh()
{
}

bool Host::IsFullscreen()
{
    return false;
}

void Host::SetFullscreen(bool enabled)
{
}

void Host::OnCaptureStarted(const std::string& filename)
{
}

void Host::OnCaptureStopped()
{
}

void Host::RequestExitApplication(bool allow_confirm)
{
}

void Host::RequestExitBigPicture()
{
}

void Host::RequestVMShutdown(bool allow_confirm, bool allow_save_state, bool default_save_state)
{
    // This runs as a queued CPU-thread task (Host::RunOnCPUThread from the shutdown
    // JNI). Since the EE now bails out of Execute() promptly on Stopping, the run
    // loop can reach its post-Shutdown message pump and process THIS task AFTER
    // VMManager::Shutdown(false) already drove s_state to Shutdown. Re-setting
    // Stopping here would leave s_state stuck at Stopping, so the next game's
    // VMManager::Initialize fails with "already running" (kick-back to library on
    // the 2nd launch). If we're already shut down, there's nothing left to stop.
    if (VMManager::GetState() == VMState::Shutdown)
        return;
    VMManager::SetState(VMState::Stopping);
    if (!s_execute_exit.load(std::memory_order_acquire) && Cpu)
        Cpu->ExitExecution();
}

void Host::OnAchievementsLoginSuccess(const char* username, u32 points, u32 sc_points, u32 unread_messages)
{
    // noop
}

void Host::OnAchievementsLoginRequested(Achievements::LoginRequestReason reason)
{
    // noop
}

void Host::OnAchievementsHardcoreModeChanged(bool enabled)
{
    // noop
}

void Host::OnAchievementsRefreshed()
{
    // noop
}

void Host::OnCoverDownloaderOpenRequested()
{
    // noop
}

void Host::OnCreateMemoryCardOpenRequested()
{
    // noop
}

bool Host::ShouldPreferHostFileSelector()
{
    return false;
}

void Host::OpenHostFileSelectorAsync(std::string_view title, bool select_directory, FileSelectorCallback callback,
                                     FileSelectorFilters filters, std::string_view initial_directory)
{
    callback(std::string());
}

// -------------------------------------------------------------------------
// USB keyboard (#254) — host-keyboard code conversion.
//
// PCSX2's usb-hid HIDKbdDevice builds its host-key -> QKeyCode map by asking
// InputManager::ConvertHostKeyboardStringToCode(name) for every QKeyCode name
// (usb-hid.cpp: s_qkeycode_names). On desktop the "host code" is an SDL
// scancode; on Android we have no SDL keyboard, so we define the host code to
// BE the QKeyCode enum value. That makes the round-trip identity:
//   ConvertHostKeyboardStringToCode("A") == Q_KEY_CODE_A
// and lets usb-hid populate keycode_mapping[Q_KEY_CODE_A] = Q_KEY_CODE_A.
// The USB-keyboard JNI below then feeds Android KeyEvent codes through the
// Android-keycode -> QKeyCode table and calls USB::SetDeviceBindValue(port,
// qcode, value) — SetBindingValue looks qcode up in keycode_mapping and queues
// the emulated key.
//
// The name<->QKeyCode pairs MUST match the strings in usb-hid.cpp's
// s_qkeycode_names, otherwise the map entry for that key is never created.
namespace
{
    struct QKeyName
    {
        QKeyCode qcode;
        const char* name;
    };

    // Mirror of usb-hid.cpp s_qkeycode_names (host-string -> QKeyCode). Kept in
    // sync by hand — both derive from the fixed QKeyCode enum in qemu-usb/hid.h.
    constexpr QKeyName s_qkey_names[] = {
        {Q_KEY_CODE_0, "0"}, {Q_KEY_CODE_1, "1"}, {Q_KEY_CODE_2, "2"}, {Q_KEY_CODE_3, "3"},
        {Q_KEY_CODE_4, "4"}, {Q_KEY_CODE_5, "5"}, {Q_KEY_CODE_6, "6"}, {Q_KEY_CODE_7, "7"},
        {Q_KEY_CODE_8, "8"}, {Q_KEY_CODE_9, "9"},
        {Q_KEY_CODE_A, "A"}, {Q_KEY_CODE_B, "B"}, {Q_KEY_CODE_C, "C"}, {Q_KEY_CODE_D, "D"},
        {Q_KEY_CODE_E, "E"}, {Q_KEY_CODE_F, "F"}, {Q_KEY_CODE_G, "G"}, {Q_KEY_CODE_H, "H"},
        {Q_KEY_CODE_I, "I"}, {Q_KEY_CODE_J, "J"}, {Q_KEY_CODE_K, "K"}, {Q_KEY_CODE_L, "L"},
        {Q_KEY_CODE_M, "M"}, {Q_KEY_CODE_N, "N"}, {Q_KEY_CODE_O, "O"}, {Q_KEY_CODE_P, "P"},
        {Q_KEY_CODE_Q, "Q"}, {Q_KEY_CODE_R, "R"}, {Q_KEY_CODE_S, "S"}, {Q_KEY_CODE_T, "T"},
        {Q_KEY_CODE_U, "U"}, {Q_KEY_CODE_V, "V"}, {Q_KEY_CODE_W, "W"}, {Q_KEY_CODE_X, "X"},
        {Q_KEY_CODE_Y, "Y"}, {Q_KEY_CODE_Z, "Z"},
        {Q_KEY_CODE_MINUS, "Minus"}, {Q_KEY_CODE_EQUAL, "Equal"},
        {Q_KEY_CODE_BACKSPACE, "Backspace"}, {Q_KEY_CODE_TAB, "Tab"},
        {Q_KEY_CODE_BRACKET_LEFT, "BracketLeft"}, {Q_KEY_CODE_BRACKET_RIGHT, "BracketRight"},
        {Q_KEY_CODE_RET, "Return"}, {Q_KEY_CODE_SEMICOLON, "Semicolon"},
        {Q_KEY_CODE_APOSTROPHE, "Apostrophe"}, {Q_KEY_CODE_GRAVE_ACCENT, "Agrave"},
        {Q_KEY_CODE_BACKSLASH, "Backslash"}, {Q_KEY_CODE_COMMA, "Comma"},
        {Q_KEY_CODE_DOT, "Period"}, {Q_KEY_CODE_SLASH, "Slash"},
        {Q_KEY_CODE_ASTERISK, "Asterisk"}, {Q_KEY_CODE_SPC, "Space"},
        {Q_KEY_CODE_CAPS_LOCK, "Caps_lock"}, {Q_KEY_CODE_ESC, "Escape"},
        {Q_KEY_CODE_SHIFT, "Shift"}, {Q_KEY_CODE_SHIFT_R, "Shift_r"},
        {Q_KEY_CODE_CTRL, "Control"}, {Q_KEY_CODE_CTRL_R, "Control_r"},
        {Q_KEY_CODE_ALT, "Alt"}, {Q_KEY_CODE_ALT_R, "Alt_r"},
        {Q_KEY_CODE_META_L, "Meta"}, {Q_KEY_CODE_MENU, "Menu"},
        {Q_KEY_CODE_F1, "F1"}, {Q_KEY_CODE_F2, "F2"}, {Q_KEY_CODE_F3, "F3"},
        {Q_KEY_CODE_F4, "F4"}, {Q_KEY_CODE_F5, "F5"}, {Q_KEY_CODE_F6, "F6"},
        {Q_KEY_CODE_F7, "F7"}, {Q_KEY_CODE_F8, "F8"}, {Q_KEY_CODE_F9, "F9"},
        {Q_KEY_CODE_F10, "F10"}, {Q_KEY_CODE_F11, "F11"}, {Q_KEY_CODE_F12, "F12"},
        {Q_KEY_CODE_NUM_LOCK, "Num_lock"}, {Q_KEY_CODE_SCROLL_LOCK, "Scroll_lock"},
        {Q_KEY_CODE_KP_DIVIDE, "NumpadSlash"}, {Q_KEY_CODE_KP_MULTIPLY, "NumpadAsterisk"},
        {Q_KEY_CODE_KP_SUBTRACT, "NumpadMinus"}, {Q_KEY_CODE_KP_ADD, "NumpadPlus"},
        {Q_KEY_CODE_KP_ENTER, "NumpadReturn"}, {Q_KEY_CODE_KP_DECIMAL, "NumpadPeriod"},
        {Q_KEY_CODE_KP_0, "Numpad0"}, {Q_KEY_CODE_KP_1, "Numpad1"}, {Q_KEY_CODE_KP_2, "Numpad2"},
        {Q_KEY_CODE_KP_3, "Numpad3"}, {Q_KEY_CODE_KP_4, "Numpad4"}, {Q_KEY_CODE_KP_5, "Numpad5"},
        {Q_KEY_CODE_KP_6, "Numpad6"}, {Q_KEY_CODE_KP_7, "Numpad7"}, {Q_KEY_CODE_KP_8, "Numpad8"},
        {Q_KEY_CODE_KP_9, "Numpad9"}, {Q_KEY_CODE_KP_COMMA, "NumpadComma"},
        {Q_KEY_CODE_KP_EQUALS, "NumpadEqual"},
        {Q_KEY_CODE_HOME, "Home"}, {Q_KEY_CODE_PGUP, "PageUp"}, {Q_KEY_CODE_PGDN, "PageDown"},
        {Q_KEY_CODE_END, "End"}, {Q_KEY_CODE_LEFT, "Left"}, {Q_KEY_CODE_UP, "Up"},
        {Q_KEY_CODE_DOWN, "Down"}, {Q_KEY_CODE_RIGHT, "Right"},
        {Q_KEY_CODE_INSERT, "Insert"}, {Q_KEY_CODE_DELETE, "Delete"},
        {Q_KEY_CODE_PRINT, "Print"}, {Q_KEY_CODE_PAUSE, "Pause"}, {Q_KEY_CODE_SYSRQ, "Sysrq"},
        {Q_KEY_CODE_LESS, "Less"},
    };
}

std::optional<u32> InputManager::ConvertHostKeyboardStringToCode(const std::string_view str)
{
    for (const QKeyName& kn : s_qkey_names)
    {
        if (str == kn.name)
            return static_cast<u32>(kn.qcode);
    }
    return std::nullopt;
}

std::optional<std::string> InputManager::ConvertHostKeyboardCodeToString(u32 code)
{
    for (const QKeyName& kn : s_qkey_names)
    {
        if (static_cast<u32>(kn.qcode) == code)
            return std::string(kn.name);
    }
    return std::nullopt;
}

const char* InputManager::ConvertHostKeyboardCodeToIcon(u32 code)
{
    return nullptr;
}

// -------------------------------------------------------------------------
// Android KeyEvent keyCode -> QKeyCode. Android AKEYCODE_* values are the same
// integers Java's android.view.KeyEvent.KEYCODE_* constants use; the Kotlin
// side forwards event.keyCode straight through. Only the subset a PS2 game
// (EQOA, Konami keyboard titles) actually reads is mapped — printable keys,
// modifiers, editing/navigation, function and numpad keys. Unmapped codes
// return Q_KEY_CODE_UNMAPPED and are dropped.
static QKeyCode AndroidKeyCodeToQKeyCode(int kc)
{
    // AKEYCODE letters A..Z = 29..54 (alphabetical). QKeyCode letters are NOT
    // alphabetical (keyboard-row order: A=36, S=37, D=38, ...), so map each
    // explicitly rather than by arithmetic offset.
    if (kc >= 29 && kc <= 54)
    {
        static constexpr QKeyCode kLetters[26] = {
            Q_KEY_CODE_A, Q_KEY_CODE_B, Q_KEY_CODE_C, Q_KEY_CODE_D, Q_KEY_CODE_E,
            Q_KEY_CODE_F, Q_KEY_CODE_G, Q_KEY_CODE_H, Q_KEY_CODE_I, Q_KEY_CODE_J,
            Q_KEY_CODE_K, Q_KEY_CODE_L, Q_KEY_CODE_M, Q_KEY_CODE_N, Q_KEY_CODE_O,
            Q_KEY_CODE_P, Q_KEY_CODE_Q, Q_KEY_CODE_R, Q_KEY_CODE_S, Q_KEY_CODE_T,
            Q_KEY_CODE_U, Q_KEY_CODE_V, Q_KEY_CODE_W, Q_KEY_CODE_X, Q_KEY_CODE_Y,
            Q_KEY_CODE_Z,
        };
        return kLetters[kc - 29];
    }
    if (kc >= 7 && kc <= 16)
    {
        // Android orders 0 first (7), then 1..9 (8..16). QKeyCode digits are
        // 1..9 (=9..17) then 0 (=18), so 0 is special-cased and 1..9 are
        // contiguous in QKeyCode too.
        if (kc == 7)
            return Q_KEY_CODE_0;
        return static_cast<QKeyCode>(Q_KEY_CODE_1 + (kc - 8));
    }
    switch (kc)
    {
        // Whitespace / editing
        case 62: return Q_KEY_CODE_SPC;         // SPACE
        case 66: return Q_KEY_CODE_RET;         // ENTER
        case 67: return Q_KEY_CODE_BACKSPACE;   // DEL (backspace)
        case 61: return Q_KEY_CODE_TAB;         // TAB
        case 111: return Q_KEY_CODE_ESC;        // ESCAPE
        case 112: return Q_KEY_CODE_DELETE;     // FORWARD_DEL
        // Punctuation
        case 69: return Q_KEY_CODE_MINUS;       // MINUS
        case 70: return Q_KEY_CODE_EQUAL;       // EQUALS
        case 71: return Q_KEY_CODE_BRACKET_LEFT;  // LEFT_BRACKET
        case 72: return Q_KEY_CODE_BRACKET_RIGHT; // RIGHT_BRACKET
        case 73: return Q_KEY_CODE_BACKSLASH;   // BACKSLASH
        case 74: return Q_KEY_CODE_SEMICOLON;   // SEMICOLON
        case 75: return Q_KEY_CODE_APOSTROPHE;  // APOSTROPHE
        case 68: return Q_KEY_CODE_GRAVE_ACCENT;// GRAVE
        case 76: return Q_KEY_CODE_SLASH;       // SLASH
        case 55: return Q_KEY_CODE_COMMA;       // COMMA
        case 56: return Q_KEY_CODE_DOT;         // PERIOD
        // Modifiers
        case 59: return Q_KEY_CODE_SHIFT;       // SHIFT_LEFT
        case 60: return Q_KEY_CODE_SHIFT_R;     // SHIFT_RIGHT
        case 113: return Q_KEY_CODE_CTRL;       // CTRL_LEFT
        case 114: return Q_KEY_CODE_CTRL_R;     // CTRL_RIGHT
        case 57: return Q_KEY_CODE_ALT;         // ALT_LEFT
        case 58: return Q_KEY_CODE_ALT_R;       // ALT_RIGHT
        // Both Meta keys collapse to META_L: usb-hid's keycode_mapping is keyed
        // by ConvertHostKeyboardStringToCode("Meta"), which resolves to META_L
        // (the first "Meta" entry), so META_R has no map entry to hit.
        case 117: return Q_KEY_CODE_META_L;     // META_LEFT
        case 118: return Q_KEY_CODE_META_L;     // META_RIGHT
        case 115: return Q_KEY_CODE_CAPS_LOCK;  // CAPS_LOCK
        case 116: return Q_KEY_CODE_SCROLL_LOCK;// SCROLL_LOCK
        case 143: return Q_KEY_CODE_NUM_LOCK;   // NUM_LOCK
        // Navigation
        case 122: return Q_KEY_CODE_HOME;       // MOVE_HOME
        case 123: return Q_KEY_CODE_END;        // MOVE_END
        case 92: return Q_KEY_CODE_PGUP;        // PAGE_UP
        case 93: return Q_KEY_CODE_PGDN;        // PAGE_DOWN
        case 124: return Q_KEY_CODE_INSERT;     // INSERT
        case 21: return Q_KEY_CODE_LEFT;        // DPAD_LEFT
        case 22: return Q_KEY_CODE_RIGHT;       // DPAD_RIGHT
        case 19: return Q_KEY_CODE_UP;          // DPAD_UP
        case 20: return Q_KEY_CODE_DOWN;        // DPAD_DOWN
        // System keys occasionally on keyboards
        case 120: return Q_KEY_CODE_SYSRQ;      // SYSRQ (PrintScreen)
        case 121: return Q_KEY_CODE_PAUSE;      // BREAK
        // Function keys F1..F12 = 131..142
        case 131: return Q_KEY_CODE_F1;
        case 132: return Q_KEY_CODE_F2;
        case 133: return Q_KEY_CODE_F3;
        case 134: return Q_KEY_CODE_F4;
        case 135: return Q_KEY_CODE_F5;
        case 136: return Q_KEY_CODE_F6;
        case 137: return Q_KEY_CODE_F7;
        case 138: return Q_KEY_CODE_F8;
        case 139: return Q_KEY_CODE_F9;
        case 140: return Q_KEY_CODE_F10;
        case 141: return Q_KEY_CODE_F11;
        case 142: return Q_KEY_CODE_F12;
        // Numpad: NUMPAD_0..9 = 144..153
        case 144: return Q_KEY_CODE_KP_0;
        case 145: return Q_KEY_CODE_KP_1;
        case 146: return Q_KEY_CODE_KP_2;
        case 147: return Q_KEY_CODE_KP_3;
        case 148: return Q_KEY_CODE_KP_4;
        case 149: return Q_KEY_CODE_KP_5;
        case 150: return Q_KEY_CODE_KP_6;
        case 151: return Q_KEY_CODE_KP_7;
        case 152: return Q_KEY_CODE_KP_8;
        case 153: return Q_KEY_CODE_KP_9;
        case 154: return Q_KEY_CODE_KP_DIVIDE;   // NUMPAD_DIVIDE
        case 155: return Q_KEY_CODE_KP_MULTIPLY; // NUMPAD_MULTIPLY
        case 156: return Q_KEY_CODE_KP_SUBTRACT; // NUMPAD_SUBTRACT
        case 157: return Q_KEY_CODE_KP_ADD;      // NUMPAD_ADD
        case 158: return Q_KEY_CODE_KP_DECIMAL;  // NUMPAD_DOT
        case 159: return Q_KEY_CODE_KP_COMMA;    // NUMPAD_COMMA
        case 160: return Q_KEY_CODE_KP_ENTER;    // NUMPAD_ENTER
        case 161: return Q_KEY_CODE_KP_EQUALS;   // NUMPAD_EQUALS
        default: return Q_KEY_CODE_UNMAPPED;
    }
}

// Attach/detach the emulated USB HID keyboard on a USB port (0 or 1) LIVE on a
// running VM. Persistence of [USB{port+1}] Type is handled Kotlin-side via
// setSetting (Settings.applyTo), so this only drives the live device
// (re)creation: it sets the live EmuConfig and calls USB::CheckForConfigChanges,
// which DestroyDevice/CreateDevice the USB port so the running game sees the
// (dis)connect immediately. Mirrors enablePad2's threading discipline —
// ScopedVMPause parks the EE/MTVU/MTGS pipeline (which the USB/OHCI poll runs
// on) while the device list is rebuilt. No-op before the VM exists: the
// persisted Type is picked up by USBOptions::LoadSave on the next boot.
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_usbSetKeyboardEnabled(JNIEnv*, jclass, jint p_port, jboolean p_enabled) {
    if (p_port < 0 || static_cast<u32>(p_port) >= USB::NUM_PORTS)
        return;
    if (!VMManager::HasValidVM())
        return;

    const u32 port = static_cast<u32>(p_port);
    const s32 new_type = p_enabled ? DEVTYPE_HIDKEYBOARD : DEVTYPE_NONE;
    if (EmuConfig.USB.Ports[port].DeviceType == new_type)
        return; // already in the requested state

    ScopedVMPause vm_pause(/*pause_audio=*/false);
    if (!vm_pause.parked())
        return;

    const Pcsx2Config old_config(EmuConfig);
    EmuConfig.USB.Ports[port].DeviceType = new_type;
    EmuConfig.USB.Ports[port].DeviceSubtype = 0;
    // Serialize the device-list swap against the Android input thread's
    // usbKeyboardKey / applyPadButton calls (both touch the same emulated
    // device state) — same reasoning as enablePad2's s_pad_mutex.
    {
        std::lock_guard<std::mutex> lk(s_pad_mutex);
        USB::CheckForConfigChanges(old_config);
    }
    Console.WriteLnFmt("@@ANDROID_USBKBD@@ port={} type={}", port, p_enabled ? "hidkbd" : "None");
}

// Forward one Android hardware KeyEvent to the emulated USB keyboard on [port].
// [androidKeyCode] is android.view.KeyEvent.keyCode; [pressed] is down/up.
// Maps to a QKeyCode and drives USB::SetDeviceBindValue, which (via
// HIDKbdDevice::SetBindingValue) queues the HID key report. No-op when no USB
// keyboard is attached to that port or the key isn't mappable. Called on the
// Android input thread — SetDeviceBindValue mutates the HID event queue that
// the USB/OHCI poll (CPU thread) reads, so serialize with s_pad_mutex (shared
// with pad input; the emulated USB keyboard is a low-rate event source).
extern "C" JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_usbKeyboardKey(JNIEnv*, jclass, jint p_port, jint p_androidKeyCode, jboolean p_pressed) {
    if (p_port < 0 || static_cast<u32>(p_port) >= USB::NUM_PORTS)
        return JNI_FALSE;
    if (!VMManager::HasValidVM())
        return JNI_FALSE;
    if (EmuConfig.USB.Ports[static_cast<u32>(p_port)].DeviceType != DEVTYPE_HIDKEYBOARD)
        return JNI_FALSE;

    const QKeyCode qcode = AndroidKeyCodeToQKeyCode(static_cast<int>(p_androidKeyCode));
    if (qcode == Q_KEY_CODE_UNMAPPED)
        return JNI_FALSE;

    std::lock_guard<std::mutex> lk(s_pad_mutex);
    USB::SetDeviceBindValue(static_cast<u32>(p_port), static_cast<u32>(qcode), p_pressed ? 1.0f : 0.0f);
    return JNI_TRUE;
}

s32 Host::Internal::GetTranslatedStringImpl(
        const std::string_view context, const std::string_view msg, char* tbuf, size_t tbuf_space)
{
    if (msg.size() > tbuf_space)
        return -1;
    else if (msg.empty())
        return 0;

    std::memcpy(tbuf, msg.data(), msg.size());
    return static_cast<s32>(msg.size());
}

std::string Host::TranslatePluralToString(const char* context, const char* msg, const char* disambiguation, int count)
{
    TinyString count_str = TinyString::from_format("{}", count);

    std::string ret(msg);
    for (;;)
    {
        std::string::size_type pos = ret.find("%n");
        if (pos == std::string::npos)
            break;

        ret.replace(pos, pos + 2, count_str.view());
    }

    return ret;
}

void Host::ReportInfoAsync(const std::string_view title, const std::string_view message)
{
}

bool Host::LocaleCircleConfirm()
{
    return false;
}

bool Host::InNoGUIMode()
{
    return false;
}

int Host::LocaleSensitiveCompare(std::string_view lhs, std::string_view rhs)
{
    return lhs.compare(rhs);
}

// OSD toggle helpers. The perf-overlay renderer reads the LIVE GSConfig every
// frame; the canonical sync (EmuConfig.GS -> GSConfig) only happens inside
// MTGS::ApplySettings, which DEFERS the copy to the GS thread and is skipped
// entirely when MTGS isn't open. That meant an OSD toggle could land in
// EmuConfig yet never reach GSConfig, so the on-screen display appeared to
// ignore the switch. Copy the OSD fields straight into GSConfig here (plain
// bools/ints — a torn cross-thread read is impossible), so the change is
// immediate and reliable, then still run the MTGS reconfigure for the rest.
static void applyOsdSetting()
{
    GSConfig.OsdShowSpeed = EmuConfig.GS.OsdShowSpeed;
    GSConfig.OsdShowFPS = EmuConfig.GS.OsdShowFPS;
    GSConfig.OsdShowVPS = EmuConfig.GS.OsdShowVPS;
    GSConfig.OsdShowCPU = EmuConfig.GS.OsdShowCPU;
    GSConfig.OsdShowGPU = EmuConfig.GS.OsdShowGPU;
    GSConfig.OsdShowResolution = EmuConfig.GS.OsdShowResolution;
    GSConfig.OsdShowGSStats = EmuConfig.GS.OsdShowGSStats;
    GSConfig.OsdShowFrameTimes = EmuConfig.GS.OsdShowFrameTimes;
    GSConfig.OsdShowHardwareInfo = EmuConfig.GS.OsdShowHardwareInfo;
    GSConfig.OsdShowGPUStats = EmuConfig.GS.OsdShowGPUStats;
    GSConfig.OsdShowVersion = EmuConfig.GS.OsdShowVersion;
    GSConfig.OsdShowSettings = EmuConfig.GS.OsdShowSettings;
    GSConfig.OsdShowInputs = EmuConfig.GS.OsdShowInputs;
    GSConfig.OsdMessagesPos = EmuConfig.GS.OsdMessagesPos;
    GSConfig.OsdScale = EmuConfig.GS.OsdScale;
    GSConfig.OsdColor = EmuConfig.GS.OsdColor;
    // Record the user's authoritative OSD choice for the overlay renderer. This snapshot
    // is immune to VMManager::ApplySettings (which re-derives EmuConfig.GS from the layered
    // settings and could otherwise resurrect an OSD the user just turned off). Every OSD
    // setter (osdShow*, osdShowAll, osdApplyFlags) funnels through here, so this always
    // reflects the last explicit choice.
    ImGuiManager::SetAndroidOSDVisibility(
        EmuConfig.GS.OsdShowFPS, EmuConfig.GS.OsdShowVPS, EmuConfig.GS.OsdShowSpeed,
        EmuConfig.GS.OsdShowResolution, EmuConfig.GS.OsdShowCPU, EmuConfig.GS.OsdShowGPU,
        EmuConfig.GS.OsdShowGSStats, EmuConfig.GS.OsdShowFrameTimes, EmuConfig.GS.OsdShowHardwareInfo,
        EmuConfig.GS.OsdShowVersion, EmuConfig.GS.OsdShowGPUStats, EmuConfig.GS.OsdShowSettings,
        EmuConfig.GS.OsdShowInputs);
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowCPU(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowCPU = enabled;
    applyOsdSetting();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowGPU(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowGPU = enabled;
    applyOsdSetting();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowFPS(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowFPS = enabled;
    applyOsdSetting();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowVPS(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowVPS = enabled;
    applyOsdSetting();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowSpeed(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowSpeed = enabled;
    applyOsdSetting();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowResolution(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowResolution = enabled;
    applyOsdSetting();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowGSStats(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowGSStats = enabled;
    applyOsdSetting();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowVersion(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowVersion = enabled;
    applyOsdSetting();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowSettings(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowSettings = enabled;
    applyOsdSetting();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowInputs(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowInputs = enabled;
    applyOsdSetting();
}

// Size of on-screen messages / performance monitors, as a percentage (25–500; 100 = normal).
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdSetScale(JNIEnv*, jclass, jfloat scale) {
    EmuConfig.GS.OsdScale = scale;
    applyOsdSetting();
}

// OSD text colour as 0xRRGGBB; 0 restores the default white. Rides applyOsdSetting()'s
// reload-immune snapshot like every other OSD setter, so VMManager::ApplySettings
// re-deriving EmuConfig.GS can't revert it mid-session.
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdSetColor(JNIEnv*, jclass, jint rgb) {
    EmuConfig.GS.OsdColor = static_cast<u32>(rgb) & 0x00FFFFFFu;
    applyOsdSetting();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowFrameTimes(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowFrameTimes = enabled;
    applyOsdSetting();
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowHardwareInfo(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowHardwareInfo = enabled;
    applyOsdSetting();
}

// Transient OSD notification messages (shader-compile popups, "settings applied",
// save-state, etc.). PCSX2 gates the whole message queue on OsdMessagesPos != None
// (ImGuiManager::DrawOSDMessages), so hiding = None, showing = the default TopLeft.
// Achievement popups use a separate NotificationPosition and are unaffected.
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowMessages(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdMessagesPos = enabled ? OsdOverlayPos::TopLeft : OsdOverlayPos::None;
    applyOsdSetting();
}

// GPU pipeline-statistics OSD line (VSI/PSI). applyOsdSetting() routes through
// MTGS::ApplySettings → GSUpdateConfig, which flips the actual pipeline-stats
// query on the device (real on Vulkan; a no-op that degrades to n/a on GLES).
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowGpuStats(JNIEnv*, jclass, jboolean enabled) {
    EmuConfig.GS.OsdShowGPUStats = enabled;
    applyOsdSetting();
}

// Master OSD toggle — flips every OSD bit we enable at first init in
// initialize() so the in-game overlay's OSD pill is a single switch.
// Writes BASE too so the state survives the next ApplySettings reload
// (live EmuConfig writes get clobbered otherwise — see the EmuConfig vs
// SettingsInterface gotcha note).
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdShowAll(JNIEnv*, jclass, jboolean enabled) {
    const bool e = enabled;
    EmuConfig.GS.OsdShowFPS = e;
    EmuConfig.GS.OsdShowSpeed = e;
    EmuConfig.GS.OsdShowResolution = e;
    EmuConfig.GS.OsdShowCPU = e;
    EmuConfig.GS.OsdShowGPU = e;
    EmuConfig.GS.OsdShowGSStats = e;
    EmuConfig.GS.OsdShowFrameTimes = e;
    EmuConfig.GS.OsdShowHardwareInfo = e;
    EmuConfig.GS.OsdShowVersion = e;
    EmuConfig.GS.OsdShowSettings = e;
    EmuConfig.GS.OsdShowInputs = e;

    Host::SetBaseBoolSettingValue("EmuCore/GS", "OsdShowFPS", e);
    Host::SetBaseBoolSettingValue("EmuCore/GS", "OsdShowSpeed", e);
    Host::SetBaseBoolSettingValue("EmuCore/GS", "OsdShowResolution", e);
    Host::SetBaseBoolSettingValue("EmuCore/GS", "OsdShowCPU", e);
    Host::SetBaseBoolSettingValue("EmuCore/GS", "OsdShowGPU", e);
    Host::SetBaseBoolSettingValue("EmuCore/GS", "OsdShowGSStats", e);
    Host::SetBaseBoolSettingValue("EmuCore/GS", "OsdShowFrameTimes", e);
    Host::SetBaseBoolSettingValue("EmuCore/GS", "OsdShowHardwareInfo", e);
    Host::SetBaseBoolSettingValue("EmuCore/GS", "OsdShowVersion", e);
    Host::SetBaseBoolSettingValue("EmuCore/GS", "OsdShowSettings", e);
    Host::SetBaseBoolSettingValue("EmuCore/GS", "OsdShowInputs", e);
    if (s_settings_interface && s_settings_interface->IsDirty())
        s_settings_interface->Save();

    applyOsdSetting();
}

// Live-only OSD flag apply — writes EmuConfig.GS.* (read per-frame by the OSD renderer) but does
// NOT persist to the settings store. The OSD on/off hotkey uses this to hide/restore the on-screen
// stats without clobbering the user's saved per-stat selection: on hide it pushes all-false, on
// show it pushes the user's saved Settings values back. Because s_settings_interface is untouched,
// the stored selection survives — fixing the "hotkey resets my chosen stats" report.
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_osdApplyFlags(JNIEnv*, jclass,
    jboolean fps, jboolean vps, jboolean speed, jboolean cpu, jboolean gpu,
    jboolean res, jboolean gsStats, jboolean frameTimes, jboolean hwInfo,
    jboolean version, jboolean settings, jboolean inputs) {
    EmuConfig.GS.OsdShowFPS = fps;
    EmuConfig.GS.OsdShowVPS = vps;
    EmuConfig.GS.OsdShowSpeed = speed;
    EmuConfig.GS.OsdShowCPU = cpu;
    EmuConfig.GS.OsdShowGPU = gpu;
    EmuConfig.GS.OsdShowResolution = res;
    EmuConfig.GS.OsdShowGSStats = gsStats;
    EmuConfig.GS.OsdShowFrameTimes = frameTimes;
    EmuConfig.GS.OsdShowHardwareInfo = hwInfo;
    EmuConfig.GS.OsdShowVersion = version;
    EmuConfig.GS.OsdShowSettings = settings;
    EmuConfig.GS.OsdShowInputs = inputs;
    applyOsdSetting();
}

// ---- Per-game settings export (upstream-style sparse game INI) ----
// Mirrors PCSX2's FullscreenUI game-settings save (FullscreenUI.cpp): the
// Kotlin side streams only the keys that differ from global into a fresh
// INISettingsInterface at gamesettings/<serial>_<CRC>.ini, then commit drops
// empty sections and deletes the file when there are no overrides — so the
// on-disk artifact is sparse and portable, exactly like the desktop UI writes.
// The running game already reflects the change live (Kotlin's applySafeLiveDelta /
// ConfigStore), so we deliberately do NOT ReloadGameSettings here: that calls
// ApplySettings (a VM park) and would reintroduce the per-tap hitch the live
// delta path exists to avoid. The INI is picked up as the game layer on the
// next boot via UpdateGameSettingsLayer.
static std::unique_ptr<INISettingsInterface> s_export_game_ini;

extern "C" JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_gameIniBeginWrite(JNIEnv*, jclass) {
    if (!VMManager::HasValidVM())
        return JNI_FALSE;
    // Disc games key per-game settings on the disc CRC; standalone ELF boots (no
    // disc) have no disc CRC, so fall back to the ELF's own CRC — matches the
    // load side in UpdateGameSettingsLayer so ELF overrides round-trip (#253).
    u32 crc = VMManager::GetDiscCRC();
    if (crc == 0)
        crc = VMManager::GetCurrentCRC();
    if (crc == 0)
        return JNI_FALSE;
    // Fresh interface (no Load) so the export is a clean regeneration of the
    // current overrides — stale keys from a previous save never linger.
    s_export_game_ini = std::make_unique<INISettingsInterface>(
        VMManager::GetGameSettingsPath(VMManager::GetDiscSerial(), crc));
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_gameIniPut(JNIEnv* env, jclass,
                                                jstring p_section, jstring p_key, jstring p_value) {
    if (!s_export_game_ini)
        return;
    const char* section = env->GetStringUTFChars(p_section, nullptr);
    const char* key = env->GetStringUTFChars(p_key, nullptr);
    const char* value = env->GetStringUTFChars(p_value, nullptr);
    // CSimpleIni is untyped string storage; the typed getters (GetBoolValue etc.)
    // parse the string back, so writing the Kotlin string repr round-trips.
    if (section && key && value)
        s_export_game_ini->SetStringValue(section, key, value);
    if (value) env->ReleaseStringUTFChars(p_value, value);
    if (key) env->ReleaseStringUTFChars(p_key, key);
    if (section) env->ReleaseStringUTFChars(p_section, section);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_gameIniCommitWrite(JNIEnv*, jclass) {
    if (!s_export_game_ini)
        return JNI_FALSE;
    Error error;
    bool ok = true;
    s_export_game_ini->RemoveEmptySections();
    if (s_export_game_ini->IsEmpty()) {
        // No per-game overrides — remove the file entirely (FullscreenUI parity).
        const std::string fn = s_export_game_ini->GetFileName();
        if (FileSystem::FileExists(fn.c_str()))
            ok = FileSystem::DeleteFilePath(fn.c_str(), &error);
    } else {
        ok = s_export_game_ini->Save(&error);
    }
    s_export_game_ini.reset();
    if (!ok)
        Console.ErrorFmt("@@ANDROID_GAMEINI@@ commit failed: {}", error.GetDescription());
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_runCodegenTests(JNIEnv*, jclass) { RunArmCodegenTests(); }
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_runPatchTests(JNIEnv*, jclass) { RunPatchTests(); }
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_runVuJitTests(JNIEnv*, jclass) { RunVuJitTests(); }
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_runEeJitTests(JNIEnv*, jclass) { RunEeJitTests(); }
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_runVifTests(JNIEnv*, jclass) { RunVifTests(); }
extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_runEeSeqTests(JNIEnv*, jclass) { RunEeSeqTests(); }

// ---------------------------------------------------------------------------
// PS2 disc serial probe via ISO9660 directory walk.
//
// Reads the Primary Volume Descriptor at LBA 16, walks the root directory
// to find SYSTEM.CNF, parses its `BOOT2 = cdrom0:\SCUS_XXX.XX;1` line, and
// returns the serial in normalized "AAAA-NNNNN" form. Used by the games-
// list scanner to attach real game IDs to entries (instead of guessing
// from filenames).
//
// Handles multiple on-disk sector layouts so .iso (DVD-style 2048-byte data
// sectors), .bin/raw CD images, and CHDs all work:
//
//   2048 / 0    plain ISO — every byte is data
//   2352 / 16   Mode 1 raw — 12 byte sync, 4 byte header, 2048 data, 288 ECC
//   2352 / 24   Mode 2 Form 1 raw — 16 sync+header, 8 subheader, 2048 data, 280 ECC
//
// We try them in order and the first one that finds a valid PVD wins. CSO/ZSO
// and GZ still fall back to filename parsing on the Kotlin side.
//
// fd ownership: consumed (closed via fclose on the wrapping FILE*),
// matching the IsBIOSFromFd contract.
// ---------------------------------------------------------------------------

namespace {
// Reader abstraction so the SYSTEM.CNF probe is independent of the
// underlying container (flat ISO/BIN via FILE*, CHD via libchdr). Each
// implementation knows how to fetch up to 2048 bytes of cooked data
// starting at a given LBA + intra-sector offset.
using DiscReader = std::function<bool(std::uint32_t lba, std::uint32_t skip, void* buf, std::size_t size)>;

// FILE*-backed reader for plain ISO (2048/0) and raw .bin (2352/16, 2352/24).
static DiscReader MakeFileReader(std::FILE* fp, std::uint32_t sectorSize,
    std::uint32_t dataOffset, std::uint64_t byteBase = 0)
{
    return [fp, sectorSize, dataOffset, byteBase](std::uint32_t lba, std::uint32_t skip, void* buf, std::size_t size) -> bool {
        const std::uint64_t off = byteBase + static_cast<std::uint64_t>(lba) * sectorSize + dataOffset + skip;
        if (std::fseek(fp, static_cast<long>(off), SEEK_SET) != 0) return false;
        return std::fread(buf, 1, size, fp) == size;
    };
}

// CHD-backed reader. Pulls hunks via chd_read and indexes into them. The
// caller owns `chd` and the cached hunk buffer (so this lambda can be
// rebuilt cheaply across layout retries).
static DiscReader MakeChdReader(chd_file* chd, std::uint32_t hunkBytes,
    std::vector<std::uint8_t>& hunkBuf, std::int64_t& cachedHunk,
    std::uint32_t sectorSize, std::uint32_t dataOffset, std::uint64_t byteBase = 0)
{
    return [chd, hunkBytes, &hunkBuf, &cachedHunk, sectorSize, dataOffset, byteBase](
               std::uint32_t lba, std::uint32_t skip, void* buf, std::size_t size) -> bool {
        std::uint64_t byte_off = byteBase + static_cast<std::uint64_t>(lba) * sectorSize + dataOffset + skip;
        auto* dst = static_cast<std::uint8_t*>(buf);
        std::size_t left = size;
        while (left > 0)
        {
            const std::int64_t hunk_id = static_cast<std::int64_t>(byte_off / hunkBytes);
            const std::uint32_t in_hunk = static_cast<std::uint32_t>(byte_off % hunkBytes);
            if (cachedHunk != hunk_id)
            {
                if (chd_read(chd, static_cast<int>(hunk_id), hunkBuf.data()) != CHDERR_NONE)
                    return false;
                cachedHunk = hunk_id;
            }
            const std::size_t avail = hunkBytes - in_hunk;
            const std::size_t want = std::min(left, avail);
            std::memcpy(dst, hunkBuf.data() + in_hunk, want);
            dst += want;
            byte_off += want;
            left -= want;
        }
        return true;
    };
}

// Read `size` bytes of disc data starting at the given LBA, walking
// sectors via the supplied reader callback. Reads can span multiple
// sectors.
static bool ReadDiscData(const DiscReader& read, std::uint32_t startLba, void* buf, std::size_t size)
{
    auto* dst = static_cast<std::uint8_t*>(buf);
    std::uint32_t lba = startLba;
    std::size_t left = size;
    std::size_t skip = 0; // first sector might be partially consumed by a prior call

    while (left > 0)
    {
        const std::size_t avail = 2048 - skip;
        const std::size_t want = std::min<std::size_t>(left, avail);
        if (!read(lba, static_cast<std::uint32_t>(skip), dst, want)) return false;
        dst += want;
        left -= want;
        lba++;
        skip = 0;
    }
    return true;
}

// Parse SYSTEM.CNF using the supplied reader. Empty return = "this layout
// didn't apply" — caller tries the next one.
static std::string ProbeSerialWithReader(const DiscReader& read)
{
    // PVD lives at LBA 16 in every ISO9660 image regardless of physical
    // sector layout.
    std::uint8_t pvd[2048];
    if (!ReadDiscData(read, 16, pvd, sizeof(pvd))) return {};
    if (pvd[0] != 1 || std::memcmp(&pvd[1], "CD001", 5) != 0) return {};

    std::uint32_t rootLba  = *reinterpret_cast<const std::uint32_t*>(&pvd[156 + 2]);
    std::uint32_t rootSize = *reinterpret_cast<const std::uint32_t*>(&pvd[156 + 10]);
    if (rootLba == 0 || rootSize == 0 || rootSize > 1024 * 1024) return {};

    std::vector<std::uint8_t> rootData(rootSize);
    if (!ReadDiscData(read, rootLba, rootData.data(), rootSize)) return {};

    std::uint32_t sysLba = 0;
    std::uint32_t sysSize = 0;
    {
        std::size_t off = 0;
        while (off + 33 < rootData.size())
        {
            std::uint8_t recLen = rootData[off];
            if (recLen == 0)
            {
                // Skip to next sector boundary in the (logical) directory.
                off = (off / 2048 + 1) * 2048;
                continue;
            }
            if (off + recLen > rootData.size()) break;

            std::uint8_t nameLen = rootData[off + 32];
            if (nameLen >= 10 && nameLen <= 12 && off + 33 + nameLen <= rootData.size())
            {
                const char* name = reinterpret_cast<const char*>(&rootData[off + 33]);
                if (strncasecmp(name, "SYSTEM.CNF", 10) == 0)
                {
                    sysLba  = *reinterpret_cast<const std::uint32_t*>(&rootData[off + 2]);
                    sysSize = *reinterpret_cast<const std::uint32_t*>(&rootData[off + 10]);
                    break;
                }
            }

            off += recLen;
        }
    }

    if (sysLba == 0 || sysSize == 0 || sysSize > 64 * 1024) return {};

    std::string contents(sysSize, '\0');
    if (!ReadDiscData(read, sysLba, contents.data(), sysSize)) return {};

    // SYSTEM.CNF format examples:
    //   PS2: BOOT2 = cdrom0:\SCUS_972.28;1
    //        VER = 1.00
    //        VMODE = NTSC
    //   PS1: BOOT = cdrom:\SLUS_007.13;1
    //        TCB = tcb=64
    //        EVENT = ev=51,b=2048,s=2048
    //        STACK = stack=801fff00
    // PS2 uses BOOT2, PS1 uses BOOT (no trailing 2). Same serial format
    // (4 letters + 3 digits + dot + 2 digits) so we share the regex.
    // Returned string is `<platform>:<serial>` so the Kotlin side knows
    // which cover repo to hit (xlenore/ps2-covers vs xlenore/psx-covers).
    const char* platform = "ps2";
    std::size_t bootPos = contents.find("BOOT2");
    if (bootPos == std::string::npos)
    {
        // Check for PS1 BOOT line. Must NOT match a substring of BOOT2 —
        // we already failed that. Rare edge: if a disc has both keys
        // (it shouldn't), BOOT2 wins, which is correct (PS2).
        bootPos = contents.find("BOOT");
        if (bootPos == std::string::npos)
            return {};
        platform = "ps1";
    }

    std::size_t lineEnd = contents.find_first_of("\r\n", bootPos);
    if (lineEnd == std::string::npos) lineEnd = contents.size();
    std::string bootLine = contents.substr(bootPos, lineEnd - bootPos);

    // icase: rare but some discs have lowercase SYSTEM.CNF. Result is
    // uppercased for cover-URL stability (xlenore/ps2-covers names files
    // SLUS-20001.jpg, all caps).
    std::regex serialRe(R"(([A-Z]{4})_([0-9]{3})\.([0-9]{2}))", std::regex::icase);
    std::smatch m;
    if (!std::regex_search(bootLine, m, serialRe)) return {};

    std::string serial = m[1].str() + "-" + m[2].str() + m[3].str();
    std::transform(serial.begin(), serial.end(), serial.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return std::string(platform) + ":" + serial;
}

template <typename ReaderFactory>
static std::string ProbeSerialWithLeadIns(std::uint32_t sectorSize, const ReaderFactory& makeReader)
{
    constexpr std::uint64_t NERO_LEAD_IN_BYTES = 150ull * 2048ull;
    const std::uint64_t leadIns[] = {
        0,
        NERO_LEAD_IN_BYTES,
        150ull * static_cast<std::uint64_t>(sectorSize),
    };

    std::uint64_t lastLeadIn = static_cast<std::uint64_t>(-1);
    for (std::uint64_t leadIn : leadIns)
    {
        if (leadIn == lastLeadIn)
            continue;
        lastLeadIn = leadIn;

        std::string serial = ProbeSerialWithReader(makeReader(leadIn));
        if (!serial.empty())
            return serial;
    }

    return {};
}

// Minimal core_file wrapper around an existing FILE*. libchdr only needs
// fsize/fread/fseek/fclose; we hand-roll them to avoid bringing in the
// emulator's heavyweight ChdCoreFileWrapper (which deals with parents and
// precaching that we don't need for a one-shot serial probe).
struct ChdProbeCoreFile
{
    core_file core{};
    std::FILE* fp = nullptr;
};

static std::uint64_t ChdProbe_FSize(core_file* f)
{
    auto* w = static_cast<ChdProbeCoreFile*>(f->argp);
    if (std::fseek(w->fp, 0, SEEK_END) != 0) return static_cast<std::uint64_t>(-1);
    long sz = std::ftell(w->fp);
    return sz < 0 ? static_cast<std::uint64_t>(-1) : static_cast<std::uint64_t>(sz);
}
static std::size_t ChdProbe_FRead(void* buf, std::size_t elm, std::size_t cnt, core_file* f)
{
    auto* w = static_cast<ChdProbeCoreFile*>(f->argp);
    return std::fread(buf, elm, cnt, w->fp);
}
static int ChdProbe_FSeek(core_file* f, std::int64_t offset, int whence)
{
    auto* w = static_cast<ChdProbeCoreFile*>(f->argp);
    return std::fseek(w->fp, static_cast<long>(offset), whence);
}
static int ChdProbe_FClose(core_file* f)
{
    auto* w = static_cast<ChdProbeCoreFile*>(f->argp);
    if (w->fp) std::fclose(w->fp);
    delete w;
    return 0;
}

// Detect "MComprHD" magic at the head of the file.
static bool IsChdMagic(std::FILE* fp)
{
    char hdr[8];
    if (std::fseek(fp, 0, SEEK_SET) != 0) return false;
    if (std::fread(hdr, 1, 8, fp) != 8) return false;
    std::fseek(fp, 0, SEEK_SET);
    return std::memcmp(hdr, "MComprHD", 8) == 0;
}

// Open a CHD on top of `fp` (ownership transferred on success — libchdr
// closes the file via the core_file's fclose). Returns null on any
// error and leaves `fp` open for the caller to close.
static chd_file* OpenChdFromFile(std::FILE* fp)
{
    auto* wrapper = new ChdProbeCoreFile();
    wrapper->fp = fp;
    wrapper->core.argp = wrapper;
    wrapper->core.fsize = ChdProbe_FSize;
    wrapper->core.fread = ChdProbe_FRead;
    wrapper->core.fseek = ChdProbe_FSeek;
    wrapper->core.fclose = ChdProbe_FClose;

    chd_file* chd = nullptr;
    chd_error err = chd_open_core_file(&wrapper->core, CHD_OPEN_READ, nullptr, &chd);
    if (err != CHDERR_NONE)
    {
        // libchdr always calls our core_file fclose on its failure paths,
        // which deletes the wrapper and closes fp. Don't double-free.
        return nullptr;
    }
    return chd;
}
} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getGameSerialFromFd(JNIEnv* env, jclass, jint fd)
{
    if (fd < 0)
        return nullptr;

    std::FILE* fp = ::fdopen(fd, "rb");
    if (!fp)
        return nullptr;

    std::string serial;

    if (IsChdMagic(fp))
    {
        // CHD path. libchdr takes ownership of fp via the core_file
        // wrapper — on success it'll be closed when chd_close runs; on
        // failure libchdr's internal cleanup also closes it. Either way
        // we must NOT fclose(fp) on this branch.
        chd_file* chd = OpenChdFromFile(fp);
        if (chd)
        {
            const chd_header* hdr = chd_get_header(chd);
            const std::uint32_t hunk_bytes = hdr->hunkbytes;
            const std::uint32_t unit_bytes = hdr->unitbytes;
            std::vector<std::uint8_t> hunk_buf(hunk_bytes);
            std::int64_t cached_hunk = -1;

            // CHD frames are `unit_bytes` long and the 2048 bytes of
            // cooked data sits somewhere inside each frame. chdman packs
            // PS2 DVD ISOs as 2448-byte units with a +24 offset (per the
            // pattern emucore's ChdFileReader/InputIsoFile uses). PS2 CDs
            // use 2448 + 16 (Mode 1) or +24 (Mode 2 Form 1). Some DVD
            // CHDs also come through as 2048-byte units with 0 offset.
            // Try every plausible offset in the actual unit size — these
            // are cheap (a couple of hunk reads) and the first match
            // wins.
            auto tryLayout = [&](std::uint32_t sectorSize, std::uint32_t dataOffset) {
                if (!serial.empty()) return;
                serial = ProbeSerialWithLeadIns(sectorSize, [&](std::uint64_t byteBase) {
                    cached_hunk = -1; // forget the previous attempt's hunk
                    return MakeChdReader(chd, hunk_bytes, hunk_buf, cached_hunk,
                        sectorSize, dataOffset, byteBase);
                });
            };

            tryLayout(unit_bytes, 0);
            tryLayout(unit_bytes, 16);
            tryLayout(unit_bytes, 24);
            // Fallbacks for CHDs whose unit_bytes doesn't match the
            // canonical layouts (defensive — shouldn't normally hit).
            if (unit_bytes != 2048) tryLayout(2048, 0);
            if (unit_bytes != 2352)
            {
                tryLayout(2352, 16);
                tryLayout(2352, 24);
            }

            chd_close(chd); // closes the wrapped core_file (and thus fp)
        }
        else
        {
            // libchdr's cleanup path already closed fp via fclose on the
            // wrapper. Don't double-close.
        }
    }
    else
    {
        // Plain ISO / raw .bin path. .iso files are virtually always
        // 2048/0; .bin files are usually 2352/16 (Mode 1 raw); 2352/24
        // (Mode 2 Form 1) is rare on PS2 but cheap to try as a last
        // resort. Try PCSX2's 150-sector/Nero-style lead-in variants too,
        // since some CD-format games otherwise hide their PVD from the
        // lightweight scanner.
        if (serial.empty()) serial = ProbeSerialWithLeadIns(2048, [&](std::uint64_t byteBase) {
            return MakeFileReader(fp, 2048, 0, byteBase);
        });
        if (serial.empty()) serial = ProbeSerialWithLeadIns(2352, [&](std::uint64_t byteBase) {
            return MakeFileReader(fp, 2352, 16, byteBase);
        });
        if (serial.empty()) serial = ProbeSerialWithLeadIns(2352, [&](std::uint64_t byteBase) {
            return MakeFileReader(fp, 2352, 24, byteBase);
        });
        if (serial.empty()) serial = ProbeSerialWithLeadIns(2448, [&](std::uint64_t byteBase) {
            return MakeFileReader(fp, 2448, 24, byteBase);
        });
        std::fclose(fp);
    }

    if (serial.empty()) return nullptr;
    return env->NewStringUTF(serial.c_str());
}

// ---------------------------------------------------------------------------
// Compatibility lookup — given a normalized serial like "SLUS-20312", asks
// the bundled PCSX2 game database for the title's compatibility rating.
// Returns one of the GameDatabaseSchema::Compatibility enum values:
//   0 Unknown, 1 Nothing, 2 Intro, 3 Menu, 4 InGame, 5 Playable, 6 Perfect
// Mapping to the games-list star display happens on the Kotlin side.
// ---------------------------------------------------------------------------
extern "C" JNIEXPORT jint JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getCompatibilityForSerial(JNIEnv* env, jclass, jstring jSerial)
{
    if (!jSerial) return 0;
    const std::string serial = GetJavaString(env, jSerial);
    if (serial.empty()) return 0;

    const GameDatabaseSchema::GameEntry* db_entry = GameDatabase::findGame(serial);
    if (!db_entry) return 0;
    return static_cast<jint>(db_entry->compat);
}

// The GameDB's curated region string for a serial (e.g. "NTSC-U", "PAL-E", "PAL-IN",
// "NTSC-C", "NTSC-K", "NTSC-HK"), or "" if not in the database. Lets the library show
// the TRUE region (India, China, Korea, Hong Kong…) that a serial PREFIX can't tell
// apart — e.g. SCES-55670 "Don 2" is PAL-IN (India), not generic PAL/Europe.
extern "C" JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getRegionForSerial(JNIEnv* env, jclass, jstring jSerial)
{
    if (!jSerial) return env->NewStringUTF("");
    const std::string serial = GetJavaString(env, jSerial);
    if (serial.empty()) return env->NewStringUTF("");

    const GameDatabaseSchema::GameEntry* db_entry = GameDatabase::findGame(serial);
    if (!db_entry) return env->NewStringUTF("");
    return env->NewStringUTF(db_entry->region.c_str());
}

// The GameDB's curated titles for a serial: "<name>\n<name-sort>\n<name-en>", or "" when
// the serial isn't in the database. Any of the three may be empty — only `name` is
// guaranteed for an entry that exists.
//
// One call rather than three getters because the library asks per game while scanning, and
// this way each game costs a single findGame() lookup. '\n' separates because a PS2 title
// can contain very nearly anything else (the DB is full of '~', '-', ':', '[', ',') but
// never a newline.
//
// This is what shows a Japanese game under its Japanese name: for NTSC-J entries `name` is
// the original title, `name-sort` its kana reading (for sorting), and `name-en` the
// romanised one. Mirrors GameList.cpp, which fills title/title_sort/title_en the same way
// and only falls back to the filename when the serial isn't found.
extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getTitlesForSerial(JNIEnv* env, jclass, jstring jSerial)
{
    if (!jSerial) return env->NewStringUTF("");
    const std::string serial = GetJavaString(env, jSerial);
    if (serial.empty()) return env->NewStringUTF("");

    const GameDatabaseSchema::GameEntry* db_entry = GameDatabase::findGame(serial);
    if (!db_entry) return env->NewStringUTF("");

    std::string out = db_entry->name;
    out += '\n';
    out += db_entry->name_sort;
    out += '\n';
    out += db_entry->name_en;
    return env->NewStringUTF(out.c_str());
}

// ---------------------------------------------------------------------------
// BIOS info probe — invoked from the setup wizard while the user is picking
// a BIOS directory. Takes ownership of `fd` (the caller MUST have detached
// it from any ParcelFileDescriptor before passing it here). Returns a
// com.armsx2.BiosInfo on success, null if the file isn't a valid PS2 BIOS
// or any read step fails.
// ---------------------------------------------------------------------------
extern "C" JNIEXPORT jobject JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getBiosInfoFromFd(JNIEnv* env, jclass, jint fd)
{
    u32 version = 0;
    u32 region = 0;
    std::string description;
    std::string zone;

    // IsBIOSFromFd consumes the fd (closes via fclose on the wrapping FILE*).
    // If parsing fails it still closes the fd, so no leak path on either branch.
    if (!IsBIOSFromFd(static_cast<int>(fd), version, description, region, zone))
        return nullptr;

    jclass biosCls = env->FindClass("com/armsx2/BiosInfo");
    if (!biosCls)
        return nullptr;

    jmethodID ctor = env->GetMethodID(biosCls, "<init>",
        "(IILjava/lang/String;Ljava/lang/String;)V");
    if (!ctor)
    {
        env->DeleteLocalRef(biosCls);
        return nullptr;
    }

    jstring jdesc = env->NewStringUTF(description.c_str());
    jstring jzone = env->NewStringUTF(zone.c_str());
    jobject obj = env->NewObject(biosCls, ctor, static_cast<jint>(version),
        static_cast<jint>(region), jdesc, jzone);

    env->DeleteLocalRef(jdesc);
    env->DeleteLocalRef(jzone);
    env->DeleteLocalRef(biosCls);
    return obj;
}

void Native::vmSetPaused(bool paused) {
    if (!s_jvm || !s_NativeApp_class || !s_vmSetPaused_mid) return;

    JNIEnv* env = nullptr;
    bool attached = false;
    const int status = s_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if (s_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    } else if (status != JNI_OK) {
        return;
    }

    env->CallStaticVoidMethod(s_NativeApp_class, s_vmSetPaused_mid, static_cast<jboolean>(paused));

    if (attached) s_jvm->DetachCurrentThread();
}

void Native::onPadRumble(int pad, int largeMotor, int smallMotor) {
    if (!s_jvm || !s_NativeApp_class || !s_onPadRumble_mid) return;

    JNIEnv* env = nullptr;
    bool attached = false;
    const int status = s_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if (s_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    } else if (status != JNI_OK) {
        return;
    }

    env->CallStaticVoidMethod(s_NativeApp_class, s_onPadRumble_mid,
                              static_cast<jint>(pad), static_cast<jint>(largeMotor),
                              static_cast<jint>(smallMotor));

    if (attached) s_jvm->DetachCurrentThread();
}

// Android implementation of the cross-platform sound helper. Used by the
// RetroAchievements code to play unlock / info / leaderboard-submit .wav files
// (LnxMisc.cpp's aplay/gstreamer path is a no-op on Android). Bridges to
// NativeApp.playSound(String), which plays via a SoundPool. Fire-and-forget.
bool Common::PlaySoundAsync(const char* path)
{
    if (!s_jvm || !s_NativeApp_class || !s_playSound_mid || !path)
        return false;

    JNIEnv* env = nullptr;
    bool attached = false;
    const int status = s_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if (s_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
        attached = true;
    } else if (status != JNI_OK) {
        return false;
    }

    jstring jpath = env->NewStringUTF(path);
    env->CallStaticVoidMethod(s_NativeApp_class, s_playSound_mid, jpath);
    if (jpath) env->DeleteLocalRef(jpath);

    if (attached) s_jvm->DetachCurrentThread();
    return true;
}

// Reads the tweakable parameters a .slangp preset exposes, as JSON:
//   [{"name":..,"description":..,"initial":f,"minimum":f,"maximum":f,"step":f}, ...]
// Returns null when librashader isn't built in or the preset won't load; "[]" for a preset
// with no parameters.
//
// This LOADS ITS OWN preset and frees it. It must not reuse the renderer's: creating a
// filter chain consumes the preset handle outright, so there is nothing left to enumerate
// afterwards. Enumeration is also pure file parsing — no VkDevice, no GL context — which is
// why it lives here rather than on GSDevice, and why it is safe to call from the UI thread
// with no game running.
//
// The author's own name/description/min/max/step come straight from the preset, so the UI
// renders what the shader declares rather than anything we invent per-shader.
extern "C" JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_shaderPresetParams(JNIEnv* env, jclass, jstring jpath)
{
#ifndef ARMSX2_HAS_LIBRASHADER
	return nullptr;
#else
	if (!jpath)
		return nullptr;

	const char* path = env->GetStringUTFChars(jpath, nullptr);
	if (!path)
		return nullptr;

	libra_shader_preset_t preset = nullptr;
	libra_error_t err = libra_preset_create(path, &preset);
	env->ReleaseStringUTFChars(jpath, path);
	if (err)
	{
		libra_error_free(&err);
		return nullptr;
	}

	libra_preset_param_list_t params = {};
	err = libra_preset_get_runtime_params(&preset, &params);
	if (err)
	{
		libra_error_free(&err);
		libra_preset_free(&preset);
		return nullptr;
	}

	// Hand-built JSON: escaping only needs to cover what a shader author can put in a name
	// or description. Everything else is a float we format ourselves.
	const auto escape = [](const char* s) {
		std::string out;
		if (!s)
			return out;
		for (const char* p = s; *p; ++p)
		{
			switch (*p)
			{
				case '"':  out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				case '\t': out += "\\t"; break;
				default:
					// Control characters would produce invalid JSON; drop them.
					if (static_cast<unsigned char>(*p) >= 0x20)
						out += *p;
					break;
			}
		}
		return out;
	};

	std::string json("[");
	for (uint64_t i = 0; i < params.length; i++)
	{
		const libra_preset_param_t& p = params.parameters[i];
		if (i)
			json += ',';
		json += StringUtil::StdStringFromFormat(
			"{\"name\":\"%s\",\"description\":\"%s\",\"initial\":%g,\"minimum\":%g,\"maximum\":%g,\"step\":%g}",
			escape(p.name).c_str(), escape(p.description).c_str(),
			p.initial, p.minimum, p.maximum, p.step);
	}
	json += ']';

	// free_runtime_params takes the list BY VALUE, and the preset is still ours to free —
	// unlike filter_chain_create, get_runtime_params does not consume it.
	libra_preset_free_runtime_params(params);
	libra_preset_free(&preset);

	return env->NewStringUTF(json.c_str());
#endif
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setShaderChainParams(
    JNIEnv* env, jclass, jstring jpreset, jobjectArray jnames, jfloatArray jvalues)
{
    // Deliberately NOT behind ARMSX2_HAS_LIBRASHADER: the store is plain values, and the
    // consumer end is already stubbed out in a build without librashader. Guarding here
    // would only add a second way for the feature to vanish silently.
    const std::vector<std::string> names = jStringArrayToVector(env, jnames);

    std::vector<std::pair<std::string, float>> params;
    if (jvalues)
    {
        // Parallel arrays: trust neither length, take the shorter. A mismatch is a caller
        // bug, but reading past either end is a crash.
        const jsize count = std::min(static_cast<jsize>(names.size()), env->GetArrayLength(jvalues));
        if (jfloat* values = env->GetFloatArrayElements(jvalues, nullptr))
        {
            params.reserve(static_cast<size_t>(count));
            for (jsize i = 0; i < count; i++)
                params.emplace_back(names[static_cast<size_t>(i)], values[i]);

            // JNI_ABORT: read-only, so don't copy anything back to the Java array.
            env->ReleaseFloatArrayElements(jvalues, values, JNI_ABORT);
        }
    }

    std::string preset;
    if (jpreset)
    {
        if (const char* p = env->GetStringUTFChars(jpreset, nullptr))
        {
            preset.assign(p);
            env->ReleaseStringUTFChars(jpreset, p);
        }
    }

    GSDevice::SetShaderChainParams(std::move(preset), std::move(params));
}
