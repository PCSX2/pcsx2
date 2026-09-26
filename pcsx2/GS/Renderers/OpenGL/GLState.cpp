// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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
	u16 f_sA;
	u16 f_dA;
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
	GSTextureOGL* ds_as_rt = nullptr;
	GSTextureOGL* ds = nullptr;

	bool rt_written;
	bool ds_as_rt_written;
	bool ds_written;

	u32 draw_buffers;

	GLuint tex_unit[8];

	u32 UpdateDrawBuffers()
	{
		draw_buffers = ds_as_rt ? 2 : 1;
		return draw_buffers;
	}

	void Init()
	{
		// Set the state to match initial API values.
		vao = 0;
		fbo = 0;
		viewport = GSVector2i(1, 1);
		scissor = GSVector4i(0, 0, 1, 1);

		blend = false;
		eq_RGB = GL_FUNC_ADD;
		f_sRGB = GL_ONE;
		f_dRGB = GL_ZERO;
		f_sA = GL_ONE;
		f_dA = GL_ZERO;
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
		ds_as_rt = nullptr;
		ds = nullptr;

		rt_written = false;
		ds_as_rt_written = false;
		ds_written = false;

		draw_buffers = 0;

		std::fill(std::begin(tex_unit), std::end(tex_unit), 0);
	}

	void Invalidate()
	{
		Init();

		// Set the state to invalid values (or fetch from the API).
		blend = glIsEnabled(GL_BLEND);
		eq_RGB = 0xFFFF;
		f_sRGB = 0xFFFF;
		f_dRGB = 0xFFFF;
		f_sA = 0xFFFF;
		f_dA = 0xFFFF;
		{
			// We only track a single value for all channels.
			// So instead reset back to default values.
			glBlendColor(0, 0, 0, 0);
		}
		wrgba = 0xFF;

		depth = glIsEnabled(GL_DEPTH_TEST);
		depth_func = 0xFFFFFFFF;
		{
			GLboolean mask;
			glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
			depth_mask = mask;
		}

		stencil = glIsEnabled(GL_STENCIL_TEST);
		stencil_func = 0xFFFFFFFF;
		stencil_pass = 0xFFFFFFFF;
	}
} // namespace GLState
