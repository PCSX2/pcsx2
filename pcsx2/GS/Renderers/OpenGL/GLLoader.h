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

#ifdef __APPLE__
	#define GL_SILENCE_DEPRECATION
	#include <OpenGL/gl3.h>
#else
	#include <GL/gl.h>
	#include <GL/glext.h>
#endif

#define GL_TEX_LEVEL_0 (0)
#define GL_TEX_LEVEL_1 (1)
#define GL_FB_DEFAULT  (0)
#define GL_BUFFER_0    (0)

#ifndef GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR
#define GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR 0x00000008
#endif

/*************************************************************
 * Extra define not provided in glcorearb.h
 * Currently they are included in the legacy glext.h but the plan
 * is to move to core only OpenGL
 ************************************************************/

// Extension not in core profile. It will become irrelevant with sparse texture
#ifndef GL_TEXTURE_FREE_MEMORY_ATI
#define GL_TEXTURE_FREE_MEMORY_ATI 0x87FC
#endif
#ifndef GL_NVX_gpu_memory_info
#define GL_NVX_gpu_memory_info 1
#define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX 0x9047
#define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX 0x9048
#define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX 0x9049
#define GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX 0x904A
#define GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX 0x904B
#endif /* GL_NVX_gpu_memory_info */

// Added in GL4.6. Code should be updated but driver support...
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

// **********************  End of the extra header ******************* //

// #define ENABLE_GL_ARB_ES3_2_compatibility 1
// #define ENABLE_GL_ARB_bindless_texture 1
// #define ENABLE_GL_ARB_cl_event 1
// #define ENABLE_GL_ARB_compute_variable_group_size 1
// #define ENABLE_GL_ARB_debug_output 1
// #define ENABLE_GL_ARB_draw_buffers_blend 1
// #define ENABLE_GL_ARB_draw_instanced 1
// #define ENABLE_GL_ARB_geometry_shader4 1
// #define ENABLE_GL_ARB_gl_spirv 1
// #define ENABLE_GL_ARB_gpu_shader_int64 1
// #define ENABLE_GL_ARB_indirect_parameters 1
// #define ENABLE_GL_ARB_instanced_arrays 1
// #define ENABLE_GL_ARB_parallel_shader_compile 1
// #define ENABLE_GL_ARB_robustness 1
// #define ENABLE_GL_ARB_sample_locations 1
// #define ENABLE_GL_ARB_sample_shading 1
// #define ENABLE_GL_ARB_shading_language_include 1
// #define ENABLE_GL_ARB_sparse_buffer 1
#define ENABLE_GL_ARB_sparse_texture 1
// #define ENABLE_GL_ARB_texture_buffer_object 1
// #define ENABLE_GL_KHR_blend_equation_advanced 1
// #define ENABLE_GL_KHR_parallel_shader_compile 1

// Dark age of openGL. GL_10 and GL_11 are provided by opengl32.dll on windows.
// Linux is a royal mess
//
// #define ENABLE_GL_VERSION_1_0 1
// #define ENABLE_GL_VERSION_1_1 1
#ifdef _WIN32
#define ENABLE_GL_VERSION_1_2 1
#define ENABLE_GL_VERSION_1_3 1
#define ENABLE_GL_VERSION_1_4 1
#endif

#ifndef __APPLE__ // macOS provides up to GL 4.1
#define ENABLE_GL_VERSION_1_5 1
#define ENABLE_GL_VERSION_2_0 1
#define ENABLE_GL_VERSION_2_1 1
#define ENABLE_GL_VERSION_3_0 1
#define ENABLE_GL_VERSION_3_1 1
#define ENABLE_GL_VERSION_3_2 1
#define ENABLE_GL_VERSION_3_3 1
#define ENABLE_GL_VERSION_4_0 1
#define ENABLE_GL_VERSION_4_1 1
#endif
#define ENABLE_GL_VERSION_4_2 1
#define ENABLE_GL_VERSION_4_3 1
#define ENABLE_GL_VERSION_4_4 1
#define ENABLE_GL_VERSION_4_5 1
// #define ENABLE_GL_VERSION_4_6 1

// FIX compilation issue with Mesa 10
#include "glext_extra.h"

// Believe me or not, they forgot to add the interaction with DSA...
#ifndef GL_EXT_direct_state_access
typedef void(APIENTRYP PFNGLTEXTUREPAGECOMMITMENTEXTPROC)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit);
#endif

#ifdef __APPLE__
// Apple uses weakly-linked dylibs to provide GL symbols
// This means that it's possible for a symbol to be null, but not writable by us
extern PFNGLSCISSORINDEXEDPROC   gs_glScissorIndexed;
extern PFNGLVIEWPORTINDEXEDFPROC gs_glViewportIndexedf;
#  define glScissorIndexed   gs_glScissorIndexed
#  define glViewportIndexedf gs_glViewportIndexedf
#endif

// It should be done by ENABLE_GL_VERSION_1_4 but it conflicts with the old gl.h
#ifdef __unix__
extern PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
#endif
extern PFNGLTEXTUREPAGECOMMITMENTEXTPROC glTexturePageCommitmentEXT;

#include "PFN_GLLOADER_HPP.h"

namespace GLExtension
{
	extern bool Has(const std::string& ext);
	extern void Set(const std::string& ext, bool v = true);
} // namespace GLExtension

namespace GLLoader
{
	void check_gl_requirements();

	extern bool vendor_id_amd;
	extern bool vendor_id_nvidia;
	extern bool vendor_id_intel;
	extern bool amd_legacy_buggy_driver;
	extern bool mesa_driver;
	extern bool buggy_sso_dual_src;
	extern bool in_replayer;

	// GL
	extern bool found_geometry_shader;
	extern bool found_GL_ARB_gpu_shader5;
	extern bool found_GL_ARB_shader_image_load_store;
	extern bool found_GL_ARB_clear_texture;

	extern bool found_compatible_GL_ARB_sparse_texture2;
	extern bool found_compatible_sparse_depth;
} // namespace GLLoader
