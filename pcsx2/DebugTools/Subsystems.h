// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"
#include <string>

namespace Subsystem
{

/// Subsystem categories for high-level emulation flow tracking
/// Used to provide context about which PS2 subsystem is being accessed
enum class Type : u8
{
	None = 0,          // Regular user code / main memory

	// Graphics subsystems
	GS,                // Graphics Synthesizer (rendering, textures)
	GIF,               // Graphics Interface (GS data transfer path)

	// Vector Unit subsystems
	VIF0,              // VU Interface 0 (VU0 data/program upload)
	VIF1,              // VU Interface 1 (VU1 data/program upload)
	VU0,               // Vector Unit 0 (geometry processing)
	VU1,               // Vector Unit 1 (geometry processing)

	// DMA subsystems
	DMA,               // Direct Memory Access controller

	// Image Processing Unit
	IPU,               // Image Processing Unit (video decompression)

	// Audio subsystems
	SPU2,              // Sound Processing Unit 2 (audio playback)

	// I/O subsystems
	CDVD,              // CD/DVD drive access
	USB,               // USB peripheral access
	DEV9,              // Network/HDD interface
	PAD,               // Controller input (via SIO2)
	SIO,               // Serial I/O
	SIO2,              // Serial I/O 2 (controller/memory card interface)

	// System interfaces
	BIOS,              // BIOS syscalls
	INTC,              // Interrupt Controller
	DMAC,              // DMA Controller registers
	TIMER,             // Timer/Counter

	// Inter-processor communication
	SIF,               // Sub-system Interface (EE <-> IOP communication)

	// Memory
	SPR,               // Scratchpad RAM DMA
};

/// Convert subsystem type to human-readable string
const char* GetName(Type subsystem);

/// Context structure for subsystem detection
struct DetectionContext
{
	u64 mem_addr;          // Memory address being accessed (if any)
	u32 opcode;            // Instruction opcode
	u32 v1_register;       // v1 register value (for syscall number)
	bool is_write;         // true if memory write, false if read
};

/// Detect which subsystem (if any) is being accessed
/// Returns Subsystem::Type::None if no specific subsystem detected
Type DetectFromMemoryAddress(u64 addr, bool is_write = false);

/// Detect BIOS syscall from opcode and v1 register
/// Returns Subsystem::Type::BIOS if syscall detected, else Subsystem::Type::None
Type DetectFromSyscall(u32 opcode, u32 v1_register);

/// Get detailed description of subsystem activity
/// For example: "DMA CH2 (GIF)" or "BIOS syscall 6 (LoadExecPS2)"
std::string GetDetailString(Type subsystem, const DetectionContext& ctx);

/// Get DMA channel name from register address
/// Returns empty string if not a DMA register
std::string GetDMAChannelName(u64 addr);

/// Check if address is in EE hardware register range
inline bool IsEEHardware(u64 addr)
{
	return (addr >= 0x10000000 && addr < 0x10010000);
}

/// Check if address is in IOP hardware register range
inline bool IsIOPHardware(u64 addr)
{
	return (addr >= 0x1F801000 && addr < 0x1F810000);
}

/// Check if address is GS register range
inline bool IsGSRegister(u64 addr)
{
	return (addr >= 0x12000000 && addr < 0x12002000);
}

} // namespace Subsystem
