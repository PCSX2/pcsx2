/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

// --------------------------------------------------------------------------------------
//  Source / Tracre Logging  (high volume logging facilities)
// --------------------------------------------------------------------------------------
// This module defines functions for performing high-volume diagnostic trace logging.
// Only ASCII versions of these logging functions are provided.  Translated messages are
// not supported, and typically all logs are written to disk (ASCII), thus making the
// ASCII versions the more efficient option.

#include "PrecompiledHeader.h"

#ifndef _WIN32
#include <sys/time.h>
#endif

#include <cstdarg>
#include <ctype.h>

#include "R3000A.h"
#include "iR5900.h"
#include "System.h"
#include "DebugTools/Debug.h"

#include "fmt/core.h"

using namespace R5900;

FILE* emuLog;
std::string emuLogName;

SysTraceLogPack SysTrace;
SysConsoleLogPack SysConsole;

// writes text directly to the logfile, no newlines appended.
void __Log(const char* fmt, ...)
{
	va_list list;
	va_start(list, fmt);

	if (emuLog != NULL)
	{
		std::vfprintf(emuLog, fmt, list);
		fputs("\n", emuLog);
		fflush(emuLog);
	}

	va_end(list);
}

void SysTraceLog::DoWrite(const char* msg) const
{
	if (emuLog == NULL)
		return;

	fputs(msg, emuLog);
	fputs("\n", emuLog);
	fflush(emuLog);
}

void SysTraceLog_EE::ApplyPrefix(std::string& ascii) const
{
	fmt::format_to(std::back_inserter(ascii), "{:<4}({:08x} {:08x}): ", ((SysTraceLogDescriptor*)m_Descriptor)->Prefix, cpuRegs.pc, cpuRegs.cycle);
}

void SysTraceLog_IOP::ApplyPrefix(std::string& ascii) const
{
	fmt::format_to(std::back_inserter(ascii), "{:<4}({:08x} {:08x}): ", ((SysTraceLogDescriptor*)m_Descriptor)->Prefix, psxRegs.pc, psxRegs.cycle);
}

void SysTraceLog_VIFcode::ApplyPrefix(std::string& ascii) const
{
	_parent::ApplyPrefix(ascii);
	ascii.append("vifCode_");
}

// --------------------------------------------------------------------------------------
//  SysConsoleLogPack  (descriptions)
// --------------------------------------------------------------------------------------
static const TraceLogDescriptor

	TLD_ELF = {
		"ELF", "E&LF",
		"Dumps detailed information for PS2 executables (ELFs)."},

	TLD_eeRecPerf = {"EErecPerf", "EErec &Performance", "Logs manual protection, split blocks, and other things that might impact performance."},

	TLD_eeConsole = {"EEout", "EE C&onsole", "Shows the game developer's logging text (EE processor)."},

	TLD_iopConsole = {"IOPout", "&IOP Console", "Shows the game developer's logging text (IOP processor)."},

	TLD_deci2 = {"DECI2", "DECI&2 Console", "Shows DECI2 debugging logs (EE processor)."},

	TLD_sysoutConsole = {"SYSout", "System Out", "Shows strings printed to the system output stream."},

	TLD_Pgif = {"PGIFout", "&PGIF Console", "Shows output from pgif the emulated ps1 gpu"},

	TLD_recordingConsole = {"Input Recording", "Input Recording Console", "Shows recording related logs and information."},

	TLD_controlInfo = {"Controller Info", "Controller Info", "Shows detailed controller input values for port 1, every frame."}
; // End init of TraceLogDescriptors

SysConsoleLogPack::SysConsoleLogPack()
	: ELF(&TLD_ELF, Color_Gray)
	, eeRecPerf(&TLD_eeRecPerf, Color_Gray)
	, sysoutConsole(&TLD_sysoutConsole, Color_Gray)
	, pgifLog(&TLD_Pgif)
	, eeConsole(&TLD_eeConsole)
	, iopConsole(&TLD_iopConsole)
	, deci2(&TLD_deci2)
	, recordingConsole(&TLD_recordingConsole)
	, controlInfo(&TLD_controlInfo)
{
}

// --------------------------------------------------------------------------------------
//  SysTraceLogPack  (descriptions)
// --------------------------------------------------------------------------------------
static const SysTraceLogDescriptor
	TLD_SIF = {
		"SIF", "SIF (EE <-> IOP)",
		"",
		"SIF"};

// ----------------------------
//   EmotionEngine (EE/R5900)
// ----------------------------

static const SysTraceLogDescriptor
	TLD_EE_Bios = {
		"Bios", "Bios",
		"SYSCALL and DECI2 activity.",
		"EE"},

	TLD_EE_Memory = {"Memory", "Memory", "Direct memory accesses to unknown or unmapped EE memory space.", "eMem"},

	TLD_EE_R5900 = {"R5900", "R5900 Core", "Disasm of executing core instructions (excluding COPs and CACHE).", "eDis"},

	TLD_EE_COP0 = {"COP0", "COP0", "Disasm of COP0 instructions (MMU, cpu and dma status, etc).", "eDis"},

	TLD_EE_COP1 = {"FPU", "COP1/FPU", "Disasm of the EE's floating point unit (FPU) only.", "eDis"},

	TLD_EE_COP2 = {"VUmacro", "COP2/VUmacro", "Disasm of the EE's VU0macro co-processor instructions.", "eDis"},

	TLD_EE_Cache = {"Cache", "Cache", "Execution of EE cache instructions.", "eDis"},

	TLD_EE_KnownHw = {"HwRegs", "Hardware Regs", "All known hardware register accesses (very slow!); not including sub filter options below.", "eReg"},

	TLD_EE_UnknownHw = {"UnknownRegs", "Unknown Regs", "Logs only unknown, unmapped, or unimplemented register accesses.", "eReg"},

	TLD_EE_DMAhw = {"DmaRegs", "DMA Regs", "Logs only DMA-related registers.", "eReg"},

	TLD_EE_IPU = {"IPU", "IPU", "IPU activity: hardware registers, decoding operations, DMA status, etc.", "IPU"},

	TLD_EE_GIFtag = {"GIFtags", "GIFtags", "All GIFtag parse activity; path index, tag type, etc.", "GIF"},

	TLD_EE_VIFcode = {"VIFcodes", "VIFcodes", "All VIFcode processing; command, tag style, interrupts.", "VIF"},

	TLD_EE_MSKPATH3 = {"MSKPATH3", "MSKPATH3", "All processing involved in Path3 Masking.", "MSKPATH3"},

	TLD_EE_SPR = {"MFIFO", "Scratchpad MFIFO", "Scratchpad's MFIFO activity.", "SPR"},

	TLD_EE_DMAC = {"DmaCtrl", "DMA Controller", "Actual data transfer logs, bus right arbitration, stalls, etc.", "eDmaC"},

	TLD_EE_Counters = {"Counters", "Counters", "Tracks all EE counters events and some counter register activity.", "eCnt"},

	TLD_EE_VIF = {"VIF", "VIF", "Dumps various VIF and VIFcode processing data.", "VIF"},

	TLD_EE_GIF = {"GIF", "GIF", "Dumps various GIF and GIFtag parsing data.", "GIF"};

// ----------------------------------
//   IOP - Input / Output Processor
// ----------------------------------

static const SysTraceLogDescriptor
	TLD_IOP_Bios = {
		"Bios", "Bios",
		"SYSCALL and IRX activity.",
		"IOP"},

	TLD_IOP_Memory = {"Memory", "Memory", "Direct memory accesses to unknown or unmapped IOP memory space.", "iMem"},

	TLD_IOP_R3000A = {"R3000A", "R3000A Core", "Disasm of executing core instructions (excluding COPs and CACHE).", "iDis"},

	TLD_IOP_COP2 = {"COP2/GPU", "COP2", "Disasm of the IOP's GPU co-processor instructions.", "iDis"},

	TLD_IOP_KnownHw = {"HwRegs", "Hardware Regs", "All known hardware register accesses, not including the sub-filters below.", "iReg"},

	TLD_IOP_UnknownHw = {"UnknownRegs", "Unknown Regs", "Logs only unknown, unmapped, or unimplemented register accesses.", "iReg"},

	TLD_IOP_DMAhw = {"DmaRegs", "DMA Regs", "Logs only DMA-related registers.", "iReg"},

	TLD_IOP_Memcards = {"Memorycards", "Memorycards", "Memorycard reads, writes, erases, terminators, and other processing.", "Mcd"},

	TLD_IOP_PAD = {"Pad", "Pad", "Gamepad activity on the SIO.", "Pad"},

	TLD_IOP_DMAC = {"DmaCrl", "DMA Controller", "Actual DMA event processing and data transfer logs.", "iDmaC"},

	TLD_IOP_Counters = {"Counters", "Counters", "Tracks all IOP counters events and some counter register activity.", "iCnt"},

	TLD_IOP_CDVD = {"CDVD", "CDVD", "Detailed logging of CDVD hardware.", "CDVD"},

	TLD_IOP_MDEC = {"MDEC", "MDEC", "Detailed logging of the Motion (FMV) Decoder hardware unit.", "MDEC"};

SysTraceLogPack::SysTraceLogPack()
	: SIF(&TLD_SIF)
{
}

SysTraceLogPack::EE_PACK::EE_PACK()
	: Bios(&TLD_EE_Bios)
	, Memory(&TLD_EE_Memory)
	, GIFtag(&TLD_EE_GIFtag)
	, VIFcode(&TLD_EE_VIFcode)
	, MSKPATH3(&TLD_EE_MSKPATH3)

	, R5900(&TLD_EE_R5900)
	, COP0(&TLD_EE_COP0)
	, COP1(&TLD_EE_COP1)
	, COP2(&TLD_EE_COP2)
	, Cache(&TLD_EE_Cache)

	, KnownHw(&TLD_EE_KnownHw)
	, UnknownHw(&TLD_EE_UnknownHw)
	, DMAhw(&TLD_EE_DMAhw)
	, IPU(&TLD_EE_IPU)

	, DMAC(&TLD_EE_DMAC)
	, Counters(&TLD_EE_Counters)
	, SPR(&TLD_EE_SPR)

	, VIF(&TLD_EE_VIF)
	, GIF(&TLD_EE_GIF)
{
}

SysTraceLogPack::IOP_PACK::IOP_PACK()
	: Bios(&TLD_IOP_Bios)
	, Memcards(&TLD_IOP_Memcards)
	, PAD(&TLD_IOP_PAD)

	, R3000A(&TLD_IOP_R3000A)
	, COP2(&TLD_IOP_COP2)
	, Memory(&TLD_IOP_Memory)

	, KnownHw(&TLD_IOP_KnownHw)
	, UnknownHw(&TLD_IOP_UnknownHw)
	, DMAhw(&TLD_IOP_DMAhw)

	, DMAC(&TLD_IOP_DMAC)
	, Counters(&TLD_IOP_Counters)
	, CDVD(&TLD_IOP_CDVD)
	, MDEC(&TLD_IOP_MDEC)
{
}
