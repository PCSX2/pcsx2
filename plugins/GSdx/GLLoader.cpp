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
// GL4.2
PFNGLBINDIMAGETEXTUREPROC              glBindImageTexture                  = NULL;
PFNGLMEMORYBARRIERPROC                 glMemoryBarrier                     = NULL;
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

namespace GLLoader {

	bool fglrx_buggy_driver    = false;
	bool mesa_amd_buggy_driver = false;
	bool nvidia_buggy_driver   = false;
	bool intel_buggy_driver    = false;
	bool in_replayer           = false;


	bool found_geometry_shader = true; // we require GL3.3 so geometry must be supported by default
	bool found_GL_EXT_texture_filter_anisotropic = false;
	bool found_GL_ARB_clear_texture = false; // Miss AMD Mesa (otherwise seems SW)
	// DX10 GPU limited driver (SW)
	bool found_GL_ARB_copy_image = false;
	// DX11 GPU
	bool found_GL_ARB_draw_buffers_blend = false; // Not supported on AMD R600 (80 nm class chip, HD2900). Nvidia requires FERMI. Intel SB
	bool found_GL_ARB_gpu_shader5 = false; // Require IvyBridge
	bool found_GL_ARB_shader_image_load_store = false; // Intel IB. Nvidia/AMD miss Mesa implementation.
	bool found_GL_ARB_viewport_array = false; // Intel IB. AMD/NVIDIA DX10

	// Mandatory
	bool found_GL_ARB_buffer_storage = false;
	bool found_GL_ARB_clip_control = false;
	bool found_GL_ARB_direct_state_access = false;
	bool found_GL_ARB_separate_shader_objects = false;
	bool found_GL_ARB_shading_language_420pack = false;
	bool found_GL_ARB_texture_barrier = false;
	bool found_GL_ARB_texture_storage = false;

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
				if (ext.compare("GL_ARB_viewport_array") == 0) found_GL_ARB_viewport_array = true;
				if (ext.compare("GL_ARB_separate_shader_objects") == 0) found_GL_ARB_separate_shader_objects = true;
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
		status &= status_and_override(found_GL_ARB_viewport_array, "GL_ARB_viewport_array");
		status &= status_and_override(found_GL_ARB_separate_shader_objects, "GL_ARB_separate_shader_objects", true);
		// GL4.2
		status &= status_and_override(found_GL_ARB_shader_image_load_store, "GL_ARB_shader_image_load_store");
		status &= status_and_override(found_GL_ARB_shading_language_420pack, "GL_ARB_shading_language_420pack", true);
		status &= status_and_override(found_GL_ARB_texture_storage, "GL_ARB_texture_storage", true);
		// GL4.3
		status &= status_and_override(found_GL_ARB_copy_image, "GL_ARB_copy_image");
		// GL4.4
		status &= status_and_override(found_GL_ARB_buffer_storage,"GL_ARB_buffer_storage", true);
		status &= status_and_override(found_GL_ARB_clear_texture,"GL_ARB_clear_texture");
		// GL4.5
		status &= status_and_override(found_GL_ARB_clip_control, "GL_ARB_clip_control", true);
		status &= status_and_override(found_GL_ARB_direct_state_access, "GL_ARB_direct_state_access", true);
		status &= status_and_override(found_GL_ARB_texture_barrier, "GL_ARB_texture_barrier", true);

#ifdef _WIN32
		if (status) {
			if (fglrx_buggy_driver) {
				fprintf(stderr, "OpenGL renderer is slow on AMD GPU due to inefficient driver. Sorry.");
			}
		}
#endif

		fprintf(stdout, "\n");

		return status;
	}
}
