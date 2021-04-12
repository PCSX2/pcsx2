/*
 *	Copyright (C) 2014-2016 Gregory hainaut
 *	Copyright (C) 2016-2016 Jason Brown
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

#include "stdafx.h"
#include "GSVector.h"
#include "GSVertex.h"
#include "GSTexture.h"
#include <ft2build.h>
#include FT_FREETYPE_H

class GSOsdManager
{
	struct glyph_info
	{
		int32 ax; // advance.x
		int32 ay; // advance.y

		uint32 bw; // bitmap.width;
		uint32 bh; // bitmap.rows;

		int32 bl; // bitmap_left;
		int32 bt; // bitmap_top;

		float tx; // x offset of glyph
		float ty; // y offset of glyph
		float tw; // nomalized glyph width
	};

	std::map<char32_t, glyph_info> m_char_info;
	std::map<std::pair<char32_t, char32_t>, FT_Pos> m_kern_info;

	FT_Library m_library;
	FT_Face m_face;
	FT_UInt m_size;

	uint32 m_atlas_h;
	uint32 m_atlas_w;
	int32 m_max_width;
	int32 m_onscreen_messages;

	struct log_info
	{
		std::u32string msg;
		std::chrono::system_clock::time_point OnScreen;
	};
	std::vector<log_info> m_log;

	std::map<std::u32string, std::u32string> m_monitor;

	void AddGlyph(char32_t codepoint);
	void RenderGlyph(GSVertexPT1* dst, const glyph_info g, float x, float y, uint32 color);
	void RenderString(GSVertexPT1* dst, const std::u32string msg, float x, float y, uint32 color);
	float StringSize(const std::u32string msg);

	bool m_log_enabled;
	int m_log_timeout;
	bool m_monitor_enabled;
	int m_opacity;
	uint32 m_color;
	int m_max_onscreen_messages;

public:
	GSOsdManager();
	~GSOsdManager();

	void LoadFont();
	void LoadSize();

	GSVector2i get_texture_font_size();

	bool m_texture_dirty;
	void upload_texture_atlas(GSTexture* t);

	void Log(const char* utf8);
	void Monitor(const char* key, const char* value);

	GSVector2i m_real_size;
	size_t Size();
	size_t GeneratePrimitives(GSVertexPT1* dst, size_t count);

private:
	std::vector<char> resource_data_buffer;
};
