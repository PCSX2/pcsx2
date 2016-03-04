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

PFNGLACTIVETEXTUREPROC                 gl_ActiveTexture                    = NULL;
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
PFNGLCOMPILESHADERPROC                 glCompileShader                     = NULL;
PFNGLCREATEPROGRAMPROC                 glCreateProgram                     = NULL;
PFNGLCREATESHADERPROC                  glCreateShader                      = NULL;
PFNGLCREATESHADERPROGRAMVPROC          glCreateShaderProgramv              = NULL;
PFNGLDELETEBUFFERSPROC                 glDeleteBuffers                     = NULL;
PFNGLDELETEFRAMEBUFFERSPROC            glDeleteFramebuffers                = NULL;
PFNGLDELETEPROGRAMPROC                 glDeleteProgram                     = NULL;
PFNGLDELETESAMPLERSPROC                glDeleteSamplers                    = NULL;
PFNGLDELETESHADERPROC                  glDeleteShader                      = NULL;
PFNGLDELETEVERTEXARRAYSPROC            glDeleteVertexArrays                = NULL;
PFNGLDETACHSHADERPROC                  glDetachShader                      = NULL;
PFNGLDRAWBUFFERSPROC                   glDrawBuffers                       = NULL;
PFNGLDRAWELEMENTSBASEVERTEXPROC        glDrawElementsBaseVertex            = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC       glEnableVertexAttribArray           = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC       glFramebufferRenderbuffer           = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC          glFramebufferTexture2D              = NULL;
PFNGLGENBUFFERSPROC                    glGenBuffers                        = NULL;
PFNGLGENFRAMEBUFFERSPROC               glGenFramebuffers                   = NULL;
PFNGLGENSAMPLERSPROC                   glGenSamplers                       = NULL;
PFNGLGENVERTEXARRAYSPROC               glGenVertexArrays                   = NULL;
PFNGLGETBUFFERPARAMETERIVPROC          glGetBufferParameteriv              = NULL;
PFNGLGETDEBUGMESSAGELOGARBPROC         glGetDebugMessageLogARB             = NULL;
PFNGLDEBUGMESSAGECALLBACKPROC          glDebugMessageCallback              = NULL;
PFNGLGETPROGRAMINFOLOGPROC             glGetProgramInfoLog                 = NULL;
PFNGLGETPROGRAMIVPROC                  glGetProgramiv                      = NULL;
PFNGLGETSHADERIVPROC                   glGetShaderiv                       = NULL;
PFNGLGETSTRINGIPROC                    glGetStringi                        = NULL;
PFNGLISFRAMEBUFFERPROC                 glIsFramebuffer                     = NULL;
PFNGLLINKPROGRAMPROC                   glLinkProgram                       = NULL;
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
// GL4.0
// GL4.1
PFNGLBINDPROGRAMPIPELINEPROC           glBindProgramPipeline               = NULL;
PFNGLGENPROGRAMPIPELINESPROC           glGenProgramPipelines               = NULL;
PFNGLDELETEPROGRAMPIPELINESPROC        glDeleteProgramPipelines            = NULL;
PFNGLGETPROGRAMPIPELINEIVPROC          glGetProgramPipelineiv              = NULL;
PFNGLVALIDATEPROGRAMPIPELINEPROC       glValidateProgramPipeline           = NULL;
PFNGLGETPROGRAMPIPELINEINFOLOGPROC     glGetProgramPipelineInfoLog         = NULL;
PFNGLGETPROGRAMBINARYPROC              glGetProgramBinary                  = NULL;
// NO GL4.1
PFNGLUSEPROGRAMPROC                    glUseProgram                        = NULL;
PFNGLGETSHADERINFOLOGPROC              glGetShaderInfoLog                  = NULL;
PFNGLPROGRAMUNIFORM1IPROC              glProgramUniform1i                  = NULL;
// GL4.3
PFNGLCOPYIMAGESUBDATAPROC              glCopyImageSubData                  = NULL;
PFNGLINVALIDATETEXIMAGEPROC            glInvalidateTexImage                = NULL;
PFNGLPUSHDEBUGGROUPPROC                glPushDebugGroup                    = NULL;
PFNGLPOPDEBUGGROUPPROC                 glPopDebugGroup                     = NULL;
PFNGLDEBUGMESSAGEINSERTPROC            glDebugMessageInsert                = NULL;
PFNGLDEBUGMESSAGECONTROLPROC           glDebugMessageControl               = NULL;
// GL4.2
PFNGLBINDIMAGETEXTUREPROC              glBindImageTexture                  = NULL;
PFNGLMEMORYBARRIERPROC                 glMemoryBarrier                     = NULL;
PFNGLTEXSTORAGE2DPROC                  glTexStorage2D                      = NULL;
// GL4.4
PFNGLCLEARTEXIMAGEPROC                 glClearTexImage                     = NULL;
PFNGLBUFFERSTORAGEPROC                 glBufferStorage                     = NULL;

// GL4.5
PFNGLCREATETEXTURESPROC                glCreateTextures                    = NULL;
PFNGLTEXTURESTORAGE2DPROC              glTextureStorage2D                  = NULL;
PFNGLTEXTURESUBIMAGE2DPROC             glTextureSubImage2D                 = NULL;
PFNGLCOPYTEXTURESUBIMAGE2DPROC         glCopyTextureSubImage2D             = NULL;
PFNGLBINDTEXTUREUNITPROC               glBindTextureUnit                   = NULL;
PFNGLGETTEXTUREIMAGEPROC               glGetTextureImage                   = NULL;
PFNGLTEXTUREPARAMETERIPROC             glTextureParameteri                 = NULL;

PFNGLCREATEFRAMEBUFFERSPROC            glCreateFramebuffers                = NULL;
PFNGLCLEARNAMEDFRAMEBUFFERFVPROC       glClearNamedFramebufferfv           = NULL;
PFNGLCLEARNAMEDFRAMEBUFFERIVPROC       glClearNamedFramebufferiv           = NULL;
PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC      glClearNamedFramebufferuiv          = NULL;
PFNGLNAMEDFRAMEBUFFERTEXTUREPROC       glNamedFramebufferTexture           = NULL;
PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC   glNamedFramebufferDrawBuffers       = NULL;
PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC    glNamedFramebufferReadBuffer        = NULL;
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

	// Framebuffer entry point
	GLenum fb_target = 0;
	void SetFramebufferTarget(GLenum target) {
		fb_target = target;
	}

	void APIENTRY CreateFramebuffers(GLsizei n, GLuint *framebuffers) {
		glGenFramebuffers(n, framebuffers);
	}

	void APIENTRY ClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value) {
		glBindFramebuffer(fb_target, framebuffer);
		glClearBufferfv(buffer, drawbuffer, value);
	}

	void APIENTRY ClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value) {
		glBindFramebuffer(fb_target, framebuffer);
		glClearBufferiv(buffer, drawbuffer, value);
	}

	void APIENTRY ClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value) {
		glBindFramebuffer(fb_target, framebuffer);
		glClearBufferuiv(buffer, drawbuffer, value);
	}

	void APIENTRY NamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
		glBindFramebuffer(fb_target, framebuffer);
		glFramebufferTexture2D(fb_target, attachment, GL_TEXTURE_2D, texture, level);
	}

	void APIENTRY NamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum *bufs) {
		glBindFramebuffer(fb_target, framebuffer);
		glDrawBuffers(n, bufs);
	}

	void APIENTRY NamedFramebufferReadBuffer(GLuint framebuffer, GLenum src) {
		glBindFramebuffer(fb_target, framebuffer);
		glReadBuffer(src);
		glBindFramebuffer(fb_target, 0);
	}

	GLenum APIENTRY CheckNamedFramebufferStatus(GLuint framebuffer, GLenum target) {
		glBindFramebuffer(fb_target, framebuffer);
		return glCheckFramebufferStatus(fb_target);
	}

	// Buffer entry point
	GLenum buffer_target = 0;
	void SetBufferTarget(GLenum target) {
		buffer_target = target;
	}

	void APIENTRY CreateBuffers(GLsizei n, GLuint *buffers) {
		glGenBuffers(1, buffers);
	}

	void APIENTRY NamedBufferStorage(GLuint buffer, buffer_proc_t size, const void *data, GLbitfield flags) {
		glBindBuffer(buffer_target, buffer);
		glBufferStorage(buffer_target, size, data, flags);
	}

	void APIENTRY NamedBufferData(GLuint buffer, buffer_proc_t size, const void *data, GLenum usage) {
		glBindBuffer(buffer_target, buffer);
		glBufferData(buffer_target, size, data, usage);
	}

	void APIENTRY NamedBufferSubData(GLuint buffer, GLintptr offset, buffer_proc_t size, const void *data) {
		glBindBuffer(buffer_target, buffer);
		glBufferSubData(buffer_target, offset, size, data);
	}

	void *APIENTRY MapNamedBuffer(GLuint buffer, GLenum access) {
		glBindBuffer(buffer_target, buffer);
		return glMapBuffer(buffer_target, access);
	}

	void *APIENTRY MapNamedBufferRange(GLuint buffer, GLintptr offset, buffer_proc_t length, GLbitfield access) {
		glBindBuffer(buffer_target, buffer);
		return glMapBufferRange(buffer_target, offset, length, access);
	}

	GLboolean APIENTRY UnmapNamedBuffer(GLuint buffer) {
		glBindBuffer(buffer_target, buffer);
		return glUnmapBuffer(buffer_target);
	}

	void APIENTRY FlushMappedNamedBufferRange(GLuint buffer, GLintptr offset, buffer_proc_t length) {
		glBindBuffer(buffer_target, buffer);
		glFlushMappedBufferRange(buffer_target, offset, length);
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
		fprintf(stderr, "DSA is not supported. Replacing the GL function pointer to emulate it\n");
		glBindTextureUnit             = BindTextureUnit;
		glCreateTextures              = CreateTexture;
		glTextureStorage2D            = TextureStorage;
		glTextureSubImage2D           = TextureSubImage;
		glCopyTextureSubImage2D       = CopyTextureSubImage;
		glGetTextureImage             = GetTexureImage;
		glTextureParameteri           = TextureParameteri;

		glCreateFramebuffers          = CreateFramebuffers;
		glClearNamedFramebufferfv     = ClearNamedFramebufferfv;
		glClearNamedFramebufferiv     = ClearNamedFramebufferiv;
		glClearNamedFramebufferuiv    = ClearNamedFramebufferuiv;
		glNamedFramebufferDrawBuffers = NamedFramebufferDrawBuffers;
		glNamedFramebufferReadBuffer  = NamedFramebufferReadBuffer;
		glCheckNamedFramebufferStatus = CheckNamedFramebufferStatus;

		glCreateBuffers               = CreateBuffers;
		glNamedBufferStorage          = NamedBufferStorage;
		glNamedBufferData             = NamedBufferData;
		glNamedBufferSubData          = NamedBufferSubData;
		glMapNamedBuffer              = MapNamedBuffer;
		glMapNamedBufferRange         = MapNamedBufferRange;
		glUnmapNamedBuffer            = UnmapNamedBuffer;
		glFlushMappedNamedBufferRange = FlushMappedNamedBufferRange;

		glCreateProgramPipelines      = CreateProgramPipelines;
		glCreateSamplers              = CreateSamplers;
	}
}

namespace GLLoader {

	bool fglrx_buggy_driver    = false;
	bool mesa_amd_buggy_driver = false;
	bool nvidia_buggy_driver   = false;
	bool intel_buggy_driver    = false;
	bool in_replayer           = false;


	// GL4 hardware (due to proprietary driver limitation)
	bool found_GL_ARB_separate_shader_objects = false; // Issue with Catalyst...
	bool found_geometry_shader = true; // we require GL3.3 so geometry must be supported by default
	bool found_GL_EXT_texture_filter_anisotropic = false;
	bool found_GL_ARB_clear_texture = false; // Don't know if GL3 GPU can support it
	bool found_GL_ARB_buffer_storage = false;
	bool found_GL_ARB_copy_image = false; // Not sure actually maybe GL3 GPU can do it
	bool found_GL_ARB_gpu_shader5 = false;
	bool found_GL_ARB_shader_image_load_store = false; // GLES3.1
	// DX10 GPU limited driver
	bool found_GL_ARB_texture_barrier = false; // Well maybe supported by older hardware I don't know
	bool found_GL_ARB_draw_buffers_blend = false;
	bool found_GL_ARB_clip_control = false;
	bool found_GL_ARB_direct_state_access = false;

	// Mandatory
	bool found_GL_ARB_texture_storage = false;
	bool found_GL_ARB_shading_language_420pack = false;

	static bool status_and_override(bool& found, const std::string& name, bool mandatory = false)
	{
		if (mandatory) {
			if (!found) {
				fprintf(stderr, "ERROR: %s is NOT SUPPORTED\n", name.c_str());
			}
			return found;
		}

		if (!found) {
			fprintf(stdout, "INFO: %s is NOT SUPPORTED\n", name.c_str());
		} else {
			fprintf(stdout, "INFO: %s is available\n", name.c_str());
		}

		std::string opt("override_");
		opt += name;

		if (theApp.GetConfig(opt.c_str(), -1) != -1) {
			found = !!theApp.GetConfig(opt.c_str(), -1);
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
		fprintf(stdout, "OpenGL information. GPU: %s. Vendor: %s. Driver: %s\n", glGetString(GL_RENDERER), vendor, &s[v]);

		// Name changed but driver is still bad!
		if (strstr(vendor, "ATI") || strstr(vendor, "Advanced Micro Devices"))
			fglrx_buggy_driver = true;
		if (strstr(vendor, "NVIDIA Corporation"))
			nvidia_buggy_driver = true;
		if (strstr(vendor, "Intel"))
			intel_buggy_driver = true;
		if (strstr(vendor, "X.Org") || strstr(vendor, "nouveau")) // Note: it might actually catch nouveau too, but bugs are likely to be the same anyway
			mesa_amd_buggy_driver = true;
		if (strstr(vendor, "VMware")) // Assume worst case because I don't know the real status
			mesa_amd_buggy_driver = intel_buggy_driver = true;

		if (mesa_amd_buggy_driver) {
			fprintf(stderr, "Buggy driver detected. Geometry shaders will be disabled\n");
			found_geometry_shader = false;
		}
		if (theApp.GetConfig("override_geometry_shader", -1) != -1) {
			found_geometry_shader = !!theApp.GetConfig("override_geometry_shader", -1);
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
				string ext((const char*)glGetStringi(GL_EXTENSIONS, i));
				// Bonus
				if (ext.compare("GL_EXT_texture_filter_anisotropic") == 0) found_GL_EXT_texture_filter_anisotropic = true;
				// GL4.0
				if (ext.compare("GL_ARB_gpu_shader5") == 0) found_GL_ARB_gpu_shader5 = true;
				if (ext.compare("GL_ARB_draw_buffers_blend") == 0) found_GL_ARB_draw_buffers_blend = true;
				// GL4.1
				if (ext.compare("GL_ARB_separate_shader_objects") == 0) {
					if (!fglrx_buggy_driver && !mesa_amd_buggy_driver && !intel_buggy_driver) found_GL_ARB_separate_shader_objects = true;
					else fprintf(stderr, "Buggy driver detected, GL_ARB_separate_shader_objects will be disabled\n"
#ifdef __linux__
							"Note the extension will be fixed on Mesa 11.2 or 11.1.2.\n"
#endif
							"AMD proprietary driver => https://community.amd.com/thread/194895\n"
							"If you want to try it, you can set the variable override_GL_ARB_separate_shader_objects to 1 in the ini file\n");
				}
				// GL4.2
				if (ext.compare("GL_ARB_shading_language_420pack") == 0) found_GL_ARB_shading_language_420pack = true;
				if (ext.compare("GL_ARB_texture_storage") == 0) found_GL_ARB_texture_storage = true;
				if (ext.compare("GL_ARB_shader_image_load_store") == 0) found_GL_ARB_shader_image_load_store = true;
				// GL4.3
				if (ext.compare("GL_ARB_copy_image") == 0) found_GL_ARB_copy_image = true;
				// GL4.4
				if (ext.compare("GL_ARB_buffer_storage") == 0) found_GL_ARB_buffer_storage = true;
				if (ext.compare("GL_ARB_clear_texture") == 0) found_GL_ARB_clear_texture = true;
				// GL4.5
				if (ext.compare("GL_ARB_direct_state_access") == 0) found_GL_ARB_direct_state_access = true;
				if (ext.compare("GL_ARB_clip_control") == 0) found_GL_ARB_clip_control = true;
				if (ext.compare("GL_ARB_texture_barrier") == 0) found_GL_ARB_texture_barrier = true;

				//fprintf(stderr, "DEBUG ext: %s\n", ext.c_str());
			}
		}

		bool status = true;

		// Bonus
		status &= status_and_override(found_GL_EXT_texture_filter_anisotropic, "GL_EXT_texture_filter_anisotropic");
		// GL4.0
		status &= status_and_override(found_GL_ARB_gpu_shader5, "GL_ARB_gpu_shader5");
		status &= status_and_override(found_GL_ARB_draw_buffers_blend, "GL_ARB_draw_buffers_blend");
		// GL4.1
		status &= status_and_override(found_GL_ARB_separate_shader_objects, "GL_ARB_separate_shader_objects");
		// GL4.2
		status &= status_and_override(found_GL_ARB_shader_image_load_store, "GL_ARB_shader_image_load_store");
		status &= status_and_override(found_GL_ARB_shading_language_420pack, "GL_ARB_shading_language_420pack", true);
		status &= status_and_override(found_GL_ARB_texture_storage, "GL_ARB_texture_storage", true);
		// GL4.3
		status &= status_and_override(found_GL_ARB_copy_image, "GL_ARB_copy_image");
		// GL4.4
		status &= status_and_override(found_GL_ARB_buffer_storage,"GL_ARB_buffer_storage");
		status &= status_and_override(found_GL_ARB_clear_texture,"GL_ARB_clear_texture");
		// GL4.5
		status &= status_and_override(found_GL_ARB_clip_control, "GL_ARB_clip_control");
		status &= status_and_override(found_GL_ARB_direct_state_access, "GL_ARB_direct_state_access");
		status &= status_and_override(found_GL_ARB_texture_barrier, "GL_ARB_texture_barrier");

		if (!found_GL_ARB_direct_state_access) {
			Emulate_DSA::Init();
		}
		if (glBindTextureUnit == NULL) {
			fprintf(stderr, "FATAL ERROR !!!! Failed to setup DSA function pointer!!!\n");
			status = false;
		}

		if (!found_GL_ARB_texture_barrier) {
			fprintf(stderr, "Error GL_ARB_texture_barrier is not supported by your driver. You can't emulate correctly the GS blending unit! Sorry!\n");
			theApp.SetConfig("accurate_blending_unit", 0);
			theApp.SetConfig("accurate_date", 0);
		}

#ifdef _WIN32
		if (status) {
			if (intel_buggy_driver) {
				fprintf(stderr, "OpenGL renderer isn't compatible with SandyBridge/IvyBridge GPU due to issues. Sorry.\n"
						"Tip:Try it on Linux");
			}
			if (fglrx_buggy_driver) {
				fprintf(stderr, "OpenGL renderer is slow on AMD GPU due to inefficient driver. Sorry.");
			}
		}
#endif

		fprintf(stdout, "\n");

		return status;
	}
}
