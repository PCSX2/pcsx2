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
#include "GSSetupPrimCodeGenerator.h"
#include "GSVertexSW.h"
#include "GS/GS_codegen.h"

#if _M_SSE >= 0x501 && (defined(_M_AMD64) || defined(_WIN64))

#define _rip_local(field) (m_rip ? ptr[rip + &m_local.field] : ptr[t0 + offsetof(GSScanlineLocalData, field)])
#define _rip_local_v(field, offset) (m_rip ? ptr[rip + &m_local.field] : ptr[t0 + offset])

#define _m_shift(i) (Ymm(7 + i))

// FIXME windows ?
#define _vertex rcx

void GSSetupPrimCodeGenerator::Generate_AVX2()
{
	// Technically we just need the delta < 2GB
	m_rip = (size_t)&m_local < 0x80000000 && (size_t)getCurr() < 0x80000000;

#ifdef _WIN64
	sub(rsp, 8 + 2 * 16);

	vmovdqa(ptr[rsp + 0], ymm6);
	vmovdqa(ptr[rsp + 16], ymm7);
#endif

	if (!m_rip)
		mov(t0, (size_t)&m_local);

	if ((m_en.z || m_en.f) && m_sel.prim != GS_SPRITE_CLASS || m_en.t || m_en.c && m_sel.iip)
	{
		mov(rax, (size_t)g_const->m_shift_256b);

		for (int i = 0; i < (m_sel.notest ? 2 : 9); i++)
		{
			vmovaps(_m_shift(i), ptr[rax + i * 32]);
		}
	}
	// ymm7 to ymm 15 = m_shift[i]

	Depth_AVX2();

	Texture_AVX2();

	Color_AVX2();

#ifdef _WIN64
	vmovdqa(ymm6, ptr[rsp + 0]);
	vmovdqa(ymm7, ptr[rsp + 16]);

	add(rsp, 8 + 2 * 16);
#endif

	ret();
}

void GSSetupPrimCodeGenerator::Depth_AVX2()
{
	if (!m_en.z && !m_en.f)
	{
		return;
	}

	if (m_sel.prim != GS_SPRITE_CLASS)
	{
		const Ymm& dscan_p = ymm6;

		// GSVector4 dp8 = dscan.p * GSVector4::broadcast32(&shift[0]);

		vbroadcastf128(dscan_p, ptr[a2 + offsetof(GSVertexSW, p)]);

		vmulps(ymm1, dscan_p, _m_shift(0));

		if (m_en.z)
		{
			// m_local.d8.p.z = dp8.extract32<2>();

			vextractps(_rip_local(d8.p.z), xmm1, 2);

			// GSVector8 dz = GSVector8(dscan.p).zzzz();

			vshufps(ymm2, dscan_p, dscan_p, _MM_SHUFFLE(2, 2, 2, 2));

			for (int i = 0; i < (m_sel.notest ? 1 : 8); i++)
			{
				// m_local.d[i].z = dz * shift[1 + i];

				vmulps(ymm0, ymm2, _m_shift(1 + i));

				const size_t variableOffset = offsetof(GSScanlineLocalData, d[0].z) + (i * sizeof(GSScanlineLocalData::d[0]));
				vmovaps(_rip_local_v(d[i].z, variableOffset), ymm0);
			}
		}

		if (m_en.f)
		{
			// m_local.d8.p.f = GSVector4i(dp8).extract32<3>();

			// FIXME no truncate ? why ? vcvttps2dq ?
			//vcvtps2dq(ymm2, ymm1); // let's guess a typo
			vcvttps2dq(ymm2, ymm1);
			vpextrd(_rip_local(d8.p.f), xmm2, 3);

			// GSVector8 df = GSVector8(dscan.p).wwww();

			vshufps(ymm3, dscan_p, dscan_p, _MM_SHUFFLE(3, 3, 3, 3));

			for (int i = 0; i < (m_sel.notest ? 1 : 8); i++)
			{
				// m_local.d[i].f = GSVector8i(df * m_shift[i]).xxzzlh();

				vmulps(ymm0, ymm3, _m_shift(1 + i));
				vcvttps2dq(ymm0, ymm0);

				vpshuflw(ymm0, ymm0, _MM_SHUFFLE(2, 2, 0, 0));
				vpshufhw(ymm0, ymm0, _MM_SHUFFLE(2, 2, 0, 0));

				const size_t variableOffset = offsetof(GSScanlineLocalData, d[0].f) + (i * sizeof(GSScanlineLocalData::d[0]));
				vmovdqa(_rip_local_v(d[i].f, variableOffset), ymm0);
			}
		}
	}
	else
	{
		// GSVector4 p = vertex[index[1]].p;

		mov(_vertex.cvt32(), ptr[a1 + sizeof(uint32) * 1]);
		shl(_vertex.cvt32(), 6); // * sizeof(GSVertexSW)
		add(_vertex, a0);

		if (m_en.f)
		{
			// m_local.p.f = GSVector4i(vertex[index[1]].p).extract32<3>();

			vmovaps(xmm0, ptr[_vertex + offsetof(GSVertexSW, p)]);
			vcvttps2dq(xmm0, xmm0);
			vpextrd(_rip_local(p.f), xmm0, 3);
		}

		if (m_en.z)
		{
			// m_local.p.z = vertex[index[1]].t.u32[3]; // uint32 z is bypassed in t.w

			mov(eax, ptr[ecx + offsetof(GSVertexSW, t.w)]);
			mov(_rip_local(p.z), eax);
		}
	}
}

void GSSetupPrimCodeGenerator::Texture_AVX2()
{
	if (!m_en.t)
	{
		return;
	}

	// GSVector8 dt(dscan.t);

	vbroadcastf128(ymm0, ptr[a2 + offsetof(GSVertexSW, t)]);

	// GSVector8 dt8 = dt * shift[0];

	vmulps(ymm1, ymm0, _m_shift(0));

	if (m_sel.fst)
	{
		// m_local.84.stq = GSVector4i(t * 4.0f);

		vcvttps2dq(ymm1, ymm1);

		vmovdqa(_rip_local(d8.stq), xmm1);
	}
	else
	{
		// m_local.d8.stq = t * 4.0f;

		vmovaps(_rip_local(d8.stq), xmm1);
	}

	for (int j = 0, k = m_sel.fst ? 2 : 3; j < k; j++)
	{
		// GSVector8 dstq = dt.xxxx/yyyy/zzzz();

		vshufps(ymm1, ymm0, ymm0, (uint8)_MM_SHUFFLE(j, j, j, j));

		for (int i = 0; i < (m_sel.notest ? 1 : 8); i++)
		{
			// GSVector8 v = dstq * shift[1 + i];

			vmulps(ymm2, ymm1, _m_shift(1 + i));

			if (m_sel.fst)
			{
				// m_local.d[i].s/t = GSVector8::cast(GSVector8i(v));

				vcvttps2dq(ymm2, ymm2);

				const size_t variableOffsetS = offsetof(GSScanlineLocalData, d[0].s) + (i * sizeof(GSScanlineLocalData::d[0]));
				const size_t variableOffsetT = offsetof(GSScanlineLocalData, d[0].t) + (i * sizeof(GSScanlineLocalData::d[0]));

				switch (j)
				{
					case 0: vmovdqa(_rip_local_v(d[i].s, variableOffsetS), ymm2); break;
					case 1: vmovdqa(_rip_local_v(d[i].t, variableOffsetT), ymm2); break;
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
					case 0: vmovaps(_rip_local_v(d[i].s, variableOffsetS), ymm2); break;
					case 1: vmovaps(_rip_local_v(d[i].t, variableOffsetT), ymm2); break;
					case 2: vmovaps(_rip_local_v(d[i].q, variableOffsetQ), ymm2); break;
				}
			}
		}
	}
}

void GSSetupPrimCodeGenerator::Color_AVX2()
{
	if (!m_en.c)
	{
		return;
	}

	if (m_sel.iip)
	{
		const Ymm& dscan_c = ymm6;

		// GSVector8 dc(dscan.c);

		vbroadcastf128(dscan_c, ptr[a2 + offsetof(GSVertexSW, c)]);

		// m_local.d8.c = GSVector4i(c * 4.0f).xzyw().ps32();

		vmulps(ymm1, dscan_c, ymm3);
		vcvttps2dq(ymm1, ymm1);
		vpshufd(ymm1, ymm1, _MM_SHUFFLE(3, 1, 2, 0));
		vpackssdw(ymm1, ymm1);
		vmovq(_rip_local(d8.c), xmm1);

		// GSVector8 dr = dc.xxxx();
		// GSVector8 db = dc.zzzz();

		vshufps(ymm2, dscan_c, dscan_c, _MM_SHUFFLE(0, 0, 0, 0));
		vshufps(ymm3, dscan_c, dscan_c, _MM_SHUFFLE(2, 2, 2, 2));

		for (int i = 0; i < (m_sel.notest ? 1 : 8); i++)
		{
			// GSVector8i r = GSVector8i(dr * shift[1 + i]).ps32();

			vmulps(ymm0, ymm2, _m_shift(1 + i));
			vcvttps2dq(ymm0, ymm0);
			vpackssdw(ymm0, ymm0);

			// GSVector4i b = GSVector8i(db * shift[1 + i]).ps32();

			vmulps(ymm1, ymm3, _m_shift(1 + i));
			vcvttps2dq(ymm1, ymm1);
			vpackssdw(ymm1, ymm1);

			// m_local.d[i].rb = r.upl16(b);

			vpunpcklwd(ymm0, ymm1);

			const size_t variableOffset = offsetof(GSScanlineLocalData, d[0].rb) + (i * sizeof(GSScanlineLocalData::d[0]));
			vmovdqa(_rip_local_v(d[i].rb, variableOffset), ymm0);
		}

		// GSVector8 dg = dc.yyyy();
		// GSVector8 da = dc.wwww();

		vshufps(ymm2, dscan_c, dscan_c, _MM_SHUFFLE(1, 1, 1, 1));
		vshufps(ymm3, dscan_c, dscan_c, _MM_SHUFFLE(3, 3, 3, 3));

		for (int i = 0; i < (m_sel.notest ? 1 : 8); i++)
		{
			// GSVector8i g = GSVector8i(dg * shift[1 + i]).ps32();

			vmulps(ymm0, ymm2, _m_shift(1 + i));
			vcvttps2dq(ymm0, ymm0);
			vpackssdw(ymm0, ymm0);

			// GSVector8i a = GSVector8i(da * shift[1 + i]).ps32();

			vmulps(ymm1, ymm3, _m_shift(1 + i));
			vcvttps2dq(ymm1, ymm1);
			vpackssdw(ymm1, ymm1);

			// m_local.d[i].ga = g.upl16(a);

			vpunpcklwd(ymm0, ymm1);

			const size_t variableOffset = offsetof(GSScanlineLocalData, d[0].ga) + (i * sizeof(GSScanlineLocalData::d[0]));
			vmovdqa(_rip_local_v(d[i].ga, variableOffset), ymm0);
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
			mov(_vertex.cvt32(), ptr[a1 + sizeof(uint32) * last]);
			shl(_vertex.cvt32(), 6); // * sizeof(GSVertexSW)
			add(_vertex, a0);
		}

		vbroadcasti128(ymm0, ptr[_vertex + offsetof(GSVertexSW, c)]);
		vcvttps2dq(ymm0, ymm0);

		// c = c.upl16(c.zwxy());

		vpshufd(ymm1, ymm0, _MM_SHUFFLE(1, 0, 3, 2));
		vpunpcklwd(ymm0, ymm1);

		// if(!tme) c = c.srl16(7);

		if (m_sel.tfx == TFX_NONE)
		{
			vpsrlw(ymm0, 7);
		}

		// m_local.c.rb = c.xxxx();
		// m_local.c.ga = c.zzzz();

		vpshufd(ymm1, ymm0, _MM_SHUFFLE(0, 0, 0, 0));
		vpshufd(ymm2, ymm0, _MM_SHUFFLE(2, 2, 2, 2));

		vmovdqa(_rip_local(c.rb), ymm1);
		vmovdqa(_rip_local(c.ga), ymm2);
	}
}

#endif
