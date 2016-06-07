/*
 *	Copyright (C) 2011-2014 Gregory hainaut
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

#pragma once

#define GL_TEX_LEVEL_0 (0)
#define GL_TEX_LEVEL_1 (1)
#define GL_FB_DEFAULT  (0)
#define GL_BUFFER_0    (0)

#ifndef GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR
#define GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR  0x00000008
#endif

// FIX compilation issue with Mesa 10
// Note it might be possible to do better with the right include
// in the rigth order but I don't have time
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

// Allow compilation with older mesa
#ifndef GL_VERSION_4_3
#define GL_VERSION_4_3 1
typedef void (APIENTRYP PFNGLDEBUGMESSAGECALLBACKPROC) (GLDEBUGPROC callback, const void *userParam);
#endif

#ifndef GL_ARB_copy_image
#define GL_ARB_copy_image 1
#ifdef GL_GLEXT_PROTOTYPES
GLAPI void APIENTRY glCopyImageSubData (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
#endif /* GL_GLEXT_PROTOTYPES */
typedef void (APIENTRYP PFNGLCOPYIMAGESUBDATAPROC) (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
#endif

#ifndef GL_VERSION_4_4
#define GL_VERSION_4_4 1
#define GL_MAX_VERTEX_ATTRIB_STRIDE       0x82E5
#define GL_MAP_PERSISTENT_BIT             0x0040
#define GL_MAP_COHERENT_BIT               0x0080
#define GL_DYNAMIC_STORAGE_BIT            0x0100
#define GL_CLIENT_STORAGE_BIT             0x0200
#define GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT 0x00004000
#define GL_BUFFER_IMMUTABLE_STORAGE       0x821F
#define GL_BUFFER_STORAGE_FLAGS           0x8220
#define GL_CLEAR_TEXTURE                  0x9365
#define GL_LOCATION_COMPONENT             0x934A
#define GL_TRANSFORM_FEEDBACK_BUFFER_INDEX 0x934B
#define GL_TRANSFORM_FEEDBACK_BUFFER_STRIDE 0x934C
#define GL_QUERY_BUFFER                   0x9192
#define GL_QUERY_BUFFER_BARRIER_BIT       0x00008000
#define GL_QUERY_BUFFER_BINDING           0x9193
#define GL_QUERY_RESULT_NO_WAIT           0x9194
#define GL_MIRROR_CLAMP_TO_EDGE           0x8743
typedef void (APIENTRYP PFNGLBUFFERSTORAGEPROC) (GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
typedef void (APIENTRYP PFNGLCLEARTEXIMAGEPROC) (GLuint texture, GLint level, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP PFNGLCLEARTEXSUBIMAGEPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP PFNGLBINDBUFFERSBASEPROC) (GLenum target, GLuint first, GLsizei count, const GLuint *buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERSRANGEPROC) (GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizeiptr *sizes);
typedef void (APIENTRYP PFNGLBINDTEXTURESPROC) (GLuint first, GLsizei count, const GLuint *textures);
typedef void (APIENTRYP PFNGLBINDSAMPLERSPROC) (GLuint first, GLsizei count, const GLuint *samplers);
typedef void (APIENTRYP PFNGLBINDIMAGETEXTURESPROC) (GLuint first, GLsizei count, const GLuint *textures);
typedef void (APIENTRYP PFNGLBINDVERTEXBUFFERSPROC) (GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides);
#endif /* GL_VERSION_4_4 */

// Note: trim it
#ifndef GL_VERSION_4_5
#define GL_VERSION_4_5 1
#define GL_CONTEXT_LOST                   0x0507
#define GL_NEGATIVE_ONE_TO_ONE            0x935E
#define GL_ZERO_TO_ONE                    0x935F
#define GL_CLIP_ORIGIN                    0x935C
#define GL_CLIP_DEPTH_MODE                0x935D
#define GL_QUERY_WAIT_INVERTED            0x8E17
#define GL_QUERY_NO_WAIT_INVERTED         0x8E18
#define GL_QUERY_BY_REGION_WAIT_INVERTED  0x8E19
#define GL_QUERY_BY_REGION_NO_WAIT_INVERTED 0x8E1A
#define GL_MAX_CULL_DISTANCES             0x82F9
#define GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES 0x82FA
#define GL_TEXTURE_TARGET                 0x1006
#define GL_QUERY_TARGET                   0x82EA
#define GL_TEXTURE_BINDING                0x82EB
#define GL_GUILTY_CONTEXT_RESET           0x8253
#define GL_INNOCENT_CONTEXT_RESET         0x8254
#define GL_UNKNOWN_CONTEXT_RESET          0x8255
#define GL_RESET_NOTIFICATION_STRATEGY    0x8256
#define GL_LOSE_CONTEXT_ON_RESET          0x8252
#define GL_NO_RESET_NOTIFICATION          0x8261
#define GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT 0x00000004
#define GL_CONTEXT_RELEASE_BEHAVIOR       0x82FB
#define GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH 0x82FC
typedef void (APIENTRYP PFNGLCLIPCONTROLPROC) (GLenum origin, GLenum depth);
typedef void (APIENTRYP PFNGLCREATEBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLNAMEDBUFFERSTORAGEPROC) (GLuint buffer, GLsizei size, const void *data, GLbitfield flags);
typedef void (APIENTRYP PFNGLNAMEDBUFFERDATAPROC) (GLuint buffer, GLsizei size, const void *data, GLenum usage);
typedef void (APIENTRYP PFNGLNAMEDBUFFERSUBDATAPROC) (GLuint buffer, GLintptr offset, GLsizei size, const void *data);
typedef void (APIENTRYP PFNGLCOPYNAMEDBUFFERSUBDATAPROC) (GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizei size);
typedef void (APIENTRYP PFNGLCLEARNAMEDBUFFERDATAPROC) (GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP PFNGLCLEARNAMEDBUFFERSUBDATAPROC) (GLuint buffer, GLenum internalformat, GLintptr offset, GLsizei size, GLenum format, GLenum type, const void *data);
typedef void *(APIENTRYP PFNGLMAPNAMEDBUFFERPROC) (GLuint buffer, GLenum access);
typedef void *(APIENTRYP PFNGLMAPNAMEDBUFFERRANGEPROC) (GLuint buffer, GLintptr offset, GLsizei length, GLbitfield access);
typedef GLboolean (APIENTRYP PFNGLUNMAPNAMEDBUFFERPROC) (GLuint buffer);
typedef void (APIENTRYP PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC) (GLuint buffer, GLintptr offset, GLsizei length);
typedef void (APIENTRYP PFNGLCREATEFRAMEBUFFERSPROC) (GLsizei n, GLuint *framebuffers);
typedef void (APIENTRYP PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC) (GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (APIENTRYP PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC) (GLuint framebuffer, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLNAMEDFRAMEBUFFERTEXTUREPROC) (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
typedef void (APIENTRYP PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC) (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (APIENTRYP PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC) (GLuint framebuffer, GLenum buf);
typedef void (APIENTRYP PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC) (GLuint framebuffer, GLsizei n, const GLenum *bufs);
typedef void (APIENTRYP PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC) (GLuint framebuffer, GLenum src);
typedef void (APIENTRYP PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC) (GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments);
typedef void (APIENTRYP PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC) (GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLCLEARNAMEDFRAMEBUFFERIVPROC) (GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value);
typedef void (APIENTRYP PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC) (GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value);
typedef void (APIENTRYP PFNGLCLEARNAMEDFRAMEBUFFERFVPROC) (GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value);
typedef void (APIENTRYP PFNGLCLEARNAMEDFRAMEBUFFERFIPROC) (GLuint framebuffer, GLenum buffer, const GLfloat depth, GLint stencil);
typedef void (APIENTRYP PFNGLBLITNAMEDFRAMEBUFFERPROC) (GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef GLenum (APIENTRYP PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC) (GLuint framebuffer, GLenum target);
typedef void (APIENTRYP PFNGLCREATERENDERBUFFERSPROC) (GLsizei n, GLuint *renderbuffers);
typedef void (APIENTRYP PFNGLNAMEDRENDERBUFFERSTORAGEPROC) (GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLCREATETEXTURESPROC) (GLenum target, GLsizei n, GLuint *textures);
typedef void (APIENTRYP PFNGLTEXTUREBUFFERPROC) (GLuint texture, GLenum internalformat, GLuint buffer);
typedef void (APIENTRYP PFNGLTEXTUREBUFFERRANGEPROC) (GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizei size);
typedef void (APIENTRYP PFNGLTEXTURESTORAGE2DPROC) (GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLTEXTURESUBIMAGE2DPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
typedef void (APIENTRYP PFNGLCOPYTEXTURESUBIMAGE2DPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLTEXTUREPARAMETERFPROC) (GLuint texture, GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLTEXTUREPARAMETERFVPROC) (GLuint texture, GLenum pname, const GLfloat *param);
typedef void (APIENTRYP PFNGLTEXTUREPARAMETERIPROC) (GLuint texture, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLTEXTUREPARAMETERIIVPROC) (GLuint texture, GLenum pname, const GLint *params);
typedef void (APIENTRYP PFNGLTEXTUREPARAMETERIUIVPROC) (GLuint texture, GLenum pname, const GLuint *params);
typedef void (APIENTRYP PFNGLTEXTUREPARAMETERIVPROC) (GLuint texture, GLenum pname, const GLint *param);
typedef void (APIENTRYP PFNGLGENERATETEXTUREMIPMAPPROC) (GLuint texture);
typedef void (APIENTRYP PFNGLBINDTEXTUREUNITPROC) (GLuint unit, GLuint texture);
typedef void (APIENTRYP PFNGLCREATEVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef void (APIENTRYP PFNGLDISABLEVERTEXARRAYATTRIBPROC) (GLuint vaobj, GLuint index);
typedef void (APIENTRYP PFNGLENABLEVERTEXARRAYATTRIBPROC) (GLuint vaobj, GLuint index);
typedef void (APIENTRYP PFNGLVERTEXARRAYELEMENTBUFFERPROC) (GLuint vaobj, GLuint buffer);
typedef void (APIENTRYP PFNGLVERTEXARRAYVERTEXBUFFERPROC) (GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
typedef void (APIENTRYP PFNGLVERTEXARRAYVERTEXBUFFERSPROC) (GLuint vaobj, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides);
typedef void (APIENTRYP PFNGLVERTEXARRAYATTRIBBINDINGPROC) (GLuint vaobj, GLuint attribindex, GLuint bindingindex);
typedef void (APIENTRYP PFNGLVERTEXARRAYATTRIBFORMATPROC) (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
typedef void (APIENTRYP PFNGLVERTEXARRAYATTRIBIFORMATPROC) (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
typedef void (APIENTRYP PFNGLVERTEXARRAYATTRIBLFORMATPROC) (GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
typedef void (APIENTRYP PFNGLVERTEXARRAYBINDINGDIVISORPROC) (GLuint vaobj, GLuint bindingindex, GLuint divisor);
typedef void (APIENTRYP PFNGLGETVERTEXARRAYIVPROC) (GLuint vaobj, GLenum pname, GLint *param);
typedef void (APIENTRYP PFNGLGETVERTEXARRAYINDEXEDIVPROC) (GLuint vaobj, GLuint index, GLenum pname, GLint *param);
typedef void (APIENTRYP PFNGLGETVERTEXARRAYINDEXED64IVPROC) (GLuint vaobj, GLuint index, GLenum pname, GLint64 *param);
typedef void (APIENTRYP PFNGLCREATESAMPLERSPROC) (GLsizei n, GLuint *samplers);
typedef void (APIENTRYP PFNGLCREATEPROGRAMPIPELINESPROC) (GLsizei n, GLuint *pipelines);
typedef void (APIENTRYP PFNGLCREATEQUERIESPROC) (GLenum target, GLsizei n, GLuint *ids);
typedef void (APIENTRYP PFNGLMEMORYBARRIERBYREGIONPROC) (GLbitfield barriers);
typedef void (APIENTRYP PFNGLGETTEXTURESUBIMAGEPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
typedef void (APIENTRYP PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei bufSize, void *pixels);
typedef GLenum (APIENTRYP PFNGLGETGRAPHICSRESETSTATUSPROC) (void);
typedef void (APIENTRYP PFNGLGETNCOMPRESSEDTEXIMAGEPROC) (GLenum target, GLint lod, GLsizei bufSize, void *pixels);
typedef void (APIENTRYP PFNGLGETNTEXIMAGEPROC) (GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
typedef void (APIENTRYP PFNGLGETNUNIFORMDVPROC) (GLuint program, GLint location, GLsizei bufSize, GLdouble *params);
typedef void (APIENTRYP PFNGLGETNUNIFORMFVPROC) (GLuint program, GLint location, GLsizei bufSize, GLfloat *params);
typedef void (APIENTRYP PFNGLGETNUNIFORMIVPROC) (GLuint program, GLint location, GLsizei bufSize, GLint *params);
typedef void (APIENTRYP PFNGLGETNUNIFORMUIVPROC) (GLuint program, GLint location, GLsizei bufSize, GLuint *params);
typedef void (APIENTRYP PFNGLREADNPIXELSPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data);
typedef void (APIENTRYP PFNGLTEXTUREBARRIERPROC) (void);
typedef void (APIENTRYP PFNGLGETTEXTUREIMAGEPROC) (GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
#endif /* GL_VERSION_4_5 */

// Note: glActiveTexture & glBlendColor aren't included in the win GL ABI.
// (maybe gl.h is outdated, or my setup is wrong)
// Anyway, let's just keep the mangled function pointer for those 2 functions.
extern   PFNGLBLENDCOLORPROC                    gl_BlendColor;

extern   PFNGLATTACHSHADERPROC                  glAttachShader;
extern   PFNGLBINDBUFFERPROC                    glBindBuffer;
extern   PFNGLBINDBUFFERBASEPROC                glBindBufferBase;
extern   PFNGLBINDBUFFERRANGEPROC               glBindBufferRange;
extern   PFNGLBINDFRAMEBUFFERPROC               glBindFramebuffer;
extern   PFNGLBINDSAMPLERPROC                   glBindSampler;
extern   PFNGLBINDVERTEXARRAYPROC               glBindVertexArray;
extern   PFNGLBLENDEQUATIONSEPARATEIARBPROC     glBlendEquationSeparateiARB;
extern   PFNGLBLENDFUNCSEPARATEIARBPROC         glBlendFuncSeparateiARB;
extern   PFNGLBLITFRAMEBUFFERPROC               glBlitFramebuffer;
extern   PFNGLBUFFERDATAPROC                    glBufferData;
extern   PFNGLCHECKFRAMEBUFFERSTATUSPROC        glCheckFramebufferStatus;
extern   PFNGLCLEARBUFFERFVPROC                 glClearBufferfv;
extern   PFNGLCLEARBUFFERIVPROC                 glClearBufferiv;
extern   PFNGLCLEARBUFFERUIVPROC                glClearBufferuiv;
extern   PFNGLCOLORMASKIPROC                    glColorMaski;
extern   PFNGLCREATESHADERPROGRAMVPROC          glCreateShaderProgramv;
extern   PFNGLDELETEBUFFERSPROC                 glDeleteBuffers;
extern   PFNGLDELETEFRAMEBUFFERSPROC            glDeleteFramebuffers;
extern   PFNGLDELETEPROGRAMPROC                 glDeleteProgram;
extern   PFNGLDELETESAMPLERSPROC                glDeleteSamplers;
extern   PFNGLDELETEVERTEXARRAYSPROC            glDeleteVertexArrays;
extern   PFNGLDETACHSHADERPROC                  glDetachShader;
extern   PFNGLDRAWBUFFERSPROC                   glDrawBuffers;
extern   PFNGLDRAWELEMENTSBASEVERTEXPROC        glDrawElementsBaseVertex;
extern   PFNGLENABLEVERTEXATTRIBARRAYPROC       glEnableVertexAttribArray;
extern   PFNGLFRAMEBUFFERRENDERBUFFERPROC       glFramebufferRenderbuffer;
extern   PFNGLFRAMEBUFFERTEXTURE2DPROC          glFramebufferTexture2D;
extern   PFNGLGENBUFFERSPROC                    glGenBuffers;
extern   PFNGLGENFRAMEBUFFERSPROC               glGenFramebuffers;
extern   PFNGLGENVERTEXARRAYSPROC               glGenVertexArrays;
extern   PFNGLGETBUFFERPARAMETERIVPROC          glGetBufferParameteriv;
extern   PFNGLGETDEBUGMESSAGELOGARBPROC         glGetDebugMessageLogARB;
extern   PFNGLDEBUGMESSAGECALLBACKPROC          glDebugMessageCallback;
extern   PFNGLGETPROGRAMINFOLOGPROC             glGetProgramInfoLog;
extern   PFNGLGETPROGRAMIVPROC                  glGetProgramiv;
extern   PFNGLGETSHADERIVPROC                   glGetShaderiv;
extern   PFNGLGETSTRINGIPROC                    glGetStringi;
extern   PFNGLISFRAMEBUFFERPROC                 glIsFramebuffer;
extern   PFNGLMAPBUFFERPROC                     glMapBuffer;
extern   PFNGLMAPBUFFERRANGEPROC                glMapBufferRange;
extern   PFNGLPROGRAMPARAMETERIPROC             glProgramParameteri;
extern   PFNGLSAMPLERPARAMETERFPROC             glSamplerParameterf;
extern   PFNGLSAMPLERPARAMETERIPROC             glSamplerParameteri;
extern   PFNGLSHADERSOURCEPROC                  glShaderSource;
extern   PFNGLUNIFORM1IPROC                     glUniform1i;
extern   PFNGLUNMAPBUFFERPROC                   glUnmapBuffer;
extern   PFNGLUSEPROGRAMSTAGESPROC              glUseProgramStages;
extern   PFNGLVERTEXATTRIBIPOINTERPROC          glVertexAttribIPointer;
extern   PFNGLVERTEXATTRIBPOINTERPROC           glVertexAttribPointer;
extern   PFNGLBUFFERSUBDATAPROC                 glBufferSubData;
extern   PFNGLFENCESYNCPROC                     glFenceSync;
extern   PFNGLDELETESYNCPROC                    glDeleteSync;
extern   PFNGLCLIENTWAITSYNCPROC                glClientWaitSync;
extern   PFNGLFLUSHMAPPEDBUFFERRANGEPROC        glFlushMappedBufferRange;
extern   PFNGLBLENDEQUATIONSEPARATEPROC         glBlendEquationSeparate;
extern   PFNGLBLENDFUNCSEPARATEPROC             glBlendFuncSeparate;
// Shader compilation (Broken driver)
extern   PFNGLCOMPILESHADERPROC                 glCompileShader;
extern   PFNGLCREATEPROGRAMPROC                 glCreateProgram;
extern   PFNGLCREATESHADERPROC                  glCreateShader;
extern   PFNGLDELETESHADERPROC                  glDeleteShader;
extern   PFNGLLINKPROGRAMPROC                   glLinkProgram;
extern   PFNGLUSEPROGRAMPROC                    glUseProgram;
extern   PFNGLGETSHADERINFOLOGPROC              glGetShaderInfoLog;
extern   PFNGLPROGRAMUNIFORM1IPROC              glProgramUniform1i;
// Query object
extern   PFNGLBEGINQUERYPROC                    glBeginQuery;
extern   PFNGLENDQUERYPROC                      glEndQuery;
extern   PFNGLGETQUERYIVPROC                    glGetQueryiv;
extern   PFNGLGETQUERYOBJECTIVPROC              glGetQueryObjectiv;
extern   PFNGLGETQUERYOBJECTUIVPROC             glGetQueryObjectuiv;
extern   PFNGLQUERYCOUNTERPROC                  glQueryCounter;
extern   PFNGLGETQUERYOBJECTI64VPROC            glGetQueryObjecti64v;
extern   PFNGLGETQUERYOBJECTUI64VPROC           glGetQueryObjectui64v;
extern   PFNGLGETINTEGER64VPROC                 glGetInteger64v;
// GL4.0
// GL4.1
extern   PFNGLBINDPROGRAMPIPELINEPROC           glBindProgramPipeline;
extern   PFNGLDELETEPROGRAMPIPELINESPROC        glDeleteProgramPipelines;
extern   PFNGLGETPROGRAMPIPELINEIVPROC          glGetProgramPipelineiv;
extern   PFNGLVALIDATEPROGRAMPIPELINEPROC       glValidateProgramPipeline;
extern   PFNGLGETPROGRAMPIPELINEINFOLOGPROC     glGetProgramPipelineInfoLog;
extern   PFNGLGETPROGRAMBINARYPROC              glGetProgramBinary;
extern   PFNGLVIEWPORTINDEXEDFPROC              glViewportIndexedf;
extern   PFNGLVIEWPORTINDEXEDFVPROC             glViewportIndexedfv;
extern   PFNGLSCISSORINDEXEDPROC                glScissorIndexed;
extern   PFNGLSCISSORINDEXEDVPROC               glScissorIndexedv;
// GL4.2
extern   PFNGLBINDIMAGETEXTUREPROC              glBindImageTexture;
extern   PFNGLMEMORYBARRIERPROC                 glMemoryBarrier;
extern   PFNGLPOPDEBUGGROUPPROC                 glPopDebugGroup;
// GL4.3
extern   PFNGLCOPYIMAGESUBDATAPROC              glCopyImageSubData;
extern   PFNGLINVALIDATETEXIMAGEPROC            glInvalidateTexImage;
extern   PFNGLPUSHDEBUGGROUPPROC                glPushDebugGroup;
extern   PFNGLDEBUGMESSAGEINSERTPROC            glDebugMessageInsert;
extern   PFNGLDEBUGMESSAGECONTROLPROC           glDebugMessageControl;
extern   PFNGLOBJECTLABELPROC                   glObjectLabel;
extern   PFNGLOBJECTPTRLABELPROC                glObjectPtrLabel;
// GL4.4
extern   PFNGLCLEARTEXIMAGEPROC                 glClearTexImage;
extern   PFNGLCLEARTEXSUBIMAGEPROC              glClearTexSubImage;
extern   PFNGLBUFFERSTORAGEPROC                 glBufferStorage;

// GL4.5
extern PFNGLCREATETEXTURESPROC                  glCreateTextures;
extern PFNGLTEXTURESTORAGE2DPROC                glTextureStorage2D;
extern PFNGLTEXTURESUBIMAGE2DPROC               glTextureSubImage2D;
extern PFNGLCOPYTEXTURESUBIMAGE2DPROC           glCopyTextureSubImage2D;
extern PFNGLBINDTEXTUREUNITPROC                 glBindTextureUnit;
extern PFNGLGETTEXTUREIMAGEPROC                 glGetTextureImage;
extern PFNGLTEXTUREPARAMETERIPROC               glTextureParameteri;

extern PFNGLCREATEFRAMEBUFFERSPROC              glCreateFramebuffers;
extern PFNGLCLEARNAMEDFRAMEBUFFERFVPROC         glClearNamedFramebufferfv;
extern PFNGLCLEARNAMEDFRAMEBUFFERIVPROC         glClearNamedFramebufferiv;
extern PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC        glClearNamedFramebufferuiv;
extern PFNGLNAMEDFRAMEBUFFERTEXTUREPROC         glNamedFramebufferTexture;
extern PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC     glNamedFramebufferDrawBuffers;
extern PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC      glNamedFramebufferReadBuffer;
extern PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC      glNamedFramebufferParameteri;
extern PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC     glCheckNamedFramebufferStatus;

extern PFNGLCREATEBUFFERSPROC                   glCreateBuffers;
extern PFNGLNAMEDBUFFERSTORAGEPROC              glNamedBufferStorage;
extern PFNGLNAMEDBUFFERDATAPROC                 glNamedBufferData;
extern PFNGLNAMEDBUFFERSUBDATAPROC              glNamedBufferSubData;
extern PFNGLMAPNAMEDBUFFERPROC                  glMapNamedBuffer;
extern PFNGLMAPNAMEDBUFFERRANGEPROC             glMapNamedBufferRange;
extern PFNGLUNMAPNAMEDBUFFERPROC                glUnmapNamedBuffer;
extern PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC     glFlushMappedNamedBufferRange;

extern PFNGLCREATESAMPLERSPROC                  glCreateSamplers;
extern PFNGLCREATEPROGRAMPIPELINESPROC          glCreateProgramPipelines;

extern PFNGLCLIPCONTROLPROC                     glClipControl;
extern PFNGLTEXTUREBARRIERPROC                  glTextureBarrier;
extern PFNGLGETTEXTURESUBIMAGEPROC              glGetTextureSubImage;

namespace GLLoader {
	bool check_gl_version(int major, int minor);
	void init_gl_function();
	bool check_gl_supported_extension();

	extern bool fglrx_buggy_driver;
	extern bool legacy_fglrx_buggy_driver;
	extern bool mesa_amd_buggy_driver;
	extern bool nvidia_buggy_driver;
	extern bool intel_buggy_driver;
	extern bool buggy_sso_dual_src;
	extern bool in_replayer;

	// GL
	extern bool found_geometry_shader;
	extern bool found_GL_ARB_gpu_shader5;
	extern bool found_GL_ARB_shader_image_load_store;
	extern bool found_GL_ARB_clear_texture;
	extern bool found_GL_ARB_direct_state_access;
	extern bool found_GL_EXT_texture_filter_anisotropic;
}
