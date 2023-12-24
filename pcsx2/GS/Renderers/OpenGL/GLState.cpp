// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GLState.h"

#include <array>

namespace GLState
{
	GLuint vao;
	GLuint fbo;
	GSVector2i viewport;
	GSVector4i scissor;

	bool point_size = false;
	float line_width = 1.0f;

	bool blend;
	u16 eq_RGB;
	u16 f_sRGB;
	u16 f_dRGB;
	u8 bf;
	u8 wrgba;

	bool depth;
	GLenum depth_func;
	bool depth_mask;

	bool stencil;
	GLenum stencil_func;
	GLenum stencil_pass;

	GLuint ps_ss;

	GSTextureOGL* rt = nullptr;
	GSTextureOGL* ds = nullptr;
	GLuint tex_unit[8];
	GLuint64 tex_handle[8];

	void Clear()
	{
		vao = 0;
		fbo = 0;
		viewport = GSVector2i(1, 1);
		scissor = GSVector4i(0, 0, 1, 1);

		blend = false;
		eq_RGB = GL_FUNC_ADD;
		f_sRGB = GL_ONE;
		f_dRGB = GL_ZERO;
		bf = 0;
		wrgba = 0xF;

		depth = false;
		depth_func = GL_LESS;
		depth_mask = false;

		stencil = false;
		stencil_func = GL_ALWAYS;
		stencil_pass = GL_KEEP;

		ps_ss = 0;

		rt = nullptr;
		ds = nullptr;
		std::fill(std::begin(tex_unit), std::end(tex_unit), 0);
		std::fill(std::begin(tex_handle), std::end(tex_handle), 0);
	}
} // namespace GLState
