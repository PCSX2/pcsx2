/*
 *	Copyright (C) 2007-2015 Gabest
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

#include "stdafx.h"

struct GSSetting
{
	int32_t value;
	std::string name;
	std::string note;

	template< typename T>
	explicit GSSetting(T value, const char* name, const char* note) :
		value(static_cast<int32_t>(value)),
		name(name),
		note(note)
	{
	}
};

const char* dialog_message(int ID, bool* updateText = NULL);

#ifndef _WIN32
enum {
	IDC_FILTER,
	IDC_TRI_FILTER,
	IDC_SKIPDRAWHACK,
	IDC_SKIPDRAWHACKEDIT,
	IDC_ALPHAHACK,
	IDC_OFFSETHACK,
	IDC_SPRITEHACK,
	IDC_WILDHACK,
	IDC_MSAACB,
	IDC_ALPHASTENCIL,
	IDC_CHECK_DISABLE_ALL_HACKS,
	IDC_ALIGN_SPRITE,
	IDC_ROUND_SPRITE,
	IDC_TCOFFSETX,
	IDC_TCOFFSETX2,
	IDC_TCOFFSETY,
	IDC_TCOFFSETY2,
	IDC_PALTEX,
	IDC_ACCURATE_BLEND_UNIT,
	IDC_ACCURATE_DATE,
	IDC_TC_DEPTH,
	IDC_CPU_FB_CONVERSION,
	IDC_CRC_LEVEL,
	IDC_AFCOMBO,
	IDC_AA1,
	IDC_SWTHREADS,
	IDC_SWTHREADS_EDIT,
	IDC_SHADEBOOST,
	IDC_SHADER_FX,
	IDC_FXAA,
	IDC_MIPMAP_SW,
	IDC_MIPMAP_HW,
	IDC_PRELOAD_GS,
	IDC_FAST_TC_INV,
	IDC_LARGE_FB,
	IDC_LINEAR_PRESENT,
	IDC_AUTO_FLUSH,
	IDC_UNSCALE_POINT_LINE,
	IDC_MEMORY_WRAPPING,
	IDC_MERGE_PP_SPRITE,
	IDC_GEOMETRY_SHADER_OVERRIDE,
	IDC_IMAGE_LOAD_STORE,
	IDC_OSD_LOG,
	IDC_OSD_MONITOR,
	IDC_OSD_MAX_LOG,
	IDC_OSD_MAX_LOG_EDIT,
	// Shader
	IDR_CONVERT_GLSL,
	IDR_FXAA_FX,
	IDR_INTERLACE_GLSL,
	IDR_MERGE_GLSL,
	IDR_SHADEBOOST_GLSL,
	IDR_COMMON_GLSL,
	IDR_TFX_VGS_GLSL,
	IDR_TFX_FS_GLSL,
	IDR_TFX_CL,
};
#endif
