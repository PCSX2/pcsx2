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
#include "GSSetupPrimCodeGenerator.all.h"
#include "GSVertexSW.h"

using namespace Xbyak;

#define _rip_local(field) ((m_rip) ? ptr[rip + (char*)&m_local.field] : ptr[_m_local + OFFSETOF(GSScanlineLocalData, field)])

#define _64_m_local _64_t0

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

#if _M_SSE >= 0x501
	#define _rip_local_d(x) _rip_local(d8.x)
	#define _rip_local_d_p(x) _rip_local_d(p.x)
#else
	#define _rip_local_d(x) _rip_local(d4.x)
	#define _rip_local_d_p(x) _rip_local_d(x)
#endif

GSSetupPrimCodeGenerator2::GSSetupPrimCodeGenerator2(Xbyak::CodeGenerator* base, CPUInfo cpu, void* param, u64 key)
	: _parent(base, cpu)
	, m_local(*(GSScanlineLocalData*)param)
	, m_rip(false), many_regs(false)
	// On x86 arg registers are very temporary but on x64 they aren't, so on x86 some registers overlap
#ifdef _WIN32
	, _64_vertex(rcx)
	, _index(rdx)
	, _dscan(r8)
	, _64_t0(r9), t1(r10)
#else
	, _64_vertex(rdi)
	, _index(rsi)
	, _dscan(rdx)
	, _64_t0(rcx), t1(r8)
#endif
	, _m_local(chooseLocal(&m_local, _64_m_local))
{
	m_sel.key = key;

	m_en.z = m_sel.zb ? 1 : 0;
	m_en.f = m_sel.fb && m_sel.fge ? 1 : 0;
	m_en.t = m_sel.fb && m_sel.tfx != TFX_NONE ? 1 : 0;
	m_en.c = m_sel.fb && !(m_sel.tfx == TFX_DECAL && m_sel.tcc) ? 1 : 0;
}

void GSSetupPrimCodeGenerator2::broadcastf128(const XYm& reg, const Address& mem)
{
#if SETUP_PRIM_USING_YMM
	vbroadcastf128(reg, mem);
#else
	movaps(reg, mem);
#endif
}

void GSSetupPrimCodeGenerator2::broadcastss(const XYm& reg, const Address& mem)
{
	if (hasAVX)
	{
		vbroadcastss(reg, mem);
	}
	else
	{
		movss(reg, mem);
		shufps(reg, reg, _MM_SHUFFLE(0, 0, 0, 0));
	}
}

void GSSetupPrimCodeGenerator2::Generate()
{
	// Technically we just need the delta < 2GB
	m_rip = (size_t)&m_local < 0x80000000 && (size_t)getCurr() < 0x80000000;

	bool needs_shift = (m_en.z || m_en.f) && m_sel.prim != GS_SPRITE_CLASS || m_en.t || m_en.c && m_sel.iip;
	many_regs = isYmm && !m_sel.notest && needs_shift;

#ifdef _WIN64
	int needs_saving = many_regs ? 6 : m_sel.notest ? 0 : 2;
	if (needs_saving)
	{
		sub(rsp, 8 + 16 * needs_saving);
		for (int i = 0; i < needs_saving; i++)
		{
			movdqa(ptr[rsp + i * 16], Xmm(i + 6));
		}
	}
#endif

	if (!m_rip)
		mov(_64_m_local, (size_t)&m_local);

	if (needs_shift)
	{

		if (isXmm)
			mov(rax, (size_t)g_const->m_shift_128b);
		else
			mov(rax, (size_t)g_const->m_shift_256b);

		for (int i = 0; i < (m_sel.notest ? 2 : many_regs ? 9 : 5); i++)
		{
			movaps(XYm(3 + i), ptr[rax + i * vecsize]);
		}
	}

	if (isXmm)
		Depth_XMM();
	else
		Depth_YMM();

	Texture();

	Color();

#ifdef _WIN64
	if (needs_saving)
	{
		for (int i = 0; i < needs_saving; i++)
		{
			movdqa(Xmm(i + 6), ptr[rsp + i * 16]);
		}
		add(rsp, 8 + 16 * needs_saving);
	}
#endif
	if (isYmm)
		vzeroupper();
	ret();
}

void GSSetupPrimCodeGenerator2::Depth_XMM()
{
	if (!m_en.z && !m_en.f)
	{
		return;
	}

	if (m_sel.prim != GS_SPRITE_CLASS)
	{
		if (m_en.f)
		{
			// GSVector4 df = t.wwww();
			broadcastss(xym1, ptr[_dscan + offsetof(GSVertexSW, t.w)]);

			// m_local.d4.f = GSVector4i(df * 4.0f).xxzzlh();

			THREEARG(mulps, xmm2, xmm1, xmm3);
			cvttps2dq(xmm2, xmm2);
			pshuflw(xmm2, xmm2, _MM_SHUFFLE(2, 2, 0, 0));
			pshufhw(xmm2, xmm2, _MM_SHUFFLE(2, 2, 0, 0));
			movdqa(_rip_local_d_p(f), xmm2);

			for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
			{
				// m_local.d[i].f = GSVector4i(df * m_shift[i]).xxzzlh();

				THREEARG(mulps, xmm2, xmm1, XYm(4 + i));
				cvttps2dq(xmm2, xmm2);
				pshuflw(xmm2, xmm2, _MM_SHUFFLE(2, 2, 0, 0));
				pshufhw(xmm2, xmm2, _MM_SHUFFLE(2, 2, 0, 0));
				movdqa(_rip_local(d[i].f), xmm2);
			}
		}

		if (m_en.z)
		{
			// VectorF dz = VectorF::broadcast64(&dscan.p.z)
			movddup(xmm0, ptr[_dscan + offsetof(GSVertexSW, p.z)]);

			// m_local.d4.z = dz.mul64(GSVector4::f32to64(shift));
			cvtps2pd(xmm1, xmm3);
			mulpd(xmm1, xmm0);
			movaps(_rip_local_d_p(z), xmm1);

			cvtpd2ps(xmm0, xmm0);
			unpcklpd(xmm0, xmm0);

			for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
			{
				// m_local.d[i].z0 = dz.mul64(VectorF::f32to64(half_shift[2 * i + 2]));
				// m_local.d[i].z1 = dz.mul64(VectorF::f32to64(half_shift[2 * i + 3]));

				THREEARG(mulps, xmm1, xmm0, XYm(4 + i));
				movdqa(_rip_local(d[i].z), xmm1);
			}
		}
	}
	else
	{
		// GSVector4 p = vertex[index[1]].p;

		mov(eax, ptr[_index + sizeof(u32) * 1]);
		shl(eax, 6); // * sizeof(GSVertexSW)
		add(rax, _64_vertex);

		if (m_en.f)
		{
			// m_local.p.f = GSVector4i(p).zzzzh().zzzz();
			movaps(xmm0, ptr[rax + offsetof(GSVertexSW, p)]);

			cvttps2dq(xmm1, xmm0);
			pshufhw(xmm1, xmm1, _MM_SHUFFLE(2, 2, 2, 2));
			pshufd(xmm1, xmm1, _MM_SHUFFLE(2, 2, 2, 2));
			movdqa(_rip_local(p.f), xmm1);
		}

		if (m_en.z)
		{
			// u32 z is bypassed in t.w

			movdqa(xmm0, ptr[rax + offsetof(GSVertexSW, t)]);
			pshufd(xmm0, xmm0, _MM_SHUFFLE(3, 3, 3, 3));
			movdqa(_rip_local(p.z), xmm0);
		}
	}
}

void GSSetupPrimCodeGenerator2::Depth_YMM()
{
	if (!m_en.z && !m_en.f)
	{
		return;
	}

	if (m_sel.prim != GS_SPRITE_CLASS)
	{
		if (m_en.f)
		{
			// GSVector8 df = GSVector8::broadcast32(&dscan.t.w);
			vbroadcastss(ymm1, ptr[_dscan + offsetof(GSVertexSW, t.w)]);

			// local.d8.p.f = GSVector4i(tstep).extract32<3>();
			vmulps(xmm0, xmm1, xmm3);
			cvtps2dq(xmm0, xmm0);
			movd(_rip_local_d_p(f), xmm0);

			for (int i = 0; i < (m_sel.notest ? 1 : dsize); i++)
			{
				// m_local.d[i].f = GSVectorI(df * m_shift[i]).xxzzlh();

				if (i < 4 || many_regs)
					vmulps(ymm0, Ymm(4 + i), ymm1);
				else
					vmulps(ymm0, ymm1, ptr[g_const->m_shift_256b[i + 1]]);
				cvttps2dq(ymm0, ymm0);
				pshuflw(ymm0, ymm0, _MM_SHUFFLE(2, 2, 0, 0));
				pshufhw(ymm0, ymm0, _MM_SHUFFLE(2, 2, 0, 0));
				movdqa(_rip_local(d[i].f), ymm0);
			}
		}

		if (m_en.z)
		{
			// const VectorF dz = VectorF::broadcast64(&dscan.p.z);
			movsd(xmm0, ptr[_dscan + offsetof(GSVertexSW, p.z)]);

			// GSVector4::storel(&local.d8.p.z, dz.extract<0>().mul64(GSVector4::f32to64(shift)));
			vcvtss2sd(xmm1, xmm3, xmm3);
			vmulsd(xmm1, xmm0, xmm1);
			movsd(_rip_local_d_p(z), xmm1);

			cvtsd2ss(xmm0, xmm0);
			vbroadcastss(ymm0, xmm0);

			for (int i = 0; i < (m_sel.notest ? 1 : dsize); i++)
			{
				// m_local.d[i].z = dzf * shift[i + 1];

				if (i < 4 || many_regs)
					vmulps(ymm1, Ymm(4 + i), ymm0);
				else
					vmulps(ymm1, ymm0, ptr[g_const->m_shift_256b[i + 1]]);
				movaps(_rip_local(d[i].z), ymm1);
			}
		}
	}
	else
	{
		// GSVector4 p = vertex[index[1]].p;

		mov(eax, ptr[_index + sizeof(u32) * 1]);
		shl(eax, 6); // * sizeof(GSVertexSW)
		add(rax, _64_vertex);

		if (m_en.f)
		{
			// m_local.p.f = GSVector4i(vertex[index[1]].p).extract32<3>();

			movaps(xmm0, ptr[rax + offsetof(GSVertexSW, p)]);
			cvttps2dq(xmm0, xmm0);
			pextrd(_rip_local(p.f), xmm0, 3);
		}

		if (m_en.z)
		{
			// m_local.p.z = vertex[index[1]].t.u32[3]; // u32 z is bypassed in t.w

			mov(t1.cvt32(), ptr[rax + offsetof(GSVertexSW, t.w)]);
			mov(_rip_local(p.z), t1.cvt32());
		}
	}
}

void GSSetupPrimCodeGenerator2::Texture()
{
	if (!m_en.t)
	{
		return;
	}

	// GSVector4 t = dscan.t;

	broadcastf128(xym0, ptr[_dscan + offsetof(GSVertexSW, t)]);

	THREEARG(mulps, xmm1, xmm0, xmm3);

	if (m_sel.fst)
	{
		// m_local.d4.stq = GSVector4i(t * 4.0f);

		cvttps2dq(xmm1, xmm1);

		movdqa(_rip_local_d(stq), xmm1);
	}
	else
	{
		// m_local.d4.stq = t * 4.0f;

		movaps(_rip_local_d(stq), xmm1);
	}

	for (int j = 0, k = m_sel.fst ? 2 : 3; j < k; j++)
	{
		// GSVector4 ds = t.xxxx();
		// GSVector4 dt = t.yyyy();
		// GSVector4 dq = t.zzzz();

		THREEARG(shufps, xym1, xym0, xym0, _MM_SHUFFLE(j, j, j, j));

		for (int i = 0; i < (m_sel.notest ? 1 : dsize); i++)
		{
			// GSVector4 v = ds/dt * m_shift[i];

			if (i < 4 || many_regs)
				THREEARG(mulps, xym2, XYm(4 + i), xym1);
			else
				vmulps(ymm2, ymm1, ptr[g_const->m_shift_256b[i + 1]]);

			if (m_sel.fst)
			{
				// m_local.d[i].s/t = GSVector4i(v);

				cvttps2dq(xym2, xym2);

				switch (j)
				{
					case 0: movdqa(_rip_local(d[i].s), xym2); break;
					case 1: movdqa(_rip_local(d[i].t), xym2); break;
				}
			}
			else
			{
				// m_local.d[i].s/t/q = v;

				switch (j)
				{
					case 0: movaps(_rip_local(d[i].s), xym2); break;
					case 1: movaps(_rip_local(d[i].t), xym2); break;
					case 2: movaps(_rip_local(d[i].q), xym2); break;
				}
			}
		}
	}
}

void GSSetupPrimCodeGenerator2::Color()
{
	if (!m_en.c)
	{
		return;
	}

	if (m_sel.iip)
	{
		// GSVector4 c = dscan.c;

		broadcastf128(xym0, ptr[_dscan + offsetof(GSVertexSW, c)]);

		// m_local.d4.c = GSVector4i(c * 4.0f).xzyw().ps32();

		THREEARG(mulps, xmm1, xmm0, xmm3);
		cvttps2dq(xmm1, xmm1);
		pshufd(xmm1, xmm1, _MM_SHUFFLE(3, 1, 2, 0));
		packssdw(xmm1, xmm1);
		if (isXmm)
			movdqa(_rip_local_d(c), xmm1);
		else
			movq(_rip_local_d(c), xmm1);

		// xym3 is not needed anymore

		// GSVector4 dr = c.xxxx();
		// GSVector4 db = c.zzzz();

		THREEARG(shufps, xym2, xym0, xym0, _MM_SHUFFLE(0, 0, 0, 0));
		THREEARG(shufps, xym3, xym0, xym0, _MM_SHUFFLE(2, 2, 2, 2));

		for (int i = 0; i < (m_sel.notest ? 1 : dsize); i++)
		{
			// GSVector4i r = GSVector4i(dr * m_shift[i]).ps32();

			if (i < 4 || many_regs)
				THREEARG(mulps, xym0, XYm(4 + i), xym2);
			else
				vmulps(ymm0, ymm2, ptr[g_const->m_shift_256b[i + 1]]);
			cvttps2dq(xym0, xym0);
			packssdw(xym0, xym0);

			// GSVector4i b = GSVector4i(db * m_shift[i]).ps32();

			if (i < 4 || many_regs)
				THREEARG(mulps, xym1, XYm(4 + i), xym3);
			else
				vmulps(ymm1, ymm3, ptr[g_const->m_shift_256b[i + 1]]);
			cvttps2dq(xym1, xym1);
			packssdw(xym1, xym1);

			// m_local.d[i].rb = r.upl16(b);

			punpcklwd(xym0, xym1);
			movdqa(_rip_local(d[i].rb), xym0);
		}

		// GSVector4 c = dscan.c;

		broadcastf128(xym0, ptr[_dscan + offsetof(GSVertexSW, c)]); // not enough regs, have to reload it

		// GSVector4 dg = c.yyyy();
		// GSVector4 da = c.wwww();

		THREEARG(shufps, xym2, xym0, xym0, _MM_SHUFFLE(1, 1, 1, 1));
		THREEARG(shufps, xym3, xym0, xym0, _MM_SHUFFLE(3, 3, 3, 3));

		for (int i = 0; i < (m_sel.notest ? 1 : dsize); i++)
		{
			// GSVector4i g = GSVector4i(dg * m_shift[i]).ps32();

			if (i < 4 || many_regs)
				THREEARG(mulps, xym0, XYm(4 + i), xym2);
			else
				vmulps(ymm0, ymm2, ptr[g_const->m_shift_256b[i + 1]]);
			cvttps2dq(xym0, xym0);
			packssdw(xym0, xym0);

			// GSVector4i a = GSVector4i(da * m_shift[i]).ps32();

			if (i < 4 || many_regs)
				THREEARG(mulps, xym1, XYm(4 + i), xym3);
			else
				vmulps(ymm1, ymm3, ptr[g_const->m_shift_256b[i + 1]]);
			cvttps2dq(xym1, xym1);
			packssdw(xym1, xym1);

			// m_local.d[i].ga = g.upl16(a);

			punpcklwd(xym0, xym1);
			movdqa(_rip_local(d[i].ga), xym0);
		}
	}
	else
	{
		// GSVector4i c = GSVector4i(vertex[index[last].c);

		int last = 0;

		switch (m_sel.prim)
		{
			case GS_POINT_CLASS:    last = 0; break;
			case GS_LINE_CLASS:     last = 1; break;
			case GS_TRIANGLE_CLASS: last = 2; break;
			case GS_SPRITE_CLASS:   last = 1; break;
		}

		if (!(m_sel.prim == GS_SPRITE_CLASS && (m_en.z || m_en.f))) // if this is a sprite, the last vertex was already loaded in Depth()
		{
			mov(eax, ptr[_index + sizeof(u32) * last]);
			shl(eax, 6); // * sizeof(GSVertexSW)
			add(rax, _64_vertex);
		}

		if (isXmm)
		{
			cvttps2dq(xmm0, ptr[rax + offsetof(GSVertexSW, c)]);
		}
		else
		{
			vbroadcasti128(ymm0, ptr[rax + offsetof(GSVertexSW, c)]);
			cvttps2dq(ymm0, ymm0);
		}

		// c = c.upl16(c.zwxy());

		pshufd(xym1, xym0, _MM_SHUFFLE(1, 0, 3, 2));
		punpcklwd(xym0, xym1);

		// if(!tme) c = c.srl16(7);

		if (m_sel.tfx == TFX_NONE)
		{
			psrlw(xym0, 7);
		}

		// m_local.c.rb = c.xxxx();
		// m_local.c.ga = c.zzzz();

		pshufd(xym1, xym0, _MM_SHUFFLE(0, 0, 0, 0));
		pshufd(xym2, xym0, _MM_SHUFFLE(2, 2, 2, 2));

		movdqa(_rip_local(c.rb), xym1);
		movdqa(_rip_local(c.ga), xym2);
	}
}
