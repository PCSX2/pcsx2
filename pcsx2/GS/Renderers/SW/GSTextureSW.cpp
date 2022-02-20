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
#include "GSTextureSW.h"
#include "GS/GSPng.h"

GSTextureSW::GSTextureSW(Type type, int width, int height)
{
	m_mapped.clear(std::memory_order_release);
	m_size = GSVector2i(width, height);
	m_type = type;
	m_format = Format::Invalid;
	m_pitch = ((width << 2) + 31) & ~31;
	m_data = _aligned_malloc(m_pitch * height, 32);
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
	int compression = theApp.GetConfigI("png_compression_level");
	return GSPng::Save(fmt, fn, static_cast<u8*>(m_data), m_size.x, m_size.y, m_pitch, compression);
}

void GSTextureSW::Swap(GSTexture* tex)
{
	GSTexture::Swap(tex);
	std::swap(m_pitch, static_cast<GSTextureSW*>(tex)->m_pitch);
	std::swap(m_data, static_cast<GSTextureSW*>(tex)->m_data);
	// std::swap(m_mapped, static_cast<GSTextureSW*>(tex)->m_mapped);
}
