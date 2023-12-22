// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSRendererHW.h"

#include "GS/Renderers/SW/GSTextureCacheSW.h"
#include "GS/Renderers/SW/GSRasterizer.h"

class CURRENT_ISA::GSRendererHWFunctions
{
public:
	static bool SwPrimRender(GSRendererHW& hw, bool invalidate_tc, bool add_ee_transfer);

	static void Populate(GSRendererHW& renderer)
	{
		renderer.SwPrimRender = SwPrimRender;
	}
};

MULTI_ISA_UNSHARED_IMPL;

void CURRENT_ISA::GSRendererHWPopulateFunctions(GSRendererHW& renderer)
{
	GSRendererHWFunctions::Populate(renderer);
}

// since there's no overlapping draws, we can just keep this intact
static GSVector4i s_dimx_storage[8];
static GIFRegDIMX s_last_dimx;

bool GSRendererHWFunctions::SwPrimRender(GSRendererHW& hw, bool invalidate_tc, bool add_ee_transfer)
{
	GSVertexTrace& vt = hw.m_vt;
	const GIFRegPRIM* PRIM = hw.PRIM;
	const GSDrawingContext* context = hw.m_context;
	const GSDrawingEnvironment& env = *hw.m_draw_env;
	const GS_PRIM_CLASS primclass = vt.m_primclass;

	GSRasterizerData data;
	GSScanlineGlobalData& gd = data.global;

	hw.m_sw_vertex_buffer.resize(((hw.m_vertex.next + 1) & ~1));

	data.primclass = vt.m_primclass;
	data.buff = nullptr;
	data.vertex = hw.m_sw_vertex_buffer.data();
	data.vertex_count = hw.m_vertex.next;
	data.index = hw.m_index.buff;
	data.index_count = hw.m_index.tail;
	data.scanmsk_value = env.SCANMSK.MSK;

	// Skip per pixel division if q is constant.
	// Optimize the division by 1 with a nop. It also means that GS_SPRITE_CLASS must be processed when !vt.m_eq.q.
	// If you have both GS_SPRITE_CLASS && vt.m_eq.q, it will depends on the first part of the 'OR'.
	const u32 q_div = !hw.IsMipMapActive() && ((vt.m_eq.q && vt.m_min.t.z != 1.0f) || (!vt.m_eq.q && vt.m_primclass == GS_SPRITE_CLASS));
	GSVertexSW::s_cvb[vt.m_primclass][PRIM->TME][PRIM->FST][q_div](context, data.vertex, hw.m_vertex.buff, hw.m_vertex.next);

	GSVector4i scissor = context->scissor.in;
	GSVector4i bbox = GSVector4i(vt.m_min.p.floor().xyxy(vt.m_max.p.ceil())).rintersect(scissor);

	// Points and lines may have zero area bbox (single line: 0, 0 - 256, 0)

	if (vt.m_primclass == GS_POINT_CLASS || vt.m_primclass == GS_LINE_CLASS)
	{
		if (bbox.x == bbox.z)
			bbox.z++;
		if (bbox.y == bbox.w)
			bbox.w++;
	}

	data.scissor = scissor;
	data.bbox = bbox;
	data.frame = g_perfmon.GetFrame();

	gd.vm = hw.m_mem.m_vm8;

	gd.fbo = context->offset.fb;
	gd.zbo = context->offset.zb;
	gd.fzbr = context->offset.fzb4->row;
	gd.fzbc = context->offset.fzb4->col;

	gd.sel.key = 0;

	gd.sel.fpsm = 3;
	gd.sel.zpsm = 3;
	gd.sel.atst = ATST_ALWAYS;
	gd.sel.tfx = TFX_NONE;
	gd.sel.ababcd = 0xff;
	gd.sel.prim = primclass;

	u32 fm = context->FRAME.FBMSK;
	u32 zm = context->ZBUF.ZMSK || context->TEST.ZTE == 0 ? 0xffffffff : 0;
	const u32 fm_mask = GSLocalMemory::m_psm[context->FRAME.PSM].fmsk;

	// When the format is 24bit (Z or C), DATE ceases to function.
	// It was believed that in 24bit mode all pixels pass because alpha doesn't exist
	// however after testing this on a PS2 it turns out nothing passes, it ignores the draw.
	if ((context->FRAME.PSM & 0xF) == PSMCT24 && context->TEST.DATE)
	{
		//DevCon.Warning("DATE on a 24bit format, Frame PSM %x", context->FRAME.PSM);
		return false;
	}

	if (context->TEST.ZTE && context->TEST.ZTST == ZTST_NEVER)
	{
		fm = 0xffffffff;
		zm = 0xffffffff;
	}

	if (PRIM->TME)
	{
		if (GSLocalMemory::m_psm[context->TEX0.PSM].pal > 0)
		{
			hw.m_mem.m_clut.Read32(context->TEX0, env.TEXA);
		}
	}

	if (context->TEST.ATE)
	{
		if (!hw.TryAlphaTest(fm, zm))
		{
			gd.sel.atst = context->TEST.ATST;
			gd.sel.afail = context->TEST.GetAFAIL(context->FRAME.PSM);

			gd.aref = GSVector4i((int)context->TEST.AREF);

			switch (gd.sel.atst)
			{
			case ATST_LESS:
				gd.sel.atst = ATST_LEQUAL;
				gd.aref -= GSVector4i::x00000001();
				break;
			case ATST_GREATER:
				gd.sel.atst = ATST_GEQUAL;
				gd.aref += GSVector4i::x00000001();
				break;
			}
		}
	}

	const bool fwrite = (fm & fm_mask) != fm_mask;
	const bool ftest = gd.sel.atst != ATST_ALWAYS || (context->TEST.DATE && context->FRAME.PSM != PSMCT24);

	const bool zwrite = zm != 0xffffffff;
	const bool ztest = context->TEST.ZTE && context->TEST.ZTST > ZTST_ALWAYS;
	if (!fwrite && !zwrite)
		return false;

	gd.sel.fwrite = fwrite;
	gd.sel.ftest = ftest;

	if (fwrite || ftest)
	{
		gd.sel.fpsm = GSLocalMemory::m_psm[context->FRAME.PSM].fmt;

		if ((primclass == GS_LINE_CLASS || primclass == GS_TRIANGLE_CLASS) && vt.m_eq.rgba != 0xffff)
		{
			gd.sel.iip = PRIM->IIP;
		}

		if (PRIM->TME)
		{
			gd.sel.tfx = context->TEX0.TFX;
			gd.sel.tcc = context->TEX0.TCC;
			gd.sel.fst = PRIM->FST;
			gd.sel.ltf = vt.IsLinear();

			if (GSLocalMemory::m_psm[context->TEX0.PSM].pal > 0)
			{
				gd.sel.tlu = 1;

				gd.clut = const_cast<u32*>(static_cast<const u32*>(hw.m_mem.m_clut));
			}

			gd.sel.wms = context->CLAMP.WMS;
			gd.sel.wmt = context->CLAMP.WMT;

			if (gd.sel.tfx == TFX_MODULATE && gd.sel.tcc && vt.m_eq.rgba == 0xffff && vt.m_min.c.eq(GSVector4i(128)))
			{
				// modulate does not do anything when vertex color is 0x80

				gd.sel.tfx = TFX_DECAL;
			}

			bool mipmap = hw.IsMipMapActive();

			GIFRegTEX0 TEX0 = context->GetSizeFixedTEX0(vt.m_min.t.xyxy(vt.m_max.t), vt.IsLinear(), mipmap);

			const GSVector4i r = hw.GetTextureMinMax(TEX0, context->CLAMP, gd.sel.ltf, true).coverage;

			if (!hw.m_sw_texture[0])
				hw.m_sw_texture[0] = std::make_unique<GSTextureCacheSW::Texture>(0, TEX0, env.TEXA);
			else
				hw.m_sw_texture[0]->Reset(0, TEX0, env.TEXA);

			hw.m_sw_texture[0]->Update(r);
			gd.tex[0] = hw.m_sw_texture[0]->m_buff;

			gd.sel.tw = hw.m_sw_texture[0]->m_tw - 3;

			if (mipmap)
			{
				// TEX1.MMIN
				// 000 p
				// 001 l
				// 010 p round
				// 011 p tri
				// 100 l round
				// 101 l tri

				if (vt.m_lod.x > 0)
				{
					gd.sel.ltf = context->TEX1.MMIN >> 2;
				}
				else
				{
					// TODO: isbilinear(mmag) != isbilinear(mmin) && vt.m_lod.x <= 0 && vt.m_lod.y > 0
				}

				gd.sel.mmin = (context->TEX1.MMIN & 1) + 1; // 1: round, 2: tri
				gd.sel.lcm = context->TEX1.LCM;

				int mxl = std::min<int>((int)context->TEX1.MXL, 6) << 16;
				int k = context->TEX1.K << 12;

				if ((int)vt.m_lod.x >= (int)context->TEX1.MXL)
				{
					k = (int)vt.m_lod.x << 16; // set lod to max level

					gd.sel.lcm = 1; // lod is constant
					gd.sel.mmin = 1; // tri-linear is meaningless
				}

				if (gd.sel.mmin == 2)
				{
					mxl--; // don't sample beyond the last level (TODO: add a dummy level instead?)
				}

				if (gd.sel.fst)
				{
					pxAssert(gd.sel.lcm == 1);
					//pxAssert(((vt.m_min.t.uph(vt.m_max.t) == GSVector4::zero()).mask() & 3) == 3); // ratchet and clank (menu)

					gd.sel.lcm = 1;
				}

				if (gd.sel.lcm)
				{
					int lod = std::max<int>(std::min<int>(k, mxl), 0);

					if (gd.sel.mmin == 1)
					{
						lod = (lod + 0x8000) & 0xffff0000; // rounding
					}

					gd.lod.i = GSVector4i(lod >> 16);
					gd.lod.f = GSVector4i(lod & 0xffff).xxxxl().xxzz();

					// TODO: lot to optimize when lod is constant
				}
				else
				{
					gd.mxl = GSVector4((float)mxl);
					gd.l = GSVector4((float)(-(0x10000 << context->TEX1.L)));
					gd.k = GSVector4((float)k);
				}

				GIFRegCLAMP MIP_CLAMP = context->CLAMP;

				GSVector4 tmin = vt.m_min.t;
				GSVector4 tmax = vt.m_max.t;

				for (int i = 1, j = std::min<int>((int)context->TEX1.MXL, 6); i <= j; i++)
				{
					const GIFRegTEX0& MIP_TEX0 = hw.GetTex0Layer(i);

					MIP_CLAMP.MINU >>= 1;
					MIP_CLAMP.MINV >>= 1;
					MIP_CLAMP.MAXU >>= 1;
					MIP_CLAMP.MAXV >>= 1;

					vt.m_min.t *= 0.5f;
					vt.m_max.t *= 0.5f;

					if (!hw.m_sw_texture[i])
						hw.m_sw_texture[i] = std::make_unique<GSTextureCacheSW::Texture>(gd.sel.tw + 3, MIP_TEX0, env.TEXA);
					else
						hw.m_sw_texture[i]->Reset(gd.sel.tw + 3, MIP_TEX0, env.TEXA);

					GSVector4i r = hw.GetTextureMinMax(MIP_TEX0, MIP_CLAMP, gd.sel.ltf, true).coverage;
					hw.m_sw_texture[i]->Update(r);
					gd.tex[i] = hw.m_sw_texture[i]->m_buff;
				}

				vt.m_min.t = tmin;
				vt.m_max.t = tmax;
			}
			else
			{
				// skip per pixel division if q is constant. Sprite uses flat
				// q, so it's always constant by primitive.
				// Note: the 'q' division was done in GSRendererSW::ConvertVertexBuffer
				gd.sel.fst |= (vt.m_eq.q || primclass == GS_SPRITE_CLASS);

				if (gd.sel.ltf && gd.sel.fst)
				{
					// if q is constant we can do the half pel shift for bilinear sampling on the vertices

					// TODO: but not when mipmapping is used!!!

					const GSVector4 half(0x8000, 0x8000);

					GSVertexSW* RESTRICT v = data.vertex;
					for (int i = 0, j = data.vertex_count; i < j; i++)
					{
						const GSVector4 t = v[i].t;
						v[i].t = (t - half).xyzw(t);
					}
				}
			}

			u16 tw = 1u << TEX0.TW;
			u16 th = 1u << TEX0.TH;

			if (tw > 1024)
				tw = 1;

			if (th > 1024)
				th = 1;

			switch (context->CLAMP.WMS)
			{
				case CLAMP_REPEAT:
					gd.t.min.U16[0] = gd.t.minmax.U16[0] = tw - 1;
					gd.t.max.U16[0] = gd.t.minmax.U16[2] = 0;
					gd.t.mask.U32[0] = 0xffffffff;
					break;
				case CLAMP_CLAMP:
					gd.t.min.U16[0] = gd.t.minmax.U16[0] = 0;
					gd.t.max.U16[0] = gd.t.minmax.U16[2] = tw - 1;
					gd.t.mask.U32[0] = 0;
					break;
				case CLAMP_REGION_CLAMP:
					// REGION_CLAMP ignores the actual texture size
					gd.t.min.U16[0] = gd.t.minmax.U16[0] = context->CLAMP.MINU;
					gd.t.max.U16[0] = gd.t.minmax.U16[2] = context->CLAMP.MAXU;
					gd.t.mask.U32[0] = 0;
					break;
				case CLAMP_REGION_REPEAT:
					// MINU is restricted to MINU or texture size, whichever is smaller, MAXU is an offset in the texture.
					gd.t.min.U16[0] = gd.t.minmax.U16[0] = context->CLAMP.MINU & (tw - 1);
					gd.t.max.U16[0] = gd.t.minmax.U16[2] = context->CLAMP.MAXU;
					gd.t.mask.U32[0] = 0xffffffff;
					break;
				default:
					ASSUME(0);
			}

			switch (context->CLAMP.WMT)
			{
				case CLAMP_REPEAT:
					gd.t.min.U16[4] = gd.t.minmax.U16[1] = th - 1;
					gd.t.max.U16[4] = gd.t.minmax.U16[3] = 0;
					gd.t.mask.U32[2] = 0xffffffff;
					break;
				case CLAMP_CLAMP:
					gd.t.min.U16[4] = gd.t.minmax.U16[1] = 0;
					gd.t.max.U16[4] = gd.t.minmax.U16[3] = th - 1;
					gd.t.mask.U32[2] = 0;
					break;
				case CLAMP_REGION_CLAMP:
					// REGION_CLAMP ignores the actual texture size
					gd.t.min.U16[4] = gd.t.minmax.U16[1] = context->CLAMP.MINV;
					gd.t.max.U16[4] = gd.t.minmax.U16[3] = context->CLAMP.MAXV; // ffx anima summon scene, when the anchor appears (th = 256, maxv > 256)
					gd.t.mask.U32[2] = 0;
					break;
				case CLAMP_REGION_REPEAT:
					// MINV is restricted to MINV or texture size, whichever is smaller, MAXV is an offset in the texture.
					gd.t.min.U16[4] = gd.t.minmax.U16[1] = context->CLAMP.MINV & (th - 1); // skygunner main menu water texture 64x64, MINV = 127
					gd.t.max.U16[4] = gd.t.minmax.U16[3] = context->CLAMP.MAXV;
					gd.t.mask.U32[2] = 0xffffffff;
					break;
				default:
					ASSUME(0);
			}

			gd.t.min = gd.t.min.xxxxlh();
			gd.t.max = gd.t.max.xxxxlh();
			gd.t.mask = gd.t.mask.xxzz();
			gd.t.invmask = ~gd.t.mask;
		}

		if (PRIM->FGE)
		{
			gd.sel.fge = 1;

			gd.frb = env.FOGCOL.U32[0] & 0x00ff00ff;
			gd.fga = (env.FOGCOL.U32[0] >> 8) & 0x00ff00ff;
		}

		if (context->FRAME.PSM != PSMCT24)
		{
			gd.sel.date = context->TEST.DATE;
			gd.sel.datm = context->TEST.DATM;
		}

		if (!hw.IsOpaque())
		{
			gd.sel.abe = PRIM->ABE;
			gd.sel.ababcd = context->ALPHA.U32[0];

			if (env.PABE.PABE)
			{
				gd.sel.pabe = 1;
			}

			if (PRIM->AA1 && (primclass == GS_LINE_CLASS || primclass == GS_TRIANGLE_CLASS))
			{
				gd.sel.aa1 = 1;
			}

			gd.afix = GSVector4i((int)context->ALPHA.FIX << 7).xxzzlh();
		}

		const u32 masked_fm = fm & fm_mask;
		if (gd.sel.date
			|| gd.sel.aba == 1 || gd.sel.abb == 1 || gd.sel.abc == 1 || gd.sel.abd == 1
			|| (gd.sel.atst != ATST_ALWAYS && gd.sel.afail == AFAIL_RGB_ONLY)
			|| (gd.sel.fpsm == 0 && masked_fm != 0 && masked_fm != fm_mask)
			|| (gd.sel.fpsm == 1 && masked_fm != 0 && masked_fm != fm_mask)
			|| (gd.sel.fpsm == 2 && masked_fm != 0 && masked_fm != fm_mask))
		{
			gd.sel.rfb = 1;
		}

		gd.sel.colclamp = env.COLCLAMP.CLAMP;
		gd.sel.fba = context->FBA.FBA;

		if (env.DTHE.DTHE)
		{
			gd.sel.dthe = 1;
			gd.dimx = s_dimx_storage;
			if (s_last_dimx != env.DIMX)
			{
				s_last_dimx = env.DIMX;
				GSState::ExpandDIMX(s_dimx_storage, env.DIMX);
			}
		}
	}

	gd.sel.zwrite = zwrite;
	gd.sel.ztest = ztest;

	if (zwrite || ztest)
	{
		const u32 z_max = 0xffffffff >> (GSLocalMemory::m_psm[context->ZBUF.PSM].fmt * 8);

		gd.sel.zpsm = GSLocalMemory::m_psm[context->ZBUF.PSM].fmt;
		gd.sel.ztst = ztest ? context->TEST.ZTST : (int)ZTST_ALWAYS;
		gd.sel.zequal = !!vt.m_eq.z;
		gd.sel.zoverflow = (u32)GSVector4i(vt.m_max.p).z == 0x80000000U;
		gd.sel.zclamp = (u32)GSVector4i(vt.m_max.p).z > z_max;
	}

#if _M_SSE >= 0x501

	gd.fm = fm;
	gd.zm = zm;

	if (gd.sel.fpsm == 1)
	{
		gd.fm |= 0xff000000;
	}
	else if (gd.sel.fpsm == 2)
	{
		u32 rb = gd.fm & 0x00f800f8;
		u32 ga = gd.fm & 0x8000f800;

		gd.fm = (ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3) | 0xffff0000;
	}

	if (gd.sel.zpsm == 1)
	{
		gd.zm |= 0xff000000;
	}
	else if (gd.sel.zpsm == 2)
	{
		gd.zm |= 0xffff0000;
	}

#else

	gd.fm = GSVector4i(fm);
	gd.zm = GSVector4i(zm);

	if (gd.sel.fpsm == 1)
	{
		gd.fm |= GSVector4i::xff000000();
	}
	else if (gd.sel.fpsm == 2)
	{
		GSVector4i rb = gd.fm & 0x00f800f8;
		GSVector4i ga = gd.fm & 0x8000f800;

		gd.fm = (ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3) | GSVector4i::xffff0000();
	}

	if (gd.sel.zpsm == 1)
	{
		gd.zm |= GSVector4i::xff000000();
	}
	else if (gd.sel.zpsm == 2)
	{
		gd.zm |= GSVector4i::xffff0000();
	}

#endif

	if (gd.sel.prim == GS_SPRITE_CLASS && !gd.sel.ftest && !gd.sel.ztest && data.bbox.eq(data.bbox.rintersect(data.scissor))) // TODO: check scissor horizontally only
	{
		gd.sel.notest = 1;

		const u32 ofx = context->XYOFFSET.OFX;

		for (int i = 0, j = hw.m_vertex.tail; i < j; i++)
		{
#if _M_SSE >= 0x501
			if ((((hw.m_vertex.buff[i].XYZ.X - ofx) + 15) >> 4) & 7) // aligned to 8
#else
			if ((((hw.m_vertex.buff[i].XYZ.X - ofx) + 15) >> 4) & 3) // aligned to 4
#endif
			{
				gd.sel.notest = 0;

				break;
			}
		}
	}

	if (!hw.m_sw_rasterizer)
		hw.m_sw_rasterizer = std::make_unique<GSSingleRasterizer>();

	static_cast<GSSingleRasterizer*>(hw.m_sw_rasterizer.get())->Draw(data);

	if (invalidate_tc)
		g_texture_cache->InvalidateVideoMem(context->offset.fb, bbox);

	// Jak does sw prim render, then draws to the same target, and it needs to be uploaded.
	if (add_ee_transfer)
	{
		GSRendererHW::GSUploadQueue uq;
		uq.blit.U64 = 0;
		uq.blit.DBP = hw.m_cached_ctx.FRAME.Block();
		uq.blit.DBW = hw.m_cached_ctx.FRAME.FBW;
		uq.blit.DPSM = hw.m_cached_ctx.FRAME.PSM;
		uq.draw = GSState::s_n;
		uq.rect = bbox;
		uq.zero_clear = false;
		hw.m_draw_transfers.push_back(uq);
	}

	return true;
}