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
#include "GSTextureSW.h"
#include "GSPng.h"

GSTextureSW::GSTextureSW(int type, int width, int height)
{
	m_mapped.clear(std::memory_order_release);
	m_size = GSVector2i(width, height);
	m_type = type;
	m_format = 0;
	m_pitch = ((width << 2) + 31) & ~31;
	m_data = _aligned_malloc(m_pitch * height, 32);
}

GSTextureSW::~GSTextureSW()
{
	_aligned_free(m_data);
}

bool GSTextureSW::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	GSMap m;

	if (m_data != NULL && Map(m, &r))
	{
		uint8* RESTRICT src = (uint8*)data;
		uint8* RESTRICT dst = m.bits;

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
			m.bits = (uint8*)m_data + m_pitch * r2.top + (r2.left << 2);
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
#ifdef ENABLE_OGL_DEBUG
	GSPng::Format fmt = GSPng::RGB_A_PNG;
#else
	GSPng::Format fmt = GSPng::RGB_PNG;
#endif
	int compression = theApp.GetConfigI("png_compression_level");
	return GSPng::Save(fmt, fn, static_cast<uint8*>(m_data), m_size.x, m_size.y, m_pitch, compression);
}
