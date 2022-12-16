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
#include "GLState.h"

namespace GLState
{
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

	GLuint rt;
	GLuint ds;
	GLuint tex_unit[8];
	GLuint64 tex_handle[8];

	s64 available_vram;

	void Clear()
	{
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

		rt = 0;
		ds = 0;
		std::fill(std::begin(tex_unit), std::end(tex_unit), 0);
		std::fill(std::begin(tex_handle), std::end(tex_handle), 0);

		// Set a max vram limit for texture allocation
		// (256MB are reserved for PBO/IBO/VBO/UBO buffers)
		available_vram = (4096u - 256u) * 1024u * 1024u;
	}
} // namespace GLState
