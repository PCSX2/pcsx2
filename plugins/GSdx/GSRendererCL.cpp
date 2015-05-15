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
#include "GSRendererCL.h"

#ifdef ENABLE_OPENCL

#define LOG 0

static FILE* s_fp = LOG ? fopen("c:\\temp1\\_.txt", "w") : NULL;

#define MAX_FRAME_SIZE 2048
#define MAX_PRIM_COUNT 4096u
#define MAX_PRIM_PER_BATCH_BITS 5
#define MAX_PRIM_PER_BATCH (1u << MAX_PRIM_PER_BATCH_BITS)
#define BATCH_COUNT(prim_count) (((prim_count) + (MAX_PRIM_PER_BATCH - 1)) / MAX_PRIM_PER_BATCH)
#define MAX_BATCH_COUNT BATCH_COUNT(MAX_PRIM_COUNT)
#define BIN_SIZE_BITS 4
#define BIN_SIZE (1u << BIN_SIZE_BITS)
#define MAX_BIN_PER_BATCH ((MAX_FRAME_SIZE / BIN_SIZE) * (MAX_FRAME_SIZE / BIN_SIZE))
#define MAX_BIN_COUNT (MAX_BIN_PER_BATCH * MAX_BATCH_COUNT)
#define TFX_PARAM_SIZE 2048
#define TFX_PROGRAM_VERSION 1

#if MAX_PRIM_PER_BATCH == 64u
#define BIN_TYPE cl_ulong
#elif MAX_PRIM_PER_BATCH == 32u
#define BIN_TYPE cl_uint
#else
#error "MAX_PRIM_PER_BATCH != 32u OR 64u"
#endif

#pragma pack(push, 1)

typedef struct
{
	GSVertexCL v[4];
} gs_prim;

typedef struct
{
	cl_float4 dx, dy;
	cl_float4 zero;
	cl_float4 reject_corner;
} gs_barycentric;

typedef struct
{
	struct { cl_uint first, last; } bounds[MAX_BIN_PER_BATCH];
	BIN_TYPE bin[MAX_BIN_COUNT];
	cl_uchar4 bbox[MAX_PRIM_COUNT];
	gs_prim prim[MAX_PRIM_COUNT];
	gs_barycentric barycentric[MAX_PRIM_COUNT];
} gs_env;

#pragma pack(pop)

GSRendererCL::GSRendererCL()
	: m_vb_count(0)
	, m_synced(true)
{
	m_nativeres = true; // ignore ini, sw is always native

	memset(m_texture, 0, sizeof(m_texture));

	m_output = (uint8*)_aligned_malloc(1024 * 1024 * sizeof(uint32), 32);

	for(int i = 0; i < 4; i++)
	{
		m_rw_pages[0][i] = GSVector4i::zero();
		m_rw_pages[1][i] = GSVector4i::zero();
		m_tc_pages[i] = GSVector4i::xffffffff();
	}

	#define InitCVB(P) \
		m_cvb[P][0][0] = &GSRendererCL::ConvertVertexBuffer<P, 0, 0>; \
		m_cvb[P][0][1] = &GSRendererCL::ConvertVertexBuffer<P, 0, 1>; \
		m_cvb[P][1][0] = &GSRendererCL::ConvertVertexBuffer<P, 1, 0>; \
		m_cvb[P][1][1] = &GSRendererCL::ConvertVertexBuffer<P, 1, 1>; \

	InitCVB(GS_POINT_CLASS);
	InitCVB(GS_LINE_CLASS);
	InitCVB(GS_TRIANGLE_CLASS);
	InitCVB(GS_SPRITE_CLASS);

	// NOTE: m_cl.vm may be cached on the device according to the specs, there are a couple of places where we access m_mem.m_vm8 without 
	// mapping the buffer (after the two invalidate* calls and in getoutput), it is currently not an issue, but on some devices it may be.

	m_cl.vm = cl::Buffer(m_cl.context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, (size_t)m_mem.m_vmsize, m_mem.m_vm8, NULL);
	m_cl.tex = cl::Buffer(m_cl.context, CL_MEM_READ_WRITE, (size_t)m_mem.m_vmsize);
}

GSRendererCL::~GSRendererCL()
{
	for(size_t i = 0; i < countof(m_texture); i++)
	{
		delete m_texture[i];
	}

	_aligned_free(m_output);
}

void GSRendererCL::Reset()
{
	Sync(-1);

	GSRenderer::Reset();
}

static int pageuploads = 0;
static int pageuploadcount = 0;
static int tfxcount = 0;
static int64 tfxpixels = 0;
static int tfxselcount = 0;
static int tfxdiffselcount = 0;

void GSRendererCL::VSync(int field)
{
	GSRenderer::VSync(field);

	//printf("vsync %d/%d/%d/%d\n", pageuploads, pageuploadcount, tfxcount, tfxpixels);
	//printf("vsync %d/%d\n", tfxselcount, tfxdiffselcount);
	pageuploads = pageuploadcount = tfxcount = tfxpixels = 0;
	tfxselcount = tfxdiffselcount = 0;

	//if(!field) memset(m_mem.m_vm8, 0, (size_t)m_mem.m_vmsize);
}

void GSRendererCL::ResetDevice()
{
	for(size_t i = 0; i < countof(m_texture); i++)
	{
		delete m_texture[i];

		m_texture[i] = NULL;
	}
}

GSTexture* GSRendererCL::GetOutput(int i)
{
	Sync(1);

	const GSRegDISPFB& DISPFB = m_regs->DISP[i].DISPFB;

	int w = DISPFB.FBW * 64;
	int h = GetFrameRect(i).bottom;

	// TODO: round up bottom

	if(m_dev->ResizeTexture(&m_texture[i], w, h))
	{
		static int pitch = 1024 * 4;

		GSVector4i r(0, 0, w, h);

		const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[DISPFB.PSM];

		(m_mem.*psm.rtx)(m_mem.GetOffset(DISPFB.Block(), DISPFB.FBW, DISPFB.PSM), r.ralign<Align_Outside>(psm.bs), m_output, pitch, m_env.TEXA);

		m_texture[i]->Update(r, m_output, pitch);

		if(s_dump)
		{
			if(s_save && s_n >= s_saven)
			{
				m_texture[i]->Save(format("c:\\temp1\\_%05d_f%lld_fr%d_%05x_%d.bmp", s_n, m_perfmon.GetFrame(), i, (int)DISPFB.Block(), (int)DISPFB.PSM));
			}

			s_n++;
		}
	}

	return m_texture[i];
}

const GSVector4 g_pos_scale(1.0f / 16, 1.0f / 16, 1.0f, 1.0f);

template<uint32 primclass, uint32 tme, uint32 fst>
void GSRendererCL::ConvertVertexBuffer(GSVertexCL* RESTRICT dst, const GSVertex* RESTRICT src, size_t count)
{
	GSVector4i off = (GSVector4i)m_context->XYOFFSET;
	GSVector4 st_scale = GSVector4(16 << m_context->TEX0.TW, 16 << m_context->TEX0.TH, 1, 0);

	for(int i = (int)m_vertex.next; i > 0; i--, src++, dst++)
	{
		GSVector4 stcq = GSVector4::load<true>(&src->m[0]); // s t rgba q

		GSVector4i xyzuvf(src->m[1]);

		dst->p = (GSVector4(xyzuvf.upl16() - off) * g_pos_scale).xyxy(GSVector4::cast(xyzuvf.ywyw())); // pass zf as uints

		GSVector4 t = GSVector4::zero();

		if(tme)
		{
			if(fst)
			{
				#if _M_SSE >= 0x401

				t = GSVector4(xyzuvf.uph16());
					
				#else

				t = GSVector4(GSVector4i::load(src->UV).upl16());

				#endif
			}
			else
			{
				t = stcq.xyww() * st_scale;
			}
		}

		dst->t = t.insert32<2, 3>(stcq); // color as uchar4 in t.w
	}
}

void GSRendererCL::Draw()
{
	const GSDrawingContext* context = m_context;

	GSVector4i scissor = GSVector4i(context->scissor.in);
	GSVector4i bbox = GSVector4i(m_vt.m_min.p.floor().xyxy(m_vt.m_max.p.ceil()));

	// points and lines may have zero area bbox (example: single line 0,0->256,0)

	if(m_vt.m_primclass == GS_POINT_CLASS || m_vt.m_primclass == GS_LINE_CLASS)
	{
		if(bbox.x == bbox.z) bbox.z++;
		if(bbox.y == bbox.w) bbox.w++;
	}

	scissor.z = std::min<int>(scissor.z, (int)context->FRAME.FBW * 64); // TODO: find a game that overflows and check which one is the right behaviour

	GSVector4i rect = bbox.rintersect(scissor);

	if(rect.rempty())
	{
		return;
	}

	if(s_dump)
	{
		Sync(2);

		uint64 frame = m_perfmon.GetFrame();

		std::string s;

		if(s_save && s_n >= s_saven && PRIM->TME)
		{
			s = format("c:\\temp1\\_%05d_f%lld_tex_%05x_%d.bmp", s_n, frame, (int)m_context->TEX0.TBP0, (int)m_context->TEX0.PSM);

			m_mem.SaveBMP(s, m_context->TEX0.TBP0, m_context->TEX0.TBW, m_context->TEX0.PSM, 1 << m_context->TEX0.TW, 1 << m_context->TEX0.TH);
		}

		s_n++;

		if(s_save && s_n >= s_saven)
		{
			s = format("c:\\temp1\\_%05d_f%lld_rt0_%05x_%d.bmp", s_n, frame, m_context->FRAME.Block(), m_context->FRAME.PSM);

			m_mem.SaveBMP(s, m_context->FRAME.Block(), m_context->FRAME.FBW, m_context->FRAME.PSM, GetFrameRect().width(), 512);
		}

		if(s_savez && s_n >= s_saven)
		{
			s = format("c:\\temp1\\_%05d_f%lld_rz0_%05x_%d.bmp", s_n, frame, m_context->ZBUF.Block(), m_context->ZBUF.PSM);

			m_mem.SaveBMP(s, m_context->ZBUF.Block(), m_context->FRAME.FBW, m_context->ZBUF.PSM, GetFrameRect().width(), 512);
		}

		s_n++;
	}

	try
	{
		size_t vb_size = m_vertex.next * sizeof(GSVertexCL);
		size_t ib_size = m_index.tail * sizeof(uint32);
		size_t pb_size = TFX_PARAM_SIZE;

		ASSERT(sizeof(TFXParameter) <= TFX_PARAM_SIZE);

		if(m_cl.vb.tail + vb_size > m_cl.vb.size || m_cl.ib.tail + ib_size > m_cl.ib.size || m_cl.pb.tail + pb_size > m_cl.pb.size)
		{
			if(vb_size > m_cl.vb.size || ib_size > m_cl.ib.size)
			{
				// buffer too small for even one batch, allow twice the size (at least 1 MB)

				Sync(2); // must sync, reallocating the input buffers

				m_cl.Unmap();

				m_cl.vb.size = 0;
				m_cl.ib.size = 0;

				size_t size = std::max(vb_size * 2, (size_t)2 << 20);

				printf("growing vertex/index buffer %d\n", size);

				m_cl.vb.buff[0] = cl::Buffer(m_cl.context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, size);
				m_cl.vb.buff[1] = cl::Buffer(m_cl.context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, size);
				m_cl.vb.size = size;

				size = std::max(size / sizeof(GSVertex) * 3 * sizeof(uint32), (size_t)1 << 20); // worst case, three times the vertex count

				ASSERT(size >= ib_size);

				if(size < ib_size) size = ib_size; // should not happen

				m_cl.ib.buff[0] = cl::Buffer(m_cl.context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, size);
				m_cl.ib.buff[1] = cl::Buffer(m_cl.context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, size);
				m_cl.ib.size = size;
			}
			else
			{
				Enqueue();

				m_cl.Unmap();

				// make the write queue wait until the rendering queue is ready, it may still use the device buffers

				std::vector<cl::Event> el(1);

				m_cl.queue[2].enqueueMarker(&el[0]);
				m_cl.wq->enqueueWaitForEvents(el);

				// switch to the other queue/buffer (double buffering)

				m_cl.wqidx = (m_cl.wqidx + 1) & 1;
				m_cl.wq = &m_cl.queue[m_cl.wqidx];
			}

			m_cl.vb.head = m_cl.vb.tail = 0;
			m_cl.ib.head = m_cl.ib.tail = 0;
			m_cl.pb.head = m_cl.pb.tail = 0;

			m_cl.Map();
		}
		else
		{
			// only allow batches of the same primclass in Enqueue

			if(!m_jobs.empty() && m_jobs.front()->sel.prim != (uint32)m_vt.m_primclass)
			{
				Enqueue();
			}
		}

		//

		GSVertexCL* vb = (GSVertexCL*)(m_cl.vb.ptr + m_cl.vb.tail);
		uint32* ib = (uint32*)(m_cl.ib.ptr + m_cl.ib.tail);
		TFXParameter* pb = (TFXParameter*)(m_cl.pb.ptr + m_cl.pb.tail);

		(this->*m_cvb[m_vt.m_primclass][PRIM->TME][PRIM->FST])(vb, m_vertex.buff, m_vertex.next); // TODO: upload in GSVertex format and extract the fields in the kernel? 

		if(m_jobs.empty())
		{
			memcpy(ib, m_index.buff, m_index.tail * sizeof(uint32));

			m_vb_start = m_cl.vb.tail;
			m_vb_count = 0;
			m_pb_start = m_cl.pb.tail;
			m_pb_count = 0;
		}
		else
		{
			// TODO: SIMD

			ASSERT(m_pb_count < 256);

			uint32 vb_count = m_vb_count | (m_pb_count << 24);

			for(size_t i = 0; i < m_index.tail; i++)
			{
				ib[i] = m_index.buff[i] + vb_count;
			}
		}

		shared_ptr<TFXJob> job(new TFXJob());

		if(!SetupParameter(job.get(), pb, vb, m_vertex.next, m_index.buff, m_index.tail))
		{
			return;
		}

		pb->scissor = scissor;

		if(bbox.eq(bbox.rintersect(scissor)))
		{
			job->sel.noscissor = 1;
		}

		job->rect.x = rect.x;
		job->rect.y = rect.y;
		job->rect.z = rect.z;
		job->rect.w = rect.w;
		job->ib_start = m_cl.ib.tail;
		job->prim_count = m_index.tail / GSUtil::GetClassVertexCount(m_vt.m_primclass);
		job->fbp = pb->fbp;
		job->zbp = pb->zbp;
		job->bw = pb->bw;

#ifdef DEBUG
		job->pb = pb;
#endif
		m_jobs.push_back(job);

		m_vb_count += m_vertex.next;
		m_pb_count++;

		m_cl.vb.tail += vb_size;
		m_cl.ib.tail += ib_size;
		m_cl.pb.tail += pb_size;

		m_synced = false;

		// mark pages used in rendering as source or target

		if(job->sel.fwrite || job->sel.rfb)
		{
			m_context->offset.fb->GetPagesAsBits(rect, m_tmp_pages);

			if(job->sel.rfb)
			{
				for(int i = 0; i < 4; i++)
				{
					m_rw_pages[0][i] |= m_tmp_pages[i];
				}
			}

			if(job->sel.fwrite)
			{
				GSVector4i* dst_pages = job->GetDstPages();

				for(int i = 0; i < 4; i++)
				{
					m_rw_pages[1][i] |= m_tmp_pages[i];

					dst_pages[i] |= m_tmp_pages[i];
				}
			}
		}

		if(job->sel.zwrite || job->sel.rzb)
		{
			m_context->offset.zb->GetPagesAsBits(rect, m_tmp_pages);

			if(job->sel.rzb)
			{
				for(int i = 0; i < 4; i++)
				{
					m_rw_pages[0][i] |= m_tmp_pages[i];
				}
			}

			if(job->sel.zwrite)
			{
				GSVector4i* dst_pages = job->GetDstPages();

				for(int i = 0; i < 4; i++)
				{
					m_rw_pages[1][i] |= m_tmp_pages[i];

					dst_pages[i] |= m_tmp_pages[i];
				}
			}
		}

		if(job->src_pages != NULL)
		{
			for(int i = 0; i < 4; i++)
			{
				m_rw_pages[0][i] |= job->src_pages[i];

				if(job->dst_pages != NULL && !(job->dst_pages[i] & job->src_pages[i]).eq(GSVector4i::zero()))
				{
					//printf("src and dst overlap!\n");
				}
			}
		}

		// don't buffer too much data, feed them to the device if there is enough

		if(m_cl.vb.tail - m_cl.vb.head >= 256 * 4096 || m_jobs.size() >= 64)
		{
			Enqueue();
		}
	}
	catch(cl::Error err)
	{
		printf("%s (%d)\n", err.what(), err.err());

		return;
	}
	catch(std::exception err)
	{
		printf("%s\n", err.what());

		return;
	}

	if(s_dump)
	{
		Sync(2);

		uint64 frame = m_perfmon.GetFrame();
		
		std::string s;

		if(s_save && s_n >= s_saven)
		{
			s = format("c:\\temp1\\_%05d_f%lld_rt1_%05x_%d.bmp", s_n, frame, m_context->FRAME.Block(), m_context->FRAME.PSM);

			m_mem.SaveBMP(s, m_context->FRAME.Block(), m_context->FRAME.FBW, m_context->FRAME.PSM, GetFrameRect().width(), 512);
		}

		if(s_savez && s_n >= s_saven)
		{
			s = format("c:\\temp1\\_%05d_f%lld_rz1_%05x_%d.bmp", s_n, frame, m_context->ZBUF.Block(), m_context->ZBUF.PSM);

			m_mem.SaveBMP(s, m_context->ZBUF.Block(), m_context->FRAME.FBW, m_context->ZBUF.PSM, GetFrameRect().width(), 512);
		}

		s_n++;
	}
}

void GSRendererCL::Sync(int reason)
{
	if(LOG) { fprintf(s_fp, "Sync (%d)\n", reason); fflush(s_fp); }

	//printf("sync %d\n", reason);

	GSPerfMonAutoTimer pmat(&m_perfmon, GSPerfMon::Sync);

	Enqueue();

	m_cl.queue[2].finish();

	for(int i = 0; i < 4; i++)
	{
		m_rw_pages[0][i] = GSVector4i::zero();
		m_rw_pages[1][i] = GSVector4i::zero();
	}

	m_synced = true;
}

void GSRendererCL::InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r)
{
	if(LOG) {fprintf(s_fp, "w %05x %d %d, %d %d %d %d\n", BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM, r.x, r.y, r.z, r.w); fflush(s_fp);}
	
	GSOffset* off = m_mem.GetOffset(BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM);

	off->GetPagesAsBits(r, m_tmp_pages);

	if(!m_synced)
	{
		for(int i = 0; i < 4; i++)
		{
			GSVector4i pages = m_rw_pages[0][i] | m_rw_pages[1][i];

			if(!(pages & m_tmp_pages[i]).eq(GSVector4i::zero()))
			{
				// TODO: an awesome idea to avoid this Sync
				// - call Enqueue() to flush m_jobs 
				// - append rendering queue with a kernel that writes the incoming data to m_mem.vm and tell the parent class to not do it
				// - the only problem, clut has to be read directly by the texture sampler, can't attach it to gs_param before being written

				Sync(3); 

				break;
			}
		}
	}

	for(int i = 0; i < 4; i++)
	{
		m_tc_pages[i] |= m_tmp_pages[i];
	}
}

void GSRendererCL::InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut)
{
	if(LOG) {fprintf(s_fp, "%s %05x %d %d, %d %d %d %d\n", clut ? "rp" : "r", BITBLTBUF.SBP, BITBLTBUF.SBW, BITBLTBUF.SPSM, r.x, r.y, r.z, r.w); fflush(s_fp);}
	
	if(!m_synced)
	{
		GSOffset* off = m_mem.GetOffset(BITBLTBUF.SBP, BITBLTBUF.SBW, BITBLTBUF.SPSM);

		off->GetPagesAsBits(r, m_tmp_pages);

		for(int i = 0; i < 4; i++)
		{
			GSVector4i pages = m_rw_pages[1][i];

			if(!(pages & m_tmp_pages[i]).eq(GSVector4i::zero()))
			{
				Sync(4);

				break;
			}
		}
	}
}

void GSRendererCL::Enqueue()
{
	if(m_jobs.empty()) return;

	try
	{
		ASSERT(m_cl.vb.tail > m_cl.vb.head);
		ASSERT(m_cl.ib.tail > m_cl.ib.head);
		ASSERT(m_cl.pb.tail > m_cl.pb.head);

		int primclass = m_jobs.front()->sel.prim;

		uint32 n = GSUtil::GetClassVertexCount(primclass);

		PrimSelector psel;

		psel.key = 0;
		psel.prim = primclass;

		cl::Kernel& pk = m_cl.GetPrimKernel(psel);

		pk.setArg(1, m_cl.vb.buff[m_cl.wqidx]);
		pk.setArg(2, m_cl.ib.buff[m_cl.wqidx]);
		pk.setArg(3, m_cl.pb.buff[m_cl.wqidx]);
		pk.setArg(4, (cl_uint)m_vb_start);
		pk.setArg(6, (cl_uint)m_pb_start);

		TileSelector tsel;

		tsel.key = 0;
		tsel.prim = primclass;

		tsel.mode = 0;

		cl::Kernel& tk_32 = m_cl.GetTileKernel(tsel);

		tsel.mode = 1;

		cl::Kernel& tk_16 = m_cl.GetTileKernel(tsel);

		tsel.mode = 2;

		cl::Kernel& tk_8 = m_cl.GetTileKernel(tsel);

		tsel.mode = 3;

		cl::Kernel& tk = m_cl.GetTileKernel(tsel);

		tsel.key = 0;
		tsel.clear = 1;

		cl::Kernel& tk_clear = m_cl.GetTileKernel(tsel);

		//

		m_cl.Unmap();

		std::vector<cl::Event> el(1);

		m_cl.wq->enqueueMarker(&el[0]);
		m_cl.queue[2].enqueueWaitForEvents(el);

		//

		auto head = m_jobs.begin();

		while(head != m_jobs.end())
		{
			uint32 total_prim_count = 0;

			auto next = head;

			while(next != m_jobs.end())
			{
				auto job = next++;

				uint32 cur_prim_count = (*job)->prim_count;
				uint32 next_prim_count = next != m_jobs.end() ? (*next)->prim_count : 0;

				total_prim_count += cur_prim_count;

				if(total_prim_count >= MAX_PRIM_COUNT || next == m_jobs.end())// || next_prim_count >= MAX_PRIM_COUNT || next_prim_count < 16 && total_prim_count >= MAX_PRIM_COUNT / 2)
				{
					uint32 prim_count = std::min(total_prim_count, MAX_PRIM_COUNT);					

					pk.setArg(5, (cl_uint)(*head)->ib_start);

					m_cl.queue[2].enqueueNDRangeKernel(pk, cl::NullRange, cl::NDRange(prim_count), cl::NullRange);

					if(0)
					{
						gs_env* ptr = (gs_env*)m_cl.queue[2].enqueueMapBuffer(m_cl.env, CL_TRUE, CL_MAP_READ, 0, sizeof(gs_env));
						m_cl.queue[2].enqueueUnmapMemObject(m_cl.env, ptr);
					}

					GSVector4i rect = GSVector4i::zero();

					for(auto i = head; i != next; i++)
					{
						rect = rect.runion(GSVector4i::load<false>(&(*i)->rect));
					}

					rect = rect.ralign<Align_Outside>(GSVector2i(BIN_SIZE, BIN_SIZE)) >> BIN_SIZE_BITS;

					int bin_w = rect.width();
					int bin_h = rect.height();

					uint32 batch_count = BATCH_COUNT(prim_count);
					uint32 bin_count = bin_w * bin_h;

					cl_uchar4 bin_dim;

					bin_dim.s[0] = (cl_uchar)rect.x;
					bin_dim.s[1] = (cl_uchar)rect.y;
					bin_dim.s[2] = (cl_uchar)bin_w;
					bin_dim.s[3] = (cl_uchar)bin_h;

					if(1)//bin_w > 1 || bin_h > 1) // && not just one sprite covering the whole area
					{
						m_cl.queue[2].enqueueNDRangeKernel(tk_clear, cl::NullRange, cl::NDRange(bin_count), cl::NullRange);

						if(bin_count <= 32 && m_cl.WIs >= 256)
						{
							uint32 item_count;
							uint32 group_count;
							cl::Kernel* k;

							if(bin_count <= 8)
							{
								item_count = std::min(prim_count, 32u);
								group_count = ((prim_count + 31) >> 5) * item_count;
								k = &tk_32;
							}
							else if(bin_count <= 16)
							{
								item_count = std::min(prim_count, 16u);
								group_count = ((prim_count + 15) >> 4) * item_count;
								k = &tk_16;
							}
							else
							{
								item_count = std::min(prim_count, 8u);
								group_count = ((prim_count + 7) >> 3) * item_count;
								k = &tk_8;
							}

							k->setArg(1, (cl_uint)prim_count);
							k->setArg(2, (cl_uint)bin_count);
							k->setArg(3, bin_dim);

							m_cl.queue[2].enqueueNDRangeKernel(*k, cl::NullRange, cl::NDRange(bin_w, bin_h, group_count), cl::NDRange(bin_w, bin_h, item_count));
						}
						else
						{
							uint32 item_count = std::min(bin_count, m_cl.WIs);
							uint32 group_count = batch_count * item_count;

							tk.setArg(1, (cl_uint)prim_count);
							tk.setArg(2, (cl_uint)bin_count);
							tk.setArg(3, bin_dim);

							m_cl.queue[2].enqueueNDRangeKernel(tk, cl::NullRange, cl::NDRange(group_count), cl::NDRange(item_count));
						}

						if(0)
						{
							gs_env* ptr = (gs_env*)m_cl.queue[2].enqueueMapBuffer(m_cl.env, CL_TRUE, CL_MAP_READ, 0, sizeof(gs_env));
							m_cl.queue[2].enqueueUnmapMemObject(m_cl.env, ptr);
						}
					}

					std::list<shared_ptr<TFXJob>> jobs(head, next);

					EnqueueTFX(jobs, bin_count, bin_dim);

					if(total_prim_count > MAX_PRIM_COUNT)
					{
						prim_count = cur_prim_count - (total_prim_count - MAX_PRIM_COUNT);

						(*job)->ib_start += prim_count * n * sizeof(uint32);
						(*job)->prim_count -= prim_count;

						next = job; // try again for the remainder

						//printf("split %d\n", (*job)->prim_count);
					}

					break;
				}
			}

			head = next;
		}
	}
	catch(cl::Error err)
	{
		printf("%s (%d)\n", err.what(), err.err());
	}

	m_jobs.clear();

	m_vb_count = 0;

	m_cl.vb.head = m_cl.vb.tail;
	m_cl.ib.head = m_cl.ib.tail;
	m_cl.pb.head = m_cl.pb.tail;

	m_cl.Map();
}

void GSRendererCL::EnqueueTFX(std::list<shared_ptr<TFXJob>>& jobs, uint32 bin_count, const cl_uchar4& bin_dim)
{
	// join tfx kernel calls where the selector and fbp/zbp/bw are the same and src_pages != prev dst_pages

	//printf("before\n"); for(auto i : jobs) printf("%016llx %05x %05x %d %d %d\n", i->sel.key, i->fbp, i->zbp, i->bw, i->prim_count, i->ib_start);

	auto next = jobs.begin();

	while(next != jobs.end())
	{
		auto prev = next++;

		if(next == jobs.end())
		{
			break;
		}

		if((*prev)->sel == (*next)->sel && (*prev)->fbp == (*next)->fbp && (*prev)->zbp == (*next)->zbp && (*prev)->bw == (*next)->bw)
		{
			if((*prev)->dst_pages != NULL && (*next)->src_pages != NULL)
			{
				bool overlap = false;

				for(int i = 0; i < 4; i++)
				{
					if(!((*prev)->dst_pages[i] & (*next)->src_pages[i]).eq(GSVector4i::zero()))
					{
						overlap = true;

						break;
					}
				}

				if(overlap)
				{
					continue;
				}
			}

			if((*prev)->src_pages != NULL)
			{
				GSVector4i* src_pages = (*next)->GetSrcPages();

				for(int i = 0; i < 4; i++)
				{
					src_pages[i] |= (*prev)->src_pages[i];
				}
			}

			if((*prev)->dst_pages != NULL)
			{
				GSVector4i* dst_pages = (*next)->GetDstPages();

				for(int i = 0; i < 4; i++)
				{
					dst_pages[i] |= (*prev)->dst_pages[i];
				}
			}

			GSVector4i prev_rect = GSVector4i::load<false>(&(*prev)->rect);
			GSVector4i next_rect = GSVector4i::load<false>(&(*next)->rect);

			GSVector4i::store<false>(&(*next)->rect, prev_rect.runion(next_rect));

			(*next)->prim_count += (*prev)->prim_count;
			(*next)->ib_start = (*prev)->ib_start;

			jobs.erase(prev);
		}
	}

	//printf("after\n"); for(auto i : jobs) printf("%016llx %05x %05x %d %d %d\n", i->sel.key, i->fbp, i->zbp, i->bw, i->prim_count, i->ib_start);

	//

	cl_kernel tfx_prev = NULL;

	uint32 prim_start = 0;

	for(auto i : jobs)
	{
		ASSERT(prim_start < MAX_PRIM_COUNT);

		tfxcount++;

		UpdateTextureCache(i.get());

		uint32 prim_count = std::min(i->prim_count, MAX_PRIM_COUNT - prim_start);

		// TODO: tile level z test

		cl::Kernel& tfx = m_cl.GetTFXKernel(i->sel);

		if(tfx_prev != tfx())
		{
			tfx.setArg(3, sizeof(m_cl.pb.buff[m_cl.wqidx]), &m_cl.pb.buff[m_cl.wqidx]);
			tfx.setArg(4, (cl_uint)m_pb_start);

			tfx_prev = tfx();
		}

		tfx.setArg(5, (cl_uint)prim_start);
		tfx.setArg(6, (cl_uint)prim_count);
		tfx.setArg(7, (cl_uint)bin_count);
		tfx.setArg(8, bin_dim);
		tfx.setArg(9, i->fbp);
		tfx.setArg(10, i->zbp);
		tfx.setArg(11, i->bw);

		GSVector4i r = GSVector4i::load<false>(&i->rect);

		r = r.ralign<Align_Outside>(GSVector2i(8, 8));

		m_cl.queue[2].enqueueNDRangeKernel(tfx, cl::NDRange(r.left, r.top), cl::NDRange(r.width(), r.height()), cl::NDRange(8, 8));

		tfxpixels += r.width() * r.height();

		InvalidateTextureCache(i.get());

		// TODO: partial job renderings (>MAX_PRIM_COUNT) may invalidate pages unnecessarily

		prim_start += prim_count;
	}
}

void GSRendererCL::UpdateTextureCache(TFXJob* job)
{
	if(job->src_pages == NULL) return;

	int count = 0;

	for(int i = 0; i < 4; i++)
	{
		GSVector4i pages = m_tc_pages[i] & job->src_pages[i];

		if(pages.eq(GSVector4i::zero())) continue;

		size_t page_size = 8192;

		// TODO: only use the texture cache if there is an overlap between src_pages and dst_pages? (or if already uploaded)

		if(0) for(int j = 0; j < 4; j++)
		{
			if(pages.u32[j] == 0) continue;

			if(pages.u32[j] == 0xffffffff)
			{
				size_t offset = (i * sizeof(GSVector4i) + j * sizeof(uint32)) * 8 * page_size;

				m_cl.queue[2].enqueueCopyBuffer(m_cl.vm, m_cl.tex, offset, offset, page_size * 32);

				if(LOG) { fprintf(s_fp, "tc (%d x32)\n", offset >> 13); fflush(s_fp); }

				pageuploadcount++;
				count += 32;

				continue;
			}

			for(int k = 0; k < 4; k++)
			{
				uint8 b = pages.u8[j * 4 + k];

				if(b == 0) continue;

				if(b == 0xff)
				{
					size_t offset = (i * sizeof(GSVector4i) + (j * 4 + k)) * 8 * page_size;

					m_cl.queue[2].enqueueCopyBuffer(m_cl.vm, m_cl.tex, offset, offset, page_size * 8);

					if(LOG) { fprintf(s_fp, "tc (%d x8)\n", offset >> 13); fflush(s_fp); }

					pageuploadcount++;
					count += 8;

					continue;
				}

				for(int l = 0; l < 8; l++)
				{
					if(b & (1 << l))
					{
						size_t offset = ((i * sizeof(GSVector4i) + (j * 4 + k)) * 8 + l) * page_size;

						m_cl.queue[2].enqueueCopyBuffer(m_cl.vm, m_cl.tex, offset, offset, page_size);

						if(LOG) { fprintf(s_fp, "tc (%d x1)\n", offset >> 13); fflush(s_fp); }

						pageuploadcount++;
						count++;
					}
				}
			}
		}

		m_tc_pages[i] &= ~job->src_pages[i];
	}

	if(count > 0)
	{
		pageuploads += count;
	}
}

void GSRendererCL::InvalidateTextureCache(TFXJob* job)
{
	if(job->dst_pages == NULL) return;

	for(int j = 0; j < 4; j++)
	{
		m_tc_pages[j] |= job->dst_pages[j];
	}
}

static int RemapPSM(int psm)
{
	switch(psm)
	{
	default:
	case PSM_PSMCT32: psm = 0; break;
	case PSM_PSMCT24: psm = 1; break;
	case PSM_PSMCT16: psm = 2; break;
	case PSM_PSMCT16S: psm = 3; break;
	case PSM_PSMZ32: psm = 4; break;
	case PSM_PSMZ24: psm = 5; break;
	case PSM_PSMZ16: psm = 6; break;
	case PSM_PSMZ16S: psm = 7; break;
	case PSM_PSMT8: psm = 8; break;
	case PSM_PSMT4: psm = 9; break;
	case PSM_PSMT8H: psm = 10; break;
	case PSM_PSMT4HL: psm = 11; break;
	case PSM_PSMT4HH: psm = 12; break;
	}

	return psm;
}

bool GSRendererCL::SetupParameter(TFXJob* job, TFXParameter* pb, GSVertexCL* vertex, size_t vertex_count, const uint32* index, size_t index_count)
{
	const GSDrawingEnvironment& env = m_env;
	const GSDrawingContext* context = m_context;
	const GS_PRIM_CLASS primclass = m_vt.m_primclass;

	job->sel.key = 0;

	job->sel.atst = ATST_ALWAYS;
	job->sel.tfx = TFX_NONE;
	job->sel.ababcd = 0xff;
	job->sel.prim = primclass;

	uint32 fm = context->FRAME.FBMSK;
	uint32 zm = context->ZBUF.ZMSK || context->TEST.ZTE == 0 ? 0xffffffff : 0;

	if(context->TEST.ZTE && context->TEST.ZTST == ZTST_NEVER)
	{
		fm = 0xffffffff;
		zm = 0xffffffff;
	}

	if(PRIM->TME)
	{
		if(GSLocalMemory::m_psm[context->TEX0.PSM].pal > 0)
		{
			m_mem.m_clut.Read32(context->TEX0, env.TEXA);
		}
	}

	if(context->TEST.ATE)
	{
		if(!TryAlphaTest(fm, zm))
		{
			job->sel.atst = context->TEST.ATST;
			job->sel.afail = context->TEST.AFAIL;
			pb->aref = context->TEST.AREF;
		}
	}

	bool fwrite;
	bool zwrite = zm != 0xffffffff;
	
	switch(context->FRAME.PSM)
	{
	default:
	case PSM_PSMCT32:
	case PSM_PSMZ32:
		fwrite = fm != 0xffffffff;
		break;
	case PSM_PSMCT24:
	case PSM_PSMZ24:
		fwrite = (fm & 0x00ffffff) != 0x00ffffff;
		break;
	case PSM_PSMCT16:
	case PSM_PSMCT16S:
	case PSM_PSMZ16:
	case PSM_PSMZ16S:
		fwrite = (fm & 0x80f8f8f8) != 0x80f8f8f8;
		break;
	}

	if(!fwrite && !zwrite) return false;

	bool ftest = job->sel.atst != ATST_ALWAYS || context->TEST.DATE && context->FRAME.PSM != PSM_PSMCT24;
	bool ztest = context->TEST.ZTE && context->TEST.ZTST > ZTST_ALWAYS;

	job->sel.fwrite = fwrite;
	job->sel.ftest = ftest;
	job->sel.zwrite = zwrite;
	job->sel.ztest = ztest;

	if(fwrite || ftest)
	{
		job->sel.fpsm = RemapPSM(context->FRAME.PSM);

		if((primclass == GS_LINE_CLASS || primclass == GS_TRIANGLE_CLASS) && m_vt.m_eq.rgba != 0xffff)
		{
			job->sel.iip = PRIM->IIP;
		}

		if(PRIM->TME)
		{
			job->sel.tfx = context->TEX0.TFX;
			job->sel.tcc = context->TEX0.TCC;
			job->sel.fst = PRIM->FST;
			job->sel.ltf = m_vt.IsLinear();
			job->sel.tpsm = RemapPSM(context->TEX0.PSM);
			job->sel.aem = m_env.TEXA.AEM;

			pb->tbp[0] = context->TEX0.TBP0;
			pb->tbw[0] = context->TEX0.TBW;
			pb->ta0 = m_env.TEXA.TA0;
			pb->ta1 = m_env.TEXA.TA1;

			if(GSLocalMemory::m_psm[context->TEX0.PSM].pal > 0)
			{
				job->sel.tlu = 1;

				memcpy(pb->clut, (const uint32*)m_mem.m_clut, sizeof(uint32) * GSLocalMemory::m_psm[context->TEX0.PSM].pal);
			}

			job->sel.wms = context->CLAMP.WMS;
			job->sel.wmt = context->CLAMP.WMT;

			if(job->sel.tfx == TFX_MODULATE && job->sel.tcc && m_vt.m_eq.rgba == 0xffff && m_vt.m_min.c.eq(GSVector4i(128)))
			{
				// modulate does not do anything when vertex color is 0x80

				job->sel.tfx = TFX_DECAL;
			}

			GSVector4i r;

			GetTextureMinMax(r, context->TEX0, context->CLAMP, job->sel.ltf);

			GSVector4i* src_pages = job->GetSrcPages();

			GSOffset* off = m_mem.GetOffset(context->TEX0.TBP0, context->TEX0.TBW, context->TEX0.PSM);
			
			off->GetPagesAsBits(r, m_tmp_pages);

			for(int i = 0; i < 4; i++)
			{
				src_pages[i] |= m_tmp_pages[i];
			}

			if(m_mipmap && context->TEX1.MXL > 0 && context->TEX1.MMIN >= 2 && context->TEX1.MMIN <= 5 && m_vt.m_lod.y > 0)
			{
				// TEX1.MMIN
				// 000 p
				// 001 l
				// 010 p round
				// 011 p tri
				// 100 l round
				// 101 l tri

				if(m_vt.m_lod.x > 0)
				{
					job->sel.ltf = context->TEX1.MMIN >> 2;
				}
				else
				{
					// TODO: isbilinear(mmag) != isbilinear(mmin) && m_vt.m_lod.x <= 0 && m_vt.m_lod.y > 0
				}

				job->sel.mmin = (context->TEX1.MMIN & 1) + 1; // 1: round, 2: tri
				job->sel.lcm = context->TEX1.LCM;

				int mxl = std::min<int>((int)context->TEX1.MXL, 6) << 16;
				int k = context->TEX1.K << 12;

				if((int)m_vt.m_lod.x >= (int)context->TEX1.MXL)
				{
					k = (int)m_vt.m_lod.x << 16; // set lod to max level

					job->sel.lcm = 1; // lod is constant
					job->sel.mmin = 1; // tri-linear is meaningless
				}

				if(job->sel.mmin == 2)
				{
					mxl--; // don't sample beyond the last level (TODO: add a dummy level instead?)
				}

				if(job->sel.fst)
				{
					ASSERT(job->sel.lcm == 1);
					ASSERT(((m_vt.m_min.t.uph(m_vt.m_max.t) == GSVector4::zero()).mask() & 3) == 3); // ratchet and clank (menu)

					job->sel.lcm = 1;
				}

				if(job->sel.lcm)
				{
					int lod = std::max<int>(std::min<int>(k, mxl), 0);

					if(job->sel.mmin == 1)
					{
						lod = (lod + 0x8000) & 0xffff0000; // rounding
					}

					pb->lod = lod;

					// TODO: lot to optimize when lod is constant
				}
				else
				{
					pb->mxl = mxl;
					pb->l = (float)(-0x10000 << context->TEX1.L);
					pb->k = (float)k;
				}

				GIFRegTEX0 MIP_TEX0 = context->TEX0;
				GIFRegCLAMP MIP_CLAMP = context->CLAMP;

				GSVector4 tmin = m_vt.m_min.t;
				GSVector4 tmax = m_vt.m_max.t;

				static int s_counter = 0;

				for(int i = 1, j = std::min<int>((int)context->TEX1.MXL, 6); i <= j; i++)
				{
					switch(i)
					{
					case 1:
						MIP_TEX0.TBP0 = context->MIPTBP1.TBP1;
						MIP_TEX0.TBW = context->MIPTBP1.TBW1;
						break;
					case 2:
						MIP_TEX0.TBP0 = context->MIPTBP1.TBP2;
						MIP_TEX0.TBW = context->MIPTBP1.TBW2;
						break;
					case 3:
						MIP_TEX0.TBP0 = context->MIPTBP1.TBP3;
						MIP_TEX0.TBW = context->MIPTBP1.TBW3;
						break;
					case 4:
						MIP_TEX0.TBP0 = context->MIPTBP2.TBP4;
						MIP_TEX0.TBW = context->MIPTBP2.TBW4;
						break;
					case 5:
						MIP_TEX0.TBP0 = context->MIPTBP2.TBP5;
						MIP_TEX0.TBW = context->MIPTBP2.TBW5;
						break;
					case 6:
						MIP_TEX0.TBP0 = context->MIPTBP2.TBP6;
						MIP_TEX0.TBW = context->MIPTBP2.TBW6;
						break;
					default:
						__assume(0);
					}

					pb->tbp[i] = MIP_TEX0.TBP0;
					pb->tbw[i] = MIP_TEX0.TBW;

					if(MIP_TEX0.TW > 0) MIP_TEX0.TW--;
					if(MIP_TEX0.TH > 0) MIP_TEX0.TH--;

					MIP_CLAMP.MINU >>= 1;
					MIP_CLAMP.MINV >>= 1;
					MIP_CLAMP.MAXU >>= 1;
					MIP_CLAMP.MAXV >>= 1;

					m_vt.m_min.t *= 0.5f;
					m_vt.m_max.t *= 0.5f;

					GSVector4i r;

					GetTextureMinMax(r, MIP_TEX0, MIP_CLAMP, job->sel.ltf);

					GSOffset* off = m_mem.GetOffset(MIP_TEX0.TBP0, MIP_TEX0.TBW, MIP_TEX0.PSM);
					
					off->GetPagesAsBits(r, m_tmp_pages);

					for(int i = 0; i < 4; i++)
					{
						src_pages[i] |= m_tmp_pages[i];
					}
				}

				s_counter++;

				m_vt.m_min.t = tmin;
				m_vt.m_max.t = tmax;
			}
			else
			{
				if(job->sel.fst == 0)
				{
					// skip per pixel division if q is constant

					GSVertexCL* RESTRICT v = vertex;

					if(m_vt.m_eq.q)
					{
						job->sel.fst = 1;

						const GSVector4& t = v[index[0]].t;

						if(t.z != 1.0f)
						{
							GSVector4 w = t.zzzz().rcpnr();

							for(int i = 0, j = vertex_count; i < j; i++)
							{
								GSVector4 t = v[i].t;

								v[i].t = (t * w).xyzw(t);
							}
						}
					}
					else if(primclass == GS_SPRITE_CLASS)
					{
						job->sel.fst = 1;

						for(int i = 0, j = vertex_count; i < j; i += 2)
						{
							GSVector4 t0 = v[i + 0].t;
							GSVector4 t1 = v[i + 1].t;

							GSVector4 w = t1.zzzz().rcpnr();

							v[i + 0].t = (t0 * w).xyzw(t0);
							v[i + 1].t = (t1 * w).xyzw(t1);
						}
					}
				}

				if(job->sel.ltf && job->sel.fst) // TODO: quite slow, do this in the prim kernel?
				{
					// if q is constant we can do the half pel shift for bilinear sampling on the vertices

					// TODO: but not when mipmapping is used!!!

					GSVector4 half(8.0f, 8.0f);

					GSVertexCL* RESTRICT v = vertex;

					for(int i = 0, j = vertex_count; i < j; i++)
					{
						GSVector4 t = v[i].t;

						v[i].t = (t - half).xyzw(t);
					}
				}
			}

			int tw = 1 << context->TEX0.TW;
			int th = 1 << context->TEX0.TH;

			switch(context->CLAMP.WMS)
			{
			case CLAMP_REPEAT:
				pb->minu = tw - 1;
				pb->maxu = 0;
				//gd.t.mask.u32[0] = 0xffffffff;
				break;
			case CLAMP_CLAMP:
				pb->minu = 0;
				pb->maxu = tw - 1;
				//gd.t.mask.u32[0] = 0;
				break;
			case CLAMP_REGION_CLAMP:
				pb->minu = std::min((int)context->CLAMP.MINU, tw - 1);
				pb->maxu = std::min((int)context->CLAMP.MAXU, tw - 1);
				//gd.t.mask.u32[0] = 0;
				break;
			case CLAMP_REGION_REPEAT:
				pb->minu = (int)context->CLAMP.MINU & (tw - 1);
				pb->maxu = (int)context->CLAMP.MAXU & (tw - 1);
				//gd.t.mask.u32[0] = 0xffffffff;
				break;
			default:
				__assume(0);
			}

			switch(context->CLAMP.WMT)
			{
			case CLAMP_REPEAT:
				pb->minv = th - 1;
				pb->maxv = 0;
				//gd.t.mask.u32[2] = 0xffffffff;
				break;
			case CLAMP_CLAMP:
				pb->minv = 0;
				pb->maxv = th - 1;
				//gd.t.mask.u32[2] = 0;
				break;
			case CLAMP_REGION_CLAMP:
				pb->minv = std::min((int)context->CLAMP.MINV, th - 1);
				pb->maxv = std::min((int)context->CLAMP.MAXV, th - 1); // ffx anima summon scene, when the anchor appears (th = 256, maxv > 256)
				//gd.t.mask.u32[2] = 0;
				break;
			case CLAMP_REGION_REPEAT:
				pb->minv = (int)context->CLAMP.MINV & (th - 1); // skygunner main menu water texture 64x64, MINV = 127
				pb->maxv = (int)context->CLAMP.MAXV & (th - 1);
				//gd.t.mask.u32[2] = 0xffffffff;
				break;
			default:
				__assume(0);
			}
		}

		if(PRIM->FGE)
		{
			job->sel.fge = 1;
			pb->fog = env.FOGCOL.u32[0];
		}

		if(context->FRAME.PSM != PSM_PSMCT24)
		{
			job->sel.date = context->TEST.DATE;
			job->sel.datm = context->TEST.DATM;
		}

		if(!IsOpaque())
		{
			job->sel.abe = PRIM->ABE;
			job->sel.ababcd = context->ALPHA.u32[0];

			if(env.PABE.PABE)
			{
				job->sel.pabe = 1;
			}

			if(m_aa1 && PRIM->AA1 && (primclass == GS_LINE_CLASS || primclass == GS_TRIANGLE_CLASS))
			{
				job->sel.aa1 = 1;
			}

			pb->afix = context->ALPHA.FIX;
		}

		if(job->sel.date || job->sel.aba == 1 || job->sel.abb == 1 || job->sel.abc == 1 || job->sel.abd == 1)
		{
			job->sel.rfb = 1;
		}
		else
		{
			if(fwrite)
			{
				if(job->sel.atst != ATST_ALWAYS && job->sel.afail == AFAIL_RGB_ONLY
				|| (job->sel.fpsm & 3) == 0 && fm != 0
				|| (job->sel.fpsm & 3) == 1 // always read-merge-write 24bpp, regardless the mask
				|| (job->sel.fpsm & 3) >= 2 && (fm & 0x80f8f8f8) != 0)
				{
					job->sel.rfb = 1;
				}
			}
		}

		job->sel.colclamp = env.COLCLAMP.CLAMP;
		job->sel.fba = context->FBA.FBA;

		if(env.DTHE.DTHE)
		{
			job->sel.dthe = 1;

			GSVector4i dimx0 = env.dimx[1].sll32(16).sra32(16);
			GSVector4i dimx1 = env.dimx[3].sll32(16).sra32(16);
			GSVector4i dimx2 = env.dimx[5].sll32(16).sra32(16);
			GSVector4i dimx3 = env.dimx[7].sll32(16).sra32(16);

			pb->dimx = dimx0.ps32(dimx1).ps16(dimx2.ps32(dimx3));
		}
	}

	if(zwrite || ztest)
	{
		job->sel.zpsm = RemapPSM(context->ZBUF.PSM);
		job->sel.ztst = ztest ? context->TEST.ZTST : ZTST_ALWAYS;

		if(ztest)
		{
			job->sel.rzb = 1;
		}
		else
		{
			if(zwrite)
			{
				if(job->sel.atst != ATST_ALWAYS && (job->sel.afail == AFAIL_FB_ONLY || job->sel.afail == AFAIL_RGB_ONLY)
				|| (job->sel.zpsm & 3) == 1) // always read-merge-write 24bpp, regardless the mask
				{
					job->sel.rzb = 1;
				}
			}
		}
	}

	pb->fm = fm;
	pb->zm = zm;

	if((job->sel.fpsm & 3) == 1)
	{
		pb->fm |= 0xff000000;
	}
	else if((job->sel.fpsm & 3) >= 2)
	{
		uint32 rb = pb->fm & 0x00f800f8;
		uint32 ga = pb->fm & 0x8000f800;

		pb->fm = (ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3) | 0xffff0000;
	}

	if((job->sel.zpsm & 3) == 1)
	{
		pb->zm |= 0xff000000;
	}
	else if((job->sel.zpsm & 3) >= 2)
	{
		pb->zm |= 0xffff0000;
	}

	pb->fbp = context->FRAME.Block();
	pb->zbp = context->ZBUF.Block();
	pb->bw = context->FRAME.FBW;

	return true;
}

//

GSRendererCL::TFXJob::TFXJob()
	: src_pages(NULL)
	, dst_pages(NULL)
{
}

GSRendererCL::TFXJob::~TFXJob()
{
	if(src_pages != NULL) _aligned_free(src_pages);
	if(dst_pages != NULL) _aligned_free(dst_pages);
}

GSVector4i* GSRendererCL::TFXJob::GetSrcPages()
{
	if(src_pages == NULL)
	{
		src_pages = (GSVector4i*)_aligned_malloc(sizeof(GSVector4i) * 4, 16);

		src_pages[0] = GSVector4i::zero();
		src_pages[1] = GSVector4i::zero();
		src_pages[2] = GSVector4i::zero();
		src_pages[3] = GSVector4i::zero();
	}

	return src_pages;
}

GSVector4i* GSRendererCL::TFXJob::GetDstPages()
{
	if(dst_pages == NULL)
	{
		dst_pages = (GSVector4i*)_aligned_malloc(sizeof(GSVector4i) * 4, 16);

		dst_pages[0] = GSVector4i::zero();
		dst_pages[1] = GSVector4i::zero();
		dst_pages[2] = GSVector4i::zero();
		dst_pages[3] = GSVector4i::zero();
	}

	return dst_pages;
}

//

//#define IOCL_DEBUG

GSRendererCL::CL::CL()
{
	WIs = INT_MAX;
	version = INT_MAX;

	std::string ocldev = theApp.GetConfig("ocldev", "");

#ifdef IOCL_DEBUG
	ocldev = "Intel(R) Corporation Intel(R) Core(TM) i7-4770 CPU @ 3.40GHz OpenCL C 1.2 CPU";
#endif

	list<OCLDeviceDesc> dl;

	GSUtil::GetDeviceDescs(dl);

	for(auto d : dl)
	{
		if(d.name == ocldev)
		{
			devs.push_back(d);

			WIs = std::min(WIs, (uint32)d.device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>());
			version = std::min(version, d.version);

			break; // TODO: multiple devices?
		}
	}

	if(devs.empty() && !dl.empty())
	{
		auto d = dl.front();

		devs.push_back(d);

		WIs = std::min(WIs, (uint32)d.device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>());
		version = std::min(version, d.version);
	}

	if(devs.empty())
	{
		throw new std::exception("OpenCL device not found");
	}

	vector<cl::Device> tmp;

	for(auto d : devs) tmp.push_back(d.device);

	context = cl::Context(tmp);

	queue[0] = cl::CommandQueue(context);
	queue[1] = cl::CommandQueue(context);
	queue[2] = cl::CommandQueue(context);

	vector<unsigned char> buff;

	if(theApp.LoadResource(IDR_TFX_CL, buff))
	{
		kernel_str = std::string((const char*)buff.data(), buff.size());
	}

	vb.head = vb.tail = vb.size = 0;
	ib.head = ib.tail = ib.size = 0;
	pb.head = pb.tail = pb.size = 0;

	vb.mapped_ptr = vb.ptr = NULL;
	ib.mapped_ptr = ib.ptr = NULL;
	pb.mapped_ptr = pb.ptr = NULL;

	pb.size = TFX_PARAM_SIZE * 256;
	pb.buff[0] = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, pb.size);
	pb.buff[1] = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, pb.size);

	env = cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(gs_env));

	wqidx = 0;
	wq = &queue[0];
}

GSRendererCL::CL::~CL()
{
	Unmap();
}

void GSRendererCL::CL::Map()
{
	Unmap();

	cl_map_flags flags = version >= 120 ? CL_MAP_WRITE_INVALIDATE_REGION : CL_MAP_WRITE;

	if(vb.head < vb.size)
	{
		vb.mapped_ptr = wq->enqueueMapBuffer(vb.buff[wqidx], CL_TRUE, flags, vb.head, vb.size - vb.head);
		vb.ptr = (unsigned char*)vb.mapped_ptr - vb.head;
		ASSERT(((size_t)vb.ptr & 15) == 0);
	}

	if(ib.head < ib.size)
	{
		ib.mapped_ptr = wq->enqueueMapBuffer(ib.buff[wqidx], CL_TRUE, flags, ib.head, ib.size - ib.head);
		ib.ptr = (unsigned char*)ib.mapped_ptr - ib.head;
	}

	if(pb.head < pb.size)
	{
		pb.mapped_ptr = wq->enqueueMapBuffer(pb.buff[wqidx], CL_TRUE, flags, pb.head, pb.size - pb.head);
		pb.ptr = (unsigned char*)pb.mapped_ptr - pb.head;
		ASSERT(((size_t)pb.ptr & 15) == 0);
	}
}

void GSRendererCL::CL::Unmap()
{
	if(vb.mapped_ptr != NULL) wq->enqueueUnmapMemObject(vb.buff[wqidx], vb.mapped_ptr);
	if(ib.mapped_ptr != NULL) wq->enqueueUnmapMemObject(ib.buff[wqidx], ib.mapped_ptr);
	if(pb.mapped_ptr != NULL) wq->enqueueUnmapMemObject(pb.buff[wqidx], pb.mapped_ptr);

	vb.mapped_ptr = vb.ptr = NULL;
	ib.mapped_ptr = ib.ptr = NULL;
	pb.mapped_ptr = pb.ptr = NULL;
}

cl::Kernel GSRendererCL::CL::Build(const char* entry, ostringstream& opt)
{
	// TODO: cache binary on disk

	cl::Program program;

	if(version >= 120)
	{
		cl::Program::Binaries binaries;

		try
		{
			for(auto d : devs)
			{
				string path = d.tmppath + "/" + entry;

				FILE* f = fopen(path.c_str(), "rb");

				if(f != NULL)
				{
					fseek(f, 0, SEEK_END);				
					long size = ftell(f);
					pair<void*, size_t> b(new char[size], size);
					fseek(f, 0, SEEK_SET);
					fread(b.first, b.second, 1, f);
					fclose(f);

					binaries.push_back(b);
				}
				else
				{
					break;
				}
			}

			if(binaries.size() == devs.size())
			{
				vector<cl::Device> tmp;

				for(auto d : devs) tmp.push_back(d.device);

				program = cl::Program(context, tmp, binaries);

				AddDefs(opt);

				program.build(opt.str().c_str());

				cl::Kernel kernel = cl::Kernel(program, entry);

				return kernel;
			}
		}
		catch(cl::Error err)
		{
			printf("%s (%d)\n", err.what(), err.err());
		}

		for(auto b : binaries)
		{
			delete [] b.first;
		}
	}

	try
	{
		printf("building kernel (%s)\n", entry);

		program = cl::Program(context, kernel_str);

		AddDefs(opt);

		program.build(opt.str().c_str());
	}
	catch(cl::Error err)
	{
		if(err.err() == CL_BUILD_PROGRAM_FAILURE)
		{
			for(auto d : devs)
			{
				auto s = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(d.device);

				printf("kernel (%s) build error: %s\n", entry, s.c_str());
			}
		}

		throw err;
	}

	if(version >= 120)
	{
		try
		{
			vector<size_t> sizes = program.getInfo<CL_PROGRAM_BINARY_SIZES>();
			vector<char*> binaries = program.getInfo<CL_PROGRAM_BINARIES>();

			for(int i = 0; i < binaries.size(); i++)
			{
				string path = devs[i].tmppath + "/" + entry;

				FILE* f = fopen(path.c_str(), "wb");

				if(f != NULL)
				{
					fwrite(binaries[i], sizes[i], 1, f);
					fclose(f);
				}

				delete[] binaries[i];
			}
		}
		catch(cl::Error err)
		{
			printf("%s (%d)\n", err.what(), err.err());
		}
	}

	return cl::Kernel(program, entry);
}

void GSRendererCL::CL::AddDefs(ostringstream& opt)
{
	if(version == 110) opt << "-cl-std=CL1.1 ";
	else opt << "-cl-std=CL1.2 ";
	opt << "-D MAX_FRAME_SIZE=" << MAX_FRAME_SIZE << "u ";
	opt << "-D MAX_PRIM_COUNT=" << MAX_PRIM_COUNT << "u ";
	opt << "-D MAX_PRIM_PER_BATCH_BITS=" << MAX_PRIM_PER_BATCH_BITS << "u ";
	opt << "-D MAX_PRIM_PER_BATCH=" << MAX_PRIM_PER_BATCH << "u ";
	opt << "-D MAX_BATCH_COUNT=" << MAX_BATCH_COUNT << "u ";
	opt << "-D BIN_SIZE_BITS=" << BIN_SIZE_BITS << " ";
	opt << "-D BIN_SIZE=" << BIN_SIZE << "u ";
	opt << "-D MAX_BIN_PER_BATCH=" << MAX_BIN_PER_BATCH << "u ";
	opt << "-D MAX_BIN_COUNT=" << MAX_BIN_COUNT << "u ";
	opt << "-D TFX_PARAM_SIZE=" << TFX_PARAM_SIZE << "u ";
#ifdef IOCL_DEBUG
	opt << "-g -s \"E:\\Progs\\pcsx2\\plugins\\GSdx\\res\\tfx.cl\" ";
#endif
}

cl::Kernel& GSRendererCL::CL::GetPrimKernel(const PrimSelector& sel)
{
	auto i = prim_map.find(sel);

	if(i != prim_map.end())
	{
		return i->second;
	}

	char entry[256];

	sprintf(entry, "prim_%02x", sel);

	ostringstream opt;

	opt << "-D KERNEL_PRIM=" << entry << " ";
	opt << "-D PRIM=" << sel.prim << " ";

	cl::Kernel k = Build(entry, opt);

	prim_map[sel] = k;

	k.setArg(0, env);

	return prim_map[sel];
}

cl::Kernel& GSRendererCL::CL::GetTileKernel(const TileSelector& sel)
{
	auto i = tile_map.find(sel);

	if(i != tile_map.end())
	{
		return i->second;
	}

	char entry[256];

	sprintf(entry, "tile_%02x", sel);

	ostringstream opt;

	opt << "-D KERNEL_TILE=" << entry << " ";
	opt << "-D PRIM=" << sel.prim << " ";
	opt << "-D MODE=" << sel.mode << " ";
	opt << "-D CLEAR=" << sel.clear << " ";

	cl::Kernel k = Build(entry, opt);

	tile_map[sel] = k;

	k.setArg(0, env);

	return tile_map[sel];
}

cl::Kernel& GSRendererCL::CL::GetTFXKernel(const TFXSelector& sel)
{
	auto i = tfx_map.find(sel);

	if(i != tfx_map.end())
	{
		return i->second;
	}

	char entry[256];

	sprintf(entry, "tfx_%016llx", sel);

	ostringstream opt;

	opt << "-D KERNEL_TFX=" << entry << " ";
	opt << "-D FPSM=" << sel.fpsm << " ";
	opt << "-D ZPSM=" << sel.zpsm << " ";
	opt << "-D ZTST=" << sel.ztst << " ";
	opt << "-D ATST=" << sel.atst << " ";
	opt << "-D AFAIL=" << sel.afail << " ";
	opt << "-D IIP=" << sel.iip << " ";
	opt << "-D TFX=" << sel.tfx << " ";
	opt << "-D TCC=" << sel.tcc << " ";
	opt << "-D FST=" << sel.fst << " ";
	opt << "-D LTF=" << sel.ltf << " ";
	opt << "-D TLU=" << sel.tlu << " ";
	opt << "-D FGE=" << sel.fge << " ";
	opt << "-D DATE=" << sel.date << " ";
	opt << "-D ABE=" << sel.abe << " ";
	opt << "-D ABA=" << sel.aba << " ";
	opt << "-D ABB=" << sel.abb << " ";
	opt << "-D ABC=" << sel.abc << " ";
	opt << "-D ABD=" << sel.abd << " ";
	opt << "-D PABE=" << sel.pabe << " ";
	opt << "-D AA1=" << sel.aa1 << " ";
	opt << "-D FWRITE=" << sel.fwrite << " ";
	opt << "-D FTEST=" << sel.ftest << " ";
	opt << "-D RFB=" << sel.rfb << " ";
	opt << "-D ZWRITE=" << sel.zwrite << " ";
	opt << "-D ZTEST=" << sel.ztest << " ";
	opt << "-D RZB=" << sel.rzb << " ";
	opt << "-D WMS=" << sel.wms << " ";
	opt << "-D WMT=" << sel.wmt << " ";
	opt << "-D DATM=" << sel.datm << " ";
	opt << "-D COLCLAMP=" << sel.colclamp << " ";
	opt << "-D FBA=" << sel.fba << " ";
	opt << "-D DTHE=" << sel.dthe << " ";
	opt << "-D PRIM=" << sel.prim << " ";
	opt << "-D LCM=" << sel.lcm << " ";
	opt << "-D MMIN=" << sel.mmin << " ";
	opt << "-D NOSCISSOR=" << sel.noscissor << " ";
	opt << "-D TPSM=" << sel.tpsm << " ";
	opt << "-D AEM=" << sel.aem << " ";
	opt << "-D FB=" << sel.fb << " ";
	opt << "-D ZB=" << sel.zb << " ";

	cl::Kernel k = Build(entry, opt);

	tfx_map[sel] = k;

	k.setArg(0, env);
	k.setArg(1, vm);
	k.setArg(2, tex);

	return tfx_map[sel];
}
#endif
