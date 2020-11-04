/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSDrawScanlineCodeGenerator.h"
#include "GSVertexSW.h"

#if _M_SSE < 0x501 && (defined(_M_AMD64) || defined(_WIN64))

// Ease the reading of the code
#define _m_local r12
#define _m_local__gd r13
#define _m_local__gd__vm a1
#define _m_local__gd__clut r11
#define _m_local__gd__tex a3
// More pretty name
#define _z		xmm8
#define _f		xmm9
#define _s		xmm10
#define _t		xmm11
#define _q		xmm12
#define _f_rb	xmm13
#define _f_ga	xmm14
#define _test	xmm15
// Extra bonus
#define _rb		xmm2
#define _ga		xmm3
#define _fm		xmm4
#define _zm		xmm5
#define _fd		xmm6

#define _rip_local(field) (m_rip ? ptr[rip + &m_local.field] : ptr[_m_local + offsetof(GSScanlineLocalData, field)])
#define _rip_global(field) (m_rip ? ptr[rip + &m_local.gd->field] : ptr[_m_local__gd + offsetof(GSScanlineGlobalData, field)])

#ifdef _WIN64
#else
static const int _rz_rbx = -8 * 1;
static const int _rz_r12 = -8 * 2;
static const int _rz_r13 = -8 * 3;
//static const int _rz_r14 = -8 * 4;
//static const int _rz_r15 = -8 * 5;
static const int _rz_top = -8 * 6;
static const int _rz_zs  = -8 * 8;
static const int _rz_zd  = -8 * 10;
static const int _rz_cov = -8 * 12;
#endif

void GSDrawScanlineCodeGenerator::Generate_AVX()
{
	bool need_tex = m_sel.fb && m_sel.tfx != TFX_NONE;
	bool need_clut = need_tex && m_sel.tlu;
	m_rip = (size_t)getCurr() < 0x80000000;
	m_rip &= (size_t)&m_local < 0x80000000;
	m_rip &= (size_t)&m_local.gd < 0x80000000;

#ifdef _WIN64
	push(rbx);
	push(rsi);
	push(rdi);
	push(rbp);
	push(r12);
	push(r13);

	sub(rsp, 8 + 10 * 16);

	for(int i = 6; i < 16; i++)
	{
		vmovdqa(ptr[rsp + (i - 6) * 16], Xmm(i));
	}
#else
	// No reservation on the stack as a red zone is available
	push(rbp);
	mov(ptr[rsp + _rz_rbx], rbx);
	if (!m_rip)
	{
		mov(ptr[rsp + _rz_r12], r12);
		mov(ptr[rsp + _rz_r13], r13);
	}
#endif

	mov(r10, (size_t)g_const->m_test_128b[0]);
	if (!m_rip)
	{
		mov(_m_local, (size_t)&m_local);
		mov(_m_local__gd, _rip_local(gd));
	}

	if(need_clut)
		mov(_m_local__gd__clut, _rip_global(clut));

	Init_AVX();

	// a0 = steps
	// t1 = fza_base
	// t0 = fza_offset
	// r10 = &m_test[0]
	// _m_local = &m_local
	// _m_local__gd = m_local->gd
	// _m_local__gd__vm = m_local->gd.vm
	// xmm7 = vf (sprite && ltf)
	// xmm8 = z
	// xmm9 = f
	// xmm10 = s
	// xmm11 = t
	// xmm12 = q
	// xmm13 = rb
	// xmm14 = ga
	// xmm15 = test

	if(!m_sel.edge)
	{
		align(16);
	}

L("loop");

	TestZ_AVX(xmm5, xmm6);

	// ebp = za

	// FIXME not yet done
	if(m_sel.mmin && 0)
	{
		SampleTextureLOD_AVX();
	}
	else
	{
		SampleTexture_AVX();
	}

	// ebp = za
	// xmm2 = rb
	// xmm3 = ga

	AlphaTFX_AVX();

	// ebp = za
	// xmm2 = rb
	// xmm3 = ga

	ReadMask_AVX();

	// ebp = za
	// xmm2 = rb
	// xmm3 = ga
	// xmm4 = fm
	// xmm5 = zm

	TestAlpha_AVX();

	// ebp = za
	// xmm2 = rb
	// xmm3 = ga
	// xmm4 = fm
	// xmm5 = zm

	ColorTFX_AVX();

	// ebp = za
	// xmm2 = rb
	// xmm3 = ga
	// xmm4 = fm
	// xmm5 = zm

	Fog_AVX();

	// ebp = za
	// xmm2 = rb
	// xmm3 = ga
	// xmm4 = fm
	// xmm5 = zm

	ReadFrame_AVX();

	// ebx = fa
	// ebp = za
	// xmm2 = rb
	// xmm3 = ga
	// xmm4 = fm
	// xmm5 = zm
	// xmm6 = fd

	TestDestAlpha_AVX();

	// ebx = fa
	// ebp = za
	// xmm2 = rb
	// xmm3 = ga
	// xmm4 = fm
	// xmm5 = zm
	// xmm6 = fd

	WriteMask_AVX();

	// ebx = fa
	// edx = fzm
	// ebp = za
	// xmm2 = rb
	// xmm3 = ga
	// xmm4 = fm
	// xmm5 = zm
	// xmm6 = fd

	WriteZBuf_AVX();

	// ebx = fa
	// edx = fzm
	// xmm2 = rb
	// xmm3 = ga
	// xmm4 = fm
	// xmm6 = fd

	AlphaBlend_AVX();

	// ebx = fa
	// edx = fzm
	// xmm2 = rb
	// xmm3 = ga
	// xmm4 = fm
	// xmm6 = fd

	WriteFrame_AVX();

L("step");

	// if(steps <= 0) break;

	if(!m_sel.edge)
	{
		test(a0.cvt32(), a0.cvt32());

		jle("exit", T_NEAR);

		Step_AVX();

		jmp("loop", T_NEAR);
	}

L("exit");

#ifdef _WIN64
	for(int i = 6; i < 16; i++)
	{
		vmovdqa(Xmm(i), ptr[rsp + (i - 6) * 16]);
	}

	add(rsp, 8 + 10 * 16);

	pop(r13);
	pop(r12);
	pop(rbp);
	pop(rdi);
	pop(rsi);
	pop(rbx);
#else
	mov(rbx, ptr[rsp + _rz_rbx]);
	if (!m_rip)
	{
		mov(r12, ptr[rsp + _rz_r12]);
		mov(r13, ptr[rsp + _rz_r13]);
	}
	pop(rbp);
#endif

	ret();
}

void GSDrawScanlineCodeGenerator::Init_AVX()
{
	if(!m_sel.notest)
	{
		// int skip = left & 3;

		mov(ebx, a1.cvt32());
		and(a1.cvt32(), 3);

		// left -= skip;

		sub(ebx, a1.cvt32());

		// int steps = pixels + skip - 4;

		lea(a0.cvt32(), ptr[a0 + a1 - 4]);

		// GSVector4i test = m_test[skip] | m_test[7 + (steps & (steps >> 31))];

		shl(a1.cvt32(), 4); // * sizeof(m_test[0])

		vmovdqa(_test, ptr[a1 + r10]);

		mov(eax, a0.cvt32());
		sar(eax, 31); // GH: 31 to extract the sign of the register
		and(eax, a0.cvt32());
		shl(eax, 4); // * sizeof(m_test[0])
		cdqe();

		vpor(_test, ptr[rax + r10 + 7 * 16]);
	}
	else
	{
		mov(ebx, a1.cvt32()); // left
		xor(a1.cvt32(), a1.cvt32()); // skip
		lea(a0.cvt32(), ptr[a0 - 4]); // steps
	}

	// a0 = steps
	// a1 = skip
	// rbx = left


	// GSVector2i* fza_base = &m_local.gd->fzbr[top];

	mov(rax, _rip_global(fzbr));
	lea(t1, ptr[rax + a2 * 8]);

	// GSVector2i* fza_offset = &m_local.gd->fzbc[left >> 2];

	mov(rax, _rip_global(fzbc));
	lea(t0, ptr[rax + rbx * 2]);

	if(m_sel.prim != GS_SPRITE_CLASS && (m_sel.fwrite && m_sel.fge || m_sel.zb) || m_sel.fb && (m_sel.edge || m_sel.tfx != TFX_NONE || m_sel.iip))
	{
		// a1 = &m_local.d[skip] // note a1 was (skip << 4)

		// FIXME
		//lea(a1, ptr[a1 * 8 + _m_local + offsetof(GSScanlineLocalData, d)]);
		lea(rax, _rip_local(d));
		lea(a1, ptr[rax + a1 * 8]);
	}

	if(m_sel.prim != GS_SPRITE_CLASS)
	{
		if(m_sel.fwrite && m_sel.fge || m_sel.zb)
		{
			vmovaps(xmm0, ptr[a3 + offsetof(GSVertexSW, p)]); // v.p

			if(m_sel.fwrite && m_sel.fge)
			{
				// f = GSVector4i(vp).zzzzh().zzzz().add16(m_local.d[skip].f);

				vcvttps2dq(_f, xmm0);
				vpshufhw(_f, _f, _MM_SHUFFLE(2, 2, 2, 2));
				vpshufd(_f, _f, _MM_SHUFFLE(2, 2, 2, 2));
				vpaddw(_f, ptr[a1 + 16 * 6]);
			}

			if(m_sel.zb)
			{
				// z = vp.zzzz() + m_local.d[skip].z;

				vshufps(_z, xmm0, xmm0, _MM_SHUFFLE(2, 2, 2, 2));
				vaddps(_z, ptr[a1]);
			}
		}
	}
	else
	{
		if(m_sel.ztest)
		{
			vmovdqa(_z, _rip_local(p.z));
		}

		if(m_sel.fwrite && m_sel.fge)
			vmovdqa(_f, _rip_local(p.f));
	}

	if(m_sel.fb)
	{
		if(m_sel.edge || m_sel.tfx != TFX_NONE)
		{
			vmovaps(xmm0, ptr[a3 + offsetof(GSVertexSW, t)]); // v.t
		}

		if(m_sel.edge)
		{
			// m_local.temp.cov = GSVector4i::cast(v.t).zzzzh().wwww().srl16(9);

			vpshufhw(xmm1, xmm0, _MM_SHUFFLE(2, 2, 2, 2));
			vpshufd(xmm1, xmm1, _MM_SHUFFLE(3, 3, 3, 3));
			vpsrlw(xmm1, 9);

#ifdef _WIN64
			vmovdqa(_rip_local(temp.cov), xmm1);
#else
			vmovdqa(ptr[rsp + _rz_cov], xmm1);
#endif
		}

		if(m_sel.tfx != TFX_NONE)
		{
			// a1 = &m_local.d[skip]

			if(m_sel.fst)
			{
				// GSVector4i vti(vt);

				vcvttps2dq(xmm0, xmm0);

				// s = vti.xxxx() + m_local.d[skip].s;
				// t = vti.yyyy(); if(!sprite) t += m_local.d[skip].t;

				vpshufd(_s, xmm0, _MM_SHUFFLE(0, 0, 0, 0));
				vpshufd(_t, xmm0, _MM_SHUFFLE(1, 1, 1, 1));

				vpaddd(_s, ptr[a1 + offsetof(GSScanlineLocalData::skip, s)]);

				if(m_sel.prim != GS_SPRITE_CLASS || m_sel.mmin)
				{
					vpaddd(_t, ptr[a1 + offsetof(GSScanlineLocalData::skip, t)]);
				}
				else if(m_sel.ltf)
				{
					vpshuflw(xmm7, _t, _MM_SHUFFLE(2, 2, 0, 0));
					vpshufhw(xmm7, xmm7, _MM_SHUFFLE(2, 2, 0, 0));
					vpsrlw(xmm7, 12);
				}
			}
			else
			{
				// s = vt.xxxx() + m_local.d[skip].s;
				// t = vt.yyyy() + m_local.d[skip].t;
				// q = vt.zzzz() + m_local.d[skip].q;

				vshufps(_s, xmm0, xmm0, _MM_SHUFFLE(0, 0, 0, 0));
				vshufps(_t, xmm0, xmm0, _MM_SHUFFLE(1, 1, 1, 1));
				vshufps(_q, xmm0, xmm0, _MM_SHUFFLE(2, 2, 2, 2));

				vaddps(_s, ptr[a1 + offsetof(GSScanlineLocalData::skip, s)]);
				vaddps(_t, ptr[a1 + offsetof(GSScanlineLocalData::skip, t)]);
				vaddps(_q, ptr[a1 + offsetof(GSScanlineLocalData::skip, q)]);
			}
		}

		if(!(m_sel.tfx == TFX_DECAL && m_sel.tcc))
		{
			if(m_sel.iip)
			{
				// GSVector4i vc = GSVector4i(v.c);

				vcvttps2dq(xmm0, ptr[a3 + offsetof(GSVertexSW, c)]); // v.c

				// vc = vc.upl16(vc.zwxy());

				vpshufd(xmm1, xmm0, _MM_SHUFFLE(1, 0, 3, 2));
				vpunpcklwd(xmm0, xmm1);

				// rb = vc.xxxx().add16(m_local.d[skip].rb);
				// ga = vc.zzzz().add16(m_local.d[skip].ga);

				vpshufd(_f_rb, xmm0, _MM_SHUFFLE(0, 0, 0, 0));
				vpshufd(_f_ga, xmm0, _MM_SHUFFLE(2, 2, 2, 2));

				vpaddw(_f_rb, ptr[a1 + offsetof(GSScanlineLocalData::skip, rb)]);
				vpaddw(_f_ga, ptr[a1 + offsetof(GSScanlineLocalData::skip, ga)]);
			}
			else
			{
				vmovdqa(_f_rb, _rip_local(c.rb));
				vmovdqa(_f_ga, _rip_local(c.ga));
			}

			vmovdqa(_rb, _f_rb);
			vmovdqa(_ga, _f_ga);
		}
	}

	if(m_sel.fwrite && m_sel.fpsm == 2 && m_sel.dthe)
	{
		// On linux, a2 is edx which will be used for fzm
		// In all case, it will require a mov in dthe code, so let's keep the value on the stack
#ifdef _WIN64
		ASSERT(0);
#else
		mov(ptr[rsp + _rz_top], a2);
#endif
	}

	mov(_m_local__gd__vm, _rip_global(vm));
	if(m_sel.fb && m_sel.tfx != TFX_NONE)
		mov(_m_local__gd__tex, _rip_global(tex));
}

void GSDrawScanlineCodeGenerator::Step_AVX()
{
	// steps -= 4;

	sub(a0.cvt32(), 4);

	// fza_offset++;

	add(t0, 8);

	if(m_sel.prim != GS_SPRITE_CLASS)
	{
		// z += m_local.d4.z;

		if(m_sel.zb)
		{
			vaddps(_z, _rip_local(d4.z));
		}

		// f = f.add16(m_local.d4.f);

		if(m_sel.fwrite && m_sel.fge)
		{
			vpaddw(_f, _rip_local(d4.f));
		}
	}
	else
	{
		if(m_sel.ztest)
		{
		}
	}

	if(m_sel.fb)
	{
		if(m_sel.tfx != TFX_NONE)
		{
			if(m_sel.fst)
			{
				// GSVector4i st = m_local.d4.st;

				// si += st.xxxx();
				// if(!sprite) ti += st.yyyy();

				vmovdqa(xmm0, _rip_local(d4.stq));

				vpshufd(xmm1, xmm0, _MM_SHUFFLE(0, 0, 0, 0));
				vpaddd(_s, xmm1);

				if(m_sel.prim != GS_SPRITE_CLASS || m_sel.mmin)
				{
					vpshufd(xmm1, xmm0, _MM_SHUFFLE(1, 1, 1, 1));
					vpaddd(_t, xmm1);
				}
			}
			else
			{
				// GSVector4 stq = m_local.d4.stq;

				// s += stq.xxxx();
				// t += stq.yyyy();
				// q += stq.zzzz();

				vmovaps(xmm0, _rip_local(d4.stq));

				vshufps(xmm1, xmm0, xmm0, _MM_SHUFFLE(0, 0, 0, 0));
				vshufps(xmm2, xmm0, xmm0, _MM_SHUFFLE(1, 1, 1, 1));
				vshufps(xmm3, xmm0, xmm0, _MM_SHUFFLE(2, 2, 2, 2));

				vaddps(_s, xmm1);
				vaddps(_t, xmm2);
				vaddps(_q, xmm3);
			}
		}

		if(!(m_sel.tfx == TFX_DECAL && m_sel.tcc))
		{
			if(m_sel.iip)
			{
				// GSVector4i c = m_local.d4.c;

				// rb = rb.add16(c.xxxx());
				// ga = ga.add16(c.yyyy());

				vmovdqa(xmm0, _rip_local(d4.c));

				vpshufd(xmm1, xmm0, _MM_SHUFFLE(0, 0, 0, 0));
				vpshufd(xmm2, xmm0, _MM_SHUFFLE(1, 1, 1, 1));

				vpaddw(_f_rb, xmm1);
				vpaddw(_f_ga, xmm2);

				// FIXME: color may underflow and roll over at the end of the line, if decreasing

				vpxor(xmm0, xmm0);
				vpmaxsw(_f_rb, xmm0);
				vpmaxsw(_f_ga, xmm0);
			}
			else
			{
				if(m_sel.tfx == TFX_NONE)
				{
				}
			}

			vmovdqa(_rb, _f_rb);
			vmovdqa(_ga, _f_ga);
		}
	}

	if(!m_sel.notest)
	{
		// test = m_test[7 + (steps & (steps >> 31))];

		mov(eax, a0.cvt32());
		sar(eax, 31); // GH: 31 to extract the sign of the register
		and(eax, a0.cvt32());
		shl(eax, 4);
		cdqe();

		vmovdqa(_test, ptr[rax + r10 + 7 * 16]);
	}
}

void GSDrawScanlineCodeGenerator::TestZ_AVX(const Xmm& temp1, const Xmm& temp2)
{
	if(!m_sel.zb)
	{
		return;
	}

	// int za = fza_base.y + fza_offset->y;

	mov(ebp, dword[t1 + 4]);
	add(ebp, dword[t0 + 4]);
	and(ebp, HALF_VM_SIZE - 1);

	// GSVector4i zs = zi;

	if(m_sel.prim != GS_SPRITE_CLASS)
	{
		if(m_sel.zoverflow)
		{
			// zs = (GSVector4i(z * 0.5f) << 1) | (GSVector4i(z) & GSVector4i::x00000001());

			mov(rax, (size_t)&GSVector4::m_half);

			vbroadcastss(xmm0, ptr[rax]);
			vmulps(xmm0, _z);
			vcvttps2dq(xmm0, xmm0);
			vpslld(xmm0, 1);

			vcvttps2dq(xmm1, _z);
			vpcmpeqd(xmm2, xmm2);
			vpsrld(xmm2, 31);
			vpand(xmm1, xmm2);

			vpor(xmm0, xmm1);
		}
		else
		{
			// zs = GSVector4i(z);

			vcvttps2dq(xmm0, _z);
		}

		if(m_sel.zwrite)
		{
#ifdef _WIN64
			vmovdqa(_rip_local(temp.zs), xmm0);
#else
			vmovdqa(ptr[rsp + _rz_zs], xmm0);
#endif
		}
	}
	else
	{
		movdqa(xmm0, _z);
	}

	if(m_sel.ztest)
	{
		ReadPixel_AVX(xmm1, rbp);

		if(m_sel.zwrite && m_sel.zpsm < 2)
		{
#ifdef _WIN64
			vmovdqa(_rip_local(temp.zd), xmm1);
#else
			vmovdqa(ptr[rsp + _rz_zd], xmm1);
#endif
		}

		// zd &= 0xffffffff >> m_sel.zpsm * 8;

		if(m_sel.zpsm)
		{
			vpslld(xmm1, static_cast<uint8>(m_sel.zpsm * 8));
			vpsrld(xmm1, static_cast<uint8>(m_sel.zpsm * 8));
		}

		if(m_sel.zoverflow || m_sel.zpsm == 0)
		{
			// GSVector4i o = GSVector4i::x80000000();

			vpcmpeqd(xmm2, xmm2);
			vpslld(xmm2, 31);

			// GSVector4i zso = zs - o;
			// GSVector4i zdo = zd - o;

			vpsubd(xmm0, xmm2);
			vpsubd(xmm1, xmm2);
		}

		switch(m_sel.ztst)
		{
		case ZTST_GEQUAL:
			// test |= zso < zdo; // ~(zso >= zdo)
			vpcmpgtd(xmm1, xmm0);
			vpor(_test, xmm1);
			break;

		case ZTST_GREATER: // TODO: tidus hair and chocobo wings only appear fully when this is tested as ZTST_GEQUAL
			// test |= zso <= zdo; // ~(zso > zdo)
			vpcmpgtd(xmm0, xmm1);
			vpcmpeqd(xmm2, xmm2);
			vpxor(xmm0, xmm2);
			vpor(_test, xmm0);
			break;
		}

		alltrue(_test);
	}
}

void GSDrawScanlineCodeGenerator::SampleTexture_AVX()
{
	if(!m_sel.fb || m_sel.tfx == TFX_NONE)
	{
		return;
	}

	if(!m_sel.fst)
	{
		vrcpps(xmm0, _q);

		vmulps(xmm4, _s, xmm0);
		vmulps(xmm5, _t, xmm0);

		vcvttps2dq(xmm4, xmm4);
		vcvttps2dq(xmm5, xmm5);

		if(m_sel.ltf)
		{
			// u -= 0x8000;
			// v -= 0x8000;

			mov(eax, 0x8000);
			vmovd(xmm0, eax);
			vpshufd(xmm0, xmm0, _MM_SHUFFLE(0, 0, 0, 0));

			vpsubd(xmm4, xmm0);
			vpsubd(xmm5, xmm0);
		}
	}
	else
	{
		vmovdqa(xmm4, _s);
		vmovdqa(xmm5, _t);
	}

	if(m_sel.ltf)
	{
		// GSVector4i uf = u.xxzzlh().srl16(12);

		vpshuflw(xmm6, xmm4, _MM_SHUFFLE(2, 2, 0, 0));
		vpshufhw(xmm6, xmm6, _MM_SHUFFLE(2, 2, 0, 0));
		vpsrlw(xmm6, 12);

		if(m_sel.prim != GS_SPRITE_CLASS)
		{
			// GSVector4i vf = v.xxzzlh().srl16(12);

			vpshuflw(xmm7, xmm5, _MM_SHUFFLE(2, 2, 0, 0));
			vpshufhw(xmm7, xmm7, _MM_SHUFFLE(2, 2, 0, 0));
			vpsrlw(xmm7, 12);
		}
	}

	// GSVector4i uv0 = u.sra32(16).ps32(v.sra32(16));

	vpsrad(xmm4, 16);
	vpsrad(xmm5, 16);
	vpackssdw(xmm4, xmm5);

	if(m_sel.ltf)
	{
		// GSVector4i uv1 = uv0.add16(GSVector4i::x0001());

		vpcmpeqd(xmm0, xmm0);
		vpsrlw(xmm0, 15);
		vpaddw(xmm5, xmm4, xmm0);

		// uv0 = Wrap(uv0);
		// uv1 = Wrap(uv1);

		Wrap_AVX(xmm4, xmm5);
	}
	else
	{
		// uv0 = Wrap(uv0);

		Wrap_AVX(xmm4);
	}

	// xmm4 = uv0
	// xmm5 = uv1 (ltf)
	// xmm6 = uf
	// xmm7 = vf

	// GSVector4i x0 = uv0.upl16();
	// GSVector4i y0 = uv0.uph16() << tw;

	vpxor(xmm0, xmm0);

	vpunpcklwd(xmm2, xmm4, xmm0);
	vpunpckhwd(xmm3, xmm4, xmm0);
	vpslld(xmm3, static_cast<uint8>(m_sel.tw + 3));

	// xmm0 = 0
	// xmm2 = x0
	// xmm3 = y0
	// xmm5 = uv1 (ltf)
	// xmm6 = uf
	// xmm7 = vf

	if(m_sel.ltf)
	{
		// GSVector4i x1 = uv1.upl16();
		// GSVector4i y1 = uv1.uph16() << tw;

		vpunpcklwd(xmm4, xmm5, xmm0);
		vpunpckhwd(xmm5, xmm5, xmm0);
		vpslld(xmm5, static_cast<uint8>(m_sel.tw + 3));

		// xmm2 = x0
		// xmm3 = y0
		// xmm4 = x1
		// xmm5 = y1
		// xmm6 = uf
		// xmm7 = vf

		// GSVector4i addr00 = y0 + x0;
		// GSVector4i addr01 = y0 + x1;
		// GSVector4i addr10 = y1 + x0;
		// GSVector4i addr11 = y1 + x1;

		vpaddd(xmm0, xmm3, xmm2);
		vpaddd(xmm1, xmm3, xmm4);
		vpaddd(xmm2, xmm5, xmm2);
		vpaddd(xmm3, xmm5, xmm4);

		// xmm0 = addr00
		// xmm1 = addr01
		// xmm2 = addr10
		// xmm3 = addr11
		// xmm6 = uf
		// xmm7 = vf

		// c00 = addr00.gather32_32((const uint32/uint8*)tex[, clut]);
		// c01 = addr01.gather32_32((const uint32/uint8*)tex[, clut]);
		// c10 = addr10.gather32_32((const uint32/uint8*)tex[, clut]);
		// c11 = addr11.gather32_32((const uint32/uint8*)tex[, clut]);

		ReadTexel_AVX(4, 0);

		// xmm0 = c10
		// xmm1 = c11
		// xmm4 = c00
		// xmm5 = c01
		// xmm6 = uf
		// xmm7 = vf

		// GSVector4i rb00 = c00 & mask;
		// GSVector4i ga00 = (c00 >> 8) & mask;

		split16_2x8(xmm2, xmm3, xmm4);

		// GSVector4i rb01 = c01 & mask;
		// GSVector4i ga01 = (c01 >> 8) & mask;

		split16_2x8(xmm4, xmm5, xmm5);

		// xmm0 = c10
		// xmm1 = c11
		// xmm2 = rb00
		// xmm3 = ga00
		// xmm4 = rb01
		// xmm5 = ga01
		// xmm6 = uf
		// xmm7 = vf

		// rb00 = rb00.lerp16_4(rb01, uf);
		// ga00 = ga00.lerp16_4(ga01, uf);

		lerp16_4(xmm4, xmm2, xmm6);
		lerp16_4(xmm5, xmm3, xmm6);

		// xmm0 = c10
		// xmm1 = c11
		// xmm4 = rb00
		// xmm5 = ga00
		// xmm6 = uf
		// xmm7 = vf

		// GSVector4i rb10 = c10 & mask;
		// GSVector4i ga10 = (c10 >> 8) & mask;

		split16_2x8(xmm2, xmm3, xmm0);

		// GSVector4i rb11 = c11 & mask;
		// GSVector4i ga11 = (c11 >> 8) & mask;

		split16_2x8(xmm0, xmm1, xmm1);

		// xmm0 = rb11
		// xmm1 = ga11
		// xmm2 = rb10
		// xmm3 = ga10
		// xmm4 = rb00
		// xmm5 = ga00
		// xmm6 = uf
		// xmm7 = vf

		// rb10 = rb10.lerp16_4(rb11, uf);
		// ga10 = ga10.lerp16_4(ga11, uf);

		lerp16_4(xmm0, xmm2, xmm6);
		lerp16_4(xmm1, xmm3, xmm6);

		// xmm0 = rb10
		// xmm1 = ga10
		// xmm4 = rb00
		// xmm5 = ga00
		// xmm7 = vf

		// rb00 = rb00.lerp16_4(rb10, vf);
		// ga00 = ga00.lerp16_4(ga10, vf);

		lerp16_4(xmm0, xmm4, xmm7);
		lerp16_4(xmm1, xmm5, xmm7);

		// FIXME not ideal (but allow different source in ReadTexel and less register dependency)
		vmovdqa(xmm2, xmm0);
		vmovdqa(xmm3, xmm1);
	}
	else
	{
		// GSVector4i addr00 = y0 + x0;

		vpaddd(xmm0, xmm3, xmm2);

		// c00 = addr00.gather32_32((const uint32/uint8*)tex[, clut]);

		ReadTexel_AVX(1, 0);

		// GSVector4i mask = GSVector4i::x00ff();

		// c[0] = c00 & mask;
		// c[1] = (c00 >> 8) & mask;

		split16_2x8(_rb, _ga, xmm4);
	}

	// xmm2 = rb
	// xmm3 = ga
}

void GSDrawScanlineCodeGenerator::Wrap_AVX(const Xmm& uv)
{
	// xmm0, xmm1, xmm2, xmm3 = free

	int wms_clamp = ((m_sel.wms + 1) >> 1) & 1;
	int wmt_clamp = ((m_sel.wmt + 1) >> 1) & 1;

	int region = ((m_sel.wms | m_sel.wmt) >> 1) & 1;

	if(wms_clamp == wmt_clamp)
	{
		if(wms_clamp)
		{
			if(region)
			{
				vpmaxsw(uv, _rip_global(t.min));
			}
			else
			{
				vpxor(xmm0, xmm0);
				vpmaxsw(uv, xmm0);
			}

			vpminsw(uv, _rip_global(t.max));
		}
		else
		{
			vpand(uv, _rip_global(t.min));

			if(region)
			{
				vpor(uv, _rip_global(t.max));
			}
		}
	}
	else
	{
		vmovdqa(xmm2, _rip_global(t.min));
		vmovdqa(xmm3, _rip_global(t.max));
		vmovdqa(xmm0, _rip_global(t.mask));

		// GSVector4i repeat = (t & m_local.gd->t.min) | m_local.gd->t.max;

		vpand(xmm1, uv, xmm2);

		if(region)
		{
			vpor(xmm1, xmm3);
		}

		// GSVector4i clamp = t.sat_i16(m_local.gd->t.min, m_local.gd->t.max);

		vpmaxsw(uv, xmm2);
		vpminsw(uv, xmm3);

		// clamp.blend8(repeat, m_local.gd->t.mask);

		vpblendvb(uv, xmm1, xmm0);
	}
}

void GSDrawScanlineCodeGenerator::Wrap_AVX(const Xmm& uv0, const Xmm& uv1)
{
	// xmm0, xmm1, xmm2, xmm3 = free

	int wms_clamp = ((m_sel.wms + 1) >> 1) & 1;
	int wmt_clamp = ((m_sel.wmt + 1) >> 1) & 1;

	int region = ((m_sel.wms | m_sel.wmt) >> 1) & 1;

	if(wms_clamp == wmt_clamp)
	{
		if(wms_clamp)
		{
			if(region)
			{
				vmovdqa(xmm0, _rip_global(t.min));
				vpmaxsw(uv0, xmm0);
				vpmaxsw(uv1, xmm0);
			}
			else
			{
				vpxor(xmm0, xmm0);
				vpmaxsw(uv0, xmm0);
				vpmaxsw(uv1, xmm0);
			}

			vmovdqa(xmm0, _rip_global(t.max));
			vpminsw(uv0, xmm0);
			vpminsw(uv1, xmm0);
		}
		else
		{
			vmovdqa(xmm0, _rip_global(t.min));
			vpand(uv0, xmm0);
			vpand(uv1, xmm0);

			if(region)
			{
				vmovdqa(xmm0, _rip_global(t.max));
				vpor(uv0, xmm0);
				vpor(uv1, xmm0);
			}
		}
	}
	else
	{
		vmovdqa(xmm2, _rip_global(t.min));
		vmovdqa(xmm3, _rip_global(t.max));
		vmovdqa(xmm0, _rip_global(t.mask));

		// uv0

		// GSVector4i repeat = (t & m_local.gd->t.min) | m_local.gd->t.max;

		vpand(xmm1, uv0, xmm2);

		if(region)
		{
			vpor(xmm1, xmm3);
		}

		// GSVector4i clamp = t.sat_i16(m_local.gd->t.min, m_local.gd->t.max);

		vpmaxsw(uv0, xmm2);
		vpminsw(uv0, xmm3);

		// clamp.blend8(repeat, m_local.gd->t.mask);

		vpblendvb(uv0, xmm1, xmm0);

		// uv1

		// GSVector4i repeat = (t & m_local.gd->t.min) | m_local.gd->t.max;

		vpand(xmm1, uv1, xmm2);

		if(region)
		{
			vpor(xmm1, xmm3);
		}

		// GSVector4i clamp = t.sat_i16(m_local.gd->t.min, m_local.gd->t.max);

		vpmaxsw(uv1, xmm2);
		vpminsw(uv1, xmm3);

		// clamp.blend8(repeat, m_local.gd->t.mask);

		vpblendvb(uv1, xmm1, xmm0);
	}
}

void GSDrawScanlineCodeGenerator::SampleTextureLOD_AVX()
{
}

void GSDrawScanlineCodeGenerator::WrapLOD_AVX(const Xmm& uv)
{
}

void GSDrawScanlineCodeGenerator::WrapLOD_AVX(const Xmm& uv0, const Xmm& uv1)
{
}

void GSDrawScanlineCodeGenerator::AlphaTFX_AVX()
{
	if(!m_sel.fb)
	{
		return;
	}

	switch(m_sel.tfx)
	{
	case TFX_MODULATE:

		// gat = gat.modulate16<1>(ga).clamp8();

		modulate16(_ga, _f_ga, 1);

		clamp16(_ga, xmm0);

		// if(!tcc) gat = gat.mix16(ga.srl16(7));

		if(!m_sel.tcc)
		{
			vpsrlw(xmm1, _f_ga, 7);

			mix16(_ga, xmm1, xmm0);
		}

		break;

	case TFX_DECAL:

		// if(!tcc) gat = gat.mix16(ga.srl16(7));

		if(!m_sel.tcc)
		{
			vpsrlw(xmm1, _f_ga, 7);

			mix16(_ga, xmm1, xmm0);
		}

		break;

	case TFX_HIGHLIGHT:

		// gat = gat.mix16(!tcc ? ga.srl16(7) : gat.addus8(ga.srl16(7)));

		vpsrlw(xmm1, _f_ga, 7);

		if(m_sel.tcc)
		{
			vpaddusb(xmm1, _ga);
		}

		mix16(_ga, xmm1, xmm0);

		break;

	case TFX_HIGHLIGHT2:

		// if(!tcc) gat = gat.mix16(ga.srl16(7));

		if(!m_sel.tcc)
		{
			vpsrlw(xmm1, _f_ga, 7);

			mix16(_ga, xmm1, xmm0);
		}

		break;

	case TFX_NONE:

		// gat = iip ? ga.srl16(7) : ga;

		if(m_sel.iip)
		{
			vpsrlw(_ga, _f_ga, 7);
		}

		break;
	}

	if(m_sel.aa1)
	{
		// gs_user figure 3-2: anti-aliasing after tfx, before tests, modifies alpha

		// FIXME: bios config screen cubes

		if(!m_sel.abe)
		{
			// a = cov

			if(m_sel.edge)
			{
#ifdef _WIN64
				vmovdqa(xmm0, _rip_local(temp.cov));
#else
				vmovdqa(xmm0, ptr[rsp + _rz_cov]);
#endif
			}
			else
			{
				vpcmpeqd(xmm0, xmm0);
				vpsllw(xmm0, 15);
				vpsrlw(xmm0, 8);
			}

			mix16(_ga, xmm0, xmm1);
		}
		else
		{
			// a = a == 0x80 ? cov : a

			vpcmpeqd(xmm0, xmm0);
			vpsllw(xmm0, 15);
			vpsrlw(xmm0, 8);

			if(m_sel.edge)
			{
#ifdef _WIN64
				vmovdqa(xmm1, _rip_local(temp.cov));
#else
				vmovdqa(xmm1, ptr[rsp + _rz_cov]);
#endif
			}
			else
			{
				vmovdqa(xmm1, xmm0);
			}

			vpcmpeqw(xmm0, _ga);
			vpsrld(xmm0, 16);
			vpslld(xmm0, 16);

			vpblendvb(_ga, xmm1, xmm0);
		}
	}
}

void GSDrawScanlineCodeGenerator::ReadMask_AVX()
{
	if(m_sel.fwrite)
	{
		vmovdqa(_fm, _rip_global(fm));
	}

	if(m_sel.zwrite)
	{
		vmovdqa(_zm, _rip_global(zm));
	}
}

void GSDrawScanlineCodeGenerator::TestAlpha_AVX()
{
	switch(m_sel.atst)
	{
	case ATST_NEVER:
		// t = GSVector4i::xffffffff();
		vpcmpeqd(xmm1, xmm1);
		break;

	case ATST_ALWAYS:
		return;

	case ATST_LESS:
	case ATST_LEQUAL:
		// t = (ga >> 16) > m_local.gd->aref;
		vpsrld(xmm1, _ga, 16);
		vpcmpgtd(xmm1, _rip_global(aref));
		break;

	case ATST_EQUAL:
		// t = (ga >> 16) != m_local.gd->aref;
		vpsrld(xmm1, _ga, 16);
		vpcmpeqd(xmm1, _rip_global(aref));
		vpcmpeqd(xmm0, xmm0);
		vpxor(xmm1, xmm0);
		break;

	case ATST_GEQUAL:
	case ATST_GREATER:
		// t = (ga >> 16) < m_local.gd->aref;
		vpsrld(xmm0, _ga, 16);
		vmovdqa(xmm1, _rip_global(aref));
		vpcmpgtd(xmm1, xmm0);
		break;

	case ATST_NOTEQUAL:
		// t = (ga >> 16) == m_local.gd->aref;
		vpsrld(xmm1, _ga, 16);
		vpcmpeqd(xmm1, _rip_global(aref));
		break;
	}

	switch(m_sel.afail)
	{
	case AFAIL_KEEP:
		// test |= t;
		vpor(_test, xmm1);
		alltrue(_test);
		break;

	case AFAIL_FB_ONLY:
		// zm |= t;
		vpor(_zm, xmm1);
		break;

	case AFAIL_ZB_ONLY:
		// fm |= t;
		vpor(_fm, xmm1);
		break;

	case AFAIL_RGB_ONLY:
		// zm |= t;
		vpor(_zm, xmm1);
		// fm |= t & GSVector4i::xff000000();
		vpsrld(xmm1, 24);
		vpslld(xmm1, 24);
		vpor(_fm, xmm1);
		break;
	}
}

void GSDrawScanlineCodeGenerator::ColorTFX_AVX()
{
	if(!m_sel.fwrite)
	{
		return;
	}

	switch(m_sel.tfx)
	{
	case TFX_MODULATE:

		// rbt = rbt.modulate16<1>(rb).clamp8();

		modulate16(_rb, _f_rb, 1);

		clamp16(_rb, xmm0);

		break;

	case TFX_DECAL:

		break;

	case TFX_HIGHLIGHT:
	case TFX_HIGHLIGHT2:

		// gat = gat.modulate16<1>(ga).add16(af).clamp8().mix16(gat);

		vmovdqa(xmm1, _ga);

		modulate16(_ga, _f_ga, 1);

		vpshuflw(xmm6, _f_ga, _MM_SHUFFLE(3, 3, 1, 1));
		vpshufhw(xmm6, xmm6, _MM_SHUFFLE(3, 3, 1, 1));
		vpsrlw(xmm6, 7);

		vpaddw(_ga, xmm6);

		clamp16(_ga, xmm0);

		mix16(_ga, xmm1, xmm0);

		// rbt = rbt.modulate16<1>(rb).add16(af).clamp8();

		modulate16(_rb, _f_rb, 1);

		vpaddw(_rb, xmm6);

		clamp16(_rb, xmm0);

		break;

	case TFX_NONE:

		// rbt = iip ? rb.srl16(7) : rb;

		if(m_sel.iip)
		{
			vpsrlw(_rb, _f_rb, 7);
		}

		break;
	}
}

void GSDrawScanlineCodeGenerator::Fog_AVX()
{
	if(!m_sel.fwrite || !m_sel.fge)
	{
		return;
	}

	// rb = m_local.gd->frb.lerp16<0>(rb, f);
	// ga = m_local.gd->fga.lerp16<0>(ga, f).mix16(ga);

	vmovdqa(xmm6, _ga);

	vmovdqa(xmm0, _rip_global(frb));
	vmovdqa(xmm1, _rip_global(fga));

	lerp16(_rb, xmm0, _f, 0);
	lerp16(_ga, xmm1, _f, 0);

	mix16(_ga, xmm6, _f);
}

void GSDrawScanlineCodeGenerator::ReadFrame_AVX()
{
	if(!m_sel.fb)
	{
		return;
	}

	// int fa = fza_base.x + fza_offset->x;

	mov(ebx, dword[t1]);
	add(ebx, dword[t0]);
	and(ebx, HALF_VM_SIZE - 1);

	if(!m_sel.rfb)
	{
		return;
	}

	ReadPixel_AVX(_fd, rbx);
}

void GSDrawScanlineCodeGenerator::TestDestAlpha_AVX()
{
	if(!m_sel.date || m_sel.fpsm != 0 && m_sel.fpsm != 2)
	{
		return;
	}

	// test |= ((fd [<< 16]) ^ m_local.gd->datm).sra32(31);

	if(m_sel.datm)
	{
		if(m_sel.fpsm == 2)
		{
			vpxor(xmm0, xmm0);
			//vpsrld(xmm1, _fd, 15);
			vpslld(xmm1, _fd, 16);
			vpsrad(xmm1, 31);
			vpcmpeqd(xmm1, xmm0);
		}
		else
		{
			vpcmpeqd(xmm0, xmm0);
			vpxor(xmm1, _fd, xmm0);
			vpsrad(xmm1, 31);
		}
	}
	else
	{
		if(m_sel.fpsm == 2)
		{
			vpslld(xmm1, _fd, 16);
			vpsrad(xmm1, 31);
		}
		else
		{
			vpsrad(xmm1, _fd, 31);
		}
	}

	vpor(_test, xmm1);

	alltrue(_test);
}

void GSDrawScanlineCodeGenerator::WriteMask_AVX()
{
	if(m_sel.notest)
	{
		return;
	}

	// fm |= test;
	// zm |= test;

	if(m_sel.fwrite)
	{
		vpor(_fm, _test);
	}

	if(m_sel.zwrite)
	{
		vpor(_zm, _test);
	}

	// int fzm = ~(fm == GSVector4i::xffffffff()).ps32(zm == GSVector4i::xffffffff()).mask();

	vpcmpeqd(xmm1, xmm1);

	if(m_sel.fwrite && m_sel.zwrite)
	{
		vpcmpeqd(xmm0, xmm1, _zm);
		vpcmpeqd(xmm1, _fm);
		vpackssdw(xmm1, xmm0);
	}
	else if(m_sel.fwrite)
	{
		vpcmpeqd(xmm1, _fm);
		vpackssdw(xmm1, xmm1);
	}
	else if(m_sel.zwrite)
	{
		vpcmpeqd(xmm1, _zm);
		vpackssdw(xmm1, xmm1);
	}

	vpmovmskb(edx, xmm1);

	not(edx);
}

void GSDrawScanlineCodeGenerator::WriteZBuf_AVX()
{
	if(!m_sel.zwrite)
	{
		return;
	}

	if (m_sel.prim != GS_SPRITE_CLASS)
#ifdef _WIN64
		vmovdqa(xmm1, _rip_local(temp.zs));
#else
		vmovdqa(xmm1, ptr[rsp + _rz_zs]);
#endif
	else
		vmovdqa(xmm1, _rip_local(p.z));

	if(m_sel.ztest && m_sel.zpsm < 2)
	{
		// zs = zs.blend8(zd, zm);

#ifdef _WIN64
		vpblendvb(xmm1, _rip_local(temp.zd), _zm);
#else
		vpblendvb(xmm1, ptr[rsp + _rz_zd], _zm);
#endif
	}

	bool fast = m_sel.ztest ? m_sel.zpsm < 2 : m_sel.zpsm == 0 && m_sel.notest;

	WritePixel_AVX(xmm1, rbp, dh, fast, m_sel.zpsm, 1);
}

void GSDrawScanlineCodeGenerator::AlphaBlend_AVX()
{
	if(!m_sel.fwrite)
	{
		return;
	}

	if(m_sel.abe == 0 && m_sel.aa1 == 0)
	{
		return;
	}

	const Xmm& _dst_rb = xmm0;
	const Xmm& _dst_ga = xmm1;

	if((m_sel.aba != m_sel.abb) && (m_sel.aba == 1 || m_sel.abb == 1 || m_sel.abc == 1) || m_sel.abd == 1)
	{
		switch(m_sel.fpsm)
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

			vpcmpeqd(xmm15, xmm15);

			vpsrld(xmm15, 27); // 0x0000001f
			vpand(_dst_rb, _fd, xmm15);
			vpslld(_dst_rb, 3);

			vpslld(xmm15, 10); // 0x00007c00
			vpand(xmm5, _fd, xmm15);
			vpslld(xmm5, 9);

			vpor(_dst_rb, xmm5);

			vpsrld(xmm15, 5); // 0x000003e0
			vpand(_dst_ga, _fd, xmm15);
			vpsrld(_dst_ga, 2);

			vpsllw(xmm15, 10); // 0x00008000
			vpand(xmm5, _fd, xmm15);
			vpslld(xmm5, 8);

			vpor(_dst_ga, xmm5);

			break;
		}
	}

	// xmm2, xmm3 = src rb, ga
	// xmm0, xmm1 = dst rb, ga
	// xmm5, xmm15 = free

	if(m_sel.pabe || (m_sel.aba != m_sel.abb) && (m_sel.abb == 0 || m_sel.abd == 0))
	{
		vmovdqa(xmm5, _rb);
	}

	if(m_sel.aba != m_sel.abb)
	{
		// rb = c[aba * 2 + 0];

		switch(m_sel.aba)
		{
		case 0: break;
		case 1: vmovdqa(_rb, _dst_rb); break;
		case 2: vpxor(_rb, _rb); break;
		}

		// rb = rb.sub16(c[abb * 2 + 0]);

		switch(m_sel.abb)
		{
		case 0: vpsubw(_rb, xmm5); break;
		case 1: vpsubw(_rb, _dst_rb); break;
		case 2: break;
		}

		if(!(m_sel.fpsm == 1 && m_sel.abc == 1))
		{
			// GSVector4i a = abc < 2 ? c[abc * 2 + 1].yywwlh().sll16(7) : m_local.gd->afix;

			switch(m_sel.abc)
			{
			case 0:
			case 1:
				vpshuflw(xmm15, m_sel.abc ? _dst_ga : _ga, _MM_SHUFFLE(3, 3, 1, 1));
				vpshufhw(xmm15, xmm15, _MM_SHUFFLE(3, 3, 1, 1));
				vpsllw(xmm15, 7);
				break;
			case 2:
				vmovdqa(xmm15, _rip_global(afix));
				break;
			}

			// rb = rb.modulate16<1>(a);

			modulate16(_rb, xmm15, 1);
		}

		// rb = rb.add16(c[abd * 2 + 0]);

		switch(m_sel.abd)
		{
		case 0: vpaddw(_rb, xmm5); break;
		case 1: vpaddw(_rb, _dst_rb); break;
		case 2: break;
		}
	}
	else
	{
		// rb = c[abd * 2 + 0];

		switch(m_sel.abd)
		{
		case 0: break;
		case 1: vmovdqa(_rb, _dst_rb); break;
		case 2: vpxor(_rb, _rb); break;
		}
	}

	if(m_sel.pabe)
	{
		// mask = (c[1] << 8).sra32(31);

		vpslld(xmm0, _ga, 8);
		vpsrad(xmm0, 31);

		// rb = c[0].blend8(rb, mask);

		vpblendvb(_rb, xmm5, _rb, xmm0);
	}

	// xmm0 = pabe mask
	// xmm3 = src ga
	// xmm1 = dst ga
	// xmm2 = rb
	// xmm15 = a
	// xmm5 = free

	vmovdqa(xmm5, _ga);

	if(m_sel.aba != m_sel.abb)
	{
		// ga = c[aba * 2 + 1];

		switch(m_sel.aba)
		{
		case 0: break;
		case 1: vmovdqa(_ga, _dst_ga); break;
		case 2: vpxor(_ga, _ga); break;
		}

		// ga = ga.sub16(c[abeb * 2 + 1]);

		switch(m_sel.abb)
		{
		case 0: vpsubw(_ga, xmm5); break;
		case 1: vpsubw(_ga, _dst_ga); break;
		case 2: break;
		}

		if(!(m_sel.fpsm == 1 && m_sel.abc == 1))
		{
			// ga = ga.modulate16<1>(a);

			modulate16(_ga, xmm15, 1);
		}

		// ga = ga.add16(c[abd * 2 + 1]);

		switch(m_sel.abd)
		{
		case 0: vpaddw(_ga, xmm5); break;
		case 1: vpaddw(_ga, _dst_ga); break;
		case 2: break;
		}
	}
	else
	{
		// ga = c[abd * 2 + 1];

		switch(m_sel.abd)
		{
		case 0: break;
		case 1: vmovdqa(_ga, _dst_ga); break;
		case 2: vpxor(_ga, _ga); break;
		}
	}

	// xmm0 = pabe mask
	// xmm5 = src ga
	// xmm2 = rb
	// xmm3 = ga
	// xmm1, xmm15 = free

	if(m_sel.pabe)
	{
		vpsrld(xmm0, 16); // zero out high words to select the source alpha in blend (so it also does mix16)

		// ga = c[1].blend8(ga, mask).mix16(c[1]);

		vpblendvb(_ga, xmm5, _ga, xmm0);
	}
	else
	{
		if(m_sel.fpsm != 1) // TODO: fm == 0xffxxxxxx
		{
			mix16(_ga, xmm5, xmm15);
		}
	}
}

void GSDrawScanlineCodeGenerator::WriteFrame_AVX()
{
	if(!m_sel.fwrite)
	{
		return;
	}

	if(m_sel.fpsm == 2 && m_sel.dthe)
	{
		// y = (top & 3) << 5

#ifdef _WIN64
		ASSERT(0);
#else
		mov(eax, ptr[rsp + _rz_top]);
#endif
		and(eax, 3);
		shl(eax, 5);

		// rb = rb.add16(m_global.dimx[0 + y]);
		// ga = ga.add16(m_global.dimx[1 + y]);

		add(rax, _rip_global(dimx));

		vpaddw(xmm2, ptr[rax + sizeof(GSVector4i) * 0]);
		vpaddw(xmm3, ptr[rax + sizeof(GSVector4i) * 1]);

	}

	if(m_sel.colclamp == 0)
	{
		// c[0] &= 0x00ff00ff;
		// c[1] &= 0x00ff00ff;

		vpcmpeqd(xmm15, xmm15);
		vpsrlw(xmm15, 8);
		vpand(xmm2, xmm15);
		vpand(xmm3, xmm15);
	}

	// GSVector4i fs = c[0].upl16(c[1]).pu16(c[0].uph16(c[1]));

	vpunpckhwd(xmm15, xmm2, xmm3);
	vpunpcklwd(xmm2, xmm3);
	vpackuswb(xmm2, xmm15);

	if(m_sel.fba && m_sel.fpsm != 1)
	{
		// fs |= 0x80000000;

		vpcmpeqd(xmm15, xmm15);
		vpslld(xmm15, 31);
		vpor(xmm2, xmm15);
	}

	// xmm2 = fs
	// xmm4 = fm
	// xmm6 = fd

	if(m_sel.fpsm == 2)
	{
		// GSVector4i rb = fs & 0x00f800f8;
		// GSVector4i ga = fs & 0x8000f800;

		mov(eax, 0x00f800f8);
		vmovd(xmm0, eax);
		vpshufd(xmm0, xmm0, _MM_SHUFFLE(0, 0, 0, 0));

		mov(eax, 0x8000f800);
		vmovd(xmm1, eax);
		vpshufd(xmm1, xmm1, _MM_SHUFFLE(0, 0, 0, 0));

		vpand(xmm0, xmm2);
		vpand(xmm1, xmm2);

		// fs = (ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3);

		vpsrld(xmm2, xmm0, 9);
		vpsrld(xmm0, 3);
		vpsrld(xmm3, xmm1, 16);
		vpsrld(xmm1, 6);

		vpor(xmm0, xmm1);
		vpor(xmm2, xmm3);
		vpor(xmm2, xmm0);
	}

	if(m_sel.rfb)
	{
		// fs = fs.blend(fd, fm);

		blend(xmm2, _fd, _fm); // TODO: could be skipped in certain cases, depending on fpsm and fm
	}

	bool fast = m_sel.rfb ? m_sel.fpsm < 2 : m_sel.fpsm == 0 && m_sel.notest;

	WritePixel_AVX(xmm2, rbx, dl, fast, m_sel.fpsm, 0);
}

void GSDrawScanlineCodeGenerator::ReadPixel_AVX(const Xmm& dst, const Reg64& addr)
{
	vmovq(dst, qword[_m_local__gd__vm + addr * 2]);
	vmovhps(dst, qword[_m_local__gd__vm + addr * 2 + 8 * 2]);
}

void GSDrawScanlineCodeGenerator::WritePixel_AVX(const Xmm& src, const Reg64& addr, const Reg8& mask, bool fast, int psm, int fz)
{
	if(m_sel.notest)
	{
		if(fast)
		{
			vmovq(qword[_m_local__gd__vm + addr * 2], src);
			vmovhps(qword[_m_local__gd__vm + addr * 2 + 8 * 2], src);
		}
		else
		{
			WritePixel_AVX(src, addr, 0, psm);
			WritePixel_AVX(src, addr, 1, psm);
			WritePixel_AVX(src, addr, 2, psm);
			WritePixel_AVX(src, addr, 3, psm);
		}
	}
	else
	{
		if(fast)
		{
			// if(fzm & 0x0f) GSVector4i::storel(&vm16[addr + 0], fs);
			// if(fzm & 0xf0) GSVector4i::storeh(&vm16[addr + 8], fs);

			test(mask, 0x0f);
			je("@f");
			vmovq(qword[_m_local__gd__vm + addr * 2], src);
			L("@@");

			test(mask, 0xf0);
			je("@f");
			vmovhps(qword[_m_local__gd__vm + addr * 2 + 8 * 2], src);
			L("@@");

			// vmaskmovps?
		}
		else
		{
			// if(fzm & 0x03) WritePixel(fpsm, &vm16[addr + 0], fs.extract32<0>());
			// if(fzm & 0x0c) WritePixel(fpsm, &vm16[addr + 2], fs.extract32<1>());
			// if(fzm & 0x30) WritePixel(fpsm, &vm16[addr + 8], fs.extract32<2>());
			// if(fzm & 0xc0) WritePixel(fpsm, &vm16[addr + 10], fs.extract32<3>());

			test(mask, 0x03);
			je("@f");
			WritePixel_AVX(src, addr, 0, psm);
			L("@@");

			test(mask, 0x0c);
			je("@f");
			WritePixel_AVX(src, addr, 1, psm);
			L("@@");

			test(mask, 0x30);
			je("@f");
			WritePixel_AVX(src, addr, 2, psm);
			L("@@");

			test(mask, 0xc0);
			je("@f");
			WritePixel_AVX(src, addr, 3, psm);
			L("@@");
		}
	}
}

static const int s_offsets[4] = {0, 2, 8, 10};

void GSDrawScanlineCodeGenerator::WritePixel_AVX(const Xmm& src, const Reg64& addr, uint8 i, int psm)
{
	Address dst = ptr[_m_local__gd__vm + addr * 2 + s_offsets[i] * 2];

	switch(psm)
	{
	case 0:
		if(i == 0) vmovd(dst, src);
		else vpextrd(dst, src, i);
		break;
	case 1:
		if(i == 0) vmovd(eax, src);
		else vpextrd(eax, src, i);
		xor(eax, dst);
		and(eax, 0xffffff);
		xor(dst, eax);
		break;
	case 2:
		vpextrw(eax, src, i * 2);
		mov(dst, ax);
		break;
	}
}

void GSDrawScanlineCodeGenerator::ReadTexel_AVX(int pixels, int mip_offset)
{
	const int in[] = {0, 1, 2, 3};
	const int out[] = {4, 5, 0, 1};

	for(int i = 0; i < pixels; i++)
	{
		for(uint8 j = 0; j < 4; j++)
		{
			ReadTexel_AVX(Xmm(out[i]), Xmm(in[i]), j);
		}
	}
}

void GSDrawScanlineCodeGenerator::ReadTexel_AVX(const Xmm& dst, const Xmm& addr, uint8 i)
{
	const Address& src = m_sel.tlu ? ptr[_m_local__gd__clut + rax * 4] : ptr[_m_local__gd__tex + rax * 4];

	// Extract address offset
	if(i == 0) vmovd(eax, addr);
	else vpextrd(eax, addr, i);

	// If clut, load the value as a byte index
	if(m_sel.tlu) movzx(eax, byte[_m_local__gd__tex + rax]);

	if(i == 0) vmovd(dst, src);
	else vpinsrd(dst, src, i);
}

// Gather example (AVX2). Not faster on Haswell but potentially better on recent CPU
// Worst case reduce Icache.
//
// Current limitation requires 1 extra free register for the mask.
// And palette need zero masking.
// It is not possible to use same source/destination so linear interpolation must be updated
#if 0
void GSDrawScanlineCodeGenerator::ReadTexel_AVX(int pixels, int mip_offset)
{
	const int in[]   = {0, 1, 2, 3};
	const int out[]  = {4, 5, 0, 1};
	const int mask[] = {5, 0, 1, 2};

	if (m_sel.tlu) {
		for(int i = 0; i < pixels; i++) {
			// FIXME can't use same dst and add register
			Gather4Texel(Xmm(in[i]), _m_local__gd__tex, Xmm(in[i]), Xmm(mask[i]));
			// FIXME need a memory and could be faster
			vpslld(Xmm(in[i]), 24);
			vpsrld(Xmm(in[i]), 24);
			Gather4Texel(Xmm(out[i]), _m_local__gd__clut, Xmm(in[i]), Xmm(mask[i]));
		}
	} else {
		for(int i = 0; i < pixels; i++) {
			Gather4Texel(Xmm(out[i]), _m_local__gd__tex, Xmm(in[i]), Xmm(mask[i]));
		}
	}
}

static void Gather4Texel(const Xmm& dst, const Reg64& base, const Xmm& addr, const Xmm& Mask)
{
	//void vpgatherdd(const Xmm& x1, const Address& addr, const Xmm& x2)
	vpcmpeqd(Mask, Mask);
	vpgatherdd(dst, ptr[base + addr * 4], Mask);
}

#endif

#endif
