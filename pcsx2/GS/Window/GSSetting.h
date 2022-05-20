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

#pragma once

#include "PrecompiledHeader.h"

struct GSSetting
{
	int32_t value;
	std::string name;
	std::string note;

	template <typename T>
	explicit GSSetting(T value, const char* name, const char* note)
		: value(static_cast<int32_t>(value))
		, name(name)
		, note(note)
	{
	}
};

#ifdef _WIN32
const wchar_t* dialog_message(int ID, bool* updateText = NULL);
#else
const char* dialog_message(int ID, bool* updateText = NULL);
#endif

enum
{
	// Renderer
	IDC_FILTER,
	IDC_PCRTC_OFFSETS,
	IDC_DISABLE_INTERLACE_OFFSETS,
	// Hardware Renderer
	IDC_PRELOAD_TEXTURES,
	IDC_ACCURATE_DATE,
	IDC_PALTEX,
	IDC_CONSERVATIVE_FB,
	IDC_AFCOMBO,
	IDC_DITHERING,
	IDC_MIPMAP_HW,
	IDC_CRC_LEVEL,
	IDC_ACCURATE_BLEND_UNIT,
	// Rendering Hacks
	IDC_AUTO_FLUSH_HW,
	IDC_TC_DEPTH,
	IDC_SAFE_FEATURES,
	IDC_DISABLE_PARTIAL_TC_INV,
	IDC_CPU_FB_CONVERSION,
	IDC_MEMORY_WRAPPING,
	IDC_PRELOAD_GS,
	IDC_HALF_SCREEN_TS,
	IDC_TRI_FILTER,
	IDC_SKIPDRAWEND,
	IDC_SKIPDRAWHACKEDIT,
	IDC_SKIPDRAWSTART,
	IDC_SKIPDRAWOFFSETEDIT,
	IDC_TEX_IN_RT,
	// Upscaling Hacks
	IDC_ALIGN_SPRITE,
	IDC_MERGE_PP_SPRITE,
	IDC_WILDHACK,
	IDC_OFFSETHACK,
	IDC_ROUND_SPRITE,
	IDC_TCOFFSETX,
	IDC_TCOFFSETX2,
	IDC_TCOFFSETY,
	IDC_TCOFFSETY2,
	// Software Renderer
	IDC_AUTO_FLUSH_SW,
	IDC_AA1,
	IDC_MIPMAP_SW,
	IDC_SWTHREADS,
	IDC_SWTHREADS_EDIT,
	// OpenGL Advanced Settings
	IDC_GEOMETRY_SHADER_OVERRIDE,
	IDC_IMAGE_LOAD_STORE,
	IDC_SPARSE_TEXTURE,
	// On-screen Display
	IDC_OSD_LOG,
	IDC_OSD_MONITOR,
	IDC_OSD_MAX_LOG,
	IDC_OSD_MAX_LOG_EDIT,
	// Shader Configuration
	IDC_SHADEBOOST,
	IDC_SHADER_FX,
	IDC_FXAA,
	IDC_LINEAR_PRESENT,
};
