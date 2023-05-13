/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "GS/Renderers/OpenGL/GLLoader.h"
#include "GS/GS.h"
#include "Host.h"

#include "glad.h"

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
	void APIENTRY CreateSamplers(GLsizei n, GLuint* samplers)
	{
		glGenSamplers(n, samplers);
	}

	// Replace function pointer to emulate DSA behavior
	void Init()
	{
		glBindTextureUnit = BindTextureUnit;
		glCreateTextures = CreateTexture;
		glTextureStorage2D = TextureStorage;
		glTextureSubImage2D = TextureSubImage;
		glCompressedTextureSubImage2D = CompressedTextureSubImage;
		glGetTextureImage = GetTexureImage;
		glTextureParameteri = TextureParameteri;
		glGenerateTextureMipmap = GenerateTextureMipmap;
		glCreateSamplers = CreateSamplers;
	}
} // namespace Emulate_DSA

namespace GLLoader
{
	bool vendor_id_amd = false;
	bool vendor_id_nvidia = false;
	bool vendor_id_intel = false;
	bool buggy_pbo = false;
	bool disable_download_pbo = false;

	static bool check_gl_version()
	{
		const char* vendor = (const char*)glGetString(GL_VENDOR);
		if (strstr(vendor, "Advanced Micro Devices") || strstr(vendor, "ATI Technologies Inc.") || strstr(vendor, "ATI"))
			vendor_id_amd = true;
		else if (strstr(vendor, "NVIDIA Corporation"))
			vendor_id_nvidia = true;
		else if (strstr(vendor, "Intel"))
			vendor_id_intel = true;

		GLint major_gl = 0;
		GLint minor_gl = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &major_gl);
		glGetIntegerv(GL_MINOR_VERSION, &minor_gl);
		if (!GLAD_GL_VERSION_3_3 && !GLAD_GL_ES_VERSION_3_1)
		{
			Host::ReportFormattedErrorAsync("GS", "OpenGL is not supported. Only OpenGL %d.%d\n was found", major_gl, minor_gl);
			return false;
		}

		return true;
	}

	static bool check_gl_supported_extension()
	{
		if (!GLAD_GL_ARB_shading_language_420pack)
		{
			Host::ReportFormattedErrorAsync("GS",
				"GL_ARB_shading_language_420pack is not supported, this is required for the OpenGL renderer.");
			return false;
		}

		if (!GLAD_GL_ARB_viewport_array)
		{
			glScissorIndexed = ReplaceGL::ScissorIndexed;
			glViewportIndexedf = ReplaceGL::ViewportIndexedf;
			Console.Warning("GL_ARB_viewport_array is not supported! Function pointer will be replaced.");
		}

		if (!GLAD_GL_ARB_texture_barrier)
		{
			glTextureBarrier = ReplaceGL::TextureBarrier;
			Host::AddOSDMessage("GL_ARB_texture_barrier is not supported, blending will not be accurate.",
				Host::OSD_ERROR_DURATION);
		}

		if (!GLAD_GL_ARB_direct_state_access)
		{
			Console.Warning("GL_ARB_direct_state_access is not supported, this will reduce performance.");
			Emulate_DSA::Init();
		}

		// Don't use PBOs when we don't have ARB_buffer_storage, orphaning buffers probably ends up worse than just
		// using the normal texture update routines and letting the driver take care of it.
		buggy_pbo = !GLAD_GL_VERSION_4_4 && !GLAD_GL_ARB_buffer_storage && !GLAD_GL_EXT_buffer_storage;
		if (buggy_pbo)
			Console.Warning("Not using PBOs for texture uploads because buffer_storage is unavailable.");

		// Give the user the option to disable PBO usage for downloads.
		// Most drivers seem to be faster with PBO.
		disable_download_pbo = Host::GetBoolSettingValue("EmuCore/GS", "DisableGLDownloadPBO", false);
		if (disable_download_pbo)
			Console.Warning("Not using PBOs for texture downloads, this may reduce performance.");

		return true;
	}

	bool check_gl_requirements()
	{
		if (!check_gl_version())
			return false;

		if (!check_gl_supported_extension())
			return false;

		return true;
	}
} // namespace GLLoader
