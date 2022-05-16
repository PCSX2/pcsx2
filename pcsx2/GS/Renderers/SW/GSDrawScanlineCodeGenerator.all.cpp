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

#include "PrecompiledHeader.h"
#include "GSDrawScanlineCodeGenerator.all.h"
#include "GS/Renderers/Common/GSFunctionMap.h"
#include "GSVertexSW.h"

using namespace Xbyak;

// Ease the reading of the code
// Note, there are versions without the _64 prefix that can be used as source (but not destination) operands on both 32 and 64 bit
#define _64_g_const r10
#define _64_m_local r12
#define _64_m_local__gd r13
#define _64_m_local__gd__vm t3
#define _64_m_local__gd__clut r11
// If use_lod, m_local.gd->tex, else m_local.gd->tex[0]
#define _64_m_local__gd__tex r14

#define _rip_local_(ptrtype, field) ((m_rip) ? ptrtype[rip + (char*)&m_local.field] : ptrtype[_m_local + OFFSETOF(GSScanlineLocalData, field)])
#define _rip_local(field) _rip_local_(ptr, field)
#define _rip_global(field) ((m_rip) ? ptr[rip + (char*)&m_local.gd->field] : ptr[_m_local__gd + OFFSETOF(GSScanlineGlobalData, field)])

/// On AVX, does a v-prefixed separate destination operation
/// On SSE, moves src1 into dst using movdqa, then does the operation
#define THREEARG(operation, dst, src1, ...) \
	do \
	{ \
		if (hasAVX) \
		{ \
			v##operation(dst, src1, __VA_ARGS__); \
		} \
		else \
		{ \
			movdqa(dst, src1); \
			operation(dst, __VA_ARGS__); \
		} \
	} while (0)

/// On x64, does a 3-operand move, on x86 uses a two-operand SSE-style
#define MOVE_IF_64(operation, dst, src64, ...) \
	do \
	{ \
		THREEARG(operation, dst, src64, __VA_ARGS__); \
	} while (0)

#define USING_XMM DRAW_SCANLINE_USING_XMM
#define USING_YMM DRAW_SCANLINE_USING_YMM

#if _M_SSE >= 0x501
	/// On AVX2, uses the given broadcast to load into the temp register, then applies the given op
	/// Otherwise, applies the given op directly
	#define BROADCAST_AND_OP(broadcast, op, dst, tmpReg, src) \
		do \
		{ \
			broadcast(tmpReg, src); \
			op(dst, tmpReg); \
		} while (0)
	#define _rip_local_d(x) _rip_local(d8.x)
	#define _rip_local_d_p(x) _rip_local_d(p.x)
#else
	/// On AVX2, uses the given broadcast to load into the temp register, then applies the given op
	/// Otherwise, applies the given op directly
	#define BROADCAST_AND_OP(broadcast, op, dst, tmpReg, src) \
		op(dst, src)
	#define _rip_local_d(x) _rip_local(d4.x)
	#define _rip_local_d_p(x) _rip_local_d(x)
#endif

GSDrawScanlineCodeGenerator2::GSDrawScanlineCodeGenerator2(Xbyak::CodeGenerator* base, CPUInfo cpu, void* param, u64 key)
	: _parent(base, cpu)
	, m_local(*(GSScanlineLocalData*)param)
	, m_rip(false)
#ifdef _WIN32
	, a0(rcx), a1(rdx)
	, a2(r8) , a3(r9)
	, t0(rdi), t1(rsi)
	, t2(r8) , t3(r9)
#else
	, a0(rdi), a1(rsi)
	, a2(rdx), a3(rcx)
	, t0(r8) , t1(r9)
	, t2(rcx), t3(rsi)
#endif
	, _g_const(chooseLocal(&*g_const, _64_g_const))
	, _m_local(chooseLocal(&m_local, _64_m_local))
	, _m_local__gd(chooseLocal(m_local.gd, _64_m_local__gd))
	, _m_local__gd__vm(chooseLocal(m_local.gd->vm, _64_m_local__gd__vm))
	, _rb(xym5), _ga(xym6), _fm(xym3), _zm(xym4), _fd(xym2), _test(xym15)
	, _z(xym8), _f(xym9), _s(xym10), _t(xym11), _q(xym12), _f_rb(xym13), _f_ga(xym14)
{
	m_sel.key = key;
	use_lod = m_sel.mmin;
	if (isYmm)
		ASSERT(hasAVX2);
}

// MARK: - Helpers

GSDrawScanlineCodeGenerator2::LocalAddr GSDrawScanlineCodeGenerator2::loadAddress(AddressReg reg, const void* addr)
{
	mov(reg, (size_t)addr);
	return choose3264((size_t)addr, reg);
}

void GSDrawScanlineCodeGenerator2::broadcastf128(const XYm& reg, const Address& mem)
{
#if USING_YMM
	vbroadcastf128(reg, mem);
#else
	movaps(reg, mem);
#endif
}

void GSDrawScanlineCodeGenerator2::broadcasti128(const XYm& reg, const Address& mem)
{
#if USING_YMM
	vbroadcasti128(reg, mem);
#else
	movdqa(reg, mem);
#endif
}

void GSDrawScanlineCodeGenerator2::broadcastssLocal(const XYm& reg, const Address& mem)
{
#if USING_YMM
	vbroadcastss(reg, mem);
#else
	movaps(reg, mem);
#endif
}

void GSDrawScanlineCodeGenerator2::pbroadcastqLocal(const XYm& reg, const Address& mem)
{
#if USING_YMM
	vpbroadcastq(reg, mem);
#else
	movdqa(reg, mem);
#endif
}

void GSDrawScanlineCodeGenerator2::pbroadcastdLocal(const XYm& reg, const Address& mem)
{
#if USING_YMM
	vpbroadcastd(reg, mem);
#else
	movdqa(reg, mem);
#endif
}

void GSDrawScanlineCodeGenerator2::pbroadcastwLocal(const XYm& reg, const Address& mem)
{
#if USING_YMM
	vpbroadcastw(reg, mem);
#else
	movdqa(reg, mem);
#endif
}

void GSDrawScanlineCodeGenerator2::broadcastsd(const XYm& reg, const Address& mem)
{
#if USING_YMM
	vbroadcastsd(reg, mem);
#else
	movddup(reg, mem);
#endif
}

void GSDrawScanlineCodeGenerator2::broadcastGPRToVec(const XYm& vec, const Xbyak::Reg32& gpr)
{
	movd(Xmm(vec.getIdx()), gpr);
#if USING_YMM
	vpbroadcastd(vec, Xmm(vec.getIdx()));
#else
	pshufd(vec, vec, _MM_SHUFFLE(0, 0, 0, 0));
#endif
}

void GSDrawScanlineCodeGenerator2::modulate16(const XYm& a, const Operand& f, u8 shift)
{
	if (shift == 0)
	{
		pmulhrsw(a, f);
	}
	else
	{
		psllw(a, shift + 1);
		pmulhw(a, f);
	}
}

void GSDrawScanlineCodeGenerator2::lerp16(const XYm& a, const XYm& b, const XYm& f, u8 shift)
{
	psubw(a, b);
	modulate16(a, f, shift);
	paddw(a, b);
}

void GSDrawScanlineCodeGenerator2::lerp16_4(const XYm& a, const XYm& b, const XYm& f)
{
	psubw(a, b);
	pmullw(a, f);
	psraw(a, 4);
	paddw(a, b);
}

void GSDrawScanlineCodeGenerator2::mix16(const XYm& a, const XYm& b, const XYm& temp)
{
	pblendw(a, b, 0xaa);
}

void GSDrawScanlineCodeGenerator2::clamp16(const XYm& a, const XYm& temp)
{
	if (isXmm)
	{
		packuswb(a, a);
		pmovzxbw(a, a);
	}
	else
	{
		packuswb(a, a);
		pxor(temp, temp);
		punpcklbw(a, temp);
	}
}

void GSDrawScanlineCodeGenerator2::alltrue(const XYm& test)
{
	u32 mask = test.isYMM() ? 0xffffffff : 0xffff;
	pmovmskb(eax, test);
	cmp(eax, mask);
	je("step", GSCodeGenerator::T_NEAR);
}

void GSDrawScanlineCodeGenerator2::blend(const XYm& a, const XYm& b, const XYm& mask)
{
	pand(b, mask);
	pandn(mask, a);
	if (hasAVX)
	{
		vpor(a, b, mask);
	}
	else
	{
		por(b, mask);
		movdqa(a, b);
	}
}

void GSDrawScanlineCodeGenerator2::blendr(const XYm& b, const XYm& a, const XYm& mask)
{
	pand(b, mask);
	pandn(mask, a);
	por(b, mask);
}

void GSDrawScanlineCodeGenerator2::blend8(const XYm& a, const XYm& b)
{
	pblendvb(a, b /*, xym0 */);
}

void GSDrawScanlineCodeGenerator2::blend8r(const XYm& b, const XYm& a)
{
	if (hasAVX)
	{
		vpblendvb(b, a, b, xym0);
	}
	else
	{
		pblendvb(a, b);
		movdqa(b, a);
	}
}

void GSDrawScanlineCodeGenerator2::split16_2x8(const XYm& l, const XYm& h, const XYm& src)
{
	// l = src & 0xFF; (1 left shift + 1 right shift)
	// h = (src >> 8) & 0xFF; (1 right shift)

	if (hasAVX)
	{
		if (src == h)
		{
			vpsllw(l, src, 8);
			psrlw(h, 8);
		}
		else if (src == l)
		{
			vpsrlw(h, src, 8);
			psllw(l, 8);
		}
		else
		{
			vpsllw(l, src, 8);
			vpsrlw(h, src, 8);
		}
		psrlw(l, 8);
	}
	else
	{
		if (src == h)
		{
			movdqa(l, src);
		}
		else if (src == l)
		{
			movdqa(h, src);
		}
		else
		{
			movdqa(l, src);
			movdqa(h, src);
		}
		psllw(l, 8);
		psrlw(l, 8);
		psrlw(h, 8);
	}
}

// MARK: - Main Implementation

void GSDrawScanlineCodeGenerator2::Generate()
{
	bool need_tex = m_sel.fb && m_sel.tfx != TFX_NONE;
	bool need_clut = need_tex && m_sel.tlu;
	m_rip = (size_t)getCurr() < 0x80000000;
	m_rip &= (size_t)&m_local < 0x80000000;
	m_rip &= (size_t)&m_local.gd < 0x80000000;

	push(rbp);
	mov(rbp, rsp); // Stack traces look much nicer this way
#ifdef _WIN32
	push(rbx);
	push(rsi);
	push(rdi);
	push(r12);
	push(r13);
	push(r14);

	sub(rsp, _64_win_stack_size);

	for (int i = 0; i < 10; i++)
	{
		movdqa(ptr[rsp + _64_win_xmm_start + 16 * i], Xmm(i + 6));
	}
#else
	mov(ptr[rsp + _64_rz_rbx], rbx);
	if (!m_rip)
	{
		mov(ptr[rsp + _64_rz_r12], r12);
		mov(ptr[rsp + _64_rz_r13], r13);
	}
	mov(ptr[rsp + _64_rz_r14], r14);
	mov(ptr[rsp + _64_rz_r15], r15);
#endif
	mov(_64_g_const, (size_t)&*g_const);
	if (!m_rip)
	{
		mov(_64_m_local, (size_t)&m_local);
		mov(_64_m_local__gd, _rip_local(gd));
	}

	if (need_clut)
		mov(_64_m_local__gd__clut, _rip_global(clut));

	Init();

	if (!m_sel.edge)
	{
		align(16);
	}

L("loop");

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// xym0 = z/zi      |
	// xym2 = s/u (tme) | free
	// xym3 = t/v (tme) | free
	// xym4 = q (tme)   | free
	// xym5 = rb (!tme)
	// xym6 = ga (!tme)
	// xym7 = test      | z0
	// xym15 =          | test

	bool tme = m_sel.tfx != TFX_NONE;

	TestZ(tme ? xym5 : xym2, tme ? xym6 : xym3);

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// t2 = za
	// xym2 = s/u (tme) | free
	// xym3 = t/v (tme) | free
	// xym4 = q (tme)   | free
	// xym5 = rb (!tme)
	// xym6 = ga (!tme)
	// xym7 = test      | free
	// xym15 =          | test

	if (use_lod)
	{
		SampleTextureLOD();
	}
	else
	{
		SampleTexture();
	}

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// t2 = za
	// xym2 = free
	// xym3 = free
	// xym4 = free
	// xym5 = rb
	// xym6 = ga
	// xym7 = test | free
	// xym15 =     | test

	AlphaTFX();

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// t2 = za
	// xym2 = gaf (TFX_HIGHLIGHT || TFX_HIGHLIGHT2 && !tcc) | free
	// xym3 = free | free
	// xym4 = free | free
	// xym5 = rb
	// xym6 = ga
	// xym7 = test | free
	// xym15 =     | test

	ReadMask();

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// t2 = za
	// xym2 = gaf (TFX_HIGHLIGHT || TFX_HIGHLIGHT2 && !tcc) | free
	// xym3 = fm
	// xym4 = zm
	// xym5 = rb
	// xym6 = ga
	// xym7 = test | free
	// xym15 =     | test

	TestAlpha();

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// t2 = za
	// xym2 = gaf (TFX_HIGHLIGHT || TFX_HIGHLIGHT2 && !tcc) | free
	// xym3 = fm
	// xym4 = zm
	// xym5 = rb
	// xym6 = ga
	// xym7 = test | free
	// xym15 =     | test

	ColorTFX();

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// t2 = za
	// xym2 = free
	// xym3 = fm
	// xym4 = zm
	// xym5 = rb
	// xym6 = ga
	// xym7 = test | free
	// xym15 =     | test

	Fog();

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// t2 = za
	// xym2 = free
	// xym3 = fm
	// xym4 = zm
	// xym5 = rb
	// xym6 = ga
	// xym7 = test | free
	// xym15 =     | test

	ReadFrame();

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// t2 = za
	// ebx = fa
	// xym2 = fd
	// xym3 = fm
	// xym4 = zm
	// xym5 = rb
	// xym6 = ga
	// xym7 = test | free
	// xym15 =     | test

	TestDestAlpha();

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// t2 = za
	// ebx = fa
	// xym2 = fd
	// xym3 = fm
	// xym4 = zm
	// xym5 = rb
	// xym6 = ga
	// xym7 = test | free
	// xym15 =     | test

	WriteMask();

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// t2 = za
	// edx = fzm
	// ebx = fa
	// xym2 = fd
	// xym3 = fm
	// xym4 = zm
	// xym5 = rb
	// xym6 = ga

	WriteZBuf();

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// edx = fzm
	// ebx = fa
	// xym2 = fd
	// xym3 = fm
	// xym4 = free
	// xym5 = rb
	// xym6 = ga

	AlphaBlend();

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// edx = fzm
	// ebx = fa
	// xym2 = fd
	// xym3 = fm
	// xym4 = free
	// xym5 = rb
	// xym6 = ga

	WriteFrame();

L("step");

	// if(steps <= 0) break;

	if (!m_sel.edge)
	{
		test(a0.cvt32(), a0.cvt32());

		jle("exit", CodeGenerator::T_NEAR);

		Step();

		jmp("loop", CodeGenerator::T_NEAR);
	}

L("exit");

#ifdef _WIN32
	for (int i = 0; i < 10; i++)
	{
		movdqa(Xmm(i + 6), ptr[rsp + _64_win_xmm_start + 16 * i]);
	}
	add(rsp, _64_win_stack_size);

	pop(r14);
	pop(r13);
	pop(r12);
	pop(rdi);
	pop(rsi);
	pop(rbx);
#else
	mov(rbx, ptr[rsp + _64_rz_rbx]);
	if (!m_rip)
	{
		mov(r12, ptr[rsp + _64_rz_r12]);
		mov(r13, ptr[rsp + _64_rz_r13]);
	}
	mov(r14, ptr[rsp + _64_rz_r14]);
	mov(r15, ptr[rsp + _64_rz_r15]);
#endif
	pop(rbp);
	if (isYmm)
		vzeroupper();
	ret();
}

/// Inputs: a0=pixels, a1=left, a2[x64]=top, a3[x64]=v
void GSDrawScanlineCodeGenerator2::Init()
{
	if (!m_sel.notest)
	{
		// int skip = left & 3;

		mov(ebx, a1.cvt32());
		and(a1.cvt32(), vecints - 1);

		// left -= skip;

		sub(ebx, a1.cvt32());

		// int steps = pixels + skip - 4;

		lea(a0.cvt32(), ptr[a0 + a1 - vecints]);

		// GSVector4i test = m_test[skip] | m_test[7 + (steps & (steps >> 31))];

		mov(eax, a0.cvt32());
		sar(eax, 31); // GH: 31 to extract the sign of the register
		and(eax, a0.cvt32());
		if (isXmm)
			shl(eax, 4); // * sizeof(m_test[0])
		cdqe();

		if (isXmm)
		{
			shl(a1.cvt32(), 4); // * sizeof(m_test[0])
			movdqa(_test, ptr[a1 + _g_const + offsetof(GSScanlineConstantData, m_test_128b[0])]);
			por(_test, ptr[rax + _g_const + offsetof(GSScanlineConstantData, m_test_128b[7])]);
		}
		else
		{
			pmovsxbd(_test, ptr[a1 * 8 + _g_const + offsetof(GSScanlineConstantData, m_test_256b[0])]);
			pmovsxbd(xym0, ptr[rax * 8 + _g_const + offsetof(GSScanlineConstantData, m_test_256b[15])]);
			por(_test, xym0);
			shl(a1.cvt32(), 5); // * sizeof(m_test[0])
		}
	}
	else
	{
		mov(ebx, a1.cvt32()); // left
		xor(a1.cvt32(), a1.cvt32()); // skip
		lea(a0.cvt32(), ptr[a0 - vecints]); // steps
	}

	// a0 = steps
	// a1 = skip
	// a2[x64] = top
	// a3[x64] = v
	// rbx = left
	// Free: rax, t0, t1

	// GSVector2i* fza_base = &m_local.gd->fzbr[top];
	mov(rax, _rip_global(fzbr));
	lea(t1, ptr[rax + a2 * 8]);

	// GSVector2i* fza_offset = &m_local.gd->fzbc[left >> 2];
	mov(rax, _rip_global(fzbc));
	lea(t0, ptr[rax + rbx * 2]);

	if (m_sel.prim != GS_SPRITE_CLASS && (m_sel.fwrite && m_sel.fge || m_sel.zb) || m_sel.fb && (m_sel.edge || m_sel.tfx != TFX_NONE || m_sel.iip))
	{
		// a1 = &m_local.d[skip] // note a1 was (skip << 4)
		lea(rax, _rip_local(d));
		lea(a1, ptr[rax + a1 * 8]);
	}

	// a0 = steps      (rcx | rdi)
	// a1 = skip       (rdx | rsi)
	// a2[x64] = top   (r8  | rdx)
	// a3 = v          (rbx | rcx)
	// t0 = fza_offset (rdi | r8 )
	// t1 = fza_base   (rsi | r9 )
	// Free: rax

	const XYm& f = _f;

	if (m_sel.prim != GS_SPRITE_CLASS)
	{
		if (m_sel.fwrite && m_sel.fge || m_sel.zb)
		{
			if (m_sel.fwrite && m_sel.fge)
			{
				// f = GSVector4i(v.t).zzzzh().zzzz().add16(m_local.d[skip].f);
				if (isYmm)
					vbroadcastss(f, ptr[a3 + offsetof(GSVertexSW, t.w)]);
				else
					movss(f, ptr[a3 + offsetof(GSVertexSW, t.w)]); // v.t.w

				cvttps2dq(f, f);
				punpcklwd(f, f);
				pshufd(f, f, _MM_SHUFFLE(0, 0, 0, 0));
				paddw(f, ptr[a1 + offsetof(GSScanlineLocalData::skip, f)]);
			}

			if (m_sel.zb && m_sel.zequal)
			{
				Xmm zx(_z.getIdx());
				cvttsd2si(rax, ptr[a3 + offsetof(GSVertexSW, p.z)]);
				movd(zx, eax);
				if (hasAVX2)
					vpbroadcastd(_z, zx);
				else
					pshufd(_z, _z, _MM_SHUFFLE(0, 0, 0, 0));
			}
			else if (m_sel.zb)
			{
				// z = vp.zzzz() + m_local.d[skip].z;
				broadcastsd(xym1, ptr[a3 + offsetof(GSVertexSW, p.z)]); // v.p.z
				cvtps2pd(xym7, ptr[a1 + offsetof(GSScanlineLocalData::skip, z.I8[0])]);
				addpd(xym7, xym1);
				movaps(_rip_local(temp.z0), xym7);
				cvtps2pd(_z, ptr[a1 + offsetof(GSScanlineLocalData::skip, z.I8[vecsize/2])]);
				addpd(_z, xym1);
			}
		}
	}
	else
	{
		if (m_sel.fwrite && m_sel.fge)
			pbroadcastwLocal(_f, _rip_local(p.f));
	}

	if (m_sel.fb)
	{
		if (m_sel.edge)
		{
			// m_local.temp.cov = GSVector8i::broadcast16(GSVector4i::cast(scan.p)).srl16(9);

			if (hasAVX2)
			{
				vpbroadcastw(xym3, ptr[a3 + offsetof(GSVertexSW, p.x)]);
			}
			else
			{
				movd(xmm3, ptr[a3 + offsetof(GSVertexSW, p.x)]);
				punpcklwd(xmm3, xmm3);
				pshufd(xmm3, xmm3, _MM_SHUFFLE(0, 0, 0, 0));
			}
			psrlw(xym3, 9);

			movdqa(_rip_local(temp.cov), xym3);
		}

		if (m_sel.tfx != TFX_NONE)
		{
			const XYm& vt = xym4;

			broadcastf128(vt, ptr[a3 + offsetof(GSVertexSW, t)]); // v.t

			// a1 = &m_local.d[skip]

			const XYm& s = _s;
			const XYm& t = _t;

			if (m_sel.fst)
			{
				// GSVector4i vti(vt);

				cvttps2dq(xym6, vt);

				// s = vti.xxxx() + m_local.d[skip].s;
				// t = vti.yyyy(); if(!sprite) t += m_local.d[skip].t;

				pshufd(s, xym6, _MM_SHUFFLE(0, 0, 0, 0));
				pshufd(t, xym6, _MM_SHUFFLE(1, 1, 1, 1));

				paddd(s, ptr[a1 + offsetof(GSScanlineLocalData::skip, s)]);

				if (m_sel.prim != GS_SPRITE_CLASS || m_sel.mmin)
				{
					paddd(t, ptr[a1 + offsetof(GSScanlineLocalData::skip, t)]);
				}
				else if (m_sel.ltf)
				{
					XYm vf = xym5;
					pshuflw(vf, t, _MM_SHUFFLE(2, 2, 0, 0));
					pshufhw(vf, vf, _MM_SHUFFLE(2, 2, 0, 0));
					psrlw(vf, 12);
					movdqa(_rip_local(temp.vf), vf);
				}
			}
			else
			{
				const XYm& q = _q;

				// s = vt.xxxx() + m_local.d[skip].s;
				// t = vt.yyyy() + m_local.d[skip].t;
				// q = vt.zzzz() + m_local.d[skip].q;

				if (hasAVX)
				{
					vshufps(s, vt, vt, _MM_SHUFFLE(0, 0, 0, 0));
					vshufps(t, vt, vt, _MM_SHUFFLE(1, 1, 1, 1));
					vshufps(q, vt, vt, _MM_SHUFFLE(2, 2, 2, 2));
				}
				else
				{
					movaps(s, vt);
					movaps(t, vt);
					movaps(q, vt);

					shufps(s, s, _MM_SHUFFLE(0, 0, 0, 0));
					shufps(t, t, _MM_SHUFFLE(1, 1, 1, 1));
					shufps(q, q, _MM_SHUFFLE(2, 2, 2, 2));
				}

				addps(s, ptr[a1 + offsetof(GSScanlineLocalData::skip, s)]);
				addps(t, ptr[a1 + offsetof(GSScanlineLocalData::skip, t)]);
				addps(q, ptr[a1 + offsetof(GSScanlineLocalData::skip, q)]);
			}
		}

		if (!(m_sel.tfx == TFX_DECAL && m_sel.tcc))
		{
			const XYm& f_rb = _f_rb;
			const XYm& f_ga = _f_ga;
			if (m_sel.iip)
			{
				// GSVector4i vc = GSVector4i(v.c);

				if (isXmm)
				{
					cvttps2dq(xym6, ptr[a3 + offsetof(GSVertexSW, c)]); // v.c
				}
				else
				{
					vbroadcastf128(ymm6, ptr[a3 + offsetof(GSVertexSW, c)]);
					cvttps2dq(ymm6, ymm6);
				}

				// vc = vc.upl16(vc.zwxy());

				pshufd(xym5, xym6, _MM_SHUFFLE(1, 0, 3, 2));
				punpcklwd(xym6, xym5);

				// rb = vc.xxxx().add16(m_local.d[skip].rb);
				// ga = vc.zzzz().add16(m_local.d[skip].ga);

				pshufd(f_rb, xym6, _MM_SHUFFLE(0, 0, 0, 0));
				pshufd(f_ga, xym6, _MM_SHUFFLE(2, 2, 2, 2));

				paddw(f_rb, ptr[a1 + offsetof(GSScanlineLocalData::skip, rb)]);
				paddw(f_ga, ptr[a1 + offsetof(GSScanlineLocalData::skip, ga)]);
			}
			else
			{
				movdqa(f_rb, _rip_local(c.rb));
				movdqa(f_ga, _rip_local(c.ga));
			}

			movdqa(_rb, _f_rb);
			movdqa(_ga, _f_ga);
		}
	}

	if (m_sel.fwrite && m_sel.fpsm == 2 && m_sel.dthe)
	{
		// On linux, a2 is edx which will be used for fzm
		// In all case, it will require a mov in dthe code, so let's keep the value on the stack
		mov(ptr[rsp + _top], a2);
	}

	mov(_64_m_local__gd__vm, _rip_global(vm));
	if (m_sel.fb && m_sel.tfx != TFX_NONE)
	{
		if (use_lod)
			lea(_64_m_local__gd__tex, _rip_global(tex));
		else
			mov(_64_m_local__gd__tex, _rip_global(tex));
	}
}

/// Inputs: a0=steps, t0=fza_offset
/// Outputs[x86]: xym0=z xym2=s, xym3=t, xym4=q, xym5=rb, xym6=ga, xym7=test
/// Destroys[x86]: all
/// Destroys[x64]: xym0, xym1, xym2, xym3
void GSDrawScanlineCodeGenerator2::Step()
{
	// steps -= 4;

	sub(a0.cvt32(), vecints);

	// fza_offset++;

	add(t0, vecsize / 2);

	const XYm& f = _f;

	if (m_sel.prim != GS_SPRITE_CLASS)
	{
		// z += m_local.d4.z;

		if (m_sel.zb && !m_sel.zequal)
		{
			broadcastsd(xym7, _rip_local_d_p(z));
			addpd(_z, xym7);
			addpd(xym7, _rip_local(temp.z0));
			movaps(_rip_local(temp.z0), xym7);
		}

		// f = f.add16(m_local.d4.f);

		if (m_sel.fwrite && m_sel.fge)
		{
			BROADCAST_AND_OP(vpbroadcastw, paddw, f, xym0, _rip_local_d_p(f));
		}
	}

	if (m_sel.fb)
	{
		if (m_sel.tfx != TFX_NONE)
		{
			if (m_sel.fst)
			{
				const XYm& stq = xym0;
				// GSVector4i stq = m_local.d4.stq;

				// s += stq.xxxx();
				// if(!sprite) t += st.yyyy();

				broadcasti128(stq, _rip_local_d(stq));

				XYm s = xym1;
				pshufd(s, stq, _MM_SHUFFLE(0, 0, 0, 0));
				paddd(_s, s);

				XYm t = xym1;
				if (m_sel.prim != GS_SPRITE_CLASS || m_sel.mmin)
				{
					pshufd(t, stq, _MM_SHUFFLE(1, 1, 1, 1));
					paddd(_t, t);
				}
			}
			else
			{
				const XYm& s = xym2;
				const XYm& t = xym3;
				const XYm& q = xym1;
				// GSVector4 stq = m_local.d4.stq;

				// s += stq.xxxx();
				// t += stq.yyyy();
				// q += stq.zzzz();

				if (hasAVX)
				{
					broadcastf128(q, _rip_local_d(stq));

					vshufps(s, q, q, _MM_SHUFFLE(0, 0, 0, 0));
					vshufps(t, q, q, _MM_SHUFFLE(1, 1, 1, 1));
					vshufps(q, q, q, _MM_SHUFFLE(2, 2, 2, 2));
				}
				else
				{
					movaps(q, _rip_local_d(stq));
					movaps(s, q);
					movaps(t, q);

					shufps(s, s, _MM_SHUFFLE(0, 0, 0, 0));
					shufps(t, t, _MM_SHUFFLE(1, 1, 1, 1));
					shufps(q, q, _MM_SHUFFLE(2, 2, 2, 2));
				}

				addps(_s, s);
				addps(_t, t);
				addps(_q, q);
			}
		}

		if (!(m_sel.tfx == TFX_DECAL && m_sel.tcc))
		{
			if (m_sel.iip)
			{
				XYm c = xym0;
				// GSVector4i c = m_local.d4.c;

				// rb = rb.add16(c.xxxx());
				// ga = ga.add16(c.yyyy());

				pbroadcastqLocal(c, _rip_local_d(c));

				pshufd(_rb, c, _MM_SHUFFLE(0, 0, 0, 0));
				pshufd(_ga, c, _MM_SHUFFLE(1, 1, 1, 1));

				paddw(_f_rb, _rb);
				paddw(_f_ga, _ga);

				// FIXME: color may underflow and roll over at the end of the line, if decreasing

				pxor(c, c);
				pmaxsw(_f_rb, c);
				pmaxsw(_f_ga, c);
			}

			movdqa(_rb, _f_rb);
			movdqa(_ga, _f_ga);
		}
	}

	if (!m_sel.notest)
	{
		// test = m_test[7 + (steps & (steps >> 31))];

		mov(eax, a0.cvt32());
		sar(eax, 31); // GH: 31 to extract the sign of the register
		and(eax, a0.cvt32());
		if (isXmm)
			shl(eax, 4);
		cdqe();

#if USING_XMM
		movdqa(_test, ptr[rax + _g_const + offsetof(GSScanlineConstantData, m_test_128b[7])]);
#else
		pmovsxbd(_test, ptr[rax * 8 + _g_const + offsetof(GSScanlineConstantData, m_test_256b[15])]);
#endif
	}
}

/// Inputs: xym0[x86]=z, xym7[x64]=z0, t1=fza_base, t0=fza_offset, _test
/// Outputs: t2=za
/// Destroys: rax, xym0, temp1, temp2
void GSDrawScanlineCodeGenerator2::TestZ(const XYm& temp1, const XYm& temp2)
{
	if (!m_sel.zb)
	{
		return;
	}

	// int za = fza_base.y + fza_offset->y;

	mov(t2.cvt32(), dword[t1 + 4]);
	add(t2.cvt32(), dword[t0 + 4]);
	and(t2.cvt32(), HALF_VM_SIZE - 1);

	// GSVector4i zs = zi;

	if (m_sel.prim != GS_SPRITE_CLASS)
	{
		if (m_sel.zequal)
		{
			movdqa(xym0, _z);
		}
		else if (m_sel.zoverflow)
		{
			// GSVector4i zl = z0.add64(VectorF::m_xc1e00000000fffff).f64toi32();
			// GSVector4i zh = z1.add64(VectorF::m_xc1e00000000fffff).f64toi32();
			// zs = GSVector8i(zl, zh);
			// zs += VectorI::x80000000();

			auto m_imin = loadAddress(rax, &GSVector4::m_xc1e00000000fffff);
			broadcastsd(temp1, ptr[m_imin]);

			addpd(xym7, temp1);
			addpd(temp1, _z);
			cvtpd2dq(xmm0, xym7);
			cvtpd2dq(Xmm(temp1.getIdx()), temp1);

#if USING_YMM
			vinserti128(xym0, xym0, Xmm(temp1.getIdx()), 1);
#else
			punpcklqdq(xym0, temp1);
#endif

			pcmpeqd(temp1, temp1);
			pslld(temp1, 31);
			paddd(xym0, temp1);
		}
		else
		{
			// zs = GSVector8i(z0.f64toi32(), z1.f64toi32());

			cvttpd2dq(xmm0, xym7);
			cvttpd2dq(Xmm(temp1.getIdx()), _z);
#if USING_YMM
			vinserti128(xym0, xym0, Xmm(temp1.getIdx()), 1);
#else
			punpcklqdq(xym0, temp1);
#endif
		}

		// Clamp Z to ZPSM_FMT_MAX
		if (m_sel.zclamp)
		{
			const u8 amt = (u8)((m_sel.zpsm & 0x3) * 8);
			pcmpeqd(temp1, temp1);
			psrld(temp1, amt);
			pminud(xym0, temp1);
		}

		if (m_sel.zwrite)
		{
			movdqa(_rip_local(temp.zs), xym0);
		}
	}
	else
	{
		pbroadcastdLocal(xym0, _rip_local(p.z));
	}

	if (m_sel.ztest)
	{
		ReadPixel(temp2, temp1, t2);

		if (m_sel.zwrite && m_sel.zpsm < 2)
		{
			movdqa(_rip_local(temp.zd), temp2);
		}

		// zd &= 0xffffffff >> m_sel.zpsm * 8;

		if (m_sel.zpsm)
		{
			pslld(temp2, static_cast<u8>(m_sel.zpsm * 8));
			psrld(temp2, static_cast<u8>(m_sel.zpsm * 8));
		}

		if (m_sel.zpsm == 0)
		{
			// GSVector4i o = GSVector4i::x80000000();

			pcmpeqd(temp1, temp1);
			pslld(temp1, 31);

			// GSVector4i zso = zs - o;
			// GSVector4i zdo = zd - o;

			psubd(xym0, temp1);
			psubd(temp2, temp1);
		}

		switch (m_sel.ztst)
		{
			case ZTST_GEQUAL:
				// test |= zso < zdo; // ~(zso >= zdo)
				pcmpgtd(temp2, xym0);
				por(_test, temp2);
				break;

			case ZTST_GREATER: // TODO: tidus hair and chocobo wings only appear fully when this is tested as ZTST_GEQUAL
				// test |= zso <= zdo; // ~(zso > zdo)
				pcmpgtd(xym0, temp2);
				pcmpeqd(temp1, temp1);
				pxor(xym0, temp1);
				por(_test, xym0);
				break;
		}

		alltrue(_test);
	}
}

/// Input[x86]: xym4=q, xym2=s, xym3=t
/// Output: _rb, _ga
/// Destroys everything except xym7[x86]
void GSDrawScanlineCodeGenerator2::SampleTexture()
{
	if (!m_sel.fb || m_sel.tfx == TFX_NONE)
	{
		return;
	}

	const bool needsMoreRegs = isYmm;

	if (!m_sel.fst)
	{
		rcpps(xym0, _q);

		MOVE_IF_64(mulps, xym2, _s, xym0);
		MOVE_IF_64(mulps, xym3, _t, xym0);

		cvttps2dq(xym2, xym2);
		cvttps2dq(xym3, xym3);

		if (m_sel.ltf)
		{
			// u -= 0x8000;
			// v -= 0x8000;

			mov(eax, 0x8000);
			broadcastGPRToVec(xym1, eax);

			psubd(xym2, xym1);
			psubd(xym3, xym1);
		}
	}
	else
	{
		movdqa(xym2, _s);
		movdqa(xym3, _t);
	}

	if (m_sel.ltf)
	{
		const XYm& vf = xym7;

		// GSVector4i uf = u.xxzzlh().srl16(12);

		pshuflw(xym4, xym2, _MM_SHUFFLE(2, 2, 0, 0));
		pshufhw(xym4, xym4, _MM_SHUFFLE(2, 2, 0, 0));
		psrlw(xym4, 12);

		if (m_sel.prim != GS_SPRITE_CLASS)
		{
			// GSVector4i vf = v.xxzzlh().srl16(12);

			pshuflw(vf, xym3, _MM_SHUFFLE(2, 2, 0, 0));
			pshufhw(vf, vf, _MM_SHUFFLE(2, 2, 0, 0));
			psrlw(vf, 12);
			if (needsMoreRegs)
				movdqa(_rip_local(temp.vf), vf);
		}
		else if (!needsMoreRegs)
		{
			movdqa(vf, _rip_local(temp.vf));
		}
	}

	// GSVector4i uv0 = u.sra32(16).ps32(v.sra32(16));

	psrad(xym2, 16);
	psrad(xym3, 16);
	packssdw(xym2, xym3);

	if (m_sel.ltf)
	{
		// GSVector4i uv1 = uv0.add16(GSVector4i::x0001());

		pcmpeqd(xym0, xym0);
		psrlw(xym0, 15);
		THREEARG(paddw, xym3, xym2, xym0);

		// uv0 = Wrap(uv0);
		// uv1 = Wrap(uv1);

		Wrap(xym2, xym3);
	}
	else
	{
		// uv0 = Wrap(uv0);

		Wrap(xym2);
	}

	// xym2 = uv0
	// xym3 = uv1
	// xym4 = uf[x64||!needsMoreRegs]
	// xym7 = used[x86] vf[x64&&!needsMoreRegs]
	// Free: xym0, xym1, xym5, xym6

	SampleTexture_TexelReadHelper(0);

	// xym5 = rb (xym5[x86], xym2[x64])
	// xym6 = ga (xym6[x86], xym3[x64])
}

/// Input[x86]: xym2=uv0, xym3=uv1 (ltf), xym4=uf (!needsMoreRegs)
/// Input[x64]: xym2=uv0, xym3=uv1 (ltf), xym4=uf, xym7=vf (!needsMoreRegs)
/// Output: _rb, _ga
/// Destroys all registers except outputs, xmm4 and xmm7
void GSDrawScanlineCodeGenerator2::SampleTexture_TexelReadHelper(int mip_offset)
{
	const bool needsMoreRegs = isYmm;

	// GSVector4i x0 = uv0.upl16();
	// GSVector4i y0 = uv0.uph16() << tw;

	pxor(xym0, xym0);

	THREEARG(punpcklwd, xym5, xym2, xym0);
	punpckhwd(xym2, xym0);
	pslld(xym2, static_cast<u8>(m_sel.tw + 3));

	// xym0 = 0
	// xym2 = y0
	// xym3 = uv1 (ltf)
	// xym4 = uf[x64||!needsMoreRegs]
	// xym5 = x0
	// xym7 = used[x86] vf[x64&&!needsMoreRegs]
	// Free: xym1, xym6

	if (m_sel.ltf)
	{
		// GSVector4i x1 = uv1.upl16();
		// GSVector4i y1 = uv1.uph16() << tw;

		THREEARG(punpcklwd, xym1, xym3, xym0);
		punpckhwd(xym3, xym0);
		pslld(xym3, static_cast<u8>(m_sel.tw + 3));

		// xym1 = x1
		// xym2 = y0
		// xym3 = y1
		// xym4 = uf[x64||!needsMoreRegs]
		// xym5 = x0
		// xym7 = used[x86] vf[x64&&!needsMoreRegs]
		// Free: xym0, xym6

		// GSVector4i addr00 = y0 + x0;
		// GSVector4i addr01 = y0 + x1;
		// GSVector4i addr10 = y1 + x0;
		// GSVector4i addr11 = y1 + x1;

		THREEARG(paddd, xym0, xym3, xym1); // addr11
		paddd(xym1, xym2); // addr01
		paddd(xym2, xym5); // addr00
		paddd(xym3, xym5); // addr10

		// xym0 = addr11
		// xym1 = addr01
		// xym2 = addr00
		// xym3 = addr10
		// xym4 = uf[x64||!needsMoreRegs]
		// xym7 = used[x86] vf[x64&&!needsMoreRegs]
		// Free: xym4, xym5

		// c00 = addr00.gather32_32((const u32/u8*)tex[, clut]);
		// c01 = addr01.gather32_32((const u32/u8*)tex[, clut]);
		// c10 = addr10.gather32_32((const u32/u8*)tex[, clut]);
		// c11 = addr11.gather32_32((const u32/u8*)tex[, clut]);

		const XYm& tmp1 = xym7; // OK to destroy if needsMoreRegs
		const XYm& tmp2 = xym4;
		//         d0    d1    d2s0  d3s1  s2    s3
		ReadTexel4(xym5, xym6, xym0, xym2, xym1, xym3, tmp1, tmp2, mip_offset);

		// xym0 = c01
		// xym2 = c10
		// xym4 = uf[x64||!needsMoreRegs]
		// xym5 = c11
		// xym6 = c00
		// xym7 = used[x86] vf[x64&&!needsMoreRegs]

		// GSVector4i rb00 = c00 & mask;
		// GSVector4i ga00 = (c00 >> 8) & mask;

		split16_2x8(xym3, xym6, xym6);

		// GSVector4i rb01 = c01 & mask;
		// GSVector4i ga01 = (c01 >> 8) & mask;

		split16_2x8(xym0, xym1, xym0);

		// xym0 = rb01
		// xym1 = ga01
		// xym2 = c10
		// xym3 = rb00
		// xym4 = uf
		// xym5 = c11
		// xym6 = ga00
		// xym7 = used[x86] vf[x64&&!needsMoreRegs]

		// rb00 = rb00.lerp16_4(rb01, uf);
		// ga00 = ga00.lerp16_4(ga01, uf);

		lerp16_4(xym0, xym3, xym4);
		lerp16_4(xym1, xym6, xym4);

		// xym0 = rb00
		// xym1 = ga00
		// xym2 = c10
		// xym4 = uf
		// xym5 = c11
		// xym7 = used[x86] vf[x64&&!needsMoreRegs]

		// GSVector4i rb10 = c10 & mask;
		// GSVector4i ga10 = (c10 >> 8) & mask;

		split16_2x8(xym2, xym3, xym2);

		// GSVector4i rb11 = c11 & mask;
		// GSVector4i ga11 = (c11 >> 8) & mask;

		split16_2x8(xym5, xym6, xym5);

		// xym0 = rb00
		// xym1 = ga00
		// xym2 = rb10
		// xym3 = ga10
		// xym4 = uf
		// xym5 = rb11
		// xym6 = ga11
		// xym7 = used[x86] vf[x64&&!needsMoreRegs]

		// rb10 = rb10.lerp16_4(rb11, uf);
		// ga10 = ga10.lerp16_4(ga11, uf);

		lerp16_4(xym5, xym2, xym4);
		lerp16_4(xym6, xym3, xym4);

		// xym0 = rb00
		// xym1 = ga00
		// xym5 = rb10
		// xym6 = ga10
		// xym7 = used[x86] vf[x64&&!needsMoreRegs]

		// rb00 = rb00.lerp16_4(rb10, vf);
		// ga00 = ga00.lerp16_4(ga10, vf);

		XYm vf = xym7;
		if (needsMoreRegs)
			movdqa(vf, _rip_local(temp.vf));

		lerp16_4(xym5, xym0, vf);
		lerp16_4(xym6, xym1, vf);
	}
	else
	{
		// GSVector4i addr00 = y0 + x0;

		paddd(xym2, xym5);

		// c00 = addr00.gather32_32((const u32/u8*)tex[, clut]);

		ReadTexel1(xym5, xym2, xym0, xym1, mip_offset);

		// GSVector4i mask = GSVector4i::x00ff();

		// c[0] = c00 & mask;
		// c[1] = (c00 >> 8) & mask;

		split16_2x8(xym5, xym6, xym5);
	}
}

void GSDrawScanlineCodeGenerator2::Wrap(const XYm& uv)
{
	// Registers free from SampleTexture
	const XYm& mask = xym0;
	const XYm& min = xym1;
	const XYm& max = xym5;
	const XYm& tmp = xym6;

	int wms_clamp = ((m_sel.wms + 1) >> 1) & 1;
	int wmt_clamp = ((m_sel.wmt + 1) >> 1) & 1;

	int region = ((m_sel.wms | m_sel.wmt) >> 1) & 1;

	if (wms_clamp == wmt_clamp)
	{
		if (wms_clamp)
		{
			if (region)
			{
				BROADCAST_AND_OP(vbroadcasti128, pmaxsw, uv, min, _rip_global(t.min));
			}
			else
			{
				pxor(tmp, tmp);
				pmaxsw(uv, tmp);
			}

			BROADCAST_AND_OP(vbroadcasti128, pminsw, uv, max, _rip_global(t.max));
		}
		else
		{
			BROADCAST_AND_OP(vbroadcasti128, pand, uv, min, _rip_global(t.min));

			if (region)
			{
				BROADCAST_AND_OP(vbroadcasti128, por, uv, max, _rip_global(t.max));
			}
		}
	}
	else
	{
		broadcasti128(min, _rip_global(t.min));
		broadcasti128(max, _rip_global(t.max));
		broadcasti128(mask, _rip_global(t.mask));

		// GSVector4i repeat = (t & m_local.gd->t.min) | m_local.gd->t.max;
		THREEARG(pand, tmp, uv, min);
		if (region)
			por(tmp, max);
		// GSVector4i clamp = t.sat_i16(m_local.gd->t.min, m_local.gd->t.max);
		pmaxsw(uv, min);
		pminsw(uv, max);
		// clamp.blend8(repeat, m_local.gd->t.mask);
		blend8(uv, tmp /*, xym0==mask */);
	}
}

/// Destroys[x86]: xym0, xym1, xym2, xym3, xym4[!sse41]
/// Destroys[x64]: xym0, xym1, xym5, xym6, xym7[!sse41]
void GSDrawScanlineCodeGenerator2::Wrap(const XYm& uv0, const XYm& uv1)
{
	// Registers free from SampleTexture
	const XYm& mask = xym0;
	const XYm& min = xym1;
	const XYm& max = xym5;
	const XYm& tmp = xym6;

	int wms_clamp = ((m_sel.wms + 1) >> 1) & 1;
	int wmt_clamp = ((m_sel.wmt + 1) >> 1) & 1;

	int region = ((m_sel.wms | m_sel.wmt) >> 1) & 1;

	if (wms_clamp == wmt_clamp)
	{
		if (wms_clamp)
		{
			if (region)
			{
				broadcasti128(min, _rip_global(t.min));
				pmaxsw(uv0, min);
				pmaxsw(uv1, min);
			}
			else
			{
				pxor(tmp, tmp);
				pmaxsw(uv0, tmp);
				pmaxsw(uv1, tmp);
			}

			broadcasti128(max, _rip_global(t.max));
			pminsw(uv0, max);
			pminsw(uv1, max);
		}
		else
		{
			broadcasti128(min, _rip_global(t.min));
			pand(uv0, min);
			pand(uv1, min);

			if (region)
			{
				broadcasti128(max, _rip_global(t.max));
				por(uv0, max);
				por(uv1, max);
			}
		}
	}
	else
	{
		broadcasti128(min, _rip_global(t.min));
		broadcasti128(max, _rip_global(t.max));
		broadcasti128(mask, _rip_global(t.mask));

		for (const XYm& uv : {uv0, uv1})
		{
			// GSVector4i repeat = (t & m_local.gd->t.min) | m_local.gd->t.max;
			THREEARG(pand, tmp, uv, min);
			if (region)
				por(tmp, max);
			// GSVector4i clamp = t.sat_i16(m_local.gd->t.min, m_local.gd->t.max);
			pmaxsw(uv, min);
			pminsw(uv, max);
			// clamp.blend8(repeat, m_local.gd->t.mask);
			pblendvb(uv, tmp /*, xym0==mask */);
		}
	}
}

/// Input[x86]: xym4=q, xym2=s, xym3=t
/// Output: _rb, _ga
/// Destroys everything except xym7[x86]
void GSDrawScanlineCodeGenerator2::SampleTextureLOD()
{
	if (!m_sel.fb || m_sel.tfx == TFX_NONE)
	{
		return;
	}

	const bool needsMoreRegs = isYmm;

	movdqa(xym4, _q);

	if (!m_sel.fst)
	{
		rcpps(xym0, xym4);

		MOVE_IF_64(mulps, xym2, _s, xym0);
		MOVE_IF_64(mulps, xym3, _t, xym0);

		cvttps2dq(xym2, xym2);
		cvttps2dq(xym3, xym3);
	}
	else
	{
		movdqa(xym2, _s);
		movdqa(xym3, _t);
	}

	// xym2 = u
	// xym3 = v
	// xym4 = q
	// xym0 = xym1 = xym5 = xym6 = free

	// TODO: if the fractional part is not needed in round-off mode then there is a faster integer log2 (just take the exp) (but can we round it?)

	if (!m_sel.lcm)
	{
		// lod = -log2(Q) * (1 << L) + K

		pcmpeqd(xym1, xym1);
		psrld(xym1, 25);
		THREEARG(pslld, xym0, xym4, 1);
		psrld(xym0, 24);
		psubd(xym0, xym1);
		cvtdq2ps(xym0, xym0);

		// xym0 = (float)(exp(q) - 127)

		pslld(xym4, 9);
		psrld(xym4, 9);

		auto log2_coeff = [this](int i) -> Address
		{
			if (isXmm)
				return ptr[_g_const + OFFSETOF(GSScanlineConstantData, m_log2_coef_128b[i])];
			else
				return ptr[_g_const + OFFSETOF(GSScanlineConstantData, m_log2_coef_256b[i])];
		};

		orps(xym4, log2_coeff(3));

		// xym4 = mant(q) | 1.0f

		if (hasFMA)
		{
			movaps(xym5, log2_coeff(0)); // c0
			vfmadd213ps(xym5, xym4, log2_coeff(1)); // c0 * xym4 + c1
			vfmadd213ps(xym5, xym4, log2_coeff(2)); // (c0 * xym4 + c1) * xym4 + c2
			subps(xym4, log2_coeff(3)); // xym4 - 1.0f
			vfmadd213ps(xym4, xym5, xym0); // ((c0 * xym4 + c1) * xym4 + c2) * (xym4 - 1.0f) + xym0
		}
		else
		{
			THREEARG(mulps, xym5, xym4, log2_coeff(0));
			addps(xym5, log2_coeff(1));
			mulps(xym5, xym4);
			subps(xym4, log2_coeff(3));
			addps(xym5, log2_coeff(2));
			mulps(xym4, xym5);
			addps(xym4, xym0);
		}

		// xym4 = log2(Q) = ((((c0 * xym4) + c1) * xym4) + c2) * (xym4 - 1.0f) + xym0

		if (hasFMA)
		{
			movaps(xym5, _rip_global(l));
			vfmadd213ps(xym4, xym5, _rip_global(k));
		}
		else
		{
			mulps(xym4, _rip_global(l));
			addps(xym4, _rip_global(k));
		}

		// xym4 = (-log2(Q) * (1 << L) + K) * 0x10000

		xorps(xym0, xym0);
		minps(xym4, _rip_global(mxl));
		maxps(xym4, xym0);
		cvtps2dq(xym4, xym4);

		if (m_sel.mmin == 1) // round-off mode
		{
			mov(eax, 0x8000);
			broadcastGPRToVec(xym0, eax);
			paddd(xym4, xym0);
		}

		THREEARG(psrld, xym0, xym4, 16);

		movdqa(_rip_local(temp.lod.i), xym0);
		/*
		 vpslld(xym5, xym0, 6);
		 vpslld(xym6, xym4, 16);
		 vpsrld(xym6, xym6, 24);
		 return;
		 */
		if (m_sel.mmin == 2) // trilinear mode
		{
			pshuflw(xym1, xym4, _MM_SHUFFLE(2, 2, 0, 0));
			pshufhw(xym1, xym1, _MM_SHUFFLE(2, 2, 0, 0));
			movdqa(_rip_local(temp.lod.f), xym1);
		}

		// shift u/v/minmax by (int)lod

		if (hasAVX2)
		{
			vpsravd(xym2, xym2, xym0);
			vpsravd(xym3, xym3, xym0);

			movdqa(_rip_local(temp.uv[0]), xym2);
			movdqa(_rip_local(temp.uv[1]), xym3);

			// m_local.gd->t.minmax => m_local.temp.uv_minmax[0/1]

			pxor(xym1, xym1);

			broadcasti128(xym4, _rip_global(t.min));
			vpunpcklwd(xym5, xym4, xym1); // minu
			vpunpckhwd(xym6, xym4, xym1); // minv
			vpsrlvd(xym5, xym5, xym0);
			vpsrlvd(xym6, xym6, xym0);
			packusdw(xym5, xym6);

			broadcasti128(xym4, _rip_global(t.max));
			vpunpcklwd(xym6, xym4, xym1); // maxu
			vpunpckhwd(xym4, xym4, xym1); // maxv
			vpsrlvd(xym6, xym6, xym0);
			vpsrlvd(xym4, xym4, xym0);
			packusdw(xym6, xym4);

			movdqa(_rip_local(temp.uv_minmax[0]), xym5);
			movdqa(_rip_local(temp.uv_minmax[1]), xym6);
		}
		else
		{
			movq(xym4, _rip_global(t.minmax));

			THREEARG(punpckhdq, xym6, xym2, xym3);
			punpckldq(xym2, xym3);
			movdqa(xym5, xym2);
			movdqa(xym3, xym6);

			movd(xym0, _rip_local(temp.lod.i.U32[0]));
			psrad(xym2, xym0);
			THREEARG(psrlw, xym1, xym4, xym0);
			movq(_rip_local(temp.uv_minmax[0].U32[0]), xym1);

			movd(xym0, _rip_local(temp.lod.i.U32[1]));
			psrad(xym5, xym0);
			THREEARG(psrlw, xym1, xym4, xym0);
			movq(_rip_local(temp.uv_minmax[1].U32[0]), xym1);

			movd(xym0, _rip_local(temp.lod.i.U32[2]));
			psrad(xym3, xym0);
			THREEARG(psrlw, xym1, xym4, xym0);
			movq(_rip_local(temp.uv_minmax[0].U32[2]), xym1);

			movd(xym0, _rip_local(temp.lod.i.U32[3]));
			psrad(xym6, xym0);
			THREEARG(psrlw, xym1, xym4, xym0);
			movq(_rip_local(temp.uv_minmax[1].U32[2]), xym1);

			punpckldq(xym2, xym3);
			punpckhdq(xym5, xym6);
			THREEARG(punpckhdq, xym3, xym2, xym5);
			punpckldq(xym2, xym5);

			movdqa(_rip_local(temp.uv[0]), xym2);
			movdqa(_rip_local(temp.uv[1]), xym3);

			movdqa(xym5, _rip_local(temp.uv_minmax[0]));
			movdqa(xym6, _rip_local(temp.uv_minmax[1]));

			if (hasAVX)
			{
				vpunpcklwd(xym0, xym5, xym6);
				vpunpckhwd(xym1, xym5, xym6);
				vpunpckldq(xym5, xym0, xym1);
				vpunpckhdq(xym6, xym0, xym1);
			}
			else
			{
				movdqa(xym0, xym5);
				punpcklwd(xym5, xym6);
				punpckhwd(xym0, xym6);
				movdqa(xym6, xym5);
				punpckldq(xym5, xym0);
				punpckhdq(xym6, xym0);
			}

			movdqa(_rip_local(temp.uv_minmax[0]), xym5);
			movdqa(_rip_local(temp.uv_minmax[1]), xym6);
		}
	}
	else
	{
		// lod = K

		movd(Xmm(xym0.getIdx()), _rip_global(lod.i.U32[0]));

		psrad(xym2, Xmm(xym0.getIdx()));
		psrad(xym3, Xmm(xym0.getIdx()));

		movdqa(_rip_local(temp.uv[0]), xym2);
		movdqa(_rip_local(temp.uv[1]), xym3);

		movdqa(xym5, _rip_local(temp.uv_minmax[0]));
		movdqa(xym6, _rip_local(temp.uv_minmax[1]));
	}

	// xym2 = m_local.temp.uv[0] = u (level m)
	// xym3 = m_local.temp.uv[1] = v (level m)
	// xym5 = minuv
	// xym6 = maxuv

	if (m_sel.ltf)
	{
		const XYm& vf = xym7;
		// u -= 0x8000;
		// v -= 0x8000;

		mov(eax, 0x8000);
		broadcastGPRToVec(xym4, eax);

		psubd(xym2, xym4);
		psubd(xym3, xym4);

		// GSVector4i uf = u.xxzzlh().srl16(1);

		pshuflw(xym4, xym2, _MM_SHUFFLE(2, 2, 0, 0));
		pshufhw(xym4, xym4, _MM_SHUFFLE(2, 2, 0, 0));
		psrlw(xym4, 12);

		// GSVector4i vf = v.xxzzlh().srl16(1);

		pshuflw(vf, xym3, _MM_SHUFFLE(2, 2, 0, 0));
		pshufhw(vf, vf, _MM_SHUFFLE(2, 2, 0, 0));
		psrlw(vf, 12);
		if (needsMoreRegs)
			movdqa(_rip_local(temp.vf), vf);
	}

	// GSVector4i uv0 = u.sra32(16).ps32(v.sra32(16));

	psrad(xym2, 16);
	psrad(xym3, 16);
	packssdw(xym2, xym3);

	if (m_sel.ltf)
	{
		// GSVector4i uv1 = uv0.add16(GSVector4i::x0001());

		pcmpeqd(xym1, xym1);
		psrlw(xym1, 15);
		THREEARG(paddw, xym3, xym2, xym1);

		// uv0 = Wrap(uv0);
		// uv1 = Wrap(uv1);

		WrapLOD(xym2, xym3);
	}
	else
	{
		// uv0 = Wrap(uv0);

		WrapLOD(xym2);
	}

	// xym2 = uv0
	// xym3 = uv1 (ltf)
	// xym4 = uf[x64||!needsMoreRegs]
	// xym7 = used[x86] vf[x64&&!needsMoreRegs]
	// Free: xym0, xym1, xym5, xym6

	SampleTexture_TexelReadHelper(0);

	// xym5: rb
	// xym6: ga


	if (m_sel.mmin != 1) // !round-off mode
	{
		movdqa(_rip_local(temp.trb), xym5);
		movdqa(_rip_local(temp.tga), xym6);

		movdqa(xym2, _rip_local(temp.uv[0]));
		movdqa(xym3, _rip_local(temp.uv[1]));

		psrad(xym2, 1);
		psrad(xym3, 1);

		movdqa(xym5, _rip_local(temp.uv_minmax[0]));
		movdqa(xym6, _rip_local(temp.uv_minmax[1]));

		psrlw(xym5, 1);
		psrlw(xym6, 1);

		if (m_sel.ltf)
		{
			const XYm& vf = xym7;
			// u -= 0x8000;
			// v -= 0x8000;

			mov(eax, 0x8000);
			broadcastGPRToVec(xym4, eax);

			psubd(xym2, xym4);
			psubd(xym3, xym4);

			// GSVector4i uf = u.xxzzlh().srl16(1);

			pshuflw(xym4, xym2, _MM_SHUFFLE(2, 2, 0, 0));
			pshufhw(xym4, xym4, _MM_SHUFFLE(2, 2, 0, 0));
			psrlw(xym4, 12);

			// GSVector4i vf = v.xxzzlh().srl16(1);

			pshuflw(vf, xym3, _MM_SHUFFLE(2, 2, 0, 0));
			pshufhw(vf, vf, _MM_SHUFFLE(2, 2, 0, 0));
			psrlw(vf, 12);
			if (needsMoreRegs)
				movdqa(_rip_local(temp.vf), vf);
		}

		// GSVector4i uv0 = u.sra32(16).ps32(v.sra32(16));

		psrad(xym2, 16);
		psrad(xym3, 16);
		packssdw(xym2, xym3);

		if (m_sel.ltf)
		{
			// GSVector4i uv1 = uv0.add16(GSVector4i::x0001());

			pcmpeqd(xym1, xym1);
			psrlw(xym1, 15);
			THREEARG(paddw, xym3, xym2, xym1);

			// uv0 = Wrap(uv0);
			// uv1 = Wrap(uv1);

			WrapLOD(xym2, xym3);
		}
		else
		{
			// uv0 = Wrap(uv0);

			WrapLOD(xym2);
		}

		// xym2 = uv0
		// xym3 = uv1 (ltf)
		// xym4 = uf[x64||!needsMoreRegs]
		// xym7 = used[x86] vf[x64&&!needsMoreRegs]
		// Free: xym0, xym1, xym5, xym6

		SampleTexture_TexelReadHelper(1);

		// xym5: rb
		// xym6: ga

		movdqa(xym0, m_sel.lcm ? _rip_global(lod.f) : _rip_local(temp.lod.f));
		psrlw(xym0, 1);

		movdqa(xym2, _rip_local(temp.trb));
		movdqa(xym3, _rip_local(temp.tga));

		lerp16(xym5, xym2, xym0, 0);
		lerp16(xym6, xym3, xym0, 0);
	}
}

void GSDrawScanlineCodeGenerator2::WrapLOD(const XYm& uv)
{
	// Registers free from SampleTexture
	const XYm& mask = xym0;
	const XYm& tmp = xym1;
	const XYm& min = xym5;
	const XYm& max = xym6;

	int wms_clamp = ((m_sel.wms + 1) >> 1) & 1;
	int wmt_clamp = ((m_sel.wmt + 1) >> 1) & 1;

	int region = ((m_sel.wms | m_sel.wmt) >> 1) & 1;

	if (wms_clamp == wmt_clamp)
	{
		if (wms_clamp)
		{
			if (region)
			{
				pmaxsw(uv, min);
			}
			else
			{
				pxor(tmp, tmp);
				pmaxsw(uv, tmp);
			}

			pminsw(uv, max);
		}
		else
		{
			pand(uv, min);

			if (region)
			{
				por(uv, max);
			}
		}
	}
	else
	{
		broadcasti128(mask, _rip_global(t.mask));

		// GSVector4i repeat = (t & m_local.gd->t.min) | m_local.gd->t.max;
		THREEARG(pand, tmp, uv, min);
		if (region)
			por(tmp, max);
		// GSVector4i clamp = t.sat_i16(m_local.gd->t.min, m_local.gd->t.max);
		pmaxsw(uv, min);
		pminsw(uv, max);
		// clamp.blend8(repeat, m_local.gd->t.mask);
		blend8(uv, tmp /*, xym0==mask */);
	}
}

void GSDrawScanlineCodeGenerator2::WrapLOD(const XYm& uv0, const XYm& uv1)
{
	// Registers free from SampleTexture
	const XYm& mask = xym0;
	const XYm& tmp = xym1;
	const XYm& min = xym5;
	const XYm& max = xym6;

	int wms_clamp = ((m_sel.wms + 1) >> 1) & 1;
	int wmt_clamp = ((m_sel.wmt + 1) >> 1) & 1;

	int region = ((m_sel.wms | m_sel.wmt) >> 1) & 1;

	if (wms_clamp == wmt_clamp)
	{
		if (wms_clamp)
		{
			if (region)
			{
				pmaxsw(uv0, min);
				pmaxsw(uv1, min);
			}
			else
			{
				pxor(tmp, tmp);
				pmaxsw(uv0, tmp);
				pmaxsw(uv1, tmp);
			}

			pminsw(uv0, max);
			pminsw(uv1, max);
		}
		else
		{
			pand(uv0, min);
			pand(uv1, min);

			if (region)
			{
				por(uv0, max);
				por(uv1, max);
			}
		}
	}
	else
	{
		broadcasti128(mask, _rip_global(t.mask));

		for (const XYm& uv : {uv0, uv1})
		{
			// GSVector4i repeat = (t & m_local.gd->t.min) | m_local.gd->t.max;
			THREEARG(pand, tmp, uv, min);
			if (region)
				por(tmp, max);
			// GSVector4i clamp = t.sat_i16(m_local.gd->t.min, m_local.gd->t.max);
			pmaxsw(uv, min);
			pminsw(uv, max);
			// clamp.blend8(repeat, m_local.gd->t.mask);*
			pblendvb(uv, tmp /*, xym0==mask */);
		}
	}
}

/// Input: _ga
/// Output: xym2[x86]=gaf (TFX_HIGHLIGHT || TFX_HIGHLIGHT2 && !tcc)
/// Destroys: xym0, xym1, xym3[x86], xym4[x86]
void GSDrawScanlineCodeGenerator2::AlphaTFX()
{
	if (!m_sel.fb)
	{
		return;
	}

	const XYm& f_ga  = _f_ga;
	const XYm& tmpga = xym1;
	const XYm& tmp   = xym0;

	switch (m_sel.tfx)
	{
		case TFX_MODULATE:

			// GSVector4i ga = iip ? gaf : m_local.c.ga;
			// gat = gat.modulate16<1>(ga).clamp8();

			modulate16(_ga, f_ga, 1);

			clamp16(_ga, tmp);

			// if(!tcc) gat = gat.mix16(ga.srl16(7));

			if (!m_sel.tcc)
			{
				MOVE_IF_64(psrlw, tmpga, f_ga, 7);

				mix16(_ga, tmpga, tmp);
			}

			break;

		case TFX_DECAL:

			// if(!tcc) gat = gat.mix16(ga.srl16(7));
			if (!m_sel.tcc)
			{
				// GSVector4i ga = iip ? gaf : m_local.c.ga;

				MOVE_IF_64(psrlw, tmpga, f_ga, 7);

				mix16(_ga, tmpga, tmp);
			}

			break;

		case TFX_HIGHLIGHT:

			// GSVector4i ga = iip ? gaf : m_local.c.ga;
			// gat = gat.mix16(!tcc ? ga.srl16(7) : gat.addus8(ga.srl16(7)));

			MOVE_IF_64(psrlw, tmpga, f_ga, 7);

			if (m_sel.tcc)
			{
				paddusb(tmpga, _ga);
			}

			mix16(_ga, tmpga, tmp);

			break;

		case TFX_HIGHLIGHT2:

			// if(!tcc) gat = gat.mix16(ga.srl16(7));

			if (!m_sel.tcc)
			{
				// GSVector4i ga = iip ? gaf : m_local.c.ga;

				MOVE_IF_64(psrlw, tmpga, f_ga, 7);

				mix16(_ga, tmpga, tmp);
			}

			break;

		case TFX_NONE:

			// gat = iip ? ga.srl16(7) : ga;

			if (m_sel.iip)
			{
				MOVE_IF_64(psrlw, _ga, f_ga, 7);
			}

			break;
	}

	if (m_sel.aa1)
	{
		// gs_user figure 3-2: anti-aliasing after tfx, before tests, modifies alpha

		// FIXME: bios config screen cubes

		if (!m_sel.abe)
		{
			// a = cov

			if (m_sel.edge)
			{
				movdqa(xym0, _rip_local(temp.cov));
			}
			else
			{
				pcmpeqd(xym0, xym0);
				psllw(xym0, 15);
				psrlw(xym0, 8);
			}

			mix16(_ga, xym0, xym1);
		}
		else
		{
			// a = a == 0x80 ? cov : a

			pcmpeqd(xym0, xym0);
			psllw(xym0, 15);
			psrlw(xym0, 8);

			if (m_sel.edge)
			{
				movdqa(xym1, _rip_local(temp.cov));
			}
			else
			{
				movdqa(xym1, xym0);
			}

			pcmpeqw(xym0, _ga);
			psrld(xym0, 16);
			pslld(xym0, 16);

			blend8(_ga, xym1 /*, xym0 */);
		}
	}
}

/// Output: _fm, _zm
void GSDrawScanlineCodeGenerator2::ReadMask()
{
	if (m_sel.fwrite)
	{
		pbroadcastdLocal(_fm, _rip_global(fm));
	}

	if (m_sel.zwrite)
	{
		pbroadcastdLocal(_zm, _rip_global(zm));
	}
}

/// Input: _ga, _fm, _zm
/// Destroys: xym0, xym1
void GSDrawScanlineCodeGenerator2::TestAlpha()
{
	switch (m_sel.atst)
	{
		case ATST_NEVER:
			// t = GSVector4i::xffffffff();
			pcmpeqd(xym1, xym1);
			break;

		case ATST_ALWAYS:
			return;

		case ATST_LESS:
		case ATST_LEQUAL:
			// t = (ga >> 16) > m_local.gd->aref;
			THREEARG(psrld, xym1, _ga, 16);
			BROADCAST_AND_OP(vbroadcasti128, pcmpgtd, xym1, xym0, _rip_global(aref));
			break;

		case ATST_EQUAL:
			// t = (ga >> 16) != m_local.gd->aref;
			THREEARG(psrld, xym1, _ga, 16);
			BROADCAST_AND_OP(vbroadcasti128, pcmpeqd, xym1, xym0, _rip_global(aref));
			pcmpeqd(xym0, xym0);
			pxor(xym1, xym0);
			break;

		case ATST_GEQUAL:
		case ATST_GREATER:
			// t = (ga >> 16) < m_local.gd->aref;
			THREEARG(psrld, xym0, _ga, 16);
			broadcasti128(xym1, _rip_global(aref));
			pcmpgtd(xym1, xym0);
			break;

		case ATST_NOTEQUAL:
			// t = (ga >> 16) == m_local.gd->aref;
			THREEARG(psrld, xym1, _ga, 16);
			BROADCAST_AND_OP(vbroadcasti128, pcmpeqd, xym1, xym0, _rip_global(aref));
			break;
	}

	switch (m_sel.afail)
	{
		case AFAIL_KEEP:
			// test |= t;
			por(_test, xym1);
			alltrue(_test);
			break;

		case AFAIL_FB_ONLY:
			// zm |= t;
			por(_zm, xym1);
			break;

		case AFAIL_ZB_ONLY:
			// fm |= t;
			por(_fm, xym1);
			break;

		case AFAIL_RGB_ONLY:
			// zm |= t;
			por(_zm, xym1);
			// fm |= t & GSVector4i::xff000000();
			psrld(xym1, 24);
			pslld(xym1, 24);
			por(_fm, xym1);
			break;
	}
}

/// Input: xym2[x86]=gaf, _rb, _ga
/// Destroys: xym0, xym1, xym2
void GSDrawScanlineCodeGenerator2::ColorTFX()
{
	if (!m_sel.fwrite)
	{
		return;
	}

	const XYm& f_ga  = _f_ga;
	const XYm& tmpga = xym2;

	auto modulate16_1_rb = [&]
	{
		// GSVector4i rb = iip ? rbf : m_local.c.rb;
		modulate16(_rb, _f_rb, 1);
	};

	switch (m_sel.tfx)
	{
		case TFX_MODULATE:

			// GSVector4i rb = iip ? rbf : m_local.c.rb;

			// rbt = rbt.modulate16<1>(rb).clamp8();

			modulate16_1_rb();

			clamp16(_rb, xym0);

			break;

		case TFX_DECAL:

			break;

		case TFX_HIGHLIGHT:
		case TFX_HIGHLIGHT2:
			// gat = gat.modulate16<1>(ga).add16(af).clamp8().mix16(gat);

			movdqa(xym1, _ga);

			modulate16(_ga, f_ga, 1);

			pshuflw(tmpga, f_ga, _MM_SHUFFLE(3, 3, 1, 1));
			pshufhw(tmpga, tmpga, _MM_SHUFFLE(3, 3, 1, 1));
			psrlw(tmpga, 7);

			paddw(_ga, tmpga);

			clamp16(_ga, xym0);

			mix16(_ga, xym1, xym0);

			// rbt = rbt.modulate16<1>(rb).add16(af).clamp8();

			modulate16_1_rb();

			paddw(_rb, tmpga);

			clamp16(_rb, xym0);

			break;

		case TFX_NONE:

			// rbt = iip ? rb.srl16(7) : rb;

			if (m_sel.iip)
			{
				MOVE_IF_64(psrlw, _rb, _f_rb, 7);
			}

			break;
	}
}

/// Input: _rb, _ga
/// Destroys: xym0, xym1, xym2[x86]
void GSDrawScanlineCodeGenerator2::Fog()
{
	if (!m_sel.fwrite || !m_sel.fge)
	{
		return;
	}

	const XYm& f   = _f;
	const XYm& tmp = xym0;

	// rb = m_local.gd->frb.lerp16<0>(rb, f);
	// ga = m_local.gd->fga.lerp16<0>(ga, f).mix16(ga);

	movdqa(xym1, _ga);

	pbroadcastdLocal(tmp, _rip_global(frb));
	lerp16(_rb, tmp, f, 0);

	pbroadcastdLocal(tmp, _rip_global(fga));
	lerp16(_ga, tmp, f, 0);

	mix16(_ga, xym1, xym0);
}

/// Outputs: _fd, rbx=fa
void GSDrawScanlineCodeGenerator2::ReadFrame()
{
	if (!m_sel.fb)
	{
		return;
	}

	mov(ebx, dword[t1]);
	add(ebx, dword[t0]);
	and(ebx, HALF_VM_SIZE - 1);

	if (!m_sel.rfb)
	{
		return;
	}

	ReadPixel(_fd, xym0, rbx);
}

/// Input: _fd, _test
/// Destroys: xym0, xym1
void GSDrawScanlineCodeGenerator2::TestDestAlpha()
{
	if (!m_sel.date || m_sel.fpsm != 0 && m_sel.fpsm != 2)
	{
		return;
	}

	// test |= ((fd [<< 16]) ^ m_local.gd->datm).sra32(31);

	if (m_sel.datm)
	{
		if (m_sel.fpsm == 2)
		{
			pxor(xym0, xym0);
			//vpsrld(xym1, _fd, 15);
			THREEARG(pslld, xym1, _fd, 16);
			psrad(xym1, 31);
			pcmpeqd(xym1, xym0);
		}
		else
		{
			pcmpeqd(xym0, xym0);
			THREEARG(pxor, xym1, _fd, xym0);
			psrad(xym1, 31);
		}
	}
	else
	{
		if (m_sel.fpsm == 2)
		{
			THREEARG(pslld, xym1, _fd, 16);
			psrad(xym1, 31);
		}
		else
		{
			THREEARG(psrad, xym1, _fd, 31);
		}
	}

	por(_test, xym1);

	alltrue(_test);
}

/// Input: _fm, _zm, _test
/// Output: edx=fzm
/// Destroys: xym0, xym1
void GSDrawScanlineCodeGenerator2::WriteMask()
{
	if (m_sel.notest)
	{
		return;
	}

	// fm |= test;
	// zm |= test;

	if (m_sel.fwrite)
	{
		por(_fm, _test);
	}

	if (m_sel.zwrite)
	{
		por(_zm, _test);
	}

	// int fzm = ~(fm == GSVector4i::xffffffff()).ps32(zm == GSVector4i::xffffffff()).mask();

	pcmpeqd(xym1, xym1);

	if (m_sel.fwrite && m_sel.zwrite)
	{
		THREEARG(pcmpeqd, xym0, xym1, _zm);
		pcmpeqd(xym1, _fm);
		packssdw(xym1, xym0);
	}
	else if (m_sel.fwrite)
	{
		pcmpeqd(xym1, _fm);
		packssdw(xym1, xym1);
	}
	else if (m_sel.zwrite)
	{
		pcmpeqd(xym1, _zm);
		packssdw(xym1, xym1);
	}

	pmovmskb(edx, xym1);

	not(edx);
}

/// Inputs: t2=za, edx=fzm, _zm
/// Destroys: xym0, xym1, xym7
void GSDrawScanlineCodeGenerator2::WriteZBuf()
{
	if (!m_sel.zwrite)
	{
		return;
	}

	if (m_sel.prim != GS_SPRITE_CLASS)
		movdqa(xym1, _rip_local(temp.zs));
	else
		pbroadcastdLocal(xym1, _rip_local(p.z));

	if (m_sel.ztest && m_sel.zpsm < 2)
	{
		// zs = zs.blend8(zd, zm);

		if (hasAVX)
		{
			vpblendvb(xym1, xym1, _rip_local(temp.zd), _zm);
		}
		else
		{
			movdqa(xym0, _zm);
			movdqa(xym7, _rip_local(temp.zd));
			blend8(xym1, xym7 /*, xym0 */);
		}
	}

	bool fast = m_sel.ztest ? m_sel.zpsm < 2 : m_sel.zpsm == 0 && m_sel.notest;

#if USING_XMM
	WritePixel(xym1, t2, dh, fast, m_sel.zpsm, 1);
#else
	WritePixel(xym1, t2, edx, fast, m_sel.zpsm, 1);
#endif
}

/// Input: _fd, _rb, _ga
/// Destroys: xym0, xym1, xym4, xym7[x86], xym15[x64]
void GSDrawScanlineCodeGenerator2::AlphaBlend()
{
	if (!m_sel.fwrite)
	{
		return;
	}

	if (m_sel.abe == 0 && m_sel.aa1 == 0)
	{
		return;
	}

	const XYm& _dst_rb = xym0;
	const XYm& _dst_ga = xym1;
	const XYm& tmp1 = _test;
	const XYm& tmp2 = xym4;

	if ((m_sel.aba != m_sel.abb) && (m_sel.aba == 1 || m_sel.abb == 1 || m_sel.abc == 1) || m_sel.abd == 1)
	{
		switch (m_sel.fpsm)
		{
			case 0:
			case 1:

				// c[2] = fd & mask;
				// c[3] = (fd >> 8) & mask;

				split16_2x8(_dst_rb, _dst_ga, _fd);

				break;

			case 2:

				// c[2] = ((fd & 0x7c00) << 9) | ((fd & 0x001f) << 3);
				// c[3] = ((fd & 0x8000) << 8) | ((fd & 0x03e0) >> 2);

				pcmpeqd(tmp1, tmp1);

				psrld(tmp1, 27); // 0x0000001f
				THREEARG(pand, _dst_rb, _fd, tmp1);
				pslld(_dst_rb, 3);

				pslld(tmp1, 10); // 0x00007c00
				THREEARG(pand, tmp2, _fd, tmp1);
				pslld(tmp2, 9);

				por(_dst_rb, tmp2);

				psrld(tmp1, 5); // 0x000003e0
				THREEARG(pand, _dst_ga, _fd, tmp1);
				psrld(_dst_ga, 2);

				psllw(tmp1, 10); // 0x00008000
				THREEARG(pand, tmp2, _fd, tmp1);
				pslld(tmp2, 8);

				por(_dst_ga, tmp2);

				break;
		}
	}

	// rb,   ga   = src rb, ga
	// xym0, xym1 = dst rb, ga
	// tmp1, tmp2 = free

	if (m_sel.pabe || (m_sel.aba != m_sel.abb) && (m_sel.abb == 0 || m_sel.abd == 0))
	{
		movdqa(tmp2, _rb);
	}

	if (m_sel.aba != m_sel.abb)
	{
		// rb = c[aba * 2 + 0];

		switch (m_sel.aba)
		{
			case 0:
				break;
			case 1:
				movdqa(_rb, _dst_rb);
				break;
			case 2:
				pxor(_rb, _rb);
				break;
		}

		// rb = rb.sub16(c[abb * 2 + 0]);

		switch (m_sel.abb)
		{
			case 0:
				psubw(_rb, tmp2);
				break;
			case 1:
				psubw(_rb, _dst_rb);
				break;
			case 2:
				break;
		}

		if (!(m_sel.fpsm == 1 && m_sel.abc == 1))
		{
			// GSVector4i a = abc < 2 ? c[abc * 2 + 1].yywwlh().sll16(7) : m_local.gd->afix;

			switch (m_sel.abc)
			{
				case 0:
				case 1:
					pshuflw(tmp1, m_sel.abc ? _dst_ga : _ga, _MM_SHUFFLE(3, 3, 1, 1));
					pshufhw(tmp1, tmp1, _MM_SHUFFLE(3, 3, 1, 1));
					psllw(tmp1, 7);
					break;
				case 2:
					pbroadcastwLocal(tmp1, _rip_global(afix));
					break;
			}

			// rb = rb.modulate16<1>(a);

			modulate16(_rb, tmp1, 1);
		}

		// rb = rb.add16(c[abd * 2 + 0]);

		switch (m_sel.abd)
		{
			case 0:
				paddw(_rb, tmp2);
				break;
			case 1:
				paddw(_rb, _dst_rb);
				break;
			case 2:
				break;
		}
	}
	else
	{
		// rb = c[abd * 2 + 0];

		switch (m_sel.abd)
		{
			case 0:
				break;
			case 1:
				movdqa(_rb, _dst_rb);
				break;
			case 2:
				pxor(_rb, _rb);
				break;
		}
	}

	if (m_sel.pabe)
	{
		// mask = (c[1] << 8).sra32(31);

		THREEARG(pslld, xym0, _ga, 8);
		psrad(xym0, 31);

		// rb = c[0].blend8(rb, mask);

		blend8r(_rb, tmp2 /*, xym0 */);
	}

	// xym0 = pabe mask (>=sse41)
	// ga   = src ga
	// xym1 = dst ga
	// rb   = rb
	// tmp1 = a
	// tmp2 = free

	movdqa(tmp2, _ga);

	if (m_sel.aba != m_sel.abb)
	{
		// ga = c[aba * 2 + 1];

		switch (m_sel.aba)
		{
			case 0:
				break;
			case 1:
				movdqa(_ga, _dst_ga);
				break;
			case 2:
				pxor(_ga, _ga);
				break;
		}

		// ga = ga.sub16(c[abeb * 2 + 1]);

		switch (m_sel.abb)
		{
			case 0:
				psubw(_ga, tmp2);
				break;
			case 1:
				psubw(_ga, _dst_ga);
				break;
			case 2:
				break;
		}

		if (!(m_sel.fpsm == 1 && m_sel.abc == 1))
		{
			// ga = ga.modulate16<1>(a);

			modulate16(_ga, tmp1, 1);
		}

		// ga = ga.add16(c[abd * 2 + 1]);

		switch (m_sel.abd)
		{
			case 0:
				paddw(_ga, tmp2);
				break;
			case 1:
				paddw(_ga, _dst_ga);
				break;
			case 2:
				break;
		}
	}
	else
	{
		// ga = c[abd * 2 + 1];

		switch (m_sel.abd)
		{
			case 0:
				break;
			case 1:
				movdqa(_ga, _dst_ga);
				break;
			case 2:
				pxor(_ga, _ga);
				break;
		}
	}

	// xym0 = pabe mask (>=sse41)
	// tmp2 = src ga
	// rb = rb
	// ga = ga
	// xym1, tmp1 = free

	if (m_sel.pabe)
	{
		psrld(xym0, 16); // zero out high words to select the source alpha in blend (so it also does mix16)

		// ga = c[1].blend8(ga, mask).mix16(c[1]);

		blend8r(_ga, tmp2 /*, xym0 */);
	}
	else
	{
		if (m_sel.fpsm != 1) // TODO: fm == 0xffxxxxxx
		{
			mix16(_ga, tmp2, tmp1);
		}
	}
}

/// Input: rbx=fa, rdx=fzm, _fd, _fm
/// Destroys: rax, xym0, xym1, xym5, xym6, xym7[x86], xmm15[x64]
void GSDrawScanlineCodeGenerator2::WriteFrame()
{
	if (!m_sel.fwrite)
	{
		return;
	}


	const XYm& tmp = xym15;

	if (m_sel.fpsm == 2 && m_sel.dthe)
	{
		// y = (top & 3) << 5

		mov(eax, ptr[rsp + _top]);
		and(eax, 3);
		shl(eax, 5);

		// rb = rb.add16(m_global.dimx[0 + y]);
		// ga = ga.add16(m_global.dimx[1 + y]);

		add(rax, _rip_global(dimx));

		BROADCAST_AND_OP(vbroadcasti128, paddw, xym5, tmp, ptr[rax + sizeof(GSVector4i) * 0]);
		BROADCAST_AND_OP(vbroadcasti128, paddw, xym6, tmp, ptr[rax + sizeof(GSVector4i) * 1]);
	}

	if (m_sel.colclamp == 0)
	{
		// c[0] &= 0x00ff00ff;
		// c[1] &= 0x00ff00ff;

		pcmpeqd(tmp, tmp);
		psrlw(tmp, 8);
		pand(xym5, tmp);
		pand(xym6, tmp);
	}

	// GSVector4i fs = c[0].upl16(c[1]).pu16(c[0].uph16(c[1]));

	THREEARG(punpckhwd, tmp, xym5, xym6);
	punpcklwd(xym5, xym6);
	packuswb(xym5, tmp);

	if (m_sel.fba && m_sel.fpsm != 1)
	{
		// fs |= 0x80000000;

		pcmpeqd(tmp, tmp);
		pslld(tmp, 31);
		por(xym5, tmp);
	}

	// tmp1 = fs
	// xym4 = fm
	// xym6 = fd

	if (m_sel.fpsm == 2)
	{
		// GSVector4i rb = fs & 0x00f800f8;
		// GSVector4i ga = fs & 0x8000f800;

		mov(eax, 0x00f800f8);
		broadcastGPRToVec(xym0, eax);

		mov(eax, 0x8000f800);
		broadcastGPRToVec(xym1, eax);

		pand(xym0, xym5);
		pand(xym1, xym5);

		// fs = (ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3);

		THREEARG(psrld, xym5, xym0, 9);
		psrld(xym0, 3);
		THREEARG(psrld, xym6, xym1, 16);
		psrld(xym1, 6);

		por(xym0, xym1);
		por(xym5, xym6);
		por(xym5, xym0);
	}

	if (m_sel.rfb)
	{
		// fs = fs.blend(fd, fm);

		blend(xym5, _fd, _fm); // TODO: could be skipped in certain cases, depending on fpsm and fm
	}

	bool fast = m_sel.rfb ? m_sel.fpsm < 2 : m_sel.fpsm == 0 && m_sel.notest;

#if USING_XMM
	WritePixel(xym5, rbx, dl, fast, m_sel.fpsm, 0);
#else
	WritePixel(xym5, rbx, edx, fast, m_sel.fpsm, 0);
#endif
}

/// Destroys: tmp[isYmm]
void GSDrawScanlineCodeGenerator2::ReadPixel(const XYm& dst, const XYm& tmp, const AddressReg& addr)
{
	RegExp base = _m_local__gd__vm + addr * 2;
#if USING_XMM
	movq(dst, qword[base]);
	movhps(dst, qword[base + 8 * 2]);
#else
	Xmm dstXmm = Xmm(dst.getIdx());
	Xmm tmpXmm = Xmm(tmp.getIdx());
	movq(dstXmm, qword[base]);
	movhps(dstXmm, qword[base + 8 * 2]);
	movq(tmpXmm, qword[base + 16 * 2]);
	movhps(tmpXmm, qword[base + 24 * 2]);
	vinserti128(dst, dst, tmpXmm, 1);
#endif
}

#if USING_XMM
void GSDrawScanlineCodeGenerator2::WritePixel(const XYm& src_, const AddressReg& addr, const Reg8& mask, bool fast, int psm, int fz)
#else
void GSDrawScanlineCodeGenerator2::WritePixel(const XYm& src_, const AddressReg& addr, const Reg32& mask, bool fast, int psm, int fz)
#endif
{
#if USING_XMM
	const Xmm& src = src_;
	int shift = 0;
#else
	Xmm src = Xmm(src_.getIdx());
	int shift = fz * 8;
#endif
	RegExp base = _m_local__gd__vm + addr * 2;

	if (m_sel.notest)
	{
		if (fast)
		{
			movq(qword[base], src);
			movhps(qword[base + 8 * 2], src);
#if USING_YMM
			vextracti128(src, src_, 1);
			movq(qword[base + 16 * 2], src);
			movhps(qword[base + 24 * 2], src);
#endif
		}
		else
		{
			WritePixel(src, addr, 0, 0, psm);
			WritePixel(src, addr, 1, 1, psm);
			WritePixel(src, addr, 2, 2, psm);
			WritePixel(src, addr, 3, 3, psm);
#if USING_YMM
			vextracti128(src, src_, 1);
			WritePixel(src, addr, 4, 0, psm);
			WritePixel(src, addr, 5, 1, psm);
			WritePixel(src, addr, 6, 2, psm);
			WritePixel(src, addr, 7, 3, psm);
#endif
		}
	}
	else
	{
		if (fast)
		{
			// if(fzm & 0x0f) GSVector4i::storel(&vm16[addr + 0], fs);
			// if(fzm & 0xf0) GSVector4i::storeh(&vm16[addr + 8], fs);

			test(mask, 0x0000000f << shift);
			je("@f");
			movq(qword[base], src);
			L("@@");

			test(mask, 0x000000f0 << shift);
			je("@f");
			movhps(qword[base + 8 * 2], src);
			L("@@");

#if USING_YMM
			vextracti128(src, src_, 1);

			test(mask, 0x000f0000 << shift);
			je("@f");
			movq(qword[base + 16 * 2], src);
			L("@@");

			test(mask, 0x00f00000 << shift);
			je("@f");
			movhps(qword[base + 24 * 2], src);
			L("@@");
#endif
			// vmaskmovps?
		}
		else
		{
			// if(fzm & 0x03) WritePixel(fpsm, &vm16[addr + 0], fs.extract32<0>());
			// if(fzm & 0x0c) WritePixel(fpsm, &vm16[addr + 2], fs.extract32<1>());
			// if(fzm & 0x30) WritePixel(fpsm, &vm16[addr + 8], fs.extract32<2>());
			// if(fzm & 0xc0) WritePixel(fpsm, &vm16[addr + 10], fs.extract32<3>());

			test(mask, 0x00000003 << shift);
			je("@f");
			WritePixel(src, addr, 0, 0, psm);
			L("@@");

			test(mask, 0x0000000c << shift);
			je("@f");
			WritePixel(src, addr, 1, 1, psm);
			L("@@");

			test(mask, 0x00000030 << shift);
			je("@f");
			WritePixel(src, addr, 2, 2, psm);
			L("@@");

			test(mask, 0x000000c0 << shift);
			je("@f");
			WritePixel(src, addr, 3, 3, psm);
			L("@@");

#if USING_YMM
			vextracti128(src, src_, 1);

			test(mask, 0x00030000 << shift);
			je("@f");
			WritePixel(src, addr, 4, 0, psm);
			L("@@");

			test(mask, 0x000c0000 << shift);
			je("@f");
			WritePixel(src, addr, 5, 1, psm);
			L("@@");

			test(mask, 0x00300000 << shift);
			je("@f");
			WritePixel(src, addr, 6, 2, psm);
			L("@@");

			test(mask, 0x00c00000 << shift);
			je("@f");
			WritePixel(src, addr, 7, 3, psm);
			L("@@");
#endif
		}
	}
}

void GSDrawScanlineCodeGenerator2::WritePixel(const Xmm& src, const AddressReg& addr, u8 i, u8 j, int psm)
{
	constexpr int s_offsets[8] = {0, 2, 8, 10, 16, 18, 24, 26};

	Address dst = ptr[_m_local__gd__vm + addr * 2 + s_offsets[i] * 2];

	switch (psm)
	{
		case 0:
			if (j == 0)
				movd(dst, src);
			else
				pextrd(dst, src, j);
			break;
		case 1:
			if (j == 0)
				movd(eax, src);
			else
				pextrd(eax, src, j);
			xor(eax, dst);
			and(eax, 0xffffff);
			xor(dst, eax);
			break;
		case 2:
			if (j == 0)
				movd(eax, src);
			else
				pextrw(eax, src, j * 2);
			mov(dst, ax);
			break;
	}
}

/// Input:
///  rbx = m_local.tex[0]  (x86 && !use_lod)
///  t2  = m_local.tex (x86 && use_lod)
///  rdx = m_local.clut (x86 && m_sel.tlu)
/// Destroys: rax, src, tmp1, tmp2
/// Destroys rbx (!use_lod)
void GSDrawScanlineCodeGenerator2::ReadTexel1(const XYm& dst, const XYm& src, const XYm& tmp1, const XYm& tmp2, int mip_offset)
{
	const XYm no(-1); // Hopefully this will assert if we accidentally use it
	ReadTexelImpl(dst, tmp1, src, no, no, no, tmp2, no, 1, mip_offset);
}

/// Will process addr## to c## from s registers to d registers
/// Destroys contents of s registers
/// Destroys tmp1 if <sse41 or isYmm
/// Will preserve tmp2
/// Input:
///  rbx = m_local.tex[0]  (x86 && !use_lod)
///  t2  = m_local.tex (x86 && use_lod)
///  rdx = m_local.clut (x86 && m_sel.tlu)
/// Destroys: rax
/// Destroys rbx (!use_lod)
void GSDrawScanlineCodeGenerator2::ReadTexel4(
	const XYm& d0,   const XYm& d1,
	const XYm& d2s0, const XYm& d3s1,
	const XYm& s2,   const XYm& s3,
	const XYm& tmp1, const XYm& tmp2,
	int mip_offset)
{
	ReadTexelImpl(d0, d1, d2s0, d3s1, s2, s3, tmp1, tmp2, 4, mip_offset);
}

void GSDrawScanlineCodeGenerator2::ReadTexelImpl(
	const XYm& d0,   const XYm& d1,
	const XYm& d2s0, const XYm& d3s1,
	const XYm& s2,   const XYm& s3,
	const XYm& tmp1, const XYm& tmp2,
	int pixels,      int mip_offset)
{
	mip_offset *= wordsize;
#if USING_XMM
	ReadTexelImplSSE4(d0, d1, d2s0, d3s1, s2, s3, pixels, mip_offset);
#else
	ReadTexelImplYmm(d0, d1, d2s0, d3s1, s2, s3, tmp1, pixels, mip_offset);
#endif
}

void GSDrawScanlineCodeGenerator2::ReadTexelImplLoadTexLOD(int lod, int mip_offset)
{
	AddressReg texIn = _64_m_local__gd__tex;
	Address lod_addr = m_sel.lcm ? _rip_global(lod.i.U32[lod]) : _rip_local(temp.lod.i.U32[lod]);
	mov(ebx, lod_addr);
	mov(rbx, ptr[texIn + rbx * wordsize + mip_offset]);
}

void GSDrawScanlineCodeGenerator2::ReadTexelImplYmm(
	const Ymm& d0,   const Ymm& d1,
	const Ymm& d2s0, const Ymm& d3s1,
	const Ymm& s2,   const Ymm& s3,
	const Ymm& tmp,
	int pixels,      int mip_offset)
{
	const Ymm dst[] = { d0,   d1,   d2s0, d3s1 };
	const Ymm src[] = { d2s0, d3s1,   s2,   s3 };
	const Ymm t1[]  = { d1,   d2s0, d3s1,   s2 };
	const Ymm t2[]  = { tmp,  tmp,  tmp,  tmp  };

	bool texInRBX = false;
	if (use_lod && m_sel.lcm)
	{
		ReadTexelImplLoadTexLOD(0, mip_offset);
		texInRBX = true;
	}

	for (int i = 0; i < pixels; i++)
	{
		const Xmm xdst{dst[i].getIdx()};
		const Xmm xsrc{src[i].getIdx()};
		const Xmm xt1{t1[i].getIdx()};
		const Xmm xt2{t2[i].getIdx()};

		if (use_lod && !m_sel.lcm)
		{
			texInRBX = true;

			vextracti128(xt1, src[i], 1);

			for (int j = 0; j < 4; j++)
			{
				ReadTexelImplLoadTexLOD(j, mip_offset);

				ReadTexelImpl(xdst, xsrc, j, texInRBX, false);

				ReadTexelImplLoadTexLOD(j + 4, mip_offset);

				ReadTexelImpl(xt2, xt1, j, texInRBX, false);
			}

			vinserti128(dst[i], dst[i], xt2, 1);
		}
		else
		{
			AddressReg tex = texInRBX ? rbx : _64_m_local__gd__tex;
			if (!m_sel.tlu)
			{
				pcmpeqd(t1[i], t1[i]);
				vpgatherdd(dst[i], ptr[tex + src[i] * 4], t1[i]);
			}
			else
			{
				vextracti128(xt1, src[i], 1);

				for (int j = 0; j < 4; j++)
				{
					ReadTexelImpl(xdst, xsrc, j, texInRBX, false);
					ReadTexelImpl(xt2, xt1, j, texInRBX, false);
				}

				vinserti128(dst[i], dst[i], xt2, 1);

				/*
				pcmpeqd(t1[i], t1[i]);
				vpgatherdd(t2[i], ptr[tex + src[i]*1], t1[i]); // either this 1x scale, or the latency of two dependendent gathers are too slow
				pslld(t2[i], 24);
				psrld(t2[i], 24);
				pcmpeqd(t1[i], t1[i]);
				vpgatherdd(dst[i], ptr[clut + t2[i]*4], t1[i]);
				*/
			}
		}
	}
}

void GSDrawScanlineCodeGenerator2::ReadTexelImplSSE4(
	const Xmm& d0,   const Xmm& d1,
	const Xmm& d2s0, const Xmm& d3s1,
	const Xmm& s2,   const Xmm& s3,
	int pixels,      int mip_offset)
{
	const bool preserve[] = { false, false, true, true };
	const Xmm dst[]       = { d0,    d1,    d2s0, d3s1 };
	const Xmm src[]       = { d2s0,  d3s1,    s2,   s3 };

	if (use_lod && !m_sel.lcm)
	{
		bool texInRBX = true;
		for (int j = 0; j < 4; j++)
		{
			ReadTexelImplLoadTexLOD(j, mip_offset);

			for (int i = 0; i < pixels; i++)
			{
				ReadTexelImpl(dst[i], src[i], j, texInRBX, preserve[i]);
			}
		}
	}
	else
	{
		bool preserve = false;
		bool texInRBX = false;

		if (use_lod && m_sel.lcm)
		{
			ReadTexelImplLoadTexLOD(0, mip_offset);
			texInRBX = true;
		}

		for (int i = 0; i < pixels; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				ReadTexelImpl(dst[i], src[i], j, texInRBX, preserve);
			}
		}
	}
}

void GSDrawScanlineCodeGenerator2::ReadTexelImpl(const Xmm& dst, const Xmm& addr, u8 i, bool texInRBX, bool preserveDst)
{
	ASSERT(i < 4);

	AddressReg clut = _64_m_local__gd__clut;
	AddressReg tex = texInRBX ? rbx : _64_m_local__gd__tex;
	Address src = m_sel.tlu ? ptr[clut + rax * 4] : ptr[tex + rax * 4];

	// Extract address offset
	if (i == 0)
		movd(eax, addr);
	else
		pextrd(eax, addr, i);

	// If clut, load the value as a byte index
	if (m_sel.tlu)
		movzx(eax, byte[tex + rax]);

	if (i == 0 && !preserveDst)
		movd(dst, src);
	else
		pinsrd(dst, src, i);
}
