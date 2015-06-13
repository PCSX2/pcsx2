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
#define A_MAX	(0x100)	 // Impossible blending uses coeff bigger than 1
#define C_CLR	(0x200)	 // Clear color blending (use directly the destination color as blending factor)
#define NO_BAR  (0x400)  // don't require texture barrier for the blending (because the RT is not used)

#ifdef ENABLE_OGL_DEBUG_MEM_BW
extern uint64 g_real_texture_upload_byte;
extern uint64 g_vertex_upload_byte;
#endif

class GSBlendStateOGL {
	// Note: You can also select the index of the draw buffer for which to set the blend setting
	// We will keep basic the first try
	bool   m_enable;
	GLenum m_equation_RGB;
	GLenum m_func_sRGB;
	GLenum m_func_dRGB;
	bool   m_constant_factor;

public:

	GSBlendStateOGL() : m_enable(false)
		, m_equation_RGB(0)
		, m_func_sRGB(0)
		, m_func_dRGB(0)
		, m_constant_factor(false)
	{}

	void SetRGB(GLenum op, GLenum src, GLenum dst)
	{
		m_equation_RGB = op;
		m_func_sRGB = src;
		m_func_dRGB = dst;
		if (IsConstant(src) || IsConstant(dst)) m_constant_factor = true;
	}

	void RevertOp()
	{
		if(m_equation_RGB == GL_FUNC_ADD)
			m_equation_RGB = GL_FUNC_REVERSE_SUBTRACT;
		else if(m_equation_RGB == GL_FUNC_REVERSE_SUBTRACT)
			m_equation_RGB = GL_FUNC_ADD;
	}

	void EnableBlend() { m_enable = true;}

	bool IsConstant(GLenum factor) { return ((factor == GL_CONSTANT_COLOR) || (factor == GL_ONE_MINUS_CONSTANT_COLOR)); }

	bool HasConstantFactor() { return m_constant_factor; }

	void SetupBlend(float factor)
	{
		if (GLState::blend != m_enable) {
			GLState::blend = m_enable;
			if (m_enable)
				glEnable(GL_BLEND);
			else
				glDisable(GL_BLEND);
		}

		if (m_enable) {
			if (HasConstantFactor()) {
				if (GLState::bf != factor) {
					GLState::bf = factor;
					gl_BlendColor(factor, factor, factor, 0);
				}
			}

			if (GLState::eq_RGB != m_equation_RGB) {
				GLState::eq_RGB = m_equation_RGB;
				if (gl_BlendEquationSeparateiARB)
					gl_BlendEquationSeparateiARB(0, m_equation_RGB, GL_FUNC_ADD);
				else
					gl_BlendEquationSeparate(m_equation_RGB, GL_FUNC_ADD);
			}
			if (GLState::f_sRGB != m_func_sRGB || GLState::f_dRGB != m_func_dRGB) {
				GLState::f_sRGB = m_func_sRGB;
				GLState::f_dRGB = m_func_dRGB;
				if (gl_BlendFuncSeparateiARB)
					gl_BlendFuncSeparateiARB(0, m_func_sRGB, m_func_dRGB, GL_ONE, GL_ZERO);
				else
					gl_BlendFuncSeparate(m_func_sRGB, m_func_dRGB, GL_ONE, GL_ZERO);
			}
		}
	}
};

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

class GSDeviceOGL : public GSDevice
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
				// Next param will be handle by subroutine
				uint32 tme:1;
				uint32 fst:1;

				uint32 _free:27;
			};

			uint32 key;
		};

		// FIXME is the & useful ?
		operator uint32() {return key & 0x3f;}

		VSSelector() : key(0) {}
		VSSelector(uint32 k) : key(k) {}

		static uint32 size() { return 1 << 5; }
	};

	__aligned(struct, 32) PSConstantBuffer
	{
		GSVector4 FogColor_AREF;
		GSVector4 WH;
		GSVector4 MinF_TA;
		GSVector4i MskFix;
		GSVector4 AlphaCoeff;

		GSVector4 HalfTexel;
		GSVector4 MinMax;
		GSVector4 TC_OffsetHack;

		PSConstantBuffer()
		{
			FogColor_AREF = GSVector4::zero();
			HalfTexel = GSVector4::zero();
			WH = GSVector4::zero();
			MinMax = GSVector4::zero();
			MinF_TA = GSVector4::zero();
			MskFix = GSVector4i::zero();
			AlphaCoeff = GSVector4::zero();
			TC_OffsetHack = GSVector4::zero();
		}

		__forceinline bool Update(const PSConstantBuffer* cb)
		{
			GSVector4i* a = (GSVector4i*)this;
			GSVector4i* b = (GSVector4i*)cb;

			// if WH matches both HalfTexel and TC_OffsetHack do too
			// MinMax depends on WH and MskFix so no need to check it too
			if(!((a[0] == b[0]) & (a[1] == b[1]) & (a[2] == b[2]) & (a[3] == b[3]) & (a[4] == b[4])).alltrue())
			{
				// Note previous check uses SSE already, a plain copy will be faster than any memcpy
				a[0] = b[0];
				a[1] = b[1];
				a[2] = b[2];
				a[3] = b[3];
				a[4] = b[4];

				return true;
			}

			return false;
		}
	};

	struct PSSelector
	{
		union
		{
			struct
			{
				uint32 fst:1;
				uint32 fmt:3;
				uint32 aem:1;
				uint32 fog:1;
				uint32 clr1:1;
				uint32 fba:1;
				uint32 aout:1;
				uint32 date:3;
				uint32 tcoffsethack:1;
				//uint32 point_sampler:1; Not tested, so keep the bit for blend
				uint32 iip:1;
				// Next param will be handle by subroutine (broken currently)
				uint32 colclip:2;
				uint32 atst:3;

				uint32 tfx:3;
				uint32 tcc:1;
				uint32 wms:2;
				uint32 wmt:2;
				uint32 ltf:1;
				uint32 ifmt:2;
				uint32 shuffle:1;
				uint32 read_ba:1;

				//uint32 _free1:0;

				// Word 2
				uint32 blend:8;
				uint32 dfmt:2;

				uint32 _free2:22;
			};

			uint64 key;
		};

		// FIXME is the & useful ?
		operator uint64() {return key;}

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

				uint32 _free:29;
			};

			uint32 key;
		};

		// FIXME is the & useful ?
		operator uint32() {return key & 0x7;}

		PSSamplerSelector() : key(0) {}
		PSSamplerSelector(uint32 k) : key(k) {}

		static uint32 size() { return 1 << 3; }
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
				uint32 alpha_stencil:1;

				uint32 _free:27;
			};

			uint32 key;
		};

		// FIXME is the & useful ?
		operator uint32() {return key & 0x1f;}

		OMDepthStencilSelector() : key(0) {}
		OMDepthStencilSelector(uint32 k) : key(k) {}

		static uint32 size() { return 1 << 5; }
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

	struct OMBlendSelector
	{
		union
		{
			struct
			{
				uint32 abe:1;
				uint32 a:2;
				uint32 b:2;
				uint32 c:2;
				uint32 d:2;
				uint32 negative:1;

				uint32 _free:22;
			};

			struct
			{
				uint32 _abe:1;
				uint32 abcd:8;
				uint32 _negative:1;

				uint32 _free2:22;
			};

			uint32 key;
		};

		// FIXME is the & useful ?
		operator uint32() {return key & 0x3ff;}

		OMBlendSelector() : key(0) {}

		bool IsCLR1() const
		{
			return (key & 0x19f) == 0x93; // abe == 1 && a == 1 && b == 2 && d == 1
		}
	};

	struct D3D9Blend {int bogus, op, src, dst;};
	static const D3D9Blend m_blendMapD3D9[3*3*3*3];

	static int s_n;

	private:
	uint32 m_msaa;				// Level of Msaa

	static bool m_debug_gl_call;
	static FILE* m_debug_gl_file;

	bool m_free_window;			
	GSWnd* m_window;

	GLuint m_fbo;				// frame buffer container
	GLuint m_fbo_read;			// frame buffer container only for reading

	GSVertexBufferStateOGL* m_va;// state of the vertex buffer/array

	struct {
		GLuint ps[2];				 // program object
		GSUniformBufferOGL* cb;		 // uniform buffer object
		GSBlendStateOGL* bs;
	} m_merge_obj;

	struct {
		GLuint ps[4];				// program object
		GSUniformBufferOGL* cb;		// uniform buffer object
	} m_interlace;

	struct {
		GLuint vs;		// program object
		GLuint ps[14];	// program object
		GLuint ln;		// sampler object
		GLuint pt;		// sampler object
		GSDepthStencilOGL* dss;
		GSDepthStencilOGL* dss_write;
		GSBlendStateOGL* bs;
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
		GSBlendStateOGL* bs;
		GSTexture* t;
	} m_date;

	struct {
		GLuint ps;
		GSUniformBufferOGL *cb;
	} m_shadeboost;

	struct {
		GSDepthStencilOGL* dss;
		GSBlendStateOGL* bs;
		float bf; // blend factor
	} m_state;

	GLuint m_vs[1<<6];
	GLuint m_gs;
	GLuint m_ps_ss[1<<3];
	GSDepthStencilOGL* m_om_dss[1<<6];
	hash_map<uint64, GLuint > m_ps;
	hash_map<uint32, GSBlendStateOGL* > m_om_bs;
	GLuint m_apitrace;

	GLuint m_palette_ss;
	GLuint m_rt_ss;

	GSUniformBufferOGL* m_vs_cb;
	GSUniformBufferOGL* m_ps_cb;

	VSConstantBuffer m_vs_cb_cache;
	PSConstantBuffer m_ps_cb_cache;

	GSTexture* CreateSurface(int type, int w, int h, bool msaa, int format);
	GSTexture* FetchSurface(int type, int w, int h, bool msaa, int format);

	void DoMerge(GSTexture* sTex[2], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, bool slbg, bool mmod, const GSVector4& c);
	void DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset = 0);
	void DoFXAA(GSTexture* sTex, GSTexture* dTex);
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex);
	void DoExternalFX(GSTexture* sTex, GSTexture* dTex);

	void OMAttachRt(GSTextureOGL* rt = NULL);
	void OMAttachDs(GSTextureOGL* ds = NULL);
	void OMSetFBO(GLuint fbo);

	public:
	GSShaderOGL* m_shader;

	GSDeviceOGL();
	virtual ~GSDeviceOGL();

	static void CheckDebugLog();
	static void DebugOutputToFile(GLenum gl_source, GLenum gl_type, GLuint id, GLenum gl_severity, GLsizei gl_length, const GLchar *gl_message, const void* userParam);

	bool HasStencil() { return true; }
	bool HasDepth32() { return true; }

	bool Create(GSWnd* wnd);
	bool Reset(int w, int h);
	void Flip();
	void SetVSync(bool enable);

	void DrawPrimitive();
	void DrawIndexedPrimitive();
	void DrawIndexedPrimitive(int offset, int count);
	void BeforeDraw();
	void AfterDraw();

	void ClearRenderTarget(GSTexture* t, const GSVector4& c);
	void ClearRenderTarget(GSTexture* t, uint32 c);
	void ClearRenderTarget_i(GSTexture* t, int32 c);
	void ClearDepth(GSTexture* t, float c);
	void ClearStencil(GSTexture* t, uint8 c);

	GSTexture* CreateRenderTarget(int w, int h, bool msaa, int format = 0);
	GSTexture* CreateDepthStencil(int w, int h, bool msaa, int format = 0);
	GSTexture* CreateTexture(int w, int h, int format = 0);
	GSTexture* CreateOffscreen(int w, int h, int format = 0);
	void InitPrimDateTexture(GSTexture* rt);
	void RecycleDateTexture();

	GSTexture* CopyOffscreen(GSTexture* src, const GSVector4& sRect, int w, int h, int format = 0, int ps_shader = 0);

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, int shader = 0, bool linear = true);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, bool linear = true);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, GSBlendStateOGL* bs, bool linear = true);

	void SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm);

	void EndScene();

	void IASetPrimitiveTopology(GLenum topology);
	void IASetVertexBuffer(const void* vertices, size_t count);
	void IASetIndexBuffer(const void* index, size_t count);

	void PSSetShaderResource(int i, GSTexture* sr);
	void PSSetShaderResources(GSTexture* sr0, GSTexture* sr1);
	void PSSetSamplerState(GLuint ss);

	void OMSetDepthStencilState(GSDepthStencilOGL* dss, uint8 sref);
	void OMSetBlendState(GSBlendStateOGL* bs, float bf);
	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor = NULL);
	void OMSetWriteBuffer(GLenum buffer = GL_COLOR_ATTACHMENT0);
	void OMSetColorMaskState(OMColorMaskSelector sel = OMColorMaskSelector());


	void CreateTextureFX();
	GLuint CompileVS(VSSelector sel, int logz);
	GLuint CompileGS();
	GLuint CompilePS(PSSelector sel);
	GLuint CreateSampler(bool bilinear, bool tau, bool tav);
	GLuint CreateSampler(PSSamplerSelector sel);
	GSDepthStencilOGL* CreateDepthStencil(OMDepthStencilSelector dssel);
	GSBlendStateOGL* CreateBlend(OMBlendSelector bsel, float afix);


	void SetupIA(const void* vertex, int vertex_count, const uint32* index, int index_count, int prim);
	void SetupVS(VSSelector sel);
	void SetupGS(bool enable);
	void SetupPS(PSSelector sel);
	void SetupCB(const VSConstantBuffer* vs_cb, const PSConstantBuffer* ps_cb);
	void SetupSampler(PSSamplerSelector ssel);
	void SetupOM(OMDepthStencilSelector dssel, OMBlendSelector bsel, float afix, bool sw_blending =  false);
	GLuint GetSamplerID(PSSamplerSelector ssel);
	GLuint GetPaletteSamplerID();

	void Barrier(GLbitfield b);
};
