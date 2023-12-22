// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GSScanlineEnvironment.h"
#include "GSNewCodeGenerator.h"
#include "GS/MultiISA.h"

#if _M_SSE >= 0x501
	#define SETUP_PRIM_VECTOR_REGISTER Xbyak::Ymm
	#define SETUP_PRIM_USING_XMM 0
	#define SETUP_PRIM_USING_YMM 1
#else
	#define SETUP_PRIM_VECTOR_REGISTER Xbyak::Xmm
	#define SETUP_PRIM_USING_XMM 1
	#define SETUP_PRIM_USING_YMM 0
#endif

MULTI_ISA_UNSHARED_START

class GSSetupPrimCodeGenerator2 : public GSNewCodeGenerator
{
	using _parent = GSNewCodeGenerator;
	using XYm = SETUP_PRIM_VECTOR_REGISTER;

	using Xmm = Xbyak::Xmm;
	using Ymm = Xbyak::Ymm;

	constexpr static bool isXmm = std::is_same<XYm, Xbyak::Xmm>::value;
	constexpr static bool isYmm = std::is_same<XYm, Xbyak::Ymm>::value;
	constexpr static int vecsize = isXmm ? 16 : 32;

	constexpr static int dsize = isXmm ? 4 : 8;

	GSScanlineSelector m_sel;
	bool many_regs;

	struct {u32 z:1, f:1, t:1, c:1;} m_en;

	const XYm xym0{0}, xym1{1}, xym2{2}, xym3{3}, xym4{4}, xym5{5}, xym6{6}, xym7{7}, xym8{8}, xym9{9}, xym10{10}, xym11{11}, xym12{12}, xym13{13}, xym14{14}, xym15{15};
	const AddressReg _64_vertex, _index, _dscan, _m_local, t1;

public:
	GSSetupPrimCodeGenerator2(Xbyak::CodeGenerator* base, const ProcessorFeatures& cpu, u64 key);
	void Generate();

private:
	/// Broadcast 128 bits of floats from memory to the whole register, whatever size that register might be
	void broadcastf128(const XYm& reg, const Xbyak::Address& mem);
	/// Broadcast a 32-bit float to the whole register, whatever size that register might be
	void broadcastss(const XYm& reg, const Xbyak::Address& mem);

	void Depth_XMM();
	void Depth_YMM();
	void Texture();
	void Color();
};

MULTI_ISA_UNSHARED_END
