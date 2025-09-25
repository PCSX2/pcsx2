// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

#include "fmt/format.h"

#include "common/Error.h"
#include "common/Timer.h"
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
#include "common/ScopedGuard.h"

#include "pcsx2/PrecompiledHeader.h"

#include "pcsx2/Achievements.h"
#include "pcsx2/CDVD/CDVD.h"
#include "pcsx2/GS.h"
#include "pcsx2/GS/GSLzma.h"
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

#include "pcsx2/GSRegressionTester.h"
#include "pcsx2/GS/GSPng.h"
#include "pcsx2/GS/GSLzma.h"

#include "svnrev.h"

// Down here because X11 has a lot of defines that can conflict
#if defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/select.h>
#include <unistd.h>
#endif
// For writing YAML files.
static constexpr const char* INDENT = "    ";
static constexpr const char* OPEN_MAP = "{";
static constexpr const char* CLOSE_MAP = "}";
static constexpr const char* QUOTE = "\"";
static constexpr const char* KEY_VAL_DEL = ": ";
static constexpr const char* LIST_DEL = ", ";
static constexpr const char* LIST_ITEM = "- ";
static constexpr const char* OPEN_LIST = "[";
static constexpr const char* CLOSE_LIST = "]";

struct GSTester
{
	struct DumpInfo
	{
		enum State
		{
			UNVISITED,
			COMPLETED,
			SKIPPED
		};

		std::string file;
		std::string name;
		std::size_t packets_skipped = 0;
		std::size_t packets_completed = 0;
		State state = UNVISITED;
		double load_time = 0.0;
		double block_time = 0.0;

		DumpInfo(const std::string& file, const std::string& name)
			: file(file)
			, name(name)
		{
		}
	};

	enum ReturnValue
	{
		SUCCESS,
		BUFFER_NOT_READY,
		ERROR_
	};

	static void PrintCommandLineHelp(const char* progname);
	bool ParseCommandLineArgs(int argc, char* argv[]);
	bool GetDumpInfo();
	ReturnValue CopyDumpToSharedMemory(const std::vector<u8>& data, const std::string& name);
	ReturnValue ProcessPackets();
	bool StartRunners();
	bool EndRunners();
	bool RestartRunners();
	std::string GetTesterName();
	int MainThread(int argc, char* argv[], u32 nthreads, u32 thread_id);
	
	static std::string GetEventNameRunner(GSProcess::PID_t tester_pid, const std::string& runner_name);
	static std::string GetEventNameTester(GSProcess::PID_t tester_pid);
	
	static int main_tester(int argc, char* argv[]);

	enum
	{
		VERBOSE_LOW,
		VERBOSE_TESTER,
		VERBOSE_TESTER_AND_RUNNER
	};

	static constexpr std::size_t regression_failure_restarts_max = 10;
	static constexpr std::size_t regression_packet_size_default = 5 * _1mb;
	static constexpr std::size_t regression_dump_size_default = 256 * _1mb;
	static constexpr std::size_t regression_num_packets_default = 100;
	static constexpr std::size_t regression_num_dumps_default = 1;
	static constexpr u32 regression_verbose_level_default = VERBOSE_LOW;
	static constexpr double regression_deadlock_timeout_default = 60.0; // seconds

	std::vector<std::string> regression_dump_files;
	std::vector<DumpInfo> regression_dumps;
	std::map<std::string, std::size_t> regression_dumps_map;
	std::string regression_dump_last_completed;
	std::string regression_output_dir;
	std::string regression_output_dir_runner[2];
	std::string regression_runner_args;
	GSRegressionBuffer regression_buffer[2];
	GSEvent regression_event_tester;
	std::string regression_runner_path[2];
	std::string regression_runner_command[2];
	std::string regression_runner_name[2];
	std::string regression_shared_file[2];
	std::string regression_dump_dir;
	GSProcess regression_runner_proc[2];
	std::size_t regression_num_packets = regression_num_packets_default;
	std::size_t regression_packet_size = regression_packet_size_default;
	std::size_t regression_num_dumps = regression_num_dumps_default;
	std::size_t regression_dump_size = regression_dump_size_default;
	u32 regression_verbose_level = regression_verbose_level_default;
	std::string regression_start_from_dump;
	bool regression_logging = false;
	double regression_deadlock_timeout = regression_deadlock_timeout_default;

	// Stats
	std::size_t regression_packets_completed = 0;
	std::size_t regression_packets_skipped = 0;
	std::size_t regression_dumps_completed = 0;
	std::size_t regression_dumps_skipped = 0;
	std::size_t regression_dumps_unvisited = 0;
	std::size_t regression_failure_restarts = 0;
	std::size_t regression_packets_skipped_unknown = 0;
	double regression_dump_load_time = 0.0;

	// Threading
	u32 regression_nthreads = 1;
	u32 regression_thread_id = 0;
};

namespace GSRunner
{
	static void InitializeConsole();
	static bool InitializeConfig();
	static void SettingsOverride();
	static bool ParseCommandLineArgs(int argc, char* argv[], VMBootParameters& params);
	static void PrintCommandLineHelp(const char* progname);
	static void DumpStats();

	static bool CreatePlatformWindow();
	static void DestroyPlatformWindow();
	static std::optional<WindowInfo> GetPlatformWindowInfo();
	static void PumpPlatformMessages(bool forever = false);
	static void StopPlatformMessagePump();
	static std::string GetDumpTitle(const std::string& path);
	static std::string GetBatchDumpTitle();

	int main_runner(int argc, char* argv[]);

	static constexpr u32 WINDOW_WIDTH = 640;
	static constexpr u32 WINDOW_HEIGHT = 480;

	static MemorySettingsInterface s_settings_interface;

	static std::string s_output_prefix;
	static std::string s_logfile;
	static std::string s_runner_name;
	static std::string s_regression_file;
	GSRegressionBuffer s_regression_buffer;
	static size_t s_regression_num_packets = GSTester::regression_num_packets_default;
	static size_t s_regression_packet_size = GSTester::regression_packet_size_default;
	static size_t s_regression_num_dumps = GSTester::regression_num_dumps_default;
	static size_t s_regression_dump_size = GSTester::regression_dump_size_default;
	static s32 s_loop_count = 1;
	static std::optional<bool> s_use_window;
	static bool s_no_console = false;
	static bool s_batch_mode = false;
	static bool s_batch_recreate_device = true;
	static u32 s_num_batches = 1;
	static u32 s_batch_id = 0;
	static u32 s_frames_max = 0xFFFFFFFF;
	static u32 s_parent_pid = 0;
	static std::string s_batch_start_from_dump;
	static bool s_verbose_logging = 0;

	// Owned by the GS thread.
	static u32 s_dump_frame_number = 0;
	static u32 s_loop_number = -1; // Invalid until initialized on first vsync.
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
	static std::string s_dump_gs_data_dir_hw;
	static std::string s_dump_gs_data_dir_sw;
	static std::string s_batch_dump_name;
	static Common::Timer s_batch_dump_timer;
} // namespace GSRunner

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
	if (GSRunner::s_loop_number == 0 && (!GSRunner::s_output_prefix.empty() || (GSIsRegressionTesting() && !GSConfig.DumpGSData)))
	{
		// when we wrap around, don't race other files
		GSJoinSnapshotThreads();

		std::string dump_file;
		bool queue_snapshot = true;

		if (GSRunner::s_batch_mode)
		{
			std::string title(GSRunner::GetBatchDumpTitle());
			std::string dir(Path::Combine(GSRunner::s_output_prefix, title));
			dump_file = fmt::format("{}_frame{:05}.png", title, GSRunner::s_dump_frame_number);
			
			// Only need directory when not regression testing.
			if (!GSIsRegressionTesting())
			{
				Error error;
				if (FileSystem::EnsureDirectoryExists(dir.c_str(), &error))
				{
					dump_file = Path::Combine(dir, fmt::format("{}_frame{:05}.png", title, GSRunner::s_dump_frame_number));
				}
				else
				{
					Console.ErrorFmt("(GSRunner/{})", "Unable to create output directory '{}' (error: {})",
						GSDumpReplayer::GetRunnerName(), dir, error.GetDescription());
					queue_snapshot = false;
				}
			}
		}
		else
		{
			dump_file = fmt::format("{}_frame{:05}.png", GSRunner::s_output_prefix, GSRunner::s_dump_frame_number);
		}

		// queue dumping of this frame
		if (queue_snapshot)
			GSQueueSnapshot(dump_file);
	}

	if (GSIsHardwareRenderer())
	{
		const u32 last_draws = GSRunner::s_total_internal_draws;
		const u32 last_uploads = GSRunner::s_total_uploads;

		static constexpr auto update_stat = [](GSPerfMon::counter_t counter, u64& dst, double& last) {
			// perfmon resets every 30 frames to zero
			const double val = g_perfmon.GetCounter(counter);
			dst += static_cast<u64>((val < last) ? val : (val - last));
			last = val;
		};

		update_stat(GSPerfMon::Draw, GSRunner::s_total_internal_draws, GSRunner::s_last_internal_draws);
		update_stat(GSPerfMon::DrawCalls, GSRunner::s_total_draws, GSRunner::s_last_draws);
		update_stat(GSPerfMon::RenderPasses, GSRunner::s_total_render_passes, GSRunner::s_last_render_passes);
		update_stat(GSPerfMon::Barriers, GSRunner::s_total_barriers, GSRunner::s_last_barriers);
		update_stat(GSPerfMon::TextureCopies, GSRunner::s_total_copies, GSRunner::s_last_copies);
		update_stat(GSPerfMon::TextureUploads, GSRunner::s_total_uploads, GSRunner::s_last_uploads);
		update_stat(GSPerfMon::Readbacks, GSRunner::s_total_readbacks, GSRunner::s_last_readbacks);

		const bool idle_frame = GSRunner::s_total_frames && (last_draws == GSRunner::s_total_internal_draws && last_uploads == GSRunner::s_total_uploads);

		if (!idle_frame)
			GSRunner::s_total_drawn_frames++;

		GSRunner::s_total_frames++;
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

void Host::OnBatchDumpStart(const std::string& dump_name)
{
	if (GSRunner::s_batch_mode)
	{
		MTGS::RunOnGSThread([dump_name]() {
			GSRunner::s_loop_number = -1; // Invalid until initialized on first vsync.

			// Stats
			GSRunner::s_batch_dump_timer.Reset();
			GSRunner::s_last_internal_draws = 0;
			GSRunner::s_last_draws = 0;
			GSRunner::s_last_render_passes = 0;
			GSRunner::s_last_barriers = 0;
			GSRunner::s_last_copies = 0;
			GSRunner::s_last_uploads = 0;
			GSRunner::s_last_readbacks = 0;
			GSRunner::s_total_internal_draws = 0;
			GSRunner::s_total_draws = 0;
			GSRunner::s_total_render_passes = 0;
			GSRunner::s_total_barriers = 0;
			GSRunner::s_total_copies = 0;
			GSRunner::s_total_uploads = 0;
			GSRunner::s_total_readbacks = 0;
			GSRunner::s_total_frames = 0;
			GSRunner::s_total_drawn_frames = 0;
			g_perfmon.Reset(); // Not really needed - will be reset when GS is reset, but just in case.

			GSRunner::s_batch_dump_name = dump_name; // Set before getting title.

			std::string dump_title(GSRunner::GetBatchDumpTitle());

			if (!GSRunner::s_logfile.empty())
			{
				std::string log_dir = Path::Combine(GSRunner::s_logfile, dump_title);

				Error error;
				if (!FileSystem::EnsureDirectoryExists(log_dir.c_str(), false, &error))
				{
					Console.ErrorFmt("(GSRunner/{}) Could not create log directory: '{}'",
						GSDumpReplayer::GetRunnerName(), log_dir);
				}
				else
				{
					std::string log_file = Path::Combine(log_dir, "emulog.txt");
					VMManager::Internal::SetFileLogPath(log_file);
					Console.WriteLnFmt("(GSRunner/{}) Logging to '{}'", GSDumpReplayer::GetRunnerName(), log_file);
				}
			}

			if (GSConfig.DumpGSData && !GSIsRegressionTesting())
			{
				// In case we are saving GS data in batch mode, make sure to update the directories.
				std::string* src_dir[] = {&GSRunner::s_dump_gs_data_dir_hw, &GSRunner::s_dump_gs_data_dir_sw};
				std::string* dst_dir[] = {&GSConfig.HWDumpDirectory, &GSConfig.SWDumpDirectory};

				Error error;
				for (int i = 0; i < 2; i++)
				{
					*dst_dir[i] = Path::Combine(*src_dir[i], dump_title);
					if (!FileSystem::EnsureDirectoryExists(dst_dir[i]->c_str(), false, &error))
					{
						Console.ErrorFmt("(GSRunner/{}) Could not create output directory: {} ({})",
							GSDumpReplayer::GetRunnerName(), *dst_dir[i], error.GetDescription());
					}
					else
					{
						Console.WriteLnFmt("(GSRunner/{}) Dumping GS data to '{}'", GSDumpReplayer::GetRunnerName(), *dst_dir[i]);
					}
				}
			}
		});
	}
}

void Host::OnBatchDumpEnd(const std::string& dump_name)
{
	if (GSRunner::s_batch_mode)
	{
		GSRunner::DumpStats();
	}
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

static void GSRunner::PrintCommandLineHelp(const char* progname)
{
	PrintCommandLineVersion();
	std::fprintf(stderr, "Usage: %s [parameters] [--] [filename]\n", progname);
	std::fprintf(stderr, "\n");
	std::fprintf(stderr, "  -help: Displays this information and exits.\n");
	std::fprintf(stderr, "  -version: Displays version information and exits.\n");
	std::fprintf(stderr, "  -dumpdir <dir>: Frame dump directory (will be dumped as filename_frameN.png).\n");
	std::fprintf(stderr, "  -dump [rt|tex|z|f|a|i|tr]: Enabling dumping of render target, texture, z buffer, frame, "
		"alphas, and info (context, vertices, transfers (list)), transfers (images), respectively, per draw. Generates lots of data.\n");
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

void GSTester::PrintCommandLineHelp(const char* progname)
{
	PrintCommandLineVersion();
	std::fprintf(stderr, "Usage: %s [parameters] [--] [filename]\n", progname);
	std::fprintf(stderr, "\n");
	std::fprintf(stderr, "  -help: Displays this information and exits.\n");
	std::fprintf(stderr, "\n");
}

void GSRunner::InitializeConsole()
{
	const char* var = std::getenv("PCSX2_NOCONSOLE");
	s_no_console = (var && StringUtil::FromChars<bool>(var).value_or(false));
	if (!s_no_console)
		Log::SetConsoleOutputLevel(LOGLEVEL_DEBUG);
}

static std::string GSRunner::GetDumpTitle(const std::string& path)
{
	std::string title(Path::GetFileTitle(path));
	if (StringUtil::EndsWithNoCase(title, ".gs"))
		title = Path::GetFileTitle(title);
	return title;
}

static std::string GSRunner::GetBatchDumpTitle()
{
	return GetDumpTitle(s_batch_dump_name);
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

			if (i == 1 && CHECK_ARG("runner"))
			{
				continue;
			}
			else if (CHECK_ARG("-help"))
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
				s_dump_gs_data_dir_hw = argv[++i];
				s_settings_interface.SetStringValue("EmuCore/GS", "HWDumpDirectory", argv[i]);
				continue;
			}
			else if (CHECK_ARG_PARAM("-dumpdirsw"))
			{
				s_dump_gs_data_dir_sw = argv[++i];
				s_settings_interface.SetStringValue("EmuCore/GS", "SWDumpDirectory", argv[i]);
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
				
				Console.WriteLnFmt("Setting number of software threads to {}", swthreads);
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

				Console.WriteLnFmt("Setting upscale multiplier to {}", upscale);
				s_settings_interface.SetFloatValue("EmuCore/GS", "upscale_multiplier", upscale);
				continue;
			}
			else if (CHECK_ARG_PARAM("-logfile"))
			{
				s_logfile = StringUtil::StripWhitespace(argv[++i]);
				continue;
			}
			else if (CHECK_ARG_PARAM("-regression-test"))
			{
				s_regression_file = std::string(argv[++i]);
				s_batch_mode = true;
				continue;
			}
			else if (CHECK_ARG_PARAM("-name"))
			{
				s_runner_name = argv[++i];
				continue;
			}
			else if (CHECK_ARG_PARAM("-regression-npackets-buffer"))
			{
				s_regression_num_packets = StringUtil::FromChars<u32>(argv[++i]).value_or(GSTester::regression_num_packets_default);
				continue;
			}
			else if (CHECK_ARG_PARAM("-regression-packet-size"))
			{
				std::optional<u32> size_mb = StringUtil::FromChars<u32>(argv[++i]);
				if (size_mb.has_value())
					s_regression_packet_size = size_mb.value() * _1mb;
				continue;
			}
			else if (CHECK_ARG_PARAM("-batch-ndumps-buffer"))
			{
				s_regression_num_dumps = StringUtil::FromChars<u32>(argv[++i]).value_or(GSTester::regression_num_dumps_default);
				continue;
			}
			else if (CHECK_ARG_PARAM("-regression-dump-size"))
			{
				std::optional<u32> size_mb = StringUtil::FromChars<u32>(argv[++i]);
				if (size_mb.has_value())
					s_regression_dump_size = size_mb.value() * _1mb;
				continue;
			}
			else if (CHECK_ARG_PARAM("-frames-max"))
			{
				s_frames_max = StringUtil::FromChars<u32>(argv[++i]).value_or(0xFFFFFFFF);
				continue;
			}
			else if (CHECK_ARG_PARAM("-regression-ppid"))
			{
				s_parent_pid = StringUtil::FromChars<u32>(argv[++i]).value_or(0);
				continue;
			}
			else if (CHECK_ARG_PARAM("-nbatches"))
			{
				s_num_batches = StringUtil::FromChars<u32>(argv[++i]).value_or(1);
				continue;
			}
			else if (CHECK_ARG_PARAM("-batch-id"))
			{
				s_batch_id = StringUtil::FromChars<u32>(argv[++i]).value_or(0);
				continue;
			}
			else if (CHECK_ARG("-batch"))
			{
				s_batch_mode = true;
				continue;
			}
			else if (CHECK_ARG("-batch-gs-fast-reopen"))
			{
				s_batch_recreate_device = false;
				continue;
			}
			else if (CHECK_ARG("-batch-start-from"))
			{
				s_batch_start_from_dump = StringUtil::StripWhitespace(argv[++i]);
				continue;
			}
			else if (CHECK_ARG("-noshadercache"))
			{
				Console.WriteLn("Disabling shader cache");
				s_settings_interface.SetBoolValue("EmuCore/GS", "disable_shader_cache", true);
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
			else if (CHECK_ARG("-verbose"))
			{
				s_verbose_logging = true;
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

	if (s_runner_name.empty())
	{
		s_runner_name = Path::GetFileName(argv[0]);
	}

	if (!s_logfile.empty())
	{
		if (s_batch_mode)
		{
			Error error;
			if (!FileSystem::EnsureDirectoryExists(s_logfile.c_str(), true, &error))
			{
				Console.ErrorFmt("(GSRunner/{}/{}/{}) Unable to ensure log directory '{}' exists (error: {})",
					s_runner_name, GSProcess::GetCurrentPID(), s_batch_id, s_logfile, error.GetDescription());
			}
		}
		else
		{
			VMManager::Internal::SetFileLogPath(s_logfile);
		}
		
		Console.WriteLn("Logging to %s...", s_logfile);
		
		// disable timestamps, since we want to be able to diff the logs
		s_settings_interface.SetBoolValue("Logging", "EnableTimestamps", false);
	}

	if (!s_regression_file.empty())
		return true; // Remaining arguments/checks are not needed if doing regression testing.

	if (params.filename.empty())
	{
		Console.Error("No dump filename provided and not in regression testing mode.");
		return false;
	}

	if (s_batch_mode)
	{
		if (!FileSystem::DirectoryExists(params.filename.c_str()))
		{
			Console.Error("Provided directory does not exist.");
			return false;
		}

		s_num_batches = std::max(s_num_batches, 1u);
		s_batch_id = s_batch_id % s_num_batches;
	}
	else
	{
		if (!VMManager::IsGSDumpFileName(params.filename))
		{
			Console.Error("Provided filename is not a GS dump.");
			return false;
		}
	}

	if (s_settings_interface.GetBoolValue("EmuCore/GS", "DumpGSData") && !dumpdir.empty())
	{
		s_dump_gs_data_dir_hw = dumpdir;
		s_dump_gs_data_dir_sw = dumpdir;
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
		if (!s_batch_mode)
		{
			// strip off all extensions
			std::string_view title(Path::GetFileTitle(params.filename));
			if (StringUtil::EndsWithNoCase(title, ".gs"))
				title = Path::GetFileTitle(title);

			s_output_prefix = Path::Combine(s_output_prefix, StringUtil::StripWhitespace(title));
			Console.WriteLn(fmt::format("Saving dumps as {}_frameN.png", s_output_prefix));
		}
		else
		{
			Console.WriteLn(fmt::format("Saving dumps to {}", s_output_prefix));
		}
	}

	return true;
}

bool GSTester::ParseCommandLineArgs(int argc, char* argv[])
{
	for (int i = 1; i < argc; i++)
	{
#define CHECK_ARG(str) !std::strcmp(argv[i], str)
#define ENSURE_ARG_COUNT(str, n) \
	do { \
		if (i + n >= argc) \
		{ \
			Console.Error("Not enough arguments for " str " (need " #n ")"); \
			return false; \
		} \
	} while (0)
		
		if (i == 1 && CHECK_ARG("tester"))
		{
			continue;
		}
		else if (CHECK_ARG("-help"))
		{
			PrintCommandLineHelp(argv[0]);
			return false;
		}
		else if (CHECK_ARG("-version"))
		{
			PrintCommandLineVersion();
			return false;
		}
		else if (CHECK_ARG("-input"))
		{
			ENSURE_ARG_COUNT("-input", 1);

			regression_dump_dir = StringUtil::StripWhitespace(argv[++i]);
			if (regression_dump_dir.empty())
			{
				Console.Error("Invalid input directory/file specified.");
				return false;
			}

			if (!FileSystem::DirectoryExists(regression_dump_dir.c_str()) && !FileSystem::FileExists(regression_dump_dir.c_str()))
			{
				Console.Error("Input directory/file does not exist.");
				return false;
			}

			continue;
		}
		else if (CHECK_ARG("-output"))
		{
			ENSURE_ARG_COUNT("-output", 1);

			regression_output_dir = StringUtil::StripWhitespace(argv[++i]);
			if (regression_output_dir.empty())
			{
				Console.Error("Invalid output directory specified.");
				return false;
			}

			Error e;
			if (!FileSystem::EnsureDirectoryExists(regression_output_dir.c_str(), true, &e))
			{
				Console.ErrorFmt("Error creating/checking directory: {}", e.GetDescription());
				return false;
			}

			continue;
		}
		else if (CHECK_ARG("-path"))
		{
			ENSURE_ARG_COUNT("-path", 2);

			regression_runner_path[0] = StringUtil::StripWhitespace(std::string(argv[++i]));
			regression_runner_path[1] = StringUtil::StripWhitespace(std::string(argv[++i]));

			continue;
		}
		else if (CHECK_ARG("-name"))
		{
			ENSURE_ARG_COUNT("-name", 2);

			regression_runner_name[0] = StringUtil::StripWhitespace(std::string(argv[++i]));
			regression_runner_name[1] = StringUtil::StripWhitespace(std::string(argv[++i]));

			continue;
		}
		else if (CHECK_ARG("-regression-packet-size"))
		{
			ENSURE_ARG_COUNT("-regression-packet-size", 1);

			std::optional<u32> size_mb = StringUtil::FromChars<u32>(argv[++i]);
			if (size_mb.has_value())
				GSTester::regression_packet_size = size_mb.value() * _1mb;

			continue;
		}
		else if (CHECK_ARG("-npackets"))
		{
			ENSURE_ARG_COUNT("-npackets", 1);

			regression_num_packets = StringUtil::FromChars<u32>(argv[++i]).value_or(regression_num_packets_default);

			continue;
		}
		else if (CHECK_ARG("-ndumps"))
		{
			ENSURE_ARG_COUNT("-ndumps", 1);

			regression_num_dumps = StringUtil::FromChars<u32>(argv[++i]).value_or(regression_num_dumps_default);

			continue;
		}
		else if (CHECK_ARG("-regression-dump-size"))
		{
			ENSURE_ARG_COUNT("-regression-dump-size", 1);

			std::optional<u32> size_mb = StringUtil::FromChars<u32>(argv[++i]);
			if (size_mb.has_value())
				GSTester::regression_dump_size = size_mb.value() * _1mb;

			continue;
		}
		else if (CHECK_ARG("-start-from"))
		{
			ENSURE_ARG_COUNT("-start-from", 1);

			regression_start_from_dump = StringUtil::StripWhitespace(argv[++i]);

			continue;
		}
		else if (CHECK_ARG("-verbose-level"))
		{
			ENSURE_ARG_COUNT("-verbose-level", 1);

			regression_verbose_level = StringUtil::FromChars<u32>(argv[++i]).value_or(regression_verbose_level_default);

			continue;
		}
		else if (CHECK_ARG("-log"))
		{
			regression_logging = true;

			continue;
		}
		else if (CHECK_ARG("-timeout"))
		{
			regression_deadlock_timeout = StringUtil::FromChars<double>(argv[++i]).value_or(regression_deadlock_timeout_default);

			continue;
		}
		else if (CHECK_ARG("-nthreads"))
		{
			i++; // Skip -- handled by the main thread.
			continue;
		}
		else
		{
			regression_runner_args.append(argv[i]);
			regression_runner_args.append(" ");
			continue;
		}

#undef CHECK_ARG
	}

	if (regression_dump_dir.empty())
	{
		Console.Error("Dump directory/file not provided.");
		return false;
	}

	if (regression_output_dir.empty())
	{
		Console.Error("Output directory not provided.");
		return false;
	}

		for (int i = 0; i < 2; i++)
	{
		if (regression_runner_path[i].empty())
		{
			Console.ErrorFmt("Runner {} path not provided.", i + 1);
			return false;
		}

		if (!FileSystem::FileExists(regression_runner_path[i].c_str()))
		{
			Console.ErrorFmt("Runner {} path does not exist: \"{}\"", i + 1, regression_runner_path[i]);
			return false;
		}

		if (regression_runner_name[i].empty())
		{
			regression_runner_name[i] = Path::GetFileName(regression_runner_path[i]);
		}
	}

	if (regression_runner_name[0] == regression_runner_name[1])
	{
		// Need unique names for output directories.
		regression_runner_name[0] += " (1)";
		regression_runner_name[1] += " (2)";
	}

	for (int i = 0; i < 2; i++)
	{
		regression_output_dir_runner[i] = Path::Combine(regression_output_dir, regression_runner_name[i]);

		Error e;
		if (!FileSystem::EnsureDirectoryExists(regression_output_dir_runner[i].c_str(), true, &e))
		{
			Console.ErrorFmt("Unable to create output directory '{}' (error: {})", regression_output_dir_runner[i], e.GetDescription());
			return false;
		}
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
	MTGS::RunOnGSThread([]() {

		if (s_batch_mode)
		{
			double seconds = s_batch_dump_timer.GetTimeSeconds();

			Console.WriteLnFmt("(GSRunner/{}) Finished dump '{}' in {:.2} seconds ({:.2} FPS)", GSDumpReplayer::GetRunnerName(),
				s_batch_dump_name, seconds, s_total_frames / seconds);
		}

		Console.WriteLnFmt("======= HW STATISTICS FOR {} ({}) FRAMES ========", s_total_frames, s_total_drawn_frames);
		Console.WriteLnFmt("@HWSTAT@ Draw Calls: {} (avg {})", s_total_draws, static_cast<u64>(std::ceil(s_total_draws / static_cast<double>(s_total_drawn_frames))));
		Console.WriteLnFmt("@HWSTAT@ Render Passes: {} (avg {})", s_total_render_passes, static_cast<u64>(std::ceil(s_total_render_passes / static_cast<double>(s_total_drawn_frames))));
		Console.WriteLnFmt("@HWSTAT@ Barriers: {} (avg {})", s_total_barriers, static_cast<u64>(std::ceil(s_total_barriers / static_cast<double>(s_total_drawn_frames))));
		Console.WriteLnFmt("@HWSTAT@ Copies: {} (avg {})", s_total_copies, static_cast<u64>(std::ceil(s_total_copies / static_cast<double>(s_total_drawn_frames))));
		Console.WriteLnFmt("@HWSTAT@ Uploads: {} (avg {})", s_total_uploads, static_cast<u64>(std::ceil(s_total_uploads / static_cast<double>(s_total_drawn_frames))));
		Console.WriteLnFmt("@HWSTAT@ Readbacks: {} (avg {})", s_total_readbacks, static_cast<u64>(std::ceil(s_total_readbacks / static_cast<double>(s_total_drawn_frames))));
		Console.WriteLn("============================================");
	});
}

#ifdef _WIN32
// We can't handle unicode in filenames if we don't use wmain on Win32.
#define main real_main
#endif

static void CPUThreadMain(VMBootParameters* params)
{
	if (VMManager::Initialize(*params))
	{
		// run until end
		GSDumpReplayer::SetLoopCount(GSRunner::s_loop_count);
		VMManager::SetState(VMState::Running);
		while (VMManager::GetState() == VMState::Running)
			VMManager::Execute();
		if (!GSRunner::s_batch_mode)
		{
			GSRunner::DumpStats();
		}
		VMManager::Shutdown(false);
	}

	VMManager::Internal::CPUThreadShutdown();
	GSRunner::StopPlatformMessagePump();
}

std::string GSTester::GetEventNameRunner(GSProcess::PID_t tester_pid, const std::string& runner_name)
{
	return std::string("runner event ") + std::to_string(tester_pid) + " " + runner_name;
}

std::string GSTester::GetEventNameTester(GSProcess::PID_t tester_pid)
{
	return std::string("tester event ") + std::to_string(tester_pid);
}

GSTester::ReturnValue GSTester::CopyDumpToSharedMemory(const std::vector<u8>& data, const std::string& name)
{
	pxAssert(data.size() <= regression_dump_size);
	
	GSDumpFileSharedMemory* dump_shared[2]{};

	for (int i = 0; i < 2; i++)
	{
		dump_shared[i] = regression_buffer[i].GetDumpWrite();
		if (!dump_shared[i])
		{
			return BUFFER_NOT_READY;
		}
	}

	for (int i = 0; i < 2; i++)
	{
		dump_shared[i]->SetSizeDump(data.size());
		dump_shared[i]->SetNameDump(name);
		std::memcpy(dump_shared[i]->GetPtrDump(), data.data(), data.size());
	}

	for (int i = 0; i < 2; i++)
		regression_buffer[i].DoneDumpWrite();

	return SUCCESS;
}

GSTester::ReturnValue GSTester::ProcessPackets()
{
	Error error;
	GSRegressionPacket* packets[2];

	ScopedGuard done_read([&]() {
		if (packets[0] && packets[1])
		{
			for (int i = 0; i < 2; i++)
			{
				regression_buffer[i].DonePacketRead();
			}
		}
	});

	for (int i = 0; i < 2; i++)
	{
		packets[i] = regression_buffer[i].GetPacketRead();
	}

	// We have packets from both runners. Compare and output if different.
	if (packets[0] && packets[1])
	{
		std::string name_dump[2];
		std::string name_packet[2];
		u32 type_packet[2];

		for (int i = 0; i < 2; i++)
		{
			name_dump[i] = packets[i]->GetNameDump();
			name_packet[i] = packets[i]->GetNamePacket();
			type_packet[i] = packets[i]->type;
		}

		if (name_dump[0] == name_dump[1])
		{
			DumpInfo* dump_curr = &regression_dumps[regression_dumps_map.at(name_dump[0])];

			if (name_packet[0] == name_packet[1] && type_packet[0] == type_packet[1])
			{
				const std::string& packet_name_curr = name_packet[0];
				u32 packet_type_curr = type_packet[0];

				if (regression_verbose_level >= VERBOSE_TESTER)
					Console.WriteLnFmt("(GSTester/{}) Comparing results for {} / {}.", GetTesterName(), dump_curr->name, packet_name_curr);

				if (packet_type_curr == GSRegressionPacket::IMAGE)
				{
					if (GSRegressionImageMemCmp(packets[0], packets[1]) != 0)
					{
						if (regression_verbose_level >= VERBOSE_TESTER)
							Console.WarningFmt("(GSTester/{}) Image packet '{}' has differences.", GetTesterName(), packet_name_curr);

						for (int i = 0; i < 2; i++)
						{
							std::string dump_title(GSRunner::GetDumpTitle(dump_curr->name));
							std::string image_dir = Path::Combine(regression_output_dir_runner[i], dump_title);

							if (!FileSystem::EnsureDirectoryExists(image_dir.c_str(), true, &error))
							{
								Console.ErrorFmt("(GSTester/{}) Unable to create directory: '{}' (error: {})", GetTesterName(), image_dir, error.GetDescription());
								dump_curr->packets_skipped++;
								return ERROR_;
							}

							std::string image_file = Path::Combine(image_dir, packet_name_curr + ".png");

							GSPng::Format format = GSConfig.DumpGSData ? GSPng::RGB_A_PNG : GSPng::RGB_PNG;

							if (!GSPng::Save(format, image_file, static_cast<const u8*>(packets[i]->GetData()), packets[i]->image_header.w,
								packets[i]->image_header.h, packets[i]->image_header.pitch, GSConfig.PNGCompressionLevel, false))
							{
								Console.ErrorFmt("(GSTester/{}) Unable to save image file: '{}'", GetTesterName(), image_file);
								dump_curr->packets_skipped++;
								return ERROR_;
							}
						}
					}
				}
				else if (packet_type_curr == GSRegressionPacket::HWSTAT)
				{
					if (packets[0]->hwstat != packets[1]->hwstat)
					{
						for (int i = 0; i < 2; i++)
						{
							std::string hwstat_file = Path::Combine(regression_output_dir_runner[i], "emulog.txt");

							std::ofstream oss(hwstat_file);

							if (!oss.is_open())
							{
								Console.ErrorFmt("(GSTester/{}) Unable to open HW stat file: '{}'", GetTesterName(), hwstat_file);
								dump_curr->packets_skipped++;
								return ERROR_;
							}

							oss << "frames" << KEY_VAL_DEL << packets[i]->hwstat.frames << std::endl;
							oss << "draws" << KEY_VAL_DEL << packets[i]->hwstat.draws << std::endl;
							oss << "render_passes" << KEY_VAL_DEL << packets[i]->hwstat.render_passes << std::endl;
							oss << "barriers" << KEY_VAL_DEL << packets[i]->hwstat.barriers << std::endl;
							oss << "copies" << KEY_VAL_DEL << packets[i]->hwstat.copies << std::endl;
							oss << "uploads" << KEY_VAL_DEL << packets[i]->hwstat.uploads << std::endl;
							oss << "readbacks" << KEY_VAL_DEL << packets[i]->hwstat.readbacks << std::endl;

							oss.close();
						}
					}
				}
				else if (packet_type_curr == GSRegressionPacket::DONE_DUMP)
				{
					Console.WriteLnFmt("(GSTester/{}) Completed dump '{}'", GetTesterName(), dump_curr->name);
					regression_dump_last_completed = dump_curr->name;
					dump_curr->state = DumpInfo::COMPLETED;
					dump_curr->packets_completed++;
				}
				else
				{
					Console.ErrorFmt("(GSTester/{}) Unknown packet type '{}'", GetTesterName(), packet_type_curr);
					dump_curr->packets_skipped++;
					return ERROR_;
				}

				dump_curr->packets_completed++; // Packet processed successfully.
				return SUCCESS;
			}
			else
			{
				Console.ErrorFmt("(GSTester/{}) Runners out of sync on following dumps:", GetTesterName());
				for (int i = 0; i < 2; i++)
				{
					Console.ErrorFmt("    {}: {} / {}", regression_runner_name[i], name_dump[i], name_packet[i]);
				}
				dump_curr->packets_skipped++;
				return ERROR_;
			}
		}
		else
		{
			Console.ErrorFmt("(GSTester/{}) Runners out of sync on following dumps:", GetTesterName());
			for (int i = 0; i < 2; i++)
			{
				Console.ErrorFmt("    {}: {} / {}", regression_runner_name[i], name_dump[i], name_packet[i]);
			}
			regression_packets_skipped_unknown++;
			return ERROR_;
		}
	}
	else
	{
		return BUFFER_NOT_READY;
	}
}

int GSRunner::main_runner(int argc, char* argv[])
{
	if (!InitializeConfig())
	{
		Console.Error("Failed to initialize config.");
		return EXIT_FAILURE;
	}

	VMBootParameters params;
	if (!ParseCommandLineArgs(argc, argv, params))
		return EXIT_FAILURE;

	if (!VMManager::Internal::CPUThreadInitialize())
		return EXIT_FAILURE;

	if (s_use_window.value_or(true) && !CreatePlatformWindow())
	{
		Console.Error("Failed to create window.");
		return EXIT_FAILURE;
	}

	// Regression testing needs to be started before applying settings
	// or it might complain that there is no dumping directory
	// (regression test data is dumped to memory).
	if (!s_regression_file.empty())
	{

		if (s_parent_pid == 0)
		{
			Console.ErrorFmt("(GSRunner/{}/{}) Regression testing without a valid parent PID.", s_runner_name, GSProcess::GetCurrentPID());
			return EXIT_FAILURE;
		}

		GSStartRegressionTest(
			&s_regression_buffer,
			s_regression_file,
			GSTester::GetEventNameRunner(s_parent_pid, s_runner_name),
			GSTester::GetEventNameTester(s_parent_pid),
			s_regression_num_packets,
			s_regression_packet_size,
			s_regression_num_dumps,
			s_regression_dump_size);

		if (!GSProcess::SetParentPID(s_parent_pid))
		{
			Console.ErrorFmt("(GSRunner/{}/{}) Unable to open parent PID {}.", s_runner_name, GSProcess::GetCurrentPID(), s_parent_pid);
			return EXIT_FAILURE;
		}

		Console.WriteLnFmt("(GSRunner/{}/{}) Opened parent PID {}.", s_runner_name, GSProcess::GetCurrentPID(), s_parent_pid);
	}
	
	// Override settings that shouldn't be picked up from defaults or INIs.
	GSRunner::SettingsOverride();

	// apply new settings (e.g. pick up renderer change)
	VMManager::ApplySettings();
	GSDumpReplayer::SetIsDumpRunner(true, s_runner_name);
	GSDumpReplayer::SetFrameNumberMax(s_frames_max);
	GSDumpReplayer::SetVerboseLogging(s_verbose_logging);
	if (s_batch_mode)
	{
		GSDumpReplayer::SetIsBatchMode(true);
		GSDumpReplayer::SetNumBatches(s_num_batches);
		GSDumpReplayer::SetBatchID(s_batch_id);
		GSDumpReplayer::SetLoopCountStart(s_loop_count);
		GSDumpReplayer::SetBatchDefaultGSOptions(EmuConfig.GS);
		GSDumpReplayer::SetBatchRecreateDevice(s_batch_recreate_device);
		if (!s_batch_start_from_dump.empty())
		{
			GSDumpReplayer::SetBatchStartFromDump(s_batch_start_from_dump);
		}
		if (GSIsRegressionTesting())
		{
			// Send HWSTAT packets only if logging to file is disabled.
			GSDumpReplayer::SetRegressionSendHWSTAT(s_logfile.empty());
		}
	}

	std::thread cputhread(CPUThreadMain, &params);
	PumpPlatformMessages(/*forever=*/true);
	cputhread.join();

	VMManager::Internal::CPUThreadShutdown();
	DestroyPlatformWindow();
	if (GSIsRegressionTesting())
		GSEndRegressionTest();
	return EXIT_SUCCESS;
}

std::string GSTester::GetTesterName()
{
	return std::to_string(regression_thread_id);
}

bool GSTester::StartRunners()
{
	const auto quote = [](std::string x) { return "\"" + x + "\""; };

	// Start the runner processes in regression testing mode.
	for (int i = 0; i < 2; i++)
	{
		GSProcess::PID_t pid = GSProcess::GetCurrentPID();

		if (regression_runner_command[i].empty())
		{
			regression_runner_command[i] =
				quote(regression_runner_path[i]) +
				std::string(" runner ") +
				std::string(" -surfaceless ") +
				std::string(" -noshadercache ") +
				std::string(" -loop 1 ") +
				(regression_logging ?
					std::string(" -logfile ") + quote(regression_output_dir_runner[i]) :
					"") +
				std::string(" -regression-test ") + quote(regression_shared_file[i]) +
				std::string(" -name ") + quote(regression_runner_name[i]) +
				std::string(" -regression-npackets-buffer ") + std::to_string(regression_num_packets) +
				std::string(" -regression-packet-size ") + std::to_string(regression_packet_size / _1mb) +
				std::string(" -batch-ndumps-buffer ") + std::to_string(regression_num_dumps) +
				std::string(" -regression-dump-size ") + std::to_string(regression_dump_size / _1mb) +
				std::string(" -regression-ppid ") + std::to_string(pid) +
				std::string(" -batch ") +
				std::string(" -nbatches ") + std::to_string(regression_nthreads) +
				std::string(" -batch-id ") + std::to_string(regression_thread_id) +
				(regression_verbose_level >= VERBOSE_TESTER_AND_RUNNER ?
					std::string(" -verbose ") :
					"") +
				" " + regression_runner_args;
		}

		if (!regression_runner_proc[i].Start(regression_runner_command[i], regression_verbose_level < VERBOSE_TESTER_AND_RUNNER))
		{
			Console.ErrorFmt("(GSTester/{}) Unable to start runner: {} (command: {})", GetTesterName(), regression_runner_name[i],
				regression_runner_command[i]);
			return false;
		}

		Console.WriteLnFmt("(GSTester/{}) Created runner process (PID: {}) with command: '{}'", GetTesterName(),
			regression_runner_proc[i].GetPID(), regression_runner_command[i]);
	}

	// Wait until the runners initialize and hit the dump waiting loop.
	Common::Timer timer;
	constexpr double start_timeout = 30.0;
	while (true)
	{
		if (timer.GetTimeSeconds() > start_timeout)
		{
			Console.ErrorFmt("(GSTester/{}) Both runners not initialized after {:.1} seconds.", GetTesterName(), start_timeout);
			return false;
		}

		if (regression_buffer[0].GetStateRunner() == GSRegressionBuffer::WAIT_DUMP &&
			regression_buffer[1].GetStateRunner() == GSRegressionBuffer::WAIT_DUMP)
		{
			return true;
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));

		Console.WriteLnFmt("(GSTester/{}) Waiting for runner processes...", GetTesterName());
	}

	return false; // Unreachable
}

// Try to end the runner processes gracefully and otherwise terminate them.
bool GSTester::EndRunners()
{
	for (int i = 0; i < 2; i++)
		regression_buffer[i].SetStateTester(GSRegressionBuffer::EXIT);

	constexpr double terminate_timeout = 30.0; // Seconds to wait before forcefully terminating processes.
	Common::Timer timer;
	double sec;
	while (true)
	{
		if (!regression_runner_proc[0].IsRunning() && !regression_runner_proc[1].IsRunning())
		{
			Console.WriteLnFmt("(GSTester/{}) Both runners exited in {:.2} seconds.", GetTesterName(), timer.GetTimeSeconds());
			break;
		}

		if ((sec = timer.GetTimeSeconds()) >= terminate_timeout)
		{
			Console.ErrorFmt("(GSTester/{}) Both runners not exited after {:.1} seconds.", GetTesterName(), terminate_timeout);
			break;
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));

		Console.WriteLnFmt("(GSTester/{}) Waiting for runners to exit...", GetTesterName());
	}

	if (regression_runner_proc[0].IsRunning() || regression_runner_proc[1].IsRunning())
	{
		Console.ErrorFmt("(GSTester/{}) Unable to safely end runner processes...terminating.", GetTesterName());
		for (int i = 0; i < 2; i++)
			regression_runner_proc[i].Terminate();
	}

	return !regression_runner_proc[0].IsRunning() && !regression_runner_proc[1].IsRunning();
}

bool GSTester::RestartRunners()
{
	if (!EndRunners())
		return false;

	for (int i = 0; i < 2; i++)
		regression_buffer[i].Reset();

	return StartRunners();
}

bool GSTester::GetDumpInfo()
{
	if (VMManager::IsGSDumpFileName(regression_dump_dir))
	{
		regression_dump_files.push_back(regression_dump_dir);
	}
	else if (FileSystem::DirectoryExists(regression_dump_dir.c_str()))
	{
		GSDumpReplayer::GetDumpFileList(regression_dump_dir, regression_dump_files, regression_nthreads, regression_thread_id, regression_start_from_dump);
	}
	else
	{
		Console.WarningFmt("(GSTester/{}) Provided file is neither a dump or a directory: '{}'", GetTesterName(), regression_dump_dir);
		return false;
	}

	for (const std::string& file : regression_dump_files)
	{
		std::string name(Path::GetFileName(file));
		regression_dumps.push_back(DumpInfo(file, name));
	}

	for (std::size_t i = 0; i < regression_dumps.size(); i++)
	{
		regression_dumps_map.insert({regression_dumps[i].name, i});
	}

	return true;
}

int GSTester::MainThread(int argc, char* argv[], u32 nthreads, u32 thread_id)
{
	regression_nthreads = nthreads;
	regression_thread_id = thread_id;

	if (!ParseCommandLineArgs(argc, argv))
		return EXIT_FAILURE;

	if (!GetDumpInfo())
		return EXIT_FAILURE;

	Console.WriteLnFmt("(GSTester/{}) Found {} dumps in '{}'", GetTesterName(), regression_dumps.size(), regression_dump_dir);

	// Make the tester event.
	regression_event_tester.Create(GetEventNameTester(GSProcess::GetCurrentPID()));

	Console.WriteLnFmt("(GSTester/{}) Creating shared memory files.", GetTesterName());
	for (int i = 0; i < 2; i++)
	{
		regression_shared_file[i] = "regression-test-file-" + std::to_string(GSProcess::GetCurrentPID()) +
			"-" + std::to_string(thread_id) + "-" + std::to_string(i);

		if (!regression_buffer[i].CreateFile_(
			regression_shared_file[i],
			GetEventNameRunner(GSProcess::GetCurrentPID(), regression_runner_name[i]),
			regression_event_tester.GetName(),
			regression_num_packets,
			regression_packet_size,
			regression_num_dumps,
			regression_dump_size))
		{
			Console.ErrorFmt("(GSTester) Unable to create regression shared file: {}", regression_shared_file[i]);
			return EXIT_FAILURE;
		}

		Console.WriteLnFmt("(GSTester) Created regression packets file: {}", regression_shared_file[i]);
	}

	Console.WriteLnFmt("(GSTester/{}) Starting runner processes.", GetTesterName());
	if (!StartRunners())
	{
		Console.ErrorFmt("(GSTester/{}) Unable to start runner processes. Exiting.", GetTesterName());
		return EXIT_FAILURE;
	}
	Console.WriteLnFmt("(GSTester/{}) Runners processes are initialized.", GetTesterName());

	std::size_t dump_index = 0; // Current dump that should be written to dump buffer.
	Common::Timer deadlock_timer; // Time since seeing both runners' heartbeats.
	Common::Timer activity_timer; // Time since uploading a dump or reading a packet.
	constexpr std::size_t max_failure_restarts = 10; // Max times to attempt restarting before giving up.
	std::size_t failure_restarts = 0; // Current number of failure restarts.
	std::size_t loop_counter = 0; // Number of main loop iterations.

	std::optional<GSDumpFileLoader> dump_loader;
	dump_loader.emplace(regression_num_dumps, regression_num_dumps, regression_dump_size);
	dump_loader->Start(regression_dump_files);
	bool done_uploading = false;

	// Temporary loop variables.
	std::vector<u8> dump_data; // Cache the dump from disk to shared with runner processes.
	std::string dump_name; // Cache the dump from disk to shared with runner processes.
	Error error; // Current error.
	bool fail = false; // Signals a failure at some point in processing.

	Console.WriteLnFmt("(GSTester/{}) Starting main testing loop.", GetTesterName());

	// Main testing loop.
	while (true)
	{
		loop_counter++;

		// Failure restarting.
		if (fail)
		{
			fail = false;

			for (int i = 0; i < 2; i++)
			{
				Console.WarningFmt("Debug for {}", regression_runner_name[i]);
				regression_buffer[i].DebugState();
				regression_buffer[i].DebugDumpBuffer();
				regression_buffer[i].DebugPacketBuffer();
				Console.WarningFmt("");
			}

			dump_loader->Stop();
			dump_loader->DebugPrint();
			Console.WarningFmt("");

			if (++regression_failure_restarts >= regression_failure_restarts_max)
			{
				Console.ErrorFmt("(GSTester/{}) Attempted restarting {} times due to failures...exiting.", GetTesterName(), regression_failure_restarts);
				EndRunners();
				break;
			}
			else
			{
				Console.ErrorFmt("(GSTester/{}) Attempting to restart due to failure (attempt {}).", GetTesterName(), regression_failure_restarts);

				// Reset dump to the last one we got packets for.
				if (regression_dump_last_completed.empty())
				{
					Console.ErrorFmt("(GSTester/{}) No dumps completed; starting from beginning.", GetTesterName());
					dump_index = 0;
				}
				else
				{
					dump_index = regression_dumps_map.at(regression_dump_last_completed) + 1;
					Console.ErrorFmt("(GSTester/{}) Restarting from {}.", GetTesterName(), regression_dumps[dump_index].name);
				}

				// Reset stats of subsequent dumps.
				for (std::size_t i = dump_index; i < regression_dumps.size(); i++)
				{
					regression_dumps[i].packets_skipped = 0;
					regression_dumps[i].packets_completed = 0;
					regression_dumps[i].state = DumpInfo::UNVISITED;
				}

				// Restart the dump loader.
				dump_loader.emplace(regression_num_dumps, regression_num_dumps, regression_dump_size);
				dump_loader->Start(regression_dump_files, regression_dumps[dump_index].name);

				// Restart the runner processes
				if (!RestartRunners())
				{
					Console.ErrorFmt("(GSTester/{}) Failed to restart.", GetTesterName());
					break;
				}

				// Reset timers.
				activity_timer.Reset();
				deadlock_timer.Reset();
			}
		}

		// Dump uploading processing.
		if (!done_uploading)
		{
			std::string error;
			DumpInfo* info = nullptr;
			GSDumpFileLoader::ReturnValue retval;

			while (true)
			{
				if (dump_data.empty())
				{
					double block_time;
					double load_time;

					retval = dump_loader->Get(dump_data, &dump_name, &error, &block_time, &load_time, false);

					if (retval == GSDumpFileLoader::SUCCESS || retval == GSDumpFileLoader::ERROR_)
					{
						dump_name = Path::GetFileName(dump_name);
						info = &regression_dumps[regression_dumps_map.at(dump_name)];

						info->load_time = load_time;
						info->block_time = block_time;
					}
				}
				else
				{
					info = &regression_dumps[regression_dumps_map.at(dump_name)];
					retval = GSDumpFileLoader::SUCCESS;
				}
				
				if (retval == GSDumpFileLoader::SUCCESS)
				{
					pxAssert(!dump_data.empty());

					Common::Timer timer;

					ReturnValue retval2 = CopyDumpToSharedMemory(dump_data, dump_name);

					double seconds = timer.GetTimeSeconds();

					if (retval2 == SUCCESS)
					{
						Console.WriteLnFmt("(GSTester/{}) Copied '{}' to shared memory (load: {:.2} seconds; block: {:.2} seconds; copy: {:.2} seconds)",
							GetTesterName(), dump_name, info->load_time, info->block_time, seconds);
						dump_data.clear();
						activity_timer.Reset();
					}
					else if (retval2 == ERROR_)
					{
						Console.ErrorFmt("(GSTester/{}) Error copying '{}' to shared memory ({:.2} seconds).", GetTesterName(), regression_dumps[dump_index].file, seconds);
						regression_dumps[dump_index].state = DumpInfo::SKIPPED;
						dump_data.clear();
					}
					else if (retval2 == BUFFER_NOT_READY)
					{
						// Try again next iteration.
					}
					else
					{
						pxFail("Unknown return value."); // Impossible.
					}
					break;
				}
				else if (retval == GSDumpFileLoader::FINISHED)
				{
					done_uploading = true;
					Console.WarningFmt("(GSTester/{}) Done uploading dumps.", GetTesterName());
					dump_loader->DebugPrint();
					for (int i = 0; i < 2; i++)
						regression_buffer[i].SetStateTester(GSRegressionBuffer::DONE_UPLOADING);
					break;
				}
				else if (retval == GSDumpFileLoader::ERROR_)
				{
					info->state = DumpInfo::SKIPPED;
					Console.ErrorFmt("(GSTester/{}) Error loading dump '{}': {}.", GetTesterName(), dump_name,
						error.empty() ? std::string("Unspecified reason") : error);
				}
				else if (retval == GSDumpFileLoader::EMPTY)
				{
					break; // Try again next iteration.
				}
				else
				{
					pxFail("Unknown return value."); // Impossible.
				}
			}
		}

		// Packet processing.
		{
			ReturnValue retval = ProcessPackets();

			if (retval == SUCCESS)
			{
				activity_timer.Reset();
			}
			else if (retval == ERROR_)
			{
				Console.ErrorFmt("(GSTester/{}) Error in processing packets.", GetTesterName());
				fail = true;
				continue;
			}
			else if (retval == BUFFER_NOT_READY)
			{
				// Try again next iteration.
			}
			else
			{
				pxFailRel("Unknown return value (impossible).");
			}
		}

		// Check if remaining dumps are all skipped.
		{
			std::size_t i =
				regression_dump_last_completed.empty() ?
					0 :
			        regression_dumps_map.at(regression_dump_last_completed) + 1;
			while (i < regression_dumps.size() && regression_dumps[i].state == DumpInfo::SKIPPED)
			{
				i++;
			}
			if (i >= regression_dumps.size())
			{
				Console.WriteLnFmt("(GSTester/{}) All dumps/packets finished.", GetTesterName());
				break;
			}
		}

		// Do expensive checks every 1024 iterations.
		if ((loop_counter & 0x3FF) == 0)
		{
			// Handle children exiting unexpectedly.
			int num_exited = 0;
			for (int i = 0; i < 2; i++)
			{
				bool running = regression_runner_proc[i].IsRunning();
				u32 state = regression_buffer[i].GetStateRunner();
				if (!running)
				{
					if (state == GSRegressionBuffer::DONE_RUNNING)
					{
						num_exited++;
					}
					else
					{
						Console.ErrorFmt("(GSTester/{}) Runner {} exited unexpectedly.", GetTesterName(), regression_runner_name[i]);
						fail = true;
					}
				}
			}

			if (num_exited == 2 && !done_uploading)
			{
				// Both processes exited and we didn't finish uploading. Something bad happened.
				Console.ErrorFmt("(GSTester/{}) Both runners exited unexpectedly.", GetTesterName());
				fail = true;
			}

			if (fail)
				continue;

			// Check if children are alive and handle possible deadlock.
			if (regression_buffer[0].CheckRunnerHeartbeat() && regression_buffer[1].CheckRunnerHeartbeat())
			{
				for (int i = 0; i < 2; i++)
					regression_buffer[i].ResetRunnerHeartbeat();
				deadlock_timer.Reset();
			}
			else if (deadlock_timer.GetTimeSeconds() >= regression_deadlock_timeout)
			{
				if (regression_dump_last_completed.empty())
					Console.ErrorFmt("(GSTester/{}) Possible deadlock on dump {} (timeout: {:.2} seconds).", GetTesterName(), regression_dumps[0].name, regression_deadlock_timeout);
				else
					Console.ErrorFmt("(GSTester/{}) Possible deadlock detected after dump {}.", GetTesterName(), regression_dump_last_completed);
				deadlock_timer.Reset();
				fail = true;
				continue;
			}

			if (activity_timer.GetTimeSeconds() >= GSRegressionBuffer::EVENT_WAIT_SECONDS)
			{
				// Wait on event if no packets for some time.
				regression_event_tester.Reset();
				regression_event_tester.Wait(GSRegressionBuffer::EVENT_WAIT_SECONDS);
				activity_timer.Reset();
			}
		}
		else
		{
			std::this_thread::yield();
		}
	}

	EndRunners();

	for (int i = 0; i < 2; i++)
	{
		if (regression_runner_proc[i].WaitForExit() != 0)
		{
			Console.WarningFmt("(GSTester/{}) Runner {} exited abnormally.", GetTesterName(), regression_runner_name[i]);
		}
	}

	for (int i = 0; i < 2; i++)
	{
		regression_runner_proc[i].Close();
	}

	for (int i = 0; i < 2; i++)
		regression_buffer[i].CloseFile();

	// Get stats for main thread.
	regression_dumps_completed = 0;
	regression_dumps_skipped = 0;
	regression_packets_completed = 0;
	regression_packets_skipped = 0;
	regression_dumps_unvisited = 0;
	regression_dump_load_time = 0.0;
	for (std::size_t i = 0; i < regression_dumps.size(); i++)
	{
		bool completed = regression_dumps[i].state == DumpInfo::COMPLETED;
		bool skipped = regression_dumps[i].state == DumpInfo::SKIPPED;
		bool unvisited = regression_dumps[i].state == DumpInfo::UNVISITED;

		regression_packets_completed += regression_dumps[i].packets_completed;
		regression_packets_skipped += regression_dumps[i].packets_skipped;
		regression_dumps_completed += completed;
		regression_dumps_skipped += skipped;
		regression_dumps_unvisited += unvisited;
		regression_dump_load_time += completed ? regression_dumps[i].load_time : 0.0;
	}

	return EXIT_SUCCESS;
}

int GSTester::main_tester(int argc, char* argv[])
{
	Common::Timer timer_total;
	std::string output_dir;

	int nthreads = 1;
	for (int i = 1; i < argc; i++)
	{
		if (strncmp(argv[i], "-nthreads", 9) == 0)
		{
			if (i + 1 >= argc)
			{
				Console.ErrorFmt("(GSTester) Expected an argument for '-nthreads'");
				return EXIT_FAILURE;
			}
			nthreads = StringUtil::FromChars<int>(argv[++i]).value_or(1);
		}
		else if (strncmp(argv[i], "-output", 7) == 0)
		{
			if (i + 1 >= argc)
			{
				Console.ErrorFmt("(GSTester) Expected an argument for '-output'");
				return EXIT_FAILURE;
			}
			
			output_dir = StringUtil::StripWhitespace(argv[++i]);
		}
	}

	Error error;
	if (!FileSystem::EnsureDirectoryExists(output_dir.c_str(), false, &error))
	{
		Console.ErrorFmt("(GSTester) Unable to create output directory '{}'", output_dir);
		return EXIT_FAILURE;
	}

	std::string logfile = Path::Combine(output_dir, "tester_log.txt");
	VMManager::Internal::SetFileLogPath(logfile);

	nthreads = std::clamp(nthreads, 1, 8);

	Console.WriteLnFmt("(GSTester) Running regression test with {} threads.", nthreads);

	std::vector<GSTester> testers;
	std::vector<int> return_value;
	std::vector<std::thread> threads;
	
	testers.resize(nthreads);
	return_value.resize(nthreads);
	threads.resize(nthreads);

	for (int i = 0; i < nthreads; i++)
		return_value[i] = EXIT_FAILURE;

	const auto run_thread = [&](u32 thread_id) {
		return_value[thread_id] = testers[thread_id].MainThread(argc, argv, nthreads, thread_id);
	};
	
	for (int i = 1; i < nthreads; i++)
	{
		threads[i] = std::thread(run_thread, i);
	}

	// Run ID 0 on main thread and rest on worker threads.
	run_thread(0);
	for (int i = 1; i < nthreads; i++)
		threads[i].join();

	std::string threads_failed;
	for (int i = 0; i < nthreads; i++)
	{
		if (return_value[i] != EXIT_SUCCESS)
		{
			if (!threads_failed.empty())
				threads_failed += ", ";
			threads_failed += std::to_string(i);
		}
	}

	if (threads_failed.empty())
	{
		Console.WriteLn("(GSTester) All threads succeeded.");
	}
	else
	{
		Console.WriteLnFmt("(GSTester) The follow threads failed: {}", threads_failed);
	}

	std::size_t dumps_total;
	std::size_t dumps_completed = 0;
	std::size_t dumps_skipped = 0;
	std::size_t dumps_unvisited = 0;
	std::size_t packets_completed = 0;
	std::size_t packets_skipped = 0;
	std::size_t packets_skipped_unknown = 0;
	std::size_t failure_restarts = 0;
	double dump_load_time = 0.0;

	for (int i = 0; i < nthreads; i++)
	{
		dumps_completed += testers[i].regression_dumps_completed;
		dumps_skipped += testers[i].regression_dumps_skipped;
		dumps_unvisited += testers[i].regression_dumps_unvisited;
		packets_completed += testers[i].regression_packets_completed;
		packets_skipped += testers[i].regression_packets_skipped;
		packets_skipped_unknown += testers[i].regression_packets_skipped_unknown;
		failure_restarts += testers[i].regression_failure_restarts;
		dump_load_time += testers[i].regression_dump_load_time;
	}

	dumps_total = dumps_completed + dumps_skipped + dumps_unvisited;

	double run_time = timer_total.GetTimeSeconds();

	const auto safe_avg = [](auto a, auto b) {
		return b == 0 ? 0 : a / b;
	};

	Console.WriteLnFmt("GSTester Stats:");
	Console.WriteLnFmt("    Run time: {:.2} minutes (Avg {:.2} sec)", run_time / 60.0, safe_avg(run_time, dumps_completed));
	Console.WriteLnFmt("    Dump load time: {:.2} minutes (Avg {:.2} sec)", dump_load_time / 60.0, safe_avg(dump_load_time, dumps_completed));
	Console.WriteLnFmt("    Dumps completed: {} / {}", dumps_completed, dumps_total);
	Console.WriteLnFmt("    Dumps skipped: {} / {}", dumps_skipped, dumps_total);
	Console.WriteLnFmt("    Dumps unvisited: {} / {}", dumps_unvisited, dumps_total);
	Console.WriteLnFmt("    Packets completed: {} (Avg {})",
		packets_completed, safe_avg(packets_completed, dumps_total));
	Console.WriteLnFmt("    Packets skipped: {} (Avg {})", packets_skipped, safe_avg(packets_skipped, dumps_total));
	Console.WriteLnFmt("    Packets skipped unknown: {} (Avg {})",
		packets_skipped_unknown, safe_avg(packets_skipped_unknown, dumps_total));
	Console.WriteLnFmt("    Failure restarts: {}", failure_restarts);

	return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
	CrashHandler::Install();
	GSRunner::InitializeConsole();

	if (argc < 2)
	{
		Console.Error("Need at least one argument");
		return EXIT_FAILURE;
	}

	std::string mode(argv[1]);

	if (mode == "tester")
	{
		return GSTester::main_tester(argc, argv);
	}
	else
	{
		return GSRunner::main_runner(argc, argv);
	}
}

void Host::PumpMessagesOnCPUThread()
{
	// update GS thread copy of frame number/loop number
	MTGS::RunOnGSThread(
		[frame_number = GSDumpReplayer::GetFrameNumber(), loop_number = GSDumpReplayer::GetLoopCount()]() {
			GSRunner::s_dump_frame_number = frame_number;
			GSRunner::s_loop_number = loop_number;
		}
	);
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
