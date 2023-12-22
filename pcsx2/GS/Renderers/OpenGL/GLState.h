// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/GSVector.h"

#include "glad.h"

class GSTextureOGL;

namespace GLState
{
	extern GLuint vao; // vertex array object
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

	extern GSTextureOGL* rt; // render target
	extern GSTextureOGL* ds; // Depth-Stencil
	extern GLuint tex_unit[8]; // shader input texture
	extern GLuint64 tex_handle[8]; // shader input texture

	extern void Clear();
} // namespace GLState
