// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Console.h"
#include "Config.h"
#include "Memory.h"

#include <string>

extern char* disVU0MicroUF(u32 code, u32 pc);
extern char* disVU0MicroLF(u32 code, u32 pc);
extern char* disVU1MicroUF(u32 code, u32 pc);
extern char* disVU1MicroLF(u32 code, u32 pc);

namespace R5900
{
	void disR5900Fasm(std::string& output, u32 code, u32 pc, bool simplify = false);

	extern const char* const GPR_REG[32];
	extern const char* const COP0_REG[32];
	extern const char* const COP1_REG_FP[32];
	extern const char* const COP1_REG_FCR[32];
	extern const char* const COP2_REG_FP[32];
	extern const char* const COP2_REG_CTL[32];
	extern const char* const COP2_VFnames[4];
	extern const char* const GS_REG_PRIV[19];
	extern const u32 GS_REG_PRIV_ADDR[19];
}

namespace R3000A
{
	extern void (*IOP_DEBUG_BSC[64])(char *buf);

	extern const char * const disRNameGPR[];
	extern char* disR3000AF(u32 code, u32 pc);
}

struct LogDescriptor
{
	std::string Prefix;
	std::string Name;
	std::string Description;
};

struct LogBase
{
	const LogDescriptor& Descriptor;
	ConsoleColors Color;
	bool Enabled = false;
	LogBase(const LogDescriptor& descriptor, ConsoleColors color = Color_Gray)
		: Descriptor(descriptor)
		, Color(color) {};
};

// --------------------------------------------------------------------------------------
//  TraceLog
// --------------------------------------------------------------------------------------
struct TraceLog : public LogBase
{
	TraceLog(const LogDescriptor& descriptor, ConsoleColors color = Color_Gray)
		: LogBase(descriptor, color) {};

	bool Write(const char* fmt, ...) const;
	bool Write(ConsoleColors color, const char* fmt, ...) const;
	bool IsActive() const
	{
		return EmuConfig.Trace.Enabled && Enabled;
	}
};

struct ConsoleLog : public LogBase
{
	ConsoleLog(const LogDescriptor& descriptor, ConsoleColors color = Color_Gray)
		: LogBase(descriptor, color) {};

	bool Write(const char* fmt, ...) const;
	bool Write(ConsoleColors color, const char* fmt, ...) const;
	bool IsActive() const
	{
		return Enabled;
	}
};

// --------------------------------------------------------------------------------------
//  ConsoleLogFromVM
// --------------------------------------------------------------------------------------
// Special console logger for Virtual Machine log sources, such as the EE and IOP console
// writes (actual game developer messages and such).  These logs do *not* automatically
// append newlines, since the VM generates them manually; and they do *not* support printf
// formatting, since anything coming over the EE/IOP consoles should be considered raw
// string data.  (otherwise %'s would get mis-interpreted).
//
template< ConsoleColors conColor >
class ConsoleLogFromVM : public LogBase
{
public:
	ConsoleLogFromVM(const LogDescriptor& descriptor) : LogBase(descriptor, conColor) {};

	bool Write(std::string_view msg)
	{
		for (const char ch : msg)
		{
			if (ch == '\n')
			{
				if (!m_buffer.empty())
				{
					Console.WriteLn(conColor, m_buffer);
					m_buffer.clear();
				}
			}
			else if (ch < 0x20)
			{
				// Ignore control characters.
				// Otherwise you get fun bells going off.
			}
			else
			{
				m_buffer.push_back(ch);
			}
		}

		return false;
	}

	bool IsActive()
	{
		return Enabled;
	}

private:
	std::string m_buffer;
};

// --------------------------------------------------------------------------------------
//  TraceLogPack
// --------------------------------------------------------------------------------------
struct TraceLogPack
{
	TraceLog	SIF;
	struct EE_PACK
	{
		TraceLog Bios;
		TraceLog Memory;
		TraceLog GIFtag;
		TraceLog VIFcode;
		TraceLog MSKPATH3;

		TraceLog R5900;
		TraceLog COP0;
		TraceLog COP1;
		TraceLog COP2;
		TraceLog Cache;

		TraceLog KnownHw;
		TraceLog UnknownHw;
		TraceLog DMAhw;
		TraceLog IPU;

		TraceLog DMAC;
		TraceLog Counters;
		TraceLog SPR;

		TraceLog VIF;
		TraceLog GIF;

		EE_PACK();
	} EE;

	struct IOP_PACK
	{
		TraceLog Bios;
		TraceLog Memcards;
		TraceLog PAD;

		TraceLog R3000A;
		TraceLog COP2;
		TraceLog Memory;

		TraceLog KnownHw;
		TraceLog UnknownHw;
		TraceLog DMAhw;

		// TODO items to be added, or removed?  I can't remember which! --air
		//TraceLog_IOP_Registers	SPU2;
		//TraceLog_IOP_Registers	USB;
		//TraceLog_IOP_Registers	FW;

		TraceLog DMAC;
		TraceLog Counters;
		TraceLog CDVD;
		TraceLog MDEC;

		IOP_PACK();
	} IOP;

	TraceLogPack();
};

struct ConsoleLogPack
{
	ConsoleLog ELF;
	ConsoleLog eeRecPerf;
	ConsoleLog pgifLog;

	ConsoleLogFromVM<Color_Cyan>		eeConsole;
	ConsoleLogFromVM<Color_Yellow>		iopConsole;
	ConsoleLogFromVM<Color_Cyan>		deci2;
	ConsoleLogFromVM<Color_StrongMagenta>	recordingConsole;
	ConsoleLogFromVM<Color_Red>				controlInfo;

	ConsoleLogPack();
};


extern TraceLogPack TraceLogging;
extern ConsoleLogPack ConsoleLogging;

// Helper macro for cut&paste.  Note that we intentionally use a top-level *inline* bitcheck
// against Trace.Enabled, to avoid extra overhead in Debug builds when logging is disabled.
// (specifically this allows debug builds to skip havingto resolve all the parameters being
//  passed into the function)
#ifdef PCSX2_DEVBUILD
#	define TraceActive(trace)	TraceLogging.trace.IsActive()
#else
#	define TraceActive(trace)	(false)
#endif

#define macTrace(trace)	TraceActive(trace) && TraceLogging.trace.Write

#define SIF_LOG			macTrace(SIF)

#define BIOS_LOG		macTrace(EE.Bios)
#define CPU_LOG			macTrace(EE.R5900)
#define COP0_LOG		macTrace(EE.COP0)
#define VUM_LOG			macTrace(EE.COP2)
#define MEM_LOG			macTrace(EE.Memory)
#define CACHE_LOG		macTrace(EE.Cache)
#define HW_LOG			macTrace(EE.KnownHw)
#define UnknownHW_LOG	macTrace(EE.UnknownHw)
#define DMA_LOG			macTrace(EE.DMAhw)
#define IPU_LOG			macTrace(EE.IPU)
#define VIF_LOG			macTrace(EE.VIF)
#define SPR_LOG			macTrace(EE.SPR)
#define GIF_LOG			macTrace(EE.GIF)
#define MSKPATH3_LOG	macTrace(EE.MSKPATH3)
#define EECNT_LOG		macTrace(EE.Counters)
#define VifCodeLog		macTrace(EE.VIFcode)
#define GifTagLog		macTrace(EE.GIFtag)


#define PSXBIOS_LOG		macTrace(IOP.Bios)
#define PSXCPU_LOG		macTrace(IOP.R3000A)
#define PSXMEM_LOG		macTrace(IOP.Memory)
#define PSXHW_LOG		macTrace(IOP.KnownHw)
#define PSXUnkHW_LOG	macTrace(IOP.UnknownHw)
#define PSXDMA_LOG		macTrace(IOP.DMAhw)
#define PSXCNT_LOG		macTrace(IOP.Counters)
#define MEMCARDS_LOG	macTrace(IOP.Memcards)
#define PAD_LOG			macTrace(IOP.PAD)
#define GPU_LOG			macTrace(IOP.GPU)
#define CDVD_LOG		macTrace(IOP.CDVD)
#define MDEC_LOG		macTrace(IOP.MDEC)


#define ELF_LOG         ConsoleLogging.ELF.IsActive()              && ConsoleLogging.ELF.Write
#define eeRecPerfLog    ConsoleLogging.eeRecPerf.IsActive()        && ConsoleLogging.eeRecPerf
#define eeConLog        ConsoleLogging.eeConsole.IsActive()        && ConsoleLogging.eeConsole.Write
#define eeDeci2Log      ConsoleLogging.deci2.IsActive()            && ConsoleLogging.deci2.Write
#define iopConLog       ConsoleLogging.iopConsole.IsActive()       && ConsoleLogging.iopConsole.Write
#define pgifConLog      ConsoleLogging.pgifLog.IsActive()          && ConsoleLogging.pgifLog.Write
#define recordingConLog ConsoleLogging.recordingConsole.IsActive() && ConsoleLogging.recordingConsole.Write
#define controlLog      ConsoleLogging.controlInfo.IsActive()      && ConsoleLogging.controlInfo.Write
