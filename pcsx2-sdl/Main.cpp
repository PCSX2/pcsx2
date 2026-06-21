// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// pcsx2-sdl — SDL3 frontend for kmsdrm-only handhelds.
//
// Boots a game from a CLI ISO path, or wires PCSX2's existing FullscreenUI
// (ImGui) for in-game settings, game-picker, and configuration. With no ISO
// supplied (and the "UI"/"StartBigPictureMode" flag set, which is on by
// default for this frontend), comes up directly into the FullscreenUI
// game-picker.
//
// Display surface is acquired via Vulkan VK_KHR_display, so no Wayland/X11
// compositor is needed. The Vulkan renderer enumerates monitors itself; this
// frontend only reports the requested resolution back through WindowInfo.
//
// Audio + input come from SDL3 via the existing SDLAudioStream / SDLInputSource
// modules in the core (already linked, already non-Qt).

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#include "fmt/format.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/CrashHandler.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/ProgressCallback.h"
#include "common/StringUtil.h"
#include "common/Threading.h"

#include "pcsx2/PrecompiledHeader.h"

#include "pcsx2/Achievements.h"
#include "pcsx2/CDVD/CDVDcommon.h"
#include "pcsx2/GS.h"
#include "pcsx2/GameList.h"
#include "pcsx2/Host.h"
#include "pcsx2/INISettingsInterface.h"
#include "pcsx2/ImGui/FullscreenUI.h"
#include "pcsx2/ImGui/ImGuiFullscreen.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/Input/InputManager.h"
#include "pcsx2/MTGS.h"
#include "pcsx2/PerformanceMetrics.h"
#include "pcsx2/SIO/Pad/Pad.h"
#include "pcsx2/VMManager.h"

#include "svnrev.h"

namespace Pcsx2SDL
{
	static bool InitializeConfig();
	static bool ParseCommandLineArgs(int argc, char* argv[], VMBootParameters& params);
	static void InstallSignalHandler();
	static std::optional<WindowInfo> BuildWindowInfo();
	static void CPUThreadMain(VMBootParameters initial_params, bool start_in_fsui, std::atomic<int>* ret);
	static void DrainCPUThreadQueue();
	static void StopGameListRefreshThread();
} // namespace Pcsx2SDL

// Settings persistence (INI on disk).
static std::unique_ptr<INISettingsInterface> s_base_settings;
static std::unique_ptr<INISettingsInterface> s_secrets_settings;

// Shutdown signal from SIGTERM/SIGINT or VM exit.
static std::atomic<bool> s_shutdown_requested{false};

// Display mode requested via --fullscreen-mode (0 = let the renderer pick the
// display's preferred mode).
static u32 s_requested_width = 0;
static u32 s_requested_height = 0;

// Pending CPU-thread callbacks queued by Host::RunOnCPUThread (FullscreenUI
// uses this to schedule work back from the GS thread, e.g. "user picked an
// ISO from the game list, please VMManager::Initialize it on the CPU thread").
// Drained by Host::PumpMessagesOnCPUThread which the VM polls every vsync;
// also drained on a 16ms tick by the idle loop while no VM is running.
static std::mutex s_cpu_queue_lock;
static std::deque<std::function<void()>> s_cpu_queue;
static std::condition_variable s_cpu_queue_cv;
// Set once the CPU thread is up; used to detect "RunOnCPUThread(block=true)"
// being called from the CPU thread itself (which would self-deadlock).
static std::atomic<std::thread::id> s_cpu_thread_id{};

// Background game-list scanner. FullscreenUI's GameList page calls
// Host::RefreshGameListAsync, which spawns a single worker thread to
// GameList::Refresh. The thread is joined (blocking) at shutdown and at
// restart, to avoid two scans racing.
static std::thread s_gamelist_thread;
static std::atomic<bool> s_gamelist_running{false};

//////////////////////////////////////////////////////////////////////////
// Settings + lifecycle
//////////////////////////////////////////////////////////////////////////

bool Pcsx2SDL::InitializeConfig()
{
	EmuFolders::SetAppRoot();
	if (!EmuFolders::SetResourcesDirectory() || !EmuFolders::SetDataDirectory(nullptr))
		return false;

	CrashHandler::SetWriteDirectory(EmuFolders::DataRoot);

	const char* hw_check_error = nullptr;
	if (!VMManager::PerformEarlyHardwareChecks(&hw_check_error))
	{
		Console.ErrorFmt("Early hardware check failed: {}", hw_check_error ? hw_check_error : "unknown");
		return false;
	}

	// Load Roboto for OSD / FullscreenUI (font is bundled in resources).
	{
		const std::string roboto_path =
			EmuFolders::GetOverridableResourcePath("fonts" FS_OSPATH_SEPARATOR_STR "Roboto-Regular.ttf");
		const auto roboto_data = FileSystem::MapBinaryFileForRead(roboto_path.c_str());
		if (roboto_data.empty())
		{
			Console.ErrorFmt("Failed to load font file '{}'.", roboto_path);
			return false;
		}

		std::vector<ImGuiManager::FontInfo> fonts;
		ImGuiManager::FontInfo fi{};
		fi.data = roboto_data;
		fi.exclude_ranges = {};
		fi.face_name = nullptr;
		fi.is_emoji_font = false;
		fonts.push_back(fi);
		ImGuiManager::SetFonts(std::move(fonts));
	}

	// Open / create the persistent INI at $XDG_CONFIG_HOME/PCSX2/PCSX2.ini.
	const std::string ini_path = Path::Combine(EmuFolders::Settings, "PCSX2.ini");
	const bool ini_exists = FileSystem::FileExists(ini_path.c_str());
	Console.WriteLnFmt("Loading config from {}.", ini_path);

	s_base_settings = std::make_unique<INISettingsInterface>(ini_path);
	Host::Internal::SetBaseSettingsLayer(s_base_settings.get());

	if (!ini_exists || !s_base_settings->Load() || !VMManager::Internal::CheckSettingsVersion())
	{
		Console.WriteLnFmt("Initialising fresh config at {}.", ini_path);
		VMManager::SetDefaultSettings(*s_base_settings, true, true, true, true, true);
	}

	// Secrets layer (achievements credentials etc). Created on first run;
	// failure to create is non-fatal — just log and continue.
	const std::string secrets_path = Path::Combine(EmuFolders::Settings, "secrets.ini");
	s_secrets_settings = std::make_unique<INISettingsInterface>(secrets_path);
	Host::Internal::SetSecretsSettingsLayer(s_secrets_settings.get());
	if (FileSystem::FileExists(secrets_path.c_str()))
		s_secrets_settings->Load();

	// Apply handheld-frontend defaults. These override anything the INI
	// might have for fields that don't make sense without a desktop:
	//  - Vulkan renderer (only one with VK_KHR_display direct-display path).
	//  - Fullscreen always (no window manager to support windowed mode).
	//  - SDL audio + SDL gamepad input (no Qt-coupled keyboard input layer).
	{
		auto lock = Host::GetSettingsLock();
		s_base_settings->SetBoolValue("InputSources", "SDL", true);
		// Don't disable user-set values they may have customised — only fill
		// in missing defaults for first-run.
		if (!s_base_settings->ContainsValue("EmuCore/GS", "Renderer"))
			s_base_settings->SetIntValue("EmuCore/GS", "Renderer",
				static_cast<int>(GSRendererType::VK));
		if (!s_base_settings->ContainsValue("SPU2/Output", "OutputModule"))
			s_base_settings->SetStringValue("SPU2/Output", "OutputModule", "sdl");
		if (!s_base_settings->ContainsValue("EmuCore/GS", "FullscreenMode"))
			s_base_settings->SetBoolValue("EmuCore/GS", "FullscreenMode", true);
	}

	// Persist any first-run defaults so the user can hand-edit
	// the INI between sessions.
	Error save_error;
	if (!s_base_settings->Save(&save_error))
		Console.ErrorFmt("Failed to save config: {}", save_error.GetDescription());

	VMManager::Internal::LoadStartupSettings();
	return true;
}

bool Pcsx2SDL::ParseCommandLineArgs(int argc, char* argv[], VMBootParameters& params)
{
	for (int i = 1; i < argc; i++)
	{
#define ARG(s) (!std::strcmp(argv[i], s))
#define ARG_PARAM(s) (!std::strcmp(argv[i], s) && (i + 1) < argc)

		if (ARG("--help") || ARG("-h"))
		{
			std::fprintf(stderr, "PCSX2 SDL frontend %s\n", GIT_REV);
			std::fprintf(stderr, "Usage: %s [options] <iso-path>\n\n", argv[0]);
			std::fprintf(stderr, "  --fullscreen-mode WxH    Request a specific display mode\n");
			std::fprintf(stderr, "                           (default: monitor's preferred mode)\n");
			std::fprintf(stderr, "  --bios-only              Boot the BIOS without loading a disc\n");
			std::fprintf(stderr, "  --state-from-file PATH   Resume from a save state file\n");
			std::fprintf(stderr, "  --no-fast-boot           Skip fast boot (run full BIOS animation)\n");
			std::fprintf(stderr, "  -h, --help               Show this help and exit\n");
			std::fprintf(stderr, "  --version                Show version and exit\n");
			return false;
		}
		if (ARG("--version"))
		{
			std::fprintf(stderr, "PCSX2 SDL frontend %s\n", GIT_REV);
			return false;
		}
		if (ARG_PARAM("--fullscreen-mode"))
		{
			const char* mode = argv[++i];
			const char* x_pos = std::strchr(mode, 'x');
			if (!x_pos)
			{
				Console.ErrorFmt("Invalid --fullscreen-mode '{}', expected WxH (e.g. 1280x720).", mode);
				return false;
			}
			s_requested_width = StringUtil::FromChars<u32>(std::string_view(mode, x_pos - mode)).value_or(0);
			s_requested_height = StringUtil::FromChars<u32>(x_pos + 1).value_or(0);
			if (s_requested_width == 0 || s_requested_height == 0)
			{
				Console.ErrorFmt("Invalid --fullscreen-mode '{}'.", mode);
				return false;
			}
			continue;
		}
		if (ARG("--bios-only"))
		{
			params.source_type = CDVD_SourceType::NoDisc;
			continue;
		}
		if (ARG_PARAM("--state-from-file"))
		{
			params.save_state = argv[++i];
			continue;
		}
		if (ARG("--no-fast-boot"))
		{
			params.fast_boot = false;
			continue;
		}
		if (argv[i][0] == '-')
		{
			Console.ErrorFmt("Unknown argument: '{}'", argv[i]);
			return false;
		}

		// Positional: ISO path.
		if (!params.filename.empty())
		{
			Console.Error("Multiple ISO paths supplied; expected exactly one.");
			return false;
		}
		params.filename = argv[i];

#undef ARG
#undef ARG_PARAM
	}

	// Empty positional + no --bios-only is allowed: the frontend boots into
	// FullscreenUI's game-picker if "UI/StartBigPictureMode" is set
	// (the default for this frontend; see Host::SetDefaultUISettings).
	// The decision happens in main() after settings are loaded.

	if (!params.fast_boot.has_value())
		params.fast_boot = true;
	if (!params.fullscreen.has_value())
		params.fullscreen = true;

	return true;
}

static void HandleSignal(int)
{
	s_shutdown_requested.store(true, std::memory_order_release);
	if (VMManager::HasValidVM())
		VMManager::SetState(VMState::Stopping);
}

void Pcsx2SDL::InstallSignalHandler()
{
	std::signal(SIGTERM, &HandleSignal);
	std::signal(SIGINT, &HandleSignal);
	std::signal(SIGHUP, &HandleSignal);
}

std::optional<WindowInfo> Pcsx2SDL::BuildWindowInfo()
{
	WindowInfo wi;
	wi.type = WindowInfo::Type::VulkanDirect;
	wi.surface_width = s_requested_width;
	wi.surface_height = s_requested_height;
	wi.surface_scale = 1.0f;
	wi.display_connection = nullptr;
	wi.window_handle = nullptr;
	wi.surface_handle = nullptr;
	return wi;
}

//////////////////////////////////////////////////////////////////////////
// Host:: callbacks
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
	// No host-specific settings layer to merge in — everything lives in the
	// base INI.
}

void Host::CheckForSettingsChanges(const Pcsx2Config& old_config)
{
}

bool Host::RequestResetSettings(bool folders, bool core, bool controllers, bool hotkeys, bool ui)
{
	// FullscreenUI will trigger this; no UI is wired to drive it yet.
	return false;
}

void Host::SetDefaultUISettings(SettingsInterface& si)
{
	// Handheld defaults — start straight into FullscreenUI when no game is
	// loaded, hide pointer (no mouse), confirm via cross/A button.
	si.SetBoolValue("UI", "StartBigPictureMode", true);
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

std::optional<WindowInfo> Host::GetTopLevelWindowInfo()
{
	return Pcsx2SDL::BuildWindowInfo();
}

void Host::OnInputDeviceConnected(const std::string_view identifier, const std::string_view device_name)
{
	INFO_LOG("Input device connected: {} ({})", identifier, device_name);
}

void Host::OnInputDeviceDisconnected(const InputBindingKey key, const std::string_view identifier)
{
	INFO_LOG("Input device disconnected: {}", identifier);
}

void Host::SetMouseMode(bool relative_mode, bool hide_cursor)
{
}

void Host::SetMouseLock(bool state)
{
}

std::optional<WindowInfo> Host::AcquireRenderWindow(bool recreate_window)
{
	return Pcsx2SDL::BuildWindowInfo();
}

void Host::ReleaseRenderWindow()
{
}

void Host::BeginPresentFrame()
{
}

void Host::RequestResizeHostDisplay(s32 width, s32 height)
{
	// VK_KHR_display provides a fixed mode for the lifetime of the surface;
	// resize requests from the core are advisory only.
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

void Pcsx2SDL::DrainCPUThreadQueue()
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
	// Honour SIGTERM / SIGINT picked up by the signal handler.
	if (s_shutdown_requested.load(std::memory_order_acquire) && VMManager::HasValidVM())
		VMManager::SetState(VMState::Stopping);

	Pcsx2SDL::DrainCPUThreadQueue();
}

void Host::RunOnCPUThread(std::function<void()> function, bool block)
{
	if (block)
	{
		// Inline if already on the CPU thread to avoid self-deadlock.
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

void Pcsx2SDL::StopGameListRefreshThread()
{
	if (!s_gamelist_thread.joinable())
		return;
	s_gamelist_thread.join();
}

void Host::RefreshGameListAsync(bool invalidate_cache)
{
	// Only one scan at a time — FullscreenUI's "rescan" button can fire
	// multiple times in quick succession; coalesce by joining the previous
	// scan first.
	Pcsx2SDL::StopGameListRefreshThread();

	s_gamelist_running.store(true, std::memory_order_release);
	s_gamelist_thread = std::thread([invalidate_cache]() {
		Threading::SetNameOfCurrentThread("GameList Refresh");
		GameList::Refresh(invalidate_cache, false, nullptr);
		s_gamelist_running.store(false, std::memory_order_release);
	});
}

void Host::CancelGameListRefresh()
{
	Pcsx2SDL::StopGameListRefreshThread();
}

bool Host::IsFullscreen()
{
	return true;
}

void Host::SetFullscreen(bool enabled)
{
	// No-op: VK_KHR_display is always fullscreen; there's no compositor to
	// host a windowed mode.
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
	// FullscreenUI exit — shut down the application.
	Host::RequestExitApplication(false);
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
	return false;
}

bool Host::InNoGUIMode()
{
	return false;
}

bool Host::ShouldPreferHostFileSelector()
{
	return false;
}

void Host::OpenHostFileSelectorAsync(std::string_view title, bool select_directory, FileSelectorCallback callback,
	FileSelectorFilters filters, std::string_view initial_directory)
{
	// No native file picker on a kmsdrm-only handheld. FullscreenUI's own
	// game list / file browser handles this path.
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
// CPU thread + main
//////////////////////////////////////////////////////////////////////////

void Pcsx2SDL::CPUThreadMain(VMBootParameters initial_params, bool start_in_fsui, std::atomic<int>* ret)
{
	ret->store(EXIT_FAILURE);
	s_cpu_thread_id.store(std::this_thread::get_id(), std::memory_order_release);

	if (!VMManager::Internal::CPUThreadInitialize())
	{
		Console.Error("CPU thread init failed.");
		VMManager::Internal::CPUThreadShutdown();
		return;
	}

	VMManager::ApplySettings();

	// SDL doesn't enumerate already-connected gamepads at init time — the
	// initial SDL_EVENT_GAMEPAD_ADDED events sit in SDL's queue until
	// something calls SDL_PollEvent. Drain them now and rebind, so any
	// SDL2-style A/B/X/Y face-button bindings get migrated to SDL3 positional
	// names (FaceSouth/East/West/North) before use. Qt sidesteps this via its
	// background-poll QTimer; the SDL frontend must drain explicitly here.
	InputManager::ReloadDevices();
	VMManager::ReloadInputBindings(true);

	// Bring up the GS thread + display surface before booting anything when
	// starting straight into FullscreenUI. With a VM-on-startup path the
	// VMManager::Initialize call below will open MTGS itself; with the
	// bootless FSUI path it must be opened manually so the game-picker is
	// visible before the user selects a game.
	if (start_in_fsui)
	{
		ImGuiManager::InitializeFullscreenUI();
		if (!MTGS::WaitForOpen())
		{
			Console.Error("Failed to open MTGS for FullscreenUI startup.");
			VMManager::Internal::CPUThreadShutdown();
			return;
		}
		MTGS::SetRunIdle(true);
	}

	// The "initial" boot is the ISO/state passed on the CLI. When launching
	// straight into FullscreenUI instead, this stays empty and the user picks
	// a game from the game-list page (which queues a VMManager::Initialize
	// call back via Host::RunOnCPUThread).
	std::optional<VMBootParameters> pending_boot;
	if (!start_in_fsui)
		pending_boot = std::move(initial_params);

	bool clean_shutdown = true;

	// Main CPU thread state-machine loop. Exits on shutdown_requested OR on
	// VM shutdown when no FullscreenUI session is active to fall back to.
	while (!s_shutdown_requested.load(std::memory_order_acquire))
	{
		// Drain RunOnCPUThread callbacks (also done inside Execute via
		// PumpMessagesOnCPUThread, but must be drained in the no-VM idle
		// state too).
		Pcsx2SDL::DrainCPUThreadQueue();

		const VMState state = VMManager::GetState();
		switch (state)
		{
			case VMState::Initializing:
				// Transient — just spin until VMManager moves on.
				continue;

			case VMState::Running:
				VMManager::Execute();
				continue;

			case VMState::Resetting:
				VMManager::Reset();
				continue;

			case VMState::Stopping:
				VMManager::Shutdown(false);
				// After a clean shutdown, fall through to Shutdown / Paused.
				continue;

			case VMState::Paused:
			case VMState::Shutdown:
			{
				// If a CLI-supplied boot is pending, kick it now.
				if (pending_boot.has_value())
				{
					VMBootParameters bp = std::move(pending_boot.value());
					pending_boot.reset();
					const VMBootResult br = VMManager::Initialize(bp);
					if (br != VMBootResult::StartupSuccess)
					{
						Console.ErrorFmt("VMManager::Initialize failed (result {}).",
							static_cast<int>(br));
						clean_shutdown = false;
						s_shutdown_requested.store(true, std::memory_order_release);
						break;
					}
					VMManager::SetState(VMState::Running);
					continue;
				}

				// Nothing pending. If no FSUI session is running, exit cleanly
				// — the user's game finished and there is no UI to show next,
				// so the frontend's job is done.
				if (!start_in_fsui)
				{
					s_shutdown_requested.store(true, std::memory_order_release);
					break;
				}

				// FSUI idle: pump input so the gamepad can drive the menus.
				// Qt does this from a background QTimer; here it has to run
				// inline. Do it *outside* s_cpu_queue_lock — FSUI menu
				// callbacks can call Host::RunOnCPUThread, which takes the
				// same lock.
				VMManager::IdlePollUpdate();

				// Then wait for either a queued RunOnCPUThread (which might
				// Initialize a new VM) or a state change. Bounded timeout so
				// the shutdown flag is rechecked promptly.
				std::unique_lock<std::mutex> lock(s_cpu_queue_lock);
				s_cpu_queue_cv.wait_for(lock, std::chrono::milliseconds(16),
					[]() { return !s_cpu_queue.empty(); });
				continue;
			}

			default:
				continue;
		}
	}

	// Tear down in reverse order of setup. MTGS may already have been closed
	// by VMManager::Shutdown if VMManager opened it; if opened for FSUI
	// directly it will still be open here.
	if (VMManager::HasValidVM())
		VMManager::Shutdown(false);
	if (MTGS::IsOpen())
	{
		MTGS::SetRunIdle(false);
		MTGS::WaitForClose();
	}

	Pcsx2SDL::StopGameListRefreshThread();

	VMManager::Internal::CPUThreadShutdown();
	s_cpu_thread_id.store(std::thread::id{}, std::memory_order_release);
	ret->store(clean_shutdown ? EXIT_SUCCESS : EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
	// Short-circuit --help/--version before any heavyweight init so they
	// work even on a system where the resources dir hasn't been laid down.
	for (int i = 1; i < argc; i++)
	{
		if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h"))
		{
			std::fprintf(stderr, "PCSX2 SDL frontend %s\n", GIT_REV);
			std::fprintf(stderr, "Usage: %s [options] <iso-path>\n\n", argv[0]);
			std::fprintf(stderr, "  --fullscreen-mode WxH    Request a specific display mode\n");
			std::fprintf(stderr, "                           (default: monitor's preferred mode)\n");
			std::fprintf(stderr, "  --bios-only              Boot the BIOS without loading a disc\n");
			std::fprintf(stderr, "  --state-from-file PATH   Resume from a save state file\n");
			std::fprintf(stderr, "  --no-fast-boot           Skip fast boot (run full BIOS animation)\n");
			std::fprintf(stderr, "  -h, --help               Show this help and exit\n");
			std::fprintf(stderr, "  --version                Show version and exit\n");
			return EXIT_SUCCESS;
		}
		if (!std::strcmp(argv[i], "--version"))
		{
			std::fprintf(stderr, "PCSX2 SDL frontend %s\n", GIT_REV);
			return EXIT_SUCCESS;
		}
	}

	CrashHandler::Install();
	Log::SetConsoleOutputLevel(LOGLEVEL_INFO);

	if (!Pcsx2SDL::InitializeConfig())
	{
		Console.Error("Failed to initialize config.");
		return EXIT_FAILURE;
	}

	VMBootParameters params;
	if (!Pcsx2SDL::ParseCommandLineArgs(argc, argv, params))
		return EXIT_FAILURE;

	const bool have_boot_target = !params.filename.empty() || params.source_type.has_value();
	bool start_in_fsui = false;
	if (!have_boot_target)
	{
		// No ISO supplied and no --bios-only — only valid if the
		// StartBigPictureMode flag is set, in which case the frontend starts
		// in the FullscreenUI game-picker.
		if (Host::GetBaseBoolSettingValue("UI", "StartBigPictureMode", true))
		{
			start_in_fsui = true;
		}
		else
		{
			Console.Error("No ISO path supplied. Use --bios-only to boot the BIOS, "
						  "or set UI/StartBigPictureMode=true in PCSX2.ini for the game-picker.");
			return EXIT_FAILURE;
		}
	}

	Pcsx2SDL::InstallSignalHandler();
	SysMemory::ReserveMemory();

	std::atomic<int> thread_ret{EXIT_FAILURE};
	std::thread cpu_thread([&]() {
		Pcsx2SDL::CPUThreadMain(std::move(params), start_in_fsui, &thread_ret);
	});

	// VK_KHR_display has no host event loop; SDL3 input pumping happens
	// inside InputManager on the CPU thread; signals are async. The main
	// thread just waits for shutdown.
	cpu_thread.join();

	return thread_ret.load();
}
