// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "StateSnapshot.h"

#include "IopMem.h"
#include "Memory.h"
#include "R3000A.h"
#include "R5900.h"

#include <cstring>
#include <sstream>

namespace recompiler_tests {

namespace {

// Copy `count` bytes of IOP RAM starting at guest address `addr` to `out`.
// Uses iopMemRead8 so mirrored addresses (0x0000/0x8000/0xA000 prefixes) all
// work. Missing mappings read as zero, matching how the harness initializes
// memory — tests should only capture windows inside 2 MB main RAM.
void CopyFromIopMem(u32 addr, size_t count, u8* out)
{
	for (size_t i = 0; i < count; ++i)
		out[i] = iopMemRead8(addr + static_cast<u32>(i));
}

void CopyToIopMem(u32 addr, size_t count, const u8* src)
{
	for (size_t i = 0; i < count; ++i)
		iopMemWrite8(addr + static_cast<u32>(i), src[i]);
}

} // namespace

IopSnapshot IopSnapshot::Capture(const std::vector<MemWindow>& windows_to_capture)
{
	IopSnapshot s;
	std::memcpy(&s.regs, &psxRegs, sizeof(psxRegisters));
	s.mem_windows.reserve(windows_to_capture.size());
	for (const auto& w : windows_to_capture)
	{
		MemWindow copy{w.addr, std::vector<u8>(w.bytes.size())};
		CopyFromIopMem(w.addr, copy.bytes.size(), copy.bytes.data());
		s.mem_windows.push_back(std::move(copy));
	}
	return s;
}

void IopSnapshot::Restore() const
{
	std::memcpy(&psxRegs, &regs, sizeof(psxRegisters));
	for (const auto& w : mem_windows)
		CopyToIopMem(w.addr, w.bytes.size(), w.bytes.data());
}

void IopSnapshot::ZeroGlobals()
{
	std::memset(&psxRegs, 0, sizeof(psxRegisters));
}

std::vector<std::string> DiffIop(const IopSnapshot& a, const IopSnapshot& b)
{
	std::vector<std::string> diffs;
	auto emit = [&](const char* name, u32 lhs, u32 rhs) {
		if (lhs != rhs)
		{
			std::ostringstream ss;
			ss << name << ": JIT=0x" << std::hex << lhs
			   << " INTERP=0x" << rhs;
			diffs.push_back(ss.str());
		}
	};

	static const char* const k_gpr_names[32] = {
		"zero","at","v0","v1","a0","a1","a2","a3",
		"t0","t1","t2","t3","t4","t5","t6","t7",
		"s0","s1","s2","s3","s4","s5","s6","s7",
		"t8","t9","k0","k1","gp","sp","s8","ra"};

	for (int i = 0; i < 32; ++i)
		emit(k_gpr_names[i], a.regs.GPR.r[i], b.regs.GPR.r[i]);
	emit("hi", a.regs.GPR.r[32], b.regs.GPR.r[32]);
	emit("lo", a.regs.GPR.r[33], b.regs.GPR.r[33]);

	// CP0 registers — only user-visible ones. Index 12 is Status, 13 Cause,
	// 14 EPC, 15 PRid, others not typically touched by single-op tests but
	// included for completeness.
	for (int i = 0; i < 32; ++i)
	{
		std::string name = "cp0[" + std::to_string(i) + "]";
		emit(name.c_str(), a.regs.CP0.r[i], b.regs.CP0.r[i]);
	}

	emit("pc", a.regs.pc, b.regs.pc);

	// Memory windows — compare by address.
	for (size_t i = 0; i < a.mem_windows.size() && i < b.mem_windows.size(); ++i)
	{
		const auto& aw = a.mem_windows[i];
		const auto& bw = b.mem_windows[i];
		if (aw.addr != bw.addr || aw.bytes.size() != bw.bytes.size())
		{
			diffs.push_back("mem window[" + std::to_string(i) + "] geometry mismatch");
			continue;
		}
		for (size_t j = 0; j < aw.bytes.size(); ++j)
		{
			if (aw.bytes[j] != bw.bytes[j])
			{
				std::ostringstream ss;
				ss << "mem[0x" << std::hex << (aw.addr + static_cast<u32>(j))
				   << "]: JIT=0x" << static_cast<u32>(aw.bytes[j])
				   << " INTERP=0x" << static_cast<u32>(bw.bytes[j]);
				diffs.push_back(ss.str());
			}
		}
	}

	return diffs;
}

// ---------------------------------------------------------------------------
//  EE / R5900 snapshot
// ---------------------------------------------------------------------------

namespace {

// Read/write EE RAM via memRead8/memWrite8. EE guest addresses passed in
// are KSEG0-style physical offsets into main RAM.
void CopyFromEeMem(u32 addr, size_t count, u8* out)
{
	for (size_t i = 0; i < count; ++i)
		out[i] = memRead8(addr + static_cast<u32>(i));
}

void CopyToEeMem(u32 addr, size_t count, const u8* src)
{
	for (size_t i = 0; i < count; ++i)
		memWrite8(addr + static_cast<u32>(i), src[i]);
}

} // namespace

EeSnapshot EeSnapshot::Capture(const std::vector<MemWindow>& windows_to_capture)
{
	EeSnapshot s;
	std::memcpy(&s.regs, &cpuRegs, sizeof(cpuRegisters));
	std::memcpy(&s.fprs, &fpuRegs, sizeof(fpuRegisters));
	s.mem_windows.reserve(windows_to_capture.size());
	for (const auto& w : windows_to_capture)
	{
		MemWindow copy{w.addr, std::vector<u8>(w.bytes.size())};
		CopyFromEeMem(w.addr, copy.bytes.size(), copy.bytes.data());
		s.mem_windows.push_back(std::move(copy));
	}
	return s;
}

void EeSnapshot::Restore() const
{
	std::memcpy(&cpuRegs, &regs, sizeof(cpuRegisters));
	std::memcpy(&fpuRegs, &fprs, sizeof(fpuRegisters));
	for (const auto& w : mem_windows)
		CopyToEeMem(w.addr, w.bytes.size(), w.bytes.data());
}

void EeSnapshot::ZeroGlobals()
{
	std::memset(&cpuRegs, 0, sizeof(cpuRegisters));
	std::memset(&fpuRegs, 0, sizeof(fpuRegisters));
}

std::vector<std::string> DiffEe(const EeSnapshot& a, const EeSnapshot& b)
{
	std::vector<std::string> diffs;
	auto emit64 = [&](const char* name, u64 lhs, u64 rhs) {
		if (lhs != rhs)
		{
			std::ostringstream ss;
			ss << name << ": JIT=0x" << std::hex << lhs
			   << " INTERP=0x" << rhs;
			diffs.push_back(ss.str());
		}
	};
	auto emit32 = [&](const char* name, u32 lhs, u32 rhs) {
		if (lhs != rhs)
		{
			std::ostringstream ss;
			ss << name << ": JIT=0x" << std::hex << lhs
			   << " INTERP=0x" << rhs;
			diffs.push_back(ss.str());
		}
	};

	static const char* const k_gpr_names[32] = {
		"zero","at","v0","v1","a0","a1","a2","a3",
		"t0","t1","t2","t3","t4","t5","t6","t7",
		"s0","s1","s2","s3","s4","s5","s6","s7",
		"t8","t9","k0","k1","gp","sp","s8","ra"};

	for (int i = 0; i < 32; ++i)
	{
		std::string lo = std::string(k_gpr_names[i]) + ".lo";
		std::string hi = std::string(k_gpr_names[i]) + ".hi";
		emit64(lo.c_str(), a.regs.GPR.r[i].UD[0], b.regs.GPR.r[i].UD[0]);
		emit64(hi.c_str(), a.regs.GPR.r[i].UD[1], b.regs.GPR.r[i].UD[1]);
	}
	emit64("hi.lo", a.regs.HI.UD[0], b.regs.HI.UD[0]);
	emit64("hi.hi", a.regs.HI.UD[1], b.regs.HI.UD[1]);
	emit64("lo.lo", a.regs.LO.UD[0], b.regs.LO.UD[0]);
	emit64("lo.hi", a.regs.LO.UD[1], b.regs.LO.UD[1]);

	for (int i = 0; i < 32; ++i)
	{
		// CP0[1]  Random   — TLB pseudo-random index, dispatcher-driven
		// CP0[9]  Count    — EE cycle counter, dispatcher-driven
		// CP0[11] Compare  — Count interrupt target, moves with Count
		// These differ between JIT and interp for reasons unrelated to
		// ISA semantics; same discipline DiffIop uses for cycle /
		// iopNextEventCycle / sCycle / eCycle bookkeeping.
		if (i == 1 || i == 9 || i == 11)
			continue;
		std::string name = "cp0[" + std::to_string(i) + "]";
		emit32(name.c_str(), a.regs.CP0.r[i], b.regs.CP0.r[i]);
	}

	for (int i = 0; i < 32; ++i)
	{
		std::string name = "fpr[" + std::to_string(i) + "]";
		emit32(name.c_str(), a.fprs.fpr[i].UL, b.fprs.fpr[i].UL);
	}

	// PS2 FPU accumulator — written by ADDA/SUBA/MULA/MADDA/MSUBA and read
	// by MADD/MSUB. Diverging ACC corrupts geometry/lighting silently if
	// not included in the diff.
	emit32("ACC", a.fprs.ACC.UL, b.fprs.ACC.UL);

	emit32("pc", a.regs.pc, b.regs.pc);
	emit32("sa", a.regs.sa, b.regs.sa);

	for (size_t i = 0; i < a.mem_windows.size() && i < b.mem_windows.size(); ++i)
	{
		const auto& aw = a.mem_windows[i];
		const auto& bw = b.mem_windows[i];
		if (aw.addr != bw.addr || aw.bytes.size() != bw.bytes.size())
		{
			diffs.push_back("mem window[" + std::to_string(i) + "] geometry mismatch");
			continue;
		}
		for (size_t j = 0; j < aw.bytes.size(); ++j)
		{
			if (aw.bytes[j] != bw.bytes[j])
			{
				std::ostringstream ss;
				ss << "mem[0x" << std::hex << (aw.addr + static_cast<u32>(j))
				   << "]: JIT=0x" << static_cast<u32>(aw.bytes[j])
				   << " INTERP=0x" << static_cast<u32>(bw.bytes[j]);
				diffs.push_back(ss.str());
			}
		}
	}

	return diffs;
}

void PrintEe(std::ostream& os, const EeSnapshot& s)
{
	os << std::hex;
	os << "  pc=0x" << s.regs.pc << "\n";
	for (int i = 0; i < 32; i += 2)
	{
		os << "  r" << std::dec << i << ": 0x" << std::hex
		   << s.regs.GPR.r[i].UD[1] << "_" << s.regs.GPR.r[i].UD[0]
		   << "   r" << std::dec << i+1 << ": 0x" << std::hex
		   << s.regs.GPR.r[i+1].UD[1] << "_" << s.regs.GPR.r[i+1].UD[0] << "\n";
	}
	os << "  hi=0x" << s.regs.HI.UD[1] << "_" << s.regs.HI.UD[0]
	   << " lo=0x" << s.regs.LO.UD[1] << "_" << s.regs.LO.UD[0] << "\n";
	os << std::dec;
}

void PrintIop(std::ostream& os, const IopSnapshot& s)
{
	os << std::hex;
	os << "  pc=0x" << s.regs.pc << "\n";
	for (int i = 0; i < 32; i += 4)
	{
		os << "  r" << std::dec << i << "-" << i + 3 << ": ";
		for (int j = 0; j < 4; ++j)
			os << "0x" << std::hex << s.regs.GPR.r[i + j] << " ";
		os << "\n";
	}
	os << "  hi=0x" << s.regs.GPR.r[32] << " lo=0x" << s.regs.GPR.r[33] << "\n";
	os << std::dec;
}

} // namespace recompiler_tests
