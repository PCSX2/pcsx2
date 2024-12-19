// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// --------------------------------------------------------------------------------------
//  Source / Tracre Logging  (high volume logging facilities)
// --------------------------------------------------------------------------------------
// This module defines functions for performing high-volume diagnostic trace logging.
// Only ASCII versions of these logging functions are provided.  Translated messages are
// not supported, and typically all logs are written to disk (ASCII), thus making the
// ASCII versions the more efficient option.

#include "DebugTools/Debug.h"
#include "R3000A.h"
#include "R5900.h"

#include "fmt/core.h"

#include <cctype>
#include <cstdarg>

#ifndef _WIN32
#include <sys/time.h>
#endif

using namespace R5900;

TraceLogPack TraceLogging;
ConsoleLogPack ConsoleLogging;

bool TraceLog::Write(const char* fmt, ...) const
{
	auto prefixed_str = fmt::format("{:<8}: {}", Descriptor.Prefix, fmt);
	va_list args;
	va_start(args, fmt);
	Log::Writev(LOGLEVEL_TRACE, Color, prefixed_str.c_str(), args);
	va_end(args);

	return false;
}

bool TraceLog::Write(ConsoleColors color, const char* fmt, ...) const
{
	auto prefixed_str = fmt::format("{:<8}: {}", Descriptor.Prefix, fmt);
	va_list args;
	va_start(args, fmt);
	Log::Writev(LOGLEVEL_TRACE, color, prefixed_str.c_str(), args);
	va_end(args);

	return false;
}

bool ConsoleLog::Write(const char* fmt, ...) const
{
	auto prefixed_str = fmt::format("{:<8}: {}", Descriptor.Prefix, fmt);
	va_list args;
	va_start(args, fmt);
	Console.WriteLn(Color, prefixed_str.c_str(), args);
	va_end(args);

	return false;
}

bool ConsoleLog::Write(ConsoleColors color, const char* fmt, ...) const
{
	auto prefixed_str = fmt::format("{:<8}: {}", Descriptor.Prefix, fmt);

	va_list args;
	va_start(args, fmt);
	Console.WriteLn(color, prefixed_str.c_str(), args);
	va_end(args);

	return false;
}

// --------------------------------------------------------------------------------------
//  ConsoleLogPack  (descriptions)
// --------------------------------------------------------------------------------------
static const LogDescriptor

	LD_ELF = {
		"ELF", "E&LF",
		"Dumps detailed information for PS2 executables (ELFs)."},

	LD_eeRecPerf = {"EErecPerf", "EErec &Performance", "Logs manual protection, split blocks, and other things that might impact performance."},

	LD_eeConsole = {"EEout", "EE C&onsole", "Shows the game developer's logging text (EE processor)."},

	LD_iopConsole = {"IOPout", "&IOP Console", "Shows the game developer's logging text (IOP processor)."},

	LD_deci2 = {"DECI2", "DECI&2 Console", "Shows DECI2 debugging logs (EE processor)."},

	LD_Pgif = {"PGIFout", "&PGIF Console", "Shows output from pgif the emulated ps1 gpu"},

	LD_recordingConsole = {"Input Recording", "Input Recording Console", "Shows recording related logs and information."},

	LD_controlInfo = {"Controller Info", "Controller Info", "Shows detailed controller input values for port 1, every frame."};

ConsoleLogPack::ConsoleLogPack()
	: ELF(LD_ELF, Color_Gray)
	, eeRecPerf(LD_eeRecPerf, Color_Gray)
	, pgifLog(LD_Pgif)
	, eeConsole(LD_eeConsole)
	, iopConsole(LD_iopConsole)
	, deci2(LD_deci2)
	, recordingConsole(LD_recordingConsole)
	, controlInfo(LD_controlInfo)
{
}

// --------------------------------------------------------------------------------------
//  TraceLogPack  (descriptions)
// --------------------------------------------------------------------------------------
static const LogDescriptor
	LD_SIF = {"SIF", "SIF (EE <-> IOP)", ""};

// ----------------------------
//   EmotionEngine (EE/R5900)
// ----------------------------

static const LogDescriptor
	LD_EE_Bios = {"Bios", "Bios", "SYSCALL and DECI2 activity."},

	LD_EE_Memory = {"Memory", "Memory", "Direct memory accesses to unknown or unmapped EE memory space."},

	LD_EE_R5900 = {"R5900", "R5900 Core", "Disasm of executing core instructions (excluding COPs and CACHE)."},

	LD_EE_COP0 = {"COP0", "COP0", "Disasm of COP0 instructions (MMU, cpu and dma status, etc)."},

	LD_EE_COP1 = {"FPU", "COP1/FPU", "Disasm of the EE's floating point unit (FPU) only."},

	LD_EE_COP2 = {"VUmacro", "COP2/VUmacro", "Disasm of the EE's VU0macro co-processor instructions."},

	LD_EE_Cache = {"Cache", "Cache", "Execution of EE cache instructions."},

	LD_EE_KnownHw = {"HwRegs", "Hardware Regs", "All known hardware register accesses (very slow!); not including sub filter options below."},

	LD_EE_UnknownHw = {"UnknownRegs", "Unknown Regs", "Logs only unknown, unmapped, or unimplemented register accesses."},

	LD_EE_DMAhw = {"DmaRegs", "DMA Regs", "Logs only DMA-related registers."},

	LD_EE_IPU = {"IPU", "IPU", "IPU activity: hardware registers, decoding operations, DMA status, etc."},

	LD_EE_GIFtag = {"GIFtags", "GIFtags", "All GIFtag parse activity; path index, tag type, etc."},

	LD_EE_VIFcode = {"VIFcodes", "VIFcodes", "All VIFcode processing; command, tag style, interrupts."},

	LD_EE_MSKPATH3 = {"MSKPATH3", "MSKPATH3", "All processing involved in Path3 Masking."},

	LD_EE_SPR = {"MFIFO", "Scratchpad MFIFO", "Scratchpad's MFIFO activity."},

	LD_EE_DMAC = {"DmaCtrl", "DMA Controller", "Actual data transfer logs, bus right arbitration, stalls, etc."},

	LD_EE_Counters = {"Counters", "Counters", "Tracks all EE counters events and some counter register activity."},

	LD_EE_VIF = {"VIF", "VIF", "Dumps various VIF and VIFcode processing data."},

	LD_EE_GIF = {"GIF", "GIF", "Dumps various GIF and GIFtag parsing data."};

// ----------------------------------
//   IOP - Input / Output Processor
// ----------------------------------

static const LogDescriptor
	LD_IOP_Bios = {"Bios", "Bios", "SYSCALL and IRX activity."},

	LD_IOP_Memory = {"Memory", "Memory", "Direct memory accesses to unknown or unmapped IOP memory space."},

	LD_IOP_R3000A = {"R3000A", "R3000A Core", "Disasm of executing core instructions (excluding COPs and CACHE)."},

	LD_IOP_COP2 = {"COP2/GPU", "COP2", "Disasm of the IOP's GPU co-processor instructions."},

	LD_IOP_KnownHw = {"HwRegs", "Hardware Regs", "All known hardware register accesses, not including the sub-filters below."},

	LD_IOP_UnknownHw = {"UnknownRegs", "Unknown Regs", "Logs only unknown, unmapped, or unimplemented register accesses."},

	LD_IOP_DMAhw = {"DmaRegs", "DMA Regs", "Logs only DMA-related registers."},

	LD_IOP_Memcards = {"Memorycards", "Memorycards", "Memorycard reads, writes, erases, terminators, and other processing."},

	LD_IOP_PAD = {"Pad", "Pad", "Gamepad activity on the SIO."},

	LD_IOP_DMAC = {"DmaCtrl", "DMA Controller", "Actual DMA event processing and data transfer logs."},

	LD_IOP_Counters = {"Counters", "Counters", "Tracks all IOP counters events and some counter register activity."},

	LD_IOP_CDVD = {"CDVD", "CDVD", "Detailed logging of CDVD hardware."},

	LD_IOP_MDEC = {"MDEC", "MDEC", "Detailed logging of the Motion (FMV) Decoder hardware unit."};

TraceLogPack::TraceLogPack()
	: SIF(LD_SIF)
{
}

TraceLogPack::EE_PACK::EE_PACK()
	: Bios(LD_EE_Bios)
	, Memory(LD_EE_Memory)
	, GIFtag(LD_EE_GIFtag)
	, VIFcode(LD_EE_VIFcode)
	, MSKPATH3(LD_EE_MSKPATH3)

	, R5900(LD_EE_R5900)
	, COP0(LD_EE_COP0)
	, COP1(LD_EE_COP1)
	, COP2(LD_EE_COP2)
	, Cache(LD_EE_Cache)

	, KnownHw(LD_EE_KnownHw)
	, UnknownHw(LD_EE_UnknownHw)
	, DMAhw(LD_EE_DMAhw)
	, IPU(LD_EE_IPU)

	, DMAC(LD_EE_DMAC)
	, Counters(LD_EE_Counters)
	, SPR(LD_EE_SPR)

	, VIF(LD_EE_VIF)
	, GIF(LD_EE_GIF)
{
}

TraceLogPack::IOP_PACK::IOP_PACK()
	: Bios(LD_IOP_Bios)
	, Memcards(LD_IOP_Memcards)
	, PAD(LD_IOP_PAD)

	, R3000A(LD_IOP_R3000A)
	, COP2(LD_IOP_COP2)
	, Memory(LD_IOP_Memory)

	, KnownHw(LD_IOP_KnownHw)
	, UnknownHw(LD_IOP_UnknownHw)
	, DMAhw(LD_IOP_DMAhw)

	, DMAC(LD_IOP_DMAC)
	, Counters(LD_IOP_Counters)
	, CDVD(LD_IOP_CDVD)
	, MDEC(LD_IOP_MDEC)
{
}
