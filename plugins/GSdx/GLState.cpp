/*
 *	Copyright (C) 2011-2013 Gregory hainaut
 *	Copyright (C) 2007-2009 Gabest
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

#include "stdafx.h"
#include "GLState.h"

namespace GLState {
	GLuint fbo;
	GSVector2i viewport;
	GSVector4i scissor;

	bool blend;
	GLenum eq_RGB;
	GLenum f_sRGB;
	GLenum f_dRGB;
	uint32 wrgba;

	float bf;

	bool depth;
	GLenum depth_func;
	bool depth_mask;

	bool stencil;
	GLenum stencil_func;
	GLenum stencil_pass;

	GLuint ubo;

	GLuint ps_ss;

	GLuint rt;
	GLuint ds;
	GLuint tex_unit[4];
	GLuint64 tex_handle[4];
	bool dirty_ressources;

	GLuint ps;
	GLuint gs;
	GLuint vs;
	GLuint program;
	bool dirty_prog;
	bool dirty_subroutine_vs;
	bool dirty_subroutine_ps;
#if 0
	struct {
		GSVertexBufferStateOGL* vb;
		GSDepthStencilOGL* dss;
		GSBlendStateOGL* bs;
		float bf; // blend factor
	} m_state;
#endif

	void Clear() {
		fbo = 0;
		viewport = GSVector2i(0, 0);
		scissor = GSVector4i(0, 0, 0, 0);

		blend = false;
		eq_RGB = 0;
		f_sRGB = 0;
		f_dRGB = 0;
		wrgba = 0xF;
		bf = 0.0;

		depth = false;
		depth_func = 0;
		depth_mask = false;

		stencil = false;
		stencil_func = 0;
		stencil_pass = 0;

		ubo = 0;

		ps_ss = 0;

		rt = 0;
		ds = 0;
		for (size_t i = 0; i < countof(tex_unit); i++)
			tex_unit[i] = 0;
		for (size_t i = 0; i < countof(tex_handle); i++)
			tex_handle[i] = 0;

		ps = 0;
		gs = 0;
		vs = 0;
		program = 0;
		dirty_prog = false;
		dirty_subroutine_vs = false;
		dirty_subroutine_ps = false;
		dirty_ressources = false;
	}
}
