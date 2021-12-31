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

#include "GS/Renderers/OpenGL/GLLoader.h"
#include "GS/GSVector.h"

namespace GLState
{
	extern GLuint fbo; // frame buffer object
	extern GSVector2i viewport;
	extern GSVector4i scissor;

	extern bool point_size;
	extern float line_width;

	extern bool blend;
	extern u16 eq_RGB;
	extern u16 f_sRGB;
	extern u16 f_dRGB;
	extern u8 bf;
	extern u8 wrgba;

	extern bool depth;
	extern GLenum depth_func;
	extern bool depth_mask;

	extern bool stencil;
	extern GLenum stencil_func;
	extern GLenum stencil_pass;

	extern GLuint ps_ss; // sampler

	extern GLuint rt; // render target
	extern GLuint ds; // Depth-Stencil
	extern GLuint tex_unit[8]; // shader input texture
	extern GLuint64 tex_handle[8]; // shader input texture

	extern s64 available_vram;

	extern void Clear();
} // namespace GLState
