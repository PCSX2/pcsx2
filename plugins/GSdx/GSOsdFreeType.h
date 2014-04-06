/*
 *	Copyright (C) 2014-2014 Gregory hainaut
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
#include "GSTexture.h"
#include <ft2build.h> 
#include FT_FREETYPE_H 

class OsdManager {
	struct character_info {
		uint32 ax; // advance.x
		uint32 ay; // advance.y

		uint32 bw; // bitmap.width;
		uint32 bh; // bitmap.rows;

		uint32 bl; // bitmap_left;
		uint32 bt; // bitmap_top;

		uint32 tx; // x offset of glyph
	} c_info[128];

	FT_Library library; 
	FT_Face	   face;

	uint32 atlas_h;
	uint32 atlas_w;

	const uint32 ascii_start;
	const uint32 ascii_stop;

	void compute_glyph_size();


	public:

	OsdManager();
	~OsdManager();

	GSVector2i get_texture_font_size();
	void upload_texture_atlas(GSTexture* t);
	void text_to_vertex(GSVector4* dst, const char* text, GSVector4 r);
};
