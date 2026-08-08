// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "PerformanceMetrics.h"
#include "GS/GSPerfMon.h"
#include "GS.h"
#include "GS/GSLzma.h"
#include "GSDumpReplayer.h"
#include "GameList.h"
#include "Gif.h"
#include "Gif_Unit.h"
#include "Host.h"
#include "ImGui/ImGuiManager.h"
#include "ImGui/ImGuiOverlays.h"
#include "R3000A.h"
#include "R5900.h"
#include "VMManager.h"
#include "VUmicro.h"

#include "imgui.h"

#include "fmt/format.h"

#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/Timer.h"

#include <atomic>
#include <cinttypes>

static void GSDumpReplayerCpuReserve();
static void GSDumpReplayerCpuShutdown();
static void GSDumpReplayerCpuReset();
static void GSDumpReplayerCpuStep();
static void GSDumpReplayerCpuExecute();
static void GSDumpReplayerExitExecution();
static void GSDumpReplayerCancelInstruction();
static void GSDumpReplayerCpuClear(u32 addr, u32 size);

static std::unique_ptr<GSDumpFile> s_dump_file;
static u32 s_current_packet = 0;
static u32 s_dump_frame_number = 0;
static s32 s_dump_loop_count = 0;
static bool s_dump_running = false;
static bool s_needs_state_loaded = false;
static u64 s_frame_ticks = 0;
static u64 s_next_frame_time = 0;
static bool s_is_dump_runner = false;
static bool s_dump_perf_metrics = true;

static bool s_use_frame_range = false;
static bool s_init_frame_range = false;
static u32 s_frame_start = 0;
static u32 s_frame_end = 0;
static u32 s_frame_start_packet = 0;
static u32 s_frame_end_packet = 0;

static GSDumpReplayer::PerfMetrics s_perf_metrics = {};
static GSPerfMon s_gs_stats;
static u64 s_total_frames = 0;
static u64 s_total_drawn_frames = 0;

R5900cpu GSDumpReplayerCpu = {
	GSDumpReplayerCpuReserve,
	GSDumpReplayerCpuShutdown,
	GSDumpReplayerCpuReset,
	GSDumpReplayerCpuStep,
	GSDumpReplayerCpuExecute,
	GSDumpReplayerExitExecution,
	GSDumpReplayerCancelInstruction,
	GSDumpReplayerCpuClear};

static InterpVU0 gsDumpVU0;
static InterpVU1 gsDumpVU1;

bool GSDumpReplayer::IsReplayingDump()
{
	return static_cast<bool>(s_dump_file);
}

bool GSDumpReplayer::IsRunner()
{
	return s_is_dump_runner;
}

void GSDumpReplayer::SetIsDumpRunner(bool is_runner, bool dump_perf)
{
	s_is_dump_runner = is_runner;
	s_dump_perf_metrics = dump_perf;
}

void GSDumpReplayer::SetLoopCount(s32 loop_count)
{
	s_dump_loop_count = loop_count - 1;
}

int GSDumpReplayer::GetLoopCount()
{
	return s_dump_loop_count;
}

void GSDumpReplayer::SetFrameRange(bool use, u32 start, u32 end)
{
	s_use_frame_range = use;
	s_frame_start = start;
	s_frame_end = end;
}

bool GSDumpReplayer::Initialize(const char* filename, Error* error)
{
	Common::Timer timer;
	Console.WriteLn("(GSDumpReplayer) Reading file '%s'...", filename);

	Error dump_error;
	s_dump_file = GSDumpFile::OpenGSDump(filename, &dump_error);
	if (!s_dump_file || !s_dump_file->ReadFile(&dump_error))
	{
		Error::SetStringFmt(error, TRANSLATE_FS("GSDumpReplayer", "Failed to open or read '{}': {}"),
			Path::GetFileName(filename), dump_error.GetDescription());
		s_dump_file.reset();
		return false;
	}

	Console.WriteLn("(GSDumpReplayer) Read file in %.2f ms.", timer.GetTimeMilliseconds());

	// We replace all CPUs.
	Cpu = &GSDumpReplayerCpu;
	psxCpu = &psxInt;
	CpuVU0 = &gsDumpVU0;
	CpuVU1 = &gsDumpVU1;

	// loop infinitely by default
	s_dump_loop_count = -1;

	return true;
}

bool GSDumpReplayer::ChangeDump(const char* filename)
{
	Console.WriteLn("(GSDumpReplayer) Switching to '%s'...", filename);

	if (!VMManager::IsGSDumpFileName(filename))
	{
		Host::ReportFormattedErrorAsync("GSDumpReplayer", "'%s' is not a GS dump.", filename);
		return false;
	}

	Error error;
	std::unique_ptr<GSDumpFile> new_dump(GSDumpFile::OpenGSDump(filename));
	if (!new_dump || !new_dump->ReadFile(&error))
	{
		Host::ReportErrorAsync("GSDumpReplayer", fmt::format("Failed to open or read '{}': {}",
													 Path::GetFileName(filename), error.GetDescription()));
		return false;
	}

	s_dump_file = std::move(new_dump);
	s_current_packet = 0;

	// Don't forget to reset the GS!
	GSDumpReplayerCpuReset();
	return true;
}

void GSDumpReplayer::Shutdown()
{
	Console.WriteLn("(GSDumpReplayer) Shutting down.");

	DumpStats();

	Cpu = nullptr;
	psxCpu = nullptr;
	CpuVU0 = nullptr;
	CpuVU1 = nullptr;
	s_dump_file.reset();
}

std::string GSDumpReplayer::GetDumpSerial()
{
	std::string ret;

	if (!s_dump_file->GetSerial().empty())
	{
		ret = s_dump_file->GetSerial();
	}
	else if (s_dump_file->GetCRC() != 0)
	{
		// old dump files don't have serials, but we have the crc...
		// so, let's try searching the game list for a crc match.
		auto lock = GameList::GetLock();
		const GameList::Entry* entry = GameList::GetEntryByCRC(s_dump_file->GetCRC());
		if (entry)
			ret = entry->serial;
	}

	return ret;
}

u32 GSDumpReplayer::GetDumpCRC()
{
	return s_dump_file->GetCRC();
}

u32 GSDumpReplayer::GetFrameNumber()
{
	return s_dump_frame_number;
}

void GSDumpReplayerCpuReserve()
{
}

void GSDumpReplayerCpuShutdown()
{
}

void GSDumpReplayerCpuReset()
{
	s_needs_state_loaded = true;
	s_current_packet = 0;
	s_dump_frame_number = 0;
	s_perf_metrics = {};
	s_gs_stats.Reset();
	s_init_frame_range = false;
	s_frame_start_packet = 0;
	s_frame_end_packet = 0;
	s_total_frames = 0;
	s_total_drawn_frames = 0;
}

static void GSDumpReplayerLoadInitialState()
{
	// reset GS registers to initial dump values
	std::memcpy(PS2MEM_GS, s_dump_file->GetRegsData().data(),
		std::min(Ps2MemSize::GSregs, static_cast<u32>(s_dump_file->GetRegsData().size())));

	// load GS state
	freezeData fd = {static_cast<int>(s_dump_file->GetStateData().size()),
		const_cast<u8*>(s_dump_file->GetStateData().data())};
	MTGS::FreezeData mfd = {&fd, 0};
	MTGS::Freeze(FreezeAction::Load, mfd);
	if (mfd.retval != 0)
		Host::ReportFormattedErrorAsync("GSDumpReplayer", "Failed to load GS state.");
}

static void GSDumpReplayerSendPacketToMTGS(GIF_PATH path, const u8* data, size_t length)
{
	pxAssert((length % 16) == 0 && length < UINT32_MAX);

	const u32 truncated_length = static_cast<u32>(length);

	Gif_Path& gifPath = gifUnit.gifPath[path];
	gifPath.CopyGSPacketData(const_cast<u8*>(data), truncated_length);

	GS_Packet gsPack;
	gsPack.offset = gifPath.curOffset;
	gsPack.size = truncated_length;
	gifPath.curOffset += length;
	Gif_AddCompletedGSPacket(gsPack, path);
}

static void GSDumpReplayerUpdateFrameLimit()
{
	constexpr u32 default_frame_limit = 60;
	const u32 frame_limit = static_cast<u32>(default_frame_limit * VMManager::GetTargetSpeed());

	if (frame_limit > 0)
		s_frame_ticks = (GetTickFrequency() + (frame_limit / 2)) / frame_limit;
	else
		s_frame_ticks = 0;
}

static void GSDumpReplayerFrameLimit()
{
	if (s_frame_ticks == 0)
		return;

	// Frame limiter
	u64 now = GetCPUTicks();
	const s64 ms = GetTickFrequency() / 1000;
	const s64 sleep = s_next_frame_time - now - ms;
	if (sleep > ms)
		Threading::Sleep(static_cast<s32>(sleep / ms));
	while ((now = GetCPUTicks()) < s_next_frame_time)
		ShortSpin();
	s_next_frame_time = std::max(now, s_next_frame_time + s_frame_ticks);
}

void AdvanceNextLoop()
{
	const bool end_of_dump = (s_current_packet == static_cast<u32>(s_dump_file->GetPackets().size()));
	if (end_of_dump || (s_init_frame_range && s_current_packet > s_frame_end_packet))
	{
		s_dump_frame_number = 0;
		if (s_dump_loop_count > 0)
			s_dump_loop_count--;
		else if (s_dump_loop_count == 0)
		{
			Host::RequestVMShutdown(false, false, false);
			s_dump_running = false;
		}
	}
}

void UpdateFrameRangePacketsOnVSync()
{
	if (s_use_frame_range && !s_init_frame_range)
	{
		if (s_dump_frame_number == s_frame_start)
		{
			// +1 since the current packet is the previous frame VSync packet.
			s_frame_start_packet = s_current_packet + 1;
		}
		else if (s_dump_frame_number >= s_frame_end && s_frame_end > s_frame_start)
		{
			// Current packet is the VSync packet of the last frame we want to loop over.
			s_frame_end_packet = s_current_packet;
			s_init_frame_range = true;
		}
	}
}

void CheckFrameRange()
{
	if (s_init_frame_range)
	{
		if (s_current_packet > s_frame_end_packet || s_current_packet < s_frame_start_packet)
		{
			s_dump_frame_number = s_frame_start;
			s_current_packet = s_frame_start_packet;
		}
	}
	else if (s_current_packet == static_cast<u32>(s_dump_file->GetPackets().size()))
	{
		// This will run if the frame range goes past the end of the dump.
		s_frame_end_packet = s_current_packet;
		s_init_frame_range = true;
	}
}

void GSDumpReplayerCpuStep()
{
	if (s_needs_state_loaded)
	{
		GSDumpReplayerLoadInitialState();
		s_needs_state_loaded = false;
	}

	// Get the next packet.
	const GSDumpFile::GSData& packet = s_dump_file->GetPackets()[s_current_packet];

	switch (packet.id)
	{
		case GSDumpTypes::GSType::Transfer:
		{
			switch (packet.path)
			{
				case GSDumpTypes::GSTransferPath::Path1Old:
				{
					if(packet.length > 16384)
					{
						Console.Error("GSDumpReplayer: Path1Old transfer exceeds 16KB buffer. Skipping transfer");
						break;
					}
					std::unique_ptr<u8[]> data(new u8[16384]);
					const size_t addr = 16384 - packet.length;
					std::memcpy(data.get(), packet.data + addr, packet.length);
					GSDumpReplayerSendPacketToMTGS(GIF_PATH_1, data.get(), packet.length);
				}
				break;

				case GSDumpTypes::GSTransferPath::Path1New:
				case GSDumpTypes::GSTransferPath::Path2:
				case GSDumpTypes::GSTransferPath::Path3:
				{
					GSDumpReplayerSendPacketToMTGS(static_cast<GIF_PATH>(static_cast<u8>(packet.path) - 1),
						packet.data, packet.length);
				}
				break;

				default:
					break;
			}
			break;
		}

		case GSDumpTypes::GSType::VSync:
		{
			s_dump_frame_number++;
			GSDumpReplayerUpdateFrameLimit();
			GSDumpReplayerFrameLimit();
			MTGS::PostVsyncStart(false);
			VMManager::Internal::VSyncOnCPUThread();
			if (VMManager::Internal::IsExecutionInterrupted())
				GSDumpReplayerExitExecution();
			Host::PumpMessagesOnCPUThread();
			UpdateFrameRangePacketsOnVSync();
		}
		break;

		case GSDumpTypes::GSType::ReadFIFO2:
		{
			u32 size;
			std::memcpy(&size, packet.data, sizeof(size));

			// Allocate an extra quadword, some transfers write too much (e.g. Lego Racers 2 with Z24 downloads).
			std::unique_ptr<u8[]> arr(new u8[(size + 1) * 16]);
			MTGS::InitAndReadFIFO(arr.get(), size);
		}
		break;

		case GSDumpTypes::GSType::Registers:
		{
			std::memcpy(PS2MEM_GS, packet.data, std::min<s32>(static_cast<u32>(packet.length), Ps2MemSize::GSregs));
		}
		break;
	}

	// Increment the packet counter.
	s_current_packet++;

	// Increment the loop counter.
	AdvanceNextLoop();

	// Adjust packet index based on the frame range we're replaying.
	CheckFrameRange();

	// Loop packet index.
	s_current_packet %= static_cast<u32>(s_dump_file->GetPackets().size());
}

void GSDumpReplayerCpuExecute()
{
	s_dump_running = true;
	s_next_frame_time = GetCPUTicks();

	while (s_dump_running)
	{
		GSDumpReplayerCpuStep();
	}
}

void GSDumpReplayerExitExecution()
{
	s_dump_running = false;
}

void GSDumpReplayerCancelInstruction()
{
}

void GSDumpReplayerCpuClear(u32 addr, u32 size)
{
}

void GSDumpReplayer::RenderUI()
{
	const float scale = ImGuiManager::GetGlobalScale();
	const float shadow_offset = std::ceil(1.0f * scale);
	const float margin = std::ceil(GSConfig.OsdMargin * scale);
	const float spacing = std::ceil(5.0f * scale);
	float position_y = margin;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	ImFont* const font = ImGuiManager::GetFixedFont();
	const float font_size = ImGuiManager::GetFontSizeStandard();

	std::string text;
	ImVec2 text_size;
	text.reserve(128);

#define DRAW_LINE(font, size, text, color) \
	do \
	{ \
		text_size = font->CalcTextSizeA(size, std::numeric_limits<float>::max(), -1.0f, (text), nullptr, nullptr); \
		const ImVec2 text_pos = CalculatePerformanceOverlayTextPosition(GSConfig.OsdMessagesPos, margin, text_size, ImGuiManager::GetWindowWidth(), position_y); \
		dl->AddText(font, size, ImVec2(text_pos.x + shadow_offset, text_pos.y + shadow_offset), IM_COL32(0, 0, 0, 100), (text)); \
		dl->AddText(font, size, text_pos, color, (text)); \
		position_y += text_size.y + spacing; \
	} while (0)

	fmt::format_to(std::back_inserter(text), "Dump Frame: {}", s_dump_frame_number);
	DRAW_LINE(font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));

	text.clear();
	fmt::format_to(std::back_inserter(text), "Packet Number: {}/{}", s_current_packet, static_cast<u32>(s_dump_file->GetPackets().size()));
	DRAW_LINE(font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));

#undef DRAW_LINE
}

void GSDumpReplayer::UpdatePerformanceMetrics()
{
	s_perf_metrics.num_updates += 1.0f;
	s_perf_metrics.fps += PerformanceMetrics::GetFPS();
	s_perf_metrics.internal_fps += PerformanceMetrics::GetInternalFPS();
	s_perf_metrics.cpu_thread_usage += PerformanceMetrics::GetCPUThreadUsage();
	s_perf_metrics.cpu_thread_time += PerformanceMetrics::GetCPUThreadAverageTime();
	s_perf_metrics.gs_thread_usage += PerformanceMetrics::GetGSThreadUsage();
	s_perf_metrics.gs_thread_time += PerformanceMetrics::GetGSThreadAverageTime();
	s_perf_metrics.gpu_time += PerformanceMetrics::GetGPUAverageTime();
	s_perf_metrics.gpu_usage += PerformanceMetrics::GetGPUUsage();
	std::atomic_thread_fence(std::memory_order_release);
}

static void UpdateGSStatOne(GSPerfMon::counter_t counter)
{
	const double curr = g_perfmon.GetCounter(counter);
	const double last = s_gs_stats.GetCounter(counter);
	// GSPerfMon resets every 30 frames to zero.
	s_gs_stats.Put(counter, curr < last ? curr : (curr - last));
};

void GSDumpReplayer::UpdateGSStats()
{
	// Called on GS thread.
	if (GSIsHardwareRenderer())
	{
		const bool idle_frame = s_total_frames > 0 &&
			g_perfmon.GetCounter(GSPerfMon::Draw) == s_gs_stats.GetCounter(GSPerfMon::Draw) &&
			g_perfmon.GetCounter(GSPerfMon::TextureUploads) == s_gs_stats.GetCounter(GSPerfMon::TextureUploads);

		UpdateGSStatOne(GSPerfMon::Draw);
		UpdateGSStatOne(GSPerfMon::DrawCalls);
		UpdateGSStatOne(GSPerfMon::RenderPasses);
		UpdateGSStatOne(GSPerfMon::Barriers);
		UpdateGSStatOne(GSPerfMon::TextureCopies);
		UpdateGSStatOne(GSPerfMon::TextureUploads);
		UpdateGSStatOne(GSPerfMon::Readbacks);
		UpdateGSStatOne(GSPerfMon::TextureCopiesROV);
		UpdateGSStatOne(GSPerfMon::DrawCallsROV);
		UpdateGSStatOne(GSPerfMon::BarriersROV);

		if (!idle_frame)
			s_total_drawn_frames++;

		s_total_frames++;

		std::atomic_thread_fence(std::memory_order_release);
	}
}

static void DumpStatAndAvg(const char* format, double stat)
{
	Console.WriteLn(format, static_cast<u64>(stat), static_cast<u64>(std::ceil(stat / static_cast<double>(s_total_drawn_frames))));
}

void GSDumpReplayer::DumpStats()
{
	std::atomic_thread_fence(std::memory_order_acquire);
	Console.WriteLnFmt("======= HW STATISTICS FOR {} ({}) FRAMES ========", s_total_frames, s_total_drawn_frames);
	DumpStatAndAvg("@HWSTAT@ Draw Calls: %" PRIu64 " (avg %" PRIu64 ")", s_gs_stats.GetCounter(GSPerfMon::DrawCalls));
	DumpStatAndAvg("@HWSTAT@ Render Passes: %" PRIu64 " (avg %" PRIu64 ")", s_gs_stats.GetCounter(GSPerfMon::RenderPasses));
	DumpStatAndAvg("@HWSTAT@ Barriers: %" PRIu64 " (avg %" PRIu64 ")", s_gs_stats.GetCounter(GSPerfMon::Barriers));
	DumpStatAndAvg("@HWSTAT@ Copies: %" PRIu64 " (avg %" PRIu64 ")",  s_gs_stats.GetCounter(GSPerfMon::TextureCopies));
	DumpStatAndAvg("@HWSTAT@ Uploads: %" PRIu64 " (avg %" PRIu64 ")", s_gs_stats.GetCounter(GSPerfMon::TextureUploads));
	DumpStatAndAvg("@HWSTAT@ Readbacks: %" PRIu64 " (avg %" PRIu64 ")", s_gs_stats.GetCounter(GSPerfMon::Readbacks));
	DumpStatAndAvg("@HWSTAT@ Copies (ROV): %" PRIu64 " (avg %" PRIu64 ")", s_gs_stats.GetCounter(GSPerfMon::TextureCopiesROV));
	DumpStatAndAvg("@HWSTAT@ Draws Calls (ROV): %" PRIu64 " (avg %" PRIu64 ")", s_gs_stats.GetCounter(GSPerfMon::DrawCallsROV));
	DumpStatAndAvg("@HWSTAT@ Barriers (ROV): %" PRIu64 " (avg %" PRIu64 ")", s_gs_stats.GetCounter(GSPerfMon::BarriersROV));
	if (s_dump_perf_metrics)
	{
		Console.WriteLnFmt("@HWSTAT@ Minimum Frame Time: {:.3f} ms ({:.3f} FPS)", PerformanceMetrics::GetMinimumFrameTime(), 1000.0f / PerformanceMetrics::GetMinimumFrameTime());
		Console.WriteLnFmt("@HWSTAT@ Average Frame Time: {:.3f} ms ({:.3f} FPS)", PerformanceMetrics::GetAverageFrameTime(), 1000.0f / PerformanceMetrics::GetAverageFrameTime());
		Console.WriteLnFmt("@HWSTAT@ Maximum Frame Time: {:.3f} ms ({:.3f} FPS)", PerformanceMetrics::GetMaximumFrameTime(), 1000.0f / PerformanceMetrics::GetMaximumFrameTime());
		Console.WriteLnFmt("@HWSTAT@ Average CPU Thread Usage: {:.3f} %", s_perf_metrics.cpu_thread_usage / s_perf_metrics.num_updates);
		Console.WriteLnFmt("@HWSTAT@ Average GS Thread Usage: {:.3f} %", s_perf_metrics.gs_thread_usage / s_perf_metrics.num_updates);
		Console.WriteLnFmt("@HWSTAT@ Average GPU Usage: {:.3f} %", s_perf_metrics.gpu_usage / s_perf_metrics.num_updates);
		Console.WriteLnFmt("@HWSTAT@ Average CPU Thread Time: {:.3f} ms", s_perf_metrics.cpu_thread_time / s_perf_metrics.num_updates);
		Console.WriteLnFmt("@HWSTAT@ Average GS Thread Time: {:.3f} ms", s_perf_metrics.gs_thread_time / s_perf_metrics.num_updates);
		Console.WriteLnFmt("@HWSTAT@ Average GPU Time: {:.3f} ms", s_perf_metrics.gpu_time / s_perf_metrics.num_updates);
	}
	Console.WriteLnFmt("============================================");
}