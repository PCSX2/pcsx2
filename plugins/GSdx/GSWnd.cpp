/*
 *	Copyright (C) 2011-2014 Gregory hainaut
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
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
#include "GSWnd.h"

void GSWndGL::PopulateGlFunction()
{
	*(void**)&(gl_ActiveTexture) = GetProcAddress("glActiveTexture");
	*(void**)&(gl_BlendColor) = GetProcAddress("glBlendColor");

	*(void**)&(glBlendEquationSeparate) = GetProcAddress("glBlendEquationSeparate");
	*(void**)&(glBlendFuncSeparate) = GetProcAddress("glBlendFuncSeparate");
	*(void**)&(glAttachShader) = GetProcAddress("glAttachShader");
	*(void**)&(glBindBuffer) = GetProcAddress("glBindBuffer");
	*(void**)&(glBindBufferBase) = GetProcAddress("glBindBufferBase");
	*(void**)&(glBindBufferRange) = GetProcAddress("glBindBufferRange");
	*(void**)&(glBindFramebuffer) = GetProcAddress("glBindFramebuffer");
	*(void**)&(glBindSampler) = GetProcAddress("glBindSampler");
	*(void**)&(glBindVertexArray) = GetProcAddress("glBindVertexArray");
	*(void**)&(glBlitFramebuffer) = GetProcAddress("glBlitFramebuffer");
	*(void**)&(glBufferData) = GetProcAddress("glBufferData");
	*(void**)&(glCheckFramebufferStatus) = GetProcAddress("glCheckFramebufferStatus");
	*(void**)&(glClearBufferfv) = GetProcAddress("glClearBufferfv");
	*(void**)&(glClearBufferiv) = GetProcAddress("glClearBufferiv");
	*(void**)&(glClearBufferuiv) = GetProcAddress("glClearBufferuiv");
	*(void**)&(glColorMaski) = GetProcAddress("glColorMaski");
	*(void**)&(glDeleteBuffers) = GetProcAddress("glDeleteBuffers");
	*(void**)&(glDeleteFramebuffers) = GetProcAddress("glDeleteFramebuffers");
	*(void**)&(glDeleteSamplers) = GetProcAddress("glDeleteSamplers");
	*(void**)&(glDeleteVertexArrays) = GetProcAddress("glDeleteVertexArrays");
	*(void**)&(glDetachShader) = GetProcAddress("glDetachShader");
	*(void**)&(glDrawBuffers) = GetProcAddress("glDrawBuffers");
	*(void**)&(glDrawElementsBaseVertex) = GetProcAddress("glDrawElementsBaseVertex");
	*(void**)&(glEnableVertexAttribArray) = GetProcAddress("glEnableVertexAttribArray");
	*(void**)&(glFramebufferRenderbuffer) = GetProcAddress("glFramebufferRenderbuffer");
	*(void**)&(glFramebufferTexture2D) = GetProcAddress("glFramebufferTexture2D");
	*(void**)&(glGenBuffers) = GetProcAddress("glGenBuffers");
	*(void**)&(glGenFramebuffers) = GetProcAddress("glGenFramebuffers");
	*(void**)&(glGenSamplers) = GetProcAddress("glGenSamplers");
	*(void**)&(glGenVertexArrays) = GetProcAddress("glGenVertexArrays");
	*(void**)&(glGetBufferParameteriv) = GetProcAddress("glGetBufferParameteriv");
	*(void**)&(glGetDebugMessageLogARB) = GetProcAddress("glGetDebugMessageLogARB");
	*(void**)&(glDebugMessageCallback) = GetProcAddress("glDebugMessageCallback", true);
	*(void**)&(glGetProgramInfoLog) = GetProcAddress("glGetProgramInfoLog");
	*(void**)&(glGetProgramiv) = GetProcAddress("glGetProgramiv");
	*(void**)&(glGetShaderiv) = GetProcAddress("glGetShaderiv");
	*(void**)&(glGetStringi) = GetProcAddress("glGetStringi");
	*(void**)&(glIsFramebuffer) = GetProcAddress("glIsFramebuffer");
	*(void**)&(glMapBuffer) = GetProcAddress("glMapBuffer");
	*(void**)&(glMapBufferRange) = GetProcAddress("glMapBufferRange");
	*(void**)&(glProgramParameteri) = GetProcAddress("glProgramParameteri");
	*(void**)&(glSamplerParameterf) = GetProcAddress("glSamplerParameterf");
	*(void**)&(glSamplerParameteri) = GetProcAddress("glSamplerParameteri");
	*(void**)&(glShaderSource) = GetProcAddress("glShaderSource");
	*(void**)&(glUniform1i) = GetProcAddress("glUniform1i");
	*(void**)&(glUnmapBuffer) = GetProcAddress("glUnmapBuffer");
	*(void**)&(glVertexAttribIPointer) = GetProcAddress("glVertexAttribIPointer");
	*(void**)&(glVertexAttribPointer) = GetProcAddress("glVertexAttribPointer");
	*(void**)&(glBufferSubData) = GetProcAddress("glBufferSubData");
	*(void**)&(glFenceSync) = GetProcAddress("glFenceSync");
	*(void**)&(glDeleteSync) = GetProcAddress("glDeleteSync");
	*(void**)&(glClientWaitSync) = GetProcAddress("glClientWaitSync");
	*(void**)&(glFlushMappedBufferRange) = GetProcAddress("glFlushMappedBufferRange");
	// GL4.0
	*(void**)&(glBlendEquationSeparateiARB) = GetProcAddress("glBlendEquationSeparateiARB", true);
	*(void**)&(glBlendFuncSeparateiARB) = GetProcAddress("glBlendFuncSeparateiARB", true);
	// GL4.1
	*(void**)&(glCreateShaderProgramv) = GetProcAddress("glCreateShaderProgramv", true);
	*(void**)&(glBindProgramPipeline) = GetProcAddress("glBindProgramPipeline", true);
	*(void**)&(glDeleteProgramPipelines) = GetProcAddress("glDeleteProgramPipelines", true);
	*(void**)&(glGenProgramPipelines) = GetProcAddress("glGenProgramPipelines", true);
	*(void**)&(glGetProgramPipelineiv) = GetProcAddress("glGetProgramPipelineiv", true);
	*(void**)&(glGetProgramPipelineInfoLog) = GetProcAddress("glGetProgramPipelineInfoLog", true);
	*(void**)&(glValidateProgramPipeline) = GetProcAddress("glValidateProgramPipeline", true);
	*(void**)&(glUseProgramStages) = GetProcAddress("glUseProgramStages", true);
	*(void**)&(glProgramUniform1i) = GetProcAddress("glProgramUniform1i", true); // but no GL4.2
	*(void**)&(glGetProgramBinary) = GetProcAddress("glGetProgramBinary", true);
	// NO GL4.1
	*(void**)&(glDeleteProgram) = GetProcAddress("glDeleteProgram");
	*(void**)&(glDeleteShader) = GetProcAddress("glDeleteShader");
	*(void**)&(glCompileShader) = GetProcAddress("glCompileShader");
	*(void**)&(glCreateProgram) = GetProcAddress("glCreateProgram");
	*(void**)&(glCreateShader) = GetProcAddress("glCreateShader");
	*(void**)&(glUseProgram) = GetProcAddress("glUseProgram");
	*(void**)&(glGetShaderInfoLog) = GetProcAddress("glGetShaderInfoLog");
	*(void**)&(glLinkProgram) = GetProcAddress("glLinkProgram");
	// GL4.2
	*(void**)&(glBindImageTexture) = GetProcAddress("glBindImageTexture", true);
	*(void**)&(glMemoryBarrier) = GetProcAddress("glMemoryBarrier", true);
	*(void**)&(glTexStorage2D) = GetProcAddress("glTexStorage2D");
	// GL4.3
	*(void**)&(glCopyImageSubData) = GetProcAddress("glCopyImageSubData", true);
	*(void**)&(glInvalidateTexImage) = GetProcAddress("glInvalidateTexImage", true);
	*(void**)&(glPushDebugGroup) = GetProcAddress("glPushDebugGroup", true);
	*(void**)&(glPopDebugGroup) = GetProcAddress("glPopDebugGroup", true);
	*(void**)&(glDebugMessageInsert) = GetProcAddress("glDebugMessageInsert", true);
	*(void**)&(glDebugMessageControl) = GetProcAddress("glDebugMessageControl", true);
	// GL4.4
	*(void**)&(glClearTexImage) = GetProcAddress("glClearTexImage", true);
	*(void**)&(glBufferStorage) = GetProcAddress("glBufferStorage", true);

	// GL4.5
	*(void**)&(glCreateTextures) = GetProcAddress("glCreateTextures", true);
	*(void**)&(glTextureStorage2D) = GetProcAddress("glTextureStorage2D", true);
	*(void**)&(glTextureSubImage2D) = GetProcAddress("glTextureSubImage2D", true);
	*(void**)&(glCopyTextureSubImage2D) = GetProcAddress("glCopyTextureSubImage2D", true);
	*(void**)&(glBindTextureUnit) = GetProcAddress("glBindTextureUnit", true);
	*(void**)&(glGetTextureImage) = GetProcAddress("glGetTextureImage", true);
	*(void**)&(glTextureParameteri) = GetProcAddress("glTextureParameteri", true);

	*(void**)&(glCreateFramebuffers) = GetProcAddress("glCreateFramebuffers", true);
	*(void**)&(glClearNamedFramebufferfv) = GetProcAddress("glClearNamedFramebufferfv", true);
	*(void**)&(glClearNamedFramebufferuiv) = GetProcAddress("glClearNamedFramebufferuiv", true);
	*(void**)&(glClearNamedFramebufferiv) = GetProcAddress("glClearNamedFramebufferiv", true);
	*(void**)&(glNamedFramebufferTexture) = GetProcAddress("glNamedFramebufferTexture", true);
	*(void**)&(glNamedFramebufferDrawBuffers) = GetProcAddress("glNamedFramebufferDrawBuffers", true);
	*(void**)&(glNamedFramebufferReadBuffer) = GetProcAddress("glNamedFramebufferReadBuffer", true);
	*(void**)&(glCheckNamedFramebufferStatus) = GetProcAddress("glCheckNamedFramebufferStatus", true);

	*(void**)&(glCreateBuffers) = GetProcAddress("glCreateBuffers", true);
	*(void**)&(glNamedBufferStorage) = GetProcAddress("glNamedBufferStorage", true);
	*(void**)&(glNamedBufferData) = GetProcAddress("glNamedBufferData", true);
	*(void**)&(glNamedBufferSubData) = GetProcAddress("glNamedBufferSubData", true);
	*(void**)&(glMapNamedBuffer) = GetProcAddress("glMapNamedBuffer", true);
	*(void**)&(glMapNamedBufferRange) = GetProcAddress("glMapNamedBufferRange", true);
	*(void**)&(glUnmapNamedBuffer) = GetProcAddress("glUnmapNamedBuffer", true);
	*(void**)&(glFlushMappedNamedBufferRange) = GetProcAddress("glFlushMappedNamedBufferRange", true);

	*(void**)&(glCreateSamplers) = GetProcAddress("glCreateSamplers", true);
	*(void**)&(glCreateProgramPipelines) = GetProcAddress("glCreateProgramPipelines", true);

	*(void**)&(glClipControl) = GetProcAddress("glClipControl", true);
	*(void**)&(glTextureBarrier) = GetProcAddress("glTextureBarrier", true);

	if (glCreateFramebuffers == NULL) {
		Emulate_DSA::Init();
	}
}
