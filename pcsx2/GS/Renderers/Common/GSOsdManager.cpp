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
#include "GSOsdManager.h"
#include "GS/GS.h"
#ifdef _WIN32
#include "GS/resource.h"
#endif

#if __GNUC__ > 5 || (__GNUC__ == 5 && __GNUC_MINOR__ >= 4)
#include <codecvt>
#include <locale>
#endif

void GSOsdManager::LoadFont()
{
	FT_Error error = FT_New_Face(m_library, theApp.GetConfigS("osd_fontname").c_str(), 0, &m_face);
	if (error)
	{
		FT_Error error_load_res = 1;
		if (theApp.LoadResource(IDR_FONT_ROBOTO, resource_data_buffer))
			error_load_res = FT_New_Memory_Face(m_library, (const FT_Byte*)resource_data_buffer.data(), resource_data_buffer.size(), 0, &m_face);

		if (error_load_res)
		{
			m_face = NULL;
			fprintf(stderr, "Failed to init freetype face from external and internal resource\n");
			if (error == FT_Err_Unknown_File_Format)
				fprintf(stderr, "\tFreetype unknown file format for external file\n");
			return;
		}
	}

	LoadSize();
}

void GSOsdManager::LoadSize()
{
	if (!m_face)
		return;

	FT_Error error = FT_Set_Pixel_Sizes(m_face, 0, m_size);
	if (error)
	{
		fprintf(stderr, "Failed to init the face size\n");
		return;
	}

	/* This is not exact, I'm sure there's some convoluted way to determine these
	 * from FreeType but they don't make it easy. */
	m_atlas_w = m_size * 96; // random guess
	m_atlas_h = m_size + 10; // another random guess
}

GSOsdManager::GSOsdManager()
	: m_atlas_h(0)
	, m_atlas_w(0)
	, m_max_width(0)
	, m_onscreen_messages(0)
	, m_texture_dirty(true)
{
	m_monitor_enabled       = theApp.GetConfigB("osd_monitor_enabled");
	m_log_enabled           = theApp.GetConfigB("osd_log_enabled");
	m_size                  = std::clamp(theApp.GetConfigI("osd_fontsize"), 1, 100);
	m_opacity               = std::clamp(theApp.GetConfigI("osd_color_opacity"), 0, 100);
	m_log_timeout           = std::clamp(theApp.GetConfigI("osd_log_timeout"), 2, 10);
	m_max_onscreen_messages = std::clamp(theApp.GetConfigI("osd_max_log_messages"), 1, 20);

	const int r = std::clamp(theApp.GetConfigI("osd_color_r"), 0, 255);
	const int g = std::clamp(theApp.GetConfigI("osd_color_g"), 0, 255);
	const int b = std::clamp(theApp.GetConfigI("osd_color_b"), 0, 255);

	m_color = r | (g << 8) | (b << 16) | (255 << 24);

	if (FT_Init_FreeType(&m_library))
	{
		m_face = NULL;
		fprintf(stderr, "Failed to init the freetype library\n");
		return;
	}

	LoadFont();

	/* The space character's width is used in GeneratePrimitives() */
	AddGlyph(' ');
}

GSOsdManager::~GSOsdManager()
{
	FT_Done_FreeType(m_library);
}

GSVector2i GSOsdManager::get_texture_font_size()
{
	return GSVector2i(m_atlas_w, m_atlas_h);
}

void GSOsdManager::upload_texture_atlas(GSTexture* t)
{
	if (!m_face)
		return;

	if (m_char_info.size() > 96) // we only reserved space for this many glyphs
		fprintf(stderr, "More than 96 glyphs needed for OSD");

	// This can be sped up a bit by only uploading new glyphs
	int x = 0;
	for (auto& pair : m_char_info)
	{
		if (FT_Load_Char(m_face, pair.first, FT_LOAD_RENDER))
		{
			fprintf(stderr, "failed to load char U%d\n", (int)pair.first);
			continue;
		}

		// Size of char
		pair.second.ax = m_face->glyph->advance.x >> 6;
		pair.second.ay = m_face->glyph->advance.y >> 6;

		pair.second.bw = m_face->glyph->bitmap.width;
		pair.second.bh = m_face->glyph->bitmap.rows;

		pair.second.bl = m_face->glyph->bitmap_left;
		pair.second.bt = m_face->glyph->bitmap_top;

		GSVector4i r(x, 0, x + pair.second.bw, pair.second.bh);
		if (r.width())
			t->Update(r, m_face->glyph->bitmap.buffer, m_face->glyph->bitmap.pitch);

		if (r.width() > m_max_width)
			m_max_width = r.width();

		pair.second.tx = (float)x / m_atlas_w;
		pair.second.ty = (float)pair.second.bh / m_atlas_h;
		pair.second.tw = (float)pair.second.bw / m_atlas_w;

		x += pair.second.bw;
	}

	m_texture_dirty = false;
}

#if __GNUC__ < 5 || (__GNUC__ == 5 && __GNUC_MINOR__ < 4)
/* This is dumb in that it doesn't check for malformed UTF8. This function
 * is not expected to operate on user input, but only on compiled in strings */
void dumb_utf8_to_utf32(const char* utf8, char32_t* utf32, unsigned size)
{
	while (*utf8 && --size)
	{
		if ((*utf8 & 0xF1) == 0xF0)
		{
			*utf32++ = (utf8[0] & 0x07) << 18 | (utf8[1] & 0x3F) << 12 | (utf8[2] & 0x3F) << 6 | utf8[3] & 0x3F;
			utf8 += 4;
		}
		else if ((*utf8 & 0xF0) == 0xE0)
		{
			*utf32++ =                          (utf8[0] & 0x0F) << 12 | (utf8[1] & 0x3F) << 6 | utf8[2] & 0x3F;
			utf8 += 3;
		}
		else if ((*utf8 & 0xE0) == 0xC0)
		{
			*utf32++ =                                                   (utf8[0] & 0x1F) << 6 | utf8[1] & 0x3F;
			utf8 += 2;
		}
		else if ((*utf8 & 0x80) == 0x00)
		{
			*utf32++ =                                                                           utf8[0] & 0x7F;
			utf8 += 1;
		}
	}

	if (size)
		*utf32 = *utf8; // Copy NUL char
}
#endif

void GSOsdManager::AddGlyph(char32_t codepoint)
{
	if (!m_face)
		return;
	if (m_char_info.count(codepoint) == 0)
	{
		m_texture_dirty = true;
		m_char_info[codepoint]; // add it
		if (FT_HAS_KERNING(m_face))
		{
			FT_UInt new_glyph = FT_Get_Char_Index(m_face, codepoint);
			for (auto pair : m_char_info)
			{
				FT_Vector delta;

				FT_UInt glyph_index = FT_Get_Char_Index(m_face, pair.first);
				FT_Get_Kerning(m_face, glyph_index, new_glyph, FT_KERNING_DEFAULT, &delta);
				m_kern_info[std::make_pair(pair.first, codepoint)] = delta.x >> 6;
			}
		}
	}
}

void GSOsdManager::Log(const char* utf8)
{
	if (!m_log_enabled)
		return;

#if __GNUC__ < 5 || (__GNUC__ == 5 && __GNUC_MINOR__ < 4)
	char32_t buffer[256];
	dumb_utf8_to_utf32(utf8, buffer, std::size(buffer));
	for (char32_t* c = buffer; *c; ++c)
		AddGlyph(*c);
#else
#if _MSC_VER == 1900
	std::wstring_convert<std::codecvt_utf8<unsigned int>, unsigned int> conv;
#else
	std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
#endif
	std::u32string buffer = conv.from_bytes(utf8);
	for (auto const& c : buffer)
		AddGlyph(c);
#endif
	m_onscreen_messages++;
	m_log.push_back(log_info{buffer, std::chrono::system_clock::time_point()});
}

void GSOsdManager::Monitor(const char* key, const char* value)
{
	if (!m_monitor_enabled)
		return;

	if (value && *value)
	{
#if __GNUC__ < 5 || (__GNUC__ == 5 && __GNUC_MINOR__ < 4)
		char32_t buffer[256], vbuffer[256];
		dumb_utf8_to_utf32(key, buffer, std::size(buffer));
		dumb_utf8_to_utf32(value, vbuffer, std::size(vbuffer));
		for (char32_t* c = buffer; *c; ++c)
			AddGlyph(*c);
		for (char32_t* c = vbuffer; *c; ++c)
			AddGlyph(*c);
#else
#if _MSC_VER == 1900
		std::wstring_convert<std::codecvt_utf8<unsigned int>, unsigned int> conv;
#else
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
#endif
		std::u32string buffer = conv.from_bytes(key);
		std::u32string vbuffer = conv.from_bytes(value);
		for (auto const& c : buffer)
			AddGlyph(c);
		for (auto const& c : vbuffer)
			AddGlyph(c);
#endif
		m_monitor[buffer] = vbuffer;
	}
	else
	{
#if __GNUC__ < 5 || (__GNUC__ == 5 && __GNUC_MINOR__ < 4)
		char32_t buffer[256];
		dumb_utf8_to_utf32(key, buffer, std::size(buffer));
#else
#if _MSC_VER == 1900
		std::wstring_convert<std::codecvt_utf8<unsigned int>, unsigned int> conv;
#else
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
#endif
		std::u32string buffer = conv.from_bytes(key);
#endif
		m_monitor.erase(buffer);
	}
}

void GSOsdManager::RenderGlyph(GSVertexPT1* dst, const glyph_info g, float x, float y, u32 color)
{
	float x2 = x + g.bl * (2.0f / m_real_size.x);
	float y2 = -y - g.bt * (2.0f / m_real_size.y);
	float w = g.bw * (2.0f / m_real_size.x);
	float h = g.bh * (2.0f / m_real_size.y);

	dst->p = GSVector4(x2    , -y2    , 0.0f, 1.0f);
	dst->t = GSVector2(g.tx       , 0.0f);
	dst->c = color;
	++dst;
	dst->p = GSVector4(x2 + w, -y2    , 0.0f, 1.0f);
	dst->t = GSVector2(g.tx + g.tw, 0.0f);
	dst->c = color;
	++dst;
	dst->p = GSVector4(x2    , -y2 - h, 0.0f, 1.0f);
	dst->t = GSVector2(g.tx       , g.ty);
	dst->c = color;
	++dst;
	dst->p = GSVector4(x2 + w, -y2    , 0.0f, 1.0f);
	dst->t = GSVector2(g.tx + g.tw, 0.0f);
	dst->c = color;
	++dst;
	dst->p = GSVector4(x2    , -y2 - h, 0.0f, 1.0f);
	dst->t = GSVector2(g.tx       , g.ty);
	dst->c = color;
	++dst;
	dst->p = GSVector4(x2 + w, -y2 - h, 0.0f, 1.0f);
	dst->t = GSVector2(g.tx + g.tw, g.ty);
	dst->c = color;
	++dst;
}

void GSOsdManager::RenderString(GSVertexPT1* dst, const std::u32string msg, float x, float y, u32 color)
{
	char32_t p = 0;
	for (const auto& c : msg)
	{
		if (p)
		{
			x += m_kern_info[std::make_pair(p, c)] * (2.0f / m_real_size.x);
		}

		RenderGlyph(dst, m_char_info[c], x, y, color);

		/* Advance the cursor to the start of the next character */
		x += m_char_info[c].ax * (2.0f / m_real_size.x);
		y += m_char_info[c].ay * (2.0f / m_real_size.y);

		dst += 6;

		p = c;
	}
}

size_t GSOsdManager::Size()
{
	size_t sum = 0;

	if (m_log_enabled)
	{
		float offset = 0;

		for (auto it = m_log.begin(); it != m_log.end(); ++it)
		{
			float y = 1 - ((m_size + 2) * (it - m_log.begin() + 1)) * (2.0f / m_real_size.y);
			if (y + offset < -1)
				break;

			std::chrono::duration<float> elapsed;
			if (it->OnScreen.time_since_epoch().count() == 0)
			{
				elapsed = std::chrono::seconds(0);
			}
			else
			{
				elapsed = std::chrono::system_clock::now() - it->OnScreen;
				if (elapsed > std::chrono::seconds(m_log_timeout) || m_onscreen_messages > m_max_onscreen_messages)
				{
					continue;
				}
			}

			float ratio = (elapsed - std::chrono::seconds(m_log_timeout / 2)).count() / std::chrono::seconds(m_log_timeout / 2).count();
			ratio = ratio > 1.0f ? 1.0f : ratio < 0.0f ? 0.0f : ratio;

			y += offset += ((m_size + 2) * (2.0f / m_real_size.y)) * ratio;
			sum += it->msg.size();
		}
	}

	if (m_monitor_enabled)
	{
		for (const auto& pair : m_monitor)
		{
			sum += pair.first.size();
			sum += pair.second.size();
		}
	}

	return sum * 6;
}

float GSOsdManager::StringSize(const std::u32string msg)
{
	char32_t p = 0;
	float x = 0.0;

	for (auto c : msg)
	{
		if (p)
		{
			x += m_kern_info[std::make_pair(p, c)] * (2.0f / m_real_size.x);
		}

		/* Advance the cursor to the start of the next character */
		x += m_char_info[c].ax * (2.0f / m_real_size.x);

		p = c;
	}

	return x;
}

size_t GSOsdManager::GeneratePrimitives(GSVertexPT1* dst, size_t count)
{
	size_t drawn = 0;
	float opacity = m_opacity * 0.01f;

	if (m_log_enabled)
	{
		float offset = 0;

		for (auto it = m_log.begin(); it != m_log.end();)
		{
			float x = -1 + 8 * (2.0f / m_real_size.x);
			float y = 1 - ((m_size + 2) * (it - m_log.begin() + 1)) * (2.0f / m_real_size.y);

			if (y + offset < -1)
				break;

			if (it->OnScreen.time_since_epoch().count() == 0)
				it->OnScreen = std::chrono::system_clock::now();

			std::chrono::duration<float> elapsed = std::chrono::system_clock::now() - it->OnScreen;
			if (elapsed > std::chrono::seconds(m_log_timeout) || m_onscreen_messages > m_max_onscreen_messages)
			{
				m_onscreen_messages--;
				it = m_log.erase(it);
				continue;
			}

			if (it->msg.size() * 6 > count - drawn)
				break;

			float ratio = (elapsed - std::chrono::seconds(m_log_timeout / 2)).count() / std::chrono::seconds(m_log_timeout / 2).count();
			ratio = ratio > 1.0f ? 1.0f : ratio < 0.0f ? 0.0f : ratio;

			y += offset += ((m_size + 2) * (2.0f / m_real_size.y)) * ratio;
			u32 color = m_color;
			((u8*)&color)[3] = (u8)(((u8*)&color)[3] * (1.0f - ratio) * opacity);
			RenderString(dst, it->msg, x, y, color);
			dst += it->msg.size() * 6;
			drawn += it->msg.size() * 6;
			++it;
		}
	}

	if (m_monitor_enabled)
	{
		// pair.first is the key and second is the value and color

		// Since the monitor is right justified, but we render from left to right
		// we need to find the longest string
		float first_max = 0.0, second_max = 0.0;
		for (const auto& pair : m_monitor)
		{
			float first_len = StringSize(pair.first);
			float second_len = StringSize(pair.second);

			first_max = first_max < first_len ? first_len : first_max;
			second_max = second_max < second_len ? second_len : second_max;
		}

		size_t line = 1;
		for (const auto& pair : m_monitor)
		{
			if ((pair.first.size() + pair.second.size()) * 6 > count - drawn)
				break;

			// Calculate where to start rendering from by taking the right most position 1.0
			// and subtracting (going left) 8 scaled pixels for a margin, then subtracting
			// the size of the longest key and subtracting a scaled space and finally
			// subtracting the longest value
			float x = 1.0f - 8 * (2.0f / m_real_size.x) - first_max - m_char_info[' '].ax * (2.0f / m_real_size.x) - second_max;
			float y = -1.0f + ((m_size + 2) * (2.0f / m_real_size.y)) * line++;

			u32 color = m_color;
			((u8*)&color)[3] = (u8)(((u8*)&color)[3] * opacity);

			// Render the key
			RenderString(dst, pair.first, x, y, color);
			dst += pair.first.size() * 6;
			drawn += pair.first.size() * 6;

			// Calculate the position for the value
			x = 1.0f - 8 * (2.0f / m_real_size.x) - second_max;

			// Render the value
			RenderString(dst, pair.second, x, y, color);
			dst += pair.second.size() * 6;
			drawn += pair.second.size() * 6;
		}
	}

	return drawn;
}
