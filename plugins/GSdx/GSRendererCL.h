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

#pragma once

#include "GSRenderer.h"
//#include "GSTextureCacheCL.h"

__aligned(struct, 32) GSVertexCL
{
	GSVector4 p, t;
};

class GSRendererCL : public GSRenderer
{
	typedef void (GSRendererCL::*ConvertVertexBufferPtr)(GSVertexCL* RESTRICT dst, const GSVertex* RESTRICT src, size_t count);

	ConvertVertexBufferPtr m_cvb[4][2][2];

	template<uint32 primclass, uint32 tme, uint32 fst>
	void ConvertVertexBuffer(GSVertexCL* RESTRICT dst, const GSVertex* RESTRICT src, size_t count);

	union PrimSelector
	{
		struct
		{
			uint32 prim:2; // 0
		};

		uint32 key;

		operator uint32() const { return key; }
	};

	union TileSelector
	{
		struct
		{
			uint32 prim:2; // 0
			uint32 mode:2; // 2
			uint32 clear:1; // 4
		};

		uint32 key;

		operator uint32() const { return key; }
	};

	union TFXSelector
	{
		struct
		{
			uint32 fpsm:3; // 0
			uint32 zpsm:3; // 3
			uint32 ztst:2; // 6 (0: off, 1: write, 2: test (ge), 3: test (g))
			uint32 atst:3; // 8
			uint32 afail:2; // 11
			uint32 iip:1; // 13
			uint32 tfx:3; // 14
			uint32 tcc:1; // 17
			uint32 fst:1; // 18
			uint32 ltf:1; // 19
			uint32 tlu:1; // 20
			uint32 fge:1; // 21
			uint32 date:1; // 22
			uint32 abe:1; // 23
			uint32 aba:2; // 24
			uint32 abb:2; // 26
			uint32 abc:2; // 28
			uint32 abd:2; // 30

			uint32 pabe:1; // 32
			uint32 aa1:1; // 33
			uint32 fwrite:1; // 34
			uint32 ftest:1; // 35
			uint32 rfb:1; // 36
			uint32 zwrite:1; // 37
			uint32 ztest:1; // 38
			uint32 rzb:1; // 39
			uint32 wms:2; // 40
			uint32 wmt:2; // 42
			uint32 datm:1; // 44
			uint32 colclamp:1; // 45
			uint32 fba:1; // 46
			uint32 dthe:1; // 47
			uint32 prim:2; // 48
			uint32 lcm:1; // 50
			uint32 mmin:2; // 51
			uint32 noscissor:1; // 53
			uint32 tpsm:4; // 54
			uint32 aem:1; // 58
			// TODO
		};

		struct
		{
			uint32 _pad1:24;
			uint32 ababcd:8;
			uint32 _pad2:2;
			uint32 fb:2;
			uint32 _pad3:1;
			uint32 zb:2;
		};

		struct
		{
			uint32 lo;
			uint32 hi;
		};

		uint64 key;

		operator uint64() const { return key; }

		bool IsSolidRect() const
		{
			return prim == GS_SPRITE_CLASS
				&& iip == 0
				&& tfx == TFX_NONE
				&& abe == 0
				&& ztst <= 1
				&& atst <= 1
				&& date == 0
				&& fge == 0;
		}
	};

	__aligned(struct, 32) TFXParameter
	{
		GSVector4i scissor;
		GSVector4i bbox;
		GSVector4i rect;
		GSVector4i dimx; // 4x4 signed char
		TFXSelector sel;
		uint32 fbp, zbp, bw;
		uint32 fm, zm;
		uint32 fog; // rgb
		uint8 aref, afix;
		uint8 ta0, ta1;
		uint32 tbp[7], tbw[7];
		int minu, maxu, minv, maxv; // umsk, ufix, vmsk, vfix
		int lod; // lcm == 1
		int mxl;
		float l; // TEX1.L * -0x10000
		float k; // TEX1.K * 0x10000
		uint32 clut[256];
	};

	class TFXJob
	{
	public:
		struct { int x, y, z, w; } rect;
		TFXSelector sel; // uses primclass, solidrect only
		uint32 ib_start, ib_count;
		uint32 pb_start;
		GSVector4i* src_pages; // read by any texture level
		GSVector4i* dst_pages; // f/z writes to it

		TFXJob()
			: src_pages(NULL)
			, dst_pages(NULL)
		{
		}

		virtual ~TFXJob()
		{
			if(src_pages != NULL) _aligned_free(src_pages);
			if(dst_pages != NULL) _aligned_free(dst_pages);
		}

		GSVector4i* GetSrcPages()
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

		GSVector4i* GetDstPages()
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
	};

	class CL
	{
		std::string kernel_str;
		std::map<uint32, cl::Kernel> prim_map;
		std::map<uint32, cl::Kernel> tile_map;
		std::map<uint64, cl::Kernel> tfx_map;

	public:
		std::vector<cl::Device> devices;
		cl::Context context;
		cl::CommandQueue queue[3];
		cl::Buffer vm;
		cl::Buffer tex;
		struct { cl::Buffer buff[2]; size_t head, tail, size; unsigned char* ptr; void* mapped_ptr; } vb, ib, pb;
		cl::Buffer env;
		cl::CommandQueue* wq;
		int wqidx;
		size_t WIs;

	public:
		CL();
		virtual ~CL();

		cl::Kernel& GetPrimKernel(const PrimSelector& sel);
		cl::Kernel& GetTileKernel(const TileSelector& sel);
		cl::Kernel& GetTFXKernel(const TFXSelector& sel);

		void Map();
		void Unmap();
	};

	CL m_cl;
	std::list<shared_ptr<TFXJob>> m_jobs;
	uint32 m_vb_start;
	uint32 m_vb_count;

	void Enqueue();

	/*
	class RasterizerData : public GSAlignedClass<32>
	{
		__aligned(struct, 16) TextureLevel
		{
			GSVector4i r;
			// TODO: GSTextureCacheCL::Texture* t;
		};

	public:
		GSRendererCL* m_parent;
		const uint32* m_fb_pages;
		const uint32* m_zb_pages;

		//cl::Buffer m_vbuff;
		//cl::Buffer m_ibuff;

		// TODO: buffers
		TextureLevel m_tex[7 + 1]; // NULL terminated
		//cl::Buffer m_clut;
		//cl::Buffer m_dimx;

		// TODO: struct in a cl::Buffer
		TFXSelector m_sel;
		GSVector4i m_scissor;
		GSVector4i m_bbox;
		uint32 m_fm, m_zm;
		int m_aref, m_afix;
		uint32 m_fog; // rgb
		int m_lod; // lcm == 1
		int m_mxl;
		float m_l; // TEX1.L * -0x10000
		float m_k; // TEX1.K * 0x10000
		// TODO: struct { GSVector4i min, max, minmax, mask, invmask; } t; // [u] x 4 [v] x 4

		RasterizerData(GSRendererCL* parent)
			: m_parent(parent)
			, m_fb_pages(NULL)
			, m_zb_pages(NULL)
		{
			m_sel.key = 0;
		}

		virtual ~RasterizerData()
		{
			// TODO: ReleasePages();
		}

		// TODO: void UsePages(const uint32* fb_pages, int fpsm, const uint32* zb_pages, int zpsm);
		// TODO: void ReleasePages();

		// TODO: void SetSource(GSTextureCacheCL::Texture* t, const GSVector4i& r, int level);
		// TODO: void UpdateSource();
	};
	*/
protected:
//	GSTextureCacheCL* m_tc;
	GSTexture* m_texture[2];
	uint8* m_output;
	
	GSVector4i m_rw_pages[2][4]; // pages that may be read or modified by the rendering queue, f/z rw, tex r
	GSVector4i m_tc_pages[4]; // invalidated texture cache pages
	GSVector4i m_tmp_pages[4];

	void Reset();
	void VSync(int field);
	void ResetDevice();
	GSTexture* GetOutput(int i);

	void Draw();
	void Sync(int reason);
	void InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r);
	void InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut = false);

	bool SetupParameter(TFXJob* job, TFXParameter* pb, GSVertexCL* vertex, size_t vertex_count, const uint32* index, size_t index_count);

public:
	GSRendererCL();
	virtual ~GSRendererCL();
};
