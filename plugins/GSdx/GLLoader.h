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

#ifndef GL_ARB_bindless_texture
#define GL_ARB_bindless_texture 1
typedef uint64_t GLuint64EXT;
#define GL_UNSIGNED_INT64_ARB             0x140F
typedef GLuint64 (APIENTRYP PFNGLGETTEXTUREHANDLEARBPROC) (GLuint texture);
typedef GLuint64 (APIENTRYP PFNGLGETTEXTURESAMPLERHANDLEARBPROC) (GLuint texture, GLuint sampler);
typedef void (APIENTRYP PFNGLMAKETEXTUREHANDLERESIDENTARBPROC) (GLuint64 handle);
typedef void (APIENTRYP PFNGLMAKETEXTUREHANDLENONRESIDENTARBPROC) (GLuint64 handle);
typedef GLuint64 (APIENTRYP PFNGLGETIMAGEHANDLEARBPROC) (GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format);
typedef void (APIENTRYP PFNGLMAKEIMAGEHANDLERESIDENTARBPROC) (GLuint64 handle, GLenum access);
typedef void (APIENTRYP PFNGLMAKEIMAGEHANDLENONRESIDENTARBPROC) (GLuint64 handle);
typedef void (APIENTRYP PFNGLUNIFORMHANDLEUI64ARBPROC) (GLint location, GLuint64 value);
typedef void (APIENTRYP PFNGLUNIFORMHANDLEUI64VARBPROC) (GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP PFNGLPROGRAMUNIFORMHANDLEUI64ARBPROC) (GLuint program, GLint location, GLuint64 value);
typedef void (APIENTRYP PFNGLPROGRAMUNIFORMHANDLEUI64VARBPROC) (GLuint program, GLint location, GLsizei count, const GLuint64 *values);
typedef GLboolean (APIENTRYP PFNGLISTEXTUREHANDLERESIDENTARBPROC) (GLuint64 handle);
typedef GLboolean (APIENTRYP PFNGLISIMAGEHANDLERESIDENTARBPROC) (GLuint64 handle);
typedef void (APIENTRYP PFNGLVERTEXATTRIBL1UI64ARBPROC) (GLuint index, GLuint64EXT x);
typedef void (APIENTRYP PFNGLVERTEXATTRIBL1UI64VARBPROC) (GLuint index, const GLuint64EXT *v);
typedef void (APIENTRYP PFNGLGETVERTEXATTRIBLUI64VARBPROC) (GLuint index, GLenum pname, GLuint64EXT *params);
#endif /* GL_ARB_bindless_texture */

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


extern   PFNGLACTIVETEXTUREPROC                 gl_ActiveTexture;
extern   PFNGLBLENDCOLORPROC                    gl_BlendColor;
extern   PFNGLATTACHSHADERPROC                  gl_AttachShader;
extern   PFNGLBINDBUFFERPROC                    gl_BindBuffer;
extern   PFNGLBINDBUFFERBASEPROC                gl_BindBufferBase;
extern   PFNGLBINDFRAMEBUFFERPROC               gl_BindFramebuffer;
extern   PFNGLBINDSAMPLERPROC                   gl_BindSampler;
extern   PFNGLBINDVERTEXARRAYPROC               gl_BindVertexArray;
extern   PFNGLBLENDEQUATIONSEPARATEIARBPROC     gl_BlendEquationSeparateiARB;
extern   PFNGLBLENDFUNCSEPARATEIARBPROC         gl_BlendFuncSeparateiARB;
extern   PFNGLBLITFRAMEBUFFERPROC               gl_BlitFramebuffer;
extern   PFNGLBUFFERDATAPROC                    gl_BufferData;
extern   PFNGLCHECKFRAMEBUFFERSTATUSPROC        gl_CheckFramebufferStatus;
extern   PFNGLCLEARBUFFERFVPROC                 gl_ClearBufferfv;
extern   PFNGLCLEARBUFFERIVPROC                 gl_ClearBufferiv;
extern   PFNGLCLEARBUFFERUIVPROC                gl_ClearBufferuiv;
extern   PFNGLCOMPILESHADERPROC                 gl_CompileShader;
extern   PFNGLCOLORMASKIPROC                    gl_ColorMaski;
extern   PFNGLCREATEPROGRAMPROC                 gl_CreateProgram;
extern   PFNGLCREATESHADERPROC                  gl_CreateShader;
extern   PFNGLCREATESHADERPROGRAMVPROC          gl_CreateShaderProgramv;
extern   PFNGLDELETEBUFFERSPROC                 gl_DeleteBuffers;
extern   PFNGLDELETEFRAMEBUFFERSPROC            gl_DeleteFramebuffers;
extern   PFNGLDELETEPROGRAMPROC                 gl_DeleteProgram;
extern   PFNGLDELETESAMPLERSPROC                gl_DeleteSamplers;
extern   PFNGLDELETESHADERPROC                  gl_DeleteShader;
extern   PFNGLDELETEVERTEXARRAYSPROC            gl_DeleteVertexArrays;
extern   PFNGLDETACHSHADERPROC                  gl_DetachShader;
extern   PFNGLDRAWBUFFERSPROC                   gl_DrawBuffers;
extern   PFNGLDRAWELEMENTSBASEVERTEXPROC        gl_DrawElementsBaseVertex;
extern   PFNGLENABLEVERTEXATTRIBARRAYPROC       gl_EnableVertexAttribArray;
extern   PFNGLFRAMEBUFFERRENDERBUFFERPROC       gl_FramebufferRenderbuffer;
extern   PFNGLFRAMEBUFFERTEXTURE2DPROC          gl_FramebufferTexture2D;
extern   PFNGLGENBUFFERSPROC                    gl_GenBuffers;
extern   PFNGLGENFRAMEBUFFERSPROC               gl_GenFramebuffers;
extern   PFNGLGENSAMPLERSPROC                   gl_GenSamplers;
extern   PFNGLGENVERTEXARRAYSPROC               gl_GenVertexArrays;
extern   PFNGLGETBUFFERPARAMETERIVPROC          gl_GetBufferParameteriv;
extern   PFNGLGETDEBUGMESSAGELOGARBPROC         gl_GetDebugMessageLogARB;
extern   PFNGLDEBUGMESSAGECALLBACKPROC          gl_DebugMessageCallback;
extern   PFNGLGETPROGRAMINFOLOGPROC             gl_GetProgramInfoLog;
extern   PFNGLGETPROGRAMIVPROC                  gl_GetProgramiv;
extern   PFNGLGETSHADERIVPROC                   gl_GetShaderiv;
extern   PFNGLGETSTRINGIPROC                    gl_GetStringi;
extern   PFNGLISFRAMEBUFFERPROC                 gl_IsFramebuffer;
extern   PFNGLLINKPROGRAMPROC                   gl_LinkProgram;
extern   PFNGLMAPBUFFERPROC                     gl_MapBuffer;
extern   PFNGLMAPBUFFERRANGEPROC                gl_MapBufferRange;
extern   PFNGLPROGRAMPARAMETERIPROC             gl_ProgramParameteri;
extern   PFNGLSAMPLERPARAMETERFPROC             gl_SamplerParameterf;
extern   PFNGLSAMPLERPARAMETERIPROC             gl_SamplerParameteri;
extern   PFNGLSHADERSOURCEPROC                  gl_ShaderSource;
extern   PFNGLUNIFORM1IPROC                     gl_Uniform1i;
extern   PFNGLUNMAPBUFFERPROC                   gl_UnmapBuffer;
extern   PFNGLUSEPROGRAMSTAGESPROC              gl_UseProgramStages;
extern   PFNGLVERTEXATTRIBIPOINTERPROC          gl_VertexAttribIPointer;
extern   PFNGLVERTEXATTRIBPOINTERPROC           gl_VertexAttribPointer;
extern   PFNGLBUFFERSUBDATAPROC                 gl_BufferSubData;
extern   PFNGLFENCESYNCPROC                     gl_FenceSync;
extern   PFNGLDELETESYNCPROC                    gl_DeleteSync;
extern   PFNGLCLIENTWAITSYNCPROC                gl_ClientWaitSync;
extern   PFNGLFLUSHMAPPEDBUFFERRANGEPROC        gl_FlushMappedBufferRange;
extern   PFNGLBLENDEQUATIONSEPARATEPROC         gl_BlendEquationSeparate;
extern   PFNGLBLENDFUNCSEPARATEPROC             gl_BlendFuncSeparate;
// GL4.0
extern   PFNGLUNIFORMSUBROUTINESUIVPROC         gl_UniformSubroutinesuiv;
// GL4.1
extern   PFNGLBINDPROGRAMPIPELINEPROC           gl_BindProgramPipeline;
extern   PFNGLDELETEPROGRAMPIPELINESPROC        gl_DeleteProgramPipelines;
extern   PFNGLGENPROGRAMPIPELINESPROC           gl_GenProgramPipelines;
extern   PFNGLGETPROGRAMPIPELINEIVPROC          gl_GetProgramPipelineiv;
extern   PFNGLVALIDATEPROGRAMPIPELINEPROC       gl_ValidateProgramPipeline;
extern   PFNGLGETPROGRAMPIPELINEINFOLOGPROC     gl_GetProgramPipelineInfoLog;
// NO GL4.1
extern   PFNGLUSEPROGRAMPROC                    gl_UseProgram;
extern   PFNGLGETSHADERINFOLOGPROC              gl_GetShaderInfoLog;
extern   PFNGLPROGRAMUNIFORM1IPROC              gl_ProgramUniform1i;
// GL4.2
extern   PFNGLBINDIMAGETEXTUREPROC              gl_BindImageTexture;
extern   PFNGLMEMORYBARRIERPROC                 gl_MemoryBarrier;
extern   PFNGLTEXSTORAGE2DPROC                  gl_TexStorage2D;
extern   PFNGLPOPDEBUGGROUPPROC                 gl_PopDebugGroup;
// GL4.3
extern   PFNGLCOPYIMAGESUBDATAPROC              gl_CopyImageSubData;
extern   PFNGLINVALIDATETEXIMAGEPROC            gl_InvalidateTexImage;
extern   PFNGLPUSHDEBUGGROUPPROC                gl_PushDebugGroup;
extern   PFNGLDEBUGMESSAGEINSERTPROC            gl_DebugMessageInsert;
// GL4.4
extern   PFNGLCLEARTEXIMAGEPROC                 gl_ClearTexImage;
extern   PFNGLBUFFERSTORAGEPROC                 gl_BufferStorage;
// GL_ARB_bindless_texture (GL5?)
extern PFNGLGETTEXTURESAMPLERHANDLEARBPROC      gl_GetTextureSamplerHandleARB;
extern PFNGLMAKETEXTUREHANDLERESIDENTARBPROC    gl_MakeTextureHandleResidentARB;
extern PFNGLMAKETEXTUREHANDLENONRESIDENTARBPROC gl_MakeTextureHandleNonResidentARB;
extern PFNGLUNIFORMHANDLEUI64VARBPROC           gl_UniformHandleui64vARB;
extern PFNGLPROGRAMUNIFORMHANDLEUI64VARBPROC    gl_ProgramUniformHandleui64vARB;

// GL4.5
extern PFNGLCREATETEXTURESPROC					gl_CreateTextures;
extern PFNGLTEXTURESTORAGE2DPROC				gl_TextureStorage2D;
extern PFNGLTEXTURESUBIMAGE2DPROC				gl_TextureSubImage2D;
extern PFNGLCOPYTEXTURESUBIMAGE2DPROC			gl_CopyTextureSubImage2D;
extern PFNGLBINDTEXTUREUNITPROC					gl_BindTextureUnit;
extern PFNGLGETTEXTUREIMAGEPROC                 gl_GetTextureImage;

extern PFNGLCREATEFRAMEBUFFERSPROC				gl_CreateFramebuffers;
extern PFNGLCLEARNAMEDFRAMEBUFFERFVPROC			gl_ClearNamedFramebufferfv;
extern PFNGLCLEARNAMEDFRAMEBUFFERIVPROC         gl_ClearNamedFramebufferiv;
extern PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC        gl_ClearNamedFramebufferuiv;
extern PFNGLNAMEDFRAMEBUFFERTEXTUREPROC			gl_NamedFramebufferTexture;
extern PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC     gl_NamedFramebufferDrawBuffers;
extern PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC      gl_NamedFramebufferReadBuffer;
extern PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC		gl_CheckNamedFramebufferStatus;

extern PFNGLCREATEBUFFERSPROC                   gl_CreateBuffers;
extern PFNGLNAMEDBUFFERSTORAGEPROC              gl_NamedBufferStorage;
extern PFNGLNAMEDBUFFERDATAPROC                 gl_NamedBufferData;
extern PFNGLNAMEDBUFFERSUBDATAPROC              gl_NamedBufferSubData;
extern PFNGLMAPNAMEDBUFFERPROC                  gl_MapNamedBuffer;
extern PFNGLMAPNAMEDBUFFERRANGEPROC             gl_MapNamedBufferRange;
extern PFNGLUNMAPNAMEDBUFFERPROC                gl_UnmapNamedBuffer;
extern PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC     gl_FlushMappedNamedBufferRange;

extern PFNGLCREATESAMPLERSPROC                  gl_CreateSamplers;
extern PFNGLCREATEPROGRAMPIPELINESPROC          gl_CreateProgramPipelines;

extern PFNGLCLIPCONTROLPROC                     gl_ClipControl;
extern PFNGLTEXTUREBARRIERPROC                  gl_TextureBarrier;

namespace Emulate_DSA {
	extern void SetFramebufferTarget(GLenum target);
	extern void SetBufferTarget(GLenum target);
	extern void Init();
}

namespace GLLoader {
	bool check_gl_version(uint32 major, uint32 minor);
	void init_gl_function();
	bool check_gl_supported_extension();

	extern bool fglrx_buggy_driver;
	extern bool mesa_amd_buggy_driver;
	extern bool nvidia_buggy_driver;
	extern bool intel_buggy_driver;
	extern bool in_replayer;

	// GL
	extern bool found_GL_ARB_separate_shader_objects;
	extern bool found_GL_ARB_copy_image;
	extern bool found_geometry_shader;
	extern bool found_GL_ARB_gpu_shader5;
	extern bool found_GL_ARB_shader_image_load_store;
	extern bool found_GL_ARB_clear_texture;
	extern bool found_GL_ARB_buffer_storage;
	extern bool found_GL_ARB_shader_subroutine;
	extern bool found_GL_ARB_bindless_texture;
	extern bool found_GL_ARB_explicit_uniform_location;
	extern bool found_GL_ARB_clip_control;
	extern bool found_GL_ARB_direct_state_access;
	extern bool found_GL_ARB_texture_barrier;
	extern bool found_GL_EXT_texture_filter_anisotropic;
}
