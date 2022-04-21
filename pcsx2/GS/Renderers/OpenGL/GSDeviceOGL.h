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

#include "common/GL/Context.h"
#include "common/GL/StreamBuffer.h"
#include "common/GL/Program.h"
#include "common/GL/ShaderCache.h"
#include "common/HashCombine.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GSTextureOGL.h"
#include "GSUniformBufferOGL.h"
#include "GLState.h"
#include "GLLoader.h"
#include "GS/GS.h"

#ifdef ENABLE_OGL_DEBUG_MEM_BW
extern u64 g_real_texture_upload_byte;
extern u64 g_vertex_upload_byte;
#endif

class GSDepthStencilOGL
{
	bool m_depth_enable;
	GLenum m_depth_func;
	bool m_depth_mask;
	// Note front face and back might be split but it seems they have same parameter configuration
	bool m_stencil_enable;
	GLenum m_stencil_func;
	GLenum m_stencil_spass_dpass_op;

public:
	GSDepthStencilOGL()
		: m_depth_enable(false)
		, m_depth_func(GL_ALWAYS)
		, m_depth_mask(0)
		, m_stencil_enable(false)
		, m_stencil_func(0)
		, m_stencil_spass_dpass_op(GL_KEEP)
	{
	}

	void EnableDepth() { m_depth_enable = true; }
	void EnableStencil() { m_stencil_enable = true; }

	void SetDepth(GLenum func, bool mask)
	{
		m_depth_func = func;
		m_depth_mask = mask;
	}
	void SetStencil(GLenum func, GLenum pass)
	{
		m_stencil_func = func;
		m_stencil_spass_dpass_op = pass;
	}

	void SetupDepth()
	{
		if (GLState::depth != m_depth_enable)
		{
			GLState::depth = m_depth_enable;
			if (m_depth_enable)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
		}

		if (m_depth_enable)
		{
			if (GLState::depth_func != m_depth_func)
			{
				GLState::depth_func = m_depth_func;
				glDepthFunc(m_depth_func);
			}
			if (GLState::depth_mask != m_depth_mask)
			{
				GLState::depth_mask = m_depth_mask;
				glDepthMask((GLboolean)m_depth_mask);
			}
		}
	}

	void SetupStencil()
	{
		if (GLState::stencil != m_stencil_enable)
		{
			GLState::stencil = m_stencil_enable;
			if (m_stencil_enable)
				glEnable(GL_STENCIL_TEST);
			else
				glDisable(GL_STENCIL_TEST);
		}

		if (m_stencil_enable)
		{
			// Note: here the mask control which bitplane is considered by the operation
			if (GLState::stencil_func != m_stencil_func)
			{
				GLState::stencil_func = m_stencil_func;
				glStencilFunc(m_stencil_func, 1, 1);
			}
			if (GLState::stencil_pass != m_stencil_spass_dpass_op)
			{
				GLState::stencil_pass = m_stencil_spass_dpass_op;
				glStencilOp(GL_KEEP, GL_KEEP, m_stencil_spass_dpass_op);
			}
		}
	}

	bool IsMaskEnable() { return m_depth_mask != GL_FALSE; }
};

class GSDeviceOGL final : public GSDevice
{
public:
	struct VSSelector
	{
		union
		{
			struct
			{
				u8 int_fst : 1;
				u8 iip : 1;
				u8 point_size : 1;
				u8 _free : 5;
			};

			u8 key;
		};

		VSSelector()
			: key(0)
		{
		}
		VSSelector(u8 k)
			: key(k)
		{
		}
	};

	struct GSSelector
	{
		union
		{
			struct
			{
				u8 sprite : 1;
				u8 point  : 1;
				u8 line   : 1;
				u8 iip    : 1;

				u8 _free : 4;
			};

			u8 key;
		};

		operator u32() const { return key; }

		GSSelector()
			: key(0)
		{
		}
		GSSelector(u8 k)
			: key(k)
		{
		}
	};

	using PSSelector = GSHWDrawConfig::PSSelector;
	using PSSamplerSelector = GSHWDrawConfig::SamplerSelector;
	using OMDepthStencilSelector = GSHWDrawConfig::DepthStencilSelector;
	using OMColorMaskSelector = GSHWDrawConfig::ColorMaskSelector;

	struct alignas(16) ProgramSelector
	{
		PSSelector ps;
		VSSelector vs;
		GSSelector gs;
		u16 pad;

		__fi bool operator==(const ProgramSelector& p) const { return (std::memcmp(this, &p, sizeof(*this)) == 0); }
		__fi bool operator!=(const ProgramSelector& p) const { return (std::memcmp(this, &p, sizeof(*this)) != 0); }
	};
	static_assert(sizeof(ProgramSelector) == 16, "Program selector is 16 bytes");

	struct ProgramSelectorHash
	{
		__fi std::size_t operator()(const ProgramSelector& p) const noexcept
		{
			std::size_t h = 0;
			HashCombine(h, p.vs.key, p.gs.key, p.ps.key_hi, p.ps.key_lo);
			return h;
		}
	};

	static int m_shader_inst;
	static int m_shader_reg;

private:
	// Increment this constant whenever shaders change, to invalidate user's program binary cache.
	static constexpr u32 SHADER_VERSION = 3;

	static FILE* m_debug_gl_file;

	bool m_disable_hw_gl_draw;

	// Place holder for the GLSL shader code (to avoid useless reload)
	std::string m_shader_common_header;
	std::string m_shader_tfx_vgs;
	std::string m_shader_tfx_fs;

	GLuint m_fbo; // frame buffer container
	GLuint m_fbo_read; // frame buffer container only for reading

	std::unique_ptr<GL::StreamBuffer> m_vertex_stream_buffer;
	std::unique_ptr<GL::StreamBuffer> m_index_stream_buffer;
	GLuint m_vertex_array_object = 0;
	GLenum m_draw_topology = 0;

	std::unique_ptr<GL::StreamBuffer> m_vertex_uniform_stream_buffer;
	std::unique_ptr<GL::StreamBuffer> m_fragment_uniform_stream_buffer;
	GLint m_uniform_buffer_alignment = 0;

	struct
	{
		GL::Program ps[2]; // program object
	} m_merge_obj;

	struct
	{
		GL::Program ps[4]; // program object
	} m_interlace;

	struct
	{
		std::string vs;
		GL::Program ps[static_cast<int>(ShaderConvert::Count)]; // program object
		GLuint ln = 0; // sampler object
		GLuint pt = 0; // sampler object
		GSDepthStencilOGL* dss = nullptr;
		GSDepthStencilOGL* dss_write = nullptr;
	} m_convert;

	struct
	{
		GL::Program ps;
	} m_fxaa;

#ifndef PCSX2_CORE
	struct
	{
		GL::Program ps;
	} m_shaderfx;
#endif

	struct
	{
		GSDepthStencilOGL* dss = nullptr;
		GSTexture* t = nullptr;
	} m_date;

	struct
	{
		GL::Program ps;
	} m_shadeboost;

	struct
	{
		u16 last_query = 0;
		GLuint timer_query[1 << 16] = {};

		GLuint timer() { return timer_query[last_query]; }
	} m_profiler;

	GLuint m_ps_ss[1 << 8];
	GSDepthStencilOGL* m_om_dss[1 << 5] = {};
	std::unordered_map<ProgramSelector, GL::Program, ProgramSelectorHash> m_programs;
	GL::ShaderCache m_shader_cache;

	GLuint m_palette_ss;

	GSHWDrawConfig::VSConstantBuffer m_vs_cb_cache;
	GSHWDrawConfig::PSConstantBuffer m_ps_cb_cache;

	AlignedBuffer<u8, 32> m_download_buffer;

	GSTexture* CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format) final;

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c) final;
	void DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset = 0) final;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) final;
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4]) final;
	void DoExternalFX(GSTexture* sTex, GSTexture* dTex) final;

	void OMAttachRt(GSTextureOGL* rt = NULL);
	void OMAttachDs(GSTextureOGL* ds = NULL);
	void OMSetFBO(GLuint fbo);

	void DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2i& ds);

public:
	GSDeviceOGL();
	virtual ~GSDeviceOGL();

	void GenerateProfilerData();

	// Used by OpenGL, so the same calling convention is required.
	static void APIENTRY DebugOutputToFile(GLenum gl_source, GLenum gl_type, GLuint id, GLenum gl_severity, GLsizei gl_length, const GLchar* gl_message, const void* userParam);

	bool Create(HostDisplay* display) override;

	void ResetAPIState() override;
	void RestoreAPIState() override;

	void DrawPrimitive();
	void DrawIndexedPrimitive();
	void DrawIndexedPrimitive(int offset, int count);

	void ClearRenderTarget(GSTexture* t, const GSVector4& c) final;
	void ClearRenderTarget(GSTexture* t, u32 c) final;
	void InvalidateRenderTarget(GSTexture* t) final;
	void ClearDepth(GSTexture* t) final;
	void ClearStencil(GSTexture* t, u8 c) final;

	void InitPrimDateTexture(GSTexture* rt, const GSVector4i& area);
	void RecycleDateTexture();

	bool DownloadTexture(GSTexture* src, const GSVector4i& rect, GSTexture::GSMap& out_map) final;

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY) final;

	void PushDebugGroup(const char* fmt, ...) final;
	void PopDebugGroup() final;
	void InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...) final;

	// BlitRect *does* mess with GL state, be sure to re-bind.
	void BlitRect(GSTexture* sTex, const GSVector4i& r, const GSVector2i& dsize, bool at_origin, bool linear);

	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderConvert shader = ShaderConvert::COPY, bool linear = true) final;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, const GL::Program& ps, bool linear = true);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha) final;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, const GL::Program& ps, bool alpha_blend, OMColorMaskSelector cms, bool linear = true);

	void RenderHW(GSHWDrawConfig& config) final;
	void SendHWDraw(const GSHWDrawConfig& config, bool needs_barrier);

	void SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm);

	void IASetPrimitiveTopology(GLenum topology);
	void IASetVertexBuffer(const void* vertices, size_t count);
	void IASetIndexBuffer(const void* index, size_t count);

	void PSSetShaderResource(int i, GSTexture* sr);
	void PSSetShaderResources(GSTexture* sr0, GSTexture* sr1);
	void PSSetSamplerState(GLuint ss);
	void ClearSamplerCache() final;

	void OMSetDepthStencilState(GSDepthStencilOGL* dss);
	void OMSetBlendState(bool enable = false, GLenum src_factor = GL_ONE, GLenum dst_factor = GL_ZERO, GLenum op = GL_FUNC_ADD, bool is_constant = false, u8 constant = 0);
	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor = NULL);
	void OMSetColorMaskState(OMColorMaskSelector sel = OMColorMaskSelector());

	bool HasColorSparse() final { return GLLoader::found_compatible_GL_ARB_sparse_texture2; }
	bool HasDepthSparse() final { return GLLoader::found_compatible_sparse_depth; }

	bool CreateTextureFX();
	std::string GetShaderSource(const std::string_view& entry, GLenum type, const std::string_view& common_header, const std::string_view& glsl_h_code, const std::string_view& macro_sel);
	std::string GenGlslHeader(const std::string_view& entry, GLenum type, const std::string_view& macro);
	std::string GetVSSource(VSSelector sel);
	std::string GetGSSource(GSSelector sel);
	std::string GetPSSource(const PSSelector& sel);
	GLuint CreateSampler(PSSamplerSelector sel);
	GSDepthStencilOGL* CreateDepthStencil(OMDepthStencilSelector dssel);

	void SetupPipeline(const ProgramSelector& psel);
	void SetupSampler(PSSamplerSelector ssel);
	void SetupOM(OMDepthStencilSelector dssel);
	GLuint GetSamplerID(PSSamplerSelector ssel);
	GLuint GetPaletteSamplerID();

	void Barrier(GLbitfield b);
};
