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
#include "GLLoader.h"
#include "GS/GS.h"
#include <unordered_set>
#include "Host.h"

namespace GLExtension
{

	static std::unordered_set<std::string> s_extensions;

	bool Has(const std::string& ext)
	{
		return !!s_extensions.count(ext);
	}

	void Set(const std::string& ext, bool v)
	{
		if (v)
			s_extensions.insert(ext);
		else
			s_extensions.erase(ext);
	}
} // namespace GLExtension

namespace ReplaceGL
{
	void APIENTRY ScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height)
	{
		glScissor(left, bottom, width, height);
	}

	void APIENTRY ViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h)
	{
		glViewport(GLint(x), GLint(y), GLsizei(w), GLsizei(h));
	}

	void APIENTRY TextureBarrier()
	{
	}

} // namespace ReplaceGL

#ifdef _WIN32
namespace Emulate_DSA
{
	// Texture entry point
	void APIENTRY BindTextureUnit(GLuint unit, GLuint texture)
	{
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, texture);
	}

	void APIENTRY CreateTexture(GLenum target, GLsizei n, GLuint* textures)
	{
		glGenTextures(1, textures);
	}

	void APIENTRY TextureStorage(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
	{
		BindTextureUnit(7, texture);
		glTexStorage2D(GL_TEXTURE_2D, levels, internalformat, width, height);
	}

	void APIENTRY TextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels)
	{
		BindTextureUnit(7, texture);
		glTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, height, format, type, pixels);
	}

	void APIENTRY CompressedTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data)
	{
		BindTextureUnit(7, texture);
		glCompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, height, format, imageSize, data);
	}

	void APIENTRY GetTexureImage(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels)
	{
		BindTextureUnit(7, texture);
		glGetTexImage(GL_TEXTURE_2D, level, format, type, pixels);
	}

	void APIENTRY TextureParameteri(GLuint texture, GLenum pname, GLint param)
	{
		BindTextureUnit(7, texture);
		glTexParameteri(GL_TEXTURE_2D, pname, param);
	}

	void APIENTRY GenerateTextureMipmap(GLuint texture)
	{
		BindTextureUnit(7, texture);
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	// Misc entry point
	// (only purpose is to have a consistent API otherwise it is useless)
	void APIENTRY CreateProgramPipelines(GLsizei n, GLuint* pipelines)
	{
		glGenProgramPipelines(n, pipelines);
	}

	void APIENTRY CreateSamplers(GLsizei n, GLuint* samplers)
	{
		glGenSamplers(n, samplers);
	}

	// Replace function pointer to emulate DSA behavior
	void Init()
	{
		Console.Warning("DSA is not supported. Expect slower performance");
		glBindTextureUnit = BindTextureUnit;
		glCreateTextures = CreateTexture;
		glTextureStorage2D = TextureStorage;
		glTextureSubImage2D = TextureSubImage;
		glCompressedTextureSubImage2D = CompressedTextureSubImage;
		glGetTextureImage = GetTexureImage;
		glTextureParameteri = TextureParameteri;

		glCreateProgramPipelines = CreateProgramPipelines;
		glCreateSamplers = CreateSamplers;
	}
} // namespace Emulate_DSA
#endif

namespace GLLoader
{
	bool vendor_id_amd = false;
	bool vendor_id_nvidia = false;
	bool vendor_id_intel = false;
	bool mesa_driver = false;
	bool in_replayer = false;

	bool has_dual_source_blend = false;
	bool found_framebuffer_fetch = false;
	bool found_geometry_shader = true; // we require GL3.3 so geometry must be supported by default
	// DX11 GPU
	bool found_GL_ARB_gpu_shader5 = false;             // Require IvyBridge
	bool found_GL_ARB_texture_barrier = false;

	static bool mandatory(const std::string& ext)
	{
		if (!GLExtension::Has(ext))
		{
			Host::ReportFormattedErrorAsync("GS", "ERROR: %s is NOT SUPPORTED\n", ext.c_str());
			return false;
		}

		return true;
	}

	static bool optional(const std::string& name)
	{
		bool found = GLExtension::Has(name);

		if (!found)
		{
			DevCon.Warning("INFO: %s is NOT SUPPORTED", name.c_str());
		}
		else
		{
			DevCon.WriteLn("INFO: %s is available", name.c_str());
		}

		std::string opt("override_");
		opt += name;

		if (theApp.GetConfigI(opt.c_str()) != -1)
		{
			found = theApp.GetConfigB(opt.c_str());
			fprintf(stderr, "Override %s detection (%s)\n", name.c_str(), found ? "Enabled" : "Disabled");
			GLExtension::Set(name, found);
		}

		return found;
	}

	bool check_gl_version(int major, int minor)
	{
		const char* vendor = (const char*)glGetString(GL_VENDOR);
		if (strstr(vendor, "Advanced Micro Devices") || strstr(vendor, "ATI Technologies Inc.") || strstr(vendor, "ATI"))
			vendor_id_amd = true;
		else if (strstr(vendor, "NVIDIA Corporation"))
			vendor_id_nvidia = true;

#ifdef _WIN32
		else if (strstr(vendor, "Intel"))
			vendor_id_intel = true;
#else
		// On linux assumes the free driver if it isn't nvidia or amd pro driver
		mesa_driver = !vendor_id_nvidia && !vendor_id_amd;
#endif

		if (GSConfig.OverrideGeometryShaders != -1)
		{
			found_geometry_shader = GSConfig.OverrideGeometryShaders != 0 &&
									(GLAD_GL_VERSION_3_2 || GL_ARB_geometry_shader4 || GSConfig.OverrideGeometryShaders == 1);
			GLExtension::Set("GL_ARB_geometry_shader4", found_geometry_shader);
			fprintf(stderr, "Overriding geometry shaders detection\n");
		}

		GLint major_gl = 0;
		GLint minor_gl = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &major_gl);
		glGetIntegerv(GL_MINOR_VERSION, &minor_gl);
		if ((major_gl < major) || (major_gl == major && minor_gl < minor))
		{
			Host::ReportFormattedErrorAsync("GS", "OpenGL %d.%d is not supported. Only OpenGL %d.%d\n was found", major, minor, major_gl, minor_gl);
			return false;
		}

		return true;
	}

	bool check_gl_supported_extension()
	{
		int max_ext = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &max_ext);
		for (GLint i = 0; i < max_ext; i++)
		{
			std::string ext{(const char*)glGetStringi(GL_EXTENSIONS, i)};
			GLExtension::Set(ext);
			//fprintf(stderr, "DEBUG ext: %s\n", ext.c_str());
		}

		// Mandatory for both renderer
		bool ok = true;
		{
			// GL4.1
			ok = ok && mandatory("GL_ARB_separate_shader_objects");
			// GL4.2
			ok = ok && mandatory("GL_ARB_shading_language_420pack");
			ok = ok && mandatory("GL_ARB_texture_storage");
			// GL4.3
			ok = ok && mandatory("GL_KHR_debug");
			// GL4.4
			ok = ok && mandatory("GL_ARB_buffer_storage");
		}

		// Only for HW renderer
		if (GSConfig.UseHardwareRenderer())
		{
			ok = ok && mandatory("GL_ARB_copy_image");
			ok = ok && mandatory("GL_ARB_clip_control");
		}
		if (!ok)
			return false;

		// Extra
		{
			// GL4.0
			found_GL_ARB_gpu_shader5 = optional("GL_ARB_gpu_shader5");
			// GL4.5
			optional("GL_ARB_direct_state_access");
			// Mandatory for the advance HW renderer effect. Unfortunately Mesa LLVMPIPE/SWR renderers doesn't support this extension.
			// Rendering might be corrupted but it could be good enough for test/virtual machine.
			found_GL_ARB_texture_barrier = optional("GL_ARB_texture_barrier");

			has_dual_source_blend = GLAD_GL_VERSION_3_2 || GLAD_GL_ARB_blend_func_extended;
			found_framebuffer_fetch = GLAD_GL_EXT_shader_framebuffer_fetch || GLAD_GL_ARM_shader_framebuffer_fetch;
			if (found_framebuffer_fetch && GSConfig.DisableFramebufferFetch)
			{
				Console.Warning("Framebuffer fetch was found but is disabled. This will reduce performance.");
				found_framebuffer_fetch = false;
			}
		}

		if (!GLExtension::Has("GL_ARB_viewport_array"))
		{
			glScissorIndexed = ReplaceGL::ScissorIndexed;
			glViewportIndexedf = ReplaceGL::ViewportIndexedf;
			Console.Warning("GL_ARB_viewport_array is not supported! Function pointer will be replaced");
		}

		if (!GLExtension::Has("GL_ARB_texture_barrier"))
		{
			glTextureBarrier = ReplaceGL::TextureBarrier;
			Console.Warning("GL_ARB_texture_barrier is not supported! Blending emulation will not be supported");
		}

#ifdef _WIN32
		// Thank you Intel for not providing support of basic features on your IGPUs.
		if (!GLExtension::Has("GL_ARB_direct_state_access"))
		{
			Emulate_DSA::Init();
		}
#endif

		return true;
	}

	bool check_gl_requirements()
	{
		if (!check_gl_version(3, 3))
			return false;

		if (!check_gl_supported_extension())
			return false;

		return true;
	}
} // namespace GLLoader
