/*
 *	Copyright (C) 2011-2013 Gregory hainaut
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

#include "GSDevice.h"
#include "GSTextureOGL.h"
#include "GSdx.h"
#include "GSVertexArrayOGL.h"
#include "GSUniformBufferOGL.h"
#include "GSShaderOGL.h"
#include "GLState.h"

// A couple of flag to determine the blending behavior
#define BLEND_A_MAX		(0x100) // Impossible blending uses coeff bigger than 1
#define BLEND_C_CLR		(0x200) // Clear color blending (use directly the destination color as blending factor)
#define BLEND_NO_BAR	(0x400) // don't require texture barrier for the blending (because the RT is not used)
#define BLEND_ACCU		(0x800) // Allow to use a mix of SW and HW blending to keep the best of the 2 worlds

#ifdef ENABLE_OGL_DEBUG_MEM_BW
extern uint64 g_real_texture_upload_byte;
extern uint64 g_vertex_upload_byte;
#endif

class GSDepthStencilOGL {
	bool m_depth_enable;
	GLenum m_depth_func;
	bool m_depth_mask;
	// Note front face and back might be split but it seems they have same parameter configuration
	bool m_stencil_enable;
	GLenum m_stencil_func;
	GLenum m_stencil_spass_dpass_op;

public:

	GSDepthStencilOGL() : m_depth_enable(false)
		, m_depth_func(GL_ALWAYS)
		, m_depth_mask(0)
		, m_stencil_enable(false)
		, m_stencil_func(0)
		, m_stencil_spass_dpass_op(GL_KEEP)
	{
	}

	void EnableDepth() { m_depth_enable = true; }
	void EnableStencil() { m_stencil_enable = true; }

	void SetDepth(GLenum func, bool mask) { m_depth_func = func; m_depth_mask = mask; }
	void SetStencil(GLenum func, GLenum pass) { m_stencil_func = func; m_stencil_spass_dpass_op = pass; }

	void SetupDepth()
	{
		if (GLState::depth != m_depth_enable) {
			GLState::depth = m_depth_enable;
			if (m_depth_enable)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
		}

		if (m_depth_enable) {
			if (GLState::depth_func != m_depth_func) {
				GLState::depth_func = m_depth_func;
				glDepthFunc(m_depth_func);
			}
			if (GLState::depth_mask != m_depth_mask) {
				GLState::depth_mask = m_depth_mask;
				glDepthMask((GLboolean)m_depth_mask);
			}
		}
	}

	void SetupStencil()
	{
		if (GLState::stencil != m_stencil_enable) {
			GLState::stencil = m_stencil_enable;
			if (m_stencil_enable)
				glEnable(GL_STENCIL_TEST);
			else
				glDisable(GL_STENCIL_TEST);
		}

		if (m_stencil_enable) {
			// Note: here the mask control which bitplane is considered by the operation
			if (GLState::stencil_func != m_stencil_func) {
				GLState::stencil_func = m_stencil_func;
				glStencilFunc(m_stencil_func, 1, 1);
			}
			if (GLState::stencil_pass != m_stencil_spass_dpass_op) {
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
	__aligned(struct, 32) VSConstantBuffer
	{
		GSVector4 Vertex_Scale_Offset;
		GSVector4 TextureScale;

		VSConstantBuffer()
		{
			Vertex_Scale_Offset = GSVector4::zero();
			TextureScale = GSVector4::zero();
		}

		__forceinline bool Update(const VSConstantBuffer* cb)
		{
			GSVector4i* a = (GSVector4i*)this;
			GSVector4i* b = (GSVector4i*)cb;

			if(!((a[0] == b[0]) & (a[1] == b[1])).alltrue())
			{
				a[0] = b[0];
				a[1] = b[1];

				return true;
			}

			return false;
		}
	};

	struct VSSelector
	{
		union
		{
			struct
			{
				uint32 wildhack:1;
				uint32 bppz:2;

				uint32 _free:29;
			};

			uint32 key;
		};

		operator uint32() const {return key;}

		VSSelector() : key(0) {}
		VSSelector(uint32 k) : key(k) {}
	};

	struct GSSelector
	{
		union
		{
			struct
			{
				uint32 sprite:1;
				uint32 point:1;

				uint32 _free:30;
			};

			uint32 key;
		};

		operator uint32() const {return key;}

		GSSelector() : key(0) {}
		GSSelector(uint32 k) : key(k) {}
	};

	__aligned(struct, 32) PSConstantBuffer
	{
		GSVector4 FogColor_AREF;
		GSVector4 WH;
		GSVector4 TA_Af;
		GSVector4i MskFix;
		GSVector4i FbMask;

		GSVector4 HalfTexel;
		GSVector4 MinMax;
		GSVector4 TC_OH_TS;

		PSConstantBuffer()
		{
			FogColor_AREF = GSVector4::zero();
			HalfTexel     = GSVector4::zero();
			WH            = GSVector4::zero();
			MinMax        = GSVector4::zero();
			MskFix        = GSVector4i::zero();
			TC_OH_TS      = GSVector4::zero();
			FbMask        = GSVector4i::zero();
		}

		__forceinline bool Update(const PSConstantBuffer* cb)
		{
			GSVector4i* a = (GSVector4i*)this;
			GSVector4i* b = (GSVector4i*)cb;

			// if WH matches both HalfTexel and TC_OH_TS do too
			// MinMax depends on WH and MskFix so no need to check it too
			if(!((a[0] == b[0]) & (a[1] == b[1]) & (a[2] == b[2]) & (a[3] == b[3]) & (a[4] == b[4])).alltrue())
			{
				// Note previous check uses SSE already, a plain copy will be faster than any memcpy
				a[0] = b[0];
				a[1] = b[1];
				a[2] = b[2];
				a[3] = b[3];
				a[4] = b[4];
				a[5] = b[5];

				return true;
			}

			return false;
		}
	};

	struct PSSelector
	{
		// Performance note: there are too many shader combinations
		// It might hurt the performance due to frequent toggling worse it could consume
		// a lots of memory.
		union
		{
			struct
			{
				// *** Word 1
				// Format
				uint32 tex_fmt:4;
				uint32 dfmt:2;
				// Alpha extension/Correction
				uint32 aem:1;
				uint32 fba:1;
				// Fog
				uint32 fog:1;
				// Flat/goround shading
				uint32 iip:1;
				// Pixel test
				uint32 date:3;
				uint32 atst:3;
				// Color sampling
				uint32 fst:1; // Investigate to do it on the VS
				uint32 tfx:3;
				uint32 tcc:1;
				uint32 wms:2;
				uint32 wmt:2;
				uint32 ltf:1;
				// Shuffle and fbmask effect
				uint32 shuffle:1;
				uint32 read_ba:1;
				uint32 write_rg:1;
				uint32 fbmask:1;

				uint32 _free1:2;

				// *** Word 2
				// Blend and Colclip
				uint32 blend_a:2;
				uint32 blend_b:2;
				uint32 blend_c:2;
				uint32 blend_d:2;
				uint32 clr1:1; // useful?
				uint32 pabe:1;
				uint32 hdr:1;
				uint32 colclip:1;

				// Hack
				uint32 tcoffsethack:1;

				uint32 _free2:19;
			};

			uint64 key;
		};

		// FIXME is the & useful ?
		operator uint64() const {return key;}

		PSSelector() : key(0) {}
	};

	struct PSSamplerSelector
	{
		union
		{
			struct
			{
				uint32 tau:1;
				uint32 tav:1;
				uint32 ltf:1;
				uint32 aniso:1;

				uint32 _free:28;
			};

			uint32 key;
		};

		operator uint32() {return key;}

		PSSamplerSelector() : key(0) {}
		PSSamplerSelector(uint32 k) : key(k) {}
	};

	struct OMDepthStencilSelector
	{
		union
		{
			struct
			{
				uint32 ztst:2;
				uint32 zwe:1;
				uint32 date:1;

				uint32 _free:28;
			};

			uint32 key;
		};

		// FIXME is the & useful ?
		operator uint32() {return key;}

		OMDepthStencilSelector() : key(0) {}
		OMDepthStencilSelector(uint32 k) : key(k) {}
	};

	struct OMColorMaskSelector
	{
		union
		{
			struct
			{
				uint32 wr:1;
				uint32 wg:1;
				uint32 wb:1;
				uint32 wa:1;

				uint32 _free:28;
			};

			struct
			{
				uint32 wrgba:4;
			};

			uint32 key;
		};

		// FIXME is the & useful ?
		operator uint32() {return key & 0xf;}

		OMColorMaskSelector() : key(0xF) {}
		OMColorMaskSelector(uint32 c) { wrgba = c; }
	};

	struct OGLBlend {uint16 bogus, op, src, dst;};
	static const OGLBlend m_blendMapOGL[3*3*3*3 + 1];
	static const int m_NO_BLEND;
	static const int m_MERGE_BLEND;

	static int s_n;

	private:
	uint32 m_msaa;				// Level of Msaa

	static bool m_debug_gl_call;
	static FILE* m_debug_gl_file;

	GSWnd* m_window;

	GLuint m_fbo;				// frame buffer container
	GLuint m_fbo_read;			// frame buffer container only for reading

	GSVertexBufferStateOGL* m_va;// state of the vertex buffer/array

	struct {
		GLuint ps[2];				 // program object
		GSUniformBufferOGL* cb;		 // uniform buffer object
	} m_merge_obj;

	struct {
		GLuint ps[4];				// program object
		GSUniformBufferOGL* cb;		// uniform buffer object
	} m_interlace;

	struct {
		GLuint vs;		// program object
		GLuint ps[18];	// program object
		GLuint ln;		// sampler object
		GLuint pt;		// sampler object
		GSDepthStencilOGL* dss;
		GSDepthStencilOGL* dss_write;
		GSUniformBufferOGL* cb;
	} m_convert;

	struct {
		GLuint ps;
		GSUniformBufferOGL *cb;
	} m_fxaa;

	struct {
		GLuint ps;
		GSUniformBufferOGL* cb;
	} m_shaderfx;

	struct {
		GSDepthStencilOGL* dss;
		GSTexture* t;
	} m_date;

	struct {
		GLuint ps;
	} m_shadeboost;

	GLuint m_vs[1<<3];
	GLuint m_gs[1<<2];
	GLuint m_ps_ss[1<<4];
	GSDepthStencilOGL* m_om_dss[1<<4];
	hash_map<uint64, GLuint > m_ps;
	GLuint m_apitrace;

	GLuint m_palette_ss;

	GSUniformBufferOGL* m_vs_cb;
	GSUniformBufferOGL* m_ps_cb;

	VSConstantBuffer m_vs_cb_cache;
	PSConstantBuffer m_ps_cb_cache;

	GSTexture* CreateSurface(int type, int w, int h, bool msaa, int format);
	GSTexture* FetchSurface(int type, int w, int h, bool msaa, int format);

	void DoMerge(GSTexture* sTex[2], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, bool slbg, bool mmod, const GSVector4& c) final;
	void DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset = 0) final;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) final;
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex) final;
	void DoExternalFX(GSTexture* sTex, GSTexture* dTex) final;

	void OMAttachRt(GSTextureOGL* rt = NULL);
	void OMAttachDs(GSTextureOGL* ds = NULL);
	void OMSetFBO(GLuint fbo);

	public:
	GSShaderOGL* m_shader;

	GSDeviceOGL();
	virtual ~GSDeviceOGL();

	static void CheckDebugLog();
	// Used by OpenGL, so the same calling convention is required.
	static void APIENTRY DebugOutputToFile(GLenum gl_source, GLenum gl_type, GLuint id, GLenum gl_severity, GLsizei gl_length, const GLchar *gl_message, const void* userParam);

	bool HasStencil() { return true; }
	bool HasDepth32() { return true; }

	bool Create(GSWnd* wnd);
	bool Reset(int w, int h);
	void Flip();
	void SetVSync(bool enable);

	void DrawPrimitive() final;
	void DrawPrimitive(int offset, int count);
	void DrawIndexedPrimitive() final;
	void DrawIndexedPrimitive(int offset, int count) final;
	inline void BeforeDraw();
	inline void AfterDraw();

	void ClearRenderTarget(GSTexture* t, const GSVector4& c) final;
	void ClearRenderTarget(GSTexture* t, uint32 c) final;
	void ClearRenderTarget_i(GSTexture* t, int32 c);
	void ClearDepth(GSTexture* t, float c) final;
	void ClearStencil(GSTexture* t, uint8 c) final;

	GSTexture* CreateRenderTarget(int w, int h, bool msaa, int format = 0) final;
	GSTexture* CreateDepthStencil(int w, int h, bool msaa, int format = 0) final;
	GSTexture* CreateTexture(int w, int h, int format = 0) final;
	GSTexture* CreateOffscreen(int w, int h, int format = 0) final;
	void InitPrimDateTexture(GSTexture* rt);
	void RecycleDateTexture();

	GSTexture* CopyOffscreen(GSTexture* src, const GSVector4& sRect, int w, int h, int format = 0, int ps_shader = 0) final;

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r) final;
	void CopyRectConv(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, bool at_origin);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, int shader = 0, bool linear = true) final;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, bool linear = true);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, int bs, bool linear = true);

	void SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm);

	void BeginScene() final {}
	void EndScene() final;

	void IASetPrimitiveTopology(GLenum topology);
	void IASetVertexBuffer(const void* vertices, size_t count);
	void IASetIndexBuffer(const void* index, size_t count);

	void PSSetShaderResource(int i, GSTexture* sr) final;
	void PSSetShaderResources(GSTexture* sr0, GSTexture* sr1) final;
	void PSSetSamplerState(GLuint ss);

	void OMSetDepthStencilState(GSDepthStencilOGL* dss);
	void OMSetBlendState(uint8 blend_index = 0, uint8 blend_factor = 0, bool is_blend_constant = false);
	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor = NULL) final;
	void OMSetColorMaskState(OMColorMaskSelector sel = OMColorMaskSelector());


	void CreateTextureFX();
	GLuint CompileVS(VSSelector sel);
	GLuint CompileGS(GSSelector sel);
	GLuint CompilePS(PSSelector sel);
	GLuint CreateSampler(bool bilinear, bool tau, bool tav, bool aniso = false);
	GLuint CreateSampler(PSSamplerSelector sel);
	GSDepthStencilOGL* CreateDepthStencil(OMDepthStencilSelector dssel);

	void SelfShaderTest();


	void SetupIA(const void* vertex, int vertex_count, const uint32* index, int index_count, int prim);
	void SetupPipeline(const VSSelector& vsel, const GSSelector& gsel, const PSSelector& psel);
	void SetupCB(const VSConstantBuffer* vs_cb, const PSConstantBuffer* ps_cb);
	void SetupSampler(PSSamplerSelector ssel);
	void SetupOM(OMDepthStencilSelector dssel);
	GLuint GetSamplerID(PSSamplerSelector ssel);
	GLuint GetPaletteSamplerID();

	void Barrier(GLbitfield b);
};
