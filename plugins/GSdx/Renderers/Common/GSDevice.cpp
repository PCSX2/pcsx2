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
#include "GSdx.h"
#include "GSDevice.h"

GSDevice::GSDevice()
	: m_wnd()
	, m_vsync(false)
	, m_rbswapped(false)
	, m_backbuffer(NULL)
	, m_merge(NULL)
	, m_weavebob(NULL)
	, m_blend(NULL)
	, m_target_tmp(NULL)
	, m_current(NULL)
	, m_frame(0)
{
	memset(&m_vertex, 0, sizeof(m_vertex));
	memset(&m_index, 0, sizeof(m_index));
	m_linear_present = theApp.GetConfigB("linear_present");
}

GSDevice::~GSDevice()
{
	for (auto t : m_pool)
		delete t;

	delete m_backbuffer;
	delete m_merge;
	delete m_weavebob;
	delete m_blend;
	delete m_target_tmp;
}

bool GSDevice::Create(const std::shared_ptr<GSWnd>& wnd)
{
	m_wnd = wnd;

	return true;
}

bool GSDevice::Reset(int w, int h)
{
	for (auto t : m_pool)
		delete t;

	m_pool.clear();

	delete m_backbuffer;
	delete m_merge;
	delete m_weavebob;
	delete m_blend;
	delete m_target_tmp;

	m_backbuffer = NULL;
	m_merge = NULL;
	m_weavebob = NULL;
	m_blend = NULL;
	m_target_tmp = NULL;

	m_current = NULL; // current is special, points to other textures, no need to delete

	return m_wnd != NULL;
}

void GSDevice::Present(const GSVector4i& r, int shader)
{
	GSVector4i cr = m_wnd->GetClientRect();

	int w = std::max<int>(cr.width(), 1);
	int h = std::max<int>(cr.height(), 1);

	if (!m_backbuffer || m_backbuffer->GetWidth() != w || m_backbuffer->GetHeight() != h)
	{
		if (!Reset(w, h))
		{
			return;
		}
	}

	GL_PUSH("Present");

	// FIXME is it mandatory, it could be slow
	ClearRenderTarget(m_backbuffer, 0);

	if (m_current)
	{
		static int s_shader[5] = {ShaderConvert_COPY, ShaderConvert_SCANLINE,
			ShaderConvert_DIAGONAL_FILTER, ShaderConvert_TRIANGULAR_FILTER,
			ShaderConvert_COMPLEX_FILTER}; // FIXME

		Present(m_current, m_backbuffer, GSVector4(r), s_shader[shader]);
		RenderOsd(m_backbuffer);
	}

	Flip();
}

void GSDevice::Present(GSTexture* sTex, GSTexture* dTex, const GSVector4& dRect, int shader)
{
	StretchRect(sTex, dTex, dRect, shader, m_linear_present);
}

GSTexture* GSDevice::FetchSurface(int type, int w, int h, int format)
{
	const GSVector2i size(w, h);

	for (auto i = m_pool.begin(); i != m_pool.end(); ++i)
	{
		GSTexture* t = *i;

		if (t->GetType() == type && t->GetFormat() == format && t->GetSize() == size)
		{
			m_pool.erase(i);

			return t;
		}
	}

	return CreateSurface(type, w, h, format);
}

void GSDevice::PrintMemoryUsage()
{
#ifdef ENABLE_OGL_DEBUG
	uint32 pool = 0;
	for (auto t : m_pool)
	{
		if (t)
			pool += t->GetMemUsage();
	}
	GL_PERF("MEM: Surface Pool %dMB", pool >> 20u);
#endif
}

void GSDevice::EndScene()
{
	m_vertex.start += m_vertex.count;
	m_vertex.count = 0;
	m_index.start += m_index.count;
	m_index.count = 0;
}

void GSDevice::Recycle(GSTexture* t)
{
	if (t)
	{
#ifdef _DEBUG
		// Uncommit saves memory but it means a futur allocation when we want to reuse the texture.
		// Which is slow and defeat the purpose of the m_pool cache.
		// However, it can help to spot part of texture that we forgot to commit
		t->Uncommit();
#endif
		t->last_frame_used = m_frame;

		m_pool.push_front(t);

		//printf("%d\n",m_pool.size());

		while (m_pool.size() > 300)
		{
			delete m_pool.back();

			m_pool.pop_back();
		}
	}
}

void GSDevice::AgePool()
{
	m_frame++;

	while (m_pool.size() > 40 && m_frame - m_pool.back()->last_frame_used > 10)
	{
		delete m_pool.back();

		m_pool.pop_back();
	}
}

void GSDevice::PurgePool()
{
	// OOM emergency. Let's free this useless pool
	while (!m_pool.empty())
	{
		delete m_pool.back();

		m_pool.pop_back();
	}
}

GSTexture* GSDevice::CreateSparseRenderTarget(int w, int h, int format)
{
	return FetchSurface(HasColorSparse() ? GSTexture::SparseRenderTarget : GSTexture::RenderTarget, w, h, format);
}

GSTexture* GSDevice::CreateSparseDepthStencil(int w, int h, int format)
{
	return FetchSurface(HasDepthSparse() ? GSTexture::SparseDepthStencil : GSTexture::DepthStencil, w, h, format);
}

GSTexture* GSDevice::CreateRenderTarget(int w, int h, int format)
{
	return FetchSurface(GSTexture::RenderTarget, w, h, format);
}

GSTexture* GSDevice::CreateDepthStencil(int w, int h, int format)
{
	return FetchSurface(GSTexture::DepthStencil, w, h, format);
}

GSTexture* GSDevice::CreateTexture(int w, int h, int format)
{
	return FetchSurface(GSTexture::Texture, w, h, format);
}

GSTexture* GSDevice::CreateOffscreen(int w, int h, int format)
{
	return FetchSurface(GSTexture::Offscreen, w, h, format);
}

void GSDevice::StretchRect(GSTexture* sTex, GSTexture* dTex, const GSVector4& dRect, int shader, bool linear)
{
	StretchRect(sTex, GSVector4(0, 0, 1, 1), dTex, dRect, shader, linear);
}

GSTexture* GSDevice::GetCurrent()
{
	return m_current;
}

void GSDevice::Merge(GSTexture* sTex[3], GSVector4* sRect, GSVector4* dRect, const GSVector2i& fs, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c)
{
	// KH:COM crashes at startup when booting *through the bios* due to m_merge being NULL.
	// (texture appears to be non-null, and is being re-created at a size around like 1700x340,
	// dunno if that's relevant) -- air

	if (ResizeTarget(&m_merge, fs.x, fs.y))
	{
		GSTexture* tex[3] = {NULL, NULL, NULL};

		for (size_t i = 0; i < countof(tex); i++)
		{
			if (sTex[i] != NULL)
			{
				tex[i] = sTex[i];
			}
		}

		DoMerge(tex, sRect, m_merge, dRect, PMODE, EXTBUF, c);

		for (size_t i = 0; i < countof(tex); i++)
		{
			if (tex[i] != sTex[i])
			{
				Recycle(tex[i]);
			}
		}
	}
	else
	{
		printf("GSdx: m_merge is NULL!\n");
	}

	m_current = m_merge;
}

void GSDevice::Interlace(const GSVector2i& ds, int field, int mode, float yoffset)
{
	ResizeTarget(&m_weavebob, ds.x, ds.y);

	if (mode == 0 || mode == 2) // weave or blend
	{
		// weave first

		DoInterlace(m_merge, m_weavebob, field, false, 0);

		if (mode == 2)
		{
			// blend

			ResizeTarget(&m_blend, ds.x, ds.y);

			DoInterlace(m_weavebob, m_blend, 2, false, 0);

			m_current = m_blend;
		}
		else
		{
			m_current = m_weavebob;
		}
	}
	else if (mode == 1) // bob
	{
		DoInterlace(m_merge, m_weavebob, 3, true, yoffset * field);

		m_current = m_weavebob;
	}
	else
	{
		m_current = m_merge;
	}
}

void GSDevice::ExternalFX()
{
	GSVector2i s = m_current->GetSize();

	if (ResizeTarget(&m_target_tmp))
	{
		GSVector4 sRect(0, 0, 1, 1);
		GSVector4 dRect(0, 0, s.x, s.y);

		StretchRect(m_current, sRect, m_target_tmp, dRect, ShaderConvert_TRANSPARENCY_FILTER, false);
		DoExternalFX(m_target_tmp, m_current);
	}
}

void GSDevice::FXAA()
{
	GSVector2i s = m_current->GetSize();

	if (ResizeTarget(&m_target_tmp))
	{
		GSVector4 sRect(0, 0, 1, 1);
		GSVector4 dRect(0, 0, s.x, s.y);

		StretchRect(m_current, sRect, m_target_tmp, dRect, ShaderConvert_TRANSPARENCY_FILTER, false);
		DoFXAA(m_target_tmp, m_current);
	}
}

void GSDevice::ShadeBoost()
{
	GSVector2i s = m_current->GetSize();

	if (ResizeTarget(&m_target_tmp))
	{
		GSVector4 sRect(0, 0, 1, 1);
		GSVector4 dRect(0, 0, s.x, s.y);

		StretchRect(m_current, sRect, m_target_tmp, dRect, ShaderConvert_COPY, false);
		DoShadeBoost(m_target_tmp, m_current);
	}
}

bool GSDevice::ResizeTexture(GSTexture** t, int type, int w, int h)
{
	if (t == NULL)
	{
		ASSERT(0);
		return false;
	}

	GSTexture* t2 = *t;

	if (t2 == NULL || t2->GetWidth() != w || t2->GetHeight() != h)
	{
		delete t2;

		t2 = FetchSurface(type, w, h, 0);

		*t = t2;
	}

	return t2 != NULL;
}

bool GSDevice::ResizeTexture(GSTexture** t, int w, int h)
{
	return ResizeTexture(t, GSTexture::Texture, w, h);
}

bool GSDevice::ResizeTarget(GSTexture** t, int w, int h)
{
	return ResizeTexture(t, GSTexture::RenderTarget, w, h);
}

bool GSDevice::ResizeTarget(GSTexture** t)
{
	GSVector2i s = m_current->GetSize();
	return ResizeTexture(t, GSTexture::RenderTarget, s.x, s.y);
}

GSAdapter::operator std::string() const
{
	char buf[sizeof "12345678:12345678:12345678:12345678"];
	sprintf(buf, "%.4X:%.4X:%.8X:%.2X", vendor, device, subsys, rev);
	return buf;
}

bool GSAdapter::operator==(const GSAdapter& desc_dxgi) const
{
	return vendor == desc_dxgi.vendor
		&& device == desc_dxgi.device
		&& subsys == desc_dxgi.subsys
		&& rev == desc_dxgi.rev;
}

#ifdef _WIN32
GSAdapter::GSAdapter(const DXGI_ADAPTER_DESC1& desc_dxgi)
	: vendor(desc_dxgi.VendorId)
	, device(desc_dxgi.DeviceId)
	, subsys(desc_dxgi.SubSysId)
	, rev(desc_dxgi.Revision)
{
}
#endif
#ifdef __linux__
// TODO
#endif

HWBlend GSDevice::GetBlend(size_t index)
{
	HWBlend blend = m_blendMap[index];
	blend.op  = ConvertBlendEnum(blend.op);
	blend.src = ConvertBlendEnum(blend.src);
	blend.dst = ConvertBlendEnum(blend.dst);
	return blend;
}

uint16 GSDevice::GetBlendFlags(size_t index) { return m_blendMap[index].flags; }

std::array<HWBlend, 3*3*3*3 + 1> GSDevice::m_blendMap =
{{
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0000: (Cs - Cs)*As + Cs ==> Cs
	{ 0                          , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 0001: (Cs - Cs)*As + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 0002: (Cs - Cs)*As +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0010: (Cs - Cs)*Ad + Cs ==> Cs
	{ 0                          , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 0011: (Cs - Cs)*Ad + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 0012: (Cs - Cs)*Ad +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0020: (Cs - Cs)*F  + Cs ==> Cs
	{ 0                          , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 0021: (Cs - Cs)*F  + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 0022: (Cs - Cs)*F  +  0 ==> 0
	{ BLEND_A_MAX                , OP_SUBTRACT     , CONST_ONE       , SRC1_ALPHA}      , //*0100: (Cs - Cd)*As + Cs ==> Cs*(As + 1) - Cd*As
	{ 0                          , OP_ADD          , SRC1_ALPHA      , INV_SRC1_ALPHA}  , // 0101: (Cs - Cd)*As + Cd ==> Cs*As + Cd*(1 - As)
	{ 0                          , OP_SUBTRACT     , SRC1_ALPHA      , SRC1_ALPHA}      , // 0102: (Cs - Cd)*As +  0 ==> Cs*As - Cd*As
	{ BLEND_A_MAX                , OP_SUBTRACT     , CONST_ONE       , DST_ALPHA}       , //*0110: (Cs - Cd)*Ad + Cs ==> Cs*(Ad + 1) - Cd*Ad
	{ 0                          , OP_ADD          , DST_ALPHA       , INV_DST_ALPHA}   , // 0111: (Cs - Cd)*Ad + Cd ==> Cs*Ad + Cd*(1 - Ad)
	{ 0                          , OP_SUBTRACT     , DST_ALPHA       , DST_ALPHA}       , // 0112: (Cs - Cd)*Ad +  0 ==> Cs*Ad - Cd*Ad
	{ BLEND_A_MAX                , OP_SUBTRACT     , CONST_ONE       , CONST_COLOR}     , //*0120: (Cs - Cd)*F  + Cs ==> Cs*(F + 1) - Cd*F
	{ 0                          , OP_ADD          , CONST_COLOR     , INV_CONST_COLOR} , // 0121: (Cs - Cd)*F  + Cd ==> Cs*F + Cd*(1 - F)
	{ 0                          , OP_SUBTRACT     , CONST_COLOR     , CONST_COLOR}     , // 0122: (Cs - Cd)*F  +  0 ==> Cs*F - Cd*F
	{ BLEND_NO_REC | BLEND_A_MAX , OP_ADD          , CONST_ONE       , CONST_ZERO}      , //*0200: (Cs -  0)*As + Cs ==> Cs*(As + 1)
	{ BLEND_ACCU                 , OP_ADD          , SRC1_ALPHA      , CONST_ONE}       , //?0201: (Cs -  0)*As + Cd ==> Cs*As + Cd
	{ BLEND_NO_REC               , OP_ADD          , SRC1_ALPHA      , CONST_ZERO}      , // 0202: (Cs -  0)*As +  0 ==> Cs*As
	{ BLEND_A_MAX                , OP_ADD          , CONST_ONE       , CONST_ZERO}      , //*0210: (Cs -  0)*Ad + Cs ==> Cs*(Ad + 1)
	{ 0                          , OP_ADD          , DST_ALPHA       , CONST_ONE}       , // 0211: (Cs -  0)*Ad + Cd ==> Cs*Ad + Cd
	{ 0                          , OP_ADD          , DST_ALPHA       , CONST_ZERO}      , // 0212: (Cs -  0)*Ad +  0 ==> Cs*Ad
	{ BLEND_NO_REC | BLEND_A_MAX , OP_ADD          , CONST_ONE       , CONST_ZERO}      , //*0220: (Cs -  0)*F  + Cs ==> Cs*(F + 1)
	{ BLEND_ACCU                 , OP_ADD          , CONST_COLOR     , CONST_ONE}       , //?0221: (Cs -  0)*F  + Cd ==> Cs*F + Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_COLOR     , CONST_ZERO}      , // 0222: (Cs -  0)*F  +  0 ==> Cs*F
	{ 0                          , OP_ADD          , INV_SRC1_ALPHA  , SRC1_ALPHA}      , // 1000: (Cd - Cs)*As + Cs ==> Cd*As + Cs*(1 - As)
	{ BLEND_A_MAX                , OP_REV_SUBTRACT , SRC1_ALPHA      , CONST_ONE}       , //*1001: (Cd - Cs)*As + Cd ==> Cd*(As + 1) - Cs*As
	{ 0                          , OP_REV_SUBTRACT , SRC1_ALPHA      , SRC1_ALPHA}      , // 1002: (Cd - Cs)*As +  0 ==> Cd*As - Cs*As
	{ 0                          , OP_ADD          , INV_DST_ALPHA   , DST_ALPHA}       , // 1010: (Cd - Cs)*Ad + Cs ==> Cd*Ad + Cs*(1 - Ad)
	{ BLEND_A_MAX                , OP_REV_SUBTRACT , DST_ALPHA       , CONST_ONE}       , //*1011: (Cd - Cs)*Ad + Cd ==> Cd*(Ad + 1) - Cs*Ad
	{ 0                          , OP_REV_SUBTRACT , DST_ALPHA       , DST_ALPHA}       , // 1012: (Cd - Cs)*Ad +  0 ==> Cd*Ad - Cs*Ad
	{ 0                          , OP_ADD          , INV_CONST_COLOR , CONST_COLOR}     , // 1020: (Cd - Cs)*F  + Cs ==> Cd*F + Cs*(1 - F)
	{ BLEND_A_MAX                , OP_REV_SUBTRACT , CONST_COLOR     , CONST_ONE}       , //*1021: (Cd - Cs)*F  + Cd ==> Cd*(F + 1) - Cs*F
	{ 0                          , OP_REV_SUBTRACT , CONST_COLOR     , CONST_COLOR}     , // 1022: (Cd - Cs)*F  +  0 ==> Cd*F - Cs*F
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 1100: (Cd - Cd)*As + Cs ==> Cs
	{ 0                          , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 1101: (Cd - Cd)*As + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 1102: (Cd - Cd)*As +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 1110: (Cd - Cd)*Ad + Cs ==> Cs
	{ 0                          , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 1111: (Cd - Cd)*Ad + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 1112: (Cd - Cd)*Ad +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 1120: (Cd - Cd)*F  + Cs ==> Cs
	{ 0                          , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 1121: (Cd - Cd)*F  + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 1122: (Cd - Cd)*F  +  0 ==> 0
	{ 0                          , OP_ADD          , CONST_ONE       , SRC1_ALPHA}      , // 1200: (Cd -  0)*As + Cs ==> Cs + Cd*As
	{ BLEND_C_CLR                , OP_ADD          , DST_COLOR       , SRC1_ALPHA}      , //#1201: (Cd -  0)*As + Cd ==> Cd*(1 + As) // ffxii main menu background
	{ 0                          , OP_ADD          , CONST_ZERO      , SRC1_ALPHA}      , // 1202: (Cd -  0)*As +  0 ==> Cd*As
	{ 0                          , OP_ADD          , CONST_ONE       , DST_ALPHA}       , // 1210: (Cd -  0)*Ad + Cs ==> Cs + Cd*Ad
	{ BLEND_C_CLR                , OP_ADD          , DST_COLOR       , DST_ALPHA}       , //#1211: (Cd -  0)*Ad + Cd ==> Cd*(1 + Ad)
	{ 0                          , OP_ADD          , CONST_ZERO      , DST_ALPHA}       , // 1212: (Cd -  0)*Ad +  0 ==> Cd*Ad
	{ 0                          , OP_ADD          , CONST_ONE       , CONST_COLOR}     , // 1220: (Cd -  0)*F  + Cs ==> Cs + Cd*F
	{ BLEND_C_CLR                , OP_ADD          , DST_COLOR       , CONST_COLOR}     , //#1221: (Cd -  0)*F  + Cd ==> Cd*(1 + F)
	{ 0                          , OP_ADD          , CONST_ZERO      , CONST_COLOR}     , // 1222: (Cd -  0)*F  +  0 ==> Cd*F
	{ BLEND_NO_REC               , OP_ADD          , INV_SRC1_ALPHA  , CONST_ZERO}      , // 2000: (0  - Cs)*As + Cs ==> Cs*(1 - As)
	{ BLEND_ACCU                 , OP_REV_SUBTRACT , SRC1_ALPHA      , CONST_ONE}       , //?2001: (0  - Cs)*As + Cd ==> Cd - Cs*As
	{ BLEND_NO_REC               , OP_REV_SUBTRACT , SRC1_ALPHA      , CONST_ZERO}      , // 2002: (0  - Cs)*As +  0 ==> 0 - Cs*As
	{ 0                          , OP_ADD          , INV_DST_ALPHA	 , CONST_ZERO}      , // 2010: (0  - Cs)*Ad + Cs ==> Cs*(1 - Ad)
	{ 0                          , OP_REV_SUBTRACT , DST_ALPHA       , CONST_ONE}       , // 2011: (0  - Cs)*Ad + Cd ==> Cd - Cs*Ad
	{ 0                          , OP_REV_SUBTRACT , DST_ALPHA       , CONST_ZERO}      , // 2012: (0  - Cs)*Ad +  0 ==> 0 - Cs*Ad
	{ BLEND_NO_REC               , OP_ADD          , INV_CONST_COLOR , CONST_ZERO}      , // 2020: (0  - Cs)*F  + Cs ==> Cs*(1 - F)
	{ BLEND_ACCU                 , OP_REV_SUBTRACT , CONST_COLOR     , CONST_ONE}       , //?2021: (0  - Cs)*F  + Cd ==> Cd - Cs*F
	{ BLEND_NO_REC               , OP_REV_SUBTRACT , CONST_COLOR     , CONST_ZERO}      , // 2022: (0  - Cs)*F  +  0 ==> 0 - Cs*F
	{ 0                          , OP_SUBTRACT     , CONST_ONE       , SRC1_ALPHA}      , // 2100: (0  - Cd)*As + Cs ==> Cs - Cd*As
	{ 0                          , OP_ADD          , CONST_ZERO      , INV_SRC1_ALPHA}  , // 2101: (0  - Cd)*As + Cd ==> Cd*(1 - As)
	{ 0                          , OP_SUBTRACT     , CONST_ZERO      , SRC1_ALPHA}      , // 2102: (0  - Cd)*As +  0 ==> 0 - Cd*As
	{ 0                          , OP_SUBTRACT     , CONST_ONE       , DST_ALPHA}       , // 2110: (0  - Cd)*Ad + Cs ==> Cs - Cd*Ad
	{ 0                          , OP_ADD          , CONST_ZERO      , INV_DST_ALPHA}   , // 2111: (0  - Cd)*Ad + Cd ==> Cd*(1 - Ad)
	{ 0                          , OP_SUBTRACT     , CONST_ONE       , DST_ALPHA}       , // 2112: (0  - Cd)*Ad +  0 ==> 0 - Cd*Ad
	{ 0                          , OP_SUBTRACT     , CONST_ONE       , CONST_COLOR}     , // 2120: (0  - Cd)*F  + Cs ==> Cs - Cd*F
	{ 0                          , OP_ADD          , CONST_ZERO      , INV_CONST_COLOR} , // 2121: (0  - Cd)*F  + Cd ==> Cd*(1 - F)
	{ 0                          , OP_SUBTRACT     , CONST_ONE       , CONST_COLOR}     , // 2122: (0  - Cd)*F  +  0 ==> 0 - Cd*F
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 2200: (0  -  0)*As + Cs ==> Cs
	{ 0                          , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 2201: (0  -  0)*As + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 2202: (0  -  0)*As +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 2210: (0  -  0)*Ad + Cs ==> Cs
	{ 0                          , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 2211: (0  -  0)*Ad + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 2212: (0  -  0)*Ad +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 2220: (0  -  0)*F  + Cs ==> Cs
	{ 0                          , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 2221: (0  -  0)*F  + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 2222: (0  -  0)*F  +  0 ==> 0
	{ 0                          , OP_ADD          , SRC_ALPHA       , INV_SRC_ALPHA}   , // extra for merge operation
}};