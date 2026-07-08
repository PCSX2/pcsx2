#include <jni.h>
#include <android/native_window_jni.h>
#include <unistd.h>
#include <cstdlib>
#include "PrecompiledHeader.h"
#include "common/StringUtil.h"
#include "common/FileSystem.h"
#include "common/ZipHelpers.h"
#include "common/Error.h"
#include "pcsx2/GS.h"
#include "pcsx2/VMManager.h"
#include "pcsx2/Config.h"
#include "pcsx2/Patch.h"
#include "PerformanceMetrics.h"
#include "GameList.h"
#include "GS/GSPerfMon.h"
#include "GSDumpReplayer.h"
#include "ImGui/ImGuiManager.h"
#include "common/Path.h"
#include "pcsx2/INISettingsInterface.h"
#include "SIO/Pad/Pad.h"
#include "Input/InputManager.h"
#include "ImGui/ImGuiFullscreen.h"
#include "Achievements.h"
#include "Host.h"
#include "ImGui/FullscreenUI.h"
#include "SIO/Pad/PadDualshock2.h"
#include "MTGS.h"
#include "SDL3/SDL.h"
#include <future>
#include <memory>
#include <fstream>
#include <algorithm>
#include <mutex>

namespace
{
	static jclass s_native_app_class = nullptr;
	static jmethodID s_on_pad_vibration = nullptr;
    static jmethodID s_native_ensure_resource_dir = nullptr;

	static jclass s_ra_bridge_class = nullptr;
	static jmethodID s_ra_notify_login_requested = nullptr;
	static jmethodID s_ra_notify_login_success = nullptr;
	static jmethodID s_ra_notify_state_changed = nullptr;
	static jmethodID s_ra_notify_hardcore_changed = nullptr;
        static std::mutex s_ra_bridge_mutex;

	static void EnsureAchievementsClientInitialized()
	{
        if (!EmuConfig.Achievements.Enabled)
            return;

        if (!Achievements::IsActive())
            Achievements::Initialize();
	}

	static void ClearJNIExceptions(JNIEnv* env)
	{
		if (env && env->ExceptionCheck())
		{
			env->ExceptionDescribe();
			env->ExceptionClear();
		}
	}

    static bool EnsureNativeAppMethods(JNIEnv* env)
    {
        if (!env)
            return false;

        if (!s_native_app_class)
        {
            jclass local = env->FindClass("kr/co/iefriends/pcsx2/NativeApp");
            if (!local)
            {
                ClearJNIExceptions(env);
                return false;
            }

            s_native_app_class = reinterpret_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
            if (!s_native_app_class)
            {
                ClearJNIExceptions(env);
                return false;
            }
        }

        if (!s_on_pad_vibration)
            s_on_pad_vibration = env->GetStaticMethodID(s_native_app_class, "onPadVibration", "(IFF)V");

        if (!s_native_ensure_resource_dir)
            s_native_ensure_resource_dir = env->GetStaticMethodID(
                s_native_app_class, "ensureResourceSubdirectoryCopied", "(Ljava/lang/String;)V");

        ClearJNIExceptions(env);
        return s_native_app_class != nullptr;
    }

	static bool EnsureRetroAchievementsBridge(JNIEnv* env)
	{
		if (!env)
			return false;

        std::lock_guard<std::mutex> lock(s_ra_bridge_mutex);
        if (!s_ra_bridge_class)
		{
			jclass local = env->FindClass("kr/co/iefriends/pcsx2/utils/RetroAchievementsBridge");
			if (!local)
				return false;

			s_ra_bridge_class = reinterpret_cast<jclass>(env->NewGlobalRef(local));
			env->DeleteLocalRef(local);
			if (!s_ra_bridge_class)
				return false;

			s_ra_notify_login_requested =
				env->GetStaticMethodID(s_ra_bridge_class, "notifyLoginRequested", "(I)V");
			s_ra_notify_login_success =
				env->GetStaticMethodID(s_ra_bridge_class, "notifyLoginSuccess", "(Ljava/lang/String;III)V");
			s_ra_notify_state_changed = env->GetStaticMethodID(
				s_ra_bridge_class, "notifyStateChanged",
				"(ZZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;IIIZZZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;IIIIIZZZ)V");
			s_ra_notify_hardcore_changed =
				env->GetStaticMethodID(s_ra_bridge_class, "notifyHardcoreModeChanged", "(Z)V");

			if (!s_ra_notify_login_requested || !s_ra_notify_login_success || !s_ra_notify_state_changed ||
				!s_ra_notify_hardcore_changed)
			{
				return false;
			}
		}

		return true;
	}
	static void NotifyRetroAchievementsState()
	{
        auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        if (!env)
            return;
		if (!EnsureRetroAchievementsBridge(env))
			return;

		Achievements::UserStats user_stats;
		Achievements::GameStats game_stats;
		const bool have_user = Achievements::GetCurrentUserStats(&user_stats);
		const bool have_game = Achievements::GetCurrentGameStats(&game_stats);

		const bool achievements_enabled = EmuConfig.Achievements.Enabled;
		const bool hardcore_preference = EmuConfig.Achievements.HardcoreMode;
		const bool hardcore_active = Achievements::IsHardcoreModeActive();

		jstring j_username = have_user ? env->NewStringUTF(user_stats.username.c_str()) : nullptr;
		jstring j_display_name = have_user ? env->NewStringUTF(user_stats.display_name.c_str()) : nullptr;
		jstring j_avatar = (have_user && !user_stats.avatar_path.empty()) ?
			env->NewStringUTF(user_stats.avatar_path.c_str()) :
			nullptr;
		jstring j_game_title = have_game ? env->NewStringUTF(game_stats.title.c_str()) : nullptr;
		jstring j_rich_presence = (have_game && !game_stats.rich_presence.empty()) ?
			env->NewStringUTF(game_stats.rich_presence.c_str()) :
			nullptr;
		jstring j_icon_path = (have_game && !game_stats.icon_path.empty()) ?
			env->NewStringUTF(game_stats.icon_path.c_str()) :
			nullptr;

		env->CallStaticVoidMethod(
			s_ra_bridge_class, s_ra_notify_state_changed,
			achievements_enabled ? JNI_TRUE : JNI_FALSE,
			have_user ? JNI_TRUE : JNI_FALSE,
			j_username,
			j_display_name,
			j_avatar,
			static_cast<jint>(have_user ? user_stats.points : 0),
			static_cast<jint>(have_user ? user_stats.softcore_points : 0),
			static_cast<jint>(have_user ? user_stats.unread_messages : 0),
			hardcore_preference ? JNI_TRUE : JNI_FALSE,
			hardcore_active ? JNI_TRUE : JNI_FALSE,
			have_game ? JNI_TRUE : JNI_FALSE,
			j_game_title,
			j_rich_presence,
			j_icon_path,
			static_cast<jint>(have_game ? game_stats.unlocked_achievements : 0),
			static_cast<jint>(have_game ? game_stats.total_achievements : 0),
			static_cast<jint>(have_game ? game_stats.unlocked_points : 0),
			static_cast<jint>(have_game ? game_stats.total_points : 0),
			static_cast<jint>(have_game ? game_stats.game_id : 0),
			have_game && game_stats.has_achievements ? JNI_TRUE : JNI_FALSE,
			have_game && game_stats.has_leaderboards ? JNI_TRUE : JNI_FALSE,
			have_game && game_stats.has_rich_presence ? JNI_TRUE : JNI_FALSE);

		ClearJNIExceptions(env);

		if (j_username)
			env->DeleteLocalRef(j_username);
		if (j_display_name)
			env->DeleteLocalRef(j_display_name);
		if (j_avatar)
			env->DeleteLocalRef(j_avatar);
		if (j_game_title)
			env->DeleteLocalRef(j_game_title);
		if (j_rich_presence)
			env->DeleteLocalRef(j_rich_presence);
		if (j_icon_path)
			env->DeleteLocalRef(j_icon_path);
	}
} 

namespace Host::Internal
{
void EnsureAndroidResourceSubdirCopied(const char* relative_path)
{
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    if (!env)
        return;

    if (!EnsureNativeAppMethods(env) || !s_native_ensure_resource_dir)
        return;

    const char* safe_path = relative_path ? relative_path : "";
    jstring j_path = env->NewStringUTF(safe_path);
    env->CallStaticVoidMethod(s_native_app_class, s_native_ensure_resource_dir, j_path);
    if (j_path)
        env->DeleteLocalRef(j_path);
    ClearJNIExceptions(env);
}
} // namespace Host::Internal

void AndroidUpdatePadVibration(u32 pad_index, float large_intensity, float small_intensity)
{
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    if (!env)
        return;

    if (!EnsureNativeAppMethods(env) || !s_on_pad_vibration)
        return;

    env->CallStaticVoidMethod(s_native_app_class, s_on_pad_vibration, static_cast<jint>(pad_index),
                              static_cast<jfloat>(large_intensity), static_cast<jfloat>(small_intensity));
}



bool s_execute_exit;
int s_window_width = 0;
int s_window_height = 0;
ANativeWindow* s_window = nullptr;

static std::unique_ptr<INISettingsInterface> s_settings_interface;
static bool IsFullscreenUIEnabled()
{
    if (!s_settings_interface)
        return false;
    return s_settings_interface->GetBoolValue("UI", "EnableFullscreenUI", false);
}

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

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_initialize(JNIEnv *env, jclass clazz,
                                                jstring p_szpath, jint p_apiVer) {
    std::string _szPath = GetJavaString(env, p_szpath);
    EmuFolders::AppRoot = _szPath;
    EmuFolders::DataRoot = _szPath;
    EmuFolders::SetResourcesDirectory();

    Log::SetConsoleOutputLevel(LOGLEVEL_DEBUG);
    ImGuiManager::SetFontPathAndRange(Path::Combine(EmuFolders::Resources, "fonts" FS_OSPATH_SEPARATOR_STR "Roboto-Regular.ttf"), {});

    if (!s_settings_interface)
    {
        const std::string ini_path = EmuFolders::DataRoot + "/PCSX2-Android.ini";
        s_settings_interface = std::make_unique<INISettingsInterface>(ini_path);
        Host::Internal::SetBaseSettingsLayer(s_settings_interface.get());
        s_settings_interface->Load();
        if (s_settings_interface->IsEmpty())
        {
            VMManager::SetDefaultSettings(*s_settings_interface, true, true, true, true, true);
            s_settings_interface->SetBoolValue("EmuCore", "EnableDiscordPresence", true);
            s_settings_interface->SetBoolValue("EmuCore/GS", "FrameLimitEnable", false);
            s_settings_interface->SetIntValue("EmuCore/GS", "VsyncEnable", false);
            s_settings_interface->SetBoolValue("InputSources", "SDL", true);
            s_settings_interface->SetBoolValue("InputSources", "XInput", false);
            s_settings_interface->SetStringValue("SPU2/Output", "OutputModule", "nullout");
            s_settings_interface->SetBoolValue("Logging", "EnableSystemConsole", true);
            s_settings_interface->SetBoolValue("Logging", "EnableTimestamps", true);
            s_settings_interface->SetBoolValue("Logging", "EnableVerbose", false);
            s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowFPS", false);
            s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowResolution", false);
            s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowGSStats", false);
            s_settings_interface->SetIntValue("EmuCore/GS", "OsdPerformancePos", 0); 
            s_settings_interface->SetBoolValue("UI", "EnableFullscreenUI", false);
            s_settings_interface->SetBoolValue("Achievements", "Enabled", false);
            s_settings_interface->SetBoolValue("Achievements", "ChallengeMode", false);
            s_settings_interface->SetBoolValue("Achievements", "AndroidMigrationV1", true);
            s_settings_interface->Save();
        }
        else
        {
            bool needs_save = false;
            if (!s_settings_interface->GetBoolValue("Achievements", "AndroidMigrationV1", false))
            {
                if (!s_settings_interface->ContainsValue("Achievements", "Enabled"))
                    s_settings_interface->SetBoolValue("Achievements", "Enabled", false);
                s_settings_interface->SetBoolValue("Achievements", "AndroidMigrationV1", true);
                needs_save = true;
            }
            if (!s_settings_interface->ContainsValue("Achievements", "ChallengeMode"))
            {
                s_settings_interface->SetBoolValue("Achievements", "ChallengeMode", false);
                needs_save = true;
            }
            if (needs_save)
                s_settings_interface->Save();
        }
    }
    VMManager::Internal::LoadStartupSettings();
    if (s_settings_interface)
        s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowFPS", false);
    VMManager::ApplySettings();
    GSConfig.OsdPerformancePos = EmuConfig.GS.OsdPerformancePos;
    GSConfig.CustomDriverPath = EmuConfig.GS.CustomDriverPath;
    if (MTGS::IsOpen()) MTGS::ApplySettings();
    VMManager::ReloadInputSources();
    VMManager::ReloadInputBindings(true);
    EnsureAchievementsClientInitialized();
    NotifyRetroAchievementsState();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_reloadDataRoot(JNIEnv* env, jclass, jstring p_szpath)
{
    std::string new_path = GetJavaString(env, p_szpath);
    if (new_path.empty())
        return;

    EmuFolders::AppRoot = new_path;
    EmuFolders::DataRoot = new_path;
    EmuFolders::SetResourcesDirectory();

    Log::SetConsoleOutputLevel(LOGLEVEL_DEBUG);
    ImGuiManager::SetFontPathAndRange(Path::Combine(EmuFolders::Resources, "fonts" FS_OSPATH_SEPARATOR_STR "Roboto-Regular.ttf"), {});

    if (s_settings_interface)
        s_settings_interface->Save();
    s_settings_interface.reset();

    const std::string ini_path = EmuFolders::DataRoot + "/PCSX2-Android.ini";
    s_settings_interface = std::make_unique<INISettingsInterface>(ini_path);
    Host::Internal::SetBaseSettingsLayer(s_settings_interface.get());
    s_settings_interface->Load();
    if (s_settings_interface->IsEmpty())
    {
            VMManager::SetDefaultSettings(*s_settings_interface, true, true, true, true, true);
        s_settings_interface->SetBoolValue("EmuCore", "EnableDiscordPresence", true);
        s_settings_interface->SetBoolValue("EmuCore/GS", "FrameLimitEnable", false);
        s_settings_interface->SetIntValue("EmuCore/GS", "VsyncEnable", false);
        s_settings_interface->SetBoolValue("InputSources", "SDL", true);
        s_settings_interface->SetBoolValue("InputSources", "XInput", false);
        s_settings_interface->SetStringValue("SPU2/Output", "OutputModule", "nullout");
        s_settings_interface->SetBoolValue("Logging", "EnableSystemConsole", true);
        s_settings_interface->SetBoolValue("Logging", "EnableTimestamps", true);
        s_settings_interface->SetBoolValue("Logging", "EnableVerbose", false);
        s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowFPS", false);
        s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowResolution", false);
        s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowGSStats", false);
        s_settings_interface->SetIntValue("EmuCore/GS", "OsdPerformancePos", 0);
        s_settings_interface->SetBoolValue("UI", "EnableFullscreenUI", false);
        s_settings_interface->SetBoolValue("Achievements", "Enabled", false);
        s_settings_interface->SetBoolValue("Achievements", "ChallengeMode", false);
        s_settings_interface->SetBoolValue("Achievements", "AndroidMigrationV1", true);
        s_settings_interface->Save();
    }
    else
    {
        bool needs_save = false;
        if (!s_settings_interface->GetBoolValue("Achievements", "AndroidMigrationV1", false))
        {
            if (!s_settings_interface->ContainsValue("Achievements", "Enabled"))
                s_settings_interface->SetBoolValue("Achievements", "Enabled", false);
            s_settings_interface->SetBoolValue("Achievements", "AndroidMigrationV1", true);
            needs_save = true;
        }
        if (!s_settings_interface->ContainsValue("Achievements", "ChallengeMode"))
        {
            s_settings_interface->SetBoolValue("Achievements", "ChallengeMode", false);
            needs_save = true;
        }
        if (needs_save)
            s_settings_interface->Save();
    }

    VMManager::Internal::LoadStartupSettings();
    if (s_settings_interface)
        s_settings_interface->SetBoolValue("EmuCore/GS", "OsdShowFPS", false);
    VMManager::ApplySettings();
    GSConfig.OsdPerformancePos = EmuConfig.GS.OsdPerformancePos;
    GSConfig.CustomDriverPath = EmuConfig.GS.CustomDriverPath;
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
    VMManager::ReloadInputSources();
    VMManager::ReloadInputBindings(true);
    EnsureAchievementsClientInitialized();
    NotifyRetroAchievementsState();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_isFullscreenUIEnabled(JNIEnv* env, jclass clazz)
{
    return IsFullscreenUIEnabled() ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getGameTitle(JNIEnv *env, jclass clazz,
                                                  jstring p_szpath) {
    std::string _szPath = GetJavaString(env, p_szpath);

    const GameList::Entry *entry;
    entry = GameList::GetEntryForPath(_szPath.c_str());

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
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_hasWidescreenPatch(JNIEnv* env, jclass clazz)
{
	(void)env;
	(void)clazz;
	if (!VMManager::HasValidVM())
		return JNI_FALSE;
	const std::string serial = VMManager::GetDiscSerial();
	if (serial.empty())
		return JNI_FALSE;
	const u32 crc = VMManager::GetDiscCRC();
	const Patch::PatchInfoList patches = Patch::GetPatchInfo(serial, crc, false, false, nullptr);
	for (const Patch::PatchInfo& info : patches)
	{
		if (info.name == "Widescreen 16:9")
			return JNI_TRUE;
	}
	return JNI_FALSE;
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
    const bool enabled = (p_isOnOff == JNI_TRUE);

    if (s_settings_interface)
    {
        s_settings_interface->SetBoolValue("Pad1", "Vibration", enabled);
        s_settings_interface->Save();
    }

    // Reload pad configuration so vibration state updates without restart.
    if (s_settings_interface)
        Pad::LoadConfig(*s_settings_interface);
}


extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setPadButton(JNIEnv *env, jclass clazz,
                                                  jint p_key, jint p_range, jboolean p_keyPressed) {
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
        default: _key = PadDualshock2::Inputs::PAD_CROSS ; break;
    }
    float value = 0.0f;
    if (p_keyPressed) {
        if (p_range > 0) {
            int clamped = std::min(255, std::max(0, p_range));
            value = static_cast<float>(clamped) / 255.0f; 
        } else {
            value = 1.0f;
        }
    }
    Pad::SetControllerState(0, static_cast<u32>(_key), value);
}

extern "C" JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_resetKeyStatus(JNIEnv *env, jclass clazz) {
    for (u32 pad = 0; pad < Pad::NUM_CONTROLLER_PORTS; pad++)
    {
        for (u32 key = 0; key < static_cast<u32>(PadDualshock2::Inputs::LENGTH); key++)
            Pad::SetControllerState(pad, key, 0.0f);
    }

    Pad::UpdateMacroButtons();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setEnableCheats(JNIEnv *env, jclass clazz,
                                                     jboolean p_isonoff) {
    const bool enabled = (p_isonoff == JNI_TRUE);

    EmuConfig.EnableCheats = enabled;

    if (s_settings_interface)
    {
        s_settings_interface->SetBoolValue("EmuCore", "EnableCheats", enabled);
        s_settings_interface->Save();
    }

    VMManager::ApplySettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setAspectRatio(JNIEnv *env, jclass clazz,
                                                    jint p_type) {
    // p_type per AspectRatioType in pcsx2/Config.h
    // 0=Stretch, 1=Auto 4:3/3:2, 2=4:3, 3=16:9, 4=10:7
    if (p_type < 0 || p_type >= static_cast<jint>(AspectRatioType::MaxCount))
        return;
    EmuConfig.GS.AspectRatio = static_cast<AspectRatioType>(p_type);
    // Keep current aspect ratio in sync for immediate effect
    EmuConfig.CurrentAspectRatio = EmuConfig.GS.AspectRatio;
    if (s_settings_interface)
    {
        const size_t index = static_cast<size_t>(p_type);
        if (index < static_cast<size_t>(AspectRatioType::MaxCount))
        {
            const char* name = Pcsx2Config::GSOptions::AspectRatioNames[index];
            if (name && name[0] != '\0')
                s_settings_interface->SetStringValue("EmuCore/GS", "AspectRatio", name);
        }
        s_settings_interface->Save();
    }
    VMManager::ApplySettings();
    if (MTGS::IsOpen())
    {
        MTGS::ApplySettings();
        // Request a display size update to reflect aspect ratio immediately
        VMManager::RequestDisplaySize(0.0f);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_speedhackLimitermode(JNIEnv *env, jclass clazz,
                                                          jint p_value) {
    // 0=Nominal, 1=Turbo, 2=Slomo, 3=Unlimited
    LimiterModeType mode = LimiterModeType::Nominal;
    switch (p_value) {
        case 1: mode = LimiterModeType::Turbo; break;
        case 2: mode = LimiterModeType::Slomo; break;
        case 3: mode = LimiterModeType::Unlimited; break;
        default: mode = LimiterModeType::Nominal; break;
    }
    VMManager::SetLimiterMode(mode);
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_speedhackEecyclerate(JNIEnv *env, jclass clazz,
                                                          jint p_value) {
    const s32 clamped = std::clamp(static_cast<s32>(p_value),
        static_cast<s32>(Pcsx2Config::SpeedhackOptions::MIN_EE_CYCLE_RATE),
        static_cast<s32>(Pcsx2Config::SpeedhackOptions::MAX_EE_CYCLE_RATE));

    EmuConfig.Speedhacks.EECycleRate = static_cast<s8>(clamped);

    if (s_settings_interface)
    {
        s_settings_interface->SetIntValue("EmuCore/Speedhacks", "EECycleRate", clamped);
        s_settings_interface->Save();
    }

    VMManager::ApplySettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_speedhackEecycleskip(JNIEnv *env, jclass clazz,
                                                          jint p_value) {
    const s32 clamped = std::clamp(static_cast<s32>(p_value), 0,
        static_cast<s32>(Pcsx2Config::SpeedhackOptions::MAX_EE_CYCLE_SKIP));

    EmuConfig.Speedhacks.EECycleSkip = static_cast<u8>(clamped);

    if (s_settings_interface)
    {
        s_settings_interface->SetIntValue("EmuCore/Speedhacks", "EECycleSkip", clamped);
        s_settings_interface->Save();
    }

    VMManager::ApplySettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderUpscalemultiplier(JNIEnv *env, jclass clazz,
                                                             jfloat p_value) {
    const float clamped = std::clamp(p_value, 1.0f, 12.0f);

    EmuConfig.GS.UpscaleMultiplier = clamped;
    GSConfig.UpscaleMultiplier = clamped;

    if (s_settings_interface)
    {
        s_settings_interface->SetFloatValue("EmuCore/GS", "upscale_multiplier", clamped);
        s_settings_interface->Save();
    }

    VMManager::ApplySettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderMipmap(JNIEnv *env, jclass clazz,
                                                  jint p_value) {
    const bool enabled = (p_value != 0);

    EmuConfig.GS.HWMipmap = enabled;
    GSConfig.HWMipmap = enabled;

    if (s_settings_interface)
    {
        s_settings_interface->SetBoolValue("EmuCore/GS", "hw_mipmap", enabled);
        s_settings_interface->Save();
    }

    VMManager::ApplySettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderHalfpixeloffset(JNIEnv *env, jclass clazz,
                                                           jint p_value) {
    const s32 min_value = 0;
    const s32 max_value = static_cast<s32>(GSHalfPixelOffset::MaxCount) - 1;
    const s32 clamped = std::clamp(static_cast<s32>(p_value), min_value, max_value);

    EmuConfig.GS.UserHacks_HalfPixelOffset = static_cast<GSHalfPixelOffset>(clamped);
    GSConfig.UserHacks_HalfPixelOffset = static_cast<GSHalfPixelOffset>(clamped);

    if (s_settings_interface)
    {
        s_settings_interface->SetIntValue("EmuCore/GS", "UserHacks_HalfPixelOffset", clamped);
        s_settings_interface->Save();
    }

    VMManager::ApplySettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderPreloading(JNIEnv *env, jclass clazz,
                                                      jint p_value) {
    const s32 min_value = 0;
    const s32 max_value = static_cast<s32>(TexturePreloadingLevel::Full);
    const s32 clamped = std::clamp(static_cast<s32>(p_value), min_value, max_value);
    const TexturePreloadingLevel level = static_cast<TexturePreloadingLevel>(clamped);

    EmuConfig.GS.TexturePreloading = level;
    GSConfig.TexturePreloading = level;

    if (s_settings_interface)
    {
        s_settings_interface->SetIntValue("EmuCore/GS", "texture_preloading", clamped);
        s_settings_interface->Save();
    }

    VMManager::ApplySettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_renderGpu(JNIEnv *env, jclass clazz,
                                               jint p_value) {
    EmuConfig.GS.Renderer = static_cast<GSRendererType>(p_value);
    GSConfig.Renderer = static_cast<GSRendererType>(p_value);

    VMManager::ApplySettings();
    if (MTGS::IsOpen())
        MTGS::ApplySettings();
    if (s_settings_interface)
    {
        s_settings_interface->SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(p_value));
        s_settings_interface->Save();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setCustomDriverPath(JNIEnv *env, jclass clazz,
                                                          jstring p_path) {
    std::string driver_path = GetJavaString(env, p_path);
    
    // Store old config to check if restart is needed
    Pcsx2Config::GSOptions old_gs_config = GSConfig;
    std::string old_emu_config_path = EmuConfig.GS.CustomDriverPath;

    EmuConfig.GS.CustomDriverPath = driver_path;
    GSConfig.CustomDriverPath = driver_path;

    if (s_settings_interface)
    {
        if (driver_path.empty())
            s_settings_interface->DeleteValue("EmuCore/GS", "CustomDriverPath");
        else
            s_settings_interface->SetStringValue("EmuCore/GS", "CustomDriverPath", driver_path.c_str());
        s_settings_interface->Save();
    }
    
    // If graphics device is already initialized and driver path changed, trigger restart
    if (old_gs_config.CustomDriverPath != driver_path || old_emu_config_path != driver_path)
    {
        // Ensure GSConfig matches EmuConfig (they should already match, but be explicit)
        GSConfig.CustomDriverPath = EmuConfig.GS.CustomDriverPath;
        
        // Trigger graphics restart if device is already open
        if (MTGS::IsOpen())
        {
            MTGS::ApplySettings();
        }
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getCustomDriverPath(JNIEnv *env, jclass clazz) {
    std::string driver_path;
    if (s_settings_interface)
    {
        s_settings_interface->GetStringValue("EmuCore/GS", "CustomDriverPath", &driver_path);
        EmuConfig.GS.CustomDriverPath = driver_path;
        GSConfig.CustomDriverPath = driver_path;
    }
    else
    {
        driver_path = GSConfig.CustomDriverPath;
    }
    
    return env->NewStringUTF(driver_path.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setNativeLibraryDir(JNIEnv *env, jclass clazz,
                                                         jstring p_path) {
    std::string native_lib_dir = GetJavaString(env, p_path);
    if (!native_lib_dir.empty())
    {
        // Set env variable for libadrenotools to use
        setenv("ANDROID_NATIVE_LIB_DIR", native_lib_dir.c_str(), 1);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_setSetting(JNIEnv* env, jclass, jstring j_section, jstring j_key, jstring j_type, jstring j_value)
{
    const std::string section = GetJavaString(env, j_section);
    const std::string key = GetJavaString(env, j_key);
    const std::string type = GetJavaString(env, j_type);
    const std::string value = GetJavaString(env, j_value);

    if (!s_settings_interface)
        return; 
    INISettingsInterface& si = *s_settings_interface;

    if (type == "bool")
    {
        const bool b = (value == "1" || value == "true" || value == "TRUE" || value == "True");
        si.SetBoolValue(section.c_str(), key.c_str(), b);
    }
    else if (type == "int")
    {
        si.SetIntValue(section.c_str(), key.c_str(), static_cast<s32>(std::strtol(value.c_str(), nullptr, 10)));
    }
    else if (type == "uint")
    {
        si.SetUIntValue(section.c_str(), key.c_str(), static_cast<u32>(std::strtoul(value.c_str(), nullptr, 10)));
    }
    else if (type == "float")
    {
        si.SetFloatValue(section.c_str(), key.c_str(), std::strtof(value.c_str(), nullptr));
    }
    else if (type == "double")
    {
        si.SetDoubleValue(section.c_str(), key.c_str(), std::strtod(value.c_str(), nullptr));
    }
    else // string
    {
        si.SetStringValue(section.c_str(), key.c_str(), value.c_str());
    }

    // Apply live where it makes sense
    VMManager::ApplySettings();
    if (MTGS::IsOpen()) {
        MTGS::ApplySettings();
    }
    si.Save();
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_getSetting(JNIEnv* env, jclass, jstring j_section, jstring j_key, jstring j_type)
{
    const std::string section = GetJavaString(env, j_section);
    const std::string key = GetJavaString(env, j_key);
    const std::string type = GetJavaString(env, j_type);

    if (type == "bool")
    {
        bool v = false;
        if (s_settings_interface)
            s_settings_interface->GetBoolValue(section.c_str(), key.c_str(), &v);
        return env->NewStringUTF(v ? "true" : "false");
    }
    else if (type == "int")
    {
        s32 v = 0;
        if (s_settings_interface)
            s_settings_interface->GetIntValue(section.c_str(), key.c_str(), &v);
        return env->NewStringUTF(StringUtil::StdStringFromFormat("%d", v).c_str());
    }
    else if (type == "uint")
    {
        u32 v = 0;
        if (s_settings_interface)
            s_settings_interface->GetUIntValue(section.c_str(), key.c_str(), &v);
        return env->NewStringUTF(StringUtil::StdStringFromFormat("%u", v).c_str());
    }
    else if (type == "float")
    {
        float v = 0.0f;
        if (s_settings_interface)
            s_settings_interface->GetFloatValue(section.c_str(), key.c_str(), &v);
        return env->NewStringUTF(StringUtil::StdStringFromFormat("%g", v).c_str());
    }
    else if (type == "double")
    {
        double v = 0.0;
        if (s_settings_interface)
            s_settings_interface->GetDoubleValue(section.c_str(), key.c_str(), &v);
        return env->NewStringUTF(StringUtil::StdStringFromFormat("%g", v).c_str());
    }
    else 
    {
        std::string v;
        if (s_settings_interface)
            s_settings_interface->GetStringValue(section.c_str(), key.c_str(), &v);
        return env->NewStringUTF(v.c_str());
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_onNativeSurfaceCreated(JNIEnv *env, jclass clazz) {
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_onNativeSurfaceChanged(JNIEnv *env, jclass clazz,
                                                            jobject p_surface, jint p_width, jint p_height) {
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
        if(MTGS::IsOpen()) {
            MTGS::UpdateDisplayWindow();
        }
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_onNativeSurfaceDestroyed(JNIEnv *env, jclass clazz) {
    if(s_window) {
        ANativeWindow_release(s_window);
        s_window = nullptr;
    }
}


std::optional<WindowInfo> Host::AcquireRenderWindow(bool recreate_window)
{
    float _fScale = 1.0;
    if (s_window_width > 0 && s_window_height > 0) {
        int _nSize = s_window_width;
        if (s_window_width <= s_window_height) {
            _nSize = s_window_height;
        }
        _fScale = (float)_nSize / 800.0f;
    }
    ////
    WindowInfo _windowInfo;
    memset(&_windowInfo, 0, sizeof(_windowInfo));
    _windowInfo.type = WindowInfo::Type::Android;
    _windowInfo.surface_width = s_window_width;
    _windowInfo.surface_height = s_window_height;
    _windowInfo.surface_scale = _fScale;
    _windowInfo.window_handle = s_window;

    return _windowInfo;
}

void Host::ReleaseRenderWindow() {

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

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_utils_DiscordBridge_nativeConfigure(JNIEnv* env, jclass, jlong application_id,
                                                         jstring scheme, jstring display_name, jstring image_key)
{
    VMManager::AndroidDiscordConfigure(static_cast<uint64_t>(application_id), GetJavaString(env, scheme),
        GetJavaString(env, display_name), GetJavaString(env, image_key));
    VMManager::InitializeDiscordPresence();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_utils_DiscordBridge_nativeProvideStoredToken(JNIEnv* env, jclass,
                                                                  jstring access_token, jstring refresh_token,
                                                                  jstring token_type, jlong expires_at,
                                                                  jstring scope)
{
    VMManager::AndroidDiscordProvideStoredToken(GetJavaString(env, access_token), GetJavaString(env, refresh_token),
        GetJavaString(env, token_type), static_cast<int64_t>(expires_at), GetJavaString(env, scope));
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_utils_DiscordBridge_nativeBeginAuthorize(JNIEnv*, jclass)
{
    VMManager::AndroidDiscordBeginAuthorize();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_utils_DiscordBridge_nativeSetAppForeground(JNIEnv*, jclass, jboolean is_foreground)
{
    VMManager::AndroidDiscordSetAppForeground(is_foreground == JNI_TRUE);
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_utils_DiscordBridge_nativePollCallbacks(JNIEnv*, jclass)
{
    VMManager::PollDiscordPresence();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_utils_DiscordBridge_nativeClearTokens(JNIEnv*, jclass)
{
    VMManager::AndroidDiscordClearTokens();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_utils_DiscordBridge_nativeIsLoggedIn(JNIEnv*, jclass)
{
    return VMManager::AndroidDiscordIsLoggedIn() ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_utils_DiscordBridge_nativeIsClientReady(JNIEnv*, jclass)
{
    return VMManager::AndroidDiscordIsClientReady() ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_utils_DiscordBridge_nativeConsumeLastError(JNIEnv* env, jclass)
{
    const std::string error = VMManager::AndroidDiscordConsumeLastError();
    if (error.empty())
        return nullptr;
    return env->NewStringUTF(error.c_str());
}



extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_runVMThread(JNIEnv *env, jclass clazz,
                                                 jstring p_szpath) {
    std::string _szPath = GetJavaString(env, p_szpath);

    /////////////////////////////

    s_execute_exit = false;

//    const char* error;
//    if (!VMManager::PerformEarlyHardwareChecks(&error)) {
//        return false;
//    }

    // fast_boot : (false: bios->game, true: direct-to-game)
    VMBootParameters boot_params;
    boot_params.filename = _szPath;
    if (!_szPath.empty())
        boot_params.fast_boot = true;
    else
        boot_params.fast_boot = false;

    if (!VMManager::Internal::CPUThreadInitialize()) {
        VMManager::Internal::CPUThreadShutdown();
    }

    VMManager::ApplySettings();
    GSDumpReplayer::SetIsDumpRunner(false);

    if (VMManager::Initialize(boot_params))
    {
        VMState _vmState = VMState::Running;
        VMManager::SetState(_vmState);
        ////
        while (true) {
            _vmState = VMManager::GetState();
            if (_vmState == VMState::Stopping || _vmState == VMState::Shutdown) {
                break;
            } else if (_vmState == VMState::Running) {
                s_execute_exit = false;
                VMManager::Execute();
                s_execute_exit = true;
            } else {
                usleep(250000);
            }
        }
        ////
        VMManager::Shutdown(false);
    }
    ////
    VMManager::Internal::CPUThreadShutdown();

    return true;
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_pause(JNIEnv *env, jclass clazz) {
    std::thread([] {
        VMManager::SetPaused(true);
    }).detach();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_resume(JNIEnv *env, jclass clazz) {
    std::thread([] {
        VMManager::SetPaused(false);
    }).detach();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_shutdown(JNIEnv *env, jclass clazz) {
    std::thread([] {
        VMManager::SetState(VMState::Stopping);
    }).detach();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_refreshBIOS(JNIEnv* env, jclass clazz)
{
    // No-op placeholder for now; implement BIOS refresh if needed later.
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_hasValidVm(JNIEnv*, jclass)
{
    return VMManager::HasValidVM() ? JNI_TRUE : JNI_FALSE;
}


extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_saveStateToSlot(JNIEnv *env, jclass clazz, jint p_slot) {
    if (!VMManager::HasValidVM()) {
        return false;
    }

    std::future<bool> ret = std::async([p_slot]
    {
       if(VMManager::GetDiscCRC() != 0) {
           if(VMManager::GetState() != VMState::Paused) {
               VMManager::SetPaused(true);
           }

           // wait 5 sec
           for (int i = 0; i < 5; ++i) {
               if (s_execute_exit) {
                   if(VMManager::SaveStateToSlot(p_slot, false)) {
                       return true;
                   }
                   break;
               }
               sleep(1);
           }
       }
       return false;

    });

    return ret.get();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_kr_co_iefriends_pcsx2_NativeApp_loadStateFromSlot(JNIEnv *env, jclass clazz, jint p_slot) {
    if (!VMManager::HasValidVM()) {
        return false;
    }

    std::future<bool> ret = std::async([p_slot]
    {
       u32 _crc = VMManager::GetDiscCRC();
       if(_crc != 0) {
           if (VMManager::HasSaveStateInSlot(VMManager::GetDiscSerial().c_str(), _crc, p_slot)) {
               if(VMManager::GetState() != VMState::Paused) {
                   VMManager::SetPaused(true);
               }

               // wait 5 sec
               for (int i = 0; i < 5; ++i) {
                   if (s_execute_exit) {
                       if(VMManager::LoadStateFromSlot(p_slot)) {
                           return true;
                       }
                       break;
                   }
                   sleep(1);
               }
           }
       }
       return false;
    });

    return ret.get();
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
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_utils_RetroAchievementsBridge_nativeRequestState(JNIEnv* env, jclass)
{
    (void)env;
    NotifyRetroAchievementsState();
}

extern "C"
JNIEXPORT jstring JNICALL
Java_kr_co_iefriends_pcsx2_utils_RetroAchievementsBridge_nativeLogin(JNIEnv* env, jclass, jstring j_username, jstring j_password)
{
    std::string username = GetJavaString(env, j_username);
    std::string password = GetJavaString(env, j_password);

    if (username.empty() || password.empty())
        return env->NewStringUTF("Username and password are required.");

    EnsureAchievementsClientInitialized();

    Error error;
    if (!Achievements::Login(username.c_str(), password.c_str(), &error))
    {
        if (error.IsValid())
            return env->NewStringUTF(error.GetDescription().c_str());
        return env->NewStringUTF("Login failed.");
    }

    NotifyRetroAchievementsState();
    return nullptr;
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_utils_RetroAchievementsBridge_nativeLogout(JNIEnv* env, jclass)
{
    (void)env;
    Achievements::Logout();
    NotifyRetroAchievementsState();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_utils_RetroAchievementsBridge_nativeSetEnabled(JNIEnv* env, jclass, jboolean enabled)
{
    (void)env;
    const bool enable = (enabled == JNI_TRUE);

    if (EmuConfig.Achievements.Enabled == enable)
    {
        Host::SetBaseBoolSettingValue("Achievements", "Enabled", enable);
        Host::CommitBaseSettingChanges();
        NotifyRetroAchievementsState();
        return;
    }

    Pcsx2Config::AchievementsOptions old_config = EmuConfig.Achievements;
    EmuConfig.Achievements.Enabled = enable;
    Host::SetBaseBoolSettingValue("Achievements", "Enabled", enable);
    Host::CommitBaseSettingChanges();
    Achievements::UpdateSettings(old_config);
    NotifyRetroAchievementsState();
}

extern "C"
JNIEXPORT void JNICALL
Java_kr_co_iefriends_pcsx2_utils_RetroAchievementsBridge_nativeSetHardcore(JNIEnv* env, jclass, jboolean enabled)
{
    (void)env;
    const bool enable = (enabled == JNI_TRUE);

    if (EmuConfig.Achievements.HardcoreMode == enable)
    {
        Host::SetBaseBoolSettingValue("Achievements", "ChallengeMode", enable);
        Host::CommitBaseSettingChanges();
        NotifyRetroAchievementsState();
        return;
    }

    Pcsx2Config::AchievementsOptions old_config = EmuConfig.Achievements;
    EmuConfig.Achievements.HardcoreMode = enable;
    Host::SetBaseBoolSettingValue("Achievements", "ChallengeMode", enable);
    Host::CommitBaseSettingChanges();
    Achievements::UpdateSettings(old_config);
    NotifyRetroAchievementsState();
}


void Host::CommitBaseSettingChanges()
{
    auto lock = GetSettingsLock();
    if (s_settings_interface)
        s_settings_interface->Save();
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

bool Host::ConfirmMessage(const std::string_view title, const std::string_view message)
{
    if (!title.empty() && !message.empty())
        ERROR_LOG("ConfirmMessage: {}: {}", title, message);
    else if (!message.empty())
        ERROR_LOG("ConfirmMessage: {}", message);

    return true;
}

void Host::OpenURL(const std::string_view url)
{
    // noop
}

bool Host::CopyTextToClipboard(const std::string_view text)
{
    return false;
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
}

void Host::OnVMResumed()
{
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
    pxFailRel("Not implemented");
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
    VMManager::SetState(VMState::Stopping);
}

void Host::OnAchievementsLoginSuccess(const char* username, u32 points, u32 sc_points, u32 unread_messages)
{
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    if (!EnsureRetroAchievementsBridge(env))
        return;

    jstring j_username = username ? env->NewStringUTF(username) : nullptr;
    env->CallStaticVoidMethod(s_ra_bridge_class, s_ra_notify_login_success, j_username,
                              static_cast<jint>(points), static_cast<jint>(sc_points),
                              static_cast<jint>(unread_messages));
    ClearJNIExceptions(env);
    if (j_username)
        env->DeleteLocalRef(j_username);

    NotifyRetroAchievementsState();
}

void Host::OnAchievementsLoginRequested(Achievements::LoginRequestReason reason)
{
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    if (!EnsureRetroAchievementsBridge(env))
        return;

    env->CallStaticVoidMethod(s_ra_bridge_class, s_ra_notify_login_requested,
                              static_cast<jint>(reason));
    ClearJNIExceptions(env);
}

void Host::OnAchievementsHardcoreModeChanged(bool enabled)
{
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    if (!EnsureRetroAchievementsBridge(env))
        return;

    env->CallStaticVoidMethod(s_ra_bridge_class, s_ra_notify_hardcore_changed,
                              enabled ? JNI_TRUE : JNI_FALSE);
    ClearJNIExceptions(env);
    NotifyRetroAchievementsState();

}

void Host::OnAchievementsRefreshed()
{
    NotifyRetroAchievementsState();
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

std::optional<u32> InputManager::ConvertHostKeyboardStringToCode(const std::string_view str)
{
    return std::nullopt;
}

std::optional<std::string> InputManager::ConvertHostKeyboardCodeToString(u32 code)
{
    return std::nullopt;
}

const char* InputManager::ConvertHostKeyboardCodeToIcon(u32 code)
{
    return nullptr;
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
