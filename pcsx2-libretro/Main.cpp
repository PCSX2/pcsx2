// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// pcsx2-libretro — libretro core frontend (armsx2_libretro).
//
// Milestone 1 scaffold: the core builds as a shared library, exposes the
// libretro v1 API, and boots the VM headlessly (GS renderer forced to Null,
// no host display surface). retro_run() presents a placeholder XRGB8888
// framebuffer while the emulated machine free-runs on a dedicated CPU
// thread — the same CPU-thread state machine the SDL frontend uses.
//
// Next milestones (mirrors the proven lrps2-libretro architecture):
//  M2: Vulkan HW render via retro_hw_render_context_negotiation_interface
//      (vkCreateInstance/Device wrapped so GSDeviceVK's own init receives the
//      frontend-negotiated instance/device), frame handoff via set_image.
//  M3: retro_run frame pacing (block until MTGS presents exactly one frame),
//      audio batching, libretro input -> Pad, core options -> settings.
//  M4: save states over retro_serialize, disk control, memcard-per-content.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "libretro.h"

#define VK_NO_PROTOTYPES
#include "libretro_vulkan.h"

#include "fmt/format.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/CrashHandler.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/MemorySettingsInterface.h"
#include "common/Path.h"
#include "common/ProgressCallback.h"
#include "common/SmallString.h"
#include "common/StringUtil.h"
#include "common/Threading.h"

#include "pcsx2/PrecompiledHeader.h"

#include "pcsx2/Achievements.h"
#include "pcsx2/CDVD/CDVDcommon.h"
#include "pcsx2/GS.h"
#include "pcsx2/GS/Renderers/Vulkan/GSDeviceVK.h"
#include "pcsx2/GS/Renderers/Vulkan/VKLibretro.h"
#include "pcsx2/GameList.h"
#include "pcsx2/Host.h"
#include "pcsx2/INISettingsInterface.h"
#include "pcsx2/ImGui/FullscreenUI.h"
#include "pcsx2/ImGui/ImGuiFullscreen.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/Input/InputManager.h"
#include "pcsx2/MTGS.h"
#include "pcsx2/PerformanceMetrics.h"
#include "pcsx2/SaveState.h"
#include "pcsx2/Memory.h"
#include "pcsx2/SIO/Pad/Pad.h"
#include "pcsx2/SIO/Pad/PadBase.h"
#include "pcsx2/SIO/Pad/PadDualshock2.h"
#include "pcsx2/SPU2/spu2.h"
#include "pcsx2/Host/AudioStream.h"
#include "pcsx2/VMManager.h"

#include "svnrev.h"

//////////////////////////////////////////////////////////////////////////
// libretro callbacks + core state
//////////////////////////////////////////////////////////////////////////

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_sample_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_log_printf_t log_cb;

namespace LibretroCore
{
	static bool InitializeConfig();
	static void CPUThreadMain(VMBootParameters initial_params);
	static void DrainCPUThreadQueue();

	// Placeholder frame while there is no real presentation path (M2).
	static constexpr u32 kFrameWidth = 640;
	static constexpr u32 kFrameHeight = 448;
	static std::vector<u32> s_frame_buffer;

	static std::string s_content_path;
	static std::thread s_cpu_thread;
	static std::atomic<bool> s_vm_thread_running{false};

	// M2 Vulkan HW-render state. The CPU thread parks the initial boot until
	// the frontend has (a) negotiated the shared VkDevice (create_device,
	// which opens MTGS/GSDeviceVK from the frontend thread) and (b) fired
	// context_reset (making the retro_hw_render_interface_vulkan available).
	static bool s_hw_render_vulkan = false;
	static std::atomic<bool> s_cpu_thread_initialized{false};
	static std::atomic<bool> s_context_ready{false};
} // namespace LibretroCore

// Settings persistence (INI under the frontend's system directory).
static std::unique_ptr<INISettingsInterface> s_base_settings;
static std::unique_ptr<INISettingsInterface> s_secrets_settings;

static std::atomic<bool> s_shutdown_requested{false};

// Pending CPU-thread callbacks queued by Host::RunOnCPUThread (same
// mechanism as the SDL frontend; drained by Host::PumpMessagesOnCPUThread
// every vsync).
static std::mutex s_cpu_queue_lock;
static std::deque<std::function<void()>> s_cpu_queue;
static std::condition_variable s_cpu_queue_cv;
static std::atomic<std::thread::id> s_cpu_thread_id{};

static void FallbackLog(enum retro_log_level level, const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	std::vfprintf(stderr, fmt, va);
	va_end(va);
}

//////////////////////////////////////////////////////////////////////////
// Settings + lifecycle
//////////////////////////////////////////////////////////////////////////

bool LibretroCore::InitializeConfig()
{
	// Everything (resources, bios, memcards, cache, ini) lives under
	// <retro system dir>/pcsx2 so the core is self-contained and shares BIOS
	// files with other PS2 cores' conventions.
	const char* system_base = nullptr;
	if (!environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_base) || !system_base)
	{
		log_cb(RETRO_LOG_ERROR, "No system directory from frontend.\n");
		return false;
	}

	EmuFolders::AppRoot = Path::Combine(system_base, "pcsx2");
	EmuFolders::Resources = Path::Combine(EmuFolders::AppRoot, "resources");
	EmuFolders::DataRoot = EmuFolders::AppRoot;
	// Normally derived inside SetDataDirectory(), which the libretro path
	// bypasses (the frontend dictates the root) -- set it explicitly or the
	// INI lands in the process cwd.
	EmuFolders::Settings = Path::Combine(EmuFolders::DataRoot, "inis");
	FileSystem::EnsureDirectoryExists(EmuFolders::DataRoot.c_str(), false);
	FileSystem::EnsureDirectoryExists(EmuFolders::Settings.c_str(), false);

	CrashHandler::SetWriteDirectory(EmuFolders::DataRoot);

	const char* hw_check_error = nullptr;
	if (!VMManager::PerformEarlyHardwareChecks(&hw_check_error))
	{
		log_cb(RETRO_LOG_ERROR, "Early hardware check failed: %s\n",
			hw_check_error ? hw_check_error : "unknown");
		return false;
	}

	// OSD / ImGui font (required by ImGuiManager even when nothing draws).
	{
		const std::string roboto_path =
			EmuFolders::GetOverridableResourcePath("fonts" FS_OSPATH_SEPARATOR_STR "Roboto-Regular.ttf");
		const auto roboto_data = FileSystem::MapBinaryFileForRead(roboto_path.c_str());
		if (!roboto_data.empty())
		{
			std::vector<ImGuiManager::FontInfo> fonts;
			ImGuiManager::FontInfo fi{};
			fi.data = roboto_data;
			fonts.push_back(fi);
			ImGuiManager::SetFonts(std::move(fonts));
		}
		else
		{
			log_cb(RETRO_LOG_WARN, "Missing font resource '%s' (OSD disabled).\n", roboto_path.c_str());
		}
	}

	const std::string ini_path = Path::Combine(EmuFolders::Settings, "armsx2-libretro.ini");
	const bool ini_exists = FileSystem::FileExists(ini_path.c_str());

	s_base_settings = std::make_unique<INISettingsInterface>(ini_path);
	Host::Internal::SetBaseSettingsLayer(s_base_settings.get());

	if (!ini_exists || !s_base_settings->Load() || !VMManager::Internal::CheckSettingsVersion())
		VMManager::SetDefaultSettings(*s_base_settings, true, true, true, true, true);

	const std::string secrets_path = Path::Combine(EmuFolders::Settings, "secrets.ini");
	s_secrets_settings = std::make_unique<INISettingsInterface>(secrets_path);
	Host::Internal::SetSecretsSettingsLayer(s_secrets_settings.get());
	if (FileSystem::FileExists(secrets_path.c_str()))
		s_secrets_settings->Load();

	// Libretro-core overrides: the shared-context Vulkan renderer, and SDL
	// input/audio replaced by the libretro paths.
	{
		auto lock = Host::GetSettingsLock();
		s_base_settings->SetIntValue("EmuCore/GS", "Renderer",
			static_cast<int>(GSRendererType::VK));
		s_base_settings->SetBoolValue("InputSources", "SDL", false);
		// Audio goes out through retro_run pulling the stream ring; the Null
		// backend keeps SPU2 mixing into the ring with no device thread.
		s_base_settings->SetStringValue("SPU2/Output", "Backend", "Null");
	}

	Error save_error;
	if (!s_base_settings->Save(&save_error))
		Console.ErrorFmt("Failed to save config: {}", save_error.GetDescription());

	VMManager::Internal::LoadStartupSettings();
	return true;
}

//////////////////////////////////////////////////////////////////////////
// Host:: callbacks (adapted from pcsx2-sdl; libretro has no windowing,
// no clipboard, no native file picker)
//////////////////////////////////////////////////////////////////////////

void Host::CommitBaseSettingChanges()
{
	if (!s_base_settings)
		return;
	Error err;
	if (!s_base_settings->Save(&err))
		Console.ErrorFmt("Failed to save settings: {}", err.GetDescription());
}

void Host::LoadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock)
{
}

void Host::CheckForSettingsChanges(const Pcsx2Config& old_config)
{
}

bool Host::RequestResetSettings(bool folders, bool core, bool controllers, bool hotkeys, bool ui)
{
	return false;
}

void Host::SetDefaultUISettings(SettingsInterface& si)
{
	si.SetBoolValue("UI", "StartBigPictureMode", false);
}

bool Host::LocaleCircleConfirm()
{
	return false;
}

std::unique_ptr<ProgressCallback> Host::CreateHostProgressCallback()
{
	return ProgressCallback::CreateNullProgressCallback();
}

void Host::ReportInfoAsync(const std::string_view title, const std::string_view message)
{
	if (!title.empty() && !message.empty())
		INFO_LOG("{}: {}", title, message);
	else if (!message.empty())
		INFO_LOG("{}", message);
}

void Host::ReportErrorAsync(const std::string_view title, const std::string_view message)
{
	if (!title.empty() && !message.empty())
		ERROR_LOG("{}: {}", title, message);
	else if (!message.empty())
		ERROR_LOG("{}", message);
}

void Host::OpenURL(const std::string_view url)
{
}

bool Host::CopyTextToClipboard(const std::string_view text)
{
	return false;
}

std::string Host::GetTextFromClipboard()
{
	return std::string();
}

void Host::BeginTextInput()
{
}

void Host::EndTextInput()
{
}

static std::optional<WindowInfo> BuildLibretroWindowInfo()
{
	// M1: no surface at all — the Null renderer is the only allowed provider.
	// M2 will return a WindowInfo describing the negotiated Vulkan context.
	WindowInfo wi;
	wi.type = WindowInfo::Type::Surfaceless;
	wi.surface_width = LibretroCore::kFrameWidth;
	wi.surface_height = LibretroCore::kFrameHeight;
	wi.surface_scale = 1.0f;
	return wi;
}

std::optional<WindowInfo> Host::GetTopLevelWindowInfo()
{
	return BuildLibretroWindowInfo();
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

void Host::SetMouseLock(bool state)
{
}

std::optional<WindowInfo> Host::AcquireRenderWindow(bool recreate_window)
{
	return BuildLibretroWindowInfo();
}

void Host::ReleaseRenderWindow()
{
}

void Host::BeginPresentFrame()
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

void Host::OnGameChanged(const std::string& title, const std::string& elf_override, const std::string& disc_path,
	const std::string& disc_serial, u32 disc_crc, u32 current_crc)
{
	if (!title.empty())
		INFO_LOG("Game changed: {} (serial {}, CRC {:08X})", title, disc_serial, current_crc);
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

void LibretroCore::DrainCPUThreadQueue()
{
	for (;;)
	{
		std::function<void()> fn;
		{
			std::lock_guard<std::mutex> lock(s_cpu_queue_lock);
			if (s_cpu_queue.empty())
				return;
			fn = std::move(s_cpu_queue.front());
			s_cpu_queue.pop_front();
		}
		fn();
	}
}

void Host::PumpMessagesOnCPUThread()
{
	if (s_shutdown_requested.load(std::memory_order_acquire) && VMManager::HasValidVM())
		VMManager::SetState(VMState::Stopping);

	LibretroCore::DrainCPUThreadQueue();
}

void Host::RunOnCPUThread(std::function<void()> function, bool block)
{
	if (block)
	{
		if (s_cpu_thread_id.load(std::memory_order_acquire) == std::this_thread::get_id())
		{
			function();
			return;
		}

		std::mutex done_lock;
		std::condition_variable done_cv;
		bool done = false;
		auto wrapped = [&function, &done_lock, &done_cv, &done]() {
			function();
			std::lock_guard<std::mutex> lk(done_lock);
			done = true;
			done_cv.notify_all();
		};
		{
			std::lock_guard<std::mutex> lock(s_cpu_queue_lock);
			s_cpu_queue.emplace_back(std::move(wrapped));
		}
		s_cpu_queue_cv.notify_all();
		std::unique_lock<std::mutex> lk(done_lock);
		done_cv.wait(lk, [&done]() { return done; });
		return;
	}

	{
		std::lock_guard<std::mutex> lock(s_cpu_queue_lock);
		s_cpu_queue.emplace_back(std::move(function));
	}
	s_cpu_queue_cv.notify_all();
}

void Host::RefreshGameListAsync(bool invalidate_cache)
{
	// The frontend owns the game list.
}

void Host::CancelGameListRefresh()
{
}

bool Host::IsFullscreen()
{
	return true;
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
	s_shutdown_requested.store(true, std::memory_order_release);
	if (VMManager::HasValidVM())
		VMManager::SetState(VMState::Stopping);
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
}

void Host::OnAchievementsLoginRequested(Achievements::LoginRequestReason reason)
{
}

void Host::OnAchievementsHardcoreModeChanged(bool enabled)
{
}

bool Host::HasNativeAchievementNotifications() { return false; }
void Host::OnAchievementNotification(const char*, float, const char*, const char*, const char*) {}

void Host::OnAchievementsRefreshed()
{
}

void Host::OnCoverDownloaderOpenRequested()
{
}

void Host::OnCreateMemoryCardOpenRequested()
{
}

bool Host::InBatchMode()
{
	return true;
}

bool Host::InNoGUIMode()
{
	return true;
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

int Host::LocaleSensitiveCompare(std::string_view lhs, std::string_view rhs)
{
	const int res = std::strncmp(lhs.data(), rhs.data(), std::min(lhs.size(), rhs.size()));
	if (res != 0)
		return res;
	return lhs.size() > rhs.size() ? 1 : (lhs.size() < rhs.size() ? -1 : 0);
}

s32 Host::Internal::GetTranslatedStringImpl(
	const std::string_view context, const std::string_view msg, char* tbuf, size_t tbuf_space)
{
	if (msg.size() > tbuf_space)
		return -1;
	if (msg.empty())
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
		ret.replace(pos, 2, count_str.view());
	}
	return ret;
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

BEGIN_HOTKEY_LIST(g_host_hotkeys)
END_HOTKEY_LIST()

//////////////////////////////////////////////////////////////////////////
// CPU thread
//////////////////////////////////////////////////////////////////////////

void LibretroCore::CPUThreadMain(VMBootParameters initial_params)
{
	Threading::SetNameOfCurrentThread("CPU Thread");
	s_cpu_thread_id.store(std::this_thread::get_id(), std::memory_order_release);
	s_vm_thread_running.store(true, std::memory_order_release);

	if (!VMManager::Internal::CPUThreadInitialize())
	{
		Console.Error("CPU thread init failed.");
		VMManager::Internal::CPUThreadShutdown();
		s_vm_thread_running.store(false, std::memory_order_release);
		s_cpu_thread_initialized.store(true, std::memory_order_release); // unblock load_game
		return;
	}

	VMManager::ApplySettings();
	s_cpu_thread_initialized.store(true, std::memory_order_release);

	// With Vulkan HW render the boot has to wait for the frontend's context
	// negotiation + context_reset; booting earlier would open MTGS before
	// VKLibretro::Init holds the shared instance.
	while (s_hw_render_vulkan && !s_context_ready.load(std::memory_order_acquire) &&
			!s_shutdown_requested.load(std::memory_order_acquire))
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	std::fprintf(stderr, "[libretro] CPU thread: context ready, entering VM state machine (boot file: %s)\n",
		initial_params.filename.c_str());

	std::optional<VMBootParameters> pending_boot = std::move(initial_params);

	while (!s_shutdown_requested.load(std::memory_order_acquire))
	{
		DrainCPUThreadQueue();

		const VMState state = VMManager::GetState();
		switch (state)
		{
			case VMState::Initializing:
				continue;

			case VMState::Running:
				VMManager::Execute();
				continue;

			case VMState::Resetting:
				VMManager::Reset();
				continue;

			case VMState::Stopping:
				VMManager::Shutdown(false);
				continue;

			case VMState::Paused:
			case VMState::Shutdown:
			{
				if (pending_boot.has_value())
				{
					VMBootParameters bp = std::move(pending_boot.value());
					pending_boot.reset();
					std::fprintf(stderr, "[libretro] CPU thread: VMManager::Initialize...\n");
					const VMBootResult br = VMManager::Initialize(bp);
					std::fprintf(stderr, "[libretro] CPU thread: Initialize -> %d\n", (int)br);
					if (br != VMBootResult::StartupSuccess)
					{
						Console.ErrorFmt("VMManager::Initialize failed (result {}).", static_cast<int>(br));
						s_shutdown_requested.store(true, std::memory_order_release);
						break;
					}
					VMManager::SetState(VMState::Running);
					continue;
				}

				if (state == VMState::Shutdown)
				{
					// Game exited on its own; nothing more to run.
					s_shutdown_requested.store(true, std::memory_order_release);
					break;
				}

				std::unique_lock<std::mutex> lock(s_cpu_queue_lock);
				s_cpu_queue_cv.wait_for(lock, std::chrono::milliseconds(16),
					[]() { return !s_cpu_queue.empty(); });
				continue;
			}

			default:
				continue;
		}
	}

	if (VMManager::HasValidVM())
		VMManager::Shutdown(false);
	if (MTGS::IsOpen())
	{
		MTGS::SetRunIdle(false);
		MTGS::WaitForClose();
	}

	VMManager::Internal::CPUThreadShutdown();
	s_cpu_thread_id.store(std::thread::id{}, std::memory_order_release);
	s_vm_thread_running.store(false, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
// Disk control (M4): multi-disc via .m3u playlists. Disc swaps go through
// VMManager::ChangeDisc on the CPU thread.
//////////////////////////////////////////////////////////////////////////

static std::vector<std::string> s_disk_images;
static unsigned s_disk_index = 0;
static bool s_disk_ejected = false;

static bool RETRO_CALLCONV DiskSetEjectState(bool ejected)
{
	if (ejected == s_disk_ejected)
		return true;
	s_disk_ejected = ejected;
	if (!ejected && s_disk_index < s_disk_images.size() && VMManager::HasValidVM())
	{
		const std::string path = s_disk_images[s_disk_index];
		Host::RunOnCPUThread([path]() { VMManager::ChangeDisc(CDVD_SourceType::Iso, path); }, false);
	}
	return true;
}

static bool RETRO_CALLCONV DiskGetEjectState(void)
{
	return s_disk_ejected;
}

static unsigned RETRO_CALLCONV DiskGetImageIndex(void)
{
	return s_disk_index;
}

static bool RETRO_CALLCONV DiskSetImageIndex(unsigned index)
{
	if (index >= s_disk_images.size())
		return false;
	s_disk_index = index;
	return true;
}

static unsigned RETRO_CALLCONV DiskGetNumImages(void)
{
	return static_cast<unsigned>(s_disk_images.size());
}

static bool RETRO_CALLCONV DiskReplaceImageIndex(unsigned index, const struct retro_game_info* info)
{
	if (index >= s_disk_images.size())
		return false;
	if (!info || !info->path)
		s_disk_images.erase(s_disk_images.begin() + index);
	else
		s_disk_images[index] = info->path;
	return true;
}

static bool RETRO_CALLCONV DiskAddImageIndex(void)
{
	s_disk_images.emplace_back();
	return true;
}

static bool RETRO_CALLCONV DiskGetImagePath(unsigned index, char* path, size_t len)
{
	if (index >= s_disk_images.size())
		return false;
	StringUtil::Strlcpy(path, s_disk_images[index], len);
	return true;
}

static bool RETRO_CALLCONV DiskGetImageLabel(unsigned index, char* label, size_t len)
{
	if (index >= s_disk_images.size())
		return false;
	StringUtil::Strlcpy(label, Path::GetFileTitle(s_disk_images[index]), len);
	return true;
}

static void RegisterDiskControl(void)
{
	static const struct retro_disk_control_ext_callback cb = {
		DiskSetEjectState, DiskGetEjectState,
		DiskGetImageIndex, DiskSetImageIndex,
		DiskGetNumImages, DiskReplaceImageIndex, DiskAddImageIndex,
		nullptr, // set_initial_image
		DiskGetImagePath, DiskGetImageLabel,
	};
	environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, (void*)&cb);
}

// Parse an .m3u playlist into s_disk_images; returns the first disc path.
static std::string LoadM3UPlaylist(const std::string& m3u_path)
{
	s_disk_images.clear();
	const std::string base = std::string(Path::GetDirectory(m3u_path));

	const auto data = FileSystem::ReadFileToString(m3u_path.c_str());
	if (!data.has_value())
		return {};

	for (std::string_view line_v : StringUtil::SplitString(data.value(), '\n', true))
	{
		std::string line(StringUtil::StripWhitespace(line_v));
		if (line.empty() || line[0] == '#')
			continue;
		if (!Path::IsAbsolute(line))
			line = Path::Combine(base, line);
		s_disk_images.push_back(std::move(line));
	}

	return s_disk_images.empty() ? std::string() : s_disk_images.front();
}

//////////////////////////////////////////////////////////////////////////
// Core options (M4). Applied at load and re-applied on change notifications
// from the frontend; ApplySettings runs on the CPU thread. Registered as
// core options v2 (categorised), with the flat retro_variable list kept as
// the fallback for frontends that only speak the legacy API.
//////////////////////////////////////////////////////////////////////////

static struct retro_core_option_v2_category kOptionCategories[] = {
	{"video", "Video", "Rendering, scaling and display options."},
	{"performance", "Performance", "Speed hacks trading accuracy for framerate."},
	{"system", "System", "Boot behaviour."},
	{nullptr, nullptr, nullptr},
};

static struct retro_core_option_v2_definition kOptionDefinitions[] = {
	{"armsx2_renderer", "GS Renderer (restart)", "GS Renderer (restart)",
		"Vulkan renders the GS on the GPU. Software renders on the CPU and presents through the same shared Vulkan context.",
		nullptr, "video",
		{{"Vulkan", nullptr}, {"Software", nullptr}, {nullptr, nullptr}}, "Vulkan"},
	{"armsx2_upscale", "Internal Resolution", "Internal Resolution",
		"Renders the PS2 output at a multiple of native resolution. The output canvas follows this size.",
		nullptr, "video",
		{{"1x", "1x (native)"}, {"2x", nullptr}, {"3x", nullptr}, {"4x", nullptr}, {nullptr, nullptr}}, "1x"},
	{"armsx2_aspect_ratio", "Aspect Ratio", "Aspect Ratio",
		"Display aspect ratio. 16:9 is intended for games with widescreen patches or native widescreen modes.",
		nullptr, "video",
		{{"Auto 4:3/3:2", "Auto (4:3 / 3:2 progressive)"}, {"4:3", nullptr}, {"16:9", nullptr},
			{"Stretch", nullptr}, {nullptr, nullptr}}, "Auto 4:3/3:2"},
	{"armsx2_deinterlacing", "Deinterlacing", "Deinterlacing",
		"How interlaced (480i/576i) output is turned into a full frame. Automatic picks per game; "
		"Bob is fast, Adaptive is highest quality; Off shows the raw field.",
		nullptr, "video",
		{{"Automatic", nullptr}, {"Off", nullptr}, {"Weave TFF", nullptr}, {"Weave BFF", nullptr},
			{"Bob TFF", nullptr}, {"Bob BFF", nullptr}, {"Blend TFF", nullptr}, {"Blend BFF", nullptr},
			{"Adaptive TFF", nullptr}, {"Adaptive BFF", nullptr}, {nullptr, nullptr}}, "Automatic"},
	{"armsx2_no_interlacing_patches", "No-Interlacing Patches (restart)", "No-Interlacing Patches (restart)",
		"Patches supported games to render progressive instead of interlaced — sharper than any deinterlacer.",
		nullptr, "video",
		{{"disabled", nullptr}, {"enabled", nullptr}, {nullptr, nullptr}}, "disabled"},
	{"armsx2_widescreen_patches", "Widescreen Patches (restart)", "Widescreen Patches (restart)",
		"Patches supported games to render 16:9. Set Aspect Ratio to 16:9 alongside this.",
		nullptr, "video",
		{{"disabled", nullptr}, {"enabled", nullptr}, {nullptr, nullptr}}, "disabled"},
	{"armsx2_blending_accuracy", "Blending Accuracy", "Blending Accuracy",
		"How accurately PS2 framebuffer blending is emulated on the GPU. Lower levels are faster; "
		"raise it only for games with visible blending artifacts.",
		nullptr, "video",
		{{"Minimum", nullptr}, {"Basic", nullptr}, {"Medium", nullptr}, {"High", nullptr},
			{"Full", nullptr}, {"Maximum", nullptr}, {nullptr, nullptr}}, "Basic"},
	{"armsx2_dithering", "Dithering", "Dithering",
		"Unscaled replicates PS2 dithering; Off can reduce banding artifacts at higher internal resolutions.",
		nullptr, "video",
		{{"Unscaled", nullptr}, {"Off", nullptr}, {"Scaled", nullptr}, {nullptr, nullptr}}, "Unscaled"},
	{"armsx2_trilinear_filtering", "Trilinear Filtering", "Trilinear Filtering",
		nullptr, nullptr, "video",
		{{"Automatic", nullptr}, {"Off", nullptr}, {"Trilinear (PS2)", nullptr},
			{"Trilinear (Forced)", nullptr}, {nullptr, nullptr}}, "Automatic"},
	{"armsx2_mipmapping", "Hardware Mipmapping", "Hardware Mipmapping",
		nullptr, nullptr, "video",
		{{"enabled", nullptr}, {"disabled", nullptr}, {nullptr, nullptr}}, "enabled"},
	{"armsx2_fxaa", "FXAA", "FXAA",
		"Cheap post-process anti-aliasing.", nullptr, "video",
		{{"disabled", nullptr}, {"enabled", nullptr}, {nullptr, nullptr}}, "disabled"},
	{"armsx2_texture_filtering", "Texture Filtering", "Texture Filtering",
		"Bilinear (PS2) filters as the game requests. Forced filters everything, which smooths textures "
		"but can blur 2D elements; the sprite-excluding variant protects UI sprites.",
		nullptr, "video",
		{{"Nearest", nullptr}, {"Bilinear (Forced)", nullptr}, {"Bilinear (PS2)", nullptr},
			{"Bilinear (Forced excluding sprites)", nullptr}, {nullptr, nullptr}}, "Bilinear (PS2)"},
	{"armsx2_anisotropic_filtering", "Anisotropic Filtering", "Anisotropic Filtering",
		"Sharpens textures viewed at an angle. Cheap on the GPU, but can cause artifacts in games that "
		"rely on point sampling.",
		nullptr, "video",
		{{"0", "disabled"}, {"2", "2x"}, {"4", "4x"}, {"8", "8x"}, {"16", "16x"}, {nullptr, nullptr}}, "0"},
	{"armsx2_sw_threads", "Software Renderer Threads", "Software Renderer Threads",
		"Worker threads for the Software renderer (in addition to the GS thread). No effect on Vulkan.",
		nullptr, "video",
		{{"0", nullptr}, {"1", nullptr}, {"2", nullptr}, {"3", nullptr}, {"4", nullptr},
			{nullptr, nullptr}}, "2"},
	{"armsx2_show_fps", "Show FPS", "Show FPS",
		"Draws the internal framerate on screen.",
		nullptr, "video",
		{{"disabled", nullptr}, {"enabled", nullptr}, {nullptr, nullptr}}, "disabled"},
	{"armsx2_ee_cycle_rate", "EE Cycle Rate", "EE Cycle Rate",
		"Underclocks or overclocks the emulated Emotion Engine. Below 100% speeds up emulation but can "
		"cause stutter or breakage; above 100% can smooth out games with internal slowdown.",
		nullptr, "performance",
		{{"50%", nullptr}, {"60%", nullptr}, {"75%", nullptr}, {"100%", "100% (default)"},
			{"130%", nullptr}, {"180%", nullptr}, {"300%", nullptr}, {nullptr, nullptr}}, "100%"},
	{"armsx2_ee_cycle_skip", "EE Cycle Skip", "EE Cycle Skip",
		"Makes the emulated EE skip cycles. Helps games with obvious VU-driven slowdown; "
		"can cause false FPS readings and breakage.",
		nullptr, "performance",
		{{"disabled", nullptr}, {"mild", nullptr}, {"moderate", nullptr}, {"maximum", nullptr},
			{nullptr, nullptr}}, "disabled"},
	{"armsx2_hw_download_mode", "Hardware Download Mode", "Hardware Download Mode",
		"How GS-to-EE readbacks are handled. Accurate is correct but expensive on mobile GPUs; "
		"Disable Readbacks skips the data copy, Unsynchronized doesn't wait for the GPU, Disabled "
		"ignores the transfer entirely. Anything but Accurate can break effects that read the framebuffer.",
		nullptr, "performance",
		{{"Accurate", nullptr}, {"Disable Readbacks", nullptr}, {"Unsynchronized", nullptr},
			{"Disabled", nullptr}, {nullptr, nullptr}}, "Accurate"},
	{"armsx2_mtvu", "MTVU (Multi-Threaded VU1)", "MTVU (Multi-Threaded VU1)",
		"Runs VU1 on its own thread. Large speedup on multi-core CPUs; a small number of games hang with it.",
		nullptr, "performance",
		{{"enabled", nullptr}, {"disabled", nullptr}, {nullptr, nullptr}}, "enabled"},
	{"armsx2_instant_vu1", "Instant VU1", "Instant VU1",
		"Runs VU1 programs to completion instantly instead of interleaving with the EE. "
		"Fast and safe for most games.",
		nullptr, "performance",
		{{"enabled", nullptr}, {"disabled", nullptr}, {nullptr, nullptr}}, "enabled"},
	{"armsx2_bios", "BIOS (restart)", "BIOS (restart)",
		"Which BIOS image from <system>/pcsx2/bios to boot. Auto picks the first valid image.",
		nullptr, "system",
		{{"auto", "Auto (first valid image)"}, {nullptr, nullptr}}, "auto"},
	{"armsx2_fast_boot", "Fast Boot", "Fast Boot",
		"Skips the BIOS boot animation.",
		nullptr, "system",
		{{"enabled", nullptr}, {"disabled", nullptr}, {nullptr, nullptr}}, "enabled"},
	{"armsx2_cheats", "Enable Cheats", "Enable Cheats",
		"Loads .pnach cheat files from <system>/pcsx2/cheats for the running game.",
		nullptr, "system",
		{{"disabled", nullptr}, {"enabled", nullptr}, {nullptr, nullptr}}, "disabled"},
	{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, {{nullptr, nullptr}}, nullptr},
};

static struct retro_core_options_v2 kOptionsV2 = {kOptionCategories, kOptionDefinitions};

// Legacy fallback: first value doubles as the default. Non-const so the
// BIOS entry can be pointed at the scanned list.
static struct retro_variable kCoreVariables[] = {
	{"armsx2_renderer", "GS renderer (restart); Vulkan|Software"},
	{"armsx2_upscale", "Internal resolution; 1x|2x|3x|4x"},
	{"armsx2_aspect_ratio", "Aspect ratio; Auto 4:3/3:2|4:3|16:9|Stretch"},
	{"armsx2_deinterlacing", "Deinterlacing; Automatic|Off|Weave TFF|Weave BFF|Bob TFF|Bob BFF|Blend TFF|Blend BFF|Adaptive TFF|Adaptive BFF"},
	{"armsx2_no_interlacing_patches", "No-interlacing patches (restart); disabled|enabled"},
	{"armsx2_widescreen_patches", "Widescreen patches (restart); disabled|enabled"},
	{"armsx2_blending_accuracy", "Blending accuracy; Basic|Minimum|Medium|High|Full|Maximum"},
	{"armsx2_dithering", "Dithering; Unscaled|Off|Scaled"},
	{"armsx2_trilinear_filtering", "Trilinear filtering; Automatic|Off|Trilinear (PS2)|Trilinear (Forced)"},
	{"armsx2_mipmapping", "Hardware mipmapping; enabled|disabled"},
	{"armsx2_fxaa", "FXAA; disabled|enabled"},
	{"armsx2_texture_filtering", "Texture filtering; Bilinear (PS2)|Nearest|Bilinear (Forced)|Bilinear (Forced excluding sprites)"},
	{"armsx2_anisotropic_filtering", "Anisotropic filtering; 0|2|4|8|16"},
	{"armsx2_sw_threads", "Software renderer threads; 2|0|1|3|4"},
	{"armsx2_show_fps", "Show FPS on screen; disabled|enabled"},
	{"armsx2_ee_cycle_rate", "EE cycle rate; 100%|50%|60%|75%|130%|180%|300%"},
	{"armsx2_ee_cycle_skip", "EE cycle skip; disabled|mild|moderate|maximum"},
	{"armsx2_hw_download_mode", "Hardware download mode; Accurate|Disable Readbacks|Unsynchronized|Disabled"},
	{"armsx2_mtvu", "MTVU (multi-threaded VU1); enabled|disabled"},
	{"armsx2_instant_vu1", "Instant VU1; enabled|disabled"},
	{"armsx2_bios", "BIOS (restart); auto"},
	{"armsx2_fast_boot", "Fast boot; enabled|disabled"},
	{"armsx2_cheats", "Enable cheats; disabled|enabled"},
	{nullptr, nullptr},
};

// BIOS images found under <system>/pcsx2/bios, scanned when the frontend
// registers the options. The vector owns the value strings; the option
// tables point into it.
static std::vector<std::string> s_bios_images;
static std::string s_bios_legacy_values;

static void PopulateBiosOptions(retro_environment_t cb)
{
	if (!s_bios_images.empty())
		return;

	const char* system_dir = nullptr;
	if (!cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) || !system_dir)
		return;

	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(Path::Combine(system_dir, "pcsx2/bios").c_str(), "*",
		FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES, &files);
	for (const FILESYSTEM_FIND_DATA& fd : files)
	{
		if (!StringUtil::EndsWithNoCase(fd.FileName, ".bin"))
			continue;
		s_bios_images.push_back(std::string(Path::GetFileName(fd.FileName)));
	}
	std::sort(s_bios_images.begin(), s_bios_images.end());

	retro_core_option_v2_definition* bios_def = nullptr;
	for (retro_core_option_v2_definition& def : kOptionDefinitions)
	{
		if (def.key && !std::strcmp(def.key, "armsx2_bios"))
		{
			bios_def = &def;
			break;
		}
	}
	if (!bios_def)
		return;

	s_bios_legacy_values = "BIOS (restart); auto";
	// values[0] is "auto", the last slot stays the terminator.
	const size_t max_values = std::size(bios_def->values) - 2;
	for (size_t i = 0; i < s_bios_images.size() && i < max_values; i++)
	{
		bios_def->values[i + 1] = {s_bios_images[i].c_str(), nullptr};
		s_bios_legacy_values += '|';
		s_bios_legacy_values += s_bios_images[i];
	}

	for (retro_variable& var : kCoreVariables)
	{
		if (var.key && !std::strcmp(var.key, "armsx2_bios"))
		{
			var.value = s_bios_legacy_values.c_str();
			break;
		}
	}
}

static void ApplyCoreOptions(bool startup)
{
	if (!s_base_settings)
		return;

	struct retro_variable var;
	{
		auto lock = Host::GetSettingsLock();

		var = {"armsx2_renderer", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && startup)
		{
			// Renderer swaps need the whole context negotiation to rerun;
			// only honour this at startup.
			if (!std::strcmp(var.value, "Software"))
				s_base_settings->SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::SW));
		}

		var = {"armsx2_upscale", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetFloatValue("EmuCore/GS", "upscale_multiplier",
				static_cast<float>(std::clamp(atoi(var.value), 1, 4)));

		var = {"armsx2_fast_boot", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetBoolValue("EmuCore", "EnableFastBoot", !std::strcmp(var.value, "enabled"));

		var = {"armsx2_widescreen_patches", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetBoolValue("EmuCore", "EnableWideScreenPatches", !std::strcmp(var.value, "enabled"));

		var = {"armsx2_show_fps", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetBoolValue("EmuCore/GS", "OsdShowFPS", !std::strcmp(var.value, "enabled"));

		var = {"armsx2_aspect_ratio", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			// Option values match Pcsx2Config::GSOptions::AspectRatioNames.
			s_base_settings->SetStringValue("EmuCore/GS", "AspectRatio", var.value);
		}

		var = {"armsx2_deinterlacing", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			// Indices match the GSInterlaceMode enum.
			static constexpr const char* kModes[] = {"Automatic", "Off", "Weave TFF", "Weave BFF",
				"Bob TFF", "Bob BFF", "Blend TFF", "Blend BFF", "Adaptive TFF", "Adaptive BFF"};
			for (size_t i = 0; i < std::size(kModes); i++)
			{
				if (!std::strcmp(var.value, kModes[i]))
				{
					s_base_settings->SetIntValue("EmuCore/GS", "deinterlace_mode", static_cast<int>(i));
					break;
				}
			}
		}

		var = {"armsx2_no_interlacing_patches", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetBoolValue("EmuCore", "EnableNoInterlacingPatches", !std::strcmp(var.value, "enabled"));

		var = {"armsx2_blending_accuracy", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			// Indices match the AccBlendLevel enum.
			static constexpr const char* kLevels[] = {"Minimum", "Basic", "Medium", "High", "Full", "Maximum"};
			for (size_t i = 0; i < std::size(kLevels); i++)
			{
				if (!std::strcmp(var.value, kLevels[i]))
				{
					s_base_settings->SetIntValue("EmuCore/GS", "accurate_blending_unit", static_cast<int>(i));
					break;
				}
			}
		}

		var = {"armsx2_dithering", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			// Values match the Dithering config field: 0=Off, 1=Scaled, 2=Unscaled.
			static constexpr std::pair<const char*, int> kDither[] = {{"Off", 0}, {"Scaled", 1}, {"Unscaled", 2}};
			for (const auto& [name, value] : kDither)
			{
				if (!std::strcmp(var.value, name))
				{
					s_base_settings->SetIntValue("EmuCore/GS", "dithering_ps2", value);
					break;
				}
			}
		}

		var = {"armsx2_trilinear_filtering", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			// Values match the TriFiltering enum (Automatic=-1).
			static constexpr std::pair<const char*, int> kTri[] = {
				{"Automatic", -1}, {"Off", 0}, {"Trilinear (PS2)", 1}, {"Trilinear (Forced)", 2}};
			for (const auto& [name, value] : kTri)
			{
				if (!std::strcmp(var.value, name))
				{
					s_base_settings->SetIntValue("EmuCore/GS", "TriFilter", value);
					break;
				}
			}
		}

		var = {"armsx2_mipmapping", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetBoolValue("EmuCore/GS", "hw_mipmap", !std::strcmp(var.value, "enabled"));

		var = {"armsx2_fxaa", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetBoolValue("EmuCore/GS", "fxaa", !std::strcmp(var.value, "enabled"));

		var = {"armsx2_ee_cycle_rate", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			static constexpr std::pair<const char*, int> kRates[] = {{"50%", -3}, {"60%", -2}, {"75%", -1},
				{"100%", 0}, {"130%", 1}, {"180%", 2}, {"300%", 3}};
			for (const auto& [name, value] : kRates)
			{
				if (!std::strcmp(var.value, name))
				{
					s_base_settings->SetIntValue("EmuCore/Speedhacks", "EECycleRate", value);
					break;
				}
			}
		}

		var = {"armsx2_ee_cycle_skip", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			static constexpr const char* kSkips[] = {"disabled", "mild", "moderate", "maximum"};
			for (size_t i = 0; i < std::size(kSkips); i++)
			{
				if (!std::strcmp(var.value, kSkips[i]))
				{
					s_base_settings->SetIntValue("EmuCore/Speedhacks", "EECycleSkip", static_cast<int>(i));
					break;
				}
			}
		}

		var = {"armsx2_texture_filtering", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			// Indices match the BiFiltering enum.
			static constexpr const char* kFilters[] = {"Nearest", "Bilinear (Forced)", "Bilinear (PS2)",
				"Bilinear (Forced excluding sprites)"};
			for (size_t i = 0; i < std::size(kFilters); i++)
			{
				if (!std::strcmp(var.value, kFilters[i]))
				{
					s_base_settings->SetIntValue("EmuCore/GS", "filter", static_cast<int>(i));
					break;
				}
			}
		}

		var = {"armsx2_anisotropic_filtering", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetIntValue("EmuCore/GS", "MaxAnisotropy", std::clamp(atoi(var.value), 0, 16));

		var = {"armsx2_sw_threads", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetIntValue("EmuCore/GS", "extrathreads", std::clamp(atoi(var.value), 0, 4));

		var = {"armsx2_hw_download_mode", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			// Explicit values: EnabledForceFull (1) is deliberately not exposed.
			static constexpr std::pair<const char*, GSHardwareDownloadMode> kModes[] = {
				{"Accurate", GSHardwareDownloadMode::Enabled},
				{"Disable Readbacks", GSHardwareDownloadMode::NoReadbacks},
				{"Unsynchronized", GSHardwareDownloadMode::Unsynchronized},
				{"Disabled", GSHardwareDownloadMode::Disabled}};
			for (const auto& [name, value] : kModes)
			{
				if (!std::strcmp(var.value, name))
				{
					s_base_settings->SetIntValue("EmuCore/GS", "HWDownloadMode", static_cast<int>(value));
					break;
				}
			}
		}

		var = {"armsx2_mtvu", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetBoolValue("EmuCore/Speedhacks", "vuThread", !std::strcmp(var.value, "enabled"));

		var = {"armsx2_instant_vu1", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetBoolValue("EmuCore/Speedhacks", "vu1Instant", !std::strcmp(var.value, "enabled"));

		var = {"armsx2_bios", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			// Empty setting = FindBiosImage() picks the first valid image.
			s_base_settings->SetStringValue("Filenames", "BIOS",
				std::strcmp(var.value, "auto") ? var.value : "");
		}

		var = {"armsx2_cheats", nullptr};
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			s_base_settings->SetBoolValue("EmuCore", "EnableCheats", !std::strcmp(var.value, "enabled"));
	}

	if (startup)
		VMManager::Internal::LoadStartupSettings();
	else
		Host::RunOnCPUThread([]() { VMManager::ApplySettings(); }, false);
}

//////////////////////////////////////////////////////////////////////////
// Vulkan context negotiation (M2) — the lrps2 pattern: create_device runs on
// the frontend thread, stashes the shared instance/GPU + frontend
// requirements into VKLibretro::Init, then opens MTGS. GSDeviceVK (on the GS
// thread) adopts the instance, and its vkCreateDevice call is intercepted by
// the VKLibretro wraps, which capture the resulting VkDevice for the context
// reply below.
//////////////////////////////////////////////////////////////////////////

static const VkApplicationInfo* GetVulkanApplicationInfo(void)
{
	static VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app_info.pApplicationName = "ARMSX2";
	app_info.applicationVersion = VK_MAKE_VERSION(2, 0, 0);
	app_info.pEngineName = "ARMSX2";
	app_info.engineVersion = VK_MAKE_VERSION(2, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_1;
	return &app_info;
}

static bool CreateVulkanDevice(retro_vulkan_context* context, VkInstance instance, VkPhysicalDevice gpu,
	VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr, const char** required_device_extensions,
	unsigned num_required_device_extensions, const char** required_device_layers,
	unsigned num_required_device_layers, const VkPhysicalDeviceFeatures* required_features)
{
	VKLibretro::Init.instance = instance;
	VKLibretro::Init.gpu = gpu;
	VKLibretro::Init.get_instance_proc_addr = get_instance_proc_addr;
	VKLibretro::Init.required_device_extensions = required_device_extensions;
	VKLibretro::Init.num_required_device_extensions = num_required_device_extensions;
	VKLibretro::Init.required_device_layers = required_device_layers;
	VKLibretro::Init.num_required_device_layers = num_required_device_layers;
	VKLibretro::Init.required_features = required_features;

	// Bring up the GS thread now: GSDeviceVK adopts Init.instance/gpu and the
	// wrapped vkCreateDevice fills Init.device with the shared device.
	if (!MTGS::IsOpen() && !MTGS::WaitForOpen())
	{
		log_cb(RETRO_LOG_ERROR, "MTGS::WaitForOpen failed during Vulkan negotiation.\n");
		return false;
	}

	GSDeviceVK* dev = GSDeviceVK::GetInstance();
	if (!dev || VKLibretro::Init.device == VK_NULL_HANDLE)
	{
		log_cb(RETRO_LOG_ERROR, "GS device missing after negotiation open.\n");
		return false;
	}

	context->gpu = dev->GetPhysicalDevice();
	context->device = VKLibretro::Init.device;
	context->queue = dev->GetGraphicsQueue();
	context->queue_family_index = dev->GetGraphicsQueueFamilyIndex();
	context->presentation_queue = context->queue;
	context->presentation_queue_family_index = context->queue_family_index;
	return true;
}

static void OnContextReset(void)
{
	retro_hw_render_interface* iface = nullptr;
	if (!environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, &iface) || !iface ||
		iface->interface_type != RETRO_HW_RENDER_INTERFACE_VULKAN)
	{
		log_cb(RETRO_LOG_ERROR, "Failed to get Vulkan HW render interface.\n");
		return;
	}
	VKLibretro::SetHWRenderInterface(iface);
	VKLibretro::SetPacing(true);
	LibretroCore::s_context_ready.store(true, std::memory_order_release);
}

static void OnContextDestroy(void)
{
	VKLibretro::AbortPacing();
	LibretroCore::s_context_ready.store(false, std::memory_order_release);
	VKLibretro::SetHWRenderInterface(nullptr);
}

//////////////////////////////////////////////////////////////////////////
// libretro API
//////////////////////////////////////////////////////////////////////////

RETRO_API unsigned retro_api_version(void)
{
	return RETRO_API_VERSION;
}

RETRO_API void retro_set_environment(retro_environment_t cb)
{
	environ_cb = cb;

	struct retro_log_callback log_iface;
	if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_iface))
		log_cb = log_iface.log;
	else
		log_cb = FallbackLog;

	bool support_no_game = false;
	cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &support_no_game);

	PopulateBiosOptions(cb);

	unsigned options_version = 0;
	if (cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &options_version) && options_version >= 2)
		cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &kOptionsV2);
	else
		cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)kCoreVariables);
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb)
{
	video_cb = cb;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb)
{
	audio_sample_cb = cb;
}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	audio_batch_cb = cb;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t cb)
{
	input_poll_cb = cb;
}

RETRO_API void retro_set_input_state(retro_input_state_t cb)
{
	input_state_cb = cb;
}

RETRO_API void retro_init(void)
{
	Log::SetConsoleOutputLevel(LOGLEVEL_INFO);
	LibretroCore::s_frame_buffer.assign(
		LibretroCore::kFrameWidth * LibretroCore::kFrameHeight, 0);
}

RETRO_API void retro_deinit(void)
{
	LibretroCore::s_frame_buffer.clear();
	LibretroCore::s_frame_buffer.shrink_to_fit();
}

RETRO_API void retro_get_system_info(struct retro_system_info* info)
{
	std::memset(info, 0, sizeof(*info));
	info->library_name = "ARMSX2";
	info->library_version = GIT_REV;
	info->valid_extensions = "elf|iso|ciso|chd|cso|zso|bin|mdf|nrg|dump|gz|img|irx|m3u";
	info->need_fullpath = true;
	info->block_extract = true;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info* info)
{
	std::memset(info, 0, sizeof(*info));
	info->geometry.base_width = LibretroCore::kFrameWidth;
	info->geometry.base_height = LibretroCore::kFrameHeight;
	info->geometry.max_width = VKLibretro::kMaxCanvasWidth;
	info->geometry.max_height = VKLibretro::kMaxCanvasHeight;
	info->geometry.aspect_ratio = 4.0f / 3.0f;
	info->timing.fps = 59.94;
	info->timing.sample_rate = 48000.0;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
}

RETRO_API void retro_reset(void)
{
	if (VMManager::HasValidVM())
		VMManager::SetState(VMState::Resetting);
}

RETRO_API bool retro_load_game(const struct retro_game_info* game)
{
	int format = RETRO_PIXEL_FORMAT_XRGB8888;
	if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format))
	{
		log_cb(RETRO_LOG_ERROR, "XRGB8888 not supported by frontend.\n");
		return false;
	}

	CrashHandler::Install();

	if (!LibretroCore::InitializeConfig())
	{
		log_cb(RETRO_LOG_ERROR, "Failed to initialize config.\n");
		return false;
	}

	ApplyCoreOptions(true);

	VMBootParameters params;
	if (game && game->path)
	{
		LibretroCore::s_content_path = game->path;
		if (StringUtil::EndsWithNoCase(LibretroCore::s_content_path, ".m3u"))
		{
			const std::string first = LoadM3UPlaylist(LibretroCore::s_content_path);
			if (first.empty())
			{
				log_cb(RETRO_LOG_ERROR, "Empty or unreadable m3u playlist.\n");
				return false;
			}
			params.filename = first;
		}
		else
		{
			s_disk_images = {LibretroCore::s_content_path};
			params.filename = LibretroCore::s_content_path;
		}
		s_disk_index = 0;
		s_disk_ejected = false;
		RegisterDiskControl();
	}
	else
	{
		params.source_type = CDVD_SourceType::NoDisc;
	}
	params.fast_boot = true;

	s_shutdown_requested.store(false, std::memory_order_release);

	// Vulkan HW render. The negotiation interface must be registered inside
	// retro_load_game; the frontend invokes it while creating its Vulkan
	// context, after this returns.
	LibretroCore::s_hw_render_vulkan = true;
	if (LibretroCore::s_hw_render_vulkan)
	{
		static struct retro_hw_render_callback hw_render = {};
		hw_render.context_type = RETRO_HW_CONTEXT_VULKAN;
		hw_render.version_major = 1;
		hw_render.version_minor = 1;
		hw_render.context_reset = OnContextReset;
		hw_render.context_destroy = OnContextDestroy;
		hw_render.cache_context = true;
		if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
		{
			log_cb(RETRO_LOG_ERROR, "Frontend refused Vulkan HW context; falling back to Null GS.\n");
			LibretroCore::s_hw_render_vulkan = false;
			auto lock = Host::GetSettingsLock();
			s_base_settings->SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::Null));
			VMManager::Internal::LoadStartupSettings();
		}
		else
		{
			static const struct retro_hw_render_context_negotiation_interface_vulkan neg_iface = {
				RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN,
				RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION,
				GetVulkanApplicationInfo,
				CreateVulkanDevice,
				nullptr, // destroy_device
			};
			environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE, (void*)&neg_iface);

			Error vk_error;
			if (!Vulkan::IsVulkanLibraryLoaded() && !Vulkan::LoadVulkanLibrary(&vk_error))
			{
				log_cb(RETRO_LOG_ERROR, "LoadVulkanLibrary: %s\n", vk_error.GetDescription().c_str());
				return false;
			}
			VKLibretro::InstallWraps();
			VKLibretro::Active = true;
		}
	}

	SysMemory::ReserveMemory();

	LibretroCore::s_cpu_thread = std::thread([params = std::move(params)]() mutable {
		LibretroCore::CPUThreadMain(std::move(params));
	});

	// The negotiation callback (frontend thread, after we return) opens MTGS;
	// global state it depends on comes from CPUThreadInitialize — wait for it.
	while (!LibretroCore::s_cpu_thread_initialized.load(std::memory_order_acquire))
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	return true;
}

RETRO_API bool retro_load_game_special(unsigned game_type, const struct retro_game_info* info, size_t num_info)
{
	return false;
}

RETRO_API void retro_unload_game(void)
{
	// The frontend replays the last set_image indefinitely (menu background,
	// duped frames) — retract it and wait for the GPU before the VM teardown
	// below destroys the textures it points at.
	if (auto* vulkan = static_cast<retro_hw_render_interface_vulkan*>(VKLibretro::GetHWRenderInterface()))
	{
		vulkan->set_image(vulkan->handle, nullptr, 0, nullptr, vulkan->queue_index);
		vulkan->wait_sync_index(vulkan->handle);
	}

	VKLibretro::AbortPacing(); // GS thread may be parked in PublishFrame
	s_shutdown_requested.store(true, std::memory_order_release);
	if (VMManager::HasValidVM())
		VMManager::SetState(VMState::Stopping);
	s_cpu_queue_cv.notify_all();
	if (LibretroCore::s_cpu_thread.joinable())
		LibretroCore::s_cpu_thread.join();

	s_base_settings.reset();
	s_secrets_settings.reset();
	LibretroCore::s_content_path.clear();
	s_disk_images.clear();
	s_disk_index = 0;
	s_disk_ejected = false;
	VKLibretro::Shutdown();
	VKLibretro::Active = false;
	LibretroCore::s_context_ready.store(false, std::memory_order_release);
	LibretroCore::s_cpu_thread_initialized.store(false, std::memory_order_release);
}

RETRO_API void retro_run(void)
{
	input_poll_cb();

	bool options_updated = false;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &options_updated) && options_updated)
		ApplyCoreOptions(false);

	// M3 input: forward the libretro joypad straight into the DualShock2
	// bind slots (bypasses InputManager entirely).
	if (VMManager::HasValidVM())
	{
		if (PadBase* pad = Pad::GetPad(0))
		{
			static constexpr struct
			{
				unsigned retro;
				u32 ds2;
			} bmap[] = {
				{RETRO_DEVICE_ID_JOYPAD_UP, PadDualshock2::Inputs::PAD_UP},
				{RETRO_DEVICE_ID_JOYPAD_RIGHT, PadDualshock2::Inputs::PAD_RIGHT},
				{RETRO_DEVICE_ID_JOYPAD_DOWN, PadDualshock2::Inputs::PAD_DOWN},
				{RETRO_DEVICE_ID_JOYPAD_LEFT, PadDualshock2::Inputs::PAD_LEFT},
				{RETRO_DEVICE_ID_JOYPAD_X, PadDualshock2::Inputs::PAD_TRIANGLE},
				{RETRO_DEVICE_ID_JOYPAD_A, PadDualshock2::Inputs::PAD_CIRCLE},
				{RETRO_DEVICE_ID_JOYPAD_B, PadDualshock2::Inputs::PAD_CROSS},
				{RETRO_DEVICE_ID_JOYPAD_Y, PadDualshock2::Inputs::PAD_SQUARE},
				{RETRO_DEVICE_ID_JOYPAD_SELECT, PadDualshock2::Inputs::PAD_SELECT},
				{RETRO_DEVICE_ID_JOYPAD_START, PadDualshock2::Inputs::PAD_START},
				{RETRO_DEVICE_ID_JOYPAD_L, PadDualshock2::Inputs::PAD_L1},
				{RETRO_DEVICE_ID_JOYPAD_L2, PadDualshock2::Inputs::PAD_L2},
				{RETRO_DEVICE_ID_JOYPAD_R, PadDualshock2::Inputs::PAD_R1},
				{RETRO_DEVICE_ID_JOYPAD_R2, PadDualshock2::Inputs::PAD_R2},
				{RETRO_DEVICE_ID_JOYPAD_L3, PadDualshock2::Inputs::PAD_L3},
				{RETRO_DEVICE_ID_JOYPAD_R3, PadDualshock2::Inputs::PAD_R3},
			};
			for (const auto& m : bmap)
				pad->Set(m.ds2, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, m.retro) ? 1.0f : 0.0f);

			// Analog sticks: split each axis into the two directional slots.
			const auto axis = [](s16 v, bool positive) {
				const float f = std::clamp(static_cast<float>(v) / 32767.0f, -1.0f, 1.0f);
				return positive ? std::max(f, 0.0f) : std::max(-f, 0.0f);
			};
			const s16 lx = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
			const s16 ly = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
			const s16 rx = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
			const s16 ry = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
			pad->Set(PadDualshock2::Inputs::PAD_L_RIGHT, axis(lx, true));
			pad->Set(PadDualshock2::Inputs::PAD_L_LEFT, axis(lx, false));
			pad->Set(PadDualshock2::Inputs::PAD_L_DOWN, axis(ly, true));
			pad->Set(PadDualshock2::Inputs::PAD_L_UP, axis(ly, false));
			pad->Set(PadDualshock2::Inputs::PAD_R_RIGHT, axis(rx, true));
			pad->Set(PadDualshock2::Inputs::PAD_R_LEFT, axis(rx, false));
			pad->Set(PadDualshock2::Inputs::PAD_R_DOWN, axis(ry, true));
			pad->Set(PadDualshock2::Inputs::PAD_R_UP, axis(ry, false));
		}
	}

	if (LibretroCore::s_hw_render_vulkan)
	{
		// M2: consume the newest GS frame (if any) and hand it to the
		// frontend. The retro_vulkan_image storage must outlive this call --
		// the frontend keeps the pointer for cached-frame replays.
		VKLibretro::Frame frame;
		auto* vulkan = static_cast<retro_hw_render_interface_vulkan*>(VKLibretro::GetHWRenderInterface());
		if (vulkan && VKLibretro::ConsumeFrame(&frame))
		{
			// The GS present path sizes the canvas to the (aspect-expanded)
			// merged frame, so it changes with the internal resolution — keep
			// the frontend's geometry in sync so scaling stays correct.
			static u32 last_geometry_width = 0;
			static u32 last_geometry_height = 0;
			if (frame.width != last_geometry_width || frame.height != last_geometry_height)
			{
				last_geometry_width = frame.width;
				last_geometry_height = frame.height;
				retro_game_geometry geometry = {};
				geometry.base_width = frame.width;
				geometry.base_height = frame.height;
				geometry.max_width = VKLibretro::kMaxCanvasWidth;
				geometry.max_height = VKLibretro::kMaxCanvasHeight;
				// The canvas is already aspect-corrected; display it 1:1.
				geometry.aspect_ratio = static_cast<float>(frame.width) / static_cast<float>(frame.height);
				environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry);
			}
			static retro_vulkan_image vkimage;
			vkimage = {};
			vkimage.image_view = frame.view;
			vkimage.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkimage.create_info = {
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0,
				frame.image, VK_IMAGE_VIEW_TYPE_2D, frame.format,
				{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
				{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
			vulkan->set_image(vulkan->handle, &vkimage, 0, nullptr, vulkan->queue_index);
			video_cb(RETRO_HW_FRAME_BUFFER_VALID, frame.width, frame.height, 0);
		}
		else
		{
			video_cb(nullptr, LibretroCore::kFrameWidth, LibretroCore::kFrameHeight, 0);
		}
	}
	else
	{
		// Null-GS fallback: placeholder software frame.
		video_cb(LibretroCore::s_frame_buffer.data(), LibretroCore::kFrameWidth,
			LibretroCore::kFrameHeight, LibretroCore::kFrameWidth * sizeof(u32));
	}

	// PAL/NTSC: retro_get_system_av_info runs before the VM boots, so the
	// initial reply assumes NTSC. Once the VM reports a different vertical
	// frequency (PAL 50Hz, progressive modes), update the frontend.
	if (VMManager::HasValidVM())
	{
		static float reported_fps = 59.94f;
		const float fps = VMManager::GetFrameRate();
		if (fps > 1.0f && std::abs(fps - reported_fps) > 0.25f)
		{
			reported_fps = fps;
			struct retro_system_av_info av;
			retro_get_system_av_info(&av);
			av.timing.fps = fps;
			environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
		}
	}

	// M3 audio: drain whatever SPU2 mixed since the last retro_run out of the
	// (null-backend) stream ring and hand it to the frontend.
	if (AudioStream* stream = SPU2::GetOutputStream())
	{
		static AudioStream::SampleType fbuf[2048 * 2];
		static s16 abuf[2048 * 2];
		u32 frames;
		while ((frames = stream->PullFrames(fbuf, 2048)) > 0)
		{
			for (u32 i = 0; i < frames * 2; i++)
				abuf[i] = static_cast<s16>(std::clamp(fbuf[i], -1.0f, 1.0f) * 32767.0f);
			const s16* p = abuf;
			while (frames > 0)
			{
				const size_t sent = audio_batch_cb(p, frames);
				p += sent * 2;
				frames -= static_cast<u32>(sent);
				if (!sent)
					break;
			}
		}
	}
}

// Fixed upper bound: SaveState_DownloadState uses a 64 MiB working buffer;
// zstd-compressed zip output is far below that. libretro requires a stable
// size, so report the bound plus slack for headers/screenshot.
static constexpr size_t kSerializeSize = 68 * 1024 * 1024;

RETRO_API size_t retro_serialize_size(void)
{
	return VMManager::HasValidVM() ? kSerializeSize : 0;
}

RETRO_API bool retro_serialize(void* data, size_t size)
{
	if (!VMManager::HasValidVM())
		return false;

	// Pacing must be off while retro_run isn't being called, or the GS
	// thread stays parked in PublishFrame and the state freeze (which needs
	// the GS thread to respond) deadlocks.
	VKLibretro::SetPacing(false);

	std::vector<u8> buffer;
	bool ok = false;
	Host::RunOnCPUThread([&buffer, &ok]() {
		if (!VMManager::HasValidVM())
			return;
		Error error;
		std::unique_ptr<ArchiveEntryList> elist = SaveState_DownloadState(&error);
		if (!elist)
		{
			Console.ErrorFmt("retro_serialize: DownloadState failed: {}", error.GetDescription());
			return;
		}
		ok = SaveState_ZipToBuffer(std::move(elist), SaveState_SaveScreenshot(), &buffer, &error);
		if (!ok)
			Console.ErrorFmt("retro_serialize: ZipToBuffer failed: {}", error.GetDescription());
	}, true);

	if (LibretroCore::s_context_ready.load(std::memory_order_acquire))
		VKLibretro::SetPacing(true);

	if (!ok || sizeof(u64) + buffer.size() > size)
	{
		if (ok)
			log_cb(RETRO_LOG_ERROR, "State (8+%zu bytes) exceeds serialize buffer (%zu).\n", buffer.size(), size);
		return false;
	}

	// Leading u64 length, then the zip. The buffer libretro hands back to
	// retro_unserialize is the full fixed-size block, so the real length has
	// to travel inside it.
	const u64 zip_len = buffer.size();
	std::memcpy(data, &zip_len, sizeof(zip_len));
	std::memcpy(static_cast<u8*>(data) + sizeof(zip_len), buffer.data(), buffer.size());
	return true;
}

RETRO_API bool retro_unserialize(const void* data, size_t size)
{
	if (!VMManager::HasValidVM() || size < sizeof(u64))
		return false;

	u64 zip_size;
	std::memcpy(&zip_size, data, sizeof(u64));
	if (zip_size == 0 || zip_size > size - sizeof(u64))
		return false;

	VKLibretro::SetPacing(false);

	bool ok = false;
	const u8* zip_data = static_cast<const u8*>(data) + sizeof(u64);
	Host::RunOnCPUThread([zip_data, zip_size, &ok]() {
		if (!VMManager::HasValidVM())
			return;
		Error error;
		ok = SaveState_UnzipFromBuffer(zip_data, static_cast<size_t>(zip_size), &error);
		if (!ok)
			Console.ErrorFmt("retro_unserialize failed: {}", error.GetDescription());
	}, true);

	if (LibretroCore::s_context_ready.load(std::memory_order_acquire))
		VKLibretro::SetPacing(true);

	return ok;
}

RETRO_API void retro_cheat_reset(void)
{
}

RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char* code)
{
}

RETRO_API unsigned retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}

RETRO_API void* retro_get_memory_data(unsigned id)
{
	return nullptr;
}

RETRO_API size_t retro_get_memory_size(unsigned id)
{
	return 0;
}
