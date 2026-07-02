// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// pcsx2-eerunner — headless standalone full-system runner for EE (R5900)
// JIT-vs-interpreter divergence triage.
//
// Modeled closely on pcsx2-gsrunner: it does a full VMManager init on a
// dedicated CPU thread and provides the complete Host:: implementation surface
// a standalone binary needs to link against libpcsx2. Unlike gsrunner it runs
// fully headless (Null GS renderer, no window, synchronous GS, no audio) so the
// run is deterministic frame-to-frame.
//
// --selfcheck loads a savestate, runs N frames under the EE interpreter twice
// from the same savestate, and proves the two per-frame fingerprint streams are
// byte-identical. That run-to-run determinism is the gate before any
// JIT-vs-interp diff (--localize / --repro) is meaningful.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <map>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef __linux__
#include <unistd.h>
#endif

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

#include "fmt/format.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/CrashHandler.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/MemorySettingsInterface.h"
#include "common/Path.h"
#include "common/Perf.h"
#include "common/ProgressCallback.h"
#include "common/StringUtil.h"

#include "pcsx2/PrecompiledHeader.h"

#include "pcsx2/Achievements.h"
#include "pcsx2/DebugTools/Debug.h"
#include "pcsx2/GS/GS.h"
#include "pcsx2/Host.h"
#include "pcsx2/INISettingsInterface.h"
#include "pcsx2/ImGui/FullscreenUI.h"
#include "pcsx2/ImGui/ImGuiFullscreen.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/Hw.h"
#include "pcsx2/Input/InputManager.h"
#include "pcsx2/Memory.h"
#include "pcsx2/R5900.h"
#include "pcsx2/SIO/Pad/Pad.h"
#include "pcsx2/VMManager.h"

#include "pcsx2/ee_divtrace.h"

#include "svnrev.h"

namespace EERunner
{
	static void InitializeConsole();
	static bool InitializeConfig();
	static void SettingsOverride();
	static bool ParseCommandLineArgs(int argc, char* argv[], VMBootParameters& params);
} // namespace EERunner

enum class RunMode
{
	None,
	SelfCheck,
	Localize,
	Repro,
	StepDiff,
	Vu0Diff,
	ContMem,
	SpeedhackDiff,
	LiveRun,
	Disasm,
};

static MemorySettingsInterface s_settings_interface;

// Parsed command-line state, read by the CPU thread.
static RunMode s_mode = RunMode::None;
static std::string s_iso_path;
static std::string s_savestate_path;
static uint32_t s_frames = 300;
static bool s_no_console = false;
static bool s_contmem_vu0_interp = false; // --vu0-interp modifier for --contmem
static GSRendererType s_renderer = GSRendererType::Null; // --renderer (Null default; vk for Intel/headless)
static std::string s_memdump_prefix; // --memdump <prefix>: write <prefix>.{interp,jit}.bin at the last frame
static bool s_perf_jitdump = false; // --perf-jitdump: emit Linux perf jitdump for `perf inject --jit` (profiling)

bool EERunner::InitializeConfig()
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

	// don't provide an ini path, or bother loading. settings are stored entirely in memory.
	MemorySettingsInterface& si = s_settings_interface;
	Host::Internal::SetBaseSettingsLayer(&si);

	VMManager::SetDefaultSettings(si, true, true, true, true, true);

	VMManager::Internal::LoadStartupSettings();
	return true;
}

void Host::CommitBaseSettingChanges()
{
	// nothing to save, settings are entirely in memory
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
	// Headless — never present anything.
	WindowInfo wi;
	wi.type = WindowInfo::Type::Surfaceless;
	return wi;
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
	// Headless — the Null renderer doesn't need a surface.
	WindowInfo wi;
	wi.type = WindowInfo::Type::Surfaceless;
	return wi;
}

void Host::ReleaseRenderWindow()
{
}

void Host::BeginPresentFrame()
{
	// Headless — nothing to present.
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

void Host::PumpMessagesOnCPUThread()
{
	// Headless — no platform message pump.
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

static void PrintCommandLineVersion()
{
	std::fprintf(stderr, "PCSX2 EE Runner Version %s\n", GIT_REV);
	std::fprintf(stderr, "https://pcsx2.net/\n");
	std::fprintf(stderr, "\n");
}

static void PrintCommandLineHelp(const char* progname)
{
	PrintCommandLineVersion();
	std::fprintf(stderr, "Usage: %s [--stepdiff|--contmem|--liverun|--vu0diff|--localize|--repro|--selfcheck] --savestate <file> --frames N [--iso <file>] [<iso>]\n", progname);
	std::fprintf(stderr, "\n");
	std::fprintf(stderr, "  --stepdiff:  Checkpoint-anchored interp-vs-JIT diff (THE primary mode). Per frame, checkpoints\n");
	std::fprintf(stderr, "               the VM and runs one frame interp-twice + JIT-once from the IDENTICAL state, so a\n");
	std::fprintf(stderr, "               clean interp control + JIT divergence = a real EE JIT bug; then zooms to the block.\n");
	std::fprintf(stderr, "  --contmem:   Continuous-trajectory memory diff. Runs interp CONTINUOUSLY (x2, control) + JIT\n");
	std::fprintf(stderr, "               CONTINUOUSLY, diffs per-frame memory hashes. Catches ACCUMULATION bugs --stepdiff\n");
	std::fprintf(stderr, "               can't (it re-anchors to golden each frame). Add --vu0-interp to force VU0=interp in\n");
	std::fprintf(stderr, "               all passes (isolate EE-FPU/integer from VU0/COP2). Run on x86 too for cross-arch.\n");
	std::fprintf(stderr, "  --speedhack-diff: Speedhack-misfire differential. EE-jit throughout; baseline = all transparency-class\n");
	std::fprintf(stderr, "               speedhacks OFF (run twice for the determinism floor), then sweeps each speedhack on its\n");
	std::fprintf(stderr, "               own (WaitLoop/IntcStat/vuFlagHack/vu1Instant/fastCDVD) + all-on. A speedhack that claims\n");
	std::fprintf(stderr, "               to be architecturally transparent must NOT change the EE-RAM trajectory before the\n");
	std::fprintf(stderr, "               baseline control floor breaks; the first such divergence (with ReportMemDiff at it) is a\n");
	std::fprintf(stderr, "               misfire (e.g. the Burnout-3 WaitLoop timeout-loop skip). Diffs EE main RAM + scratchpad.\n");
	std::fprintf(stderr, "  --liverun:   Reproduce the in-game HANG headlessly: single EE-jit pass with the LIVE subsystems\n");
	std::fprintf(stderr, "               the diff modes suppress (real GS so GIF is consumed, MTVU on). A 10s no-frame-\n");
	std::fprintf(stderr, "               progress watchdog samples the live EE PC to fingerprint the spin loop, then exits 42.\n");
	std::fprintf(stderr, "  --vu0diff:   Pin EE interp in both passes, toggle only VU0 micro engine; diff COP2 read-streams.\n");
	std::fprintf(stderr, "  --localize / --repro: aliases of --stepdiff (the old jittery frame-boundary funnel was removed).\n");
	std::fprintf(stderr, "  --selfcheck: Characterize run-to-run determinism (interp only). Expected to flag benign ~10-cycle\n");
	std::fprintf(stderr, "               pause-point sampling jitter; use it to understand the noise floor, not as a gate.\n");
	std::fprintf(stderr, "  --renderer <null|vk|ogl|sw>: GS renderer (default null). Use vk on Intel GPUs / boxes where\n");
	std::fprintf(stderr, "               the auto-check declines Vulkan and the surfaceless GL path fails to open GS.\n");
	std::fprintf(stderr, "  --savestate <file>: Savestate to load after Initialize (required).\n");
	std::fprintf(stderr, "  --frames N: Number of frames to run (default 300).\n");
	std::fprintf(stderr, "  --iso <file>: Game ISO/disc to mount (required so the savestate has its disc).\n");
	std::fprintf(stderr, "  --perf-jitdump: Emit a Linux perf jitdump (under EmuFolders::Cache) so `perf inject --jit`\n");
	std::fprintf(stderr, "               resolves EE_/VU0_/VU1_/IOP_/VIF_ JIT block symbols. Profiling only; with --liverun it\n");
	std::fprintf(stderr, "               also honors an explicit --renderer null. Requires a USE_PERF_JITDUMP build.\n");
	std::fprintf(stderr, "  -help: Displays this information and exits.\n");
	std::fprintf(stderr, "  -version: Displays version information and exits.\n");
	std::fprintf(stderr, "\n");
}

void EERunner::InitializeConsole()
{
	const char* var = std::getenv("PCSX2_NOCONSOLE");
	s_no_console = (var && StringUtil::FromChars<bool>(var).value_or(false));
	if (!s_no_console)
		Log::SetConsoleOutputLevel(LOGLEVEL_DEBUG);
}

bool EERunner::ParseCommandLineArgs(int argc, char* argv[], VMBootParameters& params)
{
	bool no_more_args = false;
	for (int i = 1; i < argc; i++)
	{
		if (!no_more_args)
		{
#define CHECK_ARG(str) !std::strcmp(argv[i], str)
#define CHECK_ARG_PARAM(str) (!std::strcmp(argv[i], str) && ((i + 1) < argc))

			if (CHECK_ARG("-help") || CHECK_ARG("--help"))
			{
				PrintCommandLineHelp(argv[0]);
				return false;
			}
			else if (CHECK_ARG("-version") || CHECK_ARG("--version"))
			{
				PrintCommandLineVersion();
				return false;
			}
			else if (CHECK_ARG("--selfcheck"))
			{
				s_mode = RunMode::SelfCheck;
				continue;
			}
			else if (CHECK_ARG("--localize"))
			{
				s_mode = RunMode::Localize;
				continue;
			}
			else if (CHECK_ARG("--repro"))
			{
				s_mode = RunMode::Repro;
				continue;
			}
			else if (CHECK_ARG("--stepdiff"))
			{
				s_mode = RunMode::StepDiff;
				continue;
			}
			else if (CHECK_ARG("--vu0diff"))
			{
				s_mode = RunMode::Vu0Diff;
				continue;
			}
			else if (CHECK_ARG("--contmem"))
			{
				s_mode = RunMode::ContMem;
				continue;
			}
			else if (CHECK_ARG("--speedhack-diff"))
			{
				s_mode = RunMode::SpeedhackDiff;
				continue;
			}
			else if (CHECK_ARG("--liverun"))
			{
				s_mode = RunMode::LiveRun;
				continue;
			}
			else if (CHECK_ARG("--perf-jitdump"))
			{
				// Emit a Linux perf jitdump so `perf inject --jit` can resolve EE_/VU1_/...
				// JIT block symbols. Profiling only; honors explicit --renderer null (the
				// liverun null->VK force below is skipped when this is set). Requires a
				// USE_PERF_JITDUMP build (no-op otherwise).
				s_perf_jitdump = true;
				continue;
			}
			else if (CHECK_ARG("--disasm"))
			{
				// --disasm: load the savestate, then disassemble EE code in
				// [EERUNNER_DIS_LO, EERUNNER_DIS_HI] with the correct R5900 disassembler
				// (generic MIPS disassemblers garble R5900 COP2/MMI opcodes). No run.
				s_mode = RunMode::Disasm;
				continue;
			}
			else if (CHECK_ARG("--vu0-interp"))
			{
				s_contmem_vu0_interp = true;
				continue;
			}
			else if (CHECK_ARG_PARAM("--memdump"))
			{
				s_memdump_prefix = StringUtil::StripWhitespace(argv[++i]);
				continue;
			}
			else if (CHECK_ARG_PARAM("--renderer"))
			{
				const std::string_view r = StringUtil::StripWhitespace(argv[++i]);
				if (r == "null" || r == "Null")            s_renderer = GSRendererType::Null;
				else if (r == "vk" || r == "vulkan")       s_renderer = GSRendererType::VK;
				else if (r == "ogl" || r == "gl" || r == "opengl") s_renderer = GSRendererType::OGL;
				else if (r == "sw" || r == "software")     s_renderer = GSRendererType::SW;
				else
				{
					Console.Error("--renderer expects one of: null, vk, ogl, sw");
					return false;
				}
				continue;
			}
			else if (CHECK_ARG_PARAM("--iso"))
			{
				s_iso_path = StringUtil::StripWhitespace(argv[++i]);
				continue;
			}
			else if (CHECK_ARG_PARAM("--savestate"))
			{
				s_savestate_path = StringUtil::StripWhitespace(argv[++i]);
				continue;
			}
			else if (CHECK_ARG_PARAM("--frames"))
			{
				const auto v = StringUtil::FromChars<uint32_t>(argv[++i]);
				if (!v.has_value() || v.value() == 0)
				{
					Console.Error("Invalid --frames value.");
					return false;
				}
				s_frames = v.value();
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

		// Positional argument = the ISO/disc.
		if (s_iso_path.empty())
			s_iso_path = argv[i];
		else
		{
			Console.Error("Unexpected extra positional argument: '%s'", argv[i]);
			return false;
		}
	}

	if (s_mode == RunMode::None)
	{
		Console.Error("No mode specified (use --stepdiff, --contmem, --speedhack-diff, --liverun, --vu0diff, --selfcheck, --localize, or --repro).");
		return false;
	}

	if (s_savestate_path.empty())
	{
		Console.Error("No savestate provided (use --savestate <file>).");
		return false;
	}
	if (!FileSystem::FileExists(s_savestate_path.c_str()))
	{
		Console.ErrorFmt("Savestate '{}' does not exist.", s_savestate_path);
		return false;
	}

	if (s_iso_path.empty())
	{
		Console.Error("No ISO provided (use --iso <file>); the savestate needs its disc mounted.");
		return false;
	}
	if (!FileSystem::FileExists(s_iso_path.c_str()))
	{
		Console.ErrorFmt("ISO '{}' does not exist.", s_iso_path);
		return false;
	}

	params.filename = s_iso_path;
	return true;
}

void EERunner::SettingsOverride()
{
	// Headless + deterministic: Null GS renderer, synchronous GS on the CPU
	// thread, no MTVU, no audio, no time-stretch. Keep wall-clock out of the
	// system clock so two runs from the same savestate produce identical
	// architectural state.

	// GS renderer. Default Null (no GS draws) — but Null is NOT self-contained: GS's
	// GetAPIForRenderer() has no Null case, so it falls through to GetPreferredRenderer()
	// for the HOST device API. On Asahi/AMD/NVIDIA that resolves to Vulkan (works with
	// the Surfaceless window we hand back); on an Intel box the auto-check declines Intel
	// Vulkan and picks OpenGL, which can't make a context for a Surfaceless window -> GS
	// fails to open. There, pass `--renderer vk` to force Vulkan (surfaceless-capable).
	// The renderer is irrelevant to EE/--contmem results (both passes on a box use the
	// same one, so GS cancels out in the jit-vs-interp diff).
	// LiveRun (the in-game hang repro) needs the live subsystems the deterministic
	// diff modes suppress: a REAL GS (so GIF/PATH3 is actually consumed, not dropped
	// like Null) and MTVU. Null GS is meaningless for it, so force VK if unset.
	const bool live = (s_mode == RunMode::LiveRun);
	GSRendererType rend = s_renderer;
	// Liverun normally needs a real GS (Null drops GIF/PATH3), so Null is forced to VK.
	// When profiling (--perf-jitdump), honor an explicit --renderer null so the
	// "scalar EE/IOP minus GS-feeding" diagnostic baseline is reachable. vk stays the
	// representative whole-system profile; null is the secondary diagnostic.
	if (live && rend == GSRendererType::Null && !s_perf_jitdump)
		rend = GSRendererType::VK;
	s_settings_interface.SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(rend));

	// Run GS synchronously on the CPU thread (no MTGS). Key is "SynchronousMTGS"
	// (Pcsx2Config::GSOptions::SynchronousMTGS; DEVBUILD-only — Devel defines it).
	// Diff modes keep it forced true. For LiveRun, default true but gate on
	// EERUNNER_SYNCMTGS: real async play uses SyncMTGS=false, so this lets us test
	// whether the MTVU+SyncMTGS combo (which can't occur in normal play) is itself
	// the deadlock trigger (harness artifact) vs a genuine hang reproducible async.
	bool sync_mtgs = true;
	if (live)
	{
		if (const char* e = std::getenv("EERUNNER_SYNCMTGS"))
			sync_mtgs = (e[0] != '0');
	}
	s_settings_interface.SetBoolValue("EmuCore/GS", "SynchronousMTGS", sync_mtgs);

	// MTVU off for the deterministic diff modes; ON for LiveRun by default. Gate on
	// EERUNNER_MTVU so the wedge can be retested MTVU-off without a rebuild: if the
	// hang reproduces identically MTVU-off, it is NOT an MTVU/MTGS thread deadlock
	// (rules out "EE thread blocked") and is a genuine EE-rec cycle/event-test bug.
	bool live_mtvu = live;
	if (live)
	{
		if (const char* e = std::getenv("EERUNNER_MTVU"))
			live_mtvu = (e[0] != '0');
	}
	s_settings_interface.SetBoolValue("EmuCore/Speedhacks", "vuThread", live_mtvu);

	// EE recompiler ON by default for LiveRun; gate on EERUNNER_EE=interp (or 0) to
	// run the clean EE-interpreter control pass — for jit-vs-interp comparison of the
	// SAME live hang (interp clean, jit hangs). Diff modes leave EnableEE untouched.
	if (live)
	{
		bool ee_jit = true;
		if (const char* e = std::getenv("EERUNNER_EE"))
			ee_jit = !(e[0] == 'i' || e[0] == 'I' || e[0] == '0');
		s_settings_interface.SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", ee_jit);
	}

	// EERUNNER_FPUFULL=1 forces CHECK_FPU_FULL (eeClampMode:3) so the EE-FPU JIT uses
	// the double-precision ADD/SUB/MUL/MADD paths that match the interp's fpuDouble()
	// math. In --liverun: decisive test of the "EE-FPU 1-ULP precision is the hang
	// cause" hypothesis (proven NEGATIVE — still hangs with FPUFULL on). In --stepdiff:
	// converges the benign mul/add 1-ULP so the lockstep differ walks past it to the
	// next (non-precision) divergence. Applied to ALL modes (set before VMManager
	// init). recDIV_S stays single even in FULL, so div divergences still surface.
	if (const char* e = std::getenv("EERUNNER_FPUFULL"))
		s_settings_interface.SetBoolValue("EmuCore/CPU/Recompiler", "fpuFullMode", e[0] != '0');

	// EERUNNER_NOFASTMEM=1 disables EE/VTLB fastmem (the 4 GB signal-backpatch fast
	// path). REQUIRED when running this binary under x86 emulation (FEX inside the
	// muvm 4K-page microVM, for single-machine cross-arch jit-vs-jit): PCSX2 fastmem
	// catches its OWN SIGSEGV to backpatch VTLB accesses, but FEX intercepts SIGSEGV
	// for its x86->arm64 translation, so the guest fault never reaches PCSX2's handler
	// -> unhandled SIGSEGV right after "Resetting fastmem mappings". Fastmem off routes
	// every load/store through the explicit VTLB call path: EE architectural state is
	// IDENTICAL (fastmem only changes speed), so memdump/contmem results are unaffected.
	// Native arm64 leaves it on (default). Applied to ALL modes (set before VM init).
	if (const char* e = std::getenv("EERUNNER_NOFASTMEM"))
		s_settings_interface.SetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", e[0] == '0');

	// EERUNNER_DIVCHOP=1 forces EE FPU DIV.S/SQRT.S to round toward zero (chop) by
	// setting FPUDiv.Roundmode = ChopZero (3), making FPUDivFPCR == FPUFPCR so the
	// arm64 recDIV_S FPCR round-mode swap-to-Nearest becomes a no-op. Diagnostic for
	// the Burnout-3 hang: the JIT div rounds Nearest (default FPUDivFPCR), interp
	// rounds chop (ambient FPUFPCR), and one game div (0.1*2560) lands on the
	// 255.9999847/256.0 boundary -> cvt.w.s gives 255 vs 256, a 1-off count that
	// corrupts the GIF scratchpad buffer bookkeeping. Roundmode: 0=Nearest 3=ChopZero.
	if (const char* e = std::getenv("EERUNNER_DIVCHOP"))
		s_settings_interface.SetIntValue("EmuCore/CPU", "FPUDiv.Roundmode", e[0] != '0' ? 3 : 0);

	// EERUNNER_HWDL=<0..4> forces EmuCore/GS HWDownloadMode (0=Enabled 1=EnabledForceFull
	// 2=NoReadbacks 3=Unsynchronized 4=Disabled). Diagnostic for GPU-readback pipeline
	// serialization (OutRun 2006 SD865: fps ≈ 1/(GS_cpu + GPU) because GSDownloadTexture::
	// Flush() drains the GPU whenever the game reads back RT data mid-frame).
	// Unsynchronized keeps all CPU-side copy work but skips the fence wait — isolates the
	// stall; NoReadbacks/Disabled also skip the copies. 1-4 are NOT render-accurate; A/B only.
	if (const char* e = std::getenv("EERUNNER_HWDL"))
		s_settings_interface.SetIntValue("EmuCore/GS", "HWDownloadMode", atoi(e));

	// EERUNNER_SPINCPU=1 sets EmuCore/GS HWSpinCPUForReadbacks: fence waits at GPU
	// readbacks spin instead of sleeping, cutting the scheduler wake latency that
	// dominates each synchronous readback round-trip once the pipeline overlap fix
	// (mid-frame command buffer kick) has the GPU already caught up.
	if (const char* e = std::getenv("EERUNNER_SPINCPU"))
		s_settings_interface.SetBoolValue("EmuCore/GS", "HWSpinCPUForReadbacks", e[0] != '0');

	// No audio output, and no time-stretch sync (wall-clock-driven). Keys live in
	// Pcsx2Config::SPU2Options::LoadSave under [SPU2/Output]: Backend / SyncMode.
	s_settings_interface.SetStringValue("SPU2/Output", "Backend", "Null");
	s_settings_interface.SetStringValue("SPU2/Output", "SyncMode", "Disabled");

	// Profiling: drive the perf jitdump enable through the normal config path so
	// ApplySettings/LoadSettings (which re-applies Perf::SetJitDumpEnabled from this
	// bool) keeps it on for the whole run instead of resetting it to the default.
	s_settings_interface.SetBoolValue("EmuCore/Profiler", "EnablePerfDump", s_perf_jitdump);

	// No frameskip.
	s_settings_interface.SetBoolValue("EmuCore/GS", "FrameSkipEnable", false);
	s_settings_interface.SetIntValue("EmuCore/GS", "FramesToDraw", 1);
	s_settings_interface.SetIntValue("EmuCore/GS", "FramesToSkip", 0);

	// Don't limit speed (also set on the VM via SetLimiterMode after init).
	s_settings_interface.SetBoolValue("EmuCore/GS", "FrameLimitEnable", false);
	s_settings_interface.SetIntValue("EmuCore/GS", "VsyncEnable", 0);

	// Disable input sources; we drive nothing.
	s_settings_interface.SetBoolValue("InputSources", "SDL", false);
	s_settings_interface.SetBoolValue("InputSources", "XInput", false);
	Pad::ClearPortBindings(s_settings_interface, 0);
	s_settings_interface.ClearSection("Hotkeys");

	// Logging.
	s_settings_interface.SetBoolValue("Logging", "EnableSystemConsole", !s_no_console);
	s_settings_interface.SetBoolValue("Logging", "EnableTimestamps", false);
	s_settings_interface.SetBoolValue("Logging", "EnableVerbose", true);

	// Remove memory cards, so we don't have sharing violations.
	for (u32 i = 0; i < 2; i++)
	{
		s_settings_interface.SetBoolValue("MemoryCards", fmt::format("Slot{}_Enable", i + 1).c_str(), false);
		s_settings_interface.SetStringValue("MemoryCards", fmt::format("Slot{}_Filename", i + 1).c_str(), "");
	}
}

// Snapshot live cpuRegs/fpuRegs into a FullSnap (frame-boundary capture; pc/cycle
// from the live registers).
static ee_divtrace::FullSnap CaptureFullSnap()
{
	ee_divtrace::FullSnap fs;
	std::memcpy(&fs.cpu, &cpuRegs, sizeof(cpuRegisters));
	std::memcpy(&fs.fpu, &fpuRegs, sizeof(fpuRegisters));
	fs.cycle = cpuRegs.cycle;
	fs.pc = cpuRegs.pc;
	fs._pad = 0;
	return fs;
}

// Per-frame selfcheck record: full register snapshot + the frame memory hash.
struct SelfCheckFrame
{
	ee_divtrace::FullSnap snap;
	uint64_t              memhash;
};

// Field-level diff of two FullSnaps (defined later); empty == identical regs.
static std::vector<std::string> DiffFullSnaps(const ee_divtrace::FullSnap& jit,
	const ee_divtrace::FullSnap& interp);

// Advance the loaded VM by `count` frames, discarding output (defined later).
static void AdvanceFrames(uint32_t count);

// Run s_frames frames in the current CPU mode, retaining a full register
// snapshot + memory hash per frame so selfcheck can field-diff the first
// divergent frame. (Snapshots are ~2 KB each; bounded by s_frames.)
static std::vector<SelfCheckFrame> RunAndSnapshot()
{
	std::vector<SelfCheckFrame> out;
	out.reserve(s_frames);
	for (uint32_t f = 0; f < s_frames && VMManager::GetState() != VMState::Shutdown; ++f)
	{
		VMManager::FrameAdvance(1);
		VMManager::Execute(); // returns after one frame (paused)
		out.push_back({CaptureFullSnap(), ee_divtrace::HashMemory()});
	}
	return out;
}

// Re-run from a fresh savestate load to the state captured at frame index
// `frame` (== AdvanceFrames(frame+1)), returning EE main RAM + scratchpad bytes
// for offline region diffing. Used to localize a store-path divergence.
static std::vector<u8> RunToFrameCaptureMem(uint32_t frame)
{
	Error error;
	if (!VMManager::LoadState(s_savestate_path.c_str(), &error))
	{
		Console.ErrorFmt("RunToFrameCaptureMem: load failed: {}", error.GetDescription());
		return {};
	}
	AdvanceFrames(frame + 1);
	std::vector<u8> out(Ps2MemSize::MainRam + Ps2MemSize::Scratch);
	std::memcpy(out.data(), eeMem->Main, Ps2MemSize::MainRam);
	std::memcpy(out.data() + Ps2MemSize::MainRam, eeMem->Scratch, Ps2MemSize::Scratch);
	return out;
}

// Count of differing 4 KB pages / bytes between two EE-memory captures.
struct MemDiffCount
{
	size_t pages = 0;
	size_t bytes = 0;
};

// Diff two EE-memory captures; print the first few differing 4 KB pages with a
// little content from each side so the divergent region/device can be reasoned
// about (main RAM byte offset == EE physical address for the first 32 MB).
// Returns the total differing page/byte counts (used by --speedhack-diff to size
// a divergence against the baseline determinism floor). verbose=false suppresses
// the per-page detail + summary line (for the many quiet trajectory samples).
static MemDiffCount ReportMemDiff(const std::vector<u8>& a, const std::vector<u8>& b, bool verbose = true)
{
	if (a.size() != b.size() || a.empty())
	{
		if (verbose)
			Console.ErrorFmt("  mem capture size mismatch ({} vs {})", a.size(), b.size());
		return {};
	}
	const size_t pageSize = 0x1000;
	size_t shown = 0, diffPages = 0, diffBytes = 0;
	for (size_t off = 0; off < a.size(); off += pageSize)
	{
		const size_t end = std::min(off + pageSize, a.size());
		size_t firstDiff = SIZE_MAX, pageDiffBytes = 0;
		for (size_t i = off; i < end; ++i)
			if (a[i] != b[i])
			{
				if (firstDiff == SIZE_MAX)
					firstDiff = i;
				++pageDiffBytes;
			}
		if (firstDiff == SIZE_MAX)
			continue;
		++diffPages;
		diffBytes += pageDiffBytes;
		if (verbose && shown < 8)
		{
			++shown;
			const char* region = (firstDiff < Ps2MemSize::MainRam) ? "Main" : "Scratch";
			const size_t addr = (firstDiff < Ps2MemSize::MainRam)
				? firstDiff : (firstDiff - Ps2MemSize::MainRam);
			Console.ErrorFmt("  {} @ {:#010x}: {} bytes differ in page; A=[{:02x} {:02x} {:02x} {:02x}] B=[{:02x} {:02x} {:02x} {:02x}]",
				region, addr, pageDiffBytes,
				a[firstDiff], a[firstDiff + 1 < a.size() ? firstDiff + 1 : firstDiff],
				a[firstDiff + 2 < a.size() ? firstDiff + 2 : firstDiff], a[firstDiff + 3 < a.size() ? firstDiff + 3 : firstDiff],
				b[firstDiff], b[firstDiff + 1 < b.size() ? firstDiff + 1 : firstDiff],
				b[firstDiff + 2 < b.size() ? firstDiff + 2 : firstDiff], b[firstDiff + 3 < b.size() ? firstDiff + 3 : firstDiff]);
		}
	}
	if (verbose)
		Console.ErrorFmt("  mem diff summary: {} pages, {} bytes differ (of {} captured)", diffPages, diffBytes, a.size());
	return {diffPages, diffBytes};
}

// Compare two per-frame snapshot streams; print the first few divergent frames
// with field detail. Returns true if identical. `la`/`lb` label the two streams.
static bool CompareStreams(const std::vector<SelfCheckFrame>& a,
	const std::vector<SelfCheckFrame>& b, const char* la, const char* lb)
{
	if (a.size() != b.size())
	{
		Console.ErrorFmt("  {} vs {}: frame count differs ({} vs {})", la, lb, a.size(), b.size());
		return false;
	}

	size_t first_regdiff = SIZE_MAX, first_memdiff = SIZE_MAX, first_pcdiff = SIZE_MAX;
	size_t diverged_frames = 0;
	for (size_t f = 0; f < a.size(); ++f)
	{
		const auto regdiffs = DiffFullSnaps(a[f].snap, b[f].snap);
		const bool memdiff = a[f].memhash != b[f].memhash;
		const bool pcdiff = a[f].snap.pc != b[f].snap.pc;
		if (regdiffs.empty() && !memdiff && !pcdiff)
			continue;

		++diverged_frames;
		if (!regdiffs.empty() && first_regdiff == SIZE_MAX)
			first_regdiff = f;
		if (memdiff && first_memdiff == SIZE_MAX)
			first_memdiff = f;
		if (pcdiff && first_pcdiff == SIZE_MAX)
			first_pcdiff = f;

		if (diverged_frames <= 4)
		{
			Console.ErrorFmt("  [{} vs {}] frame {}: pc {:#010x}/{:#010x} cycle {}/{} (dcyc={}) mem {}",
				la, lb, f, a[f].snap.pc, b[f].snap.pc, a[f].snap.cycle, b[f].snap.cycle,
				(int64_t)a[f].snap.cycle - (int64_t)b[f].snap.cycle,
				memdiff ? "DIFFERS" : "same");
			for (const auto& d : regdiffs)
				Console.ErrorFmt("    {}", d); // here "JIT="==la, "INTERP="==lb
		}
	}

	if (diverged_frames == 0)
	{
		Console.WriteLn(fmt::format("  {} vs {}: identical ({} frames)", la, lb, a.size()));
		return true;
	}

	Console.ErrorFmt("  {} vs {}: {}/{} frames diverged. first reg={} mem={} pc={}",
		la, lb, diverged_frames, a.size(),
		first_regdiff == SIZE_MAX ? -1 : (int64_t)first_regdiff,
		first_memdiff == SIZE_MAX ? -1 : (int64_t)first_memdiff,
		first_pcdiff == SIZE_MAX ? -1 : (int64_t)first_pcdiff);
	return false;
}

// --selfcheck: N interpreter passes from the same savestate must produce
// byte-identical per-frame snapshot streams. Running 3 passes (not 2) lets us
// distinguish a cold-cache/first-run artifact (B==C but A differs) from genuine
// per-run host nondeterminism (all three differ). Returns process exit code.
static int RunSelfCheck()
{
	Error error;
	const int kPasses = 3;
	std::vector<std::vector<SelfCheckFrame>> runs;

	for (int p = 0; p < kPasses; ++p)
	{
		if (!VMManager::LoadState(s_savestate_path.c_str(), &error))
		{
			Console.ErrorFmt("Failed to load savestate (pass {}): {}", p, error.GetDescription());
			return EXIT_FAILURE;
		}
		Console.WriteLn(fmt::format("eerunner: pass {} — running {} frames (interp)...", p, s_frames));
		runs.push_back(RunAndSnapshot());
	}

	static const char* const names[3] = {"A", "B", "C"};
	bool all_match = true;
	for (int p = 1; p < kPasses; ++p)
		all_match &= CompareStreams(runs[p - 1], runs[p], names[p - 1], names[p]);

	if (all_match)
	{
		Console.WriteLn(fmt::format("SELFCHECK PASS ({} passes × {} frames identical)", kPasses, s_frames));
		return EXIT_SUCCESS;
	}

	// Localize the first store-path divergence between the two WARM runs (B,C):
	// these have no cold-cache asymmetry, so a memory diff there is genuine
	// per-run nondeterminism. Re-run twice to that frame and report the region.
	size_t warm_memdiff = SIZE_MAX;
	for (size_t f = 0; f < runs[1].size() && f < runs[2].size(); ++f)
		if (runs[1][f].memhash != runs[2][f].memhash)
		{
			warm_memdiff = f;
			break;
		}
	if (warm_memdiff != SIZE_MAX)
	{
		Console.ErrorFmt("warm-run (B,C) memory first diverges at frame {} — localizing region:", warm_memdiff);
		const auto m1 = RunToFrameCaptureMem((uint32_t)warm_memdiff);
		const auto m2 = RunToFrameCaptureMem((uint32_t)warm_memdiff);
		ReportMemDiff(m1, m2);
	}

	Console.Error("SELFCHECK FAIL — see per-pair divergence above.");
	Console.Error("  (B vs C identical but A differs => cold-cache/first-run artifact; warm up before the golden.)");
	Console.Error("  (all pairs differ => per-run host nondeterminism; hunt the wall-clock/thread source.)");
	return EXIT_FAILURE;
}

// ===========================================================================
// --localize : the three-level divergence funnel.
//
//   Level 1 (per-FRAME, whole run): run interp golden, then JIT, recording one
//     (regfp, memhash) per frame. First differing frame F is the divergent
//     frame. If only memhash differs there, it's a store-only divergence (a
//     bad memory write that no register has read back yet) — reported at frame
//     granularity; finer memory localization is intentionally out of scope (it
//     is a slice-tracking tarpit).
//   Level 2 (per-OP, frame F only): re-run interp (dense, one Sample/op) and
//     JIT (sparse, one Sample per block entry) for frame F with the capture
//     sites enabled. Align by walking the JIT block-entry stream against the
//     dense interp op-stream — EE rec blocks are single basic blocks, so the
//     next interp op with pc == jit[k].pc is unambiguously that block entry.
//     First fp mismatch localizes the offending JIT block (the one that ran
//     between the last matching entry and the mismatch).
//   Level 3 (full register snapshot): re-run frame F with a 1-entry detail
//     window at the divergent index on each side, and diff the full
//     cpuRegisters/fpuRegisters to name the exact divergent field(s).
// ===========================================================================

// Switch the EE core between interpreter (jit=false) and recompiler (jit=true).
// Consumed by the next VMManager::Execute() (cpu-impl-changed -> cache clear).
static void SetEeMode(bool jit)
{
	s_settings_interface.SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", jit);
	VMManager::ApplySettings();
}

// --vu0diff axis: pin the EE INTERPRETER (deterministic, identical in both
// passes) and toggle only the VU0 micro engine. The two passes then run EE-interp
// in lockstep until mVU0-jit first hands the EE a different result than the VU0
// interpreter — isolating a VU0-jit-vs-interp value bug with no EE-rec or
// cross-arch noise.
static void SetVu0Mode(bool vu0_jit)
{
	s_settings_interface.SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", false);
	s_settings_interface.SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", vu0_jit);
	VMManager::ApplySettings();
}

// Advance the (already savestate-loaded) VM by `count` frames, discarding output.
static void AdvanceFrames(uint32_t count)
{
	for (uint32_t f = 0; f < count && VMManager::GetState() != VMState::Shutdown; ++f)
	{
		VMManager::FrameAdvance(1);
		VMManager::Execute();
	}
}

// Checkpoint-anchored fine pass: run exactly ONE frame from the CURRENT VM state
// (a freshly-loaded checkpoint) with the dense/sparse capture sites enabled. No
// AdvanceFrames — the caller positioned the VM, so there is no cross-pass drift.
static std::vector<ee_divtrace::Sample> RunFineFpFromHere()
{
	ee_divtrace::Reset();
	ee_divtrace::ReserveStream(16u * 1024u * 1024u);
	ee_divtrace::ConfigureFullWindow(0, 0); // fingerprints only
	ee_divtrace::g_enabled.store(true, std::memory_order_release);
	VMManager::FrameAdvance(1);
	VMManager::Execute();
	ee_divtrace::g_enabled.store(false, std::memory_order_release);
	return ee_divtrace::g_stream;
}

// Checkpoint-anchored detail pass: one frame from the current VM state with a
// 1-entry full-snapshot window at stream index `idx`.
static ee_divtrace::FullSnap RunFineSnapAtFromHere(uint32_t idx)
{
	ee_divtrace::Reset();
	ee_divtrace::ReserveStream(16u * 1024u * 1024u);
	ee_divtrace::ConfigureFullWindow(idx, 1);
	ee_divtrace::g_enabled.store(true, std::memory_order_release);
	VMManager::FrameAdvance(1);
	VMManager::Execute();
	ee_divtrace::g_enabled.store(false, std::memory_order_release);
	if (ee_divtrace::g_snaps.empty())
		return ee_divtrace::FullSnap{};
	return ee_divtrace::g_snaps.front();
}

struct AlignResult
{
	bool     found = false;
	bool     control_flow = false; // true: JIT reached a block interp never did
	uint32_t jit_idx = 0;          // divergent JIT block-entry index
	uint32_t interp_idx = 0;       // matched interp op index
	uint32_t pc = 0;               // block entry where divergence was observed
	uint32_t prev_pc = 0;          // entry of the JIT block that produced it
};

// Walk the sparse JIT block-entry stream against the dense interp op-stream.
//
// Semantics: a JIT block-entry sample is (pc=block start, fp=state ABOUT TO
// execute that block). An interp sample is recorded AFTER each op with
// pc=cpuRegs.pc (== the NEXT op) and fp=state after the op == state about to
// execute that next pc. So an interp sample (pc=P, fp=S) means "about to execute
// P with state S" — directly comparable to a JIT block-entry (P, S).
//
// Exception: JIT block-entry #0 is the FRAME START (state == the shared
// checkpoint), and the interp stream has no pre-first-op sample for it (the first
// interp sample with pc==entry0 is one loop iteration later). Entry #0 is equal
// by construction, so we anchor on it without comparing, and begin the real
// divergence search at entry #1.
// Walk from a given resume point (start_k JIT entry, start_ii interp position,
// start_prev_pc the entry that precedes start_k). Returns the first divergence
// at or after start_k, or {found=false} if the streams agree to the end.
static AlignResult AlignFrom(const std::vector<ee_divtrace::Sample>& interp,
	const std::vector<ee_divtrace::Sample>& jit, size_t start_k, size_t start_ii, uint32_t start_prev_pc)
{
	size_t ii = start_ii;
	uint32_t prev_pc = start_prev_pc;
	for (size_t k = start_k; k < jit.size(); ++k)
	{
		size_t scan = ii;
		while (scan < interp.size() && interp[scan].pc != jit[k].pc)
			++scan;
		if (scan >= interp.size())
			return {true, true, static_cast<uint32_t>(k), static_cast<uint32_t>(ii), jit[k].pc, prev_pc};
		if (interp[scan].fp != jit[k].fp)
			return {true, false, static_cast<uint32_t>(k), static_cast<uint32_t>(scan), jit[k].pc, prev_pc};
		ii = scan + 1;
		prev_pc = jit[k].pc;
	}
	return {}; // streams agree across every JIT block entry
}

static AlignResult Align(const std::vector<ee_divtrace::Sample>& interp,
	const std::vector<ee_divtrace::Sample>& jit)
{
	return AlignFrom(interp, jit, 1, 0, jit.empty() ? 0 : jit[0].pc);
}

// After a data divergence at (div_k, div_ii) that the caller has classified
// benign (a cycle-derived timer read taints one or more GPRs), walk forward to
// where the two streams RE-CONVERGE — the first JIT block-entry whose pc matches
// the interp op-stream AND whose full fingerprint agrees again (the tainted
// value has been overwritten / washed out). From there a strict walk is sound.
struct ResyncResult
{
	bool     reconverged = false;
	uint32_t k = 0;        // resume start_k for AlignFrom
	uint32_t ii = 0;       // resume start_ii
	uint32_t prev_pc = 0;  // resume start_prev_pc
	uint32_t blind = 0;    // JIT block-entries skipped while contaminated (a blind window)
	bool     ran_off = false; // pc-match failed mid-window (timing perturbed control flow)
};

static ResyncResult ResyncAfter(const std::vector<ee_divtrace::Sample>& interp,
	const std::vector<ee_divtrace::Sample>& jit, uint32_t div_k, uint32_t div_ii)
{
	size_t ii = static_cast<size_t>(div_ii) + 1;
	uint32_t blind = 0;
	for (size_t k = div_k + 1; k < jit.size(); ++k)
	{
		size_t scan = ii;
		while (scan < interp.size() && interp[scan].pc != jit[k].pc)
			++scan;
		if (scan >= interp.size())
		{
			ResyncResult r;
			r.ran_off = true;
			r.blind = blind;
			return r; // JIT reached a block interp never did within the window
		}
		if (interp[scan].fp == jit[k].fp)
		{
			// Full state matches again — resume strict align AFTER this entry.
			ResyncResult r;
			r.reconverged = true;
			r.k = static_cast<uint32_t>(k) + 1;
			r.ii = static_cast<uint32_t>(scan) + 1;
			r.prev_pc = jit[k].pc;
			r.blind = blind;
			return r;
		}
		ii = scan + 1;
		++blind;
	}
	ResyncResult r; // walked to end-of-frame still divergent (taint never washed out)
	r.blind = blind;
	return r;
}

// True if any non-GPR architectural field differs (HI/LO, CP0 except the
// dispatcher counters, FPR, ACC, sa) — i.e. the divergence is more than just
// tainted GPRs, so it can't be dismissed as a pure timer-read artifact.
static bool NonGprDiffers(const ee_divtrace::FullSnap& jit, const ee_divtrace::FullSnap& interp)
{
	if (jit.cpu.HI.UD[0] != interp.cpu.HI.UD[0] || jit.cpu.HI.UD[1] != interp.cpu.HI.UD[1] ||
		jit.cpu.LO.UD[0] != interp.cpu.LO.UD[0] || jit.cpu.LO.UD[1] != interp.cpu.LO.UD[1])
		return true;
	for (int i = 0; i < 32; ++i)
	{
		if (i == 1 || i == 9 || i == 11 || i == 13 || i == 14) // +Cause/EPC: interrupt-phase noise
			continue;
		if (jit.cpu.CP0.r[i] != interp.cpu.CP0.r[i])
			return true;
	}
	if (!ee_divtrace::g_fp_exclude)
	{
		for (int i = 0; i < 32; ++i)
			if (jit.fpu.fpr[i].UL != interp.fpu.fpr[i].UL)
				return true;
		if (jit.fpu.ACC.UL != interp.fpu.ACC.UL)
			return true;
	}
	if (jit.cpu.sa != interp.cpu.sa)
		return true;
	return false;
}

// Field-level diff of two full snapshots, mirroring DiffEe's ignored set
// (CP0 Random/Count/Compare, FPU control regs, cycle bookkeeping).
static std::vector<std::string> DiffFullSnaps(const ee_divtrace::FullSnap& jit,
	const ee_divtrace::FullSnap& interp)
{
	std::vector<std::string> out;
	static const char* const gpr_names[32] = {
		"zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
		"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
		"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
		"t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra"};
	auto d64 = [&](const std::string& n, u64 a, u64 b) {
		if (a != b)
			out.push_back(fmt::format("{}: JIT={:#018x} INTERP={:#018x}", n, a, b));
	};
	auto d32 = [&](const std::string& n, u32 a, u32 b) {
		if (a != b)
			out.push_back(fmt::format("{}: JIT={:#010x} INTERP={:#010x}", n, a, b));
	};
	for (int i = 0; i < 32; ++i)
	{
		d64(std::string(gpr_names[i]) + ".lo", jit.cpu.GPR.r[i].UD[0], interp.cpu.GPR.r[i].UD[0]);
		d64(std::string(gpr_names[i]) + ".hi", jit.cpu.GPR.r[i].UD[1], interp.cpu.GPR.r[i].UD[1]);
	}
	d64("hi.lo", jit.cpu.HI.UD[0], interp.cpu.HI.UD[0]);
	d64("hi.hi", jit.cpu.HI.UD[1], interp.cpu.HI.UD[1]);
	d64("lo.lo", jit.cpu.LO.UD[0], interp.cpu.LO.UD[0]);
	d64("lo.hi", jit.cpu.LO.UD[1], interp.cpu.LO.UD[1]);
	for (int i = 0; i < 32; ++i)
	{
		if (i == 1 || i == 9 || i == 11 || i == 13 || i == 14) // +Cause/EPC: interrupt-phase noise
			continue;
		d32(fmt::format("cp0[{}]", i), jit.cpu.CP0.r[i], interp.cpu.CP0.r[i]);
	}
	if (!ee_divtrace::g_fp_exclude)
	{
		for (int i = 0; i < 32; ++i)
			d32(fmt::format("fpr[{}]", i), jit.fpu.fpr[i].UL, interp.fpu.fpr[i].UL);
		d32("ACC", jit.fpu.ACC.UL, interp.fpu.ACC.UL);
	}
	d32("sa", jit.cpu.sa, interp.cpu.sa);
	return out;
}

// True for EE branch/jump primary opcodes (so we can stop disassembling a
// single basic block after its terminating branch + delay slot).
static bool IsEeBranchOpcode(u32 code)
{
	const u32 op = code >> 26;
	if (op == 0) // SPECIAL — JR (8) / JALR (9)
	{
		const u32 fn = code & 0x3f;
		return fn == 8 || fn == 9;
	}
	if (op == 1) // REGIMM — BLTZ/BGEZ/BLTZAL/...
		return true;
	if (op == 2 || op == 3) // J / JAL
		return true;
	if (op >= 4 && op <= 7) // BEQ/BNE/BLEZ/BGTZ
		return true;
	if (op >= 0x14 && op <= 0x17) // BEQL/BNEL/BLEZL/BGTZL
		return true;
	if (op == 0x10 || op == 0x11 || op == 0x12) // COP0/1/2 — may be BCxF/T
		return ((code >> 21) & 0x1f) == 0x08;
	return false;
}

// Disassemble a single EE basic block starting at `pc` to the console (stops a
// couple of instructions past the first branch, or at maxInsns). Read on the
// CPU thread where guest memory is live.
static void DisasmBlock(u32 pc, u32 maxInsns = 48)
{
	Console.WriteLn(fmt::format("    --- block disasm @ {:#010x} ---", pc));
	bool saw_branch = false;
	u32 after_branch = 0;
	for (u32 i = 0; i < maxInsns; ++i)
	{
		const u32 addr = pc + i * 4;
		const u32 code = memRead32(addr);
		std::string line;
		R5900::disR5900Fasm(line, code, addr, /*simplify=*/false);
		Console.WriteLn(fmt::format("    {:#010x}: {:08x}  {}", addr, code, line));
		if (saw_branch && ++after_branch >= 1) // include the delay slot, then stop
			break;
		if (IsEeBranchOpcode(code))
			saw_branch = true;
	}
}

// Recognize the cycle-derived-MMIO divergence class: a divergent GPR whose value
// originates — directly OR through arithmetic — from an EE timer COUNT register
// read inside the offending block. The EE timers (T0..T3 @ 0x1000_0000 / _0800 /
// _1000 / _1800, COUNT at +0) advance with cpuRegs.cycle, and the JIT vs interp
// differ by a few ticks at any mid-block read because they sync accumulated
// block-cycles at different granularity (per-block vs per-op). That is a TIMING
// artifact, not a codegen bug — a classic timing-taint trap — so we tag it rather
// than presenting it as a real bug.
//
// Two layers run over the block:
//   * const-prop (lui/ori/addiu chains) resolves each load's effective address,
//     so we can spot a load from a timer COUNT;
//   * taint-propagation tracks a per-GPR "cycle-derived" bit: set on a COUNT
//     load, propagated through pure-dataflow ALU ops (subu/addu/daddu/sll/and/…)
//     whose source is tainted, cleared on a non-timer load / lui / unmodeled
//     writer. This catches the common software-timer accumulator shape (read
//     COUNT, subtract prior COUNT for a delta, daddu it into a 64-bit virtual
//     clock) where the divergence flows into registers that were never the load
//     destination. Under-tainting is the SAFE failure mode (the divergence is
//     reported as real and a human looks); we only propagate through ops modeled
//     as pure dataflow, so we never mark a real-bug value as benign.
//
// Returns a human description for each divergent GPR proven cycle-derived (and
// pushes its index into classified_out), or empty.
static std::vector<std::string> ClassifyCycleDerivedLoads(u32 block_pc,
	const std::vector<int>& divergent_gprs, std::vector<int>* classified_out = nullptr,
	u32 maxInsns = 48)
{
	std::vector<std::string> out;
	if (divergent_gprs.empty())
		return out;

	u32 regval[32] = {0};
	bool known[32] = {false};
	bool tainted[32] = {false};
	std::string taint_src[32]; // origin description carried with the taint
	known[0] = true; // $zero

	auto setTaint = [&](u32 d, bool t, const std::string& src) {
		tainted[d] = t;
		taint_src[d] = t ? src : std::string();
	};

	bool saw_branch = false;
	for (u32 i = 0; i < maxInsns; ++i)
	{
		const u32 addr = block_pc + i * 4;
		const u32 code = memRead32(addr);
		const u32 op = code >> 26;
		const u32 rs = (code >> 21) & 0x1f;
		const u32 rt = (code >> 16) & 0x1f;
		const u32 rd = (code >> 11) & 0x1f;
		const u32 fn = code & 0x3f;
		const s32 simm = static_cast<s16>(code & 0xffff);
		const u32 uimm = code & 0xffff;

		auto isTimerCount = [](u32 a) {
			// COUNT register of any of the four EE timers (offset 0 within the
			// 0x800-strided bank). Flag the whole COUNT word.
			return (a == 0x10000000 || a == 0x10000800 || a == 0x10001000 || a == 0x10001800);
		};

		// Word/dword loads — a timer-COUNT load TAINTS rt; any other load gives rt
		// a fresh untainted value.
		if (op == 0x23 /*LW*/ || op == 0x27 /*LWU*/ || op == 0x37 /*LD*/ || op == 0x1e /*LQ*/)
		{
			if (known[rs] && isTimerCount(regval[rs] + static_cast<u32>(simm)))
			{
				const u32 ea = regval[rs] + static_cast<u32>(simm);
				const int timer = (ea - 0x10000000) / 0x800;
				setTaint(rt, true, fmt::format("EE Timer {} COUNT ({:#010x}) read at pc={:#010x}", timer, ea, addr));
			}
			else
			{
				setTaint(rt, false, {});
			}
			known[rt] = false; // loaded value is not a tracked const
		}
		else if (op == 0x10 /*COP0*/ && rs == 0x00 /*MFC0*/ && rd == 9 /*Count*/)
		{
			// mfc0 rt, $9 reads the COP0 cycle counter directly into rt. Like the
			// EE-timer COUNT MMIO loads, its value differs between the JIT and interp
			// by the per-block-vs-per-op cycle-sync granularity, so it taints rt. The
			// shape here is a Count-based timeout busy-wait (mfc0 Count; dsll32/dsra32
			// sign-extend; sltu vs a deadline) - benign cycle phase, not a codegen bug.
			setTaint(rt, true, fmt::format("COP0 Count (mfc0 $9) read at pc={:#010x}", addr));
			known[rt] = false;
		}
		else if (op == 0x0f /*LUI*/)
		{
			regval[rt] = uimm << 16; known[rt] = true;
			setTaint(rt, false, {}); // immediate — untainted
		}
		else if (op == 0x0d /*ORI*/)
		{
			if (known[rs]) { regval[rt] = regval[rs] | uimm; known[rt] = true; } else known[rt] = false;
			setTaint(rt, tainted[rs], taint_src[rs]);
		}
		else if (op == 0x09 /*ADDIU*/ || op == 0x19 /*DADDIU*/ || op == 0x08 /*ADDI*/ || op == 0x18 /*DADDI*/)
		{
			if (known[rs]) { regval[rt] = regval[rs] + static_cast<u32>(simm); known[rt] = true; } else known[rt] = false;
			setTaint(rt, tainted[rs], taint_src[rs]);
		}
		else if (op == 0x0a /*SLTI*/ || op == 0x0b /*SLTIU*/ || op == 0x0c /*ANDI*/ || op == 0x0e /*XORI*/)
		{
			known[rt] = false; // not const-tracked, but taint flows from rs
			setTaint(rt, tainted[rs], taint_src[rs]);
		}
		else if (op == 0x00 /*SPECIAL*/)
		{
			// R-type pure-dataflow ALU: dst rd, tainted iff any source operand is.
			// Shift-immediate forms (sll/srl/sra/dsll*/dsrl*/dsra*) take only rt.
			const bool isShiftImm =
				(fn == 0x00 || fn == 0x02 || fn == 0x03 ||
				 fn == 0x38 || fn == 0x3a || fn == 0x3b ||
				 fn == 0x3c || fn == 0x3e || fn == 0x3f);
			const bool isAlu =
				(fn >= 0x20 && fn <= 0x2f) || // add/addu/sub/subu/and/or/xor/nor/slt/sltu/dadd..dsubu
				isShiftImm ||
				(fn == 0x04 || fn == 0x06 || fn == 0x07) || // sllv/srlv/srav
				(fn == 0x14 || fn == 0x16 || fn == 0x17);   // dsllv/dsrlv/dsrav
			if (isAlu)
			{
				const bool srcT = isShiftImm ? tainted[rt] : (tainted[rs] || tainted[rt]);
				const std::string& src = tainted[rs] ? taint_src[rs] : taint_src[rt];
				setTaint(rd, srcT, src);
				// Keep the existing OR const-prop; other R-ops invalidate rd's const.
				if (fn == 0x25 /*OR*/ && known[rs] && known[rt]) { regval[rd] = regval[rs] | regval[rt]; known[rd] = true; }
				else known[rd] = false;
			}
			else
			{
				// Unmodeled SPECIAL GPR writer (mfhi/mflo/movz/…): clear rd taint
				// (under-taint = safe). jr/jalr/sync have rd=0, harmless.
				setTaint(rd, false, {});
			}
		}
		// (Stores, branches, COP ops write no GPR we model — taint left intact.
		// Any other GPR-writing op we don't recognize is an under-taint, which is
		// the safe direction: the divergence is reported as real, not skipped.)

		if (saw_branch)
			break;
		if (IsEeBranchOpcode(code))
			saw_branch = true;
	}

	// Classify each divergent GPR whose FINAL value is cycle-derived.
	for (int dr : divergent_gprs)
	{
		if (tainted[dr])
		{
			out.push_back(fmt::format(
				"${} is cycle-derived from {} — JIT/interp cycle-sync granularity makes a few-tick delta "
				"EXPECTED, almost certainly NOT a codegen bug.",
				dr, taint_src[dr]));
			if (classified_out)
				classified_out->push_back(dr);
		}
	}
	return out;
}

// Returns the backward-branch target if the basic block at `block_pc` terminates
// in a PC-relative branch that loops back to at/near its own entry (a self-loop),
// else 0. Only PC-relative families (REGIMM, BEQ/BNE/BLEZ/BGTZ, their likely
// variants, COPx BCxF/T) encode a reachable backward target; J/JAL/JR/JALR are
// absolute and not treated as self-loops here. When found, `*branch_addr_out`
// receives the address of the terminating branch (so the caller can bound the
// loop body's pc range, delay slot included).
static u32 BlockBackwardBranchTarget(u32 block_pc, u32* branch_addr_out = nullptr, u32 maxInsns = 64)
{
	for (u32 i = 0; i < maxInsns; ++i)
	{
		const u32 addr = block_pc + i * 4;
		const u32 code = memRead32(addr);
		if (!IsEeBranchOpcode(code))
			continue;
		const u32 op = code >> 26;
		const bool pcrel = (op == 1) || (op >= 4 && op <= 7) || (op >= 0x14 && op <= 0x17) ||
			((op == 0x10 || op == 0x11 || op == 0x12) && ((code >> 21) & 0x1f) == 0x08);
		if (!pcrel)
			return 0; // first terminating branch is absolute — not a self-loop
		const s32 off = static_cast<s16>(code & 0xffff);
		const u32 target = addr + 4 + (static_cast<u32>(off) << 2);
		// Self-loop: backward branch whose target lands in this block's head
		// region (at the entry or a few words before/within it).
		if (target <= addr && target + 8 >= block_pc)
		{
			if (branch_addr_out)
				*branch_addr_out = addr;
			return target;
		}
		return 0; // first terminating branch is forward / out-of-block
	}
	return 0;
}

// Resync past an ENTIRE self-loop run, not one iteration at a time. The JIT
// records one block-entry per loop iteration (all at the loop head pc), so we
// skip every consecutive JIT entry whose pc is in the loop body range
// [loop_lo, loop_hi] to land on the loop EXIT entry — the first JIT entry past
// the loop, carrying the loop's final architectural state. We then find that
// exact exit in the dense interp stream by (pc AND fingerprint) match.
//
// The fingerprint match (not a pc-range skip) is essential AND is the soundness
// check: a branch's delay slot is sampled by the interpreter with pc = branch+8,
// which aliases the loop's fall-through exit pc and recurs EVERY iteration — so a
// pc-only scan can't tell a mid-loop delay slot from the real exit. Only the true
// exit carries the loop's final state, so pc+fp pins it unambiguously. A pure
// sampling-phase artifact reaches an exit state identical to the JIT's; a real
// loop-body codegen bug changes the exit state or trip count, so the JIT exit
// fingerprint never appears in interp → reported as a real lead. Collapses what
// iteration-at-a-time resync would spend the whole benign-skip budget on into a
// single jump.
static ResyncResult ResyncPastSelfLoop(const std::vector<ee_divtrace::Sample>& interp,
	const std::vector<ee_divtrace::Sample>& jit, uint32_t div_k, uint32_t div_ii,
	u32 loop_lo, u32 loop_hi)
{
	auto inLoop = [&](u32 pc) { return pc >= loop_lo && pc <= loop_hi; };
	uint32_t blind = 0;
	size_t k = div_k;
	while (k < jit.size() && inLoop(jit[k].pc)) { ++k; ++blind; }
	if (k >= jit.size())
	{
		ResyncResult r; // loop never exited within the frame
		r.blind = blind;
		return r;
	}
	const u32 exit_pc = jit[k].pc;
	const u64 exit_fp = jit[k].fp;
	size_t ii = div_ii;
	while (ii < interp.size() && !(interp[ii].pc == exit_pc && interp[ii].fp == exit_fp))
		++ii;
	if (ii >= interp.size())
	{
		ResyncResult r; // JIT's loop-exit state never appears in interp — real lead
		r.blind = blind;
		return r;
	}
	ResyncResult r;
	r.reconverged = true;
	r.k = static_cast<uint32_t>(k) + 1;
	r.ii = static_cast<uint32_t>(ii) + 1;
	r.prev_pc = exit_pc;
	r.blind = blind;
	return r;
}

// Checkpoint-anchored zoom: given a checkpoint file holding the state at the
// START of the divergent frame, localize the offending JIT block and the exact
// divergent register field(s). interp (dense per-op) and JIT (sparse per-block)
// both run ONE frame from the SAME checkpoint, so the alignment is clean — no
// cross-pass drift. Prints the block disasm + field diff + fixture next-step.
// Returns true if the walk found something worth STOPPING for (a real codegen
// divergence, a control-flow split, or an inconclusive lead), false if every
// divergence this frame was a benign cycle-derived timer artifact the walk could
// resync past — in which case the caller should keep scanning later frames.
static bool ZoomFromCheckpoint(const std::string& ckpt)
{
	Error error;
	auto reload = [&](bool jit) -> bool {
		if (!VMManager::LoadState(ckpt.c_str(), &error))
		{
			Console.ErrorFmt("zoom: load checkpoint failed: {}", error.GetDescription());
			return false;
		}
		SetEeMode(jit);
		return true;
	};

	Console.WriteLn("STEPDIFF zoom — dense interp pass (one frame from checkpoint)...");
	if (!reload(false))
		return true;
	const auto interp_fine = RunFineFpFromHere();

	Console.WriteLn("STEPDIFF zoom — sparse JIT pass (one frame from checkpoint)...");
	if (!reload(true))
		return true;
	const auto jit_fine = RunFineFpFromHere();

	Console.WriteLn(fmt::format("STEPDIFF zoom — interp {} ops, JIT {} block entries.",
		interp_fine.size(), jit_fine.size()));

	// Iteratively localize. Find the first divergence and classify it: a data
	// divergence whose ONLY differing fields are GPRs loaded from EE timer COUNT
	// registers is the cycle-sync timing artifact — tag it, resync past where the
	// tainted value washes out, and keep hunting. Anything else is a real lead and
	// stops the walk. A cap bounds the per-skip re-run cost.
	size_t k = 1, ii = 0;
	uint32_t prev_pc = jit_fine.empty() ? 0 : jit_fine[0].pc;
	// Two skip budgets. Timer skips each take TWO full-frame re-runs to snapshot
	// the divergent registers, so they are capped tightly. Self-loop phase skips
	// are pure fingerprint-stream walks (no re-run), so they get a far larger cap
	// — a single frame can legitimately contain dozens of short phase-misaligned
	// string/scan loops, and stopping at 32 would falsely report the 33rd.
	int timer_skipped = 0;
	int selfloop_skipped = 0;
	int interrupt_skipped = 0;
	const int kTimerCap = 32;
	const int kSelfLoopCap = 4096;
	const int kInterruptCap = 4096; // stream-only resync, like self-loops

	while (true)
	{
		const int benign_skipped = timer_skipped + selfloop_skipped + interrupt_skipped;
		const AlignResult ar = AlignFrom(interp_fine, jit_fine, k, ii, prev_pc);
		if (!ar.found)
		{
			if (benign_skipped > 0)
			{
				Console.WriteLn(fmt::format(
					"STEPDIFF zoom: no CODEGEN divergence this frame — walked past {} benign divergence(s) "
					"({} cycle-derived timer, {} self-loop phase, {} interrupt-handler phase); the streams "
					"otherwise agree to end-of-frame.",
					benign_skipped, timer_skipped, selfloop_skipped, interrupt_skipped));
				return false; // benign — caller keeps scanning
			}
			Console.WriteLn("STEPDIFF zoom: registers diverged at frame granularity but per-op alignment found "
			                "no block-entry mismatch. The divergence likely lands on state written after the "
			                "last block boundary (event-test / COP path) — inspect the frame-boundary diff.");
			return true; // inconclusive lead — stop for a human look
		}

		if (ar.control_flow)
		{
			// control_flow means jit[k].pc was not found in interp's op-stream
			// AFTER the alignment point. Disambiguate a genuine wrong-target branch
			// from a benign spin-wait PHASE offset: if that pc appears ANYWHERE in
			// interp's stream this frame, the interpreter DID execute it (just at a
			// different iteration of a producer/consumer poll loop — e.g. the GIF
			// double-buffer wait at 0x1f24e0) and the JIT is merely further ahead at
			// the vsync cutoff. Benign — the end-of-frame MEMORY gate (caller) has
			// already proven this frame's persisted state differs only by timer/phase
			// noise, so a poll-loop iteration imbalance here carries no real signal.
			// If the pc appears NOWHERE in interp's stream, the JIT branched to a
			// block the interpreter never reached → a real control-flow codegen bug.
			bool interp_reached = false;
			for (const auto& s : interp_fine)
				if (s.pc == ar.pc) { interp_reached = true; break; }
			if (interp_reached)
			{
				Console.WriteLn(fmt::format(
					"STEPDIFF zoom: spin-wait PHASE offset entering pc={:#010x} (offending block {:#010x}) — the "
					"interpreter DID reach this pc elsewhere this frame; the JIT sits at a different poll-loop "
					"iteration at the vsync cutoff (benign, JIT ran off the end of interp's stream). Continuing the hunt.",
					ar.pc, ar.prev_pc));
				return false; // benign phase — caller keeps scanning later frames
			}
			Console.WriteLn(fmt::format(
				"STEPDIFF zoom: CONTROL-FLOW divergence — JIT dispatched to block pc={:#010x} the interpreter "
				"NEVER reached this frame. Offending JIT block: pc={:#010x} (terminating branch went to the wrong target).",
				ar.pc, ar.prev_pc));
			if (ar.prev_pc)
				DisasmBlock(ar.prev_pc);
			return true;
		}

		// Self-loop PHASE misalignment — checked FIRST and CHEAPLY (no snapshot).
		// The offending block is a tight backward self-loop; the JIT folds the
		// loop's first iteration into the preceding block, so its loop-head sample
		// runs one iteration ahead of the interpreter's dense per-op samples. Both
		// cores compute the SAME results — a sampling-phase artifact, not a codegen
		// bug. ResyncPastSelfLoop works on the fingerprint streams ALONE: it skips
		// the whole loop run and proves convergence at the loop EXIT (final state
		// identical). A real loop-body bug changes the exit state or trip count and
		// will NOT converge → it falls through to the snapshot + real report below.
		// Because this needs no re-run, it gets the large self-loop cap, so a frame
		// full of short scan loops doesn't exhaust the tight timer budget.
		{
			u32 loop_branch_addr = 0;
			const u32 loop_target = BlockBackwardBranchTarget(ar.pc, &loop_branch_addr);
			if (loop_target != 0 && selfloop_skipped < kSelfLoopCap)
			{
				const u32 loop_lo = loop_target;
				const u32 loop_hi = loop_branch_addr + 4; // include the branch's delay slot
				const ResyncResult rs = ResyncPastSelfLoop(
					interp_fine, jit_fine, ar.jit_idx, ar.interp_idx, loop_lo, loop_hi);
				if (rs.reconverged)
				{
					Console.WriteLn(fmt::format(
						"STEPDIFF zoom: skipping self-loop phase misalignment entering pc={:#010x} (loop {:#010x}..{:#010x}, "
						"branches back to {:#010x}). The JIT folds the loop's first iteration into the preceding block, so "
						"its loop-head sample runs one iteration ahead of interp; skipped the whole {}-entry loop run and "
						"converged at the exit. Continuing the hunt.",
						ar.pc, loop_lo, loop_hi, loop_target, rs.blind));
					++selfloop_skipped;
					k = rs.k;
					ii = rs.ii;
					prev_pc = rs.prev_pc;
					continue;
				}
				// Structural self-loop but the exit did NOT converge — not a mere
				// phase offset. Fall through to the snapshot + real-divergence report.
			}
		}

		// EXCEPTION/INTERRUPT-HANDLER PHASE — checked CHEAPLY (stream-only, no
		// snapshot) before the expensive data path. The divergence is entered inside
		// the EE kernel exception handler (kseg0, pc >= 0x8000_0000): the JIT and
		// interp took the same vblank/timer interrupt a few cycles apart in an idle
		// poll loop, so EPC differs by an instruction and that value propagates into
		// whatever scratch GPRs the handler touches (k0→t0→…). The handler dispatches
		// on Cause (not EPC), saves+restores the FULL user register context, and
		// ERETs to EPC — so on return to user code every user GPR is restored
		// IDENTICAL and only EPC (excluded from the fingerprint) differs. ResyncAfter
		// therefore reconverges the instant the handler returns. Gating on
		// reconvergence is the safety net: a genuine kernel-codegen bug changes the
		// post-return state and will NOT reconverge, falling through to the report.
		if ((ar.pc >= 0x80000000u || ar.prev_pc >= 0x80000000u) && interrupt_skipped < kInterruptCap)
		{
			const ResyncResult rs = ResyncAfter(interp_fine, jit_fine, ar.jit_idx, ar.interp_idx);
			if (rs.reconverged)
			{
				++interrupt_skipped;
				k = rs.k;
				ii = rs.ii;
				prev_pc = rs.prev_pc;
				continue;
			}
			// Did not reconverge within the frame — not a benign handler phase.
			// Fall through to the snapshot + real-divergence report.
		}

		// DATA divergence — snapshot both streams at the entry (per-stream indices:
		// interp dense vs JIT sparse) and classify.
		if (!reload(false))
			return true;
		const auto isnap = RunFineSnapAtFromHere(ar.interp_idx);
		if (!reload(true))
			return true;
		const auto jsnap = RunFineSnapAtFromHere(ar.jit_idx);

		std::vector<int> divergent_gprs;
		for (int i = 0; i < 32; ++i)
			if (jsnap.cpu.GPR.r[i].UD[0] != isnap.cpu.GPR.r[i].UD[0] ||
				jsnap.cpu.GPR.r[i].UD[1] != isnap.cpu.GPR.r[i].UD[1])
				divergent_gprs.push_back(i);

		std::vector<int> classified;
		const auto timer_notes = ClassifyCycleDerivedLoads(ar.prev_pc, divergent_gprs, &classified);

		const bool fully_benign =
			!divergent_gprs.empty() &&
			classified.size() == divergent_gprs.size() &&
			!NonGprDiffers(jsnap, isnap);

		if (fully_benign && timer_skipped < kTimerCap)
		{
			const ResyncResult rs = ResyncAfter(interp_fine, jit_fine, ar.jit_idx, ar.interp_idx);
			if (rs.ran_off)
			{
				Console.WriteLn(fmt::format(
					"STEPDIFF zoom: benign cycle-derived timer divergence entering pc={:#010x} ({}), but control "
					"flow then perturbed within the blind window ({} entries) — the timer value propagated into a "
					"branch. Can't cleanly look past it here; re-run from a later savestate.",
					ar.pc, timer_notes.front(), rs.blind));
				if (ar.prev_pc)
					DisasmBlock(ar.prev_pc);
				return true;
			}
			if (!rs.reconverged)
			{
				Console.WriteLn(fmt::format(
					"STEPDIFF zoom: benign cycle-derived timer divergence entering pc={:#010x} ({}); state never "
					"re-converged before end-of-frame ({} entries stayed tainted). The timing difference persisted "
					"— re-run from a later savestate to look past it.",
					ar.pc, timer_notes.front(), rs.blind));
				return true;
			}
			Console.WriteLn(fmt::format(
				"STEPDIFF zoom: skipping benign timer divergence entering pc={:#010x} (offending block {:#010x}; {}). "
				"Resynced after a {}-entry blind window; continuing the hunt.",
				ar.pc, ar.prev_pc, timer_notes.front(), rs.blind));
			++timer_skipped;
			k = rs.k;
			ii = rs.ii;
			prev_pc = rs.prev_pc;
			continue;
		}

		// REAL divergence (or benign-cap reached): full report + stop.
		Console.WriteLn(fmt::format(
			"STEPDIFF zoom: DATA divergence observed entering block pc={:#010x} (JIT block-entry #{}). "
			"Offending JIT block: entry pc={:#010x} — its body produced register state differing from interp.",
			ar.pc, ar.jit_idx, ar.prev_pc));
		if (ar.prev_pc)
			DisasmBlock(ar.prev_pc);

		const auto diffs = DiffFullSnaps(jsnap, isnap);
		if (diffs.empty())
		{
			Console.WriteLn("STEPDIFF zoom: (detail re-run did not reproduce the field diff at the entry — the "
			                "divergence may be mid-block / in memory; the block disasm above is the lead.)");
		}
		else
		{
			Console.WriteLn(fmt::format("STEPDIFF zoom: divergent register field(s) entering block pc={:#010x}:", ar.pc));
			for (const auto& d : diffs)
				Console.WriteLn(fmt::format("    {}", d));
			if (!timer_notes.empty())
			{
				Console.WriteLn("STEPDIFF zoom: SUSPECTED TIMING (some divergent GPRs are cycle-derived timer reads, "
				                "but NOT all divergent state is — see above):");
				for (const auto& n : timer_notes)
					Console.WriteLn(fmt::format("    {}", n));
			}
		}
		if (timer_skipped >= kTimerCap)
			Console.WriteLn(fmt::format(
				"STEPDIFF zoom: NOTE — hit the timer-skip cap ({}); this report may itself be another timer "
				"artifact. Re-run from a later savestate if so.", kTimerCap));
		else if (timer_skipped + selfloop_skipped > 0)
			Console.WriteLn(fmt::format(
				"STEPDIFF zoom: (walked past {} benign divergence(s) — {} timer, {} self-loop phase — before "
				"reaching this one.)", timer_skipped + selfloop_skipped, timer_skipped, selfloop_skipped));
		Console.WriteLn(fmt::format(
			"STEPDIFF zoom: offending block pc={:#010x} — capture a single-block EE fixture there to pin the opcode.",
			ar.prev_pc));
		return true;
	}
}


// ===========================================================================
// --stepdiff : checkpoint-anchored per-frame interp-vs-JIT comparison.
//
// The frame-boundary funnel (--localize) runs interp and JIT as two separate
// full passes and diffs them at frame boundaries. That conflates two things:
// real codegen divergence, and the ~10-cycle async sampling jitter at the
// frame-advance pause point (proven by --selfcheck: two warm interp runs
// already disagree at the boundary, yet committed memory re-captures identical).
// Accumulated over a pass, that jitter looks exactly like a JIT bug — a
// diagnostic tarpit.
//
// --stepdiff removes the accumulation: at each frame it CHECKPOINTS the VM
// (in-flight savestate), then runs ONE frame three times from that IDENTICAL
// state — interp twice (a determinism control) and JIT once. From a shared
// checkpoint:
//   * interp-vs-interp divergence  => async sampling jitter (this frame is noisy)
//   * interp-vs-interp clean + interp-vs-JIT divergence => a REAL EE JIT bug
// The golden timeline is advanced one interp frame between checkpoints, so the
// scan walks the whole run while every comparison starts from a clean state.
// ===========================================================================
// --contmem : continuous-trajectory memory diff. The checkpoint-anchored
// --stepdiff re-anchors to the golden interp state every frame, so it can NEVER
// reproduce an ACCUMULATION bug (drift that builds over ~1s of CONTINUOUS JIT —
// the Burnout 3 physics-explosion shape); it also fights LoadState pause-point
// jitter. This instead runs interp CONTINUOUSLY for the whole window
// (deterministic, per --selfcheck) twice as a control + JIT CONTINUOUSLY once,
// then diffs the per-frame memory-hash trajectories. The first frame whose
// hashes differ WITH a clean interp control is where continuous JIT first
// deviates; on it the EE-RAM region is localized via a byte diff. Cross-arch:
// run the SAME invocation on x86 — if x86 also diverges early the divergence is
// shared benign timing, if x86 stays clean it's an arm64-specific EE-JIT bug.
// `--vu0-interp` forces VU0=interp in every pass so the only cross-pass
// difference stays the EE engine (isolates EE-COP1-FPU/integer from VU0/COP2).
static int RunContinuousMemTrajectory()
{
	Error error;
	const bool force_vu0_interp = s_contmem_vu0_interp;
	auto runPass = [&](bool jit, std::vector<uint64_t>* cycles = nullptr) -> std::vector<uint64_t> {
		std::vector<uint64_t> hashes;
		if (!VMManager::LoadState(s_savestate_path.c_str(), &error))
		{
			Console.ErrorFmt("contmem: load failed: {}", error.GetDescription());
			return hashes;
		}
		SetEeMode(jit);
		if (force_vu0_interp)
		{
			s_settings_interface.SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", false);
			VMManager::ApplySettings();
		}
		hashes.reserve(s_frames);
		for (uint32_t f = 0; f < s_frames && VMManager::GetState() != VMState::Shutdown; ++f)
		{
			VMManager::FrameAdvance(1);
			VMManager::Execute();
			hashes.push_back(ee_divtrace::HashMemory());
			if (cycles)
				cycles->push_back(static_cast<uint64_t>(cpuRegs.cycle));
		}
		return hashes;
	};

	// Cycle-drift trajectory: EE cpuRegs.cycle at each frame boundary, JIT vs interp.
	// This is DETERMINISTIC (unlike the chaotic memory diff): if the JIT's per-block
	// emitted cycle cost matches the interpreter's, the two cycle counts stay locked;
	// a growing |jit.cycle - interp.cycle| is the EE-JIT cycle-accounting drift that
	// shifts every cycle-clocked subsystem (DMA/VIF/timers) out of phase. Cross-arch:
	// if arm64's per-frame drift >> x86's, arm64 EE block-cycle costs are the bug.
	std::vector<uint64_t> ic, jc;
	Console.WriteLn("CONTMEM: interp pass 1 (continuous)...");
	const auto i1 = runPass(false, &ic);
	Console.WriteLn("CONTMEM: interp pass 2 (continuous, determinism control)...");
	const auto i2 = runPass(false);
	Console.WriteLn("CONTMEM: JIT pass (continuous)...");
	const auto j = runPass(true, &jc);

	// Cycle-drift report (deterministic; compare arm64's vs x86's numbers cross-arch).
	{
		const size_t cn = std::min(ic.size(), jc.size());
		int64_t maxabs = 0;
		size_t maxf = 0;
		for (size_t f = 0; f < cn; ++f)
		{
			const int64_t d = (int64_t)jc[f] - (int64_t)ic[f];
			if (std::llabs(d) > std::llabs(maxabs)) { maxabs = d; maxf = f; }
		}
		Console.WriteLn("CONTMEM CYCLE-DRIFT (EE cpuRegs.cycle, jit - interp, per frame):");
		for (size_t f = 0; f < cn; ++f)
		{
			const int64_t d = (int64_t)jc[f] - (int64_t)ic[f];
			if (f < 12 || f + 4 >= cn || std::llabs(d) == std::llabs(maxabs))
				Console.WriteLn(fmt::format("  frame {:3}: interp.cycle={} jit.cycle={} drift={:+d}", f, ic[f], jc[f], d));
		}
		Console.WriteLn(fmt::format("CONTMEM CYCLE-DRIFT SUMMARY: max |drift| = {:+d} EE cycles at frame {} (of {} frames).",
			maxabs, maxf, cn));
	}

	const size_t n = std::min({i1.size(), i2.size(), j.size()});
	int first_ctrl = -1, first_real = -1;
	for (size_t f = 0; f < n; ++f)
	{
		const bool ctrl_div = i1[f] != i2[f];
		const bool jit_div = i1[f] != j[f];
		if (ctrl_div && first_ctrl < 0)
			first_ctrl = (int)f;
		if (jit_div && !ctrl_div && first_real < 0)
			first_real = (int)f;
		Console.WriteLn(fmt::format("CONTMEM frame {:3}: interp1={:#018x} interp2={:#018x} jit={:#018x}  ctrl={} jit-vs-interp={}",
			f, i1[f], i2[f], j[f], ctrl_div ? "DIFF" : "ok", jit_div ? "DIFF" : "ok"));
	}
	Console.WriteLn(fmt::format(
		"CONTMEM SUMMARY: {} frames; interp determinism first breaks at frame {} ; "
		"continuous JIT-vs-interp memory first diverges (with clean interp control) at frame {}.",
		n, first_ctrl, first_real));
	// Capture EE main RAM + scratchpad after running `frame+1` continuous frames
	// in the given mode (honors --vu0-interp), for an interp-vs-JIT byte-region diff.
	auto capMem = [&](bool jit, uint32_t frame) -> std::vector<u8> {
		std::vector<u8> out;
		if (!VMManager::LoadState(s_savestate_path.c_str(), &error)) return out;
		SetEeMode(jit);
		if (force_vu0_interp)
		{
			s_settings_interface.SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", false);
			VMManager::ApplySettings();
		}
		AdvanceFrames(frame + 1);
		out.resize(Ps2MemSize::MainRam + Ps2MemSize::Scratch);
		std::memcpy(out.data(), eeMem->Main, Ps2MemSize::MainRam);
		std::memcpy(out.data() + Ps2MemSize::MainRam, eeMem->Scratch, Ps2MemSize::Scratch);
		return out;
	};

	if (first_real >= 0)
	{
		Console.WriteLn(fmt::format("CONTMEM: EE-RAM region @ FIRST clean-control JIT divergence (frame {}) — divergence onset:", first_real));
		ReportMemDiff(capMem(false, (uint32_t)first_real), capMem(true, (uint32_t)first_real));
	}
	// Magnitude trajectory: diff again at the LAST frame. A BOUNDED drift stays a
	// similar page/byte count to the onset frame (→ the divergence is benign timing
	// phase, the explosion is elsewhere); an EXPLOSION balloons to thousands of
	// pages of NaN/garbage physics (→ this divergence IS the corruption). Interp's
	// own late-frame nondeterminism adds only a small page-count floor, far below an
	// explosion's footprint.
	if (n >= 2)
	{
		const uint32_t lastf = (uint32_t)n - 1;
		Console.WriteLn(fmt::format("CONTMEM: interp-vs-interp CONTROL region @ LAST frame ({}) — nondeterminism noise floor:", lastf));
		ReportMemDiff(capMem(false, lastf), capMem(false, lastf));
		Console.WriteLn(fmt::format("CONTMEM: EE-RAM region @ LAST frame ({}) — JIT-vs-interp; subtract the control floor above:", lastf));
		ReportMemDiff(capMem(false, lastf), capMem(true, lastf));
	}

	// --memdump: write the raw EE main RAM + scratchpad of the interp and jit passes
	// at the LAST frame, for a cross-machine JIT-vs-JIT diff. Interp is bit-identical
	// cross-arch (deterministic IEEE) and EE cycles are locked, so diffing arm64's
	// .jit.bin vs x86's .jit.bin isolates the arm64-specific COMPUTATIONAL EE-JIT
	// divergence directly. Use a SMALL --frames (e.g. 2-3) so chaotic amplification
	// hasn't spread the seed yet. The .interp.bin pair should be byte-identical
	// cross-arch (a sanity check on cross-arch determinism).
	if (!s_memdump_prefix.empty() && n >= 1)
	{
		const uint32_t lastf = (uint32_t)n - 1;
		auto dump = [&](bool jit, const char* tag) {
			const std::vector<u8> m = capMem(jit, lastf);
			const std::string path = fmt::format("{}.{}.bin", s_memdump_prefix, tag);
			std::ofstream f(path, std::ios::binary | std::ios::trunc);
			if (!f) { Console.ErrorFmt("CONTMEM: failed to open {}", path); return; }
			f.write(reinterpret_cast<const char*>(m.data()), static_cast<std::streamsize>(m.size()));
			Console.WriteLn(fmt::format("CONTMEM: wrote {} ({} bytes, frame {}).", path, m.size(), lastf));
		};
		dump(false, "interp");
		dump(true, "jit");
	}
	return EXIT_SUCCESS;
}

// --speedhack-diff : speedhack-misfire differential.
//
// Speedhacks are silent, runtime-gated divergences: each one CLAIMS to skip only
// dead work (a spin loop, an intc_stat poll, a redundant flag update) and leave
// the EE/VU architectural result unchanged. When that claim is wrong — as the EE
// recompiler's WaitLoop timeout-loop skip was for Burnout 3's DMA-display-list
// build loop — the game corrupts state and hangs, with EE=interp clean and
// EE=jit broken. No existing state-diff tool catches this because they compare
// jit-vs-interp at ONE speedhack config; the bug lived in a config axis no test
// varied.
//
// This mode varies that axis. It runs the savestate forward in EE-jit throughout
// (the speedhacks are jit-gated), establishes a baseline with every
// transparency-class speedhack OFF (run twice, for the run-to-run determinism
// floor), then sweeps each speedhack on its own and all-on. A transparent
// speedhack must NOT change the per-frame EE-RAM hash before the baseline control
// floor breaks; the FIRST such clean-control divergence is a misfire, and
// ReportMemDiff at that frame localizes the corrupted region (e.g. the GIF DMA
// chain). No core instrumentation — reuses HashMemory/ReportMemDiff. This is the
// system-level positive-side coverage the unit tests can't give (a fired skip
// diverges from naive interp BY DESIGN; only equality-against-an-honest-baseline
// validates it).
//
// Excluded by construction: vuThread/MTVU (nondeterministic — thread races defeat
// an equality diff) and EECycleRate/EECycleSkip (deliberately lossy cycle scaling
// — they change results on purpose). Diffs EE main RAM + scratchpad (where
// DMA-chain / display-list corruption lands), the same surface as --contmem.
static int RunSpeedhackDiff()
{
	Error error;

	// The swept speedhacks. equalityClass = CLAIMS bit-exact equivalence (skips
	// provably-dead work); a sustained divergence from those is off-spec. The
	// others are deliberate timing approximations that legitimately change a few
	// async-phase bytes — only a runaway EXPLOSION flags them. Bit i = knob i ON.
	struct Knob { const char* name; const char* key; bool equalityClass; };
	static const Knob kKnobs[] = {
		{"WaitLoop", "WaitLoop", true},      // EE timeout/idle-loop skip (recSkipTimeoutLoop) — the Burnout-3 culprit
		{"IntcStat", "IntcStat", true},      // fast-forward through intc_stat poll waits (skip-to-event)
		{"vuFlagHack", "vuFlagHack", true},  // microVU status/mac flag elision (redundant-write skip)
		{"vu1Instant", "vu1Instant", false}, // run VU1 to completion instantly — lossy timing approximation
		{"fastCDVD", "fastCDVD", false},      // shorten CDVD access latency — lossy timing approximation
	};
	constexpr size_t kNumKnobs = std::size(kKnobs);
	const uint32_t kAllOn = (1u << kNumKnobs) - 1u;

	auto maskLabel = [&](uint32_t mask) -> std::string {
		if (mask == 0)
			return "baseline(all-off)";
		if (mask == kAllOn)
			return "all-on";
		std::string s;
		for (size_t k = 0; k < kNumKnobs; ++k)
			if ((mask >> k) & 1u)
				s += (s.empty() ? "" : "+") + std::string(kKnobs[k].name);
		return s;
	};

	auto applyConfig = [&](uint32_t mask) {
		for (size_t k = 0; k < kNumKnobs; ++k)
			s_settings_interface.SetBoolValue("EmuCore/Speedhacks", kKnobs[k].key, ((mask >> k) & 1u) != 0u);
		// Force the excluded knobs to their neutral / off state so they never
		// contaminate the differential.
		s_settings_interface.SetBoolValue("EmuCore/Speedhacks", "vuThread", false);
		s_settings_interface.SetIntValue("EmuCore/Speedhacks", "EECycleRate", 0);
		s_settings_interface.SetIntValue("EmuCore/Speedhacks", "EECycleSkip", 0);
		// Speedhacks bite the EE recompiler; run it (not interp) in every pass.
		s_settings_interface.SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", true);
		VMManager::ApplySettings();
	};

	auto runPass = [&](uint32_t mask, std::vector<uint64_t>* cyc = nullptr) -> std::vector<uint64_t> {
		std::vector<uint64_t> hashes;
		if (!VMManager::LoadState(s_savestate_path.c_str(), &error))
		{
			Console.ErrorFmt("speedhack-diff: load failed: {}", error.GetDescription());
			return hashes;
		}
		applyConfig(mask);
		hashes.reserve(s_frames);
		for (uint32_t f = 0; f < s_frames && VMManager::GetState() != VMState::Shutdown; ++f)
		{
			VMManager::FrameAdvance(1);
			VMManager::Execute();
			hashes.push_back(ee_divtrace::HashMemory());
			if (cyc)
				cyc->push_back(static_cast<uint64_t>(cpuRegs.cycle));
		}
		return hashes;
	};

	auto capMem = [&](uint32_t mask, uint32_t frame) -> std::vector<u8> {
		std::vector<u8> out;
		if (!VMManager::LoadState(s_savestate_path.c_str(), &error))
			return out;
		applyConfig(mask);
		AdvanceFrames(frame + 1);
		out.resize(Ps2MemSize::MainRam + Ps2MemSize::Scratch);
		std::memcpy(out.data(), eeMem->Main, Ps2MemSize::MainRam);
		std::memcpy(out.data() + Ps2MemSize::MainRam, eeMem->Scratch, Ps2MemSize::Scratch);
		return out;
	};

	// Baseline determinism sanity: run all-off twice and report how long the
	// per-frame hash stays bit-identical. If this breaks immediately the
	// savestate/renderer setup is nondeterministic and the verdicts below are
	// unreliable (reduce --frames or check the setup).
	Console.WriteLn("SPEEDHACK-DIFF: baseline determinism pass A (all speedhacks OFF)...");
	const auto baseA = runPass(0);
	Console.WriteLn("SPEEDHACK-DIFF: baseline determinism pass B (all speedhacks OFF)...");
	const auto baseB = runPass(0);
	if (baseA.empty() || baseB.empty())
		return EXIT_FAILURE;
	{
		const size_t bn = std::min(baseA.size(), baseB.size());
		int floorFrame = -1;
		for (size_t f = 0; f < bn; ++f)
			if (baseA[f] != baseB[f]) { floorFrame = static_cast<int>(f); break; }
		Console.WriteLn(fmt::format(
			"SPEEDHACK-DIFF: baseline (all-off) stays run-to-run bit-identical through frame {} (of {}).",
			floorFrame < 0 ? static_cast<int>(bn) : floorFrame, bn));
	}

	// The verdict is NOT "any byte differs". Two distinct speedhack classes exist:
	//  - equality-class (WaitLoop/IntcStat/vuFlagHack): skip provably-dead work
	//    (a spin loop to its next event, a redundant flag) and CLAIM bit-exact
	//    equivalence. Any sustained divergence from all-off is suspect.
	//  - lossy-timing (vu1Instant/fastCDVD): deliberate approximations that DO
	//    change cycle timing (and therefore a few async-phase bytes) on purpose.
	//    Only a runaway EXPLOSION matters for these.
	// The clean cross-class discriminator for the corruption/hang family (the
	// Burnout-3 WaitLoop misfire) is GROWTH: corruption balloons across the
	// window; a legit timing offset stays bounded. So we sample the EE-RAM
	// divergence magnitude at several frames and compare its growth + absolute
	// size against the determinism floor (two independent all-off runs). Same
	// bounded-vs-explosion logic --contmem uses, swept over the speedhack axis.
	std::vector<uint32_t> samples;
	{
		const uint32_t N = s_frames;
		auto add = [&](uint32_t f) {
			if (f < N && std::find(samples.begin(), samples.end(), f) == samples.end())
				samples.push_back(f);
		};
		add(N >= 8 ? N / 4 : 0);
		add(N / 2);
		add((3u * N) / 4u);
		if (N >= 1)
			add(N - 1);
		std::sort(samples.begin(), samples.end());
	}

	// Reference (all-off run #1) and an independent all-off run #2 for the floor,
	// captured once per sample frame and reused across every config.
	std::vector<std::vector<u8>> baseCaps, floorCaps;
	std::vector<MemDiffCount> floorTraj;
	for (uint32_t sf : samples)
	{
		baseCaps.push_back(capMem(0, sf));
		floorCaps.push_back(capMem(0, sf));
		floorTraj.push_back(ReportMemDiff(baseCaps.back(), floorCaps.back(), /*verbose=*/false));
	}
	{
		std::string s;
		for (size_t i = 0; i < samples.size(); ++i)
			s += fmt::format("{}f={}p ", samples[i], floorTraj[i].pages);
		Console.WriteLn("SPEEDHACK-DIFF: determinism floor trajectory (all-off vs all-off): " + s);
	}
	const MemDiffCount floorLast = floorTraj.empty() ? MemDiffCount{} : floorTraj.back();

	// Is every speedhack in `mask` an equality-class one (and mask non-empty)?
	auto pureEqualityClass = [&](uint32_t mask) -> bool {
		if (mask == 0)
			return false;
		for (size_t k = 0; k < kNumKnobs; ++k)
			if (((mask >> k) & 1u) && !kKnobs[k].equalityClass)
				return false;
		return true;
	};

	struct Verdict { uint32_t mask; std::vector<MemDiffCount> traj; bool explosion; };
	std::vector<Verdict> verdicts;

	auto evalConfig = [&](uint32_t mask) {
		std::vector<MemDiffCount> traj;
		for (size_t i = 0; i < samples.size(); ++i)
			traj.push_back(ReportMemDiff(baseCaps[i], capMem(mask, samples[i]), /*verbose=*/false));
		const MemDiffCount first = traj.front();
		const MemDiffCount last = traj.back();

		// EXPLOSION = the corruption/hang signature: a page spread well past the
		// determinism floor that GREW across the window (a misfire's corruption
		// onsets mid-run and runs away, e.g. buggy WaitLoop 14p->10p->10p->206p).
		// This is class-agnostic and the ONLY automated verdict — every swept
		// speedhack perturbs SOME bounded state by design (a correct WaitLoop/IntcStat
		// leaves the abandoned spin counter + a few async-phase bytes;
		// vu1Instant/fastCDVD shift timing), so "diverges at all" is not a bug; only
		// runaway GROWTH is.
		//
		// Absolute magnitude alone cannot discriminate: chaos-amplifying 3D titles
		// (e.g. GTA San Andreas) amplify a 110-byte sub-ULP seed (the unavoidable
		// arm64 fmadd/-ffp-contract JIT-vs-interp difference) into a 576p intrinsic
		// chaos floor with NO speedhack at all (see --contmem); a 303p WaitLoop
		// divergence there is *below* that floor, same scattered-1-ULP-FP character,
		// and is NOT a misfire. Magnitude (absolute OR relative-to-floor) can't
		// separate that from a real misfire (Burnout-3 buggy 206p was only 1.2x its
		// 166p fixed floor); the late-onset GROWTH shape can:
		// GTA's flat-high 282->303 fails `grew`, Burnout's 14->206 passes it.
		// Empirically validated: Burnout-3 buggy WaitLoop flagged, fixed not;
		// GTA SA / R&C UYA WaitLoop+IntcStat correctly NOT flagged.
		const size_t floorPad = floorLast.pages * 4 + 16;
		const bool grew = last.pages >= first.pages * 4;
		const bool explosion = last.pages >= 32 && last.pages > floorPad && grew;

		std::string trajStr;
		for (size_t i = 0; i < samples.size(); ++i)
			trajStr += fmt::format("{}f={}p/{}b ", samples[i], traj[i].pages, traj[i].bytes);
		// Only an EQUALITY-class config (WaitLoop/IntcStat/vuFlagHack — claims
		// bit-exactness) exploding is an actionable misfire. Lossy-class configs
		// (vu1Instant/fastCDVD/all-on) are DESIGNED to diverge — fastCDVD shortens
		// disc latency, so on a slot that's actively streaming it legitimately loads
		// MBs of assets earlier (seen as a multi-hundred-page explosion). That is
		// expected, not a bug, so it must not read as "investigate as misfire".
		const bool isEquality = (mask != 0) && pureEqualityClass(mask);
		const char* cls = (mask == 0) ? "-" : (isEquality ? "equality" : "lossy/mixed");
		const char* verdictStr = !explosion ? "bounded (expected)"
			: (isEquality ? "EXPLOSION (likely misfire/corruption)"
			              : "EXPLOSION (expected — lossy timing class, not a misfire)");
		Console.WriteLn(fmt::format("SPEEDHACK-DIFF [{}] class={}: {}", maskLabel(mask), cls, verdictStr));
		Console.WriteLn("  trajectory (baseline-all-off vs config): " + trajStr
			+ fmt::format("(floor_last={}p)", floorLast.pages));
		if (explosion)
		{
			Console.WriteLn(fmt::format("  EE-RAM region @ frame {} — baseline(all-off) vs [{}]:",
				samples.back(), maskLabel(mask)));
			ReportMemDiff(baseCaps.back(), capMem(mask, samples.back()), /*verbose=*/true);
		}
		verdicts.push_back({mask, std::move(traj), explosion});
	};

	// Sweep each speedhack on its own, then all-on (interaction check).
	for (size_t k = 0; k < kNumKnobs; ++k)
		evalConfig(1u << k);
	evalConfig(kAllOn);

	// Summary.
	Console.WriteLn(fmt::format("SPEEDHACK-DIFF SUMMARY (floor_last = {} pages / {} bytes @ frame {}):",
		floorLast.pages, floorLast.bytes, samples.empty() ? 0u : samples.back()));
	int misfires = 0, lossyExplosions = 0;
	for (const auto& v : verdicts)
	{
		const bool isEquality = (v.mask != 0) && pureEqualityClass(v.mask);
		const bool misfire = v.explosion && isEquality;
		misfires += misfire ? 1 : 0;
		lossyExplosions += (v.explosion && !isEquality) ? 1 : 0;
		const MemDiffCount last = v.traj.empty() ? MemDiffCount{} : v.traj.back();
		const char* tag = !v.explosion ? "bounded"
			: (misfire ? "EXPLOSION <-- likely misfire" : "EXPLOSION (expected lossy)");
		Console.WriteLn(fmt::format("  {:<28} last {:>6} pages / {:>8} bytes   {}",
			maskLabel(v.mask), last.pages, last.bytes, tag));
	}
	Console.WriteLn(fmt::format(
		"SPEEDHACK-DIFF: {} equality-class MISFIRE(s) — the actionable signal — plus {} expected lossy-class explosion(s).",
		misfires, lossyExplosions));
	return EXIT_SUCCESS;
}

static int RunStepDiff()
{
	Error error;
	const std::string ckpt = Path::Combine(EmuFolders::Cache, "eerunner_stepdiff.p2s");

	auto saveCkpt = [&]() -> bool {
		bool ok = true;
		VMManager::SaveState(ckpt.c_str(), /*zip_on_thread=*/false, /*backup_old_state=*/false,
			[&](const std::string& e) { ok = false; Console.ErrorFmt("stepdiff: save failed: {}", e); });
		VMManager::WaitForSaveStateFlush();
		return ok;
	};
	auto loadCkpt = [&]() -> bool {
		if (!VMManager::LoadState(ckpt.c_str(), &error))
		{
			Console.ErrorFmt("stepdiff: load checkpoint failed: {}", error.GetDescription());
			return false;
		}
		return true;
	};
	// Run one frame in the given mode from the just-loaded checkpoint, returning
	// the end-of-frame full snapshot + memory hash.
	auto runOne = [&](bool jit, ee_divtrace::FullSnap& snap, uint64_t& memhash) -> bool {
		if (!loadCkpt())
			return false;
		SetEeMode(jit);
		AdvanceFrames(1);
		snap = CaptureFullSnap();
		memhash = ee_divtrace::HashMemory();
		return true;
	};

	if (!VMManager::LoadState(s_savestate_path.c_str(), &error))
	{
		Console.ErrorFmt("stepdiff: initial load failed: {}", error.GetDescription());
		return EXIT_FAILURE;
	}
	SetEeMode(false); // golden timeline is interp

	int benign_frames = 0;
	for (uint32_t f = 0; f < s_frames && VMManager::GetState() != VMState::Shutdown; ++f)
	{
		if (!saveCkpt())
			return EXIT_FAILURE;

		ee_divtrace::FullSnap i1, i2, j;
		uint64_t i1m = 0, i2m = 0, jm = 0;
		if (!runOne(false, i1, i1m) || !runOne(false, i2, i2m) || !runOne(true, j, jm))
			return EXIT_FAILURE;

		const auto ii = DiffFullSnaps(i1, i2);
		const bool ii_clean = ii.empty() && i1m == i2m && i1.pc == i2.pc;
		const auto ij = DiffFullSnaps(j, i1); // labels: JIT=j, INTERP=i1
		const bool ij_diverged = !ij.empty() || (i1m != jm) || (i1.pc != j.pc);

		if (ij_diverged)
		{
			Console.WriteLn(fmt::format(
				"STEPDIFF frame {}: interp-vs-JIT DIVERGES (pc interp={:#010x} jit={:#010x}, mem {}); "
				"interp-vs-interp control = {}",
				f, i1.pc, j.pc, (i1m != jm) ? "DIFFERS" : "same",
				ii_clean ? "CLEAN" : "ALSO DIVERGES (async jitter)"));
			for (const auto& d : ij)
				Console.WriteLn(fmt::format("    {}", d));
			if (ii_clean && i1m == jm)
			{
				// End-of-frame MEMORY identical — only live registers / pc differ.
				// The EE is parked in a producer/consumer spin-wait (the GIF double-
				// buffer poll at 0x1f24e0) and the two cores are sampled at different
				// iteration counts at the vsync boundary. No architectural state
				// PERSISTED differently, so this CANNOT be the divergence we hunt: a
				// real EE-JIT computational bug stores its wrong value, which would
				// show as a memory difference. Skip the zoom (it would only re-find
				// the benign spin phase) and keep scanning.
				Console.WriteLn(fmt::format(
					"  => frame {}: end-of-frame MEMORY identical (only live regs/pc differ) — benign spin-wait "
					"phase at the vsync boundary; continuing scan.", f));
				++benign_frames;
			}
			else if (ii_clean)
			{
				// Candidate real divergence (clean interp control, MEMORY differs).
				// Zoom in INLINE to classify: the checkpoint still holds this frame's
				// start (the zoom only LoadState-reads it). If the zoom resolves it
				// all to benign cycle-derived timer reads / spin phase, keep scanning
				// later frames; otherwise it's a real lead and we stop here.
				Console.Error("  => candidate REAL EE JIT divergence (clean interp control, MEMORY differs) — zooming to classify...");
				const bool stop = ZoomFromCheckpoint(ckpt);
				if (stop)
				{
					FileSystem::DeleteFilePath(ckpt.c_str());
					return EXIT_FAILURE;
				}
				++benign_frames;
				Console.WriteLn(fmt::format(
					"  => frame {}: the frame-boundary divergence resolved to benign timing only; continuing scan.", f));
			}
			else
			{
				Console.WriteLn("  => discounted: interp control also diverges, so this is sampling jitter, "
				                "not a codegen bug.");
				for (const auto& d : ii)
					Console.WriteLn(fmt::format("      [ctrl] {}", d));
			}
		}

		// Advance the golden interp timeline by one frame for the next checkpoint.
		// (After an inline zoom the VM is wherever the last re-run left it; loadCkpt
		// restores this frame's start, then AdvanceFrames steps to the next.)
		if (!loadCkpt())
			return EXIT_FAILURE;
		SetEeMode(false);
		AdvanceFrames(1);
	}

	FileSystem::DeleteFilePath(ckpt.c_str());
	if (benign_frames > 0)
		Console.WriteLn(fmt::format(
			"STEPDIFF: no real JIT divergence over {} frames ({} frame(s) had benign cycle-derived timer "
			"divergences that the zoom walked past).", s_frames, benign_frames));
	else
		Console.WriteLn(fmt::format("STEPDIFF: no real JIT divergence over {} frames (interp control clean throughout).",
			s_frames));
	return EXIT_SUCCESS;
}

// ===================================================================
// --vu0diff : live VU0-jit-vs-interp COP2-read value diff
// ===================================================================
// Pin the EE INTERPRETER in both passes and toggle ONLY the VU0 micro engine.
// Each pass loads the same checkpoint, runs one frame, and records every COP2
// read (QMFC2 reads VF[fs], CFC2 reads VI[fs]) the EE interpreter performs of
// VU0 state, in execution order. Diffing the two read-streams pins the FIRST
// VU0 program output the micro JIT computes differently from the VU0
// interpreter — live, with the real EE<->VU0 interleave the offline
// capture-replay harness can't reproduce.
//
// CAVEAT (read first): this is SINGLE-ARCH jit-vs-interp. It is the right tool
// for an arithmetic VALUE bug (the JIT computes a wrong number), but a
// divergence in the flag / Q / cycle pipeline INSTANCE is usually
// shared-with-x86 and NOT arch-specific — the FMAND-flag and cycle-bubble red
// herrings of the Burnout 3 hunt all collapsed this way. Confirm any lead from
// here with an arm64-jit-vs-x86-jit diff before trusting it.

// Capture hooks live in pcsx2/VU0.cpp; null in production.
typedef void (*Cop2ReadHook)(u32 ee_pc, u32 op, u32 fs, const u32* lanes);
typedef void (*Cop2StateHook)(u32 tpc, u32 q, u32 mac, u32 status, u32 clip);
extern Cop2ReadHook g_cop2ReadHook;
extern Cop2StateHook g_cop2StateHook;

namespace
{
struct Cop2Read
{
	u32 ee_pc, op, fs;
	u32 lanes[4];
	u32 tpc, q, mac, status, clip;
};
std::vector<Cop2Read> s_cop2_sink;

void Cop2ReadCapture(u32 ee_pc, u32 op, u32 fs, const u32* lanes)
{
	Cop2Read r{};
	r.ee_pc = ee_pc;
	r.op = op;
	r.fs = fs;
	// QMFC2 (op 0) reads a full 128-bit VF; CFC2 (op 1) reads one 32-bit VI.
	r.lanes[0] = lanes[0];
	r.lanes[1] = (op == 0) ? lanes[1] : 0;
	r.lanes[2] = (op == 0) ? lanes[2] : 0;
	r.lanes[3] = (op == 0) ? lanes[3] : 0;
	s_cop2_sink.push_back(r);
}

void Cop2StateCapture(u32 tpc, u32 q, u32 mac, u32 status, u32 clip)
{
	if (s_cop2_sink.empty())
		return;
	Cop2Read& r = s_cop2_sink.back();
	r.tpc = tpc;
	r.q = q;
	r.mac = mac;
	r.status = status;
	r.clip = clip;
}
} // namespace

// Diff two per-COP2-read VU0-value streams (a=interp golden, b=jit candidate) and
// report the FIRST read whose VU0 value differs. This pins the exact COP2 read — and
// thus the VU0 program output — where the micro JIT first diverges from the interp,
// upstream of the geometry-buffer corruption. VI flag regs read by CFC2 (16/17/18/
// 22/23/26) are stopping-point/flag noise (shared block-overshoot); they're shown but
// the FIRST-QMFC2-divergence line is the actionable signal.
static void ReportCop2ReadDiff(const std::vector<Cop2Read>& a, const std::vector<Cop2Read>& b)
{
	auto asf = [](u32 u) { float f; std::memcpy(&f, &u, 4); return f; };
	auto is_flag_vi = [](u32 fs) {
		return fs == 16 || fs == 17 || fs == 18 || fs == 22 || fs == 23 || fs == 26;
	};
	const size_t n = std::min(a.size(), b.size());
	Console.WriteLn(fmt::format("  COP2-read streams: interp={} reads, jit={} reads", a.size(), b.size()));
	int shown = 0;
	bool first_qmfc2 = false;
	for (size_t i = 0; i < n; ++i)
	{
		const Cop2Read& x = a[i];
		const Cop2Read& y = b[i];
		if (x.op != y.op || x.fs != y.fs || x.ee_pc != y.ee_pc)
		{
			Console.WriteLn(fmt::format(
				"  read #{}: STRUCTURAL divergence  interp(pc={:#010x} op={} fs={})  jit(pc={:#010x} op={} fs={})",
				i, x.ee_pc, x.op, x.fs, y.ee_pc, y.op, y.fs));
			break; // control flow split — everything after is unaligned
		}
		const bool diff = x.lanes[0] != y.lanes[0] || x.lanes[1] != y.lanes[1] ||
			x.lanes[2] != y.lanes[2] || x.lanes[3] != y.lanes[3];
		if (!diff)
			continue;
		const char* tag = (x.op == 0) ? "QMFC2 VF" : (is_flag_vi(x.fs) ? "CFC2  VI(flag)" : "CFC2  VI");
		if (x.op == 0)
		{
			if (!first_qmfc2)
			{
				Console.WriteLn(fmt::format(
					"  >>> FIRST QMFC2 VF divergence at read #{} (pc={:#010x} VF{:02}):", i, x.ee_pc, x.fs));
				first_qmfc2 = true;
			}
			Console.WriteLn(fmt::format(
				"  read #{} {} {:02}  pc={:#010x}\n"
				"      interp {:08x}_{:08x}_{:08x}_{:08x}  ({:g} {:g} {:g} {:g})\n"
				"      jit    {:08x}_{:08x}_{:08x}_{:08x}  ({:g} {:g} {:g} {:g})",
				i, tag, x.fs, x.ee_pc,
				x.lanes[0], x.lanes[1], x.lanes[2], x.lanes[3], asf(x.lanes[0]), asf(x.lanes[1]), asf(x.lanes[2]), asf(x.lanes[3]),
				y.lanes[0], y.lanes[1], y.lanes[2], y.lanes[3], asf(y.lanes[0]), asf(y.lanes[1]), asf(y.lanes[2]), asf(y.lanes[3])));
		}
		else
		{
			Console.WriteLn(fmt::format(
				"  read #{} {} {:02}  pc={:#010x}  interp={:08x}  jit={:08x}",
				i, tag, x.fs, x.ee_pc, x.lanes[0], y.lanes[0]));
		}
		if (++shown >= 40)
		{
			Console.WriteLn("  ... (40 divergent reads shown; truncating)");
			break;
		}
	}
	if (a.size() != b.size())
		Console.WriteLn(fmt::format("  NOTE: read-count differs (interp={} jit={}) — VU0-jit drove a different COP2 path",
			a.size(), b.size()));
}

static int RunVu0Diff()
{
	Error error;
	const std::string ckpt = Path::Combine(EmuFolders::Cache, "eerunner_vu0diff.p2s");

	auto saveCkpt = [&]() -> bool {
		bool ok = true;
		VMManager::SaveState(ckpt.c_str(), /*zip_on_thread=*/false, /*backup_old_state=*/false,
			[&](const std::string& e) { ok = false; Console.ErrorFmt("vu0diff: save failed: {}", e); });
		VMManager::WaitForSaveStateFlush();
		return ok;
	};
	// One pass: load the checkpoint, set VU0 mode, capture the COP2 read-stream
	// for exactly one frame from the just-loaded checkpoint.
	auto runOne = [&](bool vu0_jit, std::vector<Cop2Read>& out) -> bool {
		if (!VMManager::LoadState(ckpt.c_str(), &error))
		{
			Console.ErrorFmt("vu0diff: load checkpoint failed: {}", error.GetDescription());
			return false;
		}
		SetVu0Mode(vu0_jit);
		s_cop2_sink.clear();
		g_cop2ReadHook = &Cop2ReadCapture;
		g_cop2StateHook = &Cop2StateCapture;
		AdvanceFrames(1);
		g_cop2ReadHook = nullptr;
		g_cop2StateHook = nullptr;
		out = s_cop2_sink;
		return true;
	};

	if (!VMManager::LoadState(s_savestate_path.c_str(), &error))
	{
		Console.ErrorFmt("vu0diff: initial load failed: {}", error.GetDescription());
		return EXIT_FAILURE;
	}
	SetVu0Mode(false); // golden timeline = VU0 interp

	for (uint32_t f = 0; f < s_frames && VMManager::GetState() != VMState::Shutdown; ++f)
	{
		if (!saveCkpt())
			return EXIT_FAILURE;

		std::vector<Cop2Read> interp, jit;
		if (!runOne(false, interp) || !runOne(true, jit))
			return EXIT_FAILURE;

		// Did any QMFC2 VF value diverge? (The actionable signal; flag-VI noise
		// is reported by ReportCop2ReadDiff but doesn't gate the per-frame header.)
		bool qmfc2_diverged = false;
		const size_t n = std::min(interp.size(), jit.size());
		for (size_t i = 0; i < n && !qmfc2_diverged; ++i)
		{
			const Cop2Read& x = interp[i];
			const Cop2Read& y = jit[i];
			if (x.op == 0 && (x.lanes[0] != y.lanes[0] || x.lanes[1] != y.lanes[1] ||
				x.lanes[2] != y.lanes[2] || x.lanes[3] != y.lanes[3]))
				qmfc2_diverged = true;
		}

		Console.WriteLn(fmt::format("VU0DIFF frame {}: {} ({} interp / {} jit COP2 reads)", f,
			qmfc2_diverged ? "QMFC2 VF DIVERGES" : "no QMFC2 value divergence", interp.size(), jit.size()));
		if (qmfc2_diverged || interp.size() != jit.size())
			ReportCop2ReadDiff(interp, jit);

		// Advance the golden interp timeline by one frame for the next checkpoint.
		if (!VMManager::LoadState(ckpt.c_str(), &error))
			return EXIT_FAILURE;
		SetVu0Mode(false);
		AdvanceFrames(1);
	}

	FileSystem::DeleteFilePath(ckpt.c_str());
	Console.WriteLn(fmt::format("VU0DIFF: scanned {} frame(s).", s_frames));
	return EXIT_SUCCESS;
}

// --liverun: reproduce the in-game HANG headlessly. Unlike the deterministic diff
// modes, this enables the live subsystems (real GS so GIF is consumed, MTVU on) and
// runs a single straight EE-jit pass. If the EE wedges in a spin/sync loop (the
// frozen-frame + looping-audio symptom), VMManager::Execute() never returns for a
// frame; a watchdog thread notices the stalled frame counter, samples the live EE
// PC (cpuRegs.pc) to fingerprint the spin loop, prints a PC histogram, and exits
// with code 42. A clean completion (all frames, code 0) means we did NOT reproduce
// the hang in this configuration.
// --disasm: load the savestate and disassemble EE code in [EERUNNER_DIS_LO,
// EERUNNER_DIS_HI] (default 0x100000..0x100040) with the correct R5900 disassembler.
// Generic MIPS disassemblers garble R5900 COP2/MMI ops; this tool does not.
static int RunDisasm()
{
	Error error;
	if (!VMManager::LoadState(s_savestate_path.c_str(), &error))
	{
		Console.ErrorFmt("disasm: load failed: {}", error.GetDescription());
		return EXIT_FAILURE;
	}
	u32 lo = 0x100000, hi = 0x100040;
	if (const char* e = std::getenv("EERUNNER_DIS_LO")) lo = (u32)strtoul(e, nullptr, 0);
	if (const char* e = std::getenv("EERUNNER_DIS_HI")) hi = (u32)strtoul(e, nullptr, 0);
	Console.WriteLn(fmt::format("DISASM 0x{:08x}..0x{:08x}:", lo, hi));
	for (u32 a = lo; a <= hi; a += 4)
	{
		const u32 code = memRead32(a);
		std::string line;
		R5900::disR5900Fasm(line, code, a, false);
		Console.WriteLn(fmt::format("    {:#010x}: {:08x}  {}", a, code, line));
	}
	return EXIT_SUCCESS;
}

static std::atomic<uint32_t> s_liverun_frame{0};
static std::atomic<bool>     s_liverun_done{false};

// Step-0 wedge classifier: snapshot the EE event/interrupt scheduler state. The
// recompiler and interpreter SHARE all of _cpuEventTest_Shared / intc / dmac /
// scheduling — the only jit-specific variables are (a) when a block re-enters the
// event test and (b) the cpuRegs.cycle value it has accumulated. So a "this IRQ
// never fires under jit" wedge is one of three modes, distinguishable here:
//   A  cycle FROZEN between snapshots          -> spin block costs 0 cycles / RECCYCLE stuck
//   B  cycle advances, INTC/DMAC pending+unmasked but never serviced -> arm64 codegen (event-test/x25)
//   C  cycle advances, nothing pending          -> IRQ never scheduled (IOP/SIF/DMA upstream)
// Racy reads of globals from the watchdog thread — fine for a stuck loop.
static void DumpEeEventState(int snap)
{
	const u64 cyc   = cpuRegs.cycle;
	const u64 nextE = cpuRegs.nextEventCycle;
	const u64 lastE = cpuRegs.lastEventCycle;
	const u32 ints  = cpuRegs.interrupt;
	const u32 stat  = cpuRegs.CP0.n.Status.val;
	const u32 intcS = psHu32(INTC_STAT);
	const u32 intcM = psHu32(INTC_MASK);
	const u16 dmacS = psHu16(0xe012);
	const u16 dmacM = psHu16(0xe010);
	Console.Error(fmt::format(
		"  [snap {}] cycle={} nextEvent={} (dEvt={}) lastEvent={} | interrupt=0x{:08x} branch={}",
		snap, cyc, nextE, (s64)(nextE - cyc), lastE, ints, cpuRegs.branch));
	Console.Error(fmt::format(
		"           CP0.Status=0x{:08x} (EIE={} ERL={} EXL={} IE={} IM_INTC={} IM_DMAC={})",
		stat, (stat >> 16) & 1, (stat >> 2) & 1, (stat >> 1) & 1, stat & 1,
		(stat >> 10) & 1, (stat >> 11) & 1));
	Console.Error(fmt::format(
		"           INTC_STAT=0x{:08x} INTC_MASK=0x{:08x} (pending&unmasked=0x{:08x}) | DMAC_STAT=0x{:04x} DMAC_MASK=0x{:04x} (pend=0x{:04x})",
		intcS, intcM, intcS & intcM, dmacS, dmacM, (u16)(dmacS & dmacM)));
	// Per-source scheduled EE interrupts: which channels have a deadline, and has cycle passed it?
	if (ints)
	{
		std::string sched;
		for (int n = 0; n < 32; n++)
		{
			if (!(ints & (1u << n)))
				continue;
			const u64 deadline = cpuRegs.sCycle[n] + cpuRegs.eCycle[n];
			sched += fmt::format(" int[{}]: sCycle={} eCycle={} deadline={} ({}); ",
				n, cpuRegs.sCycle[n], cpuRegs.eCycle[n], deadline,
				(s64)(deadline - cyc) <= 0 ? "DUE" : "future");
		}
		Console.Error(fmt::format("           scheduled:{}", sched));
	}
}

static int RunLiveRun()
{
	Error error;
	if (!VMManager::LoadState(s_savestate_path.c_str(), &error))
	{
		Console.ErrorFmt("liverun: load failed: {}", error.GetDescription());
		return EXIT_FAILURE;
	}

	Console.WriteLn(fmt::format(
		"LIVERUN: EE=jit, MTVU=on, real GS — running up to {} frames (10s no-progress watchdog)...",
		s_frames));

	std::thread watchdog([]() {
		uint32_t last = 0;
		int stalled = 0;
		while (!s_liverun_done.load(std::memory_order_relaxed))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			if (s_liverun_done.load(std::memory_order_relaxed))
				return;
			const uint32_t cur = s_liverun_frame.load(std::memory_order_relaxed);
			if (cur != last)
			{
				last = cur;
				stalled = 0;
				continue;
			}
			if (++stalled < 20) // 20 * 500ms = ~10s of no frame progress
				continue;

			// Wedged: the EE has not finished a frame in ~10s. Sample the live EE PC
			// to fingerprint the spin/sync loop (racy read of a global u32 — fine for
			// a stuck loop whose PC sits in a tiny range).
			std::map<u32, int> hist;
			u32 pcmin = ~0u, pcmax = 0;
			const int N = 4000;
			for (int i = 0; i < N; i++)
			{
				const u32 pc = cpuRegs.pc;
				hist[pc]++;
				pcmin = std::min(pcmin, pc);
				pcmax = std::max(pcmax, pc);
				std::this_thread::sleep_for(std::chrono::microseconds(250));
			}
			Console.Error(fmt::format(
				"LIVERUN WEDGE: no frame completed past frame {} for ~10s. "
				"EE spin PC range [0x{:08x} .. 0x{:08x}], {} distinct PCs over {} samples:",
				last, pcmin, pcmax, hist.size(), N));
			std::vector<std::pair<u32, int>> top(hist.begin(), hist.end());
			std::sort(top.begin(), top.end(),
				[](const auto& a, const auto& b) { return a.second > b.second; });
			for (size_t i = 0; i < top.size() && i < 16; i++)
				Console.Error(fmt::format("    pc=0x{:08x}  {:5d}  ({:4.1f}%)",
					top[i].first, top[i].second, 100.0 * top[i].second / N));
			// Disassemble the dominant spin block so the poll/branch + the awaited
			// memory operand are visible. The EE is stuck looping in this one block,
			// so its code bytes are stable to read from here.
			if (!top.empty())
				DisasmBlock(top[0].first);

			// Step-0 wedge classifier: two snapshots of the EE event/interrupt
			// scheduler ~200ms apart. Whether cpuRegs.cycle MOVES between them, and
			// whether an INTC/DMAC source is pending-but-unserviced, classifies the
			// wedge into mode A (cycle frozen), B (codegen: pending never delivered),
			// or C (never scheduled — upstream). See DumpEeEventState().
			Console.Error("LIVERUN WEDGE: EE event/interrupt scheduler state:");
			DumpEeEventState(0);
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			DumpEeEventState(1);

			std::fflush(stdout);
			std::fflush(stderr);
			std::_Exit(42); // distinct from EXIT_FAILURE; bypass GS teardown deliberately
		}
	});

	// Soft-freeze probe: the frame-count watchdog can't see this hang, because the EE
	// keeps ticking vblank so frames KEEP completing (frozen frames). Instead watch a
	// guest EE-RAM word (EERUNNER_WATCH_ADDR) that should keep changing during normal
	// play; if it stays constant for many frames WHILE the frame counter advances, the
	// game is frozen even though the harness "completes" frames. For Burnout 3 the
	// idle-loop consumer pointer is 0x4e2838 (waits to equal 0x4e283c == addr+4).
	u32 watch_addr = 0;
	if (const char* e = std::getenv("EERUNNER_WATCH_ADDR"))
		watch_addr = static_cast<u32>(strtoul(e, nullptr, 0));
	auto eeR32 = [](u32 paddr) -> u32 {
		return *reinterpret_cast<const u32*>(reinterpret_cast<const char*>(eeMem->Main) + (paddr & 0x1ffffffu));
	};
	u32 watch_prev = 0;
	bool watch_init = false;
	int watch_stuck = 0;

	for (uint32_t f = 0; f < s_frames && VMManager::GetState() != VMState::Shutdown; ++f)
	{
		VMManager::FrameAdvance(1);
		VMManager::Execute(); // blocks for one frame; if the EE wedges, never returns
		s_liverun_frame.store(f + 1, std::memory_order_relaxed);

		if (watch_addr)
		{
			const u32 v = eeR32(watch_addr);
			if (watch_init && v == watch_prev)
			{
				if (++watch_stuck == 120) // ~2s of frozen game while frames advanced
				{
					const u32 vb = eeR32(watch_addr + 4);
					Console.Error(fmt::format(
						"LIVERUN SOFT-FREEZE: watch[0x{:08x}]=0x{:08x} unchanged for {} frames "
						"(frame counter reached {}); neighbor[+4]=0x{:08x}. Game frozen, vblank still ticking.",
						watch_addr, v, watch_stuck, f + 1, vb));
					Console.Error("LIVERUN SOFT-FREEZE: EE event/interrupt scheduler state:");
					DumpEeEventState(0);
					// Fingerprint the EE spin PCs (game idle loop).
					std::map<u32, int> hist;
					for (int i = 0; i < 2000; i++)
					{
						hist[cpuRegs.pc]++;
						std::this_thread::sleep_for(std::chrono::microseconds(200));
					}
					std::vector<std::pair<u32, int>> top(hist.begin(), hist.end());
					std::sort(top.begin(), top.end(), [](auto& a, auto& b) { return a.second > b.second; });
					for (size_t i = 0; i < top.size() && i < 8; i++)
						Console.Error(fmt::format("    pc=0x{:08x}  {:5d}", top[i].first, top[i].second));
					// Disasm a window around the hottest spin PC — the loop the game
					// is actually stuck in — so this is game-agnostic. Override the
					// range with EERUNNER_DIS_LO..EERUNNER_DIS_HI when chasing a
					// divergence whose deciding branch sits outside the spin window.
					{
						const u32 center = top.empty() ? cpuRegs.pc : top[0].first;
						u32 lo = center - 0x40, hi = center + 0x40;
						if (const char* e = std::getenv("EERUNNER_DIS_LO")) lo = (u32)strtoul(e, nullptr, 0);
						if (const char* e = std::getenv("EERUNNER_DIS_HI")) hi = (u32)strtoul(e, nullptr, 0);
						Console.Error(fmt::format("LIVERUN SOFT-FREEZE: disasm 0x{:08x}..0x{:08x}:", lo, hi));
						for (u32 a = lo; a <= hi; a += 4)
						{
							const u32 code = memRead32(a);
							std::string line;
							R5900::disR5900Fasm(line, code, a, false);
							Console.Error(fmt::format("    {:#010x}: {:08x}  {}", a, code, line));
						}
					}
					std::fflush(stdout);
					std::fflush(stderr);
					std::_Exit(43); // distinct from 42 (frame-count wedge)
				}
			}
			else
			{
				watch_stuck = 0;
			}
			watch_prev = v;
			watch_init = true;
		}
	}

	s_liverun_done.store(true, std::memory_order_relaxed);
	watchdog.join();

	// Per-frame GS stats (averaged over g_perfmon's last 32-frame window): draws, render
	// passes, readbacks (RB), texture copies/uploads. Read cross-thread while the VM is
	// still up — racy u64 reads, cosmetic-only. @GSSTAT@ so scripts can grep it.
	{
		SmallString gs_stats;
		GSgetStats(gs_stats);
		Console.WriteLn(fmt::format("@GSSTAT@ per-frame: {}", gs_stats.view()));
	}

#ifdef __linux__
	// Per-thread CPU seconds (utime+stime) while the VM threads are still alive.
	// Wallclock A/B on the SD865 is too noisy for sub-ms/frame codegen deltas; the
	// per-thread number isolates e.g. GS-thread cost from GPU/sync/scheduler jitter.
	// comm can contain spaces ("CPU Thread") — parse /proc stat around the ')'.
	{
		const long tck = sysconf(_SC_CLK_TCK);
		for (const auto& entry : std::filesystem::directory_iterator("/proc/self/task"))
		{
			std::ifstream stat_file(entry.path() / "stat");
			std::string line;
			if (!std::getline(stat_file, line))
				continue;
			const size_t rp = line.rfind(')');
			if (rp == std::string::npos)
				continue;
			const std::string comm = line.substr(line.find('(') + 1, rp - line.find('(') - 1);
			std::istringstream rest(line.substr(rp + 2));
			std::string field;
			u64 utime = 0, stime = 0;
			for (int i = 1; rest >> field; i++) // field 1 here = stat field 3 (state)
			{
				if (i == 12)
					utime = std::strtoull(field.c_str(), nullptr, 10);
				else if (i == 13)
				{
					stime = std::strtoull(field.c_str(), nullptr, 10);
					break;
				}
			}
			Console.WriteLn(fmt::format("@THREADCPU@ {}: {:.2f} s", comm,
				static_cast<double>(utime + stime) / static_cast<double>(tck)));
		}
	}
#endif

	Console.WriteLn(fmt::format(
		"LIVERUN: completed {} frames with NO wedge — this config did not reproduce the hang.",
		s_frames));
	return EXIT_SUCCESS;
}

#ifdef _WIN32
// Unicode filenames require wmain on Win32; use the ascii main() with this workaround.
#define main real_main
#endif

static void CPUThreadMain(VMBootParameters* params, std::atomic<int>* ret)
{
	ret->store(EXIT_FAILURE);

	if (VMManager::Internal::CPUThreadInitialize())
	{
		// Profiling: set the jitdump output dir before any JIT block compiles (the
		// first compile happens during the first FrameAdvance, well after this). Dir =
		// EmuFolders::Cache so the 100s-of-MB dump avoids /tmp/tmpfs, matching the
		// production rationale in common/Perf.cpp. The ENABLE flag is driven through the
		// normal settings path instead (EmuCore/Profiler EnablePerfDump, set in the
		// harness config) — ApplySettings() below calls LoadSettings() which re-applies
		// Perf::SetJitDumpEnabled(EnablePerfDump), so a manual enable here would just get
		// reset to the config default (false). No-op on non-jitdump builds.
		if (s_perf_jitdump)
			Perf::SetJitDumpDir(EmuFolders::Cache);

		// apply new settings (e.g. pick up renderer change)
		VMManager::ApplySettings();

		if (VMManager::Initialize(*params) == VMBootResult::StartupSuccess)
		{
			// Run unlimited — the runner steps frame-by-frame and needs no
			// wall-clock pacing.
			VMManager::SetLimiterMode(LimiterModeType::Unlimited);
			VMManager::SetState(VMState::Paused);

			int code = EXIT_FAILURE;
			switch (s_mode)
			{
				case RunMode::SelfCheck:
					code = RunSelfCheck();
					break;

				// --localize / --repro / --stepdiff all run the robust
				// checkpoint-anchored comparison. The frame-boundary pass
				// conflates codegen bugs with the ~10-cycle async pause-point
				// sampling jitter (see --selfcheck, which characterizes that
				// jitter). --repro is the fast iteration verb (point it at a
				// savestate already narrowed to the bug);
				// --localize/--stepdiff are aliases.
				case RunMode::Localize:
				case RunMode::Repro:
				case RunMode::StepDiff:
					code = RunStepDiff();
					break;

				case RunMode::Vu0Diff:
					code = RunVu0Diff();
					break;

				case RunMode::ContMem:
					code = RunContinuousMemTrajectory();
					break;

				case RunMode::SpeedhackDiff:
					code = RunSpeedhackDiff();
					break;

				case RunMode::LiveRun:
					code = RunLiveRun();
					break;

				case RunMode::Disasm:
					code = RunDisasm();
					break;

				default:
					break;
			}

			VMManager::Shutdown(false);
			ret->store(code);
		}
		else
		{
			Console.Error("eerunner: VMManager::Initialize failed.");
		}
	}

	VMManager::Internal::CPUThreadShutdown();
}

int main(int argc, char* argv[])
{
	CrashHandler::Install();
	EERunner::InitializeConsole();

	std::signal(SIGINT, [](int) { VMManager::SetState(VMState::Stopping); });
	std::signal(SIGTERM, [](int) { VMManager::SetState(VMState::Stopping); });

	if (!EERunner::InitializeConfig())
	{
		Console.Error("Failed to initialize config.");
		return EXIT_FAILURE;
	}

	VMBootParameters params;
	if (!EERunner::ParseCommandLineArgs(argc, argv, params))
		return EXIT_FAILURE;

	SysMemory::ReserveMemory();

	// --selfcheck and --vu0diff force the EE interpreter. Must be set BEFORE
	// VMManager::Initialize. (--vu0diff toggles only VU0; the EE stays interp in
	// both passes so the only moving part is the VU0 micro engine.)
	if (s_mode == RunMode::SelfCheck || s_mode == RunMode::Vu0Diff)
		s_settings_interface.SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", false);

	// The checkpoint-anchored modes emit the per-block divtrace hook into every
	// JIT block prologue (used by the zoom's sparse JIT stream). g_emit_block_hook
	// is read at block-compile time, so it must be set before the recompiler is
	// initialized; it stays true for the whole process (the interp passes simply
	// never compile EE blocks, and the hook is a g_enabled-gated no-op otherwise).
	// Production builds never set it, so they emit nothing.
	if (s_mode == RunMode::Localize || s_mode == RunMode::Repro || s_mode == RunMode::StepDiff)
		ee_divtrace::g_emit_block_hook = true;

	// Mirror the JIT's recSYSCALL FlushCache/iFlushCache skip in the golden interp
	// passes so both timelines stay bit-identical across that ABI-benign divergence
	// (otherwise the JIT skip vs interp-runs-handler shows up as a register + kernel-
	// stack diff that masks real bugs downstream). Harmless for --selfcheck (both
	// interp runs skip identically), so we set it for every mode the runner uses.
	ee_divtrace::g_skip_flushcache_syscall = true;

	// EERUNNER_NOFP=1: drop the FPU register file + ACC from the alignment
	// fingerprint and the diff helpers, so the zoom walks PAST FP-register
	// divergences to hunt a non-FP (integer / control-flow) divergence. Use when the
	// FP path is known benign (Burnout 3: hang persists with the EE-FPU fully
	// converged to interp — the real bug is the integer cond_b/pointer math, and the
	// pervasive 1-ULP div.s noise was masking it). Combine with EERUNNER_FPUFULL=1 to
	// also converge mul/add and minimize FP laundered into GPRs via store/reload.
	if (const char* e = std::getenv("EERUNNER_NOFP"))
		ee_divtrace::g_fp_exclude = (e[0] != '0');

	// Override settings that shouldn't be picked up from defaults or INIs.
	EERunner::SettingsOverride();

	std::atomic<int> thread_ret;
	std::thread cputhread(CPUThreadMain, &params, &thread_ret);
	cputhread.join();

	return thread_ret.load();
}

#ifdef _WIN32

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

	return real_main(argc, u8_argptrs.data());
}

#endif // _WIN32
