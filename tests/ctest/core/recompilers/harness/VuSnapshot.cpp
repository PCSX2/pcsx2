// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "VuSnapshot.h"

#include "VUmicro.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace recompiler_tests {

namespace {

void CopyFromVuMem(const VURegs& vu, u32 addr, size_t count, u8* out)
{
	const u32 mask = (vu.idx == 0) ? VU0_MEMMASK : VU1_MEMMASK;
	for (size_t i = 0; i < count; ++i)
		out[i] = vu.Mem[(addr + static_cast<u32>(i)) & mask];
}

void CopyToVuMem(VURegs& vu, u32 addr, size_t count, const u8* src)
{
	const u32 mask = (vu.idx == 0) ? VU0_MEMMASK : VU1_MEMMASK;
	for (size_t i = 0; i < count; ++i)
		vu.Mem[(addr + static_cast<u32>(i)) & mask] = src[i];
}

} // namespace

VuSnapshot VuSnapshot::Capture(int index, const std::vector<VuMemWindow>& windows_to_capture)
{
	VuSnapshot s;
	s.index = index;
	std::memcpy(&s.regs, &vuRegs[index], sizeof(VURegs));

	s.mem_windows.reserve(windows_to_capture.size());
	for (const auto& w : windows_to_capture)
	{
		VuMemWindow copy{w.addr, std::vector<u8>(w.bytes.size())};
		CopyFromVuMem(vuRegs[index], copy.addr, copy.bytes.size(), copy.bytes.data());
		s.mem_windows.push_back(std::move(copy));
	}
	return s;
}

void VuSnapshot::Restore() const
{
	u8* live_mem = vuRegs[index].Mem;
	u8* live_micro = vuRegs[index].Micro;
	std::memcpy(&vuRegs[index], &regs, sizeof(VURegs));
	vuRegs[index].Mem = live_mem;
	vuRegs[index].Micro = live_micro;

	for (const auto& w : mem_windows)
		CopyToVuMem(vuRegs[index], w.addr, w.bytes.size(), w.bytes.data());
}

void VuSnapshot::ZeroGlobals(int index)
{
	u8* live_mem = vuRegs[index].Mem;
	u8* live_micro = vuRegs[index].Micro;
	std::memset(&vuRegs[index], 0, sizeof(VURegs));
	vuRegs[index].Mem = live_mem;
	vuRegs[index].Micro = live_micro;
	vuRegs[index].idx = static_cast<uint>(index);
	// VF[0] is hardwired { 0, 0, 0, 1.0f } on real hardware; the interpreter
	// asserts on any drift via DbgCon.Error. Restore the canonical value here
	// so a fresh ZeroGlobals leaves the bank in a runnable state.
	vuRegs[index].VF[0].f.x = 0.0f;
	vuRegs[index].VF[0].f.y = 0.0f;
	vuRegs[index].VF[0].f.z = 0.0f;
	vuRegs[index].VF[0].f.w = 1.0f;
}

namespace {

void EmitU32(std::vector<std::string>& out, const char* name, u32 a, u32 b)
{
	if (a == b)
		return;
	std::ostringstream ss;
	ss << name << ": JIT=0x" << std::hex << a << " INTERP=0x" << b;
	out.push_back(ss.str());
}

void EmitVfLane(std::vector<std::string>& out, int reg, char lane, u32 a, u32 b)
{
	if (a == b)
		return;
	std::ostringstream ss;
	ss << "vf" << reg << "." << lane
	   << ": JIT=0x" << std::hex << a << " INTERP=0x" << b;
	out.push_back(ss.str());
}

void DiffArchitectural(std::vector<std::string>& diffs, const VURegs& a, const VURegs& b,
	const std::vector<int>& ignored_vi)
{
	// VF[32] — 4 lanes per register, compared as raw bits to catch
	// NaN-payload divergences that would silently match under float compare.
	for (int i = 0; i < 32; ++i)
	{
		EmitVfLane(diffs, i, 'x', a.VF[i].i.x, b.VF[i].i.x);
		EmitVfLane(diffs, i, 'y', a.VF[i].i.y, b.VF[i].i.y);
		EmitVfLane(diffs, i, 'z', a.VF[i].i.z, b.VF[i].i.z);
		EmitVfLane(diffs, i, 'w', a.VF[i].i.w, b.VF[i].i.w);
	}
	// VI[32] — most are 16-bit (low 16 valid; upper 112 hardwired zero on HW).
	// The "special" VIs (REG_R/I/Q/P/STATUS/MAC/CLIP/TPC/
	// FBRST/VPU_STAT) hold full 32-bit values: REG_Q/P/I/R store IEEE-754
	// floats, the *_FLAG triplet stores microVU's normalized flag layout,
	// VPU_STAT/FBRST hold control bits in the upper byte. mVUendProgram
	// writes the JIT's architectural Q to VI[REG_Q] in full, so the diff
	// must compare the full word for those slots.
	auto isFullWidthVi = [](int i) {
		return i == REG_R || i == REG_I || i == REG_Q || i == REG_P
		    || i == REG_STATUS_FLAG || i == REG_MAC_FLAG || i == REG_CLIP_FLAG
		    || i == REG_TPC || i == REG_FBRST || i == REG_VPU_STAT;
	};
	for (int i = 0; i < 32; ++i)
	{
		if (std::find(ignored_vi.begin(), ignored_vi.end(), i) != ignored_vi.end())
			continue;
		std::string name = "vi" + std::to_string(i);
		const u32 mask = isFullWidthVi(i) ? 0xFFFFFFFFu : 0x0000FFFFu;
		EmitU32(diffs, name.c_str(), a.VI[i].UL & mask, b.VI[i].UL & mask);
	}
	// ACC — 4 lanes.
	EmitVfLane(diffs, -1, 'x', a.ACC.i.x, b.ACC.i.x);
	EmitVfLane(diffs, -1, 'y', a.ACC.i.y, b.ACC.i.y);
	EmitVfLane(diffs, -1, 'z', a.ACC.i.z, b.ACC.i.z);
	EmitVfLane(diffs, -1, 'w', a.ACC.i.w, b.ACC.i.w);
	// q.UL / p.UL are interpreter-internal staging slots that the interp
	// updates per-instruction inside Q/EFU ops. The JIT only commits to
	// VI[REG_Q] / VI[REG_P] at end-of-program. Architectural state is the
	// VI cell, already covered above — don't double-diff.
}

void DiffPipeline(std::vector<std::string>& diffs, const VURegs& a, const VURegs& b)
{
	for (int i = 0; i < 4; ++i)
	{
		std::string mname = "micro_macflags[" + std::to_string(i) + "]";
		std::string cname = "micro_clipflags[" + std::to_string(i) + "]";
		std::string sname = "micro_statusflags[" + std::to_string(i) + "]";
		EmitU32(diffs, mname.c_str(), a.micro_macflags[i], b.micro_macflags[i]);
		EmitU32(diffs, cname.c_str(), a.micro_clipflags[i], b.micro_clipflags[i]);
		EmitU32(diffs, sname.c_str(), a.micro_statusflags[i], b.micro_statusflags[i]);
	}
	EmitU32(diffs, "pending_q", a.pending_q, b.pending_q);
	EmitU32(diffs, "pending_p", a.pending_p, b.pending_p);
}

void DiffXgkick(std::vector<std::string>& diffs, const VURegs& a, const VURegs& b)
{
	EmitU32(diffs, "xgkickaddr",          a.xgkickaddr,          b.xgkickaddr);
	EmitU32(diffs, "xgkickdiff",          a.xgkickdiff,          b.xgkickdiff);
	EmitU32(diffs, "xgkicksizeremaining", a.xgkicksizeremaining, b.xgkicksizeremaining);
	EmitU32(diffs, "xgkickcyclecount",    a.xgkickcyclecount,    b.xgkickcyclecount);
	EmitU32(diffs, "xgkickenable",        a.xgkickenable,        b.xgkickenable);
	EmitU32(diffs, "xgkickendpacket",     a.xgkickendpacket,     b.xgkickendpacket);
}

void DiffMemWindows(std::vector<std::string>& diffs,
	const std::vector<VuMemWindow>& a, const std::vector<VuMemWindow>& b)
{
	for (size_t i = 0; i < a.size() && i < b.size(); ++i)
	{
		const auto& aw = a[i];
		const auto& bw = b[i];
		if (aw.addr != bw.addr || aw.bytes.size() != bw.bytes.size())
		{
			diffs.push_back("vumem window[" + std::to_string(i) + "] geometry mismatch");
			continue;
		}
		for (size_t j = 0; j < aw.bytes.size(); ++j)
		{
			if (aw.bytes[j] == bw.bytes[j])
				continue;
			std::ostringstream ss;
			ss << "vumem[0x" << std::hex << (aw.addr + static_cast<u32>(j))
			   << "]: JIT=0x" << static_cast<u32>(aw.bytes[j])
			   << " INTERP=0x" << static_cast<u32>(bw.bytes[j]);
			diffs.push_back(ss.str());
		}
	}
}

} // namespace

std::vector<std::string> DiffVu(const VuSnapshot& a, const VuSnapshot& b, VuDiffMode mode,
	const std::vector<int>& ignored_vi)
{
	std::vector<std::string> diffs;
	if (a.index != b.index)
	{
		diffs.push_back("index mismatch");
		return diffs;
	}

	DiffArchitectural(diffs, a.regs, b.regs, ignored_vi);
	if (mode == VuDiffMode::Strict)
		DiffPipeline(diffs, a.regs, b.regs);
	if (a.index == 1 && mode != VuDiffMode::XgkickPacketEquivalent)
		DiffXgkick(diffs, a.regs, b.regs);
	DiffMemWindows(diffs, a.mem_windows, b.mem_windows);

	return diffs;
}

void PrintVu(std::ostream& os, const VuSnapshot& s)
{
	os << std::hex;
	os << "  VU" << s.index << " TPC=0x" << s.regs.VI[REG_TPC].UL
	   << " VPU_STAT=0x" << s.regs.VI[REG_VPU_STAT].UL
	   << " STATUS=0x" << s.regs.VI[REG_STATUS_FLAG].UL
	   << " MAC=0x" << s.regs.VI[REG_MAC_FLAG].UL
	   << " CLIP=0x" << s.regs.VI[REG_CLIP_FLAG].UL << "\n";
	for (int i = 0; i < 32; i += 2)
	{
		os << "  vf" << std::dec << i << ": "
		   << s.regs.VF[i].f.x << "," << s.regs.VF[i].f.y << ","
		   << s.regs.VF[i].f.z << "," << s.regs.VF[i].f.w
		   << "  vf" << i + 1 << ": "
		   << s.regs.VF[i + 1].f.x << "," << s.regs.VF[i + 1].f.y << ","
		   << s.regs.VF[i + 1].f.z << "," << s.regs.VF[i + 1].f.w << "\n";
	}
	for (int i = 0; i < 32; i += 8)
	{
		os << "  vi" << std::dec << i << "-" << i + 7 << ":";
		for (int j = 0; j < 8; ++j)
			os << " 0x" << std::hex << (s.regs.VI[i + j].UL & 0xFFFF);
		os << "\n";
	}
	os << "  q=0x" << std::hex << s.regs.q.UL << " p=0x" << s.regs.p.UL << "\n";
	os << std::dec;
}

} // namespace recompiler_tests
