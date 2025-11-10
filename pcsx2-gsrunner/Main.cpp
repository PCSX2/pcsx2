// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <condition_variable>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

#include "fmt/format.h"

#include "common/Assertions.h"
#include "common/CocoaTools.h"
#include "common/Console.h"
#include "common/CrashHandler.h"
#include "common/FileSystem.h"
#include "common/MemorySettingsInterface.h"
#include "common/Path.h"
#include "common/ProgressCallback.h"
#include "common/SettingsWrapper.h"
#include "common/StringUtil.h"

#include "pcsx2/PrecompiledHeader.h"

#include "pcsx2/Achievements.h"
#include "pcsx2/CDVD/CDVD.h"
#include "pcsx2/GS.h"
#include "pcsx2/GS/GSPerfMon.h"
#include "pcsx2/GSDumpReplayer.h"
#include "pcsx2/GameList.h"
#include "pcsx2/Host.h"
#include "pcsx2/INISettingsInterface.h"
#include "pcsx2/ImGui/FullscreenUI.h"
#include "pcsx2/ImGui/ImGuiFullscreen.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/Input/InputManager.h"
#include "pcsx2/MTGS.h"
#include "pcsx2/SIO/Pad/Pad.h"
#include "pcsx2/PerformanceMetrics.h"
#include "pcsx2/VMManager.h"

#include "svnrev.h"

// Down here because X11 has a lot of defines that can conflict
#if defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/select.h>
#include <unistd.h>
#endif

namespace GSRunner
{
	static void InitializeConsole();
	static bool InitializeConfig();
	static void SettingsOverride();
	static bool ParseCommandLineArgs(int argc, char* argv[], VMBootParameters& params);
	static void DumpStats();

	static bool CreatePlatformWindow();
	static void DestroyPlatformWindow();
	static std::optional<WindowInfo> GetPlatformWindowInfo();
	static void PumpPlatformMessages(bool forever = false);
	static void StopPlatformMessagePump();
} // namespace GSRunner

static constexpr u32 WINDOW_WIDTH = 640;
static constexpr u32 WINDOW_HEIGHT = 480;

static MemorySettingsInterface s_settings_interface;

static std::string s_output_prefix;
static s32 s_loop_count = 1;
static std::optional<bool> s_use_window;
static bool s_no_console = false;

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

bool GSRunner::InitializeConfig()
{
	EmuFolders::SetAppRoot();
	if (!EmuFolders::SetResourcesDirectory() || !EmuFolders::SetDataDirectory(nullptr))
		return false;

	CrashHandler::SetWriteDirectory(EmuFolders::DataRoot);

	const char* error;
	if (!VMManager::PerformEarlyHardwareChecks(&error))
		return false;

	ImGuiManager::SetFontPath(Path::Combine(EmuFolders::Resources, "fonts" FS_OSPATH_SEPARATOR_STR "Roboto-Regular.ttf"));

	// don't provide an ini path, or bother loading. we'll store everything in memory.
	MemorySettingsInterface& si = s_settings_interface;
	Host::Internal::SetBaseSettingsLayer(&si);

	VMManager::SetDefaultSettings(si, true, true, true, true, true);

	VMManager::Internal::LoadStartupSettings();
	return true;
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

bool Host::LocaleCircleConfirm()
{
	// not running any UI, so no settings requests will come in
	return false;
}

std::unique_ptr<ProgressCallback> Host::CreateHostProgressCallback()
{
	return ProgressCallback::CreateNullProgressCallback();
}

void Host::ReportInfoAsync(const std::string_view title, const std::string_view message)
{
	if (!title.empty() && !message.empty())
		INFO_LOG("ReportInfoAsync: {}: {}", title, message);
	else if (!message.empty())
		INFO_LOG("ReportInfoAsync: {}", message);
}

void Host::ReportErrorAsync(const std::string_view title, const std::string_view message)
{
	if (!title.empty() && !message.empty())
		ERROR_LOG("ReportErrorAsync: {}: {}", title, message);
	else if (!message.empty())
		ERROR_LOG("ReportErrorAsync: {}", message);
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
	return GSRunner::GetPlatformWindowInfo();
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

std::optional<WindowInfo> Host::AcquireRenderWindow(bool recreate_window)
{
	return GSRunner::GetPlatformWindowInfo();
}

void Host::ReleaseRenderWindow()
{
}

void Host::BeginPresentFrame()
{
	if (s_loop_number == 0 && !s_output_prefix.empty())
	{
		// when we wrap around, don't race other files
		GSJoinSnapshotThreads();

		// queue dumping of this frame
		std::string dump_path(fmt::format("{}_frame{:05}.png", s_output_prefix, s_dump_frame_number));
		GSQueueSnapshot(dump_path);
	}

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

BEGIN_HOTKEY_LIST(g_host_hotkeys)
END_HOTKEY_LIST()

static void PrintCommandLineVersion()
{
	std::fprintf(stderr, "PCSX2 GS Runner Version %s\n", GIT_REV);
	std::fprintf(stderr, "https://pcsx2.net/\n");
	std::fprintf(stderr, "\n");
}

static void PrintCommandLineHelp(const char* progname)
{
	PrintCommandLineVersion();
	std::fprintf(stderr, "Usage: %s [parameters] [--] [filename]\n", progname);
	std::fprintf(stderr, "\n");
	std::fprintf(stderr, "  -help: Displays this information and exits.\n");
	std::fprintf(stderr, "  -version: Displays version information and exits.\n");
	std::fprintf(stderr, "  -dumpdir <dir>: Frame dump directory (will be dumped as filename_frameN.png).\n");
	std::fprintf(stderr, "  -dump [rt|tex|z|f|a|i|tr|ds|fs]: Enabling dumping of render target, texture, z buffer, frame, "
		"alphas, and info (context, vertices, list of transfers), transfers images, draw stats, frame stats, respectively, per draw. Generates lots of data.\n");
	std::fprintf(stderr, "  -dumprange N[,L,B]: Start dumping from draw N (base 0), stops after L draws, and only "
		"those draws that are multiples of B (intersection of -dumprange and -dumprangef used)."
		"Defaults to 0,-1,1 (all draws). Only used if -dump used.\n");
	std::fprintf(stderr, "  -dumprangef NF[,LF,BF]: Start dumping from frame NF (base 0), stops after LF frames, "
		"and only those frames that are multiples of BF (intersection of -dumprange and -dumprangef used).\n"
		"Defaults to 0,-1,1 (all frames). Only used if -dump is used.\n");
	std::fprintf(stderr, "  -loop <count>: Loops dump playback N times. Defaults to 1. 0 will loop infinitely.\n");
	std::fprintf(stderr, "  -renderer <renderer>: Sets the graphics renderer. Defaults to Auto.\n");
	std::fprintf(stderr, "  -swthreads <threads>: Sets the number of threads for the software renderer.\n");
	std::fprintf(stderr, "  -window: Forces a window to be displayed.\n");
	std::fprintf(stderr, "  -surfaceless: Disables showing a window.\n");
	std::fprintf(stderr, "  -logfile <filename>: Writes emu log to filename.\n");
	std::fprintf(stderr, "  -noshadercache: Disables the shader cache (useful for parallel runs).\n");
	std::fprintf(stderr, "  --: Signals that no more arguments will follow and the remaining\n"
						 "    parameters make up the filename. Use when the filename contains\n"
						 "    spaces or starts with a dash.\n");
	std::fprintf(stderr, "\n");
}

void GSRunner::InitializeConsole()
{
	const char* var = std::getenv("PCSX2_NOCONSOLE");
	s_no_console = (var && StringUtil::FromChars<bool>(var).value_or(false));
	if (!s_no_console)
		Log::SetConsoleOutputLevel(LOGLEVEL_DEBUG);
}

bool GSRunner::ParseCommandLineArgs(int argc, char* argv[], VMBootParameters& params)
{
	std::string dumpdir; // Save from argument -dumpdir for creating sub-directories
	bool no_more_args = false;
	for (int i = 1; i < argc; i++)
	{
		if (!no_more_args)
		{
#define CHECK_ARG(str) !std::strcmp(argv[i], str)
#define CHECK_ARG_PARAM(str) (!std::strcmp(argv[i], str) && ((i + 1) < argc))

			if (CHECK_ARG("-help"))
			{
				PrintCommandLineHelp(argv[0]);
				return false;
			}
			else if (CHECK_ARG("-version"))
			{
				PrintCommandLineVersion();
				return false;
			}
			else if (CHECK_ARG_PARAM("-dumpdir"))
			{
				dumpdir = s_output_prefix = StringUtil::StripWhitespace(argv[++i]);
				if (s_output_prefix.empty())
				{
					Console.Error("Invalid dump directory specified.");
					return false;
				}

				if (!FileSystem::DirectoryExists(s_output_prefix.c_str()) && !FileSystem::CreateDirectoryPath(s_output_prefix.c_str(), false))
				{
					Console.Error("Failed to create output directory");
					return false;
				}

				continue;
			}
			else if (CHECK_ARG_PARAM("-dump"))
			{
				std::string str(argv[++i]);

				s_settings_interface.SetBoolValue("EmuCore/GS", "DumpGSData", true);

				if (str.find("rt") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "SaveRT", true);
				if (str.find("f") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "SaveFrame", true);
				if (str.find("tex") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "SaveTexture", true);
				if (str.find("z") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "SaveDepth", true);
				if (str.find("a") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "SaveAlpha", true);
				if (str.find("i") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "SaveInfo", true);
				if (str.find("tr") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "SaveTransferImages", true);
				if (str.find("ds") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "SaveDrawStats", true);
				if (str.find("fs") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "SaveFrameStats", true);
				continue;
			}
			else if (CHECK_ARG_PARAM("-dumprange"))
			{
				std::string str(argv[++i]);

				std::vector<std::string_view> split = StringUtil::SplitString(str, ',');
				int start = 0;
				int num = -1;
				int by = 1;
				if (split.size() > 0)
				{
					start = StringUtil::FromChars<int>(split[0]).value_or(0);
				}
				if (split.size() > 1)
				{
					num = StringUtil::FromChars<int>(split[1]).value_or(-1);
				}
				if (split.size() > 2)
				{
					by = std::max(1, StringUtil::FromChars<int>(split[2]).value_or(1));
				}
				s_settings_interface.SetIntValue("EmuCore/GS", "SaveDrawStart", start);
				s_settings_interface.SetIntValue("EmuCore/GS", "SaveDrawCount", num);
				s_settings_interface.SetIntValue("EmuCore/GS", "SaveDrawBy", by);
				continue;
			}
			else if (CHECK_ARG_PARAM("-dumprangef"))
			{
				std::string str(argv[++i]);

				std::vector<std::string_view> split = StringUtil::SplitString(str, ',');
				int start = 0;
				int num = -1;
				int by = 1;
				if (split.size() > 0)
				{
					start = StringUtil::FromChars<int>(split[0]).value_or(0);
				}
				if (split.size() > 1)
				{
					num = StringUtil::FromChars<int>(split[1]).value_or(-1);
				}
				if (split.size() > 2)
				{
					by = std::max(1, StringUtil::FromChars<int>(split[2]).value_or(1));
				}
				s_settings_interface.SetIntValue("EmuCore/GS", "SaveFrameStart", start);
				s_settings_interface.SetIntValue("EmuCore/GS", "SaveFrameCount", num);
				s_settings_interface.SetIntValue("EmuCore/GS", "SaveFrameBy", by);
				continue;
			}
			else if (CHECK_ARG_PARAM("-dumpdirhw"))
			{
				s_settings_interface.SetStringValue("EmuCore/GS", "HWDumpDirectory", argv[++i]);
				continue;
			}
			else if (CHECK_ARG_PARAM("-dumpdirsw"))
			{
				s_settings_interface.SetStringValue("EmuCore/GS", "SWDumpDirectory", argv[++i]);
				continue;
			}
			else if (CHECK_ARG_PARAM("-loop"))
			{
				s_loop_count = StringUtil::FromChars<s32>(argv[++i]).value_or(0);
				Console.WriteLn("Looping dump playback %d times.", s_loop_count);
				continue;
			}
			else if (CHECK_ARG_PARAM("-renderer"))
			{
				const char* rname = argv[++i];

				GSRendererType type = GSRendererType::Auto;
				if (StringUtil::Strcasecmp(rname, "Auto") == 0)
					type = GSRendererType::Auto;
#ifdef _WIN32
				else if (StringUtil::Strcasecmp(rname, "dx11") == 0)
					type = GSRendererType::DX11;
				else if (StringUtil::Strcasecmp(rname, "dx12") == 0)
					type = GSRendererType::DX12;
#endif
#ifdef ENABLE_OPENGL
				else if (StringUtil::Strcasecmp(rname, "gl") == 0)
					type = GSRendererType::OGL;
#endif
#ifdef ENABLE_VULKAN
				else if (StringUtil::Strcasecmp(rname, "vulkan") == 0)
					type = GSRendererType::VK;
#endif
#ifdef __APPLE__
				else if (StringUtil::Strcasecmp(rname, "metal") == 0)
					type = GSRendererType::Metal;
#endif
				else if (StringUtil::Strcasecmp(rname, "sw") == 0)
					type = GSRendererType::SW;
				else
				{
					Console.Error("Unknown renderer '%s'", rname);
					return false;
				}

				Console.WriteLn("Using %s renderer.", Pcsx2Config::GSOptions::GetRendererName(type));
				s_settings_interface.SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(type));
				continue;
			}
			else if (CHECK_ARG_PARAM("-swthreads"))
			{
				const int swthreads = StringUtil::FromChars<int>(argv[++i]).value_or(0);
				if (swthreads < 0)
				{
					Console.WriteLn("Invalid number of software threads");
					return false;
				}
				
				Console.WriteLn(fmt::format("Setting number of software threads to {}", swthreads));
				s_settings_interface.SetIntValue("EmuCore/GS", "SWExtraThreads", swthreads);
				continue;
			}
			else if (CHECK_ARG_PARAM("-renderhacks"))
			{
				std::string str(argv[++i]);

				s_settings_interface.SetBoolValue("EmuCore/GS", "UserHacks", true);

				if (str.find("af") != std::string::npos)
					s_settings_interface.SetIntValue("EmuCore/GS", "UserHacks_AutoFlushLevel", 1);
				if (str.find("cpufb") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "UserHacks_CPU_FB_Conversion", true);
				if (str.find("dds") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "UserHacks_DisableDepthSupport", true);
				if (str.find("dpi") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "UserHacks_DisablePartialInvalidation", true);
				if (str.find("dsf") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "UserHacks_Disable_Safe_Features", true);
				if (str.find("tinrt") != std::string::npos)
					s_settings_interface.SetIntValue("EmuCore/GS", "UserHacks_TextureInsideRt", 1);
				if (str.find("plf") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "preload_frame_with_gs_data", true);

				continue;
			}
			else if (CHECK_ARG_PARAM("-ini"))
			{
				std::string path = std::string(StringUtil::StripWhitespace(argv[++i]));
				if (!FileSystem::FileExists(path.c_str()))
				{
					Console.ErrorFmt("INI file {} does not exit.", path);
					return false;
				}

				INISettingsInterface si_ini(path);

				if (!si_ini.Load())
				{
					Console.ErrorFmt("Unable to load INI settings from {}.", path);
					return false;
				}

				for (const auto& [key, value] : si_ini.GetKeyValueList("EmuCore/GS"))
					s_settings_interface.SetStringValue("EmuCore/GS", key.c_str(), value.c_str());

				continue;
			}
			else if (CHECK_ARG_PARAM("-upscale"))
			{
				const float upscale = StringUtil::FromChars<float>(argv[++i]).value_or(0.0f);
				if (upscale < 0.5f)
				{
					Console.WriteLn("Invalid upscale multiplier");
					return false;
				}

				Console.WriteLn(fmt::format("Setting upscale multiplier to {}", upscale));
				s_settings_interface.SetFloatValue("EmuCore/GS", "upscale_multiplier", upscale);
				continue;
			}
			else if (CHECK_ARG_PARAM("-logfile"))
			{
				const char* logfile = argv[++i];
				if (std::strlen(logfile) > 0)
				{
					// disable timestamps, since we want to be able to diff the logs
					Console.WriteLn("Logging to %s...", logfile);
					VMManager::Internal::SetFileLogPath(logfile);
					s_settings_interface.SetBoolValue("Logging", "EnableFileLogging", true);
					s_settings_interface.SetBoolValue("Logging", "EnableTimestamps", false);
				}

				continue;
			}
			else if (CHECK_ARG("-noshadercache"))
			{
				Console.WriteLn("Disabling shader cache");
				s_settings_interface.SetBoolValue("EmuCore/GS", "DisableShaderCache", true);
				continue;
			}
			else if (CHECK_ARG("-window"))
			{
				Console.WriteLn("Creating window");
				s_use_window = true;
				continue;
			}
			else if (CHECK_ARG("-surfaceless"))
			{
				Console.WriteLn("Running surfaceless");
				s_use_window = false;
				continue;
			}
			else if (CHECK_ARG("--"))
			{
				no_more_args = true;
				continue;
			}
			else if (argv[i][0] == '-')
			{
				Console.Error("Unknown parameter: '%s'", argv[i]);
				return false;
			}

#undef CHECK_ARG
#undef CHECK_ARG_PARAM
		}

		if (!params.filename.empty())
			params.filename += ' ';
		params.filename += argv[i];
	}

	if (params.filename.empty())
	{
		Console.Error("No dump filename provided.");
		return false;
	}

	if (!VMManager::IsGSDumpFileName(params.filename))
	{
		Console.Error("Provided filename is not a GS dump.");
		return false;
	}

	if (s_settings_interface.GetBoolValue("EmuCore/GS", "DumpGSData") && !dumpdir.empty())
	{
		if (s_settings_interface.GetStringValue("EmuCore/GS", "HWDumpDirectory").empty())
			s_settings_interface.SetStringValue("EmuCore/GS", "HWDumpDirectory", dumpdir.c_str());
		if (s_settings_interface.GetStringValue("EmuCore/GS", "SWDumpDirectory").empty())
			s_settings_interface.SetStringValue("EmuCore/GS", "SWDumpDirectory", dumpdir.c_str());
		
		// Disable saving frames with SaveSnapshotToMemory()
		// Instead we save more "raw" snapshots when using -dump.
		s_output_prefix = "";
	}

	// set up the frame dump directory
	if (!s_output_prefix.empty())
	{
		// strip off all extensions
		std::string_view title(Path::GetFileTitle(params.filename));
		if (StringUtil::EndsWithNoCase(title, ".gs"))
			title = Path::GetFileTitle(title);

		s_output_prefix = Path::Combine(s_output_prefix, StringUtil::StripWhitespace(title));
		Console.WriteLn(fmt::format("Saving dumps as {}_frameN.png", s_output_prefix));
	}

	return true;
}

void GSRunner::SettingsOverride()
{
	// complete as quickly as possible
	s_settings_interface.SetBoolValue("EmuCore/GS", "FrameLimitEnable", false);
	s_settings_interface.SetIntValue("EmuCore/GS", "VsyncEnable", false);

	// Force screenshot quality settings to something more performant, overriding any defaults good for users.
	s_settings_interface.SetIntValue("EmuCore/GS", "ScreenshotFormat", static_cast<int>(GSScreenshotFormat::PNG));
	s_settings_interface.SetIntValue("EmuCore/GS", "ScreenshotQuality", 10);

	// ensure all input sources are disabled, we're not using them
	s_settings_interface.SetBoolValue("InputSources", "SDL", false);
	s_settings_interface.SetBoolValue("InputSources", "XInput", false);

	// we don't need any sound output
	s_settings_interface.SetStringValue("SPU2/Output", "OutputModule", "nullout");

	// none of the bindings are going to resolve to anything
	Pad::ClearPortBindings(s_settings_interface, 0);
	s_settings_interface.ClearSection("Hotkeys");

	// force logging
	s_settings_interface.SetBoolValue("Logging", "EnableSystemConsole", !s_no_console);
	s_settings_interface.SetBoolValue("Logging", "EnableTimestamps", true);
	s_settings_interface.SetBoolValue("Logging", "EnableVerbose", true);

	// and show some stats :)
	s_settings_interface.SetBoolValue("EmuCore/GS", "OsdShowFPS", true);
	s_settings_interface.SetBoolValue("EmuCore/GS", "OsdShowResolution", true);
	s_settings_interface.SetBoolValue("EmuCore/GS", "OsdShowGSStats", true);

	// remove memory cards, so we don't have sharing violations
	for (u32 i = 0; i < 2; i++)
	{
		s_settings_interface.SetBoolValue("MemoryCards", fmt::format("Slot{}_Enable", i + 1).c_str(), false);
		s_settings_interface.SetStringValue("MemoryCards", fmt::format("Slot{}_Filename", i + 1).c_str(), "");
	}
}

void GSRunner::DumpStats()
{
	std::atomic_thread_fence(std::memory_order_acquire);
	Console.WriteLn(fmt::format("======= HW STATISTICS FOR {} ({}) FRAMES ========", s_total_frames, s_total_drawn_frames));
	Console.WriteLn(fmt::format("@HWSTAT@ Draw Calls: {} (avg {})", s_total_draws, static_cast<u64>(std::ceil(s_total_draws / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Render Passes: {} (avg {})", s_total_render_passes, static_cast<u64>(std::ceil(s_total_render_passes / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Barriers: {} (avg {})", s_total_barriers, static_cast<u64>(std::ceil(s_total_barriers / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Copies: {} (avg {})", s_total_copies, static_cast<u64>(std::ceil(s_total_copies / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Uploads: {} (avg {})", s_total_uploads, static_cast<u64>(std::ceil(s_total_uploads / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Readbacks: {} (avg {})", s_total_readbacks, static_cast<u64>(std::ceil(s_total_readbacks / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn("============================================");
}

#ifdef _WIN32
// We can't handle unicode in filenames if we don't use wmain on Win32.
#define main real_main
#endif

static void CPUThreadMain(VMBootParameters* params, std::atomic<int>* ret)
{
	ret->store(EXIT_FAILURE);

	if (VMManager::Internal::CPUThreadInitialize())
	{
		// apply new settings (e.g. pick up renderer change)
		VMManager::ApplySettings();
		GSDumpReplayer::SetIsDumpRunner(true);

		if (VMManager::Initialize(*params) == VMBootResult::StartupSuccess)
		{
			// run until end
			GSDumpReplayer::SetLoopCount(s_loop_count);
			VMManager::SetState(VMState::Running);
			while (VMManager::GetState() == VMState::Running)
				VMManager::Execute();
			VMManager::Shutdown(false);
			GSRunner::DumpStats();
			ret->store(EXIT_SUCCESS);
		}
	}

	VMManager::Internal::CPUThreadShutdown();
	GSRunner::StopPlatformMessagePump();
}

int main(int argc, char* argv[])
{
	CrashHandler::Install();
	GSRunner::InitializeConsole();

	if (!GSRunner::InitializeConfig())
	{
		Console.Error("Failed to initialize config.");
		return EXIT_FAILURE;
	}

	VMBootParameters params;
	if (!GSRunner::ParseCommandLineArgs(argc, argv, params))
		return EXIT_FAILURE;

	if (s_use_window.value_or(true) && !GSRunner::CreatePlatformWindow())
	{
		Console.Error("Failed to create window.");
		return EXIT_FAILURE;
	}

	// Override settings that shouldn't be picked up from defaults or INIs.
	GSRunner::SettingsOverride();

	std::atomic<int> thread_ret;
	std::thread cputhread(CPUThreadMain, &params, &thread_ret);
	GSRunner::PumpPlatformMessages(/*forever=*/true);
	cputhread.join();

	GSRunner::DestroyPlatformWindow();

	return thread_ret.load();
}

void Host::PumpMessagesOnCPUThread()
{
	// update GS thread copy of frame number
	MTGS::RunOnGSThread([frame_number = GSDumpReplayer::GetFrameNumber()]() { s_dump_frame_number = frame_number; });
	MTGS::RunOnGSThread([loop_number = GSDumpReplayer::GetLoopCount()]() { s_loop_number = loop_number; });
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

//////////////////////////////////////////////////////////////////////////
// Platform specific code
//////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static constexpr LPCWSTR WINDOW_CLASS_NAME = L"PCSX2GSRunner";
static HWND s_hwnd = NULL;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool GSRunner::CreatePlatformWindow()
{
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = WINDOW_CLASS_NAME;
	wc.hIconSm = NULL;

	if (!RegisterClassExW(&wc))
	{
		Console.Error("Window registration failed.");
		return false;
	}

	s_hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, WINDOW_CLASS_NAME, L"PCSX2 GS Runner",
		WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_SIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH,
		WINDOW_HEIGHT, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	if (!s_hwnd)
	{
		Console.Error("CreateWindowEx failed.");
		return false;
	}

	ShowWindow(s_hwnd, SW_SHOW);
	UpdateWindow(s_hwnd);

	// make sure all messages are processed before returning
	PumpPlatformMessages();
	return true;
}

void GSRunner::DestroyPlatformWindow()
{
	if (!s_hwnd)
		return;

	PumpPlatformMessages();
	DestroyWindow(s_hwnd);
	s_hwnd = {};
}

std::optional<WindowInfo> GSRunner::GetPlatformWindowInfo()
{
	WindowInfo wi;

	if (s_hwnd)
	{
		RECT rc = {};
		GetWindowRect(s_hwnd, &rc);
		wi.surface_width = static_cast<u32>(rc.right - rc.left);
		wi.surface_height = static_cast<u32>(rc.bottom - rc.top);
		wi.surface_scale = 1.0f;
		wi.type = WindowInfo::Type::Win32;
		wi.window_handle = s_hwnd;
	}
	else
	{
		wi.type = WindowInfo::Type::Surfaceless;
	}

	return wi;
}

static constexpr int SHUTDOWN_MSG = WM_APP + 0x100;
static DWORD MainThreadID;

void GSRunner::PumpPlatformMessages(bool forever)
{
	MSG msg;
	while (true)
	{
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == SHUTDOWN_MSG)
				return;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		if (!forever)
			return;
		WaitMessage();
	}
}

void GSRunner::StopPlatformMessagePump()
{
	PostThreadMessageW(MainThreadID, SHUTDOWN_MSG, 0, 0);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int wmain(int argc, wchar_t** argv)
{
	std::vector<std::string> u8_args;
	u8_args.reserve(static_cast<size_t>(argc));
	for (int i = 0; i < argc; i++)
		u8_args.push_back(StringUtil::WideStringToUTF8String(argv[i]));

	std::vector<char*> u8_argptrs;
	u8_argptrs.reserve(u8_args.size());
	for (int i = 0; i < argc; i++)
		u8_argptrs.push_back(u8_args[i].data());
	u8_argptrs.push_back(nullptr);

	MainThreadID = GetCurrentThreadId();

	return real_main(argc, u8_argptrs.data());
}

#elif defined(__APPLE__)

static void* s_window;
static WindowInfo s_wi;

bool GSRunner::CreatePlatformWindow()
{
	pxAssertRel(!s_window, "Tried to create window when there already was one!");
	s_window = CocoaTools::CreateWindow("PCSX2 GS Runner", WINDOW_WIDTH, WINDOW_HEIGHT);
	CocoaTools::GetWindowInfoFromWindow(&s_wi, s_window);
	PumpPlatformMessages();
	return s_window;
}

void GSRunner::DestroyPlatformWindow()
{
	if (s_window) {
		CocoaTools::DestroyWindow(s_window);
		s_window = nullptr;
	}
}

std::optional<WindowInfo> GSRunner::GetPlatformWindowInfo()
{
	WindowInfo wi;
	if (s_window)
		wi = s_wi;
	else
		wi.type = WindowInfo::Type::Surfaceless;
	return wi;
}

void GSRunner::PumpPlatformMessages(bool forever)
{
	CocoaTools::RunCocoaEventLoop(forever);
}

void GSRunner::StopPlatformMessagePump()
{
	CocoaTools::StopMainThreadEventLoop();
}

#elif defined(__linux__)
static Display* s_display = nullptr;
static Window s_window = None;
static WindowInfo s_wi;
static std::atomic<bool> s_shutdown_requested{false};

bool GSRunner::CreatePlatformWindow()
{
	pxAssertRel(!s_display && s_window == None, "Tried to create window when there already was one!");

	s_display = XOpenDisplay(nullptr);
	if (!s_display)
	{
		Console.Error("Failed to open X11 display");
		return false;
	}

	int screen = DefaultScreen(s_display);
	Window root = RootWindow(s_display, screen);

	s_window = XCreateSimpleWindow(s_display, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 1,
		BlackPixel(s_display, screen), WhitePixel(s_display, screen));

	if (s_window == None)
	{
		Console.Error("Failed to create X11 window");
		XCloseDisplay(s_display);
		s_display = nullptr;
		return false;
	}

	XStoreName(s_display, s_window, "PCSX2 GS Runner");
	XSelectInput(s_display, s_window, StructureNotifyMask);
	XMapWindow(s_display, s_window);

	s_wi.type = WindowInfo::Type::X11;
	s_wi.display_connection = s_display;
	s_wi.window_handle = reinterpret_cast<void*>(s_window);
	s_wi.surface_width = WINDOW_WIDTH;
	s_wi.surface_height = WINDOW_HEIGHT;
	s_wi.surface_scale = 1.0f;

	XFlush(s_display);
	PumpPlatformMessages();
	return true;
}

void GSRunner::DestroyPlatformWindow()
{
	if (s_display && s_window != None)
	{
		XDestroyWindow(s_display, s_window);
		s_window = None;
	}

	if (s_display)
	{
		XCloseDisplay(s_display);
		s_display = nullptr;
	}
}

std::optional<WindowInfo> GSRunner::GetPlatformWindowInfo()
{
	WindowInfo wi;
	if (s_display && s_window != None)
		wi = s_wi;
	else
		wi.type = WindowInfo::Type::Surfaceless;
	return wi;
}

void GSRunner::PumpPlatformMessages(bool forever)
{
	if (!s_display)
		return;

	do
	{
		while (XPending(s_display) > 0)
		{
			XEvent event;
			XNextEvent(s_display, &event);

			switch (event.type)
			{
				case ConfigureNotify:
				{
					const XConfigureEvent& configure = event.xconfigure;
					s_wi.surface_width = static_cast<u32>(configure.width);
					s_wi.surface_height = static_cast<u32>(configure.height);
					break;
				}
				case DestroyNotify:
					return;
				default:
					break;
			}
		}

		if (s_shutdown_requested.load())
			return;

		if (forever)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	} while (forever && !s_shutdown_requested.load());
}

void GSRunner::StopPlatformMessagePump()
{
	s_shutdown_requested.store(true);
}
#endif // _WIN32 / __APPLE__
