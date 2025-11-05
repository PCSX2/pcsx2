// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Subsystems.h"
#include "common/StringUtil.h"

#include <array>

namespace Subsystem
{

const char* GetName(Type subsystem)
{
	static constexpr std::array<const char*, 23> names = {
		"None",
		"GS", "GIF",
		"VIF0", "VIF1", "VU0", "VU1",
		"DMA", "IPU",
		"SPU2",
		"CDVD", "USB", "DEV9", "PAD", "SIO", "SIO2",
		"BIOS", "INTC", "DMAC", "TIMER",
		"SIF", "SPR"
	};

	const size_t index = static_cast<size_t>(subsystem);
	if (index < names.size())
		return names[index];
	return "Unknown";
}

Type DetectFromMemoryAddress(u64 addr, bool is_write)
{
	// EE Hardware Register Range (0x10000000 - 0x1000FFFF)
	if (addr >= 0x10000000 && addr < 0x10010000)
	{
		// Timers/Counters (0x10000000 - 0x10002000)
		if (addr < 0x10002000)
			return Type::TIMER;

		// IPU (Image Processing Unit) (0x10002000 - 0x10003000)
		if (addr >= 0x10002000 && addr < 0x10003000)
			return Type::IPU;

		// GIF (Graphics Interface) (0x10003000 - 0x10003800)
		if (addr >= 0x10003000 && addr < 0x10003800)
			return Type::GIF;

		// VIF0 (0x10003800 - 0x10003C00)
		if (addr >= 0x10003800 && addr < 0x10003C00)
			return Type::VIF0;

		// VIF1 (0x10003C00 - 0x10004000)
		if (addr >= 0x10003C00 && addr < 0x10004000)
			return Type::VIF1;

		// FIFO regions (0x10004000 - 0x10008000) - map to respective subsystems
		if (addr >= 0x10004000 && addr < 0x10005000)
			return Type::VIF0;  // VIF0 FIFO
		if (addr >= 0x10005000 && addr < 0x10006000)
			return Type::VIF1;  // VIF1 FIFO
		if (addr >= 0x10006000 && addr < 0x10007000)
			return Type::GIF;   // GIF FIFO
		if (addr >= 0x10007000 && addr < 0x10008000)
			return Type::IPU;   // IPU FIFO

		// DMA Channel Registers (0x10008000 - 0x1000E000)
		if (addr >= 0x10008000 && addr < 0x1000E000)
			return Type::DMA;

		// DMAC Control Registers (0x1000E000 - 0x1000F000)
		if (addr >= 0x1000E000 && addr < 0x1000F000)
			return Type::DMAC;

		// INTC (Interrupt Controller) (0x1000F000 - 0x1000F100)
		if (addr >= 0x1000F000 && addr < 0x1000F100)
			return Type::INTC;

		// SIO (0x1000F100 - 0x1000F200)
		if (addr >= 0x1000F100 && addr < 0x1000F200)
			return Type::SIO;

		// SBUS/SIF (0x1000F200 - 0x1000F400)
		if (addr >= 0x1000F200 && addr < 0x1000F400)
			return Type::SIF;
	}

	// GS Privileged Registers (0x12000000 - 0x12002000)
	if (addr >= 0x12000000 && addr < 0x12002000)
		return Type::GS;

	// VU0 Memory (0x11000000 - 0x11004000)
	if (addr >= 0x11000000 && addr < 0x11004000)
		return Type::VU0;

	// VU1 Memory (0x11004000 - 0x11010000)
	if (addr >= 0x11004000 && addr < 0x11010000)
		return Type::VU1;

	// IOP Hardware Register Range (0x1F801000 - 0x1F810000)
	if (addr >= 0x1F801000 && addr < 0x1F810000)
	{
		// CDROM (0x1F801800 - 0x1F801900)
		if (addr >= 0x1F801800 && addr < 0x1F801900)
			return Type::CDVD;

		// USB (0x1F801600 - 0x1F801700)
		if (addr >= 0x1F801600 && addr < 0x1F801700)
			return Type::USB;

		// SPU2 (0x1F801C00 - 0x1F801E00)
		if (addr >= 0x1F801C00 && addr < 0x1F801E00)
			return Type::SPU2;

		// SIO2 (0x1F808260 - 0x1F808280)
		if (addr >= 0x1F808260 && addr < 0x1F808280)
			return Type::SIO2;

		// DEV9 (around 0x1F80146E and other addresses)
		if ((addr >= 0x1F801460 && addr < 0x1F801470) ||
			(addr >= 0x1F801400 && addr < 0x1F801430))
			return Type::DEV9;

		// IOP DMA (0x1F801080 - 0x1F801560)
		if ((addr >= 0x1F801080 && addr < 0x1F801100) ||
			(addr >= 0x1F801500 && addr < 0x1F801560))
			return Type::DMA;

		// SIO (0x1F801040 - 0x1F801050)
		if (addr >= 0x1F801040 && addr < 0x1F801050)
			return Type::SIO;

		// INTC (0x1F801070 - 0x1F80107C)
		if (addr >= 0x1F801070 && addr < 0x1F80107C)
			return Type::INTC;
	}

	return Type::None;
}

Type DetectFromSyscall(u32 opcode, u32 v1_register)
{
	// MIPS syscall instruction: opcode = 0x0000000C (in lower 6 bits: 001100)
	const u32 func = opcode & 0x3F;  // Extract function field (bits 0-5)
	if (func == 0x0C)  // syscall instruction
	{
		// v1 register contains syscall number
		// Any syscall detected = BIOS subsystem
		return Type::BIOS;
	}

	return Type::None;
}

std::string GetDMAChannelName(u64 addr)
{
	// EE DMA Channels (0x10008000 + channel * 0x1000)
	if (addr >= 0x10008000 && addr < 0x1000E000)
	{
		const u32 channel = (addr - 0x10008000) / 0x1000;
		static constexpr std::array<const char*, 10> ee_dma_names = {
			"VIF0", "VIF1", "GIF", "IPU_FROM", "IPU_TO",
			"SIF0", "SIF1", "SIF2", "SPR_FROM", "SPR_TO"
		};

		if (channel < ee_dma_names.size())
			return StringUtil::StdStringFromFormat("DMA CH%u (%s)", channel, ee_dma_names[channel]);
		return StringUtil::StdStringFromFormat("DMA CH%u", channel);
	}

	// IOP DMA Channels
	if ((addr >= 0x1F801080 && addr < 0x1F801100) ||
		(addr >= 0x1F801500 && addr < 0x1F801560))
	{
		// IOP DMA channel mapping is more complex, but we can infer some
		static constexpr std::array<const char*, 13> iop_dma_names = {
			"MDEC_IN", "MDEC_OUT", "GPU", "CDROM",
			"SPU", "PIO", "OTC",
			"SPU2", "DEV9", "SIF0", "SIF1", "SIO2_IN", "SIO2_OUT"
		};

		u32 channel = 0;
		if (addr >= 0x1F801080 && addr < 0x1F801100)
			channel = (addr - 0x1F801080) / 0x10;
		else if (addr >= 0x1F801500 && addr < 0x1F801560)
			channel = 7 + (addr - 0x1F801500) / 0x10;

		if (channel < iop_dma_names.size())
			return StringUtil::StdStringFromFormat("IOP DMA CH%u (%s)", channel, iop_dma_names[channel]);
		return StringUtil::StdStringFromFormat("IOP DMA CH%u", channel);
	}

	return "";
}

std::string GetDetailString(Type subsystem, const DetectionContext& ctx)
{
	switch (subsystem)
	{
		case Type::DMA:
		{
			std::string dma_name = GetDMAChannelName(ctx.mem_addr);
			if (!dma_name.empty())
				return dma_name + (ctx.is_write ? " write" : " read");
			return ctx.is_write ? "DMA write" : "DMA read";
		}

		case Type::BIOS:
		{
			// Common EE BIOS syscalls
			const u32 syscall_num = ctx.v1_register;
			switch (syscall_num)
			{
				case 4:  return "BIOS syscall 4 (RFU004)";
				case 6:  return "BIOS syscall 6 (LoadExecPS2)";
				case 7:  return "BIOS syscall 7 (ExecPS2)";
				case 64: return "BIOS syscall 64 (FlushCache)";
				case 76: return "BIOS syscall 76 (Exit)";
				default: return StringUtil::StdStringFromFormat("BIOS syscall %u", syscall_num);
			}
		}

		case Type::GIF:
			return ctx.is_write ? "GIF transfer (to GS)" : "GIF read";

		case Type::VIF0:
			return ctx.is_write ? "VIF0 upload (VU0 data)" : "VIF0 read";

		case Type::VIF1:
			return ctx.is_write ? "VIF1 upload (VU1 data)" : "VIF1 read";

		case Type::GS:
			return ctx.is_write ? "GS register write" : "GS register read";

		case Type::SPU2:
			return ctx.is_write ? "SPU2 write (audio)" : "SPU2 read";

		case Type::CDVD:
			return ctx.is_write ? "CDVD command" : "CDVD read";

		case Type::IPU:
			return ctx.is_write ? "IPU write (video)" : "IPU read";

		case Type::SIO2:
			return ctx.is_write ? "SIO2 write (PAD/MC)" : "SIO2 read";

		case Type::USB:
			return ctx.is_write ? "USB write" : "USB read";

		case Type::DEV9:
			return ctx.is_write ? "DEV9 write (network)" : "DEV9 read";

		default:
			return std::string(GetName(subsystem));
	}
}

} // namespace Subsystem
