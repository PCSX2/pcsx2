// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#pragma once

#include "GS/Renderers/Common/GSFunctionMap.h"
#include "GS/Renderers/SW/GSScanlineEnvironment.h"

#include "vixl/aarch64/macro-assembler-aarch64.h"

class GSDrawScanlineCodeGenerator
{
public:
	GSDrawScanlineCodeGenerator(u64 key, void* code, size_t maxsize);
	void Generate();

	size_t GetSize() const { return m_emitter.GetSizeOfCodeGenerated(); }
	const u8* GetCode() const { return m_emitter.GetBuffer().GetStartAddress<const u8*>(); }

private:
	void Init();
	void Step();
	void TestZ(const vixl::aarch64::VRegister& temp1, const vixl::aarch64::VRegister& temp2);
	void SampleTexture();
	void SampleTexture_TexelReadHelper(int mip_offset);
	void Wrap(const vixl::aarch64::VRegister& uv0);
	void Wrap(const vixl::aarch64::VRegister& uv0, const vixl::aarch64::VRegister& uv1);
	void SampleTextureLOD();
	void WrapLOD(const vixl::aarch64::VRegister& uv,
		const vixl::aarch64::VRegister& tmp, const vixl::aarch64::VRegister& tmp2,
		const vixl::aarch64::VRegister& min, const vixl::aarch64::VRegister& max);
	void WrapLOD(const vixl::aarch64::VRegister& uv0, const vixl::aarch64::VRegister& uv1,
		const vixl::aarch64::VRegister& tmp, const vixl::aarch64::VRegister& tmp2,
		const vixl::aarch64::VRegister& min, const vixl::aarch64::VRegister& max);
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
	void ReadPixel(const vixl::aarch64::VRegister& dst, const vixl::aarch64::Register& addr);
	void WritePixel(const vixl::aarch64::VRegister& src, const vixl::aarch64::Register& addr, const vixl::aarch64::Register& mask, bool high, bool fast, int psm, int fz);
	void WritePixel(const vixl::aarch64::VRegister& src, const vixl::aarch64::Register& addr, u8 i, int psm);

	void ReadTexel1(const vixl::aarch64::VRegister& dst, const vixl::aarch64::VRegister& src,
		const vixl::aarch64::VRegister& tmp1, int mip_offset);
	void ReadTexel4(
		const vixl::aarch64::VRegister& d0, const vixl::aarch64::VRegister& d1,
		const vixl::aarch64::VRegister& d2s0, const vixl::aarch64::VRegister& d3s1,
		const vixl::aarch64::VRegister& s2, const vixl::aarch64::VRegister& s3,
		int mip_offset);
	void ReadTexelImplLoadTexLOD(const vixl::aarch64::Register& addr, int lod, int mip_offset);
	void ReadTexelImpl(
		const vixl::aarch64::VRegister& d0, const vixl::aarch64::VRegister& d1,
		const vixl::aarch64::VRegister& d2s0, const vixl::aarch64::VRegister& d3s1,
		const vixl::aarch64::VRegister& s2, const vixl::aarch64::VRegister& s3,
		int pixels, int mip_offset);
	void ReadTexelImpl(const vixl::aarch64::VRegister& dst, const vixl::aarch64::VRegister& addr,
		u8 i, const vixl::aarch64::Register& baseRegister, bool preserveDst);

	void modulate16(const vixl::aarch64::VRegister& d, const vixl::aarch64::VRegister& a, const vixl::aarch64::VRegister& f, u8 shift);
	void modulate16(const vixl::aarch64::VRegister& a, const vixl::aarch64::VRegister& f, u8 shift);
	void lerp16(const vixl::aarch64::VRegister& a, const vixl::aarch64::VRegister& b, const vixl::aarch64::VRegister& f, u8 shift);
	void lerp16_4(const vixl::aarch64::VRegister& a, const vixl::aarch64::VRegister& b, const vixl::aarch64::VRegister& f);
	void mix16(const vixl::aarch64::VRegister& a, const vixl::aarch64::VRegister& b, const vixl::aarch64::VRegister& temp);
	void clamp16(const vixl::aarch64::VRegister& a, const vixl::aarch64::VRegister& temp);
	void alltrue(const vixl::aarch64::VRegister& test, const vixl::aarch64::VRegister& temp);
	void blend8(const vixl::aarch64::VRegister& a, const vixl::aarch64::VRegister& b, const vixl::aarch64::VRegister& mask, const vixl::aarch64::VRegister& temp);
	void blend8r(const vixl::aarch64::VRegister& b, const vixl::aarch64::VRegister& a, const vixl::aarch64::VRegister& mask, const vixl::aarch64::VRegister& temp);
	void split16_2x8(const vixl::aarch64::VRegister& l, const vixl::aarch64::VRegister& h, const vixl::aarch64::VRegister& src);

	vixl::aarch64::MacroAssembler m_emitter;

	GSScanlineSelector m_sel;

	vixl::aarch64::Label m_step_label;
};
