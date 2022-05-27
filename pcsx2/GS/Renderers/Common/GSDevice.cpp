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
#include "GSDevice.h"
#include "GS/GSGL.h"
#include "GS/GS.h"

const char* shaderName(ShaderConvert value)
{
	switch (value)
	{
		case ShaderConvert::COPY:                return "ps_copy";
		case ShaderConvert::RGBA8_TO_16_BITS:    return "ps_convert_rgba8_16bits";
		case ShaderConvert::DATM_1:              return "ps_datm1";
		case ShaderConvert::DATM_0:              return "ps_datm0";
		case ShaderConvert::MOD_256:             return "ps_mod256";
		case ShaderConvert::SCANLINE:            return "ps_filter_scanlines";
		case ShaderConvert::DIAGONAL_FILTER:     return "ps_filter_diagonal";
		case ShaderConvert::TRANSPARENCY_FILTER: return "ps_filter_transparency";
		case ShaderConvert::TRIANGULAR_FILTER:   return "ps_filter_triangular";
		case ShaderConvert::COMPLEX_FILTER:      return "ps_filter_complex";
		case ShaderConvert::FLOAT32_TO_16_BITS:  return "ps_convert_float32_32bits";
		case ShaderConvert::FLOAT32_TO_32_BITS:  return "ps_convert_float32_32bits";
		case ShaderConvert::FLOAT32_TO_RGBA8:    return "ps_convert_float32_rgba8";
		case ShaderConvert::FLOAT16_TO_RGB5A1:   return "ps_convert_float16_rgb5a1";
		case ShaderConvert::RGBA8_TO_FLOAT32:    return "ps_convert_rgba8_float32";
		case ShaderConvert::RGBA8_TO_FLOAT24:    return "ps_convert_rgba8_float24";
		case ShaderConvert::RGBA8_TO_FLOAT16:    return "ps_convert_rgba8_float16";
		case ShaderConvert::RGB5A1_TO_FLOAT16:   return "ps_convert_rgb5a1_float16";
		case ShaderConvert::DEPTH_COPY:          return "ps_depth_copy";
		case ShaderConvert::RGBA_TO_8I:          return "ps_convert_rgba_8i";
		case ShaderConvert::YUV:                 return "ps_yuv";
		default:
			ASSERT(0);
			return "ShaderConvertUnknownShader";
	}
}

static int MipmapLevelsForSize(int width, int height)
{
	return std::min(static_cast<int>(std::log2(std::max(width, height))) + 1, MAXIMUM_TEXTURE_MIPMAP_LEVELS);
}

std::unique_ptr<GSDevice> g_gs_device;

GSDevice::GSDevice()
	: m_merge(NULL)
	, m_weavebob(NULL)
	, m_blend(NULL)
	, m_target_tmp(NULL)
	, m_current(NULL)
	, m_frame(0)
	, m_rbswapped(false)
{
	memset(&m_vertex, 0, sizeof(m_vertex));
	memset(&m_index, 0, sizeof(m_index));
}

GSDevice::~GSDevice()
{
	PurgePool();

	delete m_merge;
	delete m_weavebob;
	delete m_blend;
	delete m_target_tmp;
}

bool GSDevice::Create(HostDisplay* display)
{
	m_display = display;
	return true;
}

void GSDevice::Destroy()
{
	PurgePool();

	delete m_merge;
	delete m_weavebob;
	delete m_blend;
	delete m_target_tmp;

	m_merge = nullptr;
	m_weavebob = nullptr;
	m_blend = nullptr;
	m_target_tmp = nullptr;

	m_current = nullptr; // current is special, points to other textures, no need to delete
}

void GSDevice::ResetAPIState()
{
}

void GSDevice::RestoreAPIState()
{
}

GSTexture* GSDevice::FetchSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format, bool clear, bool prefer_reuse)
{
	const GSVector2i size(width, height);
	const bool prefer_new_texture = (m_features.prefer_new_textures && type == GSTexture::Type::Texture && !prefer_reuse);

	GSTexture* t = nullptr;
	auto fallback = m_pool.end();

	for (auto i = m_pool.begin(); i != m_pool.end(); ++i)
	{
		t = *i;

		assert(t);

		if (t->GetType() == type && t->GetFormat() == format && t->GetSize() == size && t->GetMipmapLevels() == levels)
		{
			if (!prefer_new_texture || t->last_frame_used != m_frame)
			{
				m_pool.erase(i);
				break;
			}
			else if (fallback == m_pool.end())
			{
				fallback = i;
			}
		}

		t = nullptr;
	}

	if (!t)
	{
		if (m_pool.size() >= MAX_POOLED_TEXTURES && fallback != m_pool.end())
		{
			t = *fallback;
			m_pool.erase(fallback);
		}
		else
		{
			t = CreateSurface(type, width, height, levels, format);
			if (!t)
				throw std::bad_alloc();
		}
	}

	t->SetScale(GSVector2(1, 1)); // Things seem to assume that all textures come out of here with scale 1...
	t->Commit(); // Clear won't be done if the texture isn't committed.

	switch (type)
	{
	case GSTexture::Type::RenderTarget:
	case GSTexture::Type::SparseRenderTarget:
		{
			if (clear)
				ClearRenderTarget(t, 0);
			else
				InvalidateRenderTarget(t);
		}
		break;
	case GSTexture::Type::DepthStencil:
	case GSTexture::Type::SparseDepthStencil:
		{
			if (clear)
				ClearDepth(t);
			else
				InvalidateRenderTarget(t);
		}
		break;
	default:
		break;
	}

	return t;
}

void GSDevice::PrintMemoryUsage()
{
#ifdef ENABLE_OGL_DEBUG
	u32 pool = 0;
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

		while (m_pool.size() > MAX_POOLED_TEXTURES)
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
	for (auto t : m_pool)
		delete t;
	m_pool.clear();
}

void GSDevice::ClearSamplerCache()
{
}

GSTexture* GSDevice::CreateSparseRenderTarget(int w, int h, GSTexture::Format format, bool clear)
{
	return FetchSurface(HasColorSparse() ? GSTexture::Type::SparseRenderTarget : GSTexture::Type::RenderTarget, w, h, 1, format, clear, true);
}

GSTexture* GSDevice::CreateSparseDepthStencil(int w, int h, GSTexture::Format format, bool clear)
{
	return FetchSurface(HasDepthSparse() ? GSTexture::Type::SparseDepthStencil : GSTexture::Type::DepthStencil, w, h, 1, format, clear, true);
}

GSTexture* GSDevice::CreateRenderTarget(int w, int h, GSTexture::Format format, bool clear)
{
	return FetchSurface(GSTexture::Type::RenderTarget, w, h, 1, format, clear, true);
}

GSTexture* GSDevice::CreateDepthStencil(int w, int h, GSTexture::Format format, bool clear)
{
	return FetchSurface(GSTexture::Type::DepthStencil, w, h, 1, format, clear, true);
}

GSTexture* GSDevice::CreateTexture(int w, int h, bool mipmap, GSTexture::Format format, bool prefer_reuse /* = false */)
{
	const int levels = mipmap ? MipmapLevelsForSize(w, h) : 1;
	return FetchSurface(GSTexture::Type::Texture, w, h, levels, format, false, prefer_reuse);
}

GSTexture* GSDevice::CreateOffscreen(int w, int h, GSTexture::Format format)
{
	return FetchSurface(GSTexture::Type::Offscreen, w, h, 1, format, false, true);
}

GSTexture::Format GSDevice::GetDefaultTextureFormat(GSTexture::Type type)
{
	if (type == GSTexture::Type::DepthStencil || type == GSTexture::Type::SparseDepthStencil)
		return GSTexture::Format::DepthStencil;
	else
		return GSTexture::Format::Color;
}

bool GSDevice::DownloadTextureConvert(GSTexture* src, const GSVector4& sRect, const GSVector2i& dSize, GSTexture::Format format, ShaderConvert ps_shader, GSTexture::GSMap& out_map, const bool linear)
{
	ASSERT(src);
	ASSERT(format == GSTexture::Format::Color || format == GSTexture::Format::UInt16 || format == GSTexture::Format::UInt32);

	GSTexture* dst = CreateRenderTarget(dSize.x, dSize.y, format);
	if (!dst)
		return false;

	GSVector4i dRect(0, 0, dSize.x, dSize.y);
	StretchRect(src, sRect, dst, GSVector4(dRect), ps_shader, linear);

	bool ret = DownloadTexture(dst, dRect, out_map);
	Recycle(dst);
	return ret;
}

void GSDevice::StretchRect(GSTexture* sTex, GSTexture* dTex, const GSVector4& dRect, ShaderConvert shader, bool linear)
{
	StretchRect(sTex, GSVector4(0, 0, 1, 1), dTex, dRect, shader, linear);
}

void GSDevice::ClearCurrent()
{
	m_current = nullptr;

	delete m_merge;
	delete m_weavebob;
	delete m_blend;
	delete m_target_tmp;

	m_merge = nullptr;
	m_weavebob = nullptr;
	m_blend = nullptr;
	m_target_tmp = nullptr;
}

void GSDevice::Merge(GSTexture* sTex[3], GSVector4* sRect, GSVector4* dRect, const GSVector2i& fs, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c)
{
	// KH:COM crashes at startup when booting *through the bios* due to m_merge being NULL.
	// (texture appears to be non-null, and is being re-created at a size around like 1700x340,
	// dunno if that's relevant) -- air

	if (ResizeTarget(&m_merge, fs.x, fs.y))
	{
		GSTexture* tex[3] = {NULL, NULL, NULL};

		for (size_t i = 0; i < std::size(tex); i++)
		{
			if (sTex[i] != NULL)
			{
				tex[i] = sTex[i];
			}
		}

		DoMerge(tex, sRect, m_merge, dRect, PMODE, EXTBUF, c);

		for (size_t i = 0; i < std::size(tex); i++)
		{
			if (tex[i] != sTex[i])
			{
				Recycle(tex[i]);
			}
		}
	}
	else
	{
		printf("GS: m_merge is NULL!\n");
	}

	m_current = m_merge;
}

void GSDevice::Interlace(const GSVector2i& ds, int field, int mode, float yoffset)
{
	ResizeTarget(&m_weavebob, ds.x, ds.y);

	if (mode == 0 || mode == 2) // weave or blend
	{
		// weave first
		const int offset = static_cast<int>(yoffset) * (1 - field);

		DoInterlace(m_merge, m_weavebob, field, false, GSConfig.DisableInterlaceOffset ? 0 : offset);

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
	const GSVector2i s = m_current->GetSize();

	if (ResizeTarget(&m_target_tmp))
	{
		const GSVector4 sRect(0, 0, 1, 1);
		const GSVector4 dRect(0, 0, s.x, s.y);

		StretchRect(m_current, sRect, m_target_tmp, dRect, ShaderConvert::TRANSPARENCY_FILTER, false);
		DoExternalFX(m_target_tmp, m_current);
	}
}

void GSDevice::FXAA()
{
	const GSVector2i s = m_current->GetSize();

	if (ResizeTarget(&m_target_tmp))
	{
		const GSVector4 sRect(0, 0, 1, 1);
		const GSVector4 dRect(0, 0, s.x, s.y);

		StretchRect(m_current, sRect, m_target_tmp, dRect, ShaderConvert::TRANSPARENCY_FILTER, false);
		DoFXAA(m_target_tmp, m_current);
	}
}

void GSDevice::ShadeBoost()
{
	const GSVector2i s = m_current->GetSize();

	if (ResizeTarget(&m_target_tmp))
	{
		const GSVector4 sRect(0, 0, 1, 1);
		const GSVector4 dRect(0, 0, s.x, s.y);

		// predivide to avoid the divide (multiply) in the shader
		const float params[4] = {
			static_cast<float>(GSConfig.ShadeBoost_Brightness) * (1.0f / 50.0f),
			static_cast<float>(GSConfig.ShadeBoost_Contrast) * (1.0f / 50.0f),
			static_cast<float>(GSConfig.ShadeBoost_Saturation) * (1.0f / 50.0f),
		};

		StretchRect(m_current, sRect, m_target_tmp, dRect, ShaderConvert::COPY, false);
		DoShadeBoost(m_target_tmp, m_current, params);
	}
}

bool GSDevice::ResizeTexture(GSTexture** t, GSTexture::Type type, int w, int h, bool clear, bool prefer_reuse)
{
	if (t == NULL)
	{
		ASSERT(0);
		return false;
	}

	GSTexture* t2 = *t;

	if (t2 == NULL || t2->GetWidth() != w || t2->GetHeight() != h)
	{
		const GSTexture::Format fmt = t2 ? t2->GetFormat() : GetDefaultTextureFormat(type);
		const int levels = t2 ? (t2->IsMipmap() ? MipmapLevelsForSize(w, h) : 1) : 1;
		delete t2;

		t2 = FetchSurface(type, w, h, levels, fmt, clear, prefer_reuse);

		*t = t2;
	}

	return t2 != NULL;
}

bool GSDevice::ResizeTexture(GSTexture** t, int w, int h, bool prefer_reuse)
{
	return ResizeTexture(t, GSTexture::Type::Texture, w, h, false, prefer_reuse);
}

bool GSDevice::ResizeTarget(GSTexture** t, int w, int h)
{
	return ResizeTexture(t, GSTexture::Type::RenderTarget, w, h);
}

bool GSDevice::ResizeTarget(GSTexture** t)
{
	GSVector2i s = m_current->GetSize();
	return ResizeTexture(t, GSTexture::Type::RenderTarget, s.x, s.y);
}

void GSDevice::SetHWDrawConfigForAlphaPass(GSHWDrawConfig::PSSelector* ps,
	GSHWDrawConfig::ColorMaskSelector* cms,
	GSHWDrawConfig::BlendState* bs,
	GSHWDrawConfig::DepthStencilSelector* dss)
{
	// only need to compute the alpha component (allow the shader to optimize better)
	ps->no_ablend = false;
	ps->only_alpha = true;

	// definitely don't need to compute software blend (this may get rid of some barriers)
	ps->blend_a = ps->blend_b = ps->blend_c = ps->blend_d = 0;

	// only write alpha (RGB=0,A=1)
	cms->wrgba = (1 << 3);

	// no need for hardware blending, since we're not writing RGB
	bs->enable = false;

	// if depth writes are on, we can optimize to an EQUAL test, otherwise we leave the tests alone
	// since the alpha channel isn't blended, the last fragment wins and this'll be okay
	if (dss->zwe)
	{
		dss->zwe = false;
		dss->ztst = ZTST_GEQUAL;
	}
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

// clang-format off

const std::array<u8, 16> GSDevice::m_replaceDualSrcBlendMap =
{{
	SRC_COLOR,        // SRC_COLOR
	INV_SRC_COLOR,    // INV_SRC_COLOR
	DST_COLOR,        // DST_COLOR
	INV_DST_COLOR,    // INV_DST_COLOR
	SRC_COLOR,        // SRC1_COLOR
	INV_SRC_COLOR,    // INV_SRC1_COLOR
	SRC_ALPHA,        // SRC_ALPHA
	INV_SRC_ALPHA,    // INV_SRC_ALPHA
	DST_ALPHA,        // DST_ALPHA
	INV_DST_ALPHA,    // INV_DST_ALPHA
	SRC_ALPHA,        // SRC1_ALPHA
	INV_SRC_ALPHA,    // INV_SRC1_ALPHA
	CONST_COLOR,      // CONST_COLOR
	INV_CONST_COLOR,  // INV_CONST_COLOR
	CONST_ONE,        // CONST_ONE
	CONST_ZERO        // CONST_ZERO
}};

const std::array<HWBlend, 3*3*3*3> GSDevice::m_blendMap =
{{
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0000: (Cs - Cs)*As + Cs ==> Cs
	{ BLEND_CD                   , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 0001: (Cs - Cs)*As + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 0002: (Cs - Cs)*As +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0010: (Cs - Cs)*Ad + Cs ==> Cs
	{ BLEND_CD                   , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 0011: (Cs - Cs)*Ad + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 0012: (Cs - Cs)*Ad +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0020: (Cs - Cs)*F  + Cs ==> Cs
	{ BLEND_CD                   , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 0021: (Cs - Cs)*F  + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 0022: (Cs - Cs)*F  +  0 ==> 0
	{ BLEND_A_MAX | BLEND_MIX2   , OP_SUBTRACT     , CONST_ONE       , SRC1_ALPHA}      , //*0100: (Cs - Cd)*As + Cs ==> Cs*(As + 1) - Cd*As
	{ BLEND_MIX1                 , OP_ADD          , SRC1_ALPHA      , INV_SRC1_ALPHA}  , // 0101: (Cs - Cd)*As + Cd ==> Cs*As + Cd*(1 - As)
	{ BLEND_MIX1                 , OP_SUBTRACT     , SRC1_ALPHA      , SRC1_ALPHA}      , // 0102: (Cs - Cd)*As +  0 ==> Cs*As - Cd*As
	{ BLEND_A_MAX                , OP_SUBTRACT     , CONST_ONE       , DST_ALPHA}       , //*0110: (Cs - Cd)*Ad + Cs ==> Cs*(Ad + 1) - Cd*Ad
	{ 0                          , OP_ADD          , DST_ALPHA       , INV_DST_ALPHA}   , // 0111: (Cs - Cd)*Ad + Cd ==> Cs*Ad + Cd*(1 - Ad)
	{ 0                          , OP_SUBTRACT     , DST_ALPHA       , DST_ALPHA}       , // 0112: (Cs - Cd)*Ad +  0 ==> Cs*Ad - Cd*Ad
	{ BLEND_A_MAX | BLEND_MIX2   , OP_SUBTRACT     , CONST_ONE       , CONST_COLOR}     , //*0120: (Cs - Cd)*F  + Cs ==> Cs*(F + 1) - Cd*F
	{ BLEND_MIX1                 , OP_ADD          , CONST_COLOR     , INV_CONST_COLOR} , // 0121: (Cs - Cd)*F  + Cd ==> Cs*F + Cd*(1 - F)
	{ BLEND_MIX1                 , OP_SUBTRACT     , CONST_COLOR     , CONST_COLOR}     , // 0122: (Cs - Cd)*F  +  0 ==> Cs*F - Cd*F
	{ BLEND_NO_REC | BLEND_A_MAX , OP_ADD          , CONST_ONE       , CONST_ZERO}      , //*0200: (Cs -  0)*As + Cs ==> Cs*(As + 1)
	{ BLEND_ACCU                 , OP_ADD          , SRC1_ALPHA      , CONST_ONE}       , //?0201: (Cs -  0)*As + Cd ==> Cs*As + Cd
	{ BLEND_NO_REC               , OP_ADD          , SRC1_ALPHA      , CONST_ZERO}      , // 0202: (Cs -  0)*As +  0 ==> Cs*As
	{ BLEND_A_MAX                , OP_ADD          , CONST_ONE       , CONST_ZERO}      , //*0210: (Cs -  0)*Ad + Cs ==> Cs*(Ad + 1)
	{ BLEND_C_CLR3               , OP_ADD          , DST_ALPHA       , CONST_ONE}       , // 0211: (Cs -  0)*Ad + Cd ==> Cs*Ad + Cd
	{ BLEND_C_CLR3               , OP_ADD          , DST_ALPHA       , CONST_ZERO}      , // 0212: (Cs -  0)*Ad +  0 ==> Cs*Ad
	{ BLEND_NO_REC | BLEND_A_MAX , OP_ADD          , CONST_ONE       , CONST_ZERO}      , //*0220: (Cs -  0)*F  + Cs ==> Cs*(F + 1)
	{ BLEND_ACCU                 , OP_ADD          , CONST_COLOR     , CONST_ONE}       , //?0221: (Cs -  0)*F  + Cd ==> Cs*F + Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_COLOR     , CONST_ZERO}      , // 0222: (Cs -  0)*F  +  0 ==> Cs*F
	{ BLEND_MIX3                 , OP_ADD          , INV_SRC1_ALPHA  , SRC1_ALPHA}      , // 1000: (Cd - Cs)*As + Cs ==> Cd*As + Cs*(1 - As)
	{ BLEND_A_MAX | BLEND_MIX1   , OP_REV_SUBTRACT , SRC1_ALPHA      , CONST_ONE}       , //*1001: (Cd - Cs)*As + Cd ==> Cd*(As + 1) - Cs*As
	{ BLEND_MIX1                 , OP_REV_SUBTRACT , SRC1_ALPHA      , SRC1_ALPHA}      , // 1002: (Cd - Cs)*As +  0 ==> Cd*As - Cs*As
	{ 0                          , OP_ADD          , INV_DST_ALPHA   , DST_ALPHA}       , // 1010: (Cd - Cs)*Ad + Cs ==> Cd*Ad + Cs*(1 - Ad)
	{ BLEND_A_MAX                , OP_REV_SUBTRACT , DST_ALPHA       , CONST_ONE}       , //*1011: (Cd - Cs)*Ad + Cd ==> Cd*(Ad + 1) - Cs*Ad
	{ 0                          , OP_REV_SUBTRACT , DST_ALPHA       , DST_ALPHA}       , // 1012: (Cd - Cs)*Ad +  0 ==> Cd*Ad - Cs*Ad
	{ BLEND_MIX3                 , OP_ADD          , INV_CONST_COLOR , CONST_COLOR}     , // 1020: (Cd - Cs)*F  + Cs ==> Cd*F + Cs*(1 - F)
	{ BLEND_A_MAX | BLEND_MIX1   , OP_REV_SUBTRACT , CONST_COLOR     , CONST_ONE}       , //*1021: (Cd - Cs)*F  + Cd ==> Cd*(F + 1) - Cs*F
	{ BLEND_MIX1                 , OP_REV_SUBTRACT , CONST_COLOR     , CONST_COLOR}     , // 1022: (Cd - Cs)*F  +  0 ==> Cd*F - Cs*F
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 1100: (Cd - Cd)*As + Cs ==> Cs
	{ BLEND_CD                   , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 1101: (Cd - Cd)*As + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 1102: (Cd - Cd)*As +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 1110: (Cd - Cd)*Ad + Cs ==> Cs
	{ BLEND_CD                   , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 1111: (Cd - Cd)*Ad + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 1112: (Cd - Cd)*Ad +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 1120: (Cd - Cd)*F  + Cs ==> Cs
	{ BLEND_CD                   , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 1121: (Cd - Cd)*F  + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 1122: (Cd - Cd)*F  +  0 ==> 0
	{ 0                          , OP_ADD          , CONST_ONE       , SRC1_ALPHA}      , // 1200: (Cd -  0)*As + Cs ==> Cs + Cd*As
	{ BLEND_C_CLR1               , OP_ADD          , DST_COLOR       , SRC1_ALPHA}      , //#1201: (Cd -  0)*As + Cd ==> Cd*(1 + As)
	{ BLEND_C_CLR2_AS            , OP_ADD          , DST_COLOR       , SRC1_ALPHA}      , // 1202: (Cd -  0)*As +  0 ==> Cd*As
	{ 0                          , OP_ADD          , CONST_ONE       , DST_ALPHA}       , // 1210: (Cd -  0)*Ad + Cs ==> Cs + Cd*Ad
	{ BLEND_C_CLR1               , OP_ADD          , DST_COLOR       , DST_ALPHA}       , //#1211: (Cd -  0)*Ad + Cd ==> Cd*(1 + Ad)
	{ 0                          , OP_ADD          , CONST_ZERO      , DST_ALPHA}       , // 1212: (Cd -  0)*Ad +  0 ==> Cd*Ad
	{ 0                          , OP_ADD          , CONST_ONE       , CONST_COLOR}     , // 1220: (Cd -  0)*F  + Cs ==> Cs + Cd*F
	{ BLEND_C_CLR1               , OP_ADD          , DST_COLOR       , CONST_COLOR}     , //#1221: (Cd -  0)*F  + Cd ==> Cd*(1 + F)
	{ BLEND_C_CLR2_AF            , OP_ADD          , DST_COLOR       , CONST_COLOR}     , // 1222: (Cd -  0)*F  +  0 ==> Cd*F
	{ BLEND_NO_REC               , OP_ADD          , INV_SRC1_ALPHA  , CONST_ZERO}      , // 2000: (0  - Cs)*As + Cs ==> Cs*(1 - As)
	{ BLEND_ACCU                 , OP_REV_SUBTRACT , SRC1_ALPHA      , CONST_ONE}       , //?2001: (0  - Cs)*As + Cd ==> Cd - Cs*As
	{ BLEND_NO_REC               , OP_REV_SUBTRACT , SRC1_ALPHA      , CONST_ZERO}      , // 2002: (0  - Cs)*As +  0 ==> 0 - Cs*As
	{ 0                          , OP_ADD          , INV_DST_ALPHA	 , CONST_ZERO}      , // 2010: (0  - Cs)*Ad + Cs ==> Cs*(1 - Ad)
	{ BLEND_C_CLR3               , OP_REV_SUBTRACT , DST_ALPHA       , CONST_ONE}       , // 2011: (0  - Cs)*Ad + Cd ==> Cd - Cs*Ad
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
	{ BLEND_CD                   , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 2201: (0  -  0)*As + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 2202: (0  -  0)*As +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 2210: (0  -  0)*Ad + Cs ==> Cs
	{ BLEND_CD                   , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 2211: (0  -  0)*Ad + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 2212: (0  -  0)*Ad +  0 ==> 0
	{ BLEND_NO_REC               , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 2220: (0  -  0)*F  + Cs ==> Cs
	{ BLEND_CD                   , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 2221: (0  -  0)*F  + Cd ==> Cd
	{ BLEND_NO_REC               , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 2222: (0  -  0)*F  +  0 ==> 0
}};
