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

#define GL_TEX_LEVEL_0 (0)
#define GL_TEX_LEVEL_1 (1)
#define GL_FB_DEFAULT  (0)
#define GL_BUFFER_0    (0)

#include "glad.h"

namespace GLExtension
{
	extern bool Has(const std::string& ext);
	extern void Set(const std::string& ext, bool v = true);
} // namespace GLExtension

namespace GLLoader
{
	bool check_gl_requirements();

	extern bool vendor_id_amd;
	extern bool vendor_id_nvidia;
	extern bool vendor_id_intel;
	extern bool mesa_driver;
	extern bool in_replayer;

	// GL
	extern bool has_dual_source_blend;
	extern bool found_framebuffer_fetch;
	extern bool found_geometry_shader;
	extern bool found_GL_ARB_gpu_shader5;
	extern bool found_GL_ARB_shader_image_load_store;
	extern bool found_GL_ARB_clear_texture;

	extern bool found_compatible_GL_ARB_sparse_texture2;
	extern bool found_compatible_sparse_depth;
} // namespace GLLoader
