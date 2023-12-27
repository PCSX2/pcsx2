// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/SW/GSTextureSW.h"
#include "GS/GSExtra.h"
#include "GS/GSPng.h"

GSTextureSW::GSTextureSW(Type type, int width, int height)
{
	m_mapped.clear(std::memory_order_release);
	m_size = GSVector2i(width, height);
	m_type = type;
	m_format = Format::Invalid;
	m_pitch = ((width << 2) + 31) & ~31;
	m_data = _aligned_malloc(m_pitch * height, VECTOR_ALIGNMENT);
}

GSTextureSW::~GSTextureSW()
{
	_aligned_free(m_data);
}

void* GSTextureSW::GetNativeHandle() const
{
	return nullptr;
}

bool GSTextureSW::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	GSMap m;

	if (m_data != NULL && Map(m, &r))
	{
		u8* RESTRICT src = (u8*)data;
		u8* RESTRICT dst = m.bits;

		int rowbytes = r.width() << 2;

		for (int h = r.height(); h > 0; h--, src += pitch, dst += m.pitch)
		{
			memcpy(dst, src, rowbytes);
		}

		Unmap();

		return true;
	}

	return false;
}

bool GSTextureSW::Map(GSMap& m, const GSVector4i* r, int layer)
{
	GSVector4i r2 = r != NULL ? *r : GSVector4i(0, 0, m_size.x, m_size.y);

	if (m_data != NULL && r2.left >= 0 && r2.right <= m_size.x && r2.top >= 0 && r2.bottom <= m_size.y)
	{
		if (!m_mapped.test_and_set(std::memory_order_acquire))
		{
			m.bits = (u8*)m_data + m_pitch * r2.top + (r2.left << 2);
			m.pitch = m_pitch;

			return true;
		}
	}

	return false;
}

void GSTextureSW::Unmap()
{
	m_mapped.clear(std::memory_order_release);
}

bool GSTextureSW::Save(const std::string& fn)
{
#ifdef PCSX2_DEVBUILD
	GSPng::Format fmt = GSPng::RGB_A_PNG;
#else
	GSPng::Format fmt = GSPng::RGB_PNG;
#endif
	int compression = GSConfig.PNGCompressionLevel;
	return GSPng::Save(fmt, fn, static_cast<u8*>(m_data), m_size.x, m_size.y, m_pitch, compression);
}

void GSTextureSW::Swap(GSTexture* tex)
{
	GSTexture::Swap(tex);
	std::swap(m_pitch, static_cast<GSTextureSW*>(tex)->m_pitch);
	std::swap(m_data, static_cast<GSTextureSW*>(tex)->m_data);
	// std::swap(m_mapped, static_cast<GSTextureSW*>(tex)->m_mapped);
}
