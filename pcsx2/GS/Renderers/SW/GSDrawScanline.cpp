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
#include "GSDrawScanline.h"
#include "GSTextureCacheSW.h"

// Lack of a better home
std::unique_ptr<GSScanlineConstantData> g_const(new GSScanlineConstantData());

GSDrawScanline::GSDrawScanline()
	: m_sp_map("GSSetupPrim", &m_local)
	, m_ds_map("GSDrawScanline", &m_local)
{
	memset(&m_local, 0, sizeof(m_local));

	m_local.gd = &m_global;
}

void GSDrawScanline::BeginDraw(const GSRasterizerData* data)
{
	memcpy(&m_global, &((const SharedData*)data)->global, sizeof(m_global));

	if (m_global.sel.mmin && m_global.sel.lcm)
	{
#if defined(__GNUC__) && _M_SSE >= 0x501
		// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80286
		//
		// GCC 4.9/5/6 doesn't generate correct AVX2 code for extract32<0>. It is fixed in GCC7
		// Intrinsic code is _mm_cvtsi128_si32(_mm256_castsi256_si128(m))
		// It seems recent Clang got _mm256_cvtsi256_si32(m) instead. I don't know about GCC.
		//
		// Generated code keep the integer in an XMM register but bit [64:32] aren't cleared.
		// So the srl16 shift will be huge and v will be 0.
		//
		int lod_x = m_global.lod.i.x0;
		GSVector4i v = m_global.t.minmax.srl16(lod_x);
#else
		GSVector4i v = m_global.t.minmax.srl16(m_global.lod.i.extract32<0>()); //.x);
#endif

		v = v.upl16(v);

		m_local.temp.uv_minmax[0] = v.upl32(v);
		m_local.temp.uv_minmax[1] = v.uph32(v);
	}

	m_ds = m_ds_map[m_global.sel];

	if (m_global.sel.aa1)
	{
		GSScanlineSelector sel;

		sel.key = m_global.sel.key;
		sel.zwrite = 0;
		sel.edge = 1;

		m_de = m_ds_map[sel];
	}
	else
	{
		m_de = NULL;
	}

	if (m_global.sel.IsSolidRect())
	{
		m_dr = (DrawRectPtr)&GSDrawScanline::DrawRect;
	}
	else
	{
		m_dr = NULL;
	}

	// doesn't need all bits => less functions generated

	GSScanlineSelector sel;

	sel.key = 0;

	sel.iip = m_global.sel.iip;
	sel.tfx = m_global.sel.tfx;
	sel.tcc = m_global.sel.tcc;
	sel.fst = m_global.sel.fst;
	sel.fge = m_global.sel.fge;
	sel.prim = m_global.sel.prim;
	sel.fb = m_global.sel.fb;
	sel.zb = m_global.sel.zb;
	sel.zoverflow = m_global.sel.zoverflow;
	sel.zequal = m_global.sel.zequal;
	sel.notest = m_global.sel.notest;

	m_sp = m_sp_map[sel];
}

void GSDrawScanline::EndDraw(u64 frame, u64 ticks, int actual, int total, int prims)
{
	m_ds_map.UpdateStats(frame, ticks, actual, total, prims);
}

#if _M_SSE >= 0x501
typedef GSVector8i VectorI;
typedef GSVector8  VectorF;
#define LOCAL_STEP local.d8
#else
typedef GSVector4i VectorI;
typedef GSVector4  VectorF;
#define LOCAL_STEP local.d4
#endif

void GSDrawScanline::CSetupPrim(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan, GSScanlineLocalData& local, const GSScanlineGlobalData& global)
{
	GSScanlineSelector sel = global.sel;

	bool has_z = sel.zb != 0;
	bool has_f = sel.fb && sel.fge;
	bool has_t = sel.fb && sel.tfx != TFX_NONE;
	bool has_c = sel.fb && !(sel.tfx == TFX_DECAL && sel.tcc);

	constexpr int vlen = sizeof(VectorF) / sizeof(float);

#if _M_SSE >= 0x501
	const GSVector8* shift = (GSVector8*)g_const->m_shift_256b;
	const GSVector4 step_shift = GSVector4::broadcast32(&shift[0]);
#else
	const GSVector4* shift = (GSVector4*)g_const->m_shift_128b;
	const GSVector4 step_shift = shift[0];
#endif

	GSVector4 tstep = dscan.t * step_shift;

	if (has_z || has_f)
	{
		if (sel.prim != GS_SPRITE_CLASS)
		{
			if (has_f)
			{
#if _M_SSE >= 0x501
				local.d8.p.f = GSVector4i(tstep).extract32<3>();

				GSVector8 df = GSVector8::broadcast32(&dscan.t.w);
#else
				GSVector4 df = dscan.t.wwww();

				local.d4.f = GSVector4i(tstep).zzzzh().wwww();
#endif

				for (int i = 0; i < vlen; i++)
				{
					local.d[i].f = VectorI(df * shift[1 + i]).xxzzlh();
				}
			}

			if (has_z && !sel.zequal)
			{
				const GSVector4 dz = GSVector4::broadcast64(&dscan.p.z);
				const VectorF dzf(static_cast<float>(dscan.p.F64[1]));
#if _M_SSE >= 0x501
				GSVector4::storel(&local.d8.p.z, dz.mul64(GSVector4::f32to64(shift)));
#else
				local.d4.z = dz.mul64(GSVector4::f32to64(shift));
#endif
				for (int i = 0; i < vlen; i++)
				{
					local.d[i].z = dzf * shift[i + 1];
				}
			}
		}
		else
		{
			if (has_f)
			{
#if _M_SSE >= 0x501
				local.p.f = GSVector4i(vertex[index[1]].p).extract32<3>();
#else
				local.p.f = GSVector4i(vertex[index[1]].p).zzzzh().zzzz();
#endif
			}

			if (has_z)
			{
				local.p.z = vertex[index[1]].t.U32[3]; // u32 z is bypassed in t.w
			}
		}
	}

	if (has_t)
	{
		if (sel.fst)
		{
			LOCAL_STEP.stq = GSVector4::cast(GSVector4i(tstep));
		}
		else
		{
			LOCAL_STEP.stq = tstep;
		}

		VectorF dt(dscan.t);

		for (int j = 0, k = sel.fst ? 2 : 3; j < k; j++)
		{
			VectorF dstq;

			switch (j)
			{
				case 0: dstq = dt.xxxx(); break;
				case 1: dstq = dt.yyyy(); break;
				case 2: dstq = dt.zzzz(); break;
			}

			for (int i = 0; i < vlen; i++)
			{
				VectorF v = dstq * shift[1 + i];

				if (sel.fst)
				{
					switch (j)
					{
						case 0: local.d[i].s = VectorF::cast(VectorI(v)); break;
						case 1: local.d[i].t = VectorF::cast(VectorI(v)); break;
					}
				}
				else
				{
					switch (j)
					{
						case 0: local.d[i].s = v; break;
						case 1: local.d[i].t = v; break;
						case 2: local.d[i].q = v; break;
					}
				}
			}
		}
	}

	if (has_c)
	{
		if (sel.iip)
		{
#if _M_SSE >= 0x501
			GSVector4i::storel(&local.d8.c, GSVector4i(dscan.c * step_shift).xzyw().ps32());
#else
			local.d4.c = GSVector4i(dscan.c * step_shift).xzyw().ps32();
#endif
			VectorF dc(dscan.c);

			VectorF dr = dc.xxxx();
			VectorF db = dc.zzzz();

			for (int i = 0; i < vlen; i++)
			{
				VectorI r = VectorI(dr * shift[1 + i]).ps32();
				VectorI b = VectorI(db * shift[1 + i]).ps32();

				local.d[i].rb = r.upl16(b);
			}

			VectorF dg = dc.yyyy();
			VectorF da = dc.wwww();

			for (int i = 0; i < vlen; i++)
			{
				VectorI g = VectorI(dg * shift[1 + i]).ps32();
				VectorI a = VectorI(da * shift[1 + i]).ps32();

				local.d[i].ga = g.upl16(a);
			}
		}
		else
		{
			int last = 0;

			switch (sel.prim)
			{
				case GS_POINT_CLASS:    last = 0; break;
				case GS_LINE_CLASS:     last = 1; break;
				case GS_TRIANGLE_CLASS: last = 2; break;
				case GS_SPRITE_CLASS:   last = 1; break;
			}

			VectorI c = VectorI(VectorF(vertex[index[last]].c));

			c = c.upl16(c.zwxy());

			if (sel.tfx == TFX_NONE)
				c = c.srl16(7);

			local.c.rb = c.xxxx();
			local.c.ga = c.zzzz();
		}
	}
}

void GSDrawScanline::CDrawScanline(int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local, const GSScanlineGlobalData& global)
{
	GSScanlineSelector sel = global.sel;
	constexpr int vlen = sizeof(VectorF) / sizeof(float);

#if _M_SSE < 0x501
	const GSVector4i* const_test = (GSVector4i*)g_const->m_test_128b;
#endif
	VectorI test;
	VectorF z0, z1;
	VectorI f;
	VectorF s, t, q;
	VectorI uf, vf;
	VectorI rbf, gaf;
	VectorI cov;

	// Init

	int skip, steps;

	if (!sel.notest)
	{
		skip = left & (vlen - 1);
		steps = pixels + skip - vlen;
		left -= skip;
#if _M_SSE >= 0x501
		test = GSVector8i::i8to32(g_const->m_test_256b[skip]) | GSVector8i::i8to32(g_const->m_test_256b[15 + (steps & (steps >> 31))]);
#else
		test = const_test[skip] | const_test[7 + (steps & (steps >> 31))];
#endif
	}
	else
	{
		skip = 0;
		steps = pixels - vlen;
	}

	ASSERT((left & (vlen - 1)) == 0);

	const GSVector2i* fza_base = &global.fzbr[top];
	const GSVector2i* fza_offset = &global.fzbc[left >> 2];

	if (sel.prim != GS_SPRITE_CLASS)
	{
		if (sel.fwrite && sel.fge)
		{
#if _M_SSE >= 0x501
			f = GSVector8i::broadcast16(GSVector4i(scan.t).srl<12>()).add16(local.d[skip].f);
#else
			f = GSVector4i(scan.t).zzzzh().zzzz().add16(local.d[skip].f);
#endif
		}

		if (sel.zb)
		{
			VectorF zbase = VectorF::broadcast64(&scan.p.z);
			if (sel.zequal)
			{
#if _M_SSE >= 0x501
				z0 = GSVector8::cast(GSVector8i::broadcast32(zbase.extract<0>().f64toi32()));
#else
				z0 = GSVector4::cast(zbase.f64toi32());
				z0 = z0.upld(z0);
#endif
			}
			else
			{
				z0 = zbase.add64(VectorF::f32to64(&local.d[skip].z.F32[0]));
				z1 = zbase.add64(VectorF::f32to64(&local.d[skip].z.F32[vlen/2]));
			}
		}
	}

	if (sel.fb)
	{
		if (sel.edge)
		{
#if _M_SSE >= 0x501
			cov = GSVector8i::broadcast16(GSVector4i::cast(scan.p)).srl16(9);
#else
			cov = GSVector4i::cast(scan.p).xxxxl().xxxx().srl16(9);
#endif
		}

		if (sel.tfx != TFX_NONE)
		{
			if (sel.fst)
			{
				VectorI vt = VectorI::broadcast128(GSVector4i(scan.t));

				VectorI u = vt.xxxx() + VectorI::cast(local.d[skip].s);
				VectorI v = vt.yyyy();

				if (sel.prim != GS_SPRITE_CLASS || sel.mmin)
				{
					v += VectorI::cast(local.d[skip].t);
				}
				else if (sel.ltf)
				{
					vf = v.xxzzlh().srl16(12);
				}

				s = VectorF::cast(u);
				t = VectorF::cast(v);
			}
			else
			{
#if _M_SSE >= 0x501
				s = GSVector8::broadcast32(&scan.t.x) + local.d[skip].s;
				t = GSVector8::broadcast32(&scan.t.y) + local.d[skip].t;
				q = GSVector8::broadcast32(&scan.t.z) + local.d[skip].q;
#else
				s = scan.t.xxxx() + local.d[skip].s;
				t = scan.t.yyyy() + local.d[skip].t;
				q = scan.t.zzzz() + local.d[skip].q;
#endif
			}
		}

		if (!(sel.tfx == TFX_DECAL && sel.tcc))
		{
			if (sel.iip)
			{
				GSVector4i c(scan.c);

				c = c.upl16(c.zwxy());

#if _M_SSE >= 0x501
				rbf = GSVector8i::broadcast32(&c.x).add16(local.d[skip].rb);
				gaf = GSVector8i::broadcast32(&c.z).add16(local.d[skip].ga);
#else
				rbf = c.xxxx().add16(local.d[skip].rb);
				gaf = c.zzzz().add16(local.d[skip].ga);
#endif
			}
			else
			{
				rbf = local.c.rb;
				gaf = local.c.ga;
			}
		}
	}

	while (1)
	{
		do
		{
			int fa = 0, za = 0;
			VectorI fd, zs, zd;
			VectorI fm, zm;
			VectorI rb, ga;

			// TestZ

			if (sel.zb)
			{
				za = (fza_base->y + fza_offset->y) % HALF_VM_SIZE;

				if (sel.prim != GS_SPRITE_CLASS)
				{
					if (sel.zequal)
					{
						zs = VectorI::cast(z0);
					}
					else if (sel.zoverflow)
					{
						// SSE only has double to int32 conversion, no double to uint32
						// Work around this by subtracting 0x80000000 before converting, then adding it back after
						// Since we've subtracted 0x80000000, truncating now rounds up for numbers less than 0x80000000
						// So approximate the truncation by subtracting an extra (0.5 - ulp) and rounding instead
						GSVector4i zl = z0.add64(VectorF::m_xc1e00000000fffff).f64toi32(false);
						GSVector4i zh = z1.add64(VectorF::m_xc1e00000000fffff).f64toi32(false);
#if _M_SSE >= 0x501
						zs = GSVector8i(zl, zh);
#else
						zs = zl.upl64(zh);
#endif
						zs += VectorI::x80000000();
					}
					else
					{
#if _M_SSE >= 0x501
						zs = GSVector8i(z0.f64toi32(), z1.f64toi32());
#else
						zs = z0.f64toi32().upl64(z1.f64toi32());
#endif
					}

					if (sel.zclamp)
						zs = zs.min_u32(VectorI::xffffffff().srl32(sel.zpsm * 8));
				}
				else
				{
					zs = local.p.z;
				}

				if (sel.ztest)
				{
#if _M_SSE >= 0x501
					zd = GSVector8i::load(
						(u8*)global.vm + za * 2     , (u8*)global.vm + za * 2 + 16,
						(u8*)global.vm + za * 2 + 32, (u8*)global.vm + za * 2 + 48);
#else
					zd = GSVector4i::load((u8*)global.vm + za * 2, (u8*)global.vm + za * 2 + 16);
#endif

					VectorI zso = zs;
					VectorI zdo = zd;

					switch (sel.zpsm)
					{
						case 1: zdo = zdo.sll32( 8).srl32( 8); break;
						case 2: zdo = zdo.sll32(16).srl32(16); break;
						default: break;
					}

					if (sel.zpsm == 0)
					{
						zso -= VectorI::x80000000();
						zdo -= VectorI::x80000000();
					}

					switch (sel.ztst)
					{
						case ZTST_GEQUAL:  test |= zso <  zdo; break;
						case ZTST_GREATER: test |= zso <= zdo; break;
					}

					if (test.alltrue())
						continue;
				}
			}

			// SampleTexture

			if (sel.fb && sel.tfx != TFX_NONE)
			{
				VectorI u, v, uv[2];
				VectorI lodi, lodf;
				VectorI minuv, maxuv;
				VectorI addr00, addr01, addr10, addr11;
				VectorI c00, c01, c10, c11;

				if (sel.mmin)
				{
					if (!sel.fst)
					{
						u = VectorI(s / q);
						v = VectorI(t / q);
					}
					else
					{
						u = VectorI::cast(s);
						v = VectorI::cast(t);
					}

					if (!sel.lcm)
					{
						VectorF tmp = q.log2(3) * global.l + global.k; // (-log2(Q) * (1 << L) + K) * 0x10000

						VectorI lod = VectorI(tmp.sat(VectorF::zero(), global.mxl), false);

						if (sel.mmin == 1) // round-off mode
						{
							lod += 0x8000;
						}

						lodi = lod.srl32(16);

						if (sel.mmin == 2) // trilinear mode
						{
							lodf = lod.xxzzlh();
						}

						// shift u/v by (int)lod

#if _M_SSE >= 0x501
						u = u.srav32(lodi);
						v = v.srav32(lodi);

						uv[0] = u;
						uv[1] = v;

						GSVector8i tmin = GSVector8i::broadcast128(global.t.min);
						GSVector8i tminu = tmin.upl16().srlv32(lodi);
						GSVector8i tminv = tmin.uph16().srlv32(lodi);

						GSVector8i tmax = GSVector8i::broadcast128(global.t.max);
						GSVector8i tmaxu = tmax.upl16().srlv32(lodi);
						GSVector8i tmaxv = tmax.uph16().srlv32(lodi);

						minuv = tminu.pu32(tminv);
						maxuv = tmaxu.pu32(tmaxv);
#else
						GSVector4i aabb = u.upl32(v);
						GSVector4i ccdd = u.uph32(v);

						GSVector4i aaxx = aabb.sra32(lodi.x);
						GSVector4i xxbb = aabb.sra32(lodi.y);
						GSVector4i ccxx = ccdd.sra32(lodi.z);
						GSVector4i xxdd = ccdd.sra32(lodi.w);

						GSVector4i acac = aaxx.upl32(ccxx);
						GSVector4i bdbd = xxbb.uph32(xxdd);

						u = acac.upl32(bdbd);
						v = acac.uph32(bdbd);

						uv[0] = u;
						uv[1] = v;

						GSVector4i minmax = global.t.minmax;

						GSVector4i v0 = minmax.srl16(lodi.x);
						GSVector4i v1 = minmax.srl16(lodi.y);
						GSVector4i v2 = minmax.srl16(lodi.z);
						GSVector4i v3 = minmax.srl16(lodi.w);

						v0 = v0.upl16(v1);
						v2 = v2.upl16(v3);

						minuv = v0.upl32(v2);
						maxuv = v0.uph32(v2);
#endif
					}
					else
					{
						lodi = global.lod.i;

#if _M_SSE >= 0x501
						u = u.srav32(lodi);
						v = v.srav32(lodi);
#else
						u = u.sra32(lodi.x);
						v = v.sra32(lodi.x);
#endif

						uv[0] = u;
						uv[1] = v;

						minuv = local.temp.uv_minmax[0];
						maxuv = local.temp.uv_minmax[1];
					}

					if (sel.ltf)
					{
						u -= 0x8000;
						v -= 0x8000;

						uf = u.xxzzlh().srl16(12);
						vf = v.xxzzlh().srl16(12);
					}

					VectorI uv0 = u.sra32(16).ps32(v.sra32(16));
					VectorI uv1 = uv0;

					{
						VectorI repeat = (uv0 & minuv) | maxuv;
						VectorI clamp = uv0.sat_i16(minuv, maxuv);

						uv0 = clamp.blend8(repeat, VectorI::broadcast128(global.t.mask));
					}

					if (sel.ltf)
					{
						uv1 = uv1.add16(VectorI::x0001());

						VectorI repeat = (uv1 & minuv) | maxuv;
						VectorI clamp = uv1.sat_i16(minuv, maxuv);

						uv1 = clamp.blend8(repeat, VectorI::broadcast128(global.t.mask));
					}

					VectorI y0 = uv0.uph16() << (sel.tw + 3);
					VectorI x0 = uv0.upl16();

					if (sel.ltf)
					{
						VectorI y1 = uv1.uph16() << (sel.tw + 3);
						VectorI x1 = uv1.upl16();

						addr00 = y0 + x0;
						addr01 = y0 + x1;
						addr10 = y1 + x0;
						addr11 = y1 + x1;

						if (sel.tlu)
						{
							for (int i = 0; i < vlen; i++)
							{
								const u8* tex = (const u8*)global.tex[lodi.U32[i]];

								c00.U32[i] = global.clut[tex[addr00.U32[i]]];
								c01.U32[i] = global.clut[tex[addr01.U32[i]]];
								c10.U32[i] = global.clut[tex[addr10.U32[i]]];
								c11.U32[i] = global.clut[tex[addr11.U32[i]]];
							}
						}
						else
						{
							for (int i = 0; i < vlen; i++)
							{
								const u32* tex = (const u32*)global.tex[lodi.U32[i]];

								c00.U32[i] = tex[addr00.U32[i]];
								c01.U32[i] = tex[addr01.U32[i]];
								c10.U32[i] = tex[addr10.U32[i]];
								c11.U32[i] = tex[addr11.U32[i]];
							}
						}

						VectorI rb00 = c00.sll16(8).srl16(8);
						VectorI ga00 = c00.srl16(8);
						VectorI rb01 = c01.sll16(8).srl16(8);
						VectorI ga01 = c01.srl16(8);

						rb00 = rb00.lerp16_4(rb01, uf);
						ga00 = ga00.lerp16_4(ga01, uf);

						VectorI rb10 = c10.sll16(8).srl16(8);
						VectorI ga10 = c10.srl16(8);
						VectorI rb11 = c11.sll16(8).srl16(8);
						VectorI ga11 = c11.srl16(8);

						rb10 = rb10.lerp16_4(rb11, uf);
						ga10 = ga10.lerp16_4(ga11, uf);

						rb = rb00.lerp16_4(rb10, vf);
						ga = ga00.lerp16_4(ga10, vf);
					}
					else
					{
						addr00 = y0 + x0;

						if (sel.tlu)
						{
							for (int i = 0; i < vlen; i++)
							{
								c00.U32[i] = global.clut[((const u8*)global.tex[lodi.U32[i]])[addr00.U32[i]]];
							}
						}
						else
						{
							for (int i = 0; i < vlen; i++)
							{
								c00.U32[i] = ((const u32*)global.tex[lodi.U32[i]])[addr00.U32[i]];
							}
						}

						rb = c00.sll16(8).srl16(8);
						ga = c00.srl16(8);
					}

					if (sel.mmin != 1) // !round-off mode
					{
						VectorI rb2, ga2;

						lodi += VectorI::x00000001();

						u = uv[0].sra32(1);
						v = uv[1].sra32(1);

						minuv = minuv.srl16(1);
						maxuv = maxuv.srl16(1);

						if (sel.ltf)
						{
							u -= 0x8000;
							v -= 0x8000;

							uf = u.xxzzlh().srl16(12);
							vf = v.xxzzlh().srl16(12);
						}

						VectorI uv0 = u.sra32(16).ps32(v.sra32(16));
						VectorI uv1 = uv0;

						{
							VectorI repeat = (uv0 & minuv) | maxuv;
							VectorI clamp = uv0.sat_i16(minuv, maxuv);

							uv0 = clamp.blend8(repeat, VectorI::broadcast128(global.t.mask));
						}

						if (sel.ltf)
						{
							uv1 = uv1.add16(VectorI::x0001());

							VectorI repeat = (uv1 & minuv) | maxuv;
							VectorI clamp = uv1.sat_i16(minuv, maxuv);

							uv1 = clamp.blend8(repeat, VectorI::broadcast128(global.t.mask));
						}

						VectorI y0 = uv0.uph16() << (sel.tw + 3);
						VectorI x0 = uv0.upl16();

						if (sel.ltf)
						{
							VectorI y1 = uv1.uph16() << (sel.tw + 3);
							VectorI x1 = uv1.upl16();

							addr00 = y0 + x0;
							addr01 = y0 + x1;
							addr10 = y1 + x0;
							addr11 = y1 + x1;

							if (sel.tlu)
							{
								for (int i = 0; i < vlen; i++)
								{
									const u8* tex = (const u8*)global.tex[lodi.U32[i]];

									c00.U32[i] = global.clut[tex[addr00.U32[i]]];
									c01.U32[i] = global.clut[tex[addr01.U32[i]]];
									c10.U32[i] = global.clut[tex[addr10.U32[i]]];
									c11.U32[i] = global.clut[tex[addr11.U32[i]]];
								}
							}
							else
							{
								for (int i = 0; i < vlen; i++)
								{
									const u32* tex = (const u32*)global.tex[lodi.U32[i]];

									c00.U32[i] = tex[addr00.U32[i]];
									c01.U32[i] = tex[addr01.U32[i]];
									c10.U32[i] = tex[addr10.U32[i]];
									c11.U32[i] = tex[addr11.U32[i]];
								}
							}

							VectorI rb00 = c00.sll16(8).srl16(8);
							VectorI ga00 = c00.srl16(8);
							VectorI rb01 = c01.sll16(8).srl16(8);
							VectorI ga01 = c01.srl16(8);

							rb00 = rb00.lerp16_4(rb01, uf);
							ga00 = ga00.lerp16_4(ga01, uf);

							VectorI rb10 = c10.sll16(8).srl16(8);
							VectorI ga10 = c10.srl16(8);
							VectorI rb11 = c11.sll16(8).srl16(8);
							VectorI ga11 = c11.srl16(8);

							rb10 = rb10.lerp16_4(rb11, uf);
							ga10 = ga10.lerp16_4(ga11, uf);

							rb2 = rb00.lerp16_4(rb10, vf);
							ga2 = ga00.lerp16_4(ga10, vf);
						}
						else
						{
							addr00 = y0 + x0;

							if (sel.tlu)
							{
								for (int i = 0; i < vlen; i++)
								{
									c00.U32[i] = global.clut[((const u8*)global.tex[lodi.U32[i]])[addr00.U32[i]]];
								}
							}
							else
							{
								for (int i = 0; i < vlen; i++)
								{
									c00.U32[i] = ((const u32*)global.tex[lodi.U32[i]])[addr00.U32[i]];
								}
							}

							rb2 = c00.sll16(8).srl16(8);
							ga2 = c00.srl16(8);
						}

						if (sel.lcm)
							lodf = global.lod.f;

						lodf = lodf.srl16(1);

						rb = rb.lerp16<0>(rb2, lodf);
						ga = ga.lerp16<0>(ga2, lodf);
					}
				}
				else
				{
					if (!sel.fst)
					{
						u = VectorI(s / q);
						v = VectorI(t / q);

						if (sel.ltf)
						{
							u -= 0x8000;
							v -= 0x8000;
						}
					}
					else
					{
						u = VectorI::cast(s);
						v = VectorI::cast(t);
					}

					if (sel.ltf)
					{
						uf = u.xxzzlh().srl16(12);

						if (sel.prim != GS_SPRITE_CLASS)
						{
							vf = v.xxzzlh().srl16(12);
						}
					}

					VectorI uv0 = u.sra32(16).ps32(v.sra32(16));
					VectorI uv1 = uv0;

					VectorI tmin = VectorI::broadcast128(global.t.min);
					VectorI tmax = VectorI::broadcast128(global.t.max);

					{
						VectorI repeat = (uv0 & tmin) | tmax;
						VectorI clamp = uv0.sat_i16(tmin, tmax);

						uv0 = clamp.blend8(repeat, VectorI::broadcast128(global.t.mask));
					}

					if (sel.ltf)
					{
						uv1 = uv1.add16(VectorI::x0001());

						VectorI repeat = (uv1 & tmin) | tmax;
						VectorI clamp = uv1.sat_i16(tmin, tmax);

						uv1 = clamp.blend8(repeat, VectorI::broadcast128(global.t.mask));
					}

					VectorI y0 = uv0.uph16() << (sel.tw + 3);
					VectorI x0 = uv0.upl16();

					if (sel.ltf)
					{
						VectorI y1 = uv1.uph16() << (sel.tw + 3);
						VectorI x1 = uv1.upl16();

						addr00 = y0 + x0;
						addr01 = y0 + x1;
						addr10 = y1 + x0;
						addr11 = y1 + x1;

						if (sel.tlu)
						{
							const u8* tex = (const u8*)global.tex[0];

							c00 = addr00.gather32_32(tex, global.clut);
							c01 = addr01.gather32_32(tex, global.clut);
							c10 = addr10.gather32_32(tex, global.clut);
							c11 = addr11.gather32_32(tex, global.clut);
						}
						else
						{
							const u32* tex = (const u32*)global.tex[0];

							c00 = addr00.gather32_32(tex);
							c01 = addr01.gather32_32(tex);
							c10 = addr10.gather32_32(tex);
							c11 = addr11.gather32_32(tex);
						}

						VectorI rb00 = c00.sll16(8).srl16(8);
						VectorI ga00 = c00.srl16(8);
						VectorI rb01 = c01.sll16(8).srl16(8);
						VectorI ga01 = c01.srl16(8);

						rb00 = rb00.lerp16_4(rb01, uf);
						ga00 = ga00.lerp16_4(ga01, uf);

						VectorI rb10 = c10.sll16(8).srl16(8);
						VectorI ga10 = c10.srl16(8);
						VectorI rb11 = c11.sll16(8).srl16(8);
						VectorI ga11 = c11.srl16(8);

						rb10 = rb10.lerp16_4(rb11, uf);
						ga10 = ga10.lerp16_4(ga11, uf);

						rb = rb00.lerp16_4(rb10, vf);
						ga = ga00.lerp16_4(ga10, vf);
					}
					else
					{
						addr00 = y0 + x0;

						if (sel.tlu)
						{
							c00 = addr00.gather32_32((const u8*)global.tex[0], global.clut);
						}
						else
						{
							c00 = addr00.gather32_32((const u32*)global.tex[0]);
						}

						rb = c00.sll16(8).srl16(8);
						ga = c00.srl16(8);
					}
				}
			}

			// AlphaTFX

			if (sel.fb)
			{
				switch (sel.tfx)
				{
					case TFX_MODULATE:
						ga = ga.modulate16<1>(gaf).clamp8();
						if (!sel.tcc)
							ga = ga.mix16(gaf.srl16(7));
						break;
					case TFX_DECAL:
						if (!sel.tcc)
							ga = ga.mix16(gaf.srl16(7));
						break;
					case TFX_HIGHLIGHT:
						ga = ga.mix16(!sel.tcc ? gaf.srl16(7) : ga.addus8(gaf.srl16(7)));
						break;
					case TFX_HIGHLIGHT2:
						if (!sel.tcc)
							ga = ga.mix16(gaf.srl16(7));
						break;
					case TFX_NONE:
						ga = sel.iip ? gaf.srl16(7) : gaf;
						break;
				}

				if (sel.aa1)
				{
					VectorI x00800080(0x00800080);

					VectorI a = sel.edge ? cov : x00800080;

					if (!sel.abe)
					{
						ga = ga.mix16(a);
					}
					else
					{
						ga = ga.blend8(a, ga.eq16(x00800080).srl32(16).sll32(16));
					}
				}
			}

			// ReadMask

			if (sel.fwrite)
			{
				fm = global.fm;
			}

			if (sel.zwrite)
			{
				zm = global.zm;
			}

			// TestAlpha

			if (!TestAlpha(test, fm, zm, ga, global))
				continue;

			// ColorTFX

			if (sel.fwrite)
			{
				VectorI af;

				switch (sel.tfx)
				{
					case TFX_MODULATE:
						rb = rb.modulate16<1>(rbf).clamp8();
						break;
					case TFX_DECAL:
						break;
					case TFX_HIGHLIGHT:
					case TFX_HIGHLIGHT2:
						af = gaf.yywwlh().srl16(7);
						rb = rb.modulate16<1>(rbf).add16(af).clamp8();
						ga = ga.modulate16<1>(gaf).add16(af).clamp8().mix16(ga);
						break;
					case TFX_NONE:
						rb = sel.iip ? rbf.srl16(7) : rbf;
						break;
				}
			}

			// Fog

			if (sel.fwrite && sel.fge)
			{
#if _M_SSE >= 0x501
				GSVector8i fog = sel.prim != GS_SPRITE_CLASS ? f : GSVector8i::broadcast16(&local.p.f);

				GSVector8i frb((int)global.frb);
				GSVector8i fga((int)global.fga);
#else
				GSVector4i fog = sel.prim != GS_SPRITE_CLASS ? f : local.p.f;

				GSVector4i frb = global.frb;
				GSVector4i fga = global.fga;
#endif

				rb = frb.lerp16<0>(rb, fog);
				ga = fga.lerp16<0>(ga, fog).mix16(ga);

				/*
				fog = fog.srl16(7);

				VectorI ifog = VectorI::x00ff().sub16(fog);

				rb = rb.mul16l(fog).add16(frb.mul16l(ifog)).srl16(8);
				ga = ga.mul16l(fog).add16(fga.mul16l(ifog)).srl16(8).mix16(ga);
				*/
			}

			// ReadFrame

			if (sel.fb)
			{
				fa = (fza_base->x + fza_offset->x) % HALF_VM_SIZE;

				if (sel.rfb)
				{
#if _M_SSE >= 0x501
					fd = GSVector8i::load(
						(u8*)global.vm + fa * 2     , (u8*)global.vm + fa * 2 + 16,
						(u8*)global.vm + fa * 2 + 32, (u8*)global.vm + fa * 2 + 48);
#else
					fd = GSVector4i::load((u8*)global.vm + fa * 2, (u8*)global.vm + fa * 2 + 16);
#endif
				}
			}

			// TestDestAlpha

			if (sel.date && (sel.fpsm == 0 || sel.fpsm == 2))
			{
				if (sel.datm)
				{
					if (sel.fpsm == 2)
					{
						// test |= fd.srl32(15) == VectorI::zero();
						test |= fd.sll32(16).sra32(31) == VectorI::zero();
					}
					else
					{
						test |= (~fd).sra32(31);
					}
				}
				else
				{
					if (sel.fpsm == 2)
					{
						test |= fd.sll32(16).sra32(31); // == VectorI::xffffffff();
					}
					else
					{
						test |= fd.sra32(31);
					}
				}

				if (test.alltrue())
					continue;
			}

			// WriteMask

			int fzm = 0;

			if (!sel.notest)
			{
				if (sel.fwrite)
				{
					fm |= test;
				}

				if (sel.zwrite)
				{
					zm |= test;
				}

				if (sel.fwrite && sel.zwrite)
				{
					fzm = ~(fm == VectorI::xffffffff()).ps32(zm == VectorI::xffffffff()).mask();
				}
				else if (sel.fwrite)
				{
					fzm = ~(fm == VectorI::xffffffff()).ps32().mask();
				}
				else if (sel.zwrite)
				{
					fzm = ~(zm == VectorI::xffffffff()).ps32().mask();
				}
			}

			// WriteZBuf

			if (sel.zwrite)
			{
				if (sel.ztest && sel.zpsm < 2)
				{
					zs = zs.blend8(zd, zm);
				}

				bool fast = sel.ztest ? sel.zpsm < 2 : sel.zpsm == 0 && sel.notest;

				if (sel.notest)
				{
					if (fast)
					{
#if _M_SSE >= 0x501
						GSVector4i::storel((u8*)global.vm + za * 2     , zs.extract<0>());
						GSVector4i::storeh((u8*)global.vm + za * 2 + 16, zs.extract<0>());
						GSVector4i::storel((u8*)global.vm + za * 2 + 32, zs.extract<1>());
						GSVector4i::storeh((u8*)global.vm + za * 2 + 48, zs.extract<1>());
#else
						GSVector4i::storel((u8*)global.vm + za * 2     , zs);
						GSVector4i::storeh((u8*)global.vm + za * 2 + 16, zs);
#endif
					}
					else
					{
						WritePixel(zs, za, 0, sel.zpsm, global);
						WritePixel(zs, za, 1, sel.zpsm, global);
						WritePixel(zs, za, 2, sel.zpsm, global);
						WritePixel(zs, za, 3, sel.zpsm, global);
#if _M_SSE >= 0x501
						WritePixel(zs, za, 4, sel.zpsm, global);
						WritePixel(zs, za, 5, sel.zpsm, global);
						WritePixel(zs, za, 6, sel.zpsm, global);
						WritePixel(zs, za, 7, sel.zpsm, global);
#endif
					}
				}
				else
				{
					if (fast)
					{
#if _M_SSE >= 0x501
						if (fzm & 0x00000f00) GSVector4i::storel((u8*)global.vm + za * 2     , zs.extract<0>());
						if (fzm & 0x0000f000) GSVector4i::storeh((u8*)global.vm + za * 2 + 16, zs.extract<0>());
						if (fzm & 0x0f000000) GSVector4i::storel((u8*)global.vm + za * 2 + 32, zs.extract<1>());
						if (fzm & 0xf0000000) GSVector4i::storeh((u8*)global.vm + za * 2 + 48, zs.extract<1>());
#else
						if (fzm & 0x0f00) GSVector4i::storel((u8*)global.vm + za * 2     , zs);
						if (fzm & 0xf000) GSVector4i::storeh((u8*)global.vm + za * 2 + 16, zs);
#endif
					}
					else
					{
						if (fzm & 0x00000300) WritePixel(zs, za, 0, sel.zpsm, global);
						if (fzm & 0x00000c00) WritePixel(zs, za, 1, sel.zpsm, global);
						if (fzm & 0x00003000) WritePixel(zs, za, 2, sel.zpsm, global);
						if (fzm & 0x0000c000) WritePixel(zs, za, 3, sel.zpsm, global);
#if _M_SSE >= 0x501
						if (fzm & 0x03000000) WritePixel(zs, za, 4, sel.zpsm, global);
						if (fzm & 0x0c000000) WritePixel(zs, za, 5, sel.zpsm, global);
						if (fzm & 0x30000000) WritePixel(zs, za, 6, sel.zpsm, global);
						if (fzm & 0xc0000000) WritePixel(zs, za, 7, sel.zpsm, global);
#endif
					}
				}
			}

			// AlphaBlend

			if (sel.fwrite && (sel.abe || sel.aa1))
			{
				VectorI rbs = rb, gas = ga, rbd, gad, a, mask;

				if (sel.aba != sel.abb && (sel.aba == 1 || sel.abb == 1 || sel.abc == 1) || sel.abd == 1)
				{
					switch (sel.fpsm)
					{
						case 0:
						case 1:
							rbd = fd.sll16(8).srl16(8);
							gad = fd.srl16(8);
							break;
						case 2:
							rbd = ((fd & 0x7c00) << 9) | ((fd & 0x001f) << 3);
							gad = ((fd & 0x8000) << 8) | ((fd & 0x03e0) >> 2);
							break;
					}
				}

				if (sel.aba != sel.abb)
				{
					switch(sel.aba)
					{
						case 0: break;
						case 1: rb = rbd; break;
						case 2: rb = VectorI::zero(); break;
					}

					switch(sel.abb)
					{
						case 0: rb = rb.sub16(rbs); break;
						case 1: rb = rb.sub16(rbd); break;
						case 2: break;
					}

					if(!(sel.fpsm == 1 && sel.abc == 1))
					{
						switch(sel.abc)
						{
							case 0: a = gas.yywwlh().sll16(7); break;
							case 1: a = gad.yywwlh().sll16(7); break;
							case 2: a = global.afix; break;
						}

						rb = rb.modulate16<1>(a);
					}

					switch (sel.abd)
					{
						case 0: rb = rb.add16(rbs); break;
						case 1: rb = rb.add16(rbd); break;
						case 2: break;
					}
				}
				else
				{
					switch (sel.abd)
					{
						case 0: break;
						case 1: rb = rbd; break;
						case 2: rb = VectorI::zero(); break;
					}
				}

				if (sel.pabe)
				{
					mask = (gas << 8).sra32(31);

					rb = rbs.blend8(rb, mask);
				}

				if (sel.aba != sel.abb)
				{
					switch(sel.aba)
					{
						case 0: break;
						case 1: ga = gad; break;
						case 2: ga = VectorI::zero(); break;
					}

					switch(sel.abb)
					{
						case 0: ga = ga.sub16(gas); break;
						case 1: ga = ga.sub16(gad); break;
						case 2: break;
					}

					if (!(sel.fpsm == 1 && sel.abc == 1))
					{
						ga = ga.modulate16<1>(a);
					}

					switch (sel.abd)
					{
						case 0: ga = ga.add16(gas); break;
						case 1: ga = ga.add16(gad); break;
						case 2: break;
					}
				}
				else
				{
					switch (sel.abd)
					{
						case 0: break;
						case 1: ga = gad; break;
						case 2: ga = VectorI::zero(); break;
					}
				}

				if (sel.pabe)
				{
					ga = gas.blend8(ga, mask >> 16);
				}
				else
				{
					if (sel.fpsm != 1)
					{
						ga = ga.mix16(gas);
					}
				}
			}

			// WriteFrame

			if (sel.fwrite)
			{
				if (sel.fpsm == 2 && sel.dthe)
				{
					int y = (top & 3) << 1;

					rb = rb.add16(VectorI::broadcast128(global.dimx[0 + y]));
					ga = ga.add16(VectorI::broadcast128(global.dimx[1 + y]));
				}

				if (sel.colclamp == 0)
				{
					rb &= VectorI::x00ff();
					ga &= VectorI::x00ff();
				}

				VectorI fs = rb.upl16(ga).pu16(rb.uph16(ga));

				if (sel.fba && sel.fpsm != 1)
				{
					fs |= VectorI::x80000000();
				}

				if (sel.fpsm == 2)
				{
					VectorI rb = fs & 0x00f800f8;
					VectorI ga = fs & 0x8000f800;

					fs = (ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3);
				}

				if (sel.rfb)
				{
					fs = fs.blend(fd, fm);
				}

				bool fast = sel.rfb ? sel.fpsm < 2 : sel.fpsm == 0 && sel.notest;

				if (sel.notest)
				{
					if (fast)
					{
#if _M_SSE >= 0x501
						GSVector4i::storel((u8*)global.vm + fa * 2     , fs.extract<0>());
						GSVector4i::storeh((u8*)global.vm + fa * 2 + 16, fs.extract<0>());
						GSVector4i::storel((u8*)global.vm + fa * 2 + 32, fs.extract<1>());
						GSVector4i::storeh((u8*)global.vm + fa * 2 + 48, fs.extract<1>());
#else
						GSVector4i::storel((u8*)global.vm + fa * 2     , fs);
						GSVector4i::storeh((u8*)global.vm + fa * 2 + 16, fs);
#endif
					}
					else
					{
						WritePixel(fs, fa, 0, sel.fpsm, global);
						WritePixel(fs, fa, 1, sel.fpsm, global);
						WritePixel(fs, fa, 2, sel.fpsm, global);
						WritePixel(fs, fa, 3, sel.fpsm, global);
#if _M_SSE >= 0x501
						WritePixel(fs, fa, 4, sel.fpsm, global);
						WritePixel(fs, fa, 5, sel.fpsm, global);
						WritePixel(fs, fa, 6, sel.fpsm, global);
						WritePixel(fs, fa, 7, sel.fpsm, global);
#endif
					}
				}
				else
				{
					if (fast)
					{
#if _M_SSE >= 0x501
						if (fzm & 0x0000000f) GSVector4i::storel((u8*)global.vm + fa * 2     , fs.extract<0>());
						if (fzm & 0x000000f0) GSVector4i::storeh((u8*)global.vm + fa * 2 + 16, fs.extract<0>());
						if (fzm & 0x000f0000) GSVector4i::storel((u8*)global.vm + fa * 2 + 32, fs.extract<1>());
						if (fzm & 0x00f00000) GSVector4i::storeh((u8*)global.vm + fa * 2 + 48, fs.extract<1>());
#else
						if (fzm & 0x000f) GSVector4i::storel((u8*)global.vm + fa * 2     , fs);
						if (fzm & 0x00f0) GSVector4i::storeh((u8*)global.vm + fa * 2 + 16, fs);
#endif
					}
					else
					{
						if (fzm & 0x00000003) WritePixel(fs, fa, 0, sel.fpsm, global);
						if (fzm & 0x0000000c) WritePixel(fs, fa, 1, sel.fpsm, global);
						if (fzm & 0x00000030) WritePixel(fs, fa, 2, sel.fpsm, global);
						if (fzm & 0x000000c0) WritePixel(fs, fa, 3, sel.fpsm, global);
#if _M_SSE >= 0x501
						if (fzm & 0x00030000) WritePixel(fs, fa, 4, sel.fpsm, global);
						if (fzm & 0x000c0000) WritePixel(fs, fa, 5, sel.fpsm, global);
						if (fzm & 0x00300000) WritePixel(fs, fa, 6, sel.fpsm, global);
						if (fzm & 0x00c00000) WritePixel(fs, fa, 7, sel.fpsm, global);
#endif
					}
				}
			}
		} while (0);

		if (sel.edge)
			break;

		if (steps <= 0)
			break;

		// Step

		steps -= vlen;

		fza_offset += vlen / 4;

		if (sel.prim != GS_SPRITE_CLASS)
		{
			if (sel.zb && !sel.zequal)
			{
#if _M_SSE >= 0x501
				GSVector8 add = GSVector8::broadcast64(&local.d8.p.z);
#else
				GSVector4 add = local.d4.z;
#endif
				z0 = z0.add64(add);
				z1 = z1.add64(add);
			}

			if (sel.fwrite && sel.fge)
			{
#if _M_SSE >= 0x501
				f = f.add16(GSVector8i::broadcast16(&local.d8.p.f));
#else
				f = f.add16(local.d4.f);
#endif
			}
		}

		if (sel.fb)
		{
			if (sel.tfx != TFX_NONE)
			{
				if (sel.fst)
				{
					VectorI stq = VectorI::cast(VectorF(LOCAL_STEP.stq));

					s = VectorF::cast(VectorI::cast(s) + stq.xxxx());

					if (sel.prim != GS_SPRITE_CLASS || sel.mmin)
					{
						t = VectorF::cast(VectorI::cast(t) + stq.yyyy());
					}
				}
				else
				{
					VectorF stq(LOCAL_STEP.stq);

					s += stq.xxxx();
					t += stq.yyyy();
					q += stq.zzzz();
				}
			}
		}

		if (!(sel.tfx == TFX_DECAL && sel.tcc))
		{
			if (sel.iip)
			{
#if _M_SSE >= 0x501
				GSVector8i c = GSVector8i::broadcast64(&local.d8.c);
#else
				GSVector4i c = local.d4.c;
#endif
				rbf = rbf.add16(c.xxxx()).max_i16(VectorI::zero());
				gaf = gaf.add16(c.yyyy()).max_i16(VectorI::zero());
			}
		}

		if (!sel.notest)
		{
#if _M_SSE >= 0x501
			test = GSVector8i::i8to32(g_const->m_test_256b[15 + (steps & (steps >> 31))]);
#else
			test = const_test[7 + (steps & (steps >> 31))];
#endif
		}
	}
}

#ifndef ENABLE_JIT_RASTERIZER
void GSDrawScanline::SetupPrim(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan)
{
	CSetupPrim(vertex, index, dscan, m_local, m_global);
}
void GSDrawScanline::DrawScanline(int pixels, int left, int top, const GSVertexSW& scan)
{
	CDrawScanline(pixels, left, top, scan, m_local, m_global);
}
void GSDrawScanline::DrawEdge(int pixels, int left, int top, const GSVertexSW& scan)
{
	u32 zwrite = m_global.sel.zwrite;
	u32 edge = m_global.sel.edge;

	m_global.sel.zwrite = 0;
	m_global.sel.edge = 1;

	CDrawScanline(pixels, left, top, scan, m_local, m_global);

	m_global.sel.zwrite = zwrite;
	m_global.sel.edge = edge;
}
#endif

template <class T>
bool GSDrawScanline::TestAlpha(T& test, T& fm, T& zm, const T& ga, const GSScanlineGlobalData& global)
{
	GSScanlineSelector sel = global.sel;

	switch (sel.afail)
	{
		case AFAIL_FB_ONLY:
			if (!sel.zwrite)
				return true;
			break;

		case AFAIL_ZB_ONLY:
			if (!sel.fwrite)
				return true;
			break;

		case AFAIL_RGB_ONLY:
			if (!sel.zwrite && sel.fpsm == 1)
				return true;
			break;
	}

	T t;

	switch (sel.atst)
	{
		case ATST_NEVER:
			t = GSVector4i::xffffffff();
			break;

		case ATST_ALWAYS:
			return true;

		case ATST_LESS:
		case ATST_LEQUAL:
			t = (ga >> 16) > T(global.aref);
			break;

		case ATST_EQUAL:
			t = (ga >> 16) != T(global.aref);
			break;

		case ATST_GEQUAL:
		case ATST_GREATER:
			t = (ga >> 16) < T(global.aref);
			break;

		case ATST_NOTEQUAL:
			t = (ga >> 16) == T(global.aref);
			break;

		default:
			__assume(0);
	}

	switch (sel.afail)
	{
		case AFAIL_KEEP:
			test |= t;
			if (test.alltrue())
				return false;
			break;

		case AFAIL_FB_ONLY:
			zm |= t;
			break;

		case AFAIL_ZB_ONLY:
			fm |= t;
			break;

		case AFAIL_RGB_ONLY:
			zm |= t;
			fm |= t & T::xff000000(); // fpsm 16 bit => & 0xffff8000?
			break;

		default:
			__assume(0);
	}

	return true;
}

static const int s_offsets[] = {0, 2, 8, 10, 16, 18, 24, 26}; // columnTable16[0]

template <class T>
void GSDrawScanline::WritePixel(const T& src, int addr, int i, u32 psm, const GSScanlineGlobalData& global)
{
	u8* dst = (u8*)global.vm + addr * 2 + s_offsets[i] * 2;

	switch (psm)
	{
		case 0:
			*(u32*)dst = src.U32[i];
			break;
		case 1:
			*(u32*)dst = (src.U32[i] & 0xffffff) | (*(u32*)dst & 0xff000000);
			break;
		case 2:
			*(u16*)dst = src.U16[i * 2];
			break;
	}
}

void GSDrawScanline::DrawRect(const GSVector4i& r, const GSVertexSW& v)
{
	ASSERT(r.y >= 0);
	ASSERT(r.w >= 0);

	// FIXME: sometimes the frame and z buffer may overlap, the outcome is undefined

	u32 m;

#if _M_SSE >= 0x501
	m = m_global.zm;
#else
	m = m_global.zm.U32[0];
#endif

	if (m != 0xffffffff)
	{
		u32 z = v.t.U32[3]; // (u32)v.p.z;

		if (m_global.sel.zpsm != 2)
		{
			if (m == 0)
			{
				DrawRectT<u32, false>(m_global.zbo, r, z, m);
			}
			else
			{
				DrawRectT<u32, true>(m_global.zbo, r, z, m);
			}
		}
		else
		{
			if ((m & 0xffff) == 0)
			{
				DrawRectT<u16, false>(m_global.zbo, r, z, m);
			}
			else
			{
				DrawRectT<u16, true>(m_global.zbo, r, z, m);
			}
		}
	}

#if _M_SSE >= 0x501
	m = m_global.fm;
#else
	m = m_global.fm.U32[0];
#endif

	if (m != 0xffffffff)
	{
		u32 c = (GSVector4i(v.c) >> 7).rgba32();

		if (m_global.sel.fba)
		{
			c |= 0x80000000;
		}

		if (m_global.sel.fpsm != 2)
		{
			if (m == 0)
			{
				DrawRectT<u32, false>(m_global.fbo, r, c, m);
			}
			else
			{
				DrawRectT<u32, true>(m_global.fbo, r, c, m);
			}
		}
		else
		{
			c = ((c & 0xf8) >> 3) | ((c & 0xf800) >> 6) | ((c & 0xf80000) >> 9) | ((c & 0x80000000) >> 16);

			if ((m & 0xffff) == 0)
			{
				DrawRectT<u16, false>(m_global.fbo, r, c, m);
			}
			else
			{
				DrawRectT<u16, true>(m_global.fbo, r, c, m);
			}
		}
	}
}

template <class T, bool masked>
void GSDrawScanline::DrawRectT(const GSOffset& off, const GSVector4i& r, u32 c, u32 m)
{
	if (m == 0xffffffff)
		return;

#if _M_SSE >= 0x501

	GSVector8i color((int)c);
	GSVector8i mask((int)m);

#else

	GSVector4i color((int)c);
	GSVector4i mask((int)m);

#endif

	if (sizeof(T) == sizeof(u16))
	{
		color = color.xxzzlh();
		mask = mask.xxzzlh();
		c = (c & 0xffff) | (c << 16);
		m = (m & 0xffff) | (m << 16);
	}

	color = color.andnot(mask);
	c = c & (~m);

	if (masked)
		ASSERT(mask.U32[0] != 0);

	GSVector4i br = r.ralign<Align_Inside>(GSVector2i(8 * 4 / sizeof(T), 8));

	if (!br.rempty())
	{
		FillRect<T, masked>(off, GSVector4i(r.x, r.y, r.z, br.y), c, m);
		FillRect<T, masked>(off, GSVector4i(r.x, br.w, r.z, r.w), c, m);

		if (r.x < br.x || br.z < r.z)
		{
			FillRect<T, masked>(off, GSVector4i(r.x, br.y, br.x, br.w), c, m);
			FillRect<T, masked>(off, GSVector4i(br.z, br.y, r.z, br.w), c, m);
		}

		FillBlock<T, masked>(off, br, color, mask);
	}
	else
	{
		FillRect<T, masked>(off, r, c, m);
	}
}

template <class T, bool masked>
void GSDrawScanline::FillRect(const GSOffset& off, const GSVector4i& r, u32 c, u32 m)
{
	if (r.x >= r.z)
		return;

	T* vm = (T*)m_global.vm;

	for (int y = r.y; y < r.w; y++)
	{
		auto pa = off.paMulti(vm, 0, y);

		for (int x = r.x; x < r.z; x++)
		{
			T& d = *pa.value(x);
			d = (T)(!masked ? c : (c | (d & m)));
		}
	}
}

#if _M_SSE >= 0x501

template <class T, bool masked>
void GSDrawScanline::FillBlock(const GSOffset& off, const GSVector4i& r, const GSVector8i& c, const GSVector8i& m)
{
	if (r.x >= r.z)
		return;

	T* vm = (T*)m_global.vm;

	for (int y = r.y; y < r.w; y += 8)
	{
		for (int x = r.x; x < r.z; x += 8 * 4 / sizeof(T))
		{
			GSVector8i* RESTRICT p = (GSVector8i*)&vm[off.pa(x, y)];

			p[0] = !masked ? c : (c | (p[0] & m));
			p[1] = !masked ? c : (c | (p[1] & m));
			p[2] = !masked ? c : (c | (p[2] & m));
			p[3] = !masked ? c : (c | (p[3] & m));
			p[4] = !masked ? c : (c | (p[4] & m));
			p[5] = !masked ? c : (c | (p[5] & m));
			p[6] = !masked ? c : (c | (p[6] & m));
			p[7] = !masked ? c : (c | (p[7] & m));
		}
	}
}

#else

template <class T, bool masked>
void GSDrawScanline::FillBlock(const GSOffset& off, const GSVector4i& r, const GSVector4i& c, const GSVector4i& m)
{
	if (r.x >= r.z)
		return;

	T* vm = (T*)m_global.vm;

	for (int y = r.y; y < r.w; y += 8)
	{
		auto pa = off.paMulti(vm, 0, y);

		for (int x = r.x; x < r.z; x += 8 * 4 / sizeof(T))
		{
			GSVector4i* RESTRICT p = (GSVector4i*)pa.value(x);

			for (int i = 0; i < 16; i += 4)
			{
				p[i + 0] = !masked ? c : (c | (p[i + 0] & m));
				p[i + 1] = !masked ? c : (c | (p[i + 1] & m));
				p[i + 2] = !masked ? c : (c | (p[i + 2] & m));
				p[i + 3] = !masked ? c : (c | (p[i + 3] & m));
			}
		}
	}
}

#endif
