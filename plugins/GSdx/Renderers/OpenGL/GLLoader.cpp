/* *	Copyright (C) 2011-2014 Gregory hainaut
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
#include "GLLoader.h"
#include "GSdx.h"
#include "GS.h"

PFNGLBLENDCOLORPROC                    gl_BlendColor                       = NULL;

PFNGLATTACHSHADERPROC                  glAttachShader                      = NULL;
PFNGLBINDBUFFERPROC                    glBindBuffer                        = NULL;
PFNGLBINDBUFFERBASEPROC                glBindBufferBase                    = NULL;
PFNGLBINDBUFFERRANGEPROC               glBindBufferRange                   = NULL;
PFNGLBINDFRAMEBUFFERPROC               glBindFramebuffer                   = NULL;
PFNGLBINDSAMPLERPROC                   glBindSampler                       = NULL;
PFNGLBINDVERTEXARRAYPROC               glBindVertexArray                   = NULL;
PFNGLBLENDEQUATIONSEPARATEIARBPROC     glBlendEquationSeparateiARB         = NULL;
PFNGLBLENDFUNCSEPARATEIARBPROC         glBlendFuncSeparateiARB             = NULL;
PFNGLBLITFRAMEBUFFERPROC               glBlitFramebuffer                   = NULL;
PFNGLBUFFERDATAPROC                    glBufferData                        = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC        glCheckFramebufferStatus            = NULL;
PFNGLCLEARBUFFERFVPROC                 glClearBufferfv                     = NULL;
PFNGLCLEARBUFFERIVPROC                 glClearBufferiv                     = NULL;
PFNGLCLEARBUFFERUIVPROC                glClearBufferuiv                    = NULL;
PFNGLCOLORMASKIPROC                    glColorMaski                        = NULL;
PFNGLCREATESHADERPROGRAMVPROC          glCreateShaderProgramv              = NULL;
PFNGLDELETEBUFFERSPROC                 glDeleteBuffers                     = NULL;
PFNGLDELETEFRAMEBUFFERSPROC            glDeleteFramebuffers                = NULL;
PFNGLDELETEPROGRAMPROC                 glDeleteProgram                     = NULL;
PFNGLDELETESAMPLERSPROC                glDeleteSamplers                    = NULL;
PFNGLDELETEVERTEXARRAYSPROC            glDeleteVertexArrays                = NULL;
PFNGLDETACHSHADERPROC                  glDetachShader                      = NULL;
PFNGLDRAWBUFFERSPROC                   glDrawBuffers                       = NULL;
PFNGLDRAWELEMENTSBASEVERTEXPROC        glDrawElementsBaseVertex            = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC       glEnableVertexAttribArray           = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC       glFramebufferRenderbuffer           = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC          glFramebufferTexture2D              = NULL;
PFNGLGENBUFFERSPROC                    glGenBuffers                        = NULL;
PFNGLGENFRAMEBUFFERSPROC               glGenFramebuffers                   = NULL;
PFNGLGENVERTEXARRAYSPROC               glGenVertexArrays                   = NULL;
PFNGLGETBUFFERPARAMETERIVPROC          glGetBufferParameteriv              = NULL;
PFNGLGETDEBUGMESSAGELOGARBPROC         glGetDebugMessageLogARB             = NULL;
PFNGLDEBUGMESSAGECALLBACKPROC          glDebugMessageCallback              = NULL;
PFNGLGETPROGRAMINFOLOGPROC             glGetProgramInfoLog                 = NULL;
PFNGLGETPROGRAMIVPROC                  glGetProgramiv                      = NULL;
PFNGLGETSHADERIVPROC                   glGetShaderiv                       = NULL;
PFNGLGETSTRINGIPROC                    glGetStringi                        = NULL;
PFNGLISFRAMEBUFFERPROC                 glIsFramebuffer                     = NULL;
PFNGLMAPBUFFERPROC                     glMapBuffer                         = NULL;
PFNGLMAPBUFFERRANGEPROC                glMapBufferRange                    = NULL;
PFNGLPROGRAMPARAMETERIPROC             glProgramParameteri                 = NULL;
PFNGLSAMPLERPARAMETERFPROC             glSamplerParameterf                 = NULL;
PFNGLSAMPLERPARAMETERIPROC             glSamplerParameteri                 = NULL;
PFNGLSHADERSOURCEPROC                  glShaderSource                      = NULL;
PFNGLUNIFORM1IPROC                     glUniform1i                         = NULL;
PFNGLUNMAPBUFFERPROC                   glUnmapBuffer                       = NULL;
PFNGLUSEPROGRAMSTAGESPROC              glUseProgramStages                  = NULL;
PFNGLVERTEXATTRIBIPOINTERPROC          glVertexAttribIPointer              = NULL;
PFNGLVERTEXATTRIBPOINTERPROC           glVertexAttribPointer               = NULL;
PFNGLBUFFERSUBDATAPROC                 glBufferSubData                     = NULL;
PFNGLFENCESYNCPROC                     glFenceSync                         = NULL;
PFNGLDELETESYNCPROC                    glDeleteSync                        = NULL;
PFNGLCLIENTWAITSYNCPROC                glClientWaitSync                    = NULL;
PFNGLFLUSHMAPPEDBUFFERRANGEPROC        glFlushMappedBufferRange            = NULL;
PFNGLBLENDEQUATIONSEPARATEPROC         glBlendEquationSeparate             = NULL;
PFNGLBLENDFUNCSEPARATEPROC             glBlendFuncSeparate                 = NULL;
// Shader compilation (Broken driver)
PFNGLCOMPILESHADERPROC                 glCompileShader                     = NULL;
PFNGLCREATEPROGRAMPROC                 glCreateProgram                     = NULL;
PFNGLCREATESHADERPROC                  glCreateShader                      = NULL;
PFNGLDELETESHADERPROC                  glDeleteShader                      = NULL;
PFNGLLINKPROGRAMPROC                   glLinkProgram                       = NULL;
PFNGLUSEPROGRAMPROC                    glUseProgram                        = NULL;
PFNGLGETSHADERINFOLOGPROC              glGetShaderInfoLog                  = NULL;
PFNGLPROGRAMUNIFORM1IPROC              glProgramUniform1i                  = NULL;
// Query object
PFNGLBEGINQUERYPROC                    glBeginQuery                        = NULL;
PFNGLENDQUERYPROC                      glEndQuery                          = NULL;
PFNGLGETQUERYIVPROC                    glGetQueryiv                        = NULL;
PFNGLGETQUERYOBJECTIVPROC              glGetQueryObjectiv                  = NULL;
PFNGLGETQUERYOBJECTUIVPROC             glGetQueryObjectuiv                 = NULL;
PFNGLQUERYCOUNTERPROC                  glQueryCounter                      = NULL;
PFNGLGETQUERYOBJECTI64VPROC            glGetQueryObjecti64v                = NULL;
PFNGLGETQUERYOBJECTUI64VPROC           glGetQueryObjectui64v               = NULL;
PFNGLGETINTEGER64VPROC                 glGetInteger64v                     = NULL;
PFNGLCREATEQUERIESPROC                 glCreateQueries                     = NULL;
PFNGLDELETEQUERIESPROC                 glDeleteQueries                     = NULL;
// GL4.0
// GL4.1
PFNGLBINDPROGRAMPIPELINEPROC           glBindProgramPipeline               = NULL;
PFNGLDELETEPROGRAMPIPELINESPROC        glDeleteProgramPipelines            = NULL;
PFNGLGETPROGRAMPIPELINEIVPROC          glGetProgramPipelineiv              = NULL;
PFNGLVALIDATEPROGRAMPIPELINEPROC       glValidateProgramPipeline           = NULL;
PFNGLGETPROGRAMPIPELINEINFOLOGPROC     glGetProgramPipelineInfoLog         = NULL;
PFNGLGETPROGRAMBINARYPROC              glGetProgramBinary                  = NULL;
PFNGLVIEWPORTINDEXEDFPROC              glViewportIndexedf                  = NULL;
PFNGLVIEWPORTINDEXEDFVPROC             glViewportIndexedfv                 = NULL;
PFNGLSCISSORINDEXEDPROC                glScissorIndexed                    = NULL;
PFNGLSCISSORINDEXEDVPROC               glScissorIndexedv                   = NULL;
// GL4.3
PFNGLCOPYIMAGESUBDATAPROC              glCopyImageSubData                  = NULL;
PFNGLINVALIDATETEXIMAGEPROC            glInvalidateTexImage                = NULL;
PFNGLPUSHDEBUGGROUPPROC                glPushDebugGroup                    = NULL;
PFNGLPOPDEBUGGROUPPROC                 glPopDebugGroup                     = NULL;
PFNGLDEBUGMESSAGEINSERTPROC            glDebugMessageInsert                = NULL;
PFNGLDEBUGMESSAGECONTROLPROC           glDebugMessageControl               = NULL;
PFNGLOBJECTLABELPROC                   glObjectLabel                       = NULL;
PFNGLOBJECTPTRLABELPROC                glObjectPtrLabel                    = NULL;
// GL4.2
PFNGLBINDIMAGETEXTUREPROC              glBindImageTexture                  = NULL;
PFNGLMEMORYBARRIERPROC                 glMemoryBarrier                     = NULL;
// GL4.4
PFNGLCLEARTEXIMAGEPROC                 glClearTexImage                     = NULL;
PFNGLCLEARTEXSUBIMAGEPROC              glClearTexSubImage                  = NULL;
PFNGLBUFFERSTORAGEPROC                 glBufferStorage                     = NULL;

// GL4.5
PFNGLCREATETEXTURESPROC                glCreateTextures                    = NULL;
PFNGLTEXTURESTORAGE2DPROC              glTextureStorage2D                  = NULL;
PFNGLTEXTURESUBIMAGE2DPROC             glTextureSubImage2D                 = NULL;
PFNGLCOPYTEXTURESUBIMAGE2DPROC         glCopyTextureSubImage2D             = NULL;
PFNGLBINDTEXTUREUNITPROC               glBindTextureUnit                   = NULL;
PFNGLGETTEXTUREIMAGEPROC               glGetTextureImage                   = NULL;
PFNGLTEXTUREPARAMETERIPROC             glTextureParameteri                 = NULL;
PFNGLGENERATETEXTUREMIPMAPPROC         glGenerateTextureMipmap             = NULL;

PFNGLCREATEFRAMEBUFFERSPROC            glCreateFramebuffers                = NULL;
PFNGLCLEARNAMEDFRAMEBUFFERFVPROC       glClearNamedFramebufferfv           = NULL;
PFNGLCLEARNAMEDFRAMEBUFFERIVPROC       glClearNamedFramebufferiv           = NULL;
PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC      glClearNamedFramebufferuiv          = NULL;
PFNGLNAMEDFRAMEBUFFERTEXTUREPROC       glNamedFramebufferTexture           = NULL;
PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC   glNamedFramebufferDrawBuffers       = NULL;
PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC    glNamedFramebufferReadBuffer        = NULL;
PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC    glNamedFramebufferParameteri        = NULL;
PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC   glCheckNamedFramebufferStatus       = NULL;

PFNGLCREATEBUFFERSPROC                 glCreateBuffers                     = NULL;
PFNGLNAMEDBUFFERSTORAGEPROC            glNamedBufferStorage                = NULL;
PFNGLNAMEDBUFFERDATAPROC               glNamedBufferData                   = NULL;
PFNGLNAMEDBUFFERSUBDATAPROC            glNamedBufferSubData                = NULL;
PFNGLMAPNAMEDBUFFERPROC                glMapNamedBuffer                    = NULL;
PFNGLMAPNAMEDBUFFERRANGEPROC           glMapNamedBufferRange               = NULL;
PFNGLUNMAPNAMEDBUFFERPROC              glUnmapNamedBuffer                  = NULL;
PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC   glFlushMappedNamedBufferRange       = NULL;

PFNGLCREATESAMPLERSPROC                glCreateSamplers                    = NULL;
PFNGLCREATEPROGRAMPIPELINESPROC        glCreateProgramPipelines            = NULL;

PFNGLCLIPCONTROLPROC                   glClipControl                       = NULL;
PFNGLTEXTUREBARRIERPROC                glTextureBarrier                    = NULL;
PFNGLGETTEXTURESUBIMAGEPROC            glGetTextureSubImage                = NULL;

#ifdef _WIN32
PFNGLACTIVETEXTUREPROC                 gl_ActiveTexture                    = NULL;
PFNGLTEXSTORAGE2DPROC                  glTexStorage2D                      = NULL;
PFNGLGENPROGRAMPIPELINESPROC           glGenProgramPipelines               = NULL;
PFNGLGENSAMPLERSPROC                   glGenSamplers                       = NULL;
PFNGLGENERATEMIPMAPPROC                glGenerateMipmap                    = NULL;
#endif

namespace ReplaceGL {
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

}

#ifdef _WIN32
namespace Emulate_DSA {
	// Texture entry point
	void APIENTRY BindTextureUnit(GLuint unit, GLuint texture) {
		gl_ActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, texture);
	}

	void APIENTRY CreateTexture(GLenum target, GLsizei n, GLuint *textures) {
		glGenTextures(1, textures);
	}

	void APIENTRY TextureStorage(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
		BindTextureUnit(7, texture);
		glTexStorage2D(GL_TEXTURE_2D, levels, internalformat, width, height);
	}

	void APIENTRY TextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) {
		BindTextureUnit(7, texture);
		glTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, height, format, type, pixels);
	}

	void APIENTRY CopyTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
		BindTextureUnit(7, texture);
		glCopyTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, x, y, width, height);
	}

	void APIENTRY GetTexureImage(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels) {
		BindTextureUnit(7, texture);
		glGetTexImage(GL_TEXTURE_2D, level, format, type, pixels);
	}

	void APIENTRY TextureParameteri (GLuint texture, GLenum pname, GLint param) {
		BindTextureUnit(7, texture);
		glTexParameteri(GL_TEXTURE_2D, pname, param);
	}

	void APIENTRY GenerateTextureMipmap(GLuint texture) {
		BindTextureUnit(7, texture);
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	// Misc entry point
	// (only purpose is to have a consistent API otherwise it is useless)
	void APIENTRY CreateProgramPipelines(GLsizei n, GLuint *pipelines) {
		glGenProgramPipelines(n, pipelines);
	}

	void APIENTRY CreateSamplers(GLsizei n, GLuint *samplers) {
		glGenSamplers(n, samplers);
	}

	// Replace function pointer to emulate DSA behavior
	void Init() {
		fprintf(stderr, "DSA is not supported. Expect slower performance\n");
		glBindTextureUnit             = BindTextureUnit;
		glCreateTextures              = CreateTexture;
		glTextureStorage2D            = TextureStorage;
		glTextureSubImage2D           = TextureSubImage;
		glCopyTextureSubImage2D       = CopyTextureSubImage;
		glGetTextureImage             = GetTexureImage;
		glTextureParameteri           = TextureParameteri;

		glCreateProgramPipelines      = CreateProgramPipelines;
		glCreateSamplers              = CreateSamplers;
	}
}
#endif

namespace GLLoader {

	bool s_first_load = true;

	bool amd_legacy_buggy_driver = false;
	bool vendor_id_amd      = false;
	bool vendor_id_nvidia   = false;
	bool vendor_id_intel    = false;
	bool mesa_driver        = false;
	bool in_replayer        = false;
	bool buggy_sso_dual_src = false;


	bool found_geometry_shader = true; // we require GL3.3 so geometry must be supported by default
	bool found_GL_EXT_texture_filter_anisotropic = false;
	bool found_GL_ARB_clear_texture = false;
	bool found_GL_ARB_get_texture_sub_image = false; // Not yet used
	// DX11 GPU
	bool found_GL_ARB_gpu_shader5 = false; // Require IvyBridge
	bool found_GL_ARB_shader_image_load_store = false; // Intel IB. Nvidia/AMD miss Mesa implementation.
	bool found_GL_ARB_viewport_array = false; // Intel IB. AMD/NVIDIA DX10
	// Bonus to monitor the VRAM
	bool found_GL_NVX_gpu_memory_info = false;

	// Mandatory
	bool found_GL_ARB_buffer_storage = false;
	bool found_GL_ARB_clip_control = false;
	bool found_GL_ARB_copy_image = false;
	bool found_GL_ARB_direct_state_access = false;
	bool found_GL_ARB_separate_shader_objects = false;
	bool found_GL_ARB_shading_language_420pack = false;
	bool found_GL_ARB_texture_barrier = false;
	bool found_GL_ARB_texture_storage = false;
	bool found_GL_KHR_debug = false;

	static bool status_and_override(bool& found, const std::string& name, bool mandatory = false)
	{
		if (mandatory) {
			if (!found) {
				fprintf(stderr, "ERROR: %s is NOT SUPPORTED\n", name.c_str());
			}
			return found;
		}

		if (s_first_load) {
			if (!found) {
				fprintf(stdout, "INFO: %s is NOT SUPPORTED\n", name.c_str());
			} else {
				fprintf(stdout, "INFO: %s is available\n", name.c_str());
			}
		}

		std::string opt("override_");
		opt += name;

		if (theApp.GetConfigI(opt.c_str()) != -1) {
			found = theApp.GetConfigB(opt.c_str());
			fprintf(stderr, "Override %s detection (%s)\n", name.c_str(), found ? "Enabled" : "Disabled");
		}

		return true;
	}

    bool check_gl_version(int major, int minor) {

		const GLubyte* s = glGetString(GL_VERSION);
		if (s == NULL) {
			fprintf(stderr, "Error: GLLoader failed to get GL version\n");
			return false;
		}
		GLuint v = 1;
		while (s[v] != '\0' && s[v-1] != ' ') v++;

		const char* vendor = (const char*)glGetString(GL_VENDOR);
		if (s_first_load)
			fprintf(stdout, "OpenGL information. GPU: %s. Vendor: %s. Driver: %s\n", glGetString(GL_RENDERER), vendor, &s[v]);

		// Name changed but driver is still bad!
		if (strstr(vendor, "Advanced Micro Devices") || strstr(vendor, "ATI Technologies Inc.") || strstr(vendor, "ATI"))
			vendor_id_amd = true;
		/*if (vendor_id_amd && (
				strstr((const char*)&s[v], " 10.") || // Blacklist all 2010 AMD drivers.
				strstr((const char*)&s[v], " 11.") || // Blacklist all 2011 AMD drivers.
				strstr((const char*)&s[v], " 12.") || // Blacklist all 2012 AMD drivers.
				strstr((const char*)&s[v], " 13.") || // Blacklist all 2013 AMD drivers.
				strstr((const char*)&s[v], " 14.") || // Blacklist all 2014 AMD drivers.
				strstr((const char*)&s[v], " 15.") || // Blacklist all 2015 AMD drivers.
				strstr((const char*)&s[v], " 16.") || // Blacklist all 2016 AMD drivers.
				strstr((const char*)&s[v], " 17.") // Blacklist all 2017 AMD drivers for now.
				))
			amd_legacy_buggy_driver = true;
		*/
		if (strstr(vendor, "NVIDIA Corporation"))
			vendor_id_nvidia = true;

#ifdef _WIN32
		if (strstr(vendor, "Intel"))
			vendor_id_intel = true;
#else
		// On linux assumes the free driver if it isn't nvidia or amd pro driver
		mesa_driver = !vendor_id_nvidia && !vendor_id_amd;
#endif

		buggy_sso_dual_src = vendor_id_intel || vendor_id_amd /*|| amd_legacy_buggy_driver*/;

		if (theApp.GetConfigI("override_geometry_shader") != -1) {
			found_geometry_shader = theApp.GetConfigB("override_geometry_shader");
			fprintf(stderr, "Overriding geometry shaders detection\n");
		}

		GLint major_gl = 0;
		GLint minor_gl = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &major_gl);
		glGetIntegerv(GL_MINOR_VERSION, &minor_gl);
		if ( (major_gl < major) || ( major_gl == major && minor_gl < minor ) ) {
			fprintf(stderr, "OpenGL %d.%d is not supported. Only OpenGL %d.%d\n was found", major, minor, major_gl, minor_gl);
			return false;
		}

        return true;
    }

	bool check_gl_supported_extension() {
		int max_ext = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &max_ext);

		if (glGetStringi && max_ext) {
			for (GLint i = 0; i < max_ext; i++) {
				std::string ext{(const char*)glGetStringi(GL_EXTENSIONS, i)};
				// Bonus
				if (ext.compare("GL_EXT_texture_filter_anisotropic") == 0) found_GL_EXT_texture_filter_anisotropic = true;
				if (ext.compare("GL_NVX_gpu_memory_info") == 0) found_GL_NVX_gpu_memory_info = true;
				// GL4.0
				if (ext.compare("GL_ARB_gpu_shader5") == 0) found_GL_ARB_gpu_shader5 = true;
				// GL4.1
				if (ext.compare("GL_ARB_viewport_array") == 0) found_GL_ARB_viewport_array = true;
				if (ext.compare("GL_ARB_separate_shader_objects") == 0) found_GL_ARB_separate_shader_objects = true;
				// GL4.2
				if (ext.compare("GL_ARB_shading_language_420pack") == 0) found_GL_ARB_shading_language_420pack = true;
				if (ext.compare("GL_ARB_texture_storage") == 0) found_GL_ARB_texture_storage = true;
				if (ext.compare("GL_ARB_shader_image_load_store") == 0) found_GL_ARB_shader_image_load_store = true;
				// GL4.3
				if (ext.compare("GL_ARB_copy_image") == 0) found_GL_ARB_copy_image = true;
				if (ext.compare("GL_KHR_debug") == 0) found_GL_KHR_debug = true;
				// GL4.4
				if (ext.compare("GL_ARB_buffer_storage") == 0) found_GL_ARB_buffer_storage = true;
				if (ext.compare("GL_ARB_clear_texture") == 0) found_GL_ARB_clear_texture = true;
				// GL4.5
				if (ext.compare("GL_ARB_direct_state_access") == 0) found_GL_ARB_direct_state_access = true;
				if (ext.compare("GL_ARB_clip_control") == 0) found_GL_ARB_clip_control = true;
				if (ext.compare("GL_ARB_texture_barrier") == 0) found_GL_ARB_texture_barrier = true;
				if (ext.compare("GL_ARB_get_texture_sub_image") == 0) found_GL_ARB_get_texture_sub_image = true;

				//fprintf(stderr, "DEBUG ext: %s\n", ext.c_str());
			}
		}

		bool status = true;
		bool required_for_hw = (theApp.GetCurrentRendererType() == GSRendererType::OGL_HW);

		// Bonus
		status &= status_and_override(found_GL_EXT_texture_filter_anisotropic, "GL_EXT_texture_filter_anisotropic");
		// GL4.0
		status &= status_and_override(found_GL_ARB_gpu_shader5, "GL_ARB_gpu_shader5");
		// GL4.1
		status &= status_and_override(found_GL_ARB_viewport_array, "GL_ARB_viewport_array");
		status &= status_and_override(found_GL_ARB_separate_shader_objects, "GL_ARB_separate_shader_objects", true);
		// GL4.2
		status &= status_and_override(found_GL_ARB_shader_image_load_store, "GL_ARB_shader_image_load_store");
		status &= status_and_override(found_GL_ARB_shading_language_420pack, "GL_ARB_shading_language_420pack", true);
		status &= status_and_override(found_GL_ARB_texture_storage, "GL_ARB_texture_storage", true);
		// GL4.3
		status &= status_and_override(found_GL_ARB_copy_image, "GL_ARB_copy_image", required_for_hw);
		status &= status_and_override(found_GL_KHR_debug, "GL_KHR_debug", true);
		// GL4.4
		status &= status_and_override(found_GL_ARB_buffer_storage,"GL_ARB_buffer_storage", true);
		status &= status_and_override(found_GL_ARB_clear_texture,"GL_ARB_clear_texture");
		// GL4.5
		status &= status_and_override(found_GL_ARB_clip_control, "GL_ARB_clip_control", required_for_hw);
		status &= status_and_override(found_GL_ARB_direct_state_access, "GL_ARB_direct_state_access");
		// Mandatory for the advance HW renderer effect. Unfortunately Mesa LLVMPIPE/SWR renderers doesn't support this extension.
		// Rendering might be corrupted but it could be good enough for test/virtual machine.
		status &= status_and_override(found_GL_ARB_texture_barrier, "GL_ARB_texture_barrier");
		status &= status_and_override(found_GL_ARB_get_texture_sub_image, "GL_ARB_get_texture_sub_image");

		if (s_first_load) {
			if (vendor_id_amd) {
				fprintf(stderr, "The OpenGL hardware renderer is slow on AMD GPUs due to an inefficient driver.\n"
					"Check out the link below for further information.\n"
					"https://github.com/PCSX2/pcsx2/wiki/OpenGL-and-AMD-GPUs---All-you-need-to-know\n");
			}

			if (vendor_id_intel) {
				fprintf(stderr, "The OpenGL renderer is inefficient on Intel GPUs due to an inefficient driver.\n"
					"Check out the link below for further information.\n"
					"https://github.com/PCSX2/pcsx2/wiki/OpenGL-and-Intel-GPUs-All-you-need-to-know\n");
			}
		}

		if (!found_GL_ARB_viewport_array) {
			glScissorIndexed   = ReplaceGL::ScissorIndexed;
			glViewportIndexedf = ReplaceGL::ViewportIndexedf;
			if (s_first_load)
				fprintf(stderr, "GL_ARB_viewport_array is not supported! Function pointer will be replaced\n");
		}

		if (!found_GL_ARB_texture_barrier) {
			glTextureBarrier = ReplaceGL::TextureBarrier;
			if (s_first_load)
				fprintf(stderr, "GL_ARB_texture_barrier is not supported! Blending emulation will not be supported\n");
		}

#ifdef _WIN32
		// Thank you Intel for not providing support of basic features on your IGPUs.
		if (!found_GL_ARB_direct_state_access) {
			Emulate_DSA::Init();
		}
#endif

		if (s_first_load)
			fprintf(stdout, "\n");

		return status;
	}

	void check_gl_requirements()
	{
		if (!GLLoader::check_gl_version(3, 3))
			throw GSDXRecoverableError();

		if (!GLLoader::check_gl_supported_extension())
			throw GSDXRecoverableError();

		s_first_load = false;
	}
}
