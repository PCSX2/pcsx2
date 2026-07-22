// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <deque>
#include <functional>
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
#include "common/Timer.h"

#include "pcsx2/PrecompiledHeader.h"

#include "pcsx2/Achievements.h"
#include "pcsx2/CDVD/CDVD.h"
#include "pcsx2/GS.h"
#include "pcsx2/GS/Renderers/Common/GSDevice.h"
#include "pcsx2/GS/GSPerfMon.h"
#include "pcsx2/GS/Renderers/HW/GSDrawLog.h"
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
#if defined(__linux__) && defined(X11_API)
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
static double s_last_depth_copies_rov = 0;
static double s_last_draws_rov = 0;
static double s_last_barriers_rov = 0;
static u64 s_total_internal_draws = 0;
static u64 s_total_draws = 0;
static u64 s_total_render_passes = 0;
static u64 s_total_barriers = 0;
static u64 s_total_copies = 0;
static u64 s_total_uploads = 0;
static u64 s_total_readbacks = 0;
static u64 s_total_copies_rov = 0;
static u64 s_total_draws_rov = 0;
static u64 s_total_barriers_rov = 0;
static u32 s_total_frames = 0;
static u32 s_total_drawn_frames = 0;
static std::vector<std::string> s_extended_stats_snapshot;

// Per-frame statistics series. Run-aggregate min/avg/max cannot locate a spike, so
// every presented frame is recorded and written out as JSON at the end of the run.
// Counters are exact per-frame deltas; frame_ms is measured here rather than taken
// from PerformanceMetrics, whose values are window averages.
struct FrameSample
{
	u32 frame;
	bool idle;
	float frame_ms;
	float gpu_ms;
	u64 prims;
	u64 draws; // PS2-level (GSPerfMon::Draw)
	u64 draw_calls;
	u64 render_passes;
	u64 barriers;
	u64 copies;
	u64 uploads;
	u64 readbacks;
	u64 copies_rov;
	u64 draw_calls_rov;
	u64 barriers_rov;
	u64 tc_source_hit;
	u64 tc_source_miss;
	u64 tc_target_hit;
	u64 tc_target_miss;
	u64 hash_cache_hit;
	u64 hash_cache_miss;
};
// Work posted from other threads (the PINE server) to run on the CPU thread.
static std::mutex s_cpu_thread_tasks_mutex;
static std::condition_variable s_cpu_thread_tasks_done;
static std::deque<std::function<void()>> s_cpu_thread_tasks;

static std::string s_stats_json_path;
static std::string s_drawlog_path;
static std::vector<FrameSample> s_frame_samples;
static std::string s_device_name;
static std::string s_driver_info;
static u64 s_frame_timer_last = 0;
static double s_last_prims = 0;
static double s_last_tc_source_hit = 0;
static double s_last_tc_source_miss = 0;
static double s_last_tc_target_hit = 0;
static double s_last_tc_target_miss = 0;
static double s_last_hash_cache_hit = 0;
static double s_last_hash_cache_miss = 0;
static u64 s_total_prims = 0;
static u64 s_total_tc_source_hit = 0;
static u64 s_total_tc_source_miss = 0;
static u64 s_total_tc_target_hit = 0;
static u64 s_total_tc_target_miss = 0;
static u64 s_total_hash_cache_hit = 0;
static u64 s_total_hash_cache_miss = 0;

static bool s_perf_enable = false;
static bool s_force_vsync = false;
static float s_perf_updates = 0.0f;
static float s_perf_sum_fps = 0.0f;
static float s_perf_sum_internal_fps = 0.0f;
static float s_perf_sum_cpu_thread_usage = 0.0f;
static float s_perf_sum_cpu_thread_time = 0.0f;
static float s_perf_sum_gs_thread_usage = 0.0f;
static float s_perf_sum_gs_thread_time = 0.0f;
static float s_perf_sum_gpu_time = 0.0f;
static float s_perf_sum_gpu_usage = 0.0f;

bool GSRunner::InitializeConfig()
{
	EmuFolders::SetAppRoot();
	if (!EmuFolders::SetResourcesDirectory() || !EmuFolders::SetDataDirectory(nullptr))
		return false;

	CrashHandler::SetWriteDirectory(EmuFolders::DataRoot);

	const char* error;
	if (!VMManager::PerformEarlyHardwareChecks(&error))
		return false;

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

std::string Host::GetTextFromClipboard()
{
	return std::string();
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

void Host::SetMouseLock(bool state)
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
		// Captured here rather than at shutdown: this runs on the GS thread with the
		// device definitely live, and it is the axis that decides whether a settings
		// A/B was even applied (several GS features are force-overridden per-driver).
		if (s_device_name.empty() && g_gs_device)
		{
			s_device_name = g_gs_device->GetName();
			s_driver_info = g_gs_device->GetDriverInfo();
		}

		const u32 last_draws = s_total_internal_draws;
		const u32 last_uploads = s_total_uploads;

		// Returns this frame's delta as well as accumulating it, so the per-frame
		// series and the run totals stay derived from one source.
		static constexpr auto update_stat = [](GSPerfMon::counter_t counter, u64& dst, double& last) -> u64 {
			// perfmon resets every 32 frames to zero
			const double val = g_perfmon.GetCounter(counter);
			const u64 delta = static_cast<u64>((val < last) ? val : (val - last));
			dst += delta;
			last = val;
			return delta;
		};

		FrameSample sample = {};
		sample.frame = s_total_frames;
		sample.prims = update_stat(GSPerfMon::Prim, s_total_prims, s_last_prims);
		sample.draws = update_stat(GSPerfMon::Draw, s_total_internal_draws, s_last_internal_draws);
		sample.draw_calls = update_stat(GSPerfMon::DrawCalls, s_total_draws, s_last_draws);
		sample.render_passes = update_stat(GSPerfMon::RenderPasses, s_total_render_passes, s_last_render_passes);
		sample.barriers = update_stat(GSPerfMon::Barriers, s_total_barriers, s_last_barriers);
		sample.copies = update_stat(GSPerfMon::TextureCopies, s_total_copies, s_last_copies);
		sample.uploads = update_stat(GSPerfMon::TextureUploads, s_total_uploads, s_last_uploads);
		sample.readbacks = update_stat(GSPerfMon::Readbacks, s_total_readbacks, s_last_readbacks);
		sample.copies_rov = update_stat(GSPerfMon::TextureCopiesROV, s_total_copies_rov, s_last_depth_copies_rov);
		sample.draw_calls_rov = update_stat(GSPerfMon::DrawCallsROV, s_total_draws_rov, s_last_draws_rov);
		sample.barriers_rov = update_stat(GSPerfMon::BarriersROV, s_total_barriers_rov, s_last_barriers_rov);
		sample.tc_source_hit = update_stat(GSPerfMon::TCSourceHit, s_total_tc_source_hit, s_last_tc_source_hit);
		sample.tc_source_miss = update_stat(GSPerfMon::TCSourceMiss, s_total_tc_source_miss, s_last_tc_source_miss);
		sample.tc_target_hit = update_stat(GSPerfMon::TCTargetHit, s_total_tc_target_hit, s_last_tc_target_hit);
		sample.tc_target_miss = update_stat(GSPerfMon::TCTargetMiss, s_total_tc_target_miss, s_last_tc_target_miss);
		sample.hash_cache_hit = update_stat(GSPerfMon::HashCacheHit, s_total_hash_cache_hit, s_last_hash_cache_hit);
		sample.hash_cache_miss = update_stat(GSPerfMon::HashCacheMiss, s_total_hash_cache_miss, s_last_hash_cache_miss);

		const bool idle_frame = s_total_frames && (last_draws == s_total_internal_draws && last_uploads == s_total_uploads);

		if (!idle_frame)
			s_total_drawn_frames++;

		s_total_frames++;

		if (!s_stats_json_path.empty())
		{
			const u64 now = Common::Timer::GetCurrentValue();
			sample.idle = idle_frame;
			// First frame has no predecessor to measure against.
			sample.frame_ms = s_frame_timer_last ?
			                      static_cast<float>(Common::Timer::ConvertValueToMilliseconds(now - s_frame_timer_last)) :
			                      0.0f;
			s_frame_timer_last = now;
			sample.gpu_ms = PerformanceMetrics::GetLastGPUTime();
			s_frame_samples.push_back(sample);
		}

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
	if (s_perf_enable)
	{
		s_perf_updates += 1.0f;
		s_perf_sum_fps += PerformanceMetrics::GetFPS();
		s_perf_sum_internal_fps += PerformanceMetrics::GetInternalFPS();
		s_perf_sum_cpu_thread_usage += PerformanceMetrics::GetCPUThreadUsage();
		s_perf_sum_cpu_thread_time += PerformanceMetrics::GetCPUThreadAverageTime();
		s_perf_sum_gs_thread_usage += PerformanceMetrics::GetGSThreadUsage();
		s_perf_sum_gs_thread_time += PerformanceMetrics::GetGSThreadAverageTime();
		s_perf_sum_gpu_time += PerformanceMetrics::GetGPUAverageTime();
		s_perf_sum_gpu_usage += PerformanceMetrics::GetGPUUsage();
	}
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
	// Queued here and drained in PumpMessagesOnCPUThread(). Previously a hard
	// pxFailRel, which meant any PINE command that marshals to the CPU thread
	// (settings apply, savestates, frame advance) aborted the whole run.
	std::unique_lock lock(s_cpu_thread_tasks_mutex);
	s_cpu_thread_tasks.push_back(std::move(function));

	if (!block)
		return;

	// Wait for the drain to reach our task. The generation counter is bumped once
	// per drain, so waiting for the queue to empty is enough.
	s_cpu_thread_tasks_done.wait(lock, []() { return s_cpu_thread_tasks.empty(); });
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

bool Host::HasNativeAchievementNotifications() { return false; }
void Host::OnAchievementNotification(const char*, float, const char*, const char*, const char*) {}

void Host::OnAchievementsRefreshed()
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

int Host::LocaleSensitiveCompare(std::string_view lhs, std::string_view rhs)
{
	const int res = std::strncmp(lhs.data(), rhs.data(), std::min(lhs.size(), rhs.size()));
	if (res != 0)
		return res;
	return lhs.size() > rhs.size() ? 1 : (lhs.size() < rhs.size() ? -1 : 0);
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
	std::fprintf(stderr, "  -dump [rt|tex|z|f|a|i|tr|ds|fs|hw]: Enabling dumping of render target, texture, z buffer, frame, "
		"alphas, and info (context, vertices, list of transfers), transfers images, draw stats, frame stats, HW config, respectively, per draw. Generates lots of data.\n");
	std::fprintf(stderr, "  -dumprange N[,L,B]: Start dumping from draw N (base 0), stops after L draws, and only "
		"those draws that are multiples of B (intersection of -dumprange and -dumprangef used)."
		"Defaults to 0,-1,1 (all draws). Only used if -dump used.\n");
	std::fprintf(stderr, "  -dumprangef NF[,LF,BF]: Start dumping from frame NF (base 0), stops after LF frames, "
		"and only those frames that are multiples of BF (intersection of -dumprange and -dumprangef used).\n"
		"Defaults to 0,-1,1 (all frames). Only used if -dump is used.\n");
	std::fprintf(stderr, "  -loop <count>: Loops dump playback N times. Defaults to 1. 0 will loop infinitely.\n");
	std::fprintf(stderr, "  -renderer <renderer>: Sets the graphics renderer. Defaults to Auto.\n");
	std::fprintf(stderr, "  -swthreads <threads>: Sets the number of threads for the software renderer.\n");
	std::fprintf(stderr, "  -backthread <mode>: GS back-thread mode (0=off, 1=inline-records, 2=lockstep, 3=pipelined). Defaults to 0.\n");
	std::fprintf(stderr, "  -window: Forces a window to be displayed.\n");
	std::fprintf(stderr, "  -surfaceless: Disables showing a window.\n");
	std::fprintf(stderr, "  -logfile <filename>: Writes emu log to filename.\n");
	std::fprintf(stderr, "  -noshadercache: Disables the shader cache (useful for parallel runs).\n");
	std::fprintf(stderr, "  -perf: Enable frame timing performance stats.\n");
	std::fprintf(stderr, "  -drawlog <path.csv>: Record a per-draw ledger (PS2 register state + backend draw config).\n");
	std::fprintf(stderr, "  -stats-json <path>: Write per-frame and run-summary statistics as JSON. Combine with -perf "
						 "for frame/GPU timing.\n");
	std::fprintf(stderr, "  -set <Section/Key>=<value>: Override any setting, e.g. -set EmuCore/GS/AccurateBlendingUnit=3. "
						 "Repeatable.\n");
	std::fprintf(stderr, "  -vsync: Force vsync on (FIFO present mode). Workaround for libmali Wayland WSI which "
						 "advertises MAILBOX support but errors VK_ERROR_INITIALIZATION_FAILED on swapchain create.\n");
	std::fprintf(stderr, "  -no-fb-fetch: Disable Vulkan framebuffer fetch (VK_EXT_rasterization_order_attachment_access). "
						 "Use to A/B against drivers that mishandle subpass self-dependencies (e.g. libmali).\n");
	std::fprintf(stderr, "  -no-vs-expand: Disable vertex-shader point/line/sprite expansion (storage-buffer path). "
						 "Falls back to hardware/geometry expansion.\n");
	std::fprintf(stderr, "  -no-tex-barriers: Force OverrideTextureBarriers=0. Disables the texture-barrier render-pass pattern "
						 "and the framebuffer-fetch / depth-feedback paths that build on it.\n");
	std::fprintf(stderr, "  -accblend <0-5>: Force accurate blending unit (0=Minimum, 1=Basic, 2=Medium, 3=High, 4=Full, 5=Maximum). "
						 "Overrides the game/global default; use to exercise the SW-blend / fb-fetch (ROV) path headlessly.\n");
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
				if (str.find("hw") != std::string::npos)
					s_settings_interface.SetBoolValue("EmuCore/GS", "SaveHWConfig", true);
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
			else if (CHECK_ARG_PARAM("-backthread"))
			{
				const int mode = StringUtil::FromChars<int>(argv[++i]).value_or(-1);
				if (mode < 0 || mode > 3)
				{
					Console.Error("Invalid GS back-thread mode (0=off, 1=inline-records, 2=lockstep, 3=pipelined)");
					return false;
				}

				Console.WriteLn("Setting GS back-thread mode to %d.", mode);
				s_settings_interface.SetIntValue("EmuCore/GS", "GSBackThreadMode", mode);
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
			else if (CHECK_ARG("-perf"))
			{
				Console.WriteLn("Enable performance stats");
				s_perf_enable = true;
				continue;
			}
			else if (CHECK_ARG_PARAM("-drawlog"))
			{
				s_drawlog_path = argv[++i];
				s_settings_interface.SetBoolValue("EmuCore/GS", "DumpDrawLog", true);
				Console.WriteLn(fmt::format("Recording per-draw ledger to {}", s_drawlog_path));
				continue;
			}
			else if (CHECK_ARG_PARAM("-stats-json"))
			{
				s_stats_json_path = argv[++i];
				Console.WriteLn(fmt::format("Writing per-frame stats to {}", s_stats_json_path));
				continue;
			}
			else if (CHECK_ARG_PARAM("-set"))
			{
				// Generic settings override: -set <Section/Key>=<value>. Retires the need
				// for a bespoke flag per experiment and makes a sweep driver trivial.
				const std::string_view arg(argv[++i]);
				const std::string_view::size_type eq = arg.find('=');
				const std::string_view::size_type slash = arg.rfind('/', eq);
				if (eq == std::string_view::npos || slash == std::string_view::npos || slash == 0)
				{
					Console.Error(fmt::format("Malformed -set '{}', expected <Section/Key>=<value>", arg));
					return false;
				}

				const std::string section(arg.substr(0, slash));
				const std::string key(arg.substr(slash + 1, eq - slash - 1));
				const std::string value(arg.substr(eq + 1));
				if (key.empty())
				{
					Console.Error(fmt::format("Malformed -set '{}', empty key", arg));
					return false;
				}

				// Stored as a string; SettingsWrapper coerces on read, so this works for
				// bool/int/float keys alike.
				s_settings_interface.SetStringValue(section.c_str(), key.c_str(), value.c_str());
				Console.WriteLn(fmt::format("Override: [{}] {} = {}", section, key, value));
				continue;
			}
			else if (CHECK_ARG("-vsync"))
			{
				Console.WriteLn("Forcing vsync on (FIFO present mode). Use on libmali Wayland where MAILBOX errors VK_ERROR_INITIALIZATION_FAILED.");
				s_force_vsync = true;
				continue;
			}
			else if (CHECK_ARG("-no-fb-fetch"))
			{
				Console.WriteLn("Disabling framebuffer fetch (VK_EXT_rasterization_order_attachment_access)");
				s_settings_interface.SetBoolValue("EmuCore/GS", "DisableFramebufferFetch", true);
				continue;
			}
			else if (CHECK_ARG("-no-vs-expand"))
			{
				Console.WriteLn("Disabling vertex-shader point/line/sprite expansion");
				s_settings_interface.SetBoolValue("EmuCore/GS", "DisableVertexShaderExpand", true);
				continue;
			}
			else if (CHECK_ARG("-no-tex-barriers"))
			{
				Console.WriteLn("Forcing texture barriers off (OverrideTextureBarriers=0)");
				s_settings_interface.SetIntValue("EmuCore/GS", "OverrideTextureBarriers", 0);
				continue;
			}
			else if (CHECK_ARG_PARAM("-accblend"))
			{
				const std::optional<int> level = StringUtil::FromChars<int>(argv[++i]);
				if (!level.has_value() || level.value() < 0 || level.value() > 5)
				{
					Console.Error("Invalid -accblend level (expected 0=Minimum .. 5=Maximum)");
					return false;
				}
				Console.WriteLn(fmt::format("Forcing accurate blending unit = {}", level.value()));
				s_settings_interface.SetIntValue("EmuCore/GS", "accurate_blending_unit", level.value());
				continue;
			}
			else if (CHECK_ARG("-debugdevice"))
			{
				Console.WriteLn("Enable debug device");
				s_settings_interface.SetBoolValue("EmuCore/GS", "UseDebugDevice", true);
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
	s_settings_interface.SetBoolValue("EmuCore/GS", "FrameLimitEnable", s_force_vsync);
	s_settings_interface.SetIntValue("EmuCore/GS", "VsyncEnable", s_force_vsync);
	// -vsync needs DisableMailboxPresentation too: GetEffectiveVSyncMode() returns
	// Mailbox when VsyncEnable=true unless this is set.
	if (s_force_vsync)
		s_settings_interface.SetBoolValue("EmuCore/GS", "DisableMailboxPresentation", true);

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

static double Ratio(u64 num, u64 den)
{
	return den ? (100.0 * static_cast<double>(num) / static_cast<double>(den)) : 0.0;
}

// Nearest-rank percentile over an already-sorted vector.
static float Percentile(const std::vector<float>& sorted, double p)
{
	if (sorted.empty())
		return 0.0f;

	const size_t idx = std::min(sorted.size() - 1,
		static_cast<size_t>(std::ceil(p * static_cast<double>(sorted.size())) - 1.0));
	return sorted[idx];
}

// Writes the per-frame series plus a run summary. Emitted by hand rather than via a
// JSON library because gsrunner links none, and the schema is fixed.
static void WriteStatsJson(const std::string& path)
{
	auto fp = FileSystem::OpenManagedCFile(path.c_str(), "wb");
	if (!fp)
	{
		Console.Error(fmt::format("Failed to open '{}' for writing stats", path));
		return;
	}

	// Percentiles are computed over drawn frames only; idle frames are present-only
	// and would drag the distribution toward zero.
	std::vector<float> frame_times;
	frame_times.reserve(s_frame_samples.size());
	for (const FrameSample& s : s_frame_samples)
	{
		if (!s.idle && s.frame_ms > 0.0f)
			frame_times.push_back(s.frame_ms);
	}
	std::sort(frame_times.begin(), frame_times.end());

	u32 worst_frame = 0;
	float worst_ms = 0.0f;
	for (const FrameSample& s : s_frame_samples)
	{
		if (!s.idle && s.frame_ms > worst_ms)
		{
			worst_ms = s.frame_ms;
			worst_frame = s.frame;
		}
	}

	// GetDriverInfo() is multi-line on Vulkan, and neither string is JSON-safe as-is.
	const auto json_escape = [](const std::string& in) {
		std::string out;
		out.reserve(in.size());
		for (const char c : in)
		{
			if (c == '\n' || c == '\r' || c == '\t')
				out.push_back(' ');
			else if (c == '"' || c == '\\')
				out.push_back('\'');
			else
				out.push_back(c);
		}
		return out;
	};

	std::fprintf(fp.get(), "{\n  \"run\": {\n");
	std::fprintf(fp.get(), "    \"device_name\": \"%s\",\n    \"driver_info\": \"%s\",\n",
		json_escape(s_device_name).c_str(), json_escape(s_driver_info).c_str());
	std::fprintf(fp.get(), "    \"frames\": %u,\n    \"drawn_frames\": %u,\n", s_total_frames, s_total_drawn_frames);
	std::fprintf(fp.get(), "    \"prims\": %" PRIu64 ",\n    \"draws\": %" PRIu64 ",\n    \"draw_calls\": %" PRIu64 ",\n",
		s_total_prims, s_total_internal_draws, s_total_draws);
	std::fprintf(fp.get(), "    \"render_passes\": %" PRIu64 ",\n    \"barriers\": %" PRIu64 ",\n", s_total_render_passes, s_total_barriers);
	std::fprintf(fp.get(), "    \"copies\": %" PRIu64 ",\n    \"uploads\": %" PRIu64 ",\n    \"readbacks\": %" PRIu64 ",\n",
		s_total_copies, s_total_uploads, s_total_readbacks);
	std::fprintf(fp.get(), "    \"copies_rov\": %" PRIu64 ",\n    \"draw_calls_rov\": %" PRIu64 ",\n    \"barriers_rov\": %" PRIu64 ",\n",
		s_total_copies_rov, s_total_draws_rov, s_total_barriers_rov);
	std::fprintf(fp.get(), "    \"tc_source_hit\": %" PRIu64 ",\n    \"tc_source_miss\": %" PRIu64 ",\n",
		s_total_tc_source_hit, s_total_tc_source_miss);
	std::fprintf(fp.get(), "    \"tc_target_hit\": %" PRIu64 ",\n    \"tc_target_miss\": %" PRIu64 ",\n",
		s_total_tc_target_hit, s_total_tc_target_miss);
	std::fprintf(fp.get(), "    \"hash_cache_hit\": %" PRIu64 ",\n    \"hash_cache_miss\": %" PRIu64 ",\n",
		s_total_hash_cache_hit, s_total_hash_cache_miss);
	std::fprintf(fp.get(), "    \"frame_ms_p50\": %.3f,\n    \"frame_ms_p95\": %.3f,\n    \"frame_ms_p99\": %.3f,\n",
		Percentile(frame_times, 0.50), Percentile(frame_times, 0.95), Percentile(frame_times, 0.99));
	std::fprintf(fp.get(), "    \"frame_ms_worst\": %.3f,\n    \"frame_worst_index\": %u\n  },\n", worst_ms, worst_frame);

	std::fprintf(fp.get(), "  \"frames\": [\n");
	for (size_t i = 0; i < s_frame_samples.size(); i++)
	{
		const FrameSample& s = s_frame_samples[i];
		std::fprintf(fp.get(),
			"    {\"frame\":%u,\"idle\":%s,\"frame_ms\":%.3f,\"gpu_ms\":%.3f,"
			"\"prims\":%" PRIu64 ",\"draws\":%" PRIu64 ",\"draw_calls\":%" PRIu64 ","
			"\"render_passes\":%" PRIu64 ",\"barriers\":%" PRIu64 ",\"copies\":%" PRIu64 ","
			"\"uploads\":%" PRIu64 ",\"readbacks\":%" PRIu64 ","
			"\"copies_rov\":%" PRIu64 ",\"draw_calls_rov\":%" PRIu64 ",\"barriers_rov\":%" PRIu64 ","
			"\"tc_source_hit\":%" PRIu64 ",\"tc_source_miss\":%" PRIu64 ","
			"\"tc_target_hit\":%" PRIu64 ",\"tc_target_miss\":%" PRIu64 ","
			"\"hash_cache_hit\":%" PRIu64 ",\"hash_cache_miss\":%" PRIu64 "}%s\n",
			s.frame, s.idle ? "true" : "false", s.frame_ms, s.gpu_ms,
			s.prims, s.draws, s.draw_calls,
			s.render_passes, s.barriers, s.copies,
			s.uploads, s.readbacks,
			s.copies_rov, s.draw_calls_rov, s.barriers_rov,
			s.tc_source_hit, s.tc_source_miss,
			s.tc_target_hit, s.tc_target_miss,
			s.hash_cache_hit, s.hash_cache_miss,
			(i + 1 < s_frame_samples.size()) ? "," : "");
	}
	std::fprintf(fp.get(), "  ]\n}\n");

	Console.WriteLn(fmt::format("Wrote {} frame samples to {}", s_frame_samples.size(), path));
}

void GSRunner::DumpStats()
{
	std::atomic_thread_fence(std::memory_order_acquire);
	Console.WriteLn(fmt::format("======= HW STATISTICS FOR {} ({}) FRAMES ========", s_total_frames, s_total_drawn_frames));
	Console.WriteLn(fmt::format("@HWSTAT@ Prims: {} (avg {})", s_total_prims, static_cast<u64>(std::ceil(s_total_prims / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Draws: {} (avg {})", s_total_internal_draws, static_cast<u64>(std::ceil(s_total_internal_draws / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Draw Calls: {} (avg {})", s_total_draws, static_cast<u64>(std::ceil(s_total_draws / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Render Passes: {} (avg {})", s_total_render_passes, static_cast<u64>(std::ceil(s_total_render_passes / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Barriers: {} (avg {})", s_total_barriers, static_cast<u64>(std::ceil(s_total_barriers / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Copies: {} (avg {})", s_total_copies, static_cast<u64>(std::ceil(s_total_copies / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Uploads: {} (avg {})", s_total_uploads, static_cast<u64>(std::ceil(s_total_uploads / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Readbacks: {} (avg {})", s_total_readbacks, static_cast<u64>(std::ceil(s_total_readbacks / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Copies (ROV): {} (avg {})", s_total_copies_rov, static_cast<u64>(std::ceil(s_total_copies_rov / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Draws Calls (ROV): {} (avg {})", s_total_draws_rov, static_cast<u64>(std::ceil(s_total_draws_rov / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ Barriers (ROV): {} (avg {})", s_total_barriers_rov, static_cast<u64>(std::ceil(s_total_barriers_rov / static_cast<double>(s_total_drawn_frames)))));
	Console.WriteLn(fmt::format("@HWSTAT@ TC Source Hit/Miss: {}/{} ({:.1f}% hit)", s_total_tc_source_hit, s_total_tc_source_miss,
		Ratio(s_total_tc_source_hit, s_total_tc_source_hit + s_total_tc_source_miss)));
	Console.WriteLn(fmt::format("@HWSTAT@ TC Target Hit/Miss: {}/{} ({:.1f}% hit)", s_total_tc_target_hit, s_total_tc_target_miss,
		Ratio(s_total_tc_target_hit, s_total_tc_target_hit + s_total_tc_target_miss)));
	Console.WriteLn(fmt::format("@HWSTAT@ Hash Cache Hit/Miss: {}/{} ({:.1f}% hit)", s_total_hash_cache_hit, s_total_hash_cache_miss,
		Ratio(s_total_hash_cache_hit, s_total_hash_cache_hit + s_total_hash_cache_miss)));
	if (s_perf_enable)
	{
		Console.WriteLn(fmt::format("@HWSTAT@ Minimum Frame Time: {:.3f} ms ({:.3f} FPS)", PerformanceMetrics::GetMinimumFrameTime(), 1000.0f / PerformanceMetrics::GetMinimumFrameTime()));
		Console.WriteLn(fmt::format("@HWSTAT@ Average Frame Time: {:.3f} ms ({:.3f} FPS)", PerformanceMetrics::GetAverageFrameTime(), 1000.0f / PerformanceMetrics::GetAverageFrameTime()));
		Console.WriteLn(fmt::format("@HWSTAT@ Maximum Frame Time: {:.3f} ms ({:.3f} FPS)", PerformanceMetrics::GetMaximumFrameTime(), 1000.0f / PerformanceMetrics::GetMaximumFrameTime()));
		Console.WriteLn(fmt::format("@HWSTAT@ CPU Thread Usage: {:.3f} %", s_perf_sum_cpu_thread_usage / s_perf_updates));
		Console.WriteLn(fmt::format("@HWSTAT@ GS Thread Usage: {:.3f} %", s_perf_sum_gs_thread_usage / s_perf_updates));
		Console.WriteLn(fmt::format("@HWSTAT@ GPU Usage: {:.3f} %", s_perf_sum_gpu_usage / s_perf_updates));
		Console.WriteLn(fmt::format("@HWSTAT@ Average CPU Thread Time: {:.3f} ms", s_perf_sum_cpu_thread_time / s_perf_updates));
		Console.WriteLn(fmt::format("@HWSTAT@ Average GS Thread Time: {:.3f} ms", s_perf_sum_gs_thread_time / s_perf_updates));
		Console.WriteLn(fmt::format("@HWSTAT@ Average GPU Time: {:.3f} ms", s_perf_sum_gpu_time / s_perf_updates));
	}
	if (!s_stats_json_path.empty())
	{
		// Percentiles come from the measured per-frame series, which only exists when
		// -stats-json is active. Run-aggregate min/avg/max cannot locate a spike.
		std::vector<float> frame_times;
		frame_times.reserve(s_frame_samples.size());
		for (const FrameSample& s : s_frame_samples)
		{
			if (!s.idle && s.frame_ms > 0.0f)
				frame_times.push_back(s.frame_ms);
		}
		std::sort(frame_times.begin(), frame_times.end());

		Console.WriteLn(fmt::format("@HWSTAT@ Frame Time p50/p95/p99: {:.3f} / {:.3f} / {:.3f} ms",
			Percentile(frame_times, 0.50), Percentile(frame_times, 0.95), Percentile(frame_times, 0.99)));
	}
	for (const std::string& line : s_extended_stats_snapshot)
		Console.WriteLn(fmt::format("@HWSTAT@ {}", line));
	Console.WriteLn("============================================");

	if (!s_stats_json_path.empty())
		WriteStatsJson(s_stats_json_path);

	if (!s_drawlog_path.empty())
		GSDrawLog::WriteCSV(s_drawlog_path);
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
			// gsrunner is diagnostic-by-design; always collect extended stats so DumpStats has data.
			if (g_gs_device)
				g_gs_device->EnableExtendedStats(true);
			if (s_perf_enable)
			{
				VMManager::SetLimiterMode(LimiterModeType::Unlimited);
				g_gs_device->SetGPUTimingEnabled(true);
			}
			while (VMManager::GetState() == VMState::Running)
				VMManager::Execute();
			// Snapshot backend-specific stats before the GS device is destroyed.
			if (g_gs_device)
				s_extended_stats_snapshot = g_gs_device->GetExtendedStats();
			VMManager::Shutdown(false);
			GSRunner::DumpStats();
			ret->store(EXIT_SUCCESS);
		}
	}

	VMManager::Internal::CPUThreadShutdown();
	GSRunner::StopPlatformMessagePump();
}

// Set by the SIGINT/SIGTERM handlers (async-signal-safe: just an atomic store)
// and consumed on the CPU thread in PumpMessagesOnCPUThread(), which issues the
// actual VMManager::SetState(Stopping). Calling SetState() from signal context
// is not async-signal-safe — it can assert/log, take mutexes, and WaitGS/WaitVU.
static std::atomic<bool> s_signal_stop_requested{false};

int main(int argc, char* argv[])
{
	CrashHandler::Install();
	GSRunner::InitializeConsole();

	// Clean SIGINT/SIGTERM → VM stop, so DumpStats() still fires on ^C or SIGTERM during -loop 0.
	// Defer the actual stop to the CPU thread (see s_signal_stop_requested).
	std::signal(SIGINT, [](int) { s_signal_stop_requested.store(true); });
	std::signal(SIGTERM, [](int) { s_signal_stop_requested.store(true); });

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
	// Honor a pending ^C / SIGTERM here, on the CPU thread, where SetState() is
	// safe to call. exchange() makes the transition fire exactly once.
	if (s_signal_stop_requested.exchange(false))
		VMManager::SetState(VMState::Stopping);

	// Drain work posted by Host::RunOnCPUThread (PINE commands). Tasks run outside
	// the lock so one that posts more work cannot deadlock.
	for (;;)
	{
		std::function<void()> task;
		{
			std::unique_lock lock(s_cpu_thread_tasks_mutex);
			if (s_cpu_thread_tasks.empty())
			{
				s_cpu_thread_tasks_done.notify_all();
				break;
			}
			task = std::move(s_cpu_thread_tasks.front());
			s_cpu_thread_tasks.pop_front();
		}
		task();
	}

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

		ret.replace(pos, 2, count_str.view());
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

#elif defined(__linux__) && defined(WAYLAND_API)
// Wayland frontend for gsrunner. Used on handheld targets where the GPU's
// libmali variant is built for Wayland WSI (vkCreateWaylandSurfaceKHR) and
// VK_KHR_display is half-implemented (returns present_supported=false on the
// sole queue family). Runs as a normal Wayland client alongside the running
// compositor — no need to stop sway/weston.

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <cstring>
#include <poll.h>

static wl_display* s_display = nullptr;
static wl_registry* s_registry = nullptr;
static wl_compositor* s_compositor = nullptr;
static xdg_wm_base* s_wm_base = nullptr;
static wl_surface* s_surface = nullptr;
static xdg_surface* s_xdg_surface = nullptr;
static xdg_toplevel* s_xdg_toplevel = nullptr;
static WindowInfo s_wi;
static std::atomic<bool> s_shutdown_requested{false};
static bool s_initial_configure_received = false;

static void wl_wm_base_ping(void*, xdg_wm_base* wm_base, uint32_t serial)
{
	xdg_wm_base_pong(wm_base, serial);
}
static const xdg_wm_base_listener s_wm_base_listener = {wl_wm_base_ping};

static void wl_xdg_surface_configure(void*, xdg_surface* xs, uint32_t serial)
{
	xdg_surface_ack_configure(xs, serial);
	s_initial_configure_received = true;
}
static const xdg_surface_listener s_xdg_surface_listener = {wl_xdg_surface_configure};

static void wl_xdg_toplevel_configure(void*, xdg_toplevel*, int32_t width, int32_t height, wl_array*)
{
	if (width > 0 && height > 0)
	{
		s_wi.surface_width = static_cast<u32>(width);
		s_wi.surface_height = static_cast<u32>(height);
	}
}
static void wl_xdg_toplevel_close(void*, xdg_toplevel*)
{
	s_shutdown_requested.store(true);
}
// Stubs for the newer xdg_toplevel_listener slots. These struct members exist
// only when the wayland-scanner-generated header was built against a new enough
// xdg-shell (configure_bounds: protocol v4 / wayland-protocols >= 1.20;
// wm_capabilities: v5 / >= 1.26). Guard both the stubs and their initializer
// slots on the matching SINCE_VERSION macros so the aggregate initializer always
// matches the generated struct's member count — without the guards this is a hard
// "too many initializers" build break on older protocol headers.
#ifdef XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION
static void wl_xdg_toplevel_configure_bounds(void*, xdg_toplevel*, int32_t, int32_t) {}
#endif
#ifdef XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION
static void wl_xdg_toplevel_wm_capabilities(void*, xdg_toplevel*, wl_array*) {}
#endif
static const xdg_toplevel_listener s_xdg_toplevel_listener = {
	wl_xdg_toplevel_configure,
	wl_xdg_toplevel_close,
#ifdef XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION
	wl_xdg_toplevel_configure_bounds,
#endif
#ifdef XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION
	wl_xdg_toplevel_wm_capabilities,
#endif
};

static void wl_registry_global(void*, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
{
	if (std::strcmp(interface, wl_compositor_interface.name) == 0)
	{
		s_compositor = static_cast<wl_compositor*>(
			wl_registry_bind(registry, name, &wl_compositor_interface, std::min<uint32_t>(version, 4u)));
	}
	else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0)
	{
		s_wm_base = static_cast<xdg_wm_base*>(
			wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min<uint32_t>(version, 4u)));
		xdg_wm_base_add_listener(s_wm_base, &s_wm_base_listener, nullptr);
	}
}
static void wl_registry_global_remove(void*, wl_registry*, uint32_t) {}
static const wl_registry_listener s_registry_listener = {wl_registry_global, wl_registry_global_remove};

bool GSRunner::CreatePlatformWindow()
{
	pxAssertRel(!s_display && !s_surface, "Tried to create window when there already was one!");

	s_display = wl_display_connect(nullptr);
	if (!s_display)
	{
		Console.Error("wl_display_connect failed (check $WAYLAND_DISPLAY)");
		return false;
	}

	s_registry = wl_display_get_registry(s_display);
	wl_registry_add_listener(s_registry, &s_registry_listener, nullptr);
	wl_display_roundtrip(s_display);

	if (!s_compositor || !s_wm_base)
	{
		Console.Error("Wayland compositor missing wl_compositor or xdg_wm_base");
		DestroyPlatformWindow();
		return false;
	}

	s_surface = wl_compositor_create_surface(s_compositor);
	s_xdg_surface = xdg_wm_base_get_xdg_surface(s_wm_base, s_surface);
	xdg_surface_add_listener(s_xdg_surface, &s_xdg_surface_listener, nullptr);
	s_xdg_toplevel = xdg_surface_get_toplevel(s_xdg_surface);
	xdg_toplevel_add_listener(s_xdg_toplevel, &s_xdg_toplevel_listener, nullptr);
	xdg_toplevel_set_title(s_xdg_toplevel, "PCSX2 GS Runner");
	xdg_toplevel_set_app_id(s_xdg_toplevel, "net.pcsx2.gsrunner");

	wl_surface_commit(s_surface);
	// Round-trip until the compositor acks our initial configure, so the
	// Vulkan WSI sees a properly-sized surface from the first swapchain.
	while (!s_initial_configure_received)
	{
		if (wl_display_dispatch(s_display) < 0)
		{
			Console.Error("wl_display_dispatch failed during initial configure");
			DestroyPlatformWindow();
			return false;
		}
	}

	s_wi.type = WindowInfo::Type::Wayland;
	s_wi.display_connection = s_display;
	s_wi.window_handle = s_surface;
	if (s_wi.surface_width == 0)
		s_wi.surface_width = WINDOW_WIDTH;
	if (s_wi.surface_height == 0)
		s_wi.surface_height = WINDOW_HEIGHT;
	s_wi.surface_scale = 1.0f;
	return true;
}

void GSRunner::DestroyPlatformWindow()
{
	if (s_xdg_toplevel) { xdg_toplevel_destroy(s_xdg_toplevel); s_xdg_toplevel = nullptr; }
	if (s_xdg_surface)  { xdg_surface_destroy(s_xdg_surface);   s_xdg_surface = nullptr; }
	if (s_surface)      { wl_surface_destroy(s_surface);        s_surface = nullptr; }
	if (s_wm_base)      { xdg_wm_base_destroy(s_wm_base);       s_wm_base = nullptr; }
	if (s_compositor)   { wl_compositor_destroy(s_compositor);  s_compositor = nullptr; }
	if (s_registry)     { wl_registry_destroy(s_registry);      s_registry = nullptr; }
	if (s_display)      { wl_display_disconnect(s_display);     s_display = nullptr; }
}

std::optional<WindowInfo> GSRunner::GetPlatformWindowInfo()
{
	WindowInfo wi;
	if (s_display && s_surface)
		wi = s_wi;
	else
		wi.type = WindowInfo::Type::Surfaceless;
	return wi;
}

void GSRunner::PumpPlatformMessages(bool forever)
{
	if (!s_display)
		return;

	if (!forever)
	{
		wl_display_flush(s_display);
		wl_display_dispatch_pending(s_display);
		return;
	}

	const int fd = wl_display_get_fd(s_display);
	while (!s_shutdown_requested.load())
	{
		wl_display_flush(s_display);
		pollfd pfd = {fd, POLLIN, 0};
		const int p = poll(&pfd, 1, 16); // cap so we keep checking shutdown
		if (p > 0 && (pfd.revents & POLLIN))
		{
			if (wl_display_dispatch(s_display) < 0)
				break;
		}
		else
		{
			wl_display_dispatch_pending(s_display);
		}
	}
}

void GSRunner::StopPlatformMessagePump()
{
	s_shutdown_requested.store(true);
}

#elif defined(__linux__) && defined(X11_API)
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

#elif defined(__linux__)
// No X11/Wayland on this build (handheld kmsdrm target). Vulkan VK_KHR_display
// owns the screen; VulkanDirect is reported with the requested resolution and
// the GS device's display backend enumerates the monitor itself. Mirrors
// pcsx2-sdl/Main.cpp::BuildWindowInfo.
static std::atomic<bool> s_shutdown_requested{false};

bool GSRunner::CreatePlatformWindow()
{
	return true;
}

void GSRunner::DestroyPlatformWindow()
{
}

std::optional<WindowInfo> GSRunner::GetPlatformWindowInfo()
{
	WindowInfo wi;
	if (s_use_window.value_or(true))
	{
		wi.type = WindowInfo::Type::VulkanDirect;
		wi.surface_width = WINDOW_WIDTH;
		wi.surface_height = WINDOW_HEIGHT;
		wi.surface_scale = 1.0f;
	}
	else
	{
		wi.type = WindowInfo::Type::Surfaceless;
	}
	return wi;
}

void GSRunner::PumpPlatformMessages(bool forever)
{
	if (!forever)
		return;

	while (!s_shutdown_requested.load())
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
}

void GSRunner::StopPlatformMessagePump()
{
	s_shutdown_requested.store(true);
}

#endif // _WIN32 / __APPLE__ / __linux__
