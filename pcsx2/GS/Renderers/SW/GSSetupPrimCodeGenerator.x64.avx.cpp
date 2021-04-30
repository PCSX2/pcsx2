/*
 *	Copyright (C) 2016-2016 PCSX2 Dev Team
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
#include "GSSetupPrimCodeGenerator.h"
#include "GSVertexSW.h"

#if _M_SSE < 0x501 && (defined(_M_AMD64) || defined(_WIN64))

using namespace Xbyak;

#define _rip_local(field) (m_rip ? ptr[rip + &m_local.field] : ptr[t0 + offsetof(GSScanlineLocalData, field)])
#define _rip_local_v(field, offset) (m_rip ? ptr[rip + &m_local.field] : ptr[t0 + offset])

void GSSetupPrimCodeGenerator::Generate_AVX()
{
	// Technically we just need the delta < 2GB
	m_rip = (size_t)&m_local < 0x80000000 && (size_t)getCurr() < 0x80000000;

#ifdef _WIN64
	sub(rsp, 8 + 2 * 16);

	vmovdqa(ptr[rsp + 0], xmm6);
	vmovdqa(ptr[rsp + 16], xmm7);
#endif

	if (!m_rip)
		mov(t0, (size_t)&m_local);

	if ((m_en.z || m_en.f) && m_sel.prim != GS_SPRITE_CLASS || m_en.t || m_en.c && m_sel.iip)
	{
		mov(rax, (size_t)g_const->m_shift_128b);

		for (int i = 0; i < (m_sel.notest ? 2 : 5); i++)
		{
			vmovaps(Xmm(3 + i), ptr[rax + i * 16]);
		}
	}

	Depth_AVX();

	Texture_AVX();

	Color_AVX();

#ifdef _WIN64
	vmovdqa(xmm6, ptr[rsp + 0]);
	vmovdqa(xmm7, ptr[rsp + 16]);

	add(rsp, 8 + 2 * 16);
#endif

	ret();
}

void GSSetupPrimCodeGenerator::Depth_AVX()
{
	if (!m_en.z && !m_en.f)
	{
		return;
	}

	if (m_sel.prim != GS_SPRITE_CLASS)
	{
		// GSVector4 p = dscan.p;

		vmovaps(xmm0, ptr[a2 + offsetof(GSVertexSW, p)]);

		if (m_en.f)
		{
			// GSVector4 df = p.wwww();

			vshufps(xmm1, xmm0, xmm0, _MM_SHUFFLE(3, 3, 3, 3));

			// m_local.d4.f = GSVector4i(df * 4.0f).xxzzlh();

			vmulps(xmm2, xmm1, xmm3);
			vcvttps2dq(xmm2, xmm2);
			vpshuflw(xmm2, xmm2, _MM_SHUFFLE(2, 2, 0, 0));
			vpshufhw(xmm2, xmm2, _MM_SHUFFLE(2, 2, 0, 0));
			vmovdqa(_rip_local(d4.f), xmm2);

			for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
			{
				// m_local.d[i].f = GSVector4i(df * m_shift[i]).xxzzlh();

				vmulps(xmm2, xmm1, Xmm(4 + i));
				vcvttps2dq(xmm2, xmm2);
				vpshuflw(xmm2, xmm2, _MM_SHUFFLE(2, 2, 0, 0));
				vpshufhw(xmm2, xmm2, _MM_SHUFFLE(2, 2, 0, 0));

				const size_t variableOffset = offsetof(GSScanlineLocalData, d[0].f) + (i * sizeof(GSScanlineLocalData::d[0]));
				vmovdqa(_rip_local_v(d[i].f, variableOffset), xmm2);
			}
		}

		if (m_en.z)
		{
			// GSVector4 dz = p.zzzz();

			vshufps(xmm0, xmm0, _MM_SHUFFLE(2, 2, 2, 2));

			// m_local.d4.z = dz * 4.0f;

			vmulps(xmm1, xmm0, xmm3);
			vmovdqa(_rip_local(d4.z), xmm1);

			for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
			{
				// m_local.d[i].z = dz * m_shift[i];

				vmulps(xmm1, xmm0, Xmm(4 + i));

				const size_t variableOffset = offsetof(GSScanlineLocalData, d[0].z) + (i * sizeof(GSScanlineLocalData::d[0]));
				vmovdqa(_rip_local_v(d[i].z, variableOffset), xmm1);
			}
		}
	}
	else
	{
		// GSVector4 p = vertex[index[1]].p;

		mov(eax, ptr[a1 + sizeof(uint32) * 1]);
		shl(eax, 6); // * sizeof(GSVertexSW)
		add(rax, a0);

		if (m_en.f)
		{
			// m_local.p.f = GSVector4i(p).zzzzh().zzzz();
			vmovaps(xmm0, ptr[rax + offsetof(GSVertexSW, p)]);

			vcvttps2dq(xmm1, xmm0);
			vpshufhw(xmm1, xmm1, _MM_SHUFFLE(2, 2, 2, 2));
			vpshufd(xmm1, xmm1, _MM_SHUFFLE(2, 2, 2, 2));
			vmovdqa(_rip_local(p.f), xmm1);
		}

		if (m_en.z)
		{
			// uint32 z is bypassed in t.w

			vmovdqa(xmm0, ptr[rax + offsetof(GSVertexSW, t)]);
			vpshufd(xmm0, xmm0, _MM_SHUFFLE(3, 3, 3, 3));
			vmovdqa(_rip_local(p.z), xmm0);
		}
	}
}

void GSSetupPrimCodeGenerator::Texture_AVX()
{
	if (!m_en.t)
	{
		return;
	}

	// GSVector4 t = dscan.t;

	vmovaps(xmm0, ptr[a2 + offsetof(GSVertexSW, t)]);

	vmulps(xmm1, xmm0, xmm3);

	if (m_sel.fst)
	{
		// m_local.d4.stq = GSVector4i(t * 4.0f);

		vcvttps2dq(xmm1, xmm1);

		vmovdqa(_rip_local(d4.stq), xmm1);
	}
	else
	{
		// m_local.d4.stq = t * 4.0f;

		vmovaps(_rip_local(d4.stq), xmm1);
	}

	for (int j = 0, k = m_sel.fst ? 2 : 3; j < k; j++)
	{
		// GSVector4 ds = t.xxxx();
		// GSVector4 dt = t.yyyy();
		// GSVector4 dq = t.zzzz();

		vshufps(xmm1, xmm0, xmm0, (uint8)_MM_SHUFFLE(j, j, j, j));

		for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
		{
			// GSVector4 v = ds/dt * m_shift[i];

			vmulps(xmm2, xmm1, Xmm(4 + i));

			if (m_sel.fst)
			{
				// m_local.d[i].s/t = GSVector4i(v);

				vcvttps2dq(xmm2, xmm2);

				const size_t variableOffsetS = offsetof(GSScanlineLocalData, d[0].s) + (i * sizeof(GSScanlineLocalData::d[0]));
				const size_t variableOffsetT = offsetof(GSScanlineLocalData, d[0].t) + (i * sizeof(GSScanlineLocalData::d[0]));

				switch (j)
				{
					case 0: vmovdqa(_rip_local_v(d[i].s, variableOffsetS), xmm2); break;
					case 1: vmovdqa(_rip_local_v(d[i].t, variableOffsetT), xmm2); break;
				}
			}
			else
			{
				// m_local.d[i].s/t/q = v;

				const size_t variableOffsetS = offsetof(GSScanlineLocalData, d[0].s) + (i * sizeof(GSScanlineLocalData::d[0]));
				const size_t variableOffsetT = offsetof(GSScanlineLocalData, d[0].t) + (i * sizeof(GSScanlineLocalData::d[0]));
				const size_t variableOffsetQ = offsetof(GSScanlineLocalData, d[0].q) + (i * sizeof(GSScanlineLocalData::d[0]));

				switch (j)
				{
					case 0: vmovaps(_rip_local_v(d[i].s, variableOffsetS), xmm2); break;
					case 1: vmovaps(_rip_local_v(d[i].t, variableOffsetT), xmm2); break;
					case 2: vmovaps(_rip_local_v(d[i].q, variableOffsetQ), xmm2); break;
				}
			}
		}
	}
}

void GSSetupPrimCodeGenerator::Color_AVX()
{
	if (!m_en.c)
	{
		return;
	}

	if (m_sel.iip)
	{
		// GSVector4 c = dscan.c;

		vmovaps(xmm0, ptr[a2 + offsetof(GSVertexSW, c)]);

		// m_local.d4.c = GSVector4i(c * 4.0f).xzyw().ps32();

		vmulps(xmm1, xmm0, xmm3);
		vcvttps2dq(xmm1, xmm1);
		vpshufd(xmm1, xmm1, _MM_SHUFFLE(3, 1, 2, 0));
		vpackssdw(xmm1, xmm1);
		vmovdqa(_rip_local(d4.c), xmm1);

		// xmm3 is not needed anymore

		// GSVector4 dr = c.xxxx();
		// GSVector4 db = c.zzzz();

		vshufps(xmm2, xmm0, xmm0, _MM_SHUFFLE(0, 0, 0, 0));
		vshufps(xmm3, xmm0, xmm0, _MM_SHUFFLE(2, 2, 2, 2));

		for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
		{
			// GSVector4i r = GSVector4i(dr * m_shift[i]).ps32();

			vmulps(xmm0, xmm2, Xmm(4 + i));
			vcvttps2dq(xmm0, xmm0);
			vpackssdw(xmm0, xmm0);

			// GSVector4i b = GSVector4i(db * m_shift[i]).ps32();

			vmulps(xmm1, xmm3, Xmm(4 + i));
			vcvttps2dq(xmm1, xmm1);
			vpackssdw(xmm1, xmm1);

			// m_local.d[i].rb = r.upl16(b);

			vpunpcklwd(xmm0, xmm1);

			const size_t variableOffset = offsetof(GSScanlineLocalData, d[0].rb) + (i * sizeof(GSScanlineLocalData::d[0]));
			vmovdqa(_rip_local_v(d[i].rb, variableOffset), xmm0);
		}

		// GSVector4 c = dscan.c;

		vmovaps(xmm0, ptr[a2 + offsetof(GSVertexSW, c)]); // not enough regs, have to reload it

		// GSVector4 dg = c.yyyy();
		// GSVector4 da = c.wwww();

		vshufps(xmm2, xmm0, xmm0, _MM_SHUFFLE(1, 1, 1, 1));
		vshufps(xmm3, xmm0, xmm0, _MM_SHUFFLE(3, 3, 3, 3));

		for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
		{
			// GSVector4i g = GSVector4i(dg * m_shift[i]).ps32();

			vmulps(xmm0, xmm2, Xmm(4 + i));
			vcvttps2dq(xmm0, xmm0);
			vpackssdw(xmm0, xmm0);

			// GSVector4i a = GSVector4i(da * m_shift[i]).ps32();

			vmulps(xmm1, xmm3, Xmm(4 + i));
			vcvttps2dq(xmm1, xmm1);
			vpackssdw(xmm1, xmm1);

			// m_local.d[i].ga = g.upl16(a);

			vpunpcklwd(xmm0, xmm1);

			const size_t variableOffset = offsetof(GSScanlineLocalData, d[0].ga) + (i * sizeof(GSScanlineLocalData::d[0]));
			vmovdqa(_rip_local_v(d[i].ga, variableOffset), xmm0);
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
			mov(eax, ptr[a1 + sizeof(uint32) * last]);
			shl(eax, 6); // * sizeof(GSVertexSW)
			add(rax, a0);
		}

		vcvttps2dq(xmm0, ptr[rax + offsetof(GSVertexSW, c)]);

		// c = c.upl16(c.zwxy());

		vpshufd(xmm1, xmm0, _MM_SHUFFLE(1, 0, 3, 2));
		vpunpcklwd(xmm0, xmm1);

		// if(!tme) c = c.srl16(7);

		if (m_sel.tfx == TFX_NONE)
		{
			vpsrlw(xmm0, 7);
		}

		// m_local.c.rb = c.xxxx();
		// m_local.c.ga = c.zzzz();

		vpshufd(xmm1, xmm0, _MM_SHUFFLE(0, 0, 0, 0));
		vpshufd(xmm2, xmm0, _MM_SHUFFLE(2, 2, 2, 2));

		vmovdqa(_rip_local(c.rb), xmm1);
		vmovdqa(_rip_local(c.ga), xmm2);
	}
}

#endif
