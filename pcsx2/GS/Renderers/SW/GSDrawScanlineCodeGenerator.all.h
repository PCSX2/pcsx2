/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#pragma once

#include "GSScanlineEnvironment.h"
#include "GSNewCodeGenerator.h"

#undef _t // Conflict with wx, hopefully no one needs this

#if _M_SSE >= 0x501
	#define DRAW_SCANLINE_VECTOR_REGISTER Xbyak::Ymm
	#define DRAW_SCANLINE_USING_XMM 0
	#define DRAW_SCANLINE_USING_YMM 1
#else
	#define DRAW_SCANLINE_VECTOR_REGISTER Xbyak::Xmm
	#define DRAW_SCANLINE_USING_XMM 1
	#define DRAW_SCANLINE_USING_YMM 0
#endif

class GSDrawScanlineCodeGenerator2 : public GSNewCodeGenerator
{
	using _parent = GSNewCodeGenerator;
	using XYm = DRAW_SCANLINE_VECTOR_REGISTER;

	/// On x86-64 we reserve a bunch of GPRs for holding addresses of locals that would otherwise be hard to reach
	/// On x86-32 the same values are just raw 32-bit addresses
	using LocalAddr = Choose3264<size_t, AddressReg>::type;

	constexpr static bool isXmm = std::is_same<XYm, Xbyak::Xmm>::value;
	constexpr static bool isYmm = std::is_same<XYm, Xbyak::Ymm>::value;
	constexpr static int wordsize = 8;
	constexpr static int vecsize = isXmm ? 16 : 32;
	constexpr static int vecsizelog = isXmm ? 4 : 5;
	constexpr static int vecints = vecsize / 4;


// MARK: - Constants

	constexpr static int _32_args = 16;
	constexpr static int _invalid = 0xaaaaaaaa;
#ifdef _WIN32
	constexpr static int _64_top = 8 * 0;
	// XMM registers will be saved to `rsp + _64_win_xmm_start + id - 6`
	// Which will put xmm6 after the temporaries, them xmm7, etc
	constexpr static int _64_win_xmm_start = 8 * 2;
	// Windows has no redzone and also has 10 xmm registers to save
	constexpr static int _64_win_stack_size = _64_win_xmm_start + 16 * 10;
#else
	// System-V has a redzone so stick everything there
	constexpr static int _64_rz_rbx = -8 * 1;
	constexpr static int _64_rz_r12 = -8 * 2;
	constexpr static int _64_rz_r13 = -8 * 3;
	constexpr static int _64_rz_r14 = -8 * 4;
	constexpr static int _64_rz_r15 = -8 * 5;
	constexpr static int _64_top    = -8 * 6;
#endif
	constexpr static int _top = _64_top;

	GSScanlineSelector m_sel;
	GSScanlineLocalData& m_local;
	bool m_rip;
	bool use_lod;

	const XYm xym0{0}, xym1{1}, xym2{2}, xym3{3}, xym4{4}, xym5{5}, xym6{6}, xym7{7}, xym8{8}, xym9{9}, xym10{10}, xym11{11}, xym12{12}, xym13{13}, xym14{14}, xym15{15};
	/// Note: a2 and t3 are only available on x86-64
	/// Outside of Init, usable registers are a0, t0, t1, t2, t3[x64], rax, rbx, rdx, r10+
	const AddressReg a0, a1, a2, a3, t0, t1, t2, t3;
	const LocalAddr _g_const, _m_local, _m_local__gd, _m_local__gd__vm;
	/// Available on both x86 and x64, not always valid
	const XYm _rb, _ga, _fm, _zm, _fd, _test;
	/// Always valid if needed, x64 only
	const XYm _z, _f, _s, _t, _q, _f_rb, _f_ga;

	/// Returns the first arg on 32-bit, second on 64-bit
	static LocalAddr chooseLocal(const void* addr32, AddressReg reg64)
	{
		return choose3264((size_t)addr32, reg64);
	}

public:
	GSDrawScanlineCodeGenerator2(Xbyak::CodeGenerator* base, CPUInfo cpu, void* param, u64 key);
	void Generate();

private:
	/// Loads the given address into the given register if needed, and returns something that can be used in a `ptr[]`
	LocalAddr loadAddress(AddressReg reg, const void* addr);
	/// Broadcast 128 bits of floats from memory to the whole register, whatever size that register might be
	void broadcastf128(const XYm& reg, const Xbyak::Address& mem);
	/// Broadcast 128 bits of integers from memory to the whole register, whatever size that register might be
	void broadcasti128(const XYm& reg, const Xbyak::Address& mem);
	/// Broadcast a floating-point variable stored in GSScanlineLocalData to the whole register
	/// On YMM registers this will be a broadcast from a 32-bit value
	/// On XMM registers this will be a load of a full 128-bit value, with the broadcast happening before storing to the local data
	void broadcastssLocal(const XYm& reg, const Xbyak::Address& mem);
	/// Broadcast a qword variable stored in GSScanlineLocalData to the whole register
	/// On YMM registers this will be a broadcast from a 64-bit value
	/// On XMM registers this will be a load of a full 128-bit value, with the broadcast happening before storing to the local data
	void pbroadcastqLocal(const XYm& reg, const Xbyak::Address& mem);
	/// Broadcast a dword variable stored in GSScanlineLocalData to the whole register
	/// On YMM registers this will be a broadcast from a 32-bit value
	/// On XMM registers this will be a load of a full 128-bit value, with the broadcast happening before storing to the local data
	void pbroadcastdLocal(const XYm& reg, const Xbyak::Address& mem);
	/// Broadcast a word variable stored in GSScanlineLocalData to the whole register
	/// On YMM registers this will be a broadcast from a 16-bit value
	/// On XMM registers this will be a load of a full 128-bit value, with the broadcast happening before storing to the local data
	void pbroadcastwLocal(const XYm& reg, const Xbyak::Address& mem);
	void broadcastsd(const XYm& reg, const Xbyak::Address& mem);
	/// Broadcast a 32-bit GPR to a vector register
	void broadcastGPRToVec(const XYm& vec, const Xbyak::Reg32& gpr);
	void modulate16(const XYm& a, const Xbyak::Operand& f, u8 shift);
	void lerp16(const XYm& a, const XYm& b, const XYm& f, u8 shift);
	void lerp16_4(const XYm& a, const XYm& b, const XYm& f);
	void mix16(const XYm& a, const XYm& b, const XYm& temp);
	void clamp16(const XYm& a, const XYm& temp);
	void alltrue(const XYm& test);
	void blend(const XYm& a, const XYm& b, const XYm& mask);
	void blendr(const XYm& b, const XYm& a, const XYm& mask);
	void blend8(const XYm& a, const XYm& b);
	void blend8r(const XYm& b, const XYm& a);
	void split16_2x8(const XYm& l, const XYm& h, const XYm& src);

	void Init();
	void Step();
	void TestZ(const XYm& temp1, const XYm& temp2);
	void SampleTexture();
	void SampleTexture_TexelReadHelper(int mip_offset);
	void Wrap(const XYm& uv);
	void Wrap(const XYm& uv0, const XYm& uv1);
	void SampleTextureLOD();
	void WrapLOD(const XYm& uv);
	void WrapLOD(const XYm& uv0, const XYm& uv1);
	void AlphaTFX();
	void ReadMask();
	void TestAlpha();
	void ColorTFX();
	void Fog();
	void ReadFrame();
	void TestDestAlpha();
	void WriteMask();
	void WriteZBuf();
	void AlphaBlend();
	void WriteFrame();
	void ReadPixel(const XYm& dst, const XYm& tmp, const AddressReg& addr);
#if DRAW_SCANLINE_USING_XMM
	void WritePixel(const XYm& src_, const AddressReg& addr, const Xbyak::Reg8& mask, bool fast, int psm, int fz);
#else
	void WritePixel(const XYm& src_, const AddressReg& addr, const Xbyak::Reg32& mask, bool fast, int psm, int fz);
#endif
	void WritePixel(const Xmm& src, const AddressReg& addr, u8 i, u8 j, int psm);
	void ReadTexel1(const XYm& dst, const XYm& src, const XYm& tmp1, const XYm& tmp2, int mip_offset);
	void ReadTexel4(
		const XYm& d0,   const XYm& d1,
		const XYm& d2s0, const XYm& d3s1,
		const XYm& s2,   const XYm& s3,
		const XYm& tmp1, const XYm& tmp2,
		int mip_offset);
	void ReadTexelImpl(
		const XYm& d0,   const XYm& d1,
		const XYm& d2s0, const XYm& d3s1,
		const XYm& s2,   const XYm& s3,
		const XYm& tmp1, const XYm& tmp2,
		int pixels,      int mip_offset);
	void ReadTexelImplLoadTexLOD(int lod, int mip_offset);
	void ReadTexelImplYmm(
		const Ymm& d0,   const Ymm& d1,
		const Ymm& d2s0, const Ymm& d3s1,
		const Ymm& s2,   const Ymm& s3,
		const Ymm& tmp,
		int pixels,      int mip_offset);
	void ReadTexelImplSSE4(
		const Xmm& d0,   const Xmm& d1,
		const Xmm& d2s0, const Xmm& d3s1,
		const Xmm& s2,   const Xmm& s3,
		int pixels,      int mip_offset);
	void ReadTexelImpl(const Xmm& dst, const Xmm& addr, u8 i, bool texInA3, bool preserveDst);
};
