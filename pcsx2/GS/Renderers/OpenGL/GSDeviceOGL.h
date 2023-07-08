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

#pragma once

#include "GS/Renderers/Common/GSDevice.h"
#include "GS/Renderers/OpenGL/GLProgram.h"
#include "GS/Renderers/OpenGL/GLShaderCache.h"
#include "GS/Renderers/OpenGL/GLState.h"
#include "GS/Renderers/OpenGL/GLStreamBuffer.h"
#include "GS/Renderers/OpenGL/GSTextureOGL.h"
#include "GS/GS.h"

#include "common/HashCombine.h"

class GLContext;

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
	using VSSelector = GSHWDrawConfig::VSSelector;
	using PSSelector = GSHWDrawConfig::PSSelector;
	using PSSamplerSelector = GSHWDrawConfig::SamplerSelector;
	using OMDepthStencilSelector = GSHWDrawConfig::DepthStencilSelector;
	using OMColorMaskSelector = GSHWDrawConfig::ColorMaskSelector;

	struct alignas(16) ProgramSelector
	{
		PSSelector ps;
		VSSelector vs;
		u8 pad[3];

		__fi bool operator==(const ProgramSelector& p) const { return BitEqual(*this, p); }
		__fi bool operator!=(const ProgramSelector& p) const { return !BitEqual(*this, p); }
	};
	static_assert(sizeof(ProgramSelector) == 16, "Program selector is 16 bytes");

	struct ProgramSelectorHash
	{
		__fi std::size_t operator()(const ProgramSelector& p) const noexcept
		{
			std::size_t h = 0;
			HashCombine(h, p.vs.key, p.ps.key_hi, p.ps.key_lo);
			return h;
		}
	};

private:
	static constexpr u8 NUM_TIMESTAMP_QUERIES = 5;

	std::unique_ptr<GLContext> m_gl_context;

	bool m_disable_download_pbo = false;

	GLuint m_fbo = 0; // frame buffer container
	GLuint m_fbo_read = 0; // frame buffer container only for reading
	GLuint m_fbo_write = 0;	// frame buffer container only for writing

	std::unique_ptr<GLStreamBuffer> m_texture_upload_buffer;

	std::unique_ptr<GLStreamBuffer> m_vertex_stream_buffer;
	std::unique_ptr<GLStreamBuffer> m_index_stream_buffer;
	GLuint m_expand_ibo = 0;
	GLuint m_vao = 0;
	GLuint m_expand_vao = 0;
	GLenum m_draw_topology = 0;

	std::unique_ptr<GLStreamBuffer> m_vertex_uniform_stream_buffer;
	std::unique_ptr<GLStreamBuffer> m_fragment_uniform_stream_buffer;
	GLint m_uniform_buffer_alignment = 0;

	struct
	{
		GLProgram ps[2]; // program object
	} m_merge_obj;

	struct
	{
		GLProgram ps[NUM_INTERLACE_SHADERS]; // program object
	} m_interlace;

	struct
	{
		std::string vs;
		GLProgram ps[static_cast<int>(ShaderConvert::Count)]; // program object
		GLuint ln = 0; // sampler object
		GLuint pt = 0; // sampler object
		GSDepthStencilOGL* dss = nullptr;
		GSDepthStencilOGL* dss_write = nullptr;
	} m_convert;

	GLProgram m_present[static_cast<int>(PresentShader::Count)];

	struct
	{
		GLProgram ps;
	} m_fxaa;

	struct
	{
		GSDepthStencilOGL* dss = nullptr;
		GLProgram primid_ps[2];
	} m_date;

	struct
	{
		GLProgram ps;
	} m_shadeboost;

	struct
	{
		GLProgram upscale_ps;
		GLProgram sharpen_ps;
	} m_cas;

	struct
	{
		GLProgram ps;
		GLuint vao = 0;
	} m_imgui;

	GLuint m_ps_ss[1 << 8];
	GSDepthStencilOGL* m_om_dss[1 << 5] = {};
	std::unordered_map<ProgramSelector, GLProgram, ProgramSelectorHash> m_programs;
	GLShaderCache m_shader_cache;

	GLuint m_palette_ss = 0;

	std::array<GLuint, NUM_TIMESTAMP_QUERIES> m_timestamp_queries = {};
	float m_accumulated_gpu_time = 0.0f;
	u8 m_read_timestamp_query = 0;
	u8 m_write_timestamp_query = 0;
	u8 m_waiting_timestamp_queries = 0;
	bool m_timestamp_query_started = false;
	bool m_gpu_timing_enabled = false;

	GSHWDrawConfig::VSConstantBuffer m_vs_cb_cache;
	GSHWDrawConfig::PSConstantBuffer m_ps_cb_cache;

	std::string m_shader_tfx_vgs;
	std::string m_shader_tfx_fs;

	bool CheckFeatures(bool& buggy_pbo);

	void SetSwapInterval();
	void DestroyResources();

	void CreateTimestampQueries();
	void DestroyTimestampQueries();
	void PopTimestampQuery();
	void KickTimestampQuery();

	GSTexture* CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format) override;

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, u32 c, const bool linear) override;
	void DoInterlace(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderInterlace shader, bool linear, const InterlaceConstantBuffer& cb) override;

	bool CompileFXAAProgram();
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) override;

	bool CompileShadeBoostProgram();
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4]) override;

	bool CreateCASPrograms();
	bool DoCAS(GSTexture* sTex, GSTexture* dTex, bool sharpen_only, const std::array<u32, NUM_CAS_CONSTANTS>& constants) override;

	bool CreateImGuiProgram();
	void RenderImGui();
	void RenderBlankFrame();

	void OMAttachRt(GSTexture* rt = nullptr);
	void OMAttachDs(GSTexture* ds = nullptr);
	void OMSetFBO(GLuint fbo);

	void DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2i& ds);

public:
	GSDeviceOGL();
	virtual ~GSDeviceOGL();

	__fi static GSDeviceOGL* GetInstance() { return static_cast<GSDeviceOGL*>(g_gs_device.get()); }

	// Used by OpenGL, so the same calling convention is required.
	static void APIENTRY DebugMessageCallback(GLenum gl_source, GLenum gl_type, GLuint id, GLenum gl_severity, GLsizei gl_length, const GLchar* gl_message, const void* userParam);

	__fi bool IsDownloadPBODisabled() const { return m_disable_download_pbo; }
	__fi u32 GetFBORead() const { return m_fbo_read; }
	__fi u32 GetFBOWrite() const { return m_fbo_write; }
	__fi GLStreamBuffer* GetTextureUploadBuffer() const { return m_texture_upload_buffer.get(); }
	void CommitClear(GSTexture* t, bool use_write_fbo);

	RenderAPI GetRenderAPI() const override;
	bool HasSurface() const override;

	bool Create() override;
	void Destroy() override;

	bool UpdateWindow() override;
	void ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale) override;
	bool SupportsExclusiveFullscreen() const override;
	void DestroySurface() override;
	std::string GetDriverInfo() const override;

	void SetVSync(VsyncMode mode) override;

	PresentResult BeginPresent(bool frame_skip) override;
	void EndPresent() override;

	bool SetGPUTimingEnabled(bool enabled) override;
	float GetAndResetAccumulatedGPUTime() override;

	void DrawPrimitive();
	void DrawIndexedPrimitive();
	void DrawIndexedPrimitive(int offset, int count);

	std::unique_ptr<GSDownloadTexture> CreateDownloadTexture(u32 width, u32 height, GSTexture::Format format) override;

	GSTexture* InitPrimDateTexture(GSTexture* rt, const GSVector4i& area, bool datm);

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY) override;

	void PushDebugGroup(const char* fmt, ...) override;
	void PopDebugGroup() override;
	void InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...) override;

	// BlitRect *does* mess with GL state, be sure to re-bind.
	void BlitRect(GSTexture* sTex, const GSVector4i& r, const GSVector2i& dsize, bool at_origin, bool linear);

	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderConvert shader = ShaderConvert::COPY, bool linear = true) override;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, const GLProgram& ps, bool linear = true);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha) override;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, const GLProgram& ps, bool alpha_blend, OMColorMaskSelector cms, bool linear = true);
	void PresentRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, PresentShader shader, float shaderTime, bool linear) override;
	void UpdateCLUTTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, GSTexture* dTex, u32 dOffset, u32 dSize) override;
	void ConvertToIndexedTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, u32 SBW, u32 SPSM, GSTexture* dTex, u32 DBW, u32 DPSM) override;

	void DrawMultiStretchRects(const MultiStretchRect* rects, u32 num_rects, GSTexture* dTex, ShaderConvert shader) override;
	void DoMultiStretchRects(const MultiStretchRect* rects, u32 num_rects, const GSVector2& ds);

	void RenderHW(GSHWDrawConfig& config) override;
	void SendHWDraw(const GSHWDrawConfig& config, bool needs_barrier);

	void SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm);

	void IASetVAO(GLuint vao);
	void IASetPrimitiveTopology(GLenum topology);
	void IASetVertexBuffer(const void* vertices, size_t count);
	void IASetIndexBuffer(const void* index, size_t count);

	void PSSetShaderResource(int i, GSTexture* sr);
	void PSSetSamplerState(GLuint ss);
	void ClearSamplerCache() override;

	void OMSetDepthStencilState(GSDepthStencilOGL* dss);
	void OMSetBlendState(bool enable = false, GLenum src_factor = GL_ONE, GLenum dst_factor = GL_ZERO, GLenum op = GL_FUNC_ADD, bool is_constant = false, u8 constant = 0);
	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor = nullptr);
	void OMSetColorMaskState(OMColorMaskSelector sel = OMColorMaskSelector());
	void OMUnbindTexture(GSTextureOGL* tex);

	void SetViewport(const GSVector2i& viewport);
	void SetScissor(const GSVector4i& scissor);

	bool CreateTextureFX();
	std::string GetShaderSource(const std::string_view& entry, GLenum type, const std::string_view& glsl_h_code,
		const std::string_view& macro_sel = std::string_view());
	std::string GenGlslHeader(const std::string_view& entry, GLenum type, const std::string_view& macro);
	std::string GetVSSource(VSSelector sel);
	std::string GetPSSource(const PSSelector& sel);
	GLuint CreateSampler(PSSamplerSelector sel);
	GSDepthStencilOGL* CreateDepthStencil(OMDepthStencilSelector dssel);

	void SetupPipeline(const ProgramSelector& psel);
	void SetupSampler(PSSamplerSelector ssel);
	void SetupOM(OMDepthStencilSelector dssel);
	GLuint GetSamplerID(PSSamplerSelector ssel);
	GLuint GetPaletteSamplerID();
};
