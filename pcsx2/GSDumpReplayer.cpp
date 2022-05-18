/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include <atomic>

#include "fmt/core.h"

#include "common/FileSystem.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "imgui.h"

// Has to come before Gif.h
#include "MemoryTypes.h"

#include "Frontend/ImGuiManager.h"
#include "Frontend/GameList.h"
#include "Gif.h"
#include "Gif_Unit.h"
#include "GSDumpReplayer.h"
#include "GS/GSLzma.h"
#include "GS.h"
#include "Host.h"
#include "R3000A.h"
#include "R5900.h"
#include "VMManager.h"
#include "VUmicro.h"

static void GSDumpReplayerCpuReserve();
static void GSDumpReplayerCpuShutdown();
static void GSDumpReplayerCpuReset();
static void GSDumpReplayerCpuStep();
static void GSDumpReplayerCpuExecute();
static void GSDumpReplayerCpuCheckExecutionState();
static void GSDumpReplayerCpuThrowException(const BaseException& ex);
static void GSDumpReplayerCpuThrowCpuException(const BaseR5900Exception& ex);
static void GSDumpReplayerCpuClear(u32 addr, u32 size);
static uint GSDumpReplayerCpuGetCacheReserve();
static void GSDumpReplayerCpuSetCacheReserve(uint reserveInMegs);

static std::unique_ptr<GSDumpFile> s_dump_file;
static u32 s_current_packet = 0;
static u32 s_dump_frame_number = 0;
static bool s_dump_running = false;
static bool s_needs_state_loaded = false;
static u64 s_frame_ticks = 0;
static u64 s_next_frame_time = 0;

R5900cpu GSDumpReplayerCpu = {
	GSDumpReplayerCpuReserve,
	GSDumpReplayerCpuShutdown,
	GSDumpReplayerCpuReset,
	GSDumpReplayerCpuStep,
	GSDumpReplayerCpuExecute,
	GSDumpReplayerCpuCheckExecutionState,
	GSDumpReplayerCpuThrowException,
	GSDumpReplayerCpuThrowCpuException,
	GSDumpReplayerCpuClear,
	GSDumpReplayerCpuGetCacheReserve,
	GSDumpReplayerCpuSetCacheReserve};

static InterpVU0 gsDumpVU0;
static InterpVU1 gsDumpVU1;

bool GSDumpReplayer::IsReplayingDump()
{
	return static_cast<bool>(s_dump_file);
}

bool GSDumpReplayer::Initialize(const char* filename)
{
	Common::Timer timer;
	Console.WriteLn("(GSDumpReplayer) Reading file...");

	s_dump_file = GSDumpFile::OpenGSDump(filename);
	if (!s_dump_file || !s_dump_file->ReadFile())
	{
		Host::ReportFormattedErrorAsync("GSDumpReplayer", "Failed to open or read '%s'.", filename);
		s_dump_file.reset();
		return false;
	}

	Console.WriteLn("(GSDumpReplayer) Read file in %.2f ms.", timer.GetTimeMilliseconds());

	// We replace all CPUs.
	Cpu = &GSDumpReplayerCpu;
	psxCpu = &psxInt;
	CpuVU0 = &gsDumpVU0;
	CpuVU1 = &gsDumpVU1;

	return true;
}

void GSDumpReplayer::Reset()
{
	GSDumpReplayerCpuReset();
}

void GSDumpReplayer::Shutdown()
{
	Console.WriteLn("(GSDumpReplayer) Shutting down.");

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
}

static void GSDumpReplayerLoadInitialState()
{
	// reset GS registers to initial dump values
	std::memcpy(PS2MEM_GS, s_dump_file->GetRegsData().data(),
		std::min(Ps2MemSize::GSregs, static_cast<u32>(s_dump_file->GetRegsData().size())));

	// load GS state
	freezeData fd = {static_cast<int>(s_dump_file->GetStateData().size()),
		const_cast<u8*>(s_dump_file->GetStateData().data())};
	MTGS_FreezeData mfd = {&fd, 0};
	GetMTGS().Freeze(FreezeAction::Load, mfd);
	if (mfd.retval != 0)
		Host::ReportFormattedErrorAsync("GSDumpReplayer", "Failed to load GS state.");
}

static void GSDumpReplayerSendPacketToMTGS(GIF_PATH path, const u8* data, u32 length)
{
	pxAssert((length % 16) == 0);

	Gif_Path& gifPath = gifUnit.gifPath[path];
	gifPath.CopyGSPacketData(const_cast<u8*>(data), length);

	GS_Packet gsPack;
	gsPack.offset = gifPath.curOffset;
	gsPack.size = length;
	gifPath.curOffset += length;
	Gif_AddCompletedGSPacket(gsPack, path);
}

static void GSDumpReplayerUpdateFrameLimit()
{
	constexpr u32 default_frame_limit = 60;
	const u32 frame_limit = static_cast<u32>(default_frame_limit * EmuConfig.GS.LimitScalar);

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
		Threading::Sleep(sleep / ms);
	while ((now = GetCPUTicks()) < s_next_frame_time)
		ShortSpin();
	s_next_frame_time = std::max(now, s_next_frame_time + s_frame_ticks);
}

void GSDumpReplayerCpuStep()
{
	if (s_needs_state_loaded)
	{
		GSDumpReplayerLoadInitialState();
		s_needs_state_loaded = false;
	}

	const GSDumpFile::GSData& packet = s_dump_file->GetPackets()[s_current_packet];
	s_current_packet = (s_current_packet + 1) % static_cast<u32>(s_dump_file->GetPackets().size());
	if (s_current_packet == 0)
		s_dump_frame_number = 0;

	switch (packet.id)
	{
		case GSDumpTypes::GSType::Transfer:
		{
			switch (packet.path)
			{
				case GSDumpTypes::GSTransferPath::Path1Old:
				{
					std::unique_ptr<u8[]> data(new u8[16384]);
					const s32 addr = 16384 - packet.length;
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
			GSDumpReplayerCpuCheckExecutionState();
			GSDumpReplayerUpdateFrameLimit();
			GSDumpReplayerFrameLimit();
			GetMTGS().PostVsyncStart(false);
			VMManager::Internal::VSyncOnCPUThread();
		}
		break;

		case GSDumpTypes::GSType::ReadFIFO2:
		{
			u32 size;
			std::memcpy(&size, packet.data, sizeof(size));

			std::unique_ptr<u8[]> arr(new u8[size * 16]);
			GetMTGS().InitAndReadFIFO(arr.get(), size);
		}
		break;

		case GSDumpTypes::GSType::Registers:
		{
			std::memcpy(PS2MEM_GS, packet.data, std::min<s32>(packet.length, Ps2MemSize::GSregs));
		}
		break;
	}
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

void GSDumpReplayerCpuCheckExecutionState()
{
	if (VMManager::Internal::IsExecutionInterrupted())
		s_dump_running = false;
}

void GSDumpReplayerCpuThrowException(const BaseException& ex)
{
}

void GSDumpReplayerCpuThrowCpuException(const BaseR5900Exception& ex)
{
}

void GSDumpReplayerCpuClear(u32 addr, u32 size)
{
}

uint GSDumpReplayerCpuGetCacheReserve()
{
	return 0;
}

void GSDumpReplayerCpuSetCacheReserve(uint reserveInMegs)
{
}

void GSDumpReplayer::RenderUI()
{
	const float scale = ImGuiManager::GetGlobalScale();
	const float shadow_offset = std::ceil(1.0f * scale);
	const float margin = std::ceil(10.0f * scale);
	const float spacing = std::ceil(5.0f * scale);
	float position_y = margin;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	ImFont* font = ImGuiManager::GetFixedFont();
	std::string text;
	ImVec2 text_size;
	text.reserve(128);

#define DRAW_LINE(font, text, color) \
	do \
	{ \
		text_size = font->CalcTextSizeA(font->FontSize, std::numeric_limits<float>::max(), -1.0f, (text), nullptr, nullptr); \
		dl->AddText(font, font->FontSize, ImVec2(margin + shadow_offset, position_y + shadow_offset), IM_COL32(0, 0, 0, 100), (text)); \
		dl->AddText(font, font->FontSize, ImVec2(margin, position_y), color, (text)); \
		position_y += text_size.y + spacing; \
	} while (0)

	fmt::format_to(std::back_inserter(text), "Dump Frame: {}", s_dump_frame_number);
	DRAW_LINE(font, text.c_str(), IM_COL32(255, 255, 255, 255));

	text.clear();
	fmt::format_to(std::back_inserter(text), "Packet Number: {}/{}", s_current_packet, static_cast<u32>(s_dump_file->GetPackets().size()));
	DRAW_LINE(font, text.c_str(), IM_COL32(255, 255, 255, 255));

#undef DRAW_LINE
}
