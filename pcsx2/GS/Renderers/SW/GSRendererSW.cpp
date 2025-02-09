// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/SW/GSRendererSW.h"
#include "GS/GSGL.h"
#include "GS/GSPng.h"
#include "GS/GSUtil.h"

#include "common/StringUtil.h"

MULTI_ISA_UNSHARED_IMPL;

GSRenderer* CURRENT_ISA::makeGSRendererSW(int threads)
{
	return new GSRendererSW(threads);
}

#define LOG 0

[[maybe_unused]] static FILE* s_fp = LOG ? fopen("c:\\temp1\\_.txt", "w") : nullptr;

static constexpr GSVector4 s_pos_scale = GSVector4::cxpr(1.0f / 16, 1.0f / 16, 1.0f, 128.0f);

GSRendererSW::GSRendererSW(int threads)
	: GSRenderer(), m_fzb(NULL)
{
	m_nativeres = true; // ignore ini, sw is always native

	m_tc = std::make_unique<GSTextureCacheSW>();
	m_rl = GSRasterizerList::Create(threads);

	m_output = (u8*)_aligned_malloc(1024 * 1024 * sizeof(u32), VECTOR_ALIGNMENT);

	std::fill(std::begin(m_fzb_pages), std::end(m_fzb_pages), 0);
	std::fill(std::begin(m_tex_pages), std::end(m_tex_pages), 0);
}

GSRendererSW::~GSRendererSW()
{
	// strictly speaking we should always be destroyed when the destructor runs..
	// except if an exception gets thrown during construction. this will go once
	// we get rid of exceptions...
	GSRendererSW::Destroy();
}

void GSRendererSW::Reset(bool hardware_reset)
{
	Sync(-1);

	m_tc->RemoveAll();

	GSRenderer::Reset(hardware_reset);
}

void GSRendererSW::Destroy()
{
	// Need to destroy worker queue first to stop any pending thread work
	m_rl.reset();
	m_tc.reset();

	for (GSTexture*& tex : m_texture)
	{
		delete tex;
		tex = nullptr;
	}

	_aligned_free(m_output);
	m_output = nullptr;
}

void GSRendererSW::VSync(u32 field, bool registers_written, bool idle_frame)
{
	Sync(0); // IncAge might delete a cached texture in use

	/*
	int draw[8], sum = 0;

	for(size_t i = 0; i < std::size(draw); i++)
	{
		draw[i] = m_perfmon.CPU(GSPerfMon::WorkerDraw0 + i);
		sum += draw[i];
	}

	printf("CPU %d Sync %d W %d %d %d %d %d %d %d %d (%d)\n",
		m_perfmon.CPU(GSPerfMon::Main),
		m_perfmon.CPU(GSPerfMon::Sync),
		draw[0], draw[1], draw[2], draw[3], draw[4], draw[5], draw[6], draw[7], sum);

	//
	*/

	GSRenderer::VSync(field, registers_written, idle_frame);

	m_tc->IncAge();

	m_draw_transfers.clear();
	// if((m_perfmon.GetFrame() & 255) == 0) m_rl->PrintStats();
}

GSTexture* GSRendererSW::GetOutput(int i, float& scale, int& y_offset)
{
	Sync(1);

	int index = i >= 0 ? i : 1;
	GSPCRTCRegs::PCRTCDisplay& curFramebuffer = PCRTCDisplays.PCRTCDisplays[index];
	GSVector2i framebufferSize = PCRTCDisplays.GetFramebufferSize(i);
	GSVector4i framebufferRect = PCRTCDisplays.GetFramebufferRect(i);

	// Try to avoid broken/incomplete setups which are probably ingnored on console, but can cause us problems.
	if (framebufferRect.rempty() || curFramebuffer.FBW == 0)
		return nullptr;

	const int w = curFramebuffer.FBW * 64;
	const int h = framebufferSize.y;

	if (g_gs_device->ResizeRenderTarget(&m_texture[index], w, h, false, false))
	{
		const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[curFramebuffer.PSM];
		constexpr int pitch = 1024 * 4;
		// Should really be framebufferOffsets rather than framebufferRect but this might be compensated with anti-blur in some games.
		const int off_x = (framebufferRect.x & 0x7ff) & ~(psm.bs.x-1);
		const int off_x_end = ((framebufferRect.x & 0x7ff) + (psm.bs.x - 1)) & ~(psm.bs.x - 1);
		const int off_y = (framebufferRect.y & 0x7ff) & ~(psm.bs.y-1);
		const int off_y_end = ((framebufferRect.y & 0x7ff) + (psm.bs.y - 1)) & ~(psm.bs.y - 1);
		const GSVector4i out_r(0, 0, w, h);
		GSVector4i r(off_x, off_y, w + off_x_end, h + off_y_end);
		GSVector4i rh(off_x, off_y, w + off_x_end, (h + off_y_end) & 0x7FF);
		GSVector4i rw(off_x, off_y, (w + off_x_end) & 0x7FF, h + off_y_end);
		bool h_wrap = false;
		bool w_wrap = false;

		PCRTCDisplays.RemoveFramebufferOffset(i);
		// Need to read it in 2 parts, since you can't do a split rect.
		if (r.bottom >= 2048)
		{
			r.bottom = 2048;
			rw.bottom = 2048;
			rh.top = 0;
			h_wrap = true;
		}

		if (r.right >= 2048)
		{
			r.right = 2048;
			rh.right = 2048;
			rw.left = 0;
			w_wrap = true;
		}

		// Display doesn't use texa, and instead uses the equivalent of this
		GIFRegTEXA texa = {};
		texa.AEM = 0;
		texa.TA0 = (curFramebuffer.PSM == PSMCT24 || curFramebuffer.PSM == PSGPU24) ? 0x80 : 0;
		texa.TA1 = 0x80;

		// Top left rect
		psm.rtx(m_mem, m_mem.GetOffset(curFramebuffer.Block(), curFramebuffer.FBW, curFramebuffer.PSM), r.ralign<Align_Outside>(psm.bs), m_output, pitch, texa);

		int top = (h_wrap) ? ((r.bottom - r.top) * pitch) : 0;
		int left = (w_wrap) ? (r.right - r.left) * (GSLocalMemory::m_psm[curFramebuffer.PSM].bpp / 8) : 0;

		// The following only happen if the DBX/DBY wrap around at 2048.

		// Top right rect
		if (w_wrap)
			psm.rtx(m_mem, m_mem.GetOffset(curFramebuffer.Block(), curFramebuffer.FBW, curFramebuffer.PSM), rw.ralign<Align_Outside>(psm.bs), &m_output[left], pitch, texa);

		// Bottom left rect
		if (h_wrap)
			psm.rtx(m_mem, m_mem.GetOffset(curFramebuffer.Block(), curFramebuffer.FBW, curFramebuffer.PSM), rh.ralign<Align_Outside>(psm.bs), &m_output[top], pitch, texa);

		// Bottom right rect
		if (h_wrap && w_wrap)
		{
			// Needs also rw with the start/end height of rh, fills in the bottom right rect which will be missing if both overflow.
			const GSVector4i rwh(rw.left, rh.top, rw.right, rh.bottom);
			psm.rtx(m_mem, m_mem.GetOffset(curFramebuffer.Block(), curFramebuffer.FBW, curFramebuffer.PSM), rwh.ralign<Align_Outside>(psm.bs), &m_output[top + left], pitch, texa);
		}

		m_texture[index]->Update(out_r, m_output, pitch);

		if (GSConfig.SaveFrame && GSConfig.ShouldDump(s_n, g_perfmon.GetFrame()))
		{
			m_texture[index]->Save(GetDrawDumpPath("%05d_f%05lld_fr%d_%05x_%s.bmp", s_n, g_perfmon.GetFrame(), i, (int)curFramebuffer.Block(), psm_str(curFramebuffer.PSM)));
		}
	}

	scale = 1.0f;
	return m_texture[index];
}

GSTexture* GSRendererSW::GetFeedbackOutput(float& scale)
{
	int dummy;

	// It is enough to emulate Xenosaga cutscene. (or any game that will do a basic loopback)
	for (int i = 0; i < 2; i++)
	{
		if (m_regs->EXTBUF.EXBP == m_regs->DISP[i].DISPFB.Block())
			return GetOutput(i, scale, dummy);
	}

	return nullptr;
}

MULTI_ISA_DEF(void GSVertexSWInitStatic();)

#if MULTI_ISA_COMPILE_ONCE
GSVertexSW::ConvertVertexBufferPtr GSVertexSW::s_cvb[4][2][2][2];
void GSVertexSW::InitStatic()
{
	MULTI_ISA_SELECT(GSVertexSWInitStatic)();
}
#endif

MULTI_ISA_UNSHARED_START

template <u32 primclass, u32 tme, u32 fst, u32 q_div>
void ConvertVertexBuffer(const GSDrawingContext* RESTRICT ctx, GSVertexSW* RESTRICT dst, const GSVertex* RESTRICT src, u32 count)
{
	// FIXME q_div wasn't added to AVX2 code path.

	GSVector4i off = (GSVector4i)ctx->XYOFFSET;
	GSVector4 tsize = GSVector4(0x10000 << ctx->TEX0.TW, 0x10000 << ctx->TEX0.TH, 1, 0);
	GSVector4i z_max = GSVector4i::xffffffff().srl32(GSLocalMemory::m_psm[ctx->ZBUF.PSM].fmt * 8);

	for (int i = (int)count; i > 0; i--, src++, dst++)
	{
		GSVector4 stcq = GSVector4::load<true>(&src->m[0]); // s t rgba q

		GSVector4i xyzuvf(src->m[1]);

		GSVector4i xy = xyzuvf.upl16() - off;

		dst->c = GSVector4(GSVector4i::cast(stcq).zzzz().u8to32() << 7);

		GSVector4 t = GSVector4::zero();

		if (tme)
		{
			if (fst)
			{
				t = GSVector4(xyzuvf.uph16() << (16 - 4));
			}
			else if (q_div)
			{
				// Division is required if number are huge (Pro Soccer Club)
				if (primclass == GS_SPRITE_CLASS && (i & 1) == 0)
				{
					// q(n) isn't valid, you need to take q(n+1)
					const GSVertex* next = src + 1;
					GSVector4 stcq1 = GSVector4::load<true>(&next->m[0]); // s t rgba q
					t = (stcq / stcq1.wwww()) * tsize;
				}
				else
				{
					t = (stcq / stcq.wwww()) * tsize;
				}
			}
			else
			{
				t = stcq.xyww() * tsize;
			}
		}

		if (primclass == GS_SPRITE_CLASS)
		{
			dst->p = GSVector4(xy).xyyw(GSVector4(xyzuvf)) * s_pos_scale;

			xyzuvf = xyzuvf.min_u32(z_max);
			t = t.insert32<1, 3>(GSVector4::cast(xyzuvf));
		}
		else
		{
			double z = static_cast<double>(static_cast<u32>(xyzuvf.extract32<1>()));
			dst->p = (GSVector4(xy) * s_pos_scale).upld(GSVector4::f64(z, 0.0));
			t = t.blend32<8>(GSVector4(xyzuvf << 7));
		}

		dst->t = t;

#if 0 //_M_SSE >= 0x501

		dst->_pad = GSVector4::zero();

#endif
	}
}

void GSVertexSWInitStatic()
{
#define InitCVB4(P, T, F, Q) GSVertexSW::s_cvb[P][T][F][Q] = ConvertVertexBuffer<P, T, F, Q>;
#define InitCVB3(P, T, F) InitCVB4(P, T, F, 0) InitCVB4(P, T, F, 1)
#define InitCVB2(P, T) InitCVB3(P, T, 0) InitCVB3(P, T, 1)
#define InitCVB1(P) InitCVB2(P, 0) InitCVB2(P, 1)
	InitCVB1(GS_POINT_CLASS)
	InitCVB1(GS_LINE_CLASS)
	InitCVB1(GS_TRIANGLE_CLASS)
	InitCVB1(GS_SPRITE_CLASS)
#undef InitCVB1
#undef InitCVB2
#undef InitCVB3
#undef InitCVB4
}

MULTI_ISA_UNSHARED_END

void GSRendererSW::Draw()
{
	const GSDrawingContext* context = m_context;

	if (GSConfig.SaveInfo && GSConfig.ShouldDump(s_n, g_perfmon.GetFrame()))
	{
		std::string s;

		// Dump Register state
		s = GetDrawDumpPath("%05d_context.txt", s_n);

		m_draw_env->Dump(s);
		m_context->Dump(s);

		// Dump vertices
		s = GetDrawDumpPath("%05d_vertex.txt", s_n);
		DumpVertices(s);
	}

	auto data = m_vertex_heap.make_shared<SharedData>().cast<GSRasterizerData>();
	SharedData* sd = static_cast<SharedData*>(data.get());

	sd->primclass = m_vt.m_primclass;
	sd->buff = (u8*)m_vertex_heap.alloc(sizeof(GSVertexSW) * ((m_vertex.next + 1) & ~1) + sizeof(u32) * m_index.tail, 64);
	sd->vertex = (GSVertexSW*)sd->buff;
	sd->vertex_count = m_vertex.next;
	sd->index = (u16*)(sd->buff + sizeof(GSVertexSW) * ((m_vertex.next + 1) & ~1));
	sd->index_count = m_index.tail;
	sd->scanmsk_value = m_draw_env->SCANMSK.MSK;

	// skip per pixel division if q is constant.
	// Optimize the division by 1 with a nop. It also means that GS_SPRITE_CLASS must be processed when !m_vt.m_eq.q.
	// If you have both GS_SPRITE_CLASS && m_vt.m_eq.q, it will depends on the first part of the 'OR'
	u32 q_div = !IsMipMapActive() && ((m_vt.m_eq.q && m_vt.m_min.t.z != 1.0f) || (!m_vt.m_eq.q && m_vt.m_primclass == GS_SPRITE_CLASS));

	GSVertexSW::s_cvb[m_vt.m_primclass][PRIM->TME][PRIM->FST][q_div](m_context, sd->vertex, m_vertex.buff, m_vertex.next);

	std::memcpy(sd->index, m_index.buff, sizeof(u16) * m_index.tail);

	GSVector4i scissor = context->scissor.in;
	GSVector4i bbox = GSVector4i(m_vt.m_min.p.floor().upld(m_vt.m_max.p.ceil()));

	// points and lines may have zero area bbox (single line: 0, 0 - 256, 0)

	if (m_vt.m_primclass == GS_POINT_CLASS || m_vt.m_primclass == GS_LINE_CLASS)
	{
		if (bbox.x == bbox.z) bbox.z++;
		if (bbox.y == bbox.w) bbox.w++;
	}

	GSVector4i r = bbox.rintersect(scissor);

	sd->scissor = scissor;
	sd->bbox = bbox;
	sd->frame = g_perfmon.GetFrame();

	if (!GetScanlineGlobalData(sd))
	{
		return;
	}

	if constexpr (LOG && false)
	{
		int n = GSUtil::GetVertexCount(PRIM->PRIM);

		for (u32 i = 0, j = 0; i < m_index.tail; i += n, j++)
		{
			for (int k = 0; k < n; k++)
			{
				GSVertex* v = &m_vertex.buff[m_index.buff[i + k]];
				GSVertex* vn = &m_vertex.buff[m_index.buff[i + n - 1]];

				fprintf(s_fp, "%d:%d %f %f %f %f\n",
					j, k,
					(float)(v->XYZ.X - context->XYOFFSET.OFX) / 16,
					(float)(v->XYZ.Y - context->XYOFFSET.OFY) / 16,
					PRIM->FST ? (float)(v->U) / 16 : v->ST.S / (PRIM->PRIM == GS_SPRITE ? vn->RGBAQ.Q : v->RGBAQ.Q),
					PRIM->FST ? (float)(v->V) / 16 : v->ST.T / (PRIM->PRIM == GS_SPRITE ? vn->RGBAQ.Q : v->RGBAQ.Q));
			}
		}
	}

	//

	// GSScanlineGlobalData& gd = sd->global;

	GSOffset::PageLooper* fb_pages = NULL;
	GSOffset::PageLooper* zb_pages = NULL;
	GSOffset::PageLooper _fb_pages, _zb_pages;

	if (sd->global.sel.fb)
	{
		_fb_pages = m_context->offset.fb.pageLooperForRect(r);
		fb_pages = &_fb_pages;
	}

	if (sd->global.sel.zb)
	{
		_zb_pages = m_context->offset.zb.pageLooperForRect(r);
		zb_pages = &_zb_pages;
	}

	// check if there is an overlap between this and previous targets

	if (CheckTargetPages(fb_pages, zb_pages, r))
	{
		sd->m_syncpoint = SharedData::SyncTarget;
	}

	// check if the texture is not part of a target currently in use

	if (CheckSourcePages(sd))
	{
		sd->m_syncpoint = SharedData::SyncSource;
	}

	// addref source and target pages

	sd->UsePages(fb_pages, m_context->offset.fb.psm(), zb_pages, m_context->offset.zb.psm());

	if (GSConfig.ShouldDump(s_n, g_perfmon.GetFrame()))
	{
		Sync(2);

		std::string s;

		u64 frame = g_perfmon.GetFrame();
		// Dump the texture in 32 bits format. It helps to debug texture shuffle effect
		// It will breaks the few games that really uses 16 bits RT
		bool texture_shuffle = ((context->FRAME.PSM & 0x2) && ((context->TEX0.PSM & 3) == 2) && (m_vt.m_primclass == GS_SPRITE_CLASS));

		if (GSConfig.SaveTexture && PRIM->TME)
		{
			if (texture_shuffle)
			{
				// Dump the RT in 32 bits format. It helps to debug texture shuffle effect
				s = GetDrawDumpPath("%05d_f%05lld_itexraw_%05x_32bits.bmp", s_n, frame, (int)m_context->TEX0.TBP0);
				m_mem.SaveBMP(s, m_context->TEX0.TBP0, m_context->TEX0.TBW, 0, 1 << m_context->TEX0.TW, 1 << m_context->TEX0.TH);
			}

			s = GetDrawDumpPath("%05d_f%05lld_itexraw_%05x_%s.bmp", s_n, frame, (int)m_context->TEX0.TBP0, psm_str(m_context->TEX0.PSM));
			m_mem.SaveBMP(s, m_context->TEX0.TBP0, m_context->TEX0.TBW, m_context->TEX0.PSM, 1 << m_context->TEX0.TW, 1 << m_context->TEX0.TH);
		}

		if (GSConfig.SaveRT)
		{

			if (texture_shuffle)
			{
				// Dump the RT in 32 bits format. It helps to debug texture shuffle effect
				s = GetDrawDumpPath("%05d_f%05lld_rt0_%05x_32bits.bmp", s_n, frame, m_context->FRAME.Block());
				m_mem.SaveBMP(s, m_context->FRAME.Block(), m_context->FRAME.FBW, 0, r.z, r.w);
			}

			s = GetDrawDumpPath("%05d_f%05lld_rt0_%05x_%s.bmp", s_n, frame, m_context->FRAME.Block(), psm_str(m_context->FRAME.PSM));
			m_mem.SaveBMP(s, m_context->FRAME.Block(), m_context->FRAME.FBW, m_context->FRAME.PSM, r.z, r.w);
		}

		if (GSConfig.SaveDepth)
		{
			s = GetDrawDumpPath("%05d_f%05lld_rz0_%05x_%s.bmp", s_n, frame, m_context->ZBUF.Block(), psm_str(m_context->ZBUF.PSM));

			m_mem.SaveBMP(s, m_context->ZBUF.Block(), m_context->FRAME.FBW, m_context->ZBUF.PSM, r.z, r.w);
		}

		Queue(data);

		Sync(3);

		if (GSConfig.SaveRT)
		{
			if (texture_shuffle)
			{
				// Dump the RT in 32 bits format. It helps to debug texture shuffle effect
				s = GetDrawDumpPath("%05d_f%05lld_rt1_%05x_32bits.bmp", s_n, frame, m_context->FRAME.Block());
				m_mem.SaveBMP(s, m_context->FRAME.Block(), m_context->FRAME.FBW, 0, r.z, r.w);
			}

			s = GetDrawDumpPath("%05d_f%05lld_rt1_%05x_%s.bmp", s_n, frame, m_context->FRAME.Block(), psm_str(m_context->FRAME.PSM));
			m_mem.SaveBMP(s, m_context->FRAME.Block(), m_context->FRAME.FBW, m_context->FRAME.PSM, r.z, r.w);
		}

		if (GSConfig.SaveDepth)
		{
			s = GetDrawDumpPath("%05d_f%05lld_rz1_%05x_%s.bmp", s_n, frame, m_context->ZBUF.Block(), psm_str(m_context->ZBUF.PSM));

			m_mem.SaveBMP(s, m_context->ZBUF.Block(), m_context->FRAME.FBW, m_context->ZBUF.PSM, r.z, r.w);
		}
	}
	else
	{
		Queue(data);
	}

	/*
	if(0)//stats.ticks > 5000000)
	{
		printf("* [%lld | %012llx] ticks %lld prims %d (%d) pixels %d (%d)\n",
			m_perfmon.GetFrame(), gd->sel.key,
			stats.ticks,
			stats.prims, stats.prims > 0 ? (int)(stats.ticks / stats.prims) : -1,
			stats.pixels, stats.pixels > 0 ? (int)(stats.ticks / stats.pixels) : -1);
	}
	*/
}

void GSRendererSW::Queue(GSRingHeap::SharedPtr<GSRasterizerData>& item)
{
	SharedData* sd = (SharedData*)item.get();

	if (sd->m_syncpoint == SharedData::SyncSource)
	{
		Sync(4);
	}

	// update previously invalidated parts

	sd->UpdateSource();

	if (sd->m_syncpoint == SharedData::SyncTarget)
	{
		Sync(5);
	}

	if constexpr (LOG)
	{
		GSScanlineGlobalData& gd = ((SharedData*)item.get())->global;

		fprintf(s_fp, "[%d] queue %05x %d (%d) %05x %d (%d) %05x %d %dx%d (%d %d %d) | %u %d %d\n",
			sd->counter,
			m_context->FRAME.Block(), m_context->FRAME.PSM, gd.sel.fwrite,
			m_context->ZBUF.Block(), m_context->ZBUF.PSM, gd.sel.zwrite,
			PRIM->TME ? m_context->TEX0.TBP0 : 0xfffff, m_context->TEX0.PSM, (int)m_context->TEX0.TW, (int)m_context->TEX0.TH, m_context->TEX0.CSM, m_context->TEX0.CPSM, m_context->TEX0.CSA,
			PRIM->PRIM, sd->vertex_count, sd->index_count);

		fflush(s_fp);
	}

	m_rl->Queue(item);

	// invalidate new parts rendered onto

	if (sd->global.sel.fwrite)
	{
		m_tc->InvalidatePages(sd->m_fb_pages, sd->m_fpsm);
	}

	if (sd->global.sel.zwrite)
	{
		m_tc->InvalidatePages(sd->m_zb_pages, sd->m_zpsm);
	}
}

void GSRendererSW::Sync(int reason)
{
	//printf("sync %d\n", reason);

	u64 t = LOG ? GetCPUTicks() : 0;

	m_rl->Sync();

	if constexpr (LOG && false)
	{
		std::string s;

		if (GSConfig.SaveRT)
		{
			s = GetDrawDumpPath("%05d_f%05lld_rt1_%05x_%s.bmp", s_n, g_perfmon.GetFrame(), m_context->FRAME.Block(), psm_str(m_context->FRAME.PSM));

			m_mem.SaveBMP(s, m_context->FRAME.Block(), m_context->FRAME.FBW, m_context->FRAME.PSM, PCRTCDisplays.GetFramebufferRect(-1).width(), 512);
		}

		if (GSConfig.SaveDepth)
		{
			s = GetDrawDumpPath("%05d_f%05lld_zb1_%05x_%s.bmp", s_n, g_perfmon.GetFrame(), m_context->ZBUF.Block(), psm_str(m_context->ZBUF.PSM));

			m_mem.SaveBMP(s, m_context->ZBUF.Block(), m_context->FRAME.FBW, m_context->ZBUF.PSM, PCRTCDisplays.GetFramebufferRect(-1).width(), 512);
		}
	}

	t = LOG ? (GetCPUTicks() - t) : 0;

	int pixels = m_rl->GetPixels();

	if constexpr (LOG)
	{
		fprintf(s_fp, "sync n=%d r=%d t=%" PRIu64 " p=%d %c\n", s_n, reason, t, pixels, t > 10000000 ? '*' : ' ');
		fflush(s_fp);
	}

	g_perfmon.Put(GSPerfMon::Fillrate, pixels);
}

void GSRendererSW::InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r)
{
	if constexpr (LOG)
	{
		fprintf(s_fp, "w %05x %u %u, %d %d %d %d\n", BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM, r.x, r.y, r.z, r.w);
		fflush(s_fp);
	}

	GSOffset off = m_mem.GetOffset(BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM);
	GSOffset::PageLooper pages = off.pageLooperForRect(r);

	// check if the changing pages either used as a texture or a target

	if (!m_rl->IsSynced())
	{
		pages.loopPagesWithBreak([this](u32 page)
		{
			if (m_fzb_pages[page] | m_tex_pages[page])
			{
				Sync(6);
				return false;
			}
			return true;
		});
	}

	m_tc->InvalidatePages(pages, off.psm()); // if texture update runs on a thread and Sync(5) happens then this must come later
}

void GSRendererSW::InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut)
{
	if constexpr (LOG)
	{
		fprintf(s_fp, "%s %05x %u %u, %d %d %d %d\n", clut ? "rp" : "r", BITBLTBUF.SBP, BITBLTBUF.SBW, BITBLTBUF.SPSM, r.x, r.y, r.z, r.w);
		fflush(s_fp);
	}

	if (!m_rl->IsSynced())
	{
		GSOffset off = m_mem.GetOffset(BITBLTBUF.SBP, BITBLTBUF.SBW, BITBLTBUF.SPSM);
		GSOffset::PageLooper pages = off.pageLooperForRect(r);

		pages.loopPagesWithBreak([this](u32 page)
		{
			if (m_fzb_pages[page])
			{
				Sync(7);
				return false;
			}
			return true;
		});
	}
}

void GSRendererSW::UsePages(const GSOffset::PageLooper& pages, const int type)
{
	pages.loopPages([this, type](u32 page)
	{
		switch (type)
		{
			case 0:
				pxAssert((m_fzb_pages[page] & 0xFFFF) < USHRT_MAX);
				m_fzb_pages[page] += 1;
				break;
			case 1:
				pxAssert((m_fzb_pages[page] >> 16) < USHRT_MAX);
				m_fzb_pages[page] += 0x10000;
				break;
			case 2:
				pxAssert(m_tex_pages[page] < USHRT_MAX);
				m_tex_pages[page] += 1;
				break;
			default:
				break;
		}
	});
}

void GSRendererSW::ReleasePages(const GSOffset::PageLooper& pages, const int type)
{
	pages.loopPages([this, type](u32 page)
	{
		switch (type)
		{
			case 0:
				pxAssert((m_fzb_pages[page] & 0xFFFF) > 0);
				m_fzb_pages[page] -= 1;
				break;
			case 1:
				pxAssert((m_fzb_pages[page] >> 16) > 0);
				m_fzb_pages[page] -= 0x10000;
				break;
			case 2:
				pxAssert(m_tex_pages[page] > 0);
				m_tex_pages[page] -= 1;
				break;
			default:
				break;
		}
	});
}

bool GSRendererSW::CheckTargetPages(const GSOffset::PageLooper* fb_pages, const GSOffset::PageLooper* zb_pages, const GSVector4i& r)
{
	const bool synced = m_rl->IsSynced();

	const bool fb = (fb_pages != nullptr);
	const bool zb = (zb_pages != nullptr);

	GSOffset::PageLooper _fb_pages, _zb_pages;
	const auto requirePages = [this, &fb_pages, &zb_pages, &r, &_fb_pages, &_zb_pages]
	{
		if (!fb_pages)
		{
			_fb_pages = m_context->offset.fb.pageLooperForRect(r);
			fb_pages = &_fb_pages;
		}
		if (!zb_pages)
		{
			_zb_pages = m_context->offset.zb.pageLooperForRect(r);
			zb_pages = &_zb_pages;
		}
	};

	bool res = false;

	if (m_fzb != m_context->offset.fzb4)
	{
		// targets changed, check everything

		m_fzb = m_context->offset.fzb4;
		m_fzb_bbox = r;

		memset(m_fzb_cur_pages, 0, sizeof(m_fzb_cur_pages));

		u32 used = 0;

		requirePages();

		fb_pages->loopPages([this, &used](u32 i)
		{
			const u32 row = i >> 5;
			const u32 col = 1 << (i & 31);

			m_fzb_cur_pages[row] |= col;

			used |= m_fzb_pages[i];
			used |= m_tex_pages[i];
		});

		zb_pages->loopPages([this, &used](u32 i)
		{
			const u32 row = i >> 5;
			const u32 col = 1 << (i & 31);

			m_fzb_cur_pages[row] |= col;

			used |= m_fzb_pages[i];
			used |= m_tex_pages[i];
		});

		if (!synced)
		{
			if (used)
			{
				if constexpr (LOG)
				{
					fprintf(s_fp, "syncpoint 0\n");
					fflush(s_fp);
				}

				res = true;
			}

			//if(LOG) {fprintf(s_fp, "no syncpoint *\n"); fflush(s_fp);}
		}
	}
	else
	{
		// same target, only check new areas and cross-rendering between frame and z-buffer

		const GSVector4i bbox = m_fzb_bbox.runion(r);

		const bool check = !m_fzb_bbox.eq(bbox);

		m_fzb_bbox = bbox;

		if (check)
		{
			// drawing area is larger than previous time, check new parts only to avoid false positives (m_fzb_cur_pages guards)

			requirePages();

			u32 used = 0;

			fb_pages->loopPages([this, &used](u32 i)
			{
				u32 row = i >> 5;
				u32 col = 1 << (i & 31);

				if ((m_fzb_cur_pages[row] & col) == 0)
				{
					m_fzb_cur_pages[row] |= col;

					used |= m_fzb_pages[i];
				}
			});

			zb_pages->loopPages([this, &used](u32 i)
			{
				u32 row = i >> 5;
				u32 col = 1 << (i & 31);

				if ((m_fzb_cur_pages[row] & col) == 0)
				{
					m_fzb_cur_pages[row] |= col;

					used |= m_fzb_pages[i];
				}
			});

			if (!synced)
			{
				if (used)
				{
					if constexpr (LOG)
					{
						fprintf(s_fp, "syncpoint 1\n");
						fflush(s_fp);
					}

					res = true;
				}
			}
		}

		if (!synced)
		{
			// chross-check frame and z-buffer pages, they cannot overlap with eachother and with previous batches in queue,
			// have to be careful when the two buffers are mutually enabled/disabled and alternating (Bully FBP/ZBP = 0x2300)

			if (fb && !res)
			{
				fb_pages->loopPagesWithBreak([this, &res](u32 page)
				{
					if (m_fzb_pages[page] & 0xffff0000)
					{
						if constexpr (LOG)
						{
							fprintf(s_fp, "syncpoint 2\n");
							fflush(s_fp);
						}

						res = true;
						return false;
					}

					return true;
				});
			}

			if (zb && !res)
			{
				zb_pages->loopPagesWithBreak([this, &res](u32 page)
				{
					if (m_fzb_pages[page] & 0x0000ffff)
					{
						if constexpr (LOG)
						{
							fprintf(s_fp, "syncpoint 3\n");
							fflush(s_fp);
						}

						res = true;
						return false;
					}

					return true;
				});
			}
		}
	}

	return res;
}

bool GSRendererSW::CheckSourcePages(SharedData* sd)
{
	if (!m_rl->IsSynced())
	{
		for (size_t i = 0; sd->m_tex[i].t != NULL; i++)
		{
			GSOffset::PageLooper pages = sd->m_tex[i].t->m_offset.pageLooperForRect(sd->m_tex[i].r);

			bool ret = false;
			pages.loopPagesWithBreak([this, &ret](u32 pages)
			{
				// TODO: 8H 4HL 4HH texture at the same place as the render target (24 bit, or 32-bit where the alpha channel is masked, Valkyrie Profile 2)

				if (m_fzb_pages[pages]) // currently being drawn to? => sync
				{
					ret = true;
					return false;
				}
				return true;
			});
			if (ret)
				return true;
		}
	}

	return false;
}

bool GSRendererSW::GetScanlineGlobalData(SharedData* data)
{
	GSScanlineGlobalData& gd = data->global;

	const GSDrawingEnvironment& env = *m_draw_env;
	const GSDrawingContext* context = m_context;
	const GS_PRIM_CLASS primclass = m_vt.m_primclass;

	gd.vm = m_mem.m_vm8;

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
	const u32 fm_mask = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmsk;

	// When the format is 24bit (Z or C), DATE ceases to function.
	// It was believed that in 24bit mode all pixels pass because alpha doesn't exist
	// however after testing this on a PS2 it turns out nothing passes, it ignores the draw.
	if ((m_context->FRAME.PSM & 0xF) == PSMCT24 && m_context->TEST.DATE)
	{
		//DevCon.Warning("DATE on a 24bit format, Frame PSM %x", m_context->FRAME.PSM);
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
			m_mem.m_clut.Read32(context->TEX0, env.TEXA);
		}
	}

	if (context->TEST.ATE)
	{
		if (!TryAlphaTest(fm, zm))
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

	bool fwrite = (fm & fm_mask) != fm_mask;
	bool ftest = gd.sel.atst != ATST_ALWAYS || (context->TEST.DATE && context->FRAME.PSM != PSMCT24);

	bool zwrite = zm != 0xffffffff;
	bool ztest = context->TEST.ZTE && context->TEST.ZTST > ZTST_ALWAYS;
	/*
	printf("%05x %d %05x %d %05x %d %dx%d\n",
		fwrite || ftest ? m_context->FRAME.Block() : 0xfffff, m_context->FRAME.PSM,
		zwrite || ztest ? m_context->ZBUF.Block() : 0xfffff, m_context->ZBUF.PSM,
		PRIM->TME ? m_context->TEX0.TBP0 : 0xfffff, m_context->TEX0.PSM, (int)m_context->TEX0.TW, (int)m_context->TEX0.TH);
	*/
	if (!fwrite && !zwrite)
		return false;

	gd.sel.fwrite = fwrite;
	gd.sel.ftest = ftest;

	if (fwrite || ftest)
	{
		gd.sel.fpsm = GSLocalMemory::m_psm[context->FRAME.PSM].fmt;

		if ((primclass == GS_LINE_CLASS || primclass == GS_TRIANGLE_CLASS) && m_vt.m_eq.rgba != 0xffff)
		{
			gd.sel.iip = PRIM->IIP;
		}

		if (PRIM->TME)
		{
			gd.sel.tfx = context->TEX0.TFX;
			gd.sel.tcc = context->TEX0.TCC;
			gd.sel.fst = PRIM->FST;
			gd.sel.ltf = m_vt.IsLinear();

			if (GSLocalMemory::m_psm[context->TEX0.PSM].pal > 0)
			{
				gd.sel.tlu = 1;

				gd.clut = (u32*)m_vertex_heap.alloc(sizeof(u32) * 256, VECTOR_ALIGNMENT); // FIXME: might address uninitialized data of the texture (0xCD) that is not in 0-15 range for 4-bpp formats

				memcpy(gd.clut, (const u32*)m_mem.m_clut, sizeof(u32) * GSLocalMemory::m_psm[context->TEX0.PSM].pal);
			}

			gd.sel.wms = context->CLAMP.WMS;
			gd.sel.wmt = context->CLAMP.WMT;

			if (gd.sel.tfx == TFX_MODULATE && gd.sel.tcc && m_vt.m_eq.rgba == 0xffff && m_vt.m_min.c.eq(GSVector4i(128)))
			{
				// modulate does not do anything when vertex color is 0x80

				gd.sel.tfx = TFX_DECAL;
			}

			CalculatePrimitiveCoversWithoutGaps();

			bool mipmap = IsMipMapActive();

			GIFRegTEX0 TEX0 = m_context->GetSizeFixedTEX0(m_vt.m_min.t.xyxy(m_vt.m_max.t), m_vt.IsLinear(), mipmap);

			GSVector4i r = GetTextureMinMax(TEX0, context->CLAMP, gd.sel.ltf, true).coverage;

			GSTextureCacheSW::Texture* t = m_tc->Lookup(TEX0, env.TEXA);

			if (t == NULL)
			{
				pxAssert(0);
				return false;
			}

			data->SetSource(t, r, 0);

			gd.sel.tw = t->m_tw - 3;

			if (mipmap)
			{
				// TEX1.MMIN
				// 000 p
				// 001 l
				// 010 p round
				// 011 p tri
				// 100 l round
				// 101 l tri

				if (m_vt.m_lod.x > 0)
				{
					gd.sel.ltf = context->TEX1.MMIN >> 2;
				}
				else
				{
					// TODO: isbilinear(mmag) != isbilinear(mmin) && m_vt.m_lod.x <= 0 && m_vt.m_lod.y > 0
				}

				gd.sel.mmin = (context->TEX1.MMIN & 1) + 1; // 1: round, 2: tri
				gd.sel.lcm = context->TEX1.LCM;

				int mxl = std::min<int>((int)context->TEX1.MXL, 6) << 16;
				int k = context->TEX1.K << 12;

				if ((int)m_vt.m_lod.x >= (int)context->TEX1.MXL)
				{
					k = (int)m_vt.m_lod.x << 16; // set lod to max level

					gd.sel.lcm = 1;  // lod is constant
					gd.sel.mmin = 1; // tri-linear is meaningless
				}

				if (gd.sel.mmin == 2)
				{
					mxl--; // don't sample beyond the last level (TODO: add a dummy level instead?)
				}

				if (gd.sel.fst)
				{
					pxAssert(gd.sel.lcm == 1);
					pxAssert(((m_vt.m_min.t.uph(m_vt.m_max.t) == GSVector4::zero()).mask() & 3) == 3); // ratchet and clank (menu)

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

				GSVector4 tmin = m_vt.m_min.t;
				GSVector4 tmax = m_vt.m_max.t;

				for (int i = 1, j = std::min<int>((int)context->TEX1.MXL, 6); i <= j; i++)
				{
					const GIFRegTEX0& MIP_TEX0 = GetTex0Layer(i);

					MIP_CLAMP.MINU >>= 1;
					MIP_CLAMP.MINV >>= 1;
					MIP_CLAMP.MAXU >>= 1;
					MIP_CLAMP.MAXV >>= 1;

					m_vt.m_min.t *= 0.5f;
					m_vt.m_max.t *= 0.5f;

					GSTextureCacheSW::Texture* t = m_tc->Lookup(MIP_TEX0, env.TEXA, gd.sel.tw + 3);

					if (t == NULL)
					{
						pxAssert(0);
						return false;
					}

					GSVector4i r = GetTextureMinMax(MIP_TEX0, MIP_CLAMP, gd.sel.ltf, true).coverage;

					data->SetSource(t, r, i);
				}

				m_vt.m_min.t = tmin;
				m_vt.m_max.t = tmax;
			}
			else
			{
				// skip per pixel division if q is constant. Sprite uses flat
				// q, so it's always constant by primitive.
				// Note: the 'q' division was done in GSRendererSW::ConvertVertexBuffer
				gd.sel.fst |= (m_vt.m_eq.q || primclass == GS_SPRITE_CLASS);

				if (gd.sel.ltf && gd.sel.fst)
				{
					// if q is constant we can do the half pel shift for bilinear sampling on the vertices

					// TODO: but not when mipmapping is used!!!

					GSVector4 half(0x8000, 0x8000);

					GSVertexSW* RESTRICT v = data->vertex;

					for (int i = 0, j = data->vertex_count; i < j; i++)
					{
						GSVector4 t = v[i].t;

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
					// REGION_CLAMP ignores the actual texture size, but tw is already optimised in GetFixedTEX0Size.
					// It's important we don't go off MAXU (if bigger) here as the sw renderer can attempt to draw pixels outside the triangle which can cause out of bounds issues.
					gd.t.min.U16[0] = gd.t.minmax.U16[0] = std::min<u16>(context->CLAMP.MINU, tw - 1);
					gd.t.max.U16[0] = gd.t.minmax.U16[2] = std::min<u16>(context->CLAMP.MAXU, tw - 1);
					gd.t.mask.U32[0] = 0;
					break;
				case CLAMP_REGION_REPEAT:
					// MINU is restricted to MINU or texture size, whichever is smaller, MAXU is an offset in the texture (Can be bigger than the texture).
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
					// REGION_CLAMP ignores the actual texture size, but th is already optimised in GetFixedTEX0Size
					// It's important we don't go off MAXV (if bigger) here as the sw renderer can attempt to draw pixels outside the triangle which can cause out of bounds issues.
					gd.t.min.U16[4] = gd.t.minmax.U16[1] = std::min<u16>(context->CLAMP.MINV, th - 1);
					gd.t.max.U16[4] = gd.t.minmax.U16[3] = std::min<u16>(context->CLAMP.MAXV, th - 1); // ffx anima summon scene, when the anchor appears (th = 256, maxv > 256)
					gd.t.mask.U32[2] = 0;
					break;
				case CLAMP_REGION_REPEAT:
					// MINV is restricted to MINV or texture size, whichever is smaller, MAXV is an offset in the texture (Can be bigger than the texture).
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

		if (!IsOpaque())
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

			if (m_last_dimx != env.DIMX)
			{
				m_last_dimx = env.DIMX;
				ExpandDIMX(m_dimx, env.DIMX);
			}

			gd.dimx = (GSVector4i*)m_vertex_heap.alloc(sizeof(m_dimx), VECTOR_ALIGNMENT);

			std::memcpy(gd.dimx, m_dimx, sizeof(m_dimx));
		}
	}

	gd.sel.zwrite = zwrite;
	gd.sel.ztest = ztest;

	if (zwrite || ztest)
	{
		u32 z_max = 0xffffffff >> (GSLocalMemory::m_psm[context->ZBUF.PSM].fmt * 8);

		gd.sel.zpsm = GSLocalMemory::m_psm[context->ZBUF.PSM].fmt;
		gd.sel.ztst = ztest ? context->TEST.ZTST : (int)ZTST_ALWAYS;
		gd.sel.zequal = !!m_vt.m_eq.z;
		gd.sel.zoverflow = (u32)GSVector4i(m_vt.m_max.p).z == 0x80000000U;
		gd.sel.zclamp = (u32)GSVector4i(m_vt.m_max.p).z > z_max;
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

	if (gd.sel.prim == GS_SPRITE_CLASS && !gd.sel.ftest && !gd.sel.ztest && data->bbox.eq(data->bbox.rintersect(data->scissor))) // TODO: check scissor horizontally only
	{
		gd.sel.notest = 1;

		u32 ofx = context->XYOFFSET.OFX;

		for (int i = 0, j = m_vertex.tail; i < j; i++)
		{
#if _M_SSE >= 0x501
			if ((((m_vertex.buff[i].XYZ.X - ofx) + 15) >> 4) & 7) // aligned to 8
#else
			if ((((m_vertex.buff[i].XYZ.X - ofx) + 15) >> 4) & 3) // aligned to 4
#endif
			{
				gd.sel.notest = 0;

				break;
			}
		}
	}

	return true;
}

GSRendererSW::SharedData::SharedData()
	: m_fpsm(0)
	, m_zpsm(0)
	, m_using_pages(false)
	, m_syncpoint(SyncNone)
{
	m_tex[0].t = NULL;

	global.sel.key = 0;

	global.clut = NULL;
	global.dimx = NULL;
}

GSRendererSW::SharedData::~SharedData()
{
	ReleasePages();

	if (global.clut)
		GSRingHeap::free(global.clut);
	if (global.dimx)
		GSRingHeap::free(global.dimx);

	if constexpr (LOG)
	{
		fprintf(s_fp, "[%d] done t=%" PRId64 " p=%d | %d %d %d | %08x_%08x\n",
			counter,
			GetCPUTicks() - start, pixels,
			primclass, vertex_count, index_count,
			global.sel.hi, global.sel.lo);
		fflush(s_fp);
	}
}

//static TransactionScope::Lock s_lock;

void GSRendererSW::SharedData::UsePages(const GSOffset::PageLooper* fb_pages, int fpsm, const GSOffset::PageLooper* zb_pages, int zpsm)
{
	if (m_using_pages)
		return;

	{
		//TransactionScope scope(s_lock);

		if (global.sel.fb)
		{
			GSRendererSW::GetInstance()->UsePages(*fb_pages, 0);
		}

		if (global.sel.zb)
		{
			GSRendererSW::GetInstance()->UsePages(*zb_pages, 1);
		}

		for (size_t i = 0; m_tex[i].t != NULL; i++)
		{
			GSRendererSW::GetInstance()->UsePages(m_tex[i].t->m_pages, 2);
		}
	}

	if (fb_pages)
		m_fb_pages = *fb_pages;
	if (zb_pages)
		m_zb_pages = *zb_pages;
	m_fpsm = fpsm;
	m_zpsm = zpsm;

	m_using_pages = true;
}

void GSRendererSW::SharedData::ReleasePages()
{
	if (!m_using_pages)
		return;

	{
		//TransactionScope scope(s_lock);

		if (global.sel.fb)
		{
			GSRendererSW::GetInstance()->ReleasePages(m_fb_pages, 0);
		}

		if (global.sel.zb)
		{
			GSRendererSW::GetInstance()->ReleasePages(m_zb_pages, 1);
		}

		for (size_t i = 0; m_tex[i].t != NULL; i++)
		{
			GSRendererSW::GetInstance()->ReleasePages(m_tex[i].t->m_pages, 2);
		}
	}

	m_using_pages = false;
}

void GSRendererSW::SharedData::SetSource(GSTextureCacheSW::Texture* t, const GSVector4i& r, int level)
{
	pxAssert(!m_tex[level].t);

	m_tex[level].t = t;
	m_tex[level].r = r;

	m_tex[level + 1].t = nullptr;
}

void GSRendererSW::SharedData::UpdateSource()
{
	for (size_t i = 0; m_tex[i].t; i++)
	{
		if (m_tex[i].t->Update(m_tex[i].r))
		{
			global.tex[i] = m_tex[i].t->m_buff;
		}
		else
		{
			printf("GS: out-of-memory, texturing temporarily disabled\n");

			global.sel.tfx = TFX_NONE;
		}
	}

	if (GSConfig.SaveTexture && GSConfig.ShouldDump(s_n, g_perfmon.GetFrame()))
	{
		const u64 frame = g_perfmon.GetFrame();

		std::string s;

		for (size_t i = 0; m_tex[i].t; i++)
		{
			const GIFRegTEX0& TEX0 = g_gs_renderer->GetTex0Layer(i);

			s = GetDrawDumpPath("%05d_f%05lld_itex%d_%05x_%s.bmp", g_gs_renderer->s_n, frame, i, TEX0.TBP0, psm_str(TEX0.PSM));

			m_tex[i].t->Save(s);
		}

		if (global.clut)
		{
			s = GetDrawDumpPath("%05d_f%05lld_itexp_%05x_%s.bmp", g_gs_renderer->s_n, frame, (int)g_gs_renderer->m_context->TEX0.CBP, psm_str(g_gs_renderer->m_context->TEX0.CPSM));
			GSPng::Save((IsDevBuild || GSConfig.SaveAlpha) ? GSPng::RGB_A_PNG : GSPng::RGB_PNG, s, reinterpret_cast<const u8*>(global.clut), 256, 1, sizeof(u32) * 256, GSConfig.PNGCompressionLevel, false);
		}
	}
}
