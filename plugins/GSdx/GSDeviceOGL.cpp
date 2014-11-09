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

#include "stdafx.h"
#include "GSDeviceOGL.h"
#include "GLState.h"
#include <fstream>

#include "res/glsl_source.h"

//#define LOUD_DEBUGGING
//#define PRINT_FRAME_NUMBER
//#define ONLY_LINES

static uint32 g_draw_count = 0;
static uint32 g_frame_count = 1;
#ifdef ENABLE_OGL_DEBUG_MEM_BW
uint32 g_texture_upload_byte = 0;
uint32 g_vertex_upload_byte = 0;
#endif

static const uint32 g_merge_cb_index      = 10;
static const uint32 g_interlace_cb_index  = 11;
static const uint32 g_shadeboost_cb_index = 12;
static const uint32 g_fxaa_cb_index       = 13;
static const uint32 g_fx_cb_index         = 14;

GSDeviceOGL::GSDeviceOGL()
	: m_free_window(false)
	  , m_window(NULL)
	  , m_fbo(0)
	  , m_fbo_read(0)
	  , m_va(NULL)
	  , m_shader(NULL)
{
	memset(&m_merge_obj, 0, sizeof(m_merge_obj));
	memset(&m_interlace, 0, sizeof(m_interlace));
	memset(&m_convert, 0, sizeof(m_convert));
	memset(&m_fxaa, 0, sizeof(m_fxaa));
	memset(&m_shaderfx, 0, sizeof(m_shaderfx));
	memset(&m_date, 0, sizeof(m_date));
	memset(&m_state, 0, sizeof(m_state));
	GLState::Clear();

	// Reset the debug file
	#ifdef ENABLE_OGL_DEBUG
	FILE* f = fopen("Debug.txt","w");
	fclose(f);
	#endif
}

GSDeviceOGL::~GSDeviceOGL()
{
	// If the create function wasn't called nothing to do.
	if (m_shader == NULL)
		return;

	// Clean vertex buffer state
	delete (m_va);

	// Clean m_merge_obj
	for (size_t i = 0; i < countof(m_merge_obj.ps); i++)
		m_shader->Delete(m_merge_obj.ps[i]);
	delete (m_merge_obj.cb);
	delete (m_merge_obj.bs);

	// Clean m_interlace
	for (size_t i = 0; i < countof(m_interlace.ps); i++)
		m_shader->Delete(m_interlace.ps[i]);
	delete (m_interlace.cb);

	// Clean m_convert
	m_shader->Delete(m_convert.vs);
	for (size_t i = 0; i < countof(m_convert.ps); i++)
		m_shader->Delete(m_convert.ps[i]);
	delete m_convert.dss;
	delete m_convert.bs;

	// Clean m_fxaa
	delete m_fxaa.cb;
	m_shader->Delete(m_fxaa.ps);

	// Clean m_shaderfx
	delete m_shaderfx.cb;
	m_shader->Delete(m_shaderfx.ps);

	// Clean m_date
	delete m_date.dss;
	delete m_date.bs;

	// Clean shadeboost
	delete m_shadeboost.cb;
	m_shader->Delete(m_shadeboost.ps);


	// Clean various opengl allocation
	gl_DeleteFramebuffers(1, &m_fbo);
	gl_DeleteFramebuffers(1, &m_fbo_read);

	// Delete HW FX
	delete m_vs_cb;
	delete m_ps_cb;
	gl_DeleteSamplers(1, &m_palette_ss);
	m_shader->Delete(m_apitrace);

	for (uint32 key = 0; key < VSSelector::size(); key++) m_shader->Delete(m_vs[key]);
	m_shader->Delete(m_gs);
	for (auto it = m_ps.begin(); it != m_ps.end() ; it++) m_shader->Delete(it->second);

	m_ps.clear();

	gl_DeleteSamplers(PSSamplerSelector::size(), m_ps_ss);

	for (uint32 key = 0; key < OMDepthStencilSelector::size(); key++) delete m_om_dss[key];

	for (auto it = m_om_bs.begin(); it != m_om_bs.end(); it++) delete it->second;
	m_om_bs.clear();

	PboPool::Destroy();

	// Must be done after the destruction of all shader/program objects
	delete m_shader;
	m_shader = NULL;
}

GSTexture* GSDeviceOGL::CreateSurface(int type, int w, int h, bool msaa, int format)
{
	// A wrapper to call GSTextureOGL, with the different kind of parameter
	GSTextureOGL* t = NULL;
	t = new GSTextureOGL(type, w, h, format, m_fbo_read);

	switch(type)
	{
		case GSTexture::RenderTarget:
			ClearRenderTarget(t, 0);
			break;
		case GSTexture::DepthStencil:
			ClearDepth(t, 0);
			// No need to clear the stencil now.
			break;
	}
	return t;
}

GSTexture* GSDeviceOGL::FetchSurface(int type, int w, int h, bool msaa, int format)
{
	return GSDevice::FetchSurface(type, w, h, false, format);
}

bool GSDeviceOGL::Create(GSWnd* wnd)
{
	if (m_window == NULL) {
		if (!GLLoader::check_gl_version(3, 3)) return false;

		if (!GLLoader::check_gl_supported_extension()) return false;
	}

	m_window = wnd;

	// ****************************************************************
	// Debug helper
	// ****************************************************************
#ifndef ENABLE_GLES
#ifdef ENABLE_OGL_DEBUG
	gl_DebugMessageCallback((GLDEBUGPROC)DebugOutputToFile, NULL);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
#endif
#endif

	// ****************************************************************
	// Various object
	// ****************************************************************
	m_shader = new GSShaderOGL(!!theApp.GetConfig("debug_glsl_shader", 0));

	gl_GenFramebuffers(1, &m_fbo);
	gl_GenFramebuffers(1, &m_fbo_read);

	// ****************************************************************
	// Vertex buffer state
	// ****************************************************************
	ASSERT(sizeof(GSVertexPT1) == sizeof(GSVertex));
	GSInputLayoutOGL il_convert[] =
	{
		{2 , GL_FLOAT          , GL_FALSE , sizeof(GSVertexPT1) , (const GLvoid*)(0) }  ,
		{2 , GL_FLOAT          , GL_FALSE , sizeof(GSVertexPT1) , (const GLvoid*)(16) } ,
		{4 , GL_UNSIGNED_BYTE  , GL_TRUE  , sizeof(GSVertex)    , (const GLvoid*)(8) }  ,
		{1 , GL_FLOAT          , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(12) } ,
		{2 , GL_UNSIGNED_SHORT , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(16) } ,
		{1 , GL_UNSIGNED_INT   , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(20) } ,
		{2 , GL_UNSIGNED_SHORT , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(24) } ,
		{4 , GL_UNSIGNED_BYTE  , GL_TRUE  , sizeof(GSVertex)    , (const GLvoid*)(28) } ,
	};
	m_va = new GSVertexBufferStateOGL(sizeof(GSVertexPT1), il_convert, countof(il_convert));

	// ****************************************************************
	// Texture unit state
	// ****************************************************************
	// By default use unit 3 for texture modification
	// unit 0-2 will allocated to shader input
	gl_ActiveTexture(GL_TEXTURE0 + 3);

	// ****************************************************************
	// Pre Generate the different sampler object
	// ****************************************************************
	for (uint32 key = 0; key < PSSamplerSelector::size(); key++)
		m_ps_ss[key] = CreateSampler(PSSamplerSelector(key));

	// ****************************************************************
	// convert
	// ****************************************************************
	m_convert.vs = m_shader->Compile("convert.glsl", "vs_main", GL_VERTEX_SHADER, convert_glsl);
	for(size_t i = 0; i < countof(m_convert.ps); i++)
		m_convert.ps[i] = m_shader->Compile("convert.glsl", format("ps_main%d", i), GL_FRAGMENT_SHADER, convert_glsl);

	// Note the following object are initialized to 0 so disabled.
	// Note: maybe enable blend with a factor of 1
	// m_convert.dss, m_convert.bs

	PSSamplerSelector point;
	m_convert.pt = GetSamplerID(point);

	PSSamplerSelector bilinear;
	bilinear.ltf = true;
	m_convert.ln = GetSamplerID(bilinear);

	m_convert.dss = new GSDepthStencilOGL();
	m_convert.bs  = new GSBlendStateOGL();

	// ****************************************************************
	// merge
	// ****************************************************************
	m_merge_obj.cb = new GSUniformBufferOGL(g_merge_cb_index, sizeof(MergeConstantBuffer));

	for(size_t i = 0; i < countof(m_merge_obj.ps); i++)
		m_merge_obj.ps[i] = m_shader->Compile("merge.glsl", format("ps_main%d", i), GL_FRAGMENT_SHADER, merge_glsl);

	m_merge_obj.bs = new GSBlendStateOGL();
	m_merge_obj.bs->EnableBlend();
	m_merge_obj.bs->SetRGB(GL_FUNC_ADD, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// ****************************************************************
	// interlace
	// ****************************************************************
	m_interlace.cb = new GSUniformBufferOGL(g_interlace_cb_index, sizeof(InterlaceConstantBuffer));

	for(size_t i = 0; i < countof(m_interlace.ps); i++)
		m_interlace.ps[i] = m_shader->Compile("interlace.glsl", format("ps_main%d", i), GL_FRAGMENT_SHADER, interlace_glsl);
	// ****************************************************************
	// Shade boost
	// ****************************************************************
	m_shadeboost.cb = new GSUniformBufferOGL(g_shadeboost_cb_index, sizeof(ShadeBoostConstantBuffer));

	int ShadeBoost_Contrast = theApp.GetConfig("ShadeBoost_Contrast", 50);
	int ShadeBoost_Brightness = theApp.GetConfig("ShadeBoost_Brightness", 50);
	int ShadeBoost_Saturation = theApp.GetConfig("ShadeBoost_Saturation", 50);
	std::string shade_macro = format("#define SB_SATURATION %d.0\n", ShadeBoost_Saturation)
		+ format("#define SB_BRIGHTNESS %d.0\n", ShadeBoost_Brightness)
		+ format("#define SB_CONTRAST %d.0\n", ShadeBoost_Contrast);

	m_shadeboost.ps = m_shader->Compile("shadeboost.glsl", "ps_main", GL_FRAGMENT_SHADER, shadeboost_glsl, shade_macro);

	// ****************************************************************
	// rasterization configuration
	// ****************************************************************
#ifndef ENABLE_GLES
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
	glDisable(GL_CULL_FACE);
	glEnable(GL_SCISSOR_TEST);
	// FIXME enable it when multisample code will be here
	// DX: rd.MultisampleEnable = true;
#ifndef ENABLE_GLES
	glDisable(GL_MULTISAMPLE);
#endif
#ifdef ONLY_LINES
	glLineWidth(5.0);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif
	// Hum I don't know for those options but let's hope there are not activated
#if 0
	rd.FrontCounterClockwise = false;
	rd.DepthBias = false;
	rd.DepthBiasClamp = 0;
	rd.SlopeScaledDepthBias = 0;
	rd.DepthClipEnable = false; // ???
	rd.AntialiasedLineEnable = false;
#endif

	// ****************************************************************
	// DATE
	// ****************************************************************

	m_date.dss = new GSDepthStencilOGL();
	m_date.dss->EnableStencil();
	m_date.dss->SetStencil(GL_ALWAYS, GL_REPLACE);

	m_date.bs = new GSBlendStateOGL();
	// FIXME impact image load?
//#ifndef ENABLE_OGL_STENCIL_DEBUG
//	// Only keep stencil data
//	m_date.bs->SetMask(false, false, false, false);
//#endif

	// ****************************************************************
	// Use DX coordinate convention
	// ****************************************************************


	// VS gl_position.z => [-1,-1]
	// FS depth => [0, 1]
	// because of -1 we loose lot of precision for small GS value
	// This extension allow FS depth to range from -1 to 1. So
	// gl_position.z could range from [0, 1]
#ifndef ENABLE_GLES
	if (GLLoader::found_GL_ARB_clip_control) {
		// Change depth convention
		gl_ClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
	}
#endif

	// ****************************************************************
	// HW renderer shader
	// ****************************************************************
	CreateTextureFX();

	// ****************************************************************
	// Pbo Pool allocation
	// ****************************************************************
	PboPool::Init();

	// ****************************************************************
	// Finish window setup and backbuffer
	// ****************************************************************
	if(!GSDevice::Create(wnd))
		return false;

	GSVector4i rect = wnd->GetClientRect();
	Reset(rect.z, rect.w);

	return true;
}

bool GSDeviceOGL::Reset(int w, int h)
{
	if(!GSDevice::Reset(w, h))
		return false;

	// TODO
	// Opengl allocate the backbuffer with the window. The render is done in the backbuffer when
	// there isn't any FBO. Only a dummy texture is created to easily detect when the rendering is done
	// in the backbuffer
	m_backbuffer = new GSTextureOGL(GSTextureOGL::Backbuffer, w, h, 0, m_fbo_read);

	return true;
}

void GSDeviceOGL::SetVSync(bool enable)
{
	m_wnd->SetVSync(enable);
}

void GSDeviceOGL::Flip()
{
	// FIXME: disable it when code is working
	#ifdef ENABLE_OGL_DEBUG
	CheckDebugLog();
	#endif

	m_wnd->Flip();

#ifdef PRINT_FRAME_NUMBER
	fprintf(stderr, "Draw %d (Frame %d)\n", g_draw_count, g_frame_count);
#endif
#if defined(ENABLE_OGL_DEBUG) || defined(PRINT_FRAME_NUMBER)
	g_frame_count++;
#endif
}

void GSDeviceOGL::AttachContext()
{
	if (m_window)
		m_window->AttachContext();
}

void GSDeviceOGL::DetachContext()
{
	if (m_window)
		m_window->DetachContext();
}

void GSDeviceOGL::BeforeDraw()
{
	m_shader->UseProgram();

#ifdef _DEBUG
	ASSERT(gl_CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
#endif

//#ifdef ENABLE_OGL_STENCIL_DEBUG
//	if (m_date.t)
//		static_cast<GSTextureOGL*>(m_date.t)->Save(format("/tmp/date_before_%04ld.csv", g_draw_count));
//#endif
}

void GSDeviceOGL::AfterDraw()
{
//#ifdef ENABLE_OGL_STENCIL_DEBUG
//	if (m_date.t)
//		static_cast<GSTextureOGL*>(m_date.t)->Save(format("/tmp/date_after_%04ld.csv", g_draw_count));
//#endif
#if defined(ENABLE_OGL_DEBUG) || defined(PRINT_FRAME_NUMBER) || defined(ENABLE_OGL_STENCIL_DEBUG)
	g_draw_count++;
#endif
}

void GSDeviceOGL::DrawPrimitive()
{
	BeforeDraw();
	m_va->DrawPrimitive();
	AfterDraw();
}

void GSDeviceOGL::DrawIndexedPrimitive()
{
	BeforeDraw();
	m_va->DrawIndexedPrimitive();
	AfterDraw();
}

void GSDeviceOGL::DrawIndexedPrimitive(int offset, int count)
{
	ASSERT(offset + count <= (int)m_index.count);

	BeforeDraw();
	m_va->DrawIndexedPrimitive(offset, count);
	AfterDraw();
}

void GSDeviceOGL::ClearRenderTarget(GSTexture* t, const GSVector4& c)
{
	if (GLLoader::found_GL_ARB_clear_texture) {
		if (static_cast<GSTextureOGL*>(t)->IsBackbuffer()) {
			glDisable(GL_SCISSOR_TEST);
			OMSetFBO(0);
			// glDrawBuffer(GL_BACK); // this is the default when there is no FB
			// 0 will select the first drawbuffer ie GL_BACK
			gl_ClearBufferfv(GL_COLOR, 0, c.v);
			glEnable(GL_SCISSOR_TEST);
		} else {
			static_cast<GSTextureOGL*>(t)->Clear((const void*)&c);
		}
	} else {
		glDisable(GL_SCISSOR_TEST);
		if (static_cast<GSTextureOGL*>(t)->IsBackbuffer()) {
			OMSetFBO(0);

			// glDrawBuffer(GL_BACK); // this is the default when there is no FB
			// 0 will select the first drawbuffer ie GL_BACK
			gl_ClearBufferfv(GL_COLOR, 0, c.v);
		} else {
			OMSetFBO(m_fbo);
			OMAttachRt(static_cast<GSTextureOGL*>(t)->GetID());

			gl_ClearBufferfv(GL_COLOR, 0, c.v);
		}
		glEnable(GL_SCISSOR_TEST);
	}
}

void GSDeviceOGL::ClearRenderTarget(GSTexture* t, uint32 c)
{
	GSVector4 color = GSVector4::rgba32(c) * (1.0f / 255);
	ClearRenderTarget(t, color);
}

void GSDeviceOGL::ClearRenderTarget_ui(GSTexture* t, uint32 c)
{
	if (GLLoader::found_GL_ARB_clear_texture) {
		static_cast<GSTextureOGL*>(t)->Clear((const void*)&c);
	} else {
		uint32 col[4] = {c, c, c, c};

		glDisable(GL_SCISSOR_TEST);

		OMSetFBO(m_fbo);
		OMAttachRt(static_cast<GSTextureOGL*>(t)->GetID());

		gl_ClearBufferuiv(GL_COLOR, 0, col);

		glEnable(GL_SCISSOR_TEST);
	}
}

void GSDeviceOGL::ClearDepth(GSTexture* t, float c)
{
	// TODO is it possible with GL44 ClearTexture? no the API is garbage!
	// It can't be used here because it will clear both depth and stencil
	if (0 && GLLoader::found_GL_ARB_clear_texture) {
#ifndef ENABLE_GLES
		ASSERT(c == 0.0f);
		gl_ClearTexImage(static_cast<GSTextureOGL*>(t)->GetID(), GL_TEX_LEVEL_0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NULL);
#endif
	} else {
		OMSetFBO(m_fbo);
		OMAttachDs(static_cast<GSTextureOGL*>(t)->GetID());

		glDisable(GL_SCISSOR_TEST);
		if (GLState::depth_mask) {
			gl_ClearBufferfv(GL_DEPTH, 0, &c);
		} else {
			glDepthMask(true);
			gl_ClearBufferfv(GL_DEPTH, 0, &c);
			glDepthMask(false);
		}
		glEnable(GL_SCISSOR_TEST);
	}
}

void GSDeviceOGL::ClearStencil(GSTexture* t, uint8 c)
{
	// TODO is it possible with GL44 ClearTexture? no the API is garbage!
	// It can't be used here because it will clear both depth and stencil
	if (0 && GLLoader::found_GL_ARB_clear_texture) {
#ifndef ENABLE_GLES
		ASSERT(c == 0);
		gl_ClearTexImage(static_cast<GSTextureOGL*>(t)->GetID(), GL_TEX_LEVEL_0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NULL);
#endif
	} else {
		OMSetFBO(m_fbo);
		OMAttachDs(static_cast<GSTextureOGL*>(t)->GetID());
		GLint color = c;

		glDisable(GL_SCISSOR_TEST);
		gl_ClearBufferiv(GL_STENCIL, 0, &color);
		glEnable(GL_SCISSOR_TEST);
	}
}

GLuint GSDeviceOGL::CreateSampler(PSSamplerSelector sel)
{
	return CreateSampler(sel.ltf, sel.tau, sel.tav);
}

GLuint GSDeviceOGL::CreateSampler(bool bilinear, bool tau, bool tav)
{
	GLuint sampler;
	gl_GenSamplers(1, &sampler);
	if (bilinear) {
		gl_SamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		gl_SamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		gl_SamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		gl_SamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	// FIXME ensure U -> S,  V -> T and W->R
	if (tau)
		gl_SamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	else
		gl_SamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	if (tav)
		gl_SamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
	else
		gl_SamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	gl_SamplerParameteri(sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	// FIXME which value for GL_TEXTURE_MIN_LOD
	gl_SamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, FLT_MAX);

	// FIXME: seems there is 2 possibility in opengl
	// DX: sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	// gl_SamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
#if 0
	// Message:Program undefined behavior warning: Sampler object 5 has depth compare enabled. It is being used with non-depth texture 7, by a program that samples it with a regular sampler. This is undefined behavior.
	gl_SamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	gl_SamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, GL_NEVER);
#endif
	// FIXME: need ogl extension sd.MaxAnisotropy = 16;

	return sampler;
}

void GSDeviceOGL::InitPrimDateTexture(int w, int h)
{
	// Create a texture to avoid the useless clean@0
	if (m_date.t == NULL)
		m_date.t = CreateTexture(w, h, GL_R32I);

	ClearRenderTarget_ui(m_date.t, 0x0FFFFFFF);

#ifdef ENABLE_OGL_STENCIL_DEBUG
	gl_ActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_2D, static_cast<GSTextureOGL*>(m_date.t)->GetID());
	// Get back to the expected active texture unit
	gl_ActiveTexture(GL_TEXTURE0 + 3);
#endif

	BindDateTexture();
}

void GSDeviceOGL::BindDateTexture()
{
	// TODO: multibind?
	// GLuint textures[1] = {static_cast<GSTextureOGL*>(m_date.t)->GetID()};
	// gl_BindImageTextures(2, 1, textures);
	//gl_BindImageTexture(2, 0, 0, true, 0, GL_READ_WRITE, GL_R32I);

#ifndef ENABLE_GLES
	gl_BindImageTexture(2, static_cast<GSTextureOGL*>(m_date.t)->GetID(), 0, false, 0, GL_READ_WRITE, GL_R32I);
#endif
}

void GSDeviceOGL::RecycleDateTexture()
{
	if (m_date.t) {
#ifdef ENABLE_OGL_STENCIL_DEBUG
		//static_cast<GSTextureOGL*>(m_date.t)->Save(format("/tmp/date_adv_%04ld.csv", g_draw_count));
#endif

		// FIXME invalidate data
		Recycle(m_date.t);
		m_date.t = NULL;
	}
}

void GSDeviceOGL::Barrier(GLbitfield b)
{
#ifndef ENABLE_GLES
	gl_MemoryBarrier(b);
//#ifdef ENABLE_OGL_STENCIL_DEBUG
//	if (m_date.t)
//		static_cast<GSTextureOGL*>(m_date.t)->Save(format("/tmp/barrier_%04ld.csv", g_draw_count));
//#endif
#endif
}

/* Note: must be here because tfx_glsl is static */
GLuint GSDeviceOGL::CompileVS(VSSelector sel)
{
	std::string macro = format("#define VS_BPPZ %d\n", sel.bppz)
		+ format("#define VS_LOGZ %d\n", sel.logz)
		+ format("#define VS_TME %d\n", sel.tme)
		+ format("#define VS_FST %d\n", sel.fst)
		;

	return m_shader->Compile("tfx.glsl", "vs_main", GL_VERTEX_SHADER, tfx_glsl, macro);
}

/* Note: must be here because tfx_glsl is static */
GLuint GSDeviceOGL::CompileGS()
{
#ifdef ENABLE_GLES
	return 0;
#else
	return m_shader->Compile("tfx.glsl", "gs_main", GL_GEOMETRY_SHADER, tfx_glsl, "");
#endif
}

/* Note: must be here because tfx_glsl is static */
GLuint GSDeviceOGL::CompilePS(PSSelector sel)
{
	std::string macro = format("#define PS_FST %d\n", sel.fst)
		+ format("#define PS_WMS %d\n", sel.wms)
		+ format("#define PS_WMT %d\n", sel.wmt)
		+ format("#define PS_FMT %d\n", sel.fmt)
		+ format("#define PS_AEM %d\n", sel.aem)
		+ format("#define PS_TFX %d\n", sel.tfx)
		+ format("#define PS_TCC %d\n", sel.tcc)
		+ format("#define PS_ATST %d\n", sel.atst)
		+ format("#define PS_FOG %d\n", sel.fog)
		+ format("#define PS_CLR1 %d\n", sel.clr1)
		+ format("#define PS_FBA %d\n", sel.fba)
		+ format("#define PS_AOUT %d\n", sel.aout)
		+ format("#define PS_LTF %d\n", sel.ltf)
		+ format("#define PS_COLCLIP %d\n", sel.colclip)
		+ format("#define PS_DATE %d\n", sel.date)
		+ format("#define PS_SPRITEHACK %d\n", sel.spritehack)
		+ format("#define PS_TCOFFSETHACK %d\n", sel.tcoffsethack)
		+ format("#define PS_POINT_SAMPLER %d\n", sel.point_sampler)
		+ format("#define PS_IIP %d\n", sel.iip)
		;

	return m_shader->Compile("tfx.glsl", "ps_main", GL_FRAGMENT_SHADER, tfx_glsl, macro);
}

GSTexture* GSDeviceOGL::CreateRenderTarget(int w, int h, bool msaa, int format)
{
	return GSDevice::CreateRenderTarget(w, h, msaa, format ? format : GL_RGBA8);
}

GSTexture* GSDeviceOGL::CreateDepthStencil(int w, int h, bool msaa, int format)
{
	return GSDevice::CreateDepthStencil(w, h, msaa, format ? format : GL_DEPTH32F_STENCIL8);
}

GSTexture* GSDeviceOGL::CreateTexture(int w, int h, int format)
{
	return GSDevice::CreateTexture(w, h, format ? format : GL_RGBA8);
}

GSTexture* GSDeviceOGL::CreateOffscreen(int w, int h, int format)
{
	return GSDevice::CreateOffscreen(w, h, format ? format : GL_RGBA8);
}

// blit a texture into an offscreen buffer
GSTexture* GSDeviceOGL::CopyOffscreen(GSTexture* src, const GSVector4& sr, int w, int h, int format)
{
	ASSERT(src);
	ASSERT(format == GL_RGBA8 || format == GL_R16UI);

	if(format == 0) format = GL_RGBA8;

	if(format != GL_RGBA8 && format != GL_R16UI) return NULL;

	GSTexture* dst = CreateOffscreen(w, h, format);
	GSVector4 dr(0, 0, w, h);

	StretchRect(src, sr, dst, dr, m_convert.ps[format == GL_R16UI ? 1 : 0]);

	return dst;
}

// Copy a sub part of a texture into another
// Several question to answer did texture have same size?
// From a sub-part to the same sub-part
// From a sub-part to a full texture
void GSDeviceOGL::CopyRect(GSTexture* st, GSTexture* dt, const GSVector4i& r)
{
	ASSERT(st && dt);

	if (GLLoader::found_GL_ARB_copy_image) {
#ifndef ENABLE_GLES
		gl_CopyImageSubData( static_cast<GSTextureOGL*>(st)->GetID(), GL_TEXTURE_2D,
				0, r.x, r.y, 0,
				static_cast<GSTextureOGL*>(dt)->GetID(), GL_TEXTURE_2D,
				0, r.x, r.y, 0,
				r.width(), r.height(), 1);
#endif
	} else {

		GSTextureOGL* st_ogl = (GSTextureOGL*) st;
		GSTextureOGL* dt_ogl = (GSTextureOGL*) dt;

		gl_BindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);

		gl_FramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, static_cast<GSTextureOGL*>(st_ogl)->GetID(), 0);
		glReadBuffer(GL_COLOR_ATTACHMENT0);

		dt_ogl->EnableUnit();
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.x, r.y, r.width(), r.height());

		gl_BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}
}

void GSDeviceOGL::StretchRect(GSTexture* st, const GSVector4& sr, GSTexture* dt, const GSVector4& dr, int shader, bool linear)
{
	StretchRect(st, sr, dt, dr, m_convert.ps[shader], linear);
}

void GSDeviceOGL::StretchRect(GSTexture* st, const GSVector4& sr, GSTexture* dt, const GSVector4& dr, GLuint ps, bool linear)
{
	StretchRect(st, sr, dt, dr, ps, m_convert.bs, linear);
}

void GSDeviceOGL::StretchRect(GSTexture* st, const GSVector4& sr, GSTexture* dt, const GSVector4& dr, GLuint ps, GSBlendStateOGL* bs, bool linear)
{

	if(!st || !dt)
	{
		ASSERT(0);
		return;
	}

	// ************************************
	// Init
	// ************************************

	BeginScene();

	GSVector2i ds = dt->GetSize();

	// WARNING: setup of the program must be done first. So you can setup
	// 1/ subroutine uniform
	// 2/ bindless texture uniform
	// 3/ others uniform?
	m_shader->VS(m_convert.vs);
	m_shader->GS(0);
	m_shader->PS(ps);

	// ************************************
	// om
	// ************************************

	OMSetDepthStencilState(m_convert.dss, 0);
	OMSetBlendState(bs, 0);
	OMSetRenderTargets(dt, NULL);

	// ************************************
	// ia
	// ************************************


	// Original code from DX
	float left = dr.x * 2 / ds.x - 1.0f;
	float right = dr.z * 2 / ds.x - 1.0f;
#if 0
	float top = 1.0f - dr.y * 2 / ds.y;
	float bottom = 1.0f - dr.w * 2 / ds.y;
#else
	// Opengl get some issues with the coordinate
	// I flip top/bottom to fix scaling of the internal resolution
	float top = -1.0f + dr.y * 2 / ds.y;
	float bottom = -1.0f + dr.w * 2 / ds.y;
#endif

	// Flip y axis only when we render in the backbuffer
	// By default everything is render in the wrong order (ie dx).
	// 1/ consistency between several pass rendering (interlace)
	// 2/ in case some GSdx code expect thing in dx order.
	// Only flipping the backbuffer is transparent (I hope)...
	GSVector4 flip_sr = sr;
	if (static_cast<GSTextureOGL*>(dt)->IsBackbuffer()) {
		flip_sr.y = sr.w;
		flip_sr.w = sr.y;
	}

	GSVertexPT1 vertices[] =
	{
		{GSVector4(left  , top   , 0.0f, 0.0f) , GSVector2(flip_sr.x , flip_sr.y)} ,
		{GSVector4(right , top   , 0.0f, 0.0f) , GSVector2(flip_sr.z , flip_sr.y)} ,
		{GSVector4(left  , bottom, 0.0f, 0.0f) , GSVector2(flip_sr.x , flip_sr.w)} ,
		{GSVector4(right , bottom, 0.0f, 0.0f) , GSVector2(flip_sr.z , flip_sr.w)} ,
	};

	IASetVertexBuffer(vertices, 4);
	IASetPrimitiveTopology(GL_TRIANGLE_STRIP);

	// ************************************
	// Texture
	// ************************************

	if (GLLoader::found_GL_ARB_bindless_texture) {
		GLuint64 handle[2] = {static_cast<GSTextureOGL*>(st)->GetHandle(linear ? m_convert.ln : m_convert.pt) , 0};
		m_shader->PS_ressources(handle);
	} else {
		PSSetShaderResource(static_cast<GSTextureOGL*>(st)->GetID());
		PSSetSamplerState(linear ? m_convert.ln : m_convert.pt);
	}

	// ************************************
	// Draw
	// ************************************
	DrawPrimitive();

	// ************************************
	// End
	// ************************************

	EndScene();
}

void GSDeviceOGL::DoMerge(GSTexture* st[2], GSVector4* sr, GSTexture* dt, GSVector4* dr, bool slbg, bool mmod, const GSVector4& c)
{
	ClearRenderTarget(dt, c);

	if(st[1] && !slbg)
	{
		StretchRect(st[1], sr[1], dt, dr[1], m_merge_obj.ps[0]);
	}

	if(st[0])
	{
		m_merge_obj.cb->upload(&c.v);

		StretchRect(st[0], sr[0], dt, dr[0], m_merge_obj.ps[mmod ? 1 : 0], m_merge_obj.bs);
	}
}

void GSDeviceOGL::DoInterlace(GSTexture* st, GSTexture* dt, int shader, bool linear, float yoffset)
{
	GSVector4 s = GSVector4(dt->GetSize());

	GSVector4 sr(0, 0, 1, 1);
	GSVector4 dr(0.0f, yoffset, s.x, s.y + yoffset);

	InterlaceConstantBuffer cb;

	cb.ZrH = GSVector2(0, 1.0f / s.y);
	cb.hH = s.y / 2;

	m_interlace.cb->upload(&cb);

	StretchRect(st, sr, dt, dr, m_interlace.ps[shader], linear);
}

void GSDeviceOGL::DoFXAA(GSTexture* st, GSTexture* dt)
{
	// Lazy compile
	if (!m_fxaa.ps) {
		std::string fxaa_macro = "#define FXAA_GLSL_130 1\n";
		if (GLLoader::found_GL_ARB_gpu_shader5) {
			// This extension become core on openGL4
			fxaa_macro += "#extension GL_ARB_gpu_shader5 : enable\n";
			fxaa_macro += "#define FXAA_GATHER4_ALPHA 1\n";
		}
		m_fxaa.cb = new GSUniformBufferOGL(g_fxaa_cb_index, sizeof(FXAAConstantBuffer));
		m_fxaa.ps = m_shader->Compile("fxaa.fx", "ps_main", GL_FRAGMENT_SHADER, old_fxaa_fx, fxaa_macro);
	}

	GSVector2i s = dt->GetSize();

	GSVector4 sr(0, 0, 1, 1);
	GSVector4 dr(0, 0, s.x, s.y);

	FXAAConstantBuffer cb;

	cb.rcpFrame = GSVector4(1.0f / s.x, 1.0f / s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_fxaa.cb->upload(&cb);

	StretchRect(st, sr, dt, dr, m_fxaa.ps, true);
}

void GSDeviceOGL::DoExternalFX(GSTexture* st, GSTexture* dt)
{
	// Lazy compile
	if (!m_shaderfx.ps) {
		std::ifstream fconfig(theApp.GetConfig("shaderfx_conf", "dummy.ini"));
		std::stringstream config;
		if (fconfig.good())
			config << fconfig.rdbuf();

		std::ifstream fshader(theApp.GetConfig("shaderfx_glsl", "dummy.glsl"));
		std::stringstream shader;
		if (!fshader.good())
			return;
		shader << fshader.rdbuf();


		m_shaderfx.cb = new GSUniformBufferOGL(g_fx_cb_index, sizeof(ExternalFXConstantBuffer));
		m_shaderfx.ps = m_shader->Compile("Extra", "ps_main", GL_FRAGMENT_SHADER, shader.str().c_str(), config.str());
	}

	GSVector2i s = dt->GetSize();

	GSVector4 sr(0, 0, 1, 1);
	GSVector4 dr(0, 0, s.x, s.y);

	ExternalFXConstantBuffer cb;

	cb.xyFrame = GSVector2(s.x, s.y);
	cb.rcpFrame = GSVector4(1.0f / s.x, 1.0f / s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_shaderfx.cb->upload(&cb);

	StretchRect(st, sr, dt, dr, m_shaderfx.ps, true);
}

void GSDeviceOGL::DoShadeBoost(GSTexture* st, GSTexture* dt)
{
	GSVector2i s = dt->GetSize();

	GSVector4 sr(0, 0, 1, 1);
	GSVector4 dr(0, 0, s.x, s.y);

	ShadeBoostConstantBuffer cb;

	cb.rcpFrame = GSVector4(1.0f / s.x, 1.0f / s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_shadeboost.cb->upload(&cb);

	StretchRect(st, sr, dt, dr, m_shadeboost.ps, true);
}

void GSDeviceOGL::SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm)
{
#ifdef ENABLE_OGL_STENCIL_DEBUG
	const GSVector2i& size = rt->GetSize();
	GSTexture* t = CreateRenderTarget(size.x, size.y, false);
#else
	GSTexture* t = NULL;
#endif
	// sfex3 (after the capcom logo), vf4 (first menu fading in), ffxii shadows, rumble roses shadows, persona4 shadows

	BeginScene();

	ClearStencil(ds, 0);

	// WARNING: setup of the program must be done first. So you can setup
	// 1/ subroutine uniform
	// 2/ bindless texture uniform
	// 3/ others uniform?
	m_shader->VS(m_convert.vs);
	m_shader->GS(0);
	m_shader->PS(m_convert.ps[datm ? 2 : 3]);

	// om

	OMSetDepthStencilState(m_date.dss, 1);
	OMSetBlendState(m_date.bs, 0);
	OMSetRenderTargets(t, ds);

	// ia

	IASetVertexBuffer(vertices, 4);
	IASetPrimitiveTopology(GL_TRIANGLE_STRIP);


	// Texture

	if (GLLoader::found_GL_ARB_bindless_texture) {
		GLuint64 handle[2] = {static_cast<GSTextureOGL*>(rt)->GetHandle(m_convert.pt) , 0};
		m_shader->PS_ressources(handle);
	} else {
		PSSetShaderResource(static_cast<GSTextureOGL*>(rt)->GetID());
		PSSetSamplerState(m_convert.pt);
	}

	//

#ifdef ENABLE_OGL_STENCIL_DEBUG
	DrawPrimitive();
#else
	// normally ok without it if GL_ARB_framebuffer_no_attachments is supported (minus driver bug)
	OMSetWriteBuffer(GL_NONE);
	DrawPrimitive();
	OMSetWriteBuffer();
#endif

	EndScene();

#ifdef ENABLE_OGL_STENCIL_DEBUG
	// FIXME invalidate data
	Recycle(t);
#endif
}

void GSDeviceOGL::EndScene()
{
	m_va->EndScene();
}

void GSDeviceOGL::IASetVertexBuffer(const void* vertices, size_t count)
{
	m_va->UploadVB(vertices, count);
}

bool GSDeviceOGL::IAMapVertexBuffer(void** vertex, size_t stride, size_t count)
{
	return m_va->MapVB(vertex, count);
}

void GSDeviceOGL::IAUnmapVertexBuffer()
{
	m_va->UnmapVB();
}

void GSDeviceOGL::IASetIndexBuffer(const void* index, size_t count)
{
	m_va->UploadIB(index, count);
}

void GSDeviceOGL::IASetPrimitiveTopology(GLenum topology)
{
	m_va->SetTopology(topology);
}

void GSDeviceOGL::PSSetShaderResource(GLuint sr)
{
	if (GLState::tex_unit[0] != sr) {
		GLState::tex_unit[0] = sr;

		gl_ActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sr);

		// Get back to the expected active texture unit
		gl_ActiveTexture(GL_TEXTURE0 + 3);
	}
}

void GSDeviceOGL::PSSetShaderResources(GLuint tex[2])
{
	if (GLState::tex_unit[0] != tex[0] || GLState::tex_unit[1] != tex[1]) {
		GLState::tex_unit[0] = tex[0];
		GLState::tex_unit[1] = tex[1];

		gl_ActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex[0]);

		gl_ActiveTexture(GL_TEXTURE0 + 1);
		glBindTexture(GL_TEXTURE_2D, tex[1]);

		// Get back to the expected active texture unit
		gl_ActiveTexture(GL_TEXTURE0 + 3);
	}
}

void GSDeviceOGL::PSSetSamplerState(GLuint ss)
{
	if (GLState::ps_ss != ss) {
		GLState::ps_ss = ss;
		gl_BindSampler(0, ss);
	}
}

void GSDeviceOGL::OMAttachRt(GLuint rt)
{
	if (GLState::rt != rt) {
		GLState::rt = rt;
		gl_FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt, 0);
	}
}

void GSDeviceOGL::OMAttachDs(GLuint ds)
{
	if (GLState::ds != ds) {
		GLState::ds = ds;
		gl_FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, ds, 0);
	}
}

void GSDeviceOGL::OMSetFBO(GLuint fbo)
{
	if (GLState::fbo != fbo) {
		GLState::fbo = fbo;
		gl_BindFramebuffer(GL_FRAMEBUFFER, fbo);
	}
}

void GSDeviceOGL::OMSetWriteBuffer(GLenum buffer)
{
	GLenum target[1] = {buffer};
	gl_DrawBuffers(1, target);
}

void GSDeviceOGL::OMSetDepthStencilState(GSDepthStencilOGL* dss, uint8 sref)
{
	// State is checkd inside the object but worst case is 11 comparaisons !
	if (m_state.dss != dss) {
		m_state.dss = dss;

		dss->SetupDepth();
		dss->SetupStencil();
	}
}

void GSDeviceOGL::OMSetBlendState(GSBlendStateOGL* bs, float bf)
{
	// State is checkd inside the object but worst case is 15 comparaisons !
	if ( m_state.bs != bs || (m_state.bf != bf && bs->HasConstantFactor()) )
	{
		m_state.bs = bs;
		m_state.bf = bf;

		bs->SetupBlend(bf);
	}
}

void GSDeviceOGL::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor)
{
	if (rt == NULL || !static_cast<GSTextureOGL*>(rt)->IsBackbuffer()) {
		OMSetFBO(m_fbo);
		if (rt) {
			OMAttachRt(static_cast<GSTextureOGL*>(rt)->GetID());
		} else {
			// Note: NULL rt is only used in DATE so far.
			OMAttachRt(0);
		}

		// Note: it must be done after OMSetFBO
		if (ds)
			OMAttachDs(static_cast<GSTextureOGL*>(ds)->GetID());
		else
			OMAttachDs(0);

	} else {
		// Render in the backbuffer
		OMSetFBO(0);
	}


	GSVector2i size = rt ? rt->GetSize() : ds->GetSize();
	if(GLState::viewport != size)
	{
		GLState::viewport = size;
		glViewport(0, 0, size.x, size.y);
	}

	GSVector4i r = scissor ? *scissor : GSVector4i(size).zwxy();

	if(!GLState::scissor.eq(r))
	{
		GLState::scissor = r;
		glScissor( r.x, r.y, r.width(), r.height() );
	}
}

void GSDeviceOGL::CheckDebugLog()
{
#ifndef ENABLE_GLES
       unsigned int count = 16; // max. num. of messages that will be read from the log
       int bufsize = 2048;
	   unsigned int sources[16] = {};
	   unsigned int types[16] = {};
	   unsigned int ids[16]   = {};
	   unsigned int severities[16] = {};
	   int lengths[16] = {};
       char* messageLog = new char[bufsize];

       unsigned int retVal = gl_GetDebugMessageLogARB(count, bufsize, sources, types, ids, severities, lengths, messageLog);

       if(retVal > 0)
       {
             unsigned int pos = 0;
             for(unsigned int i=0; i<retVal; i++)
             {
                    DebugOutputToFile(sources[i], types[i], ids[i], severities[i], lengths[i], &messageLog[pos], NULL);
                    pos += lengths[i];
              }
       }

	   delete[] messageLog;
#endif
}

// Note: used as a callback of DebugMessageCallback. Don't change the signature
void GSDeviceOGL::DebugOutputToFile(GLenum gl_source, GLenum gl_type, GLuint id, GLenum gl_severity, GLsizei gl_length, const GLchar *gl_message, const void* userParam)
{
#ifndef ENABLE_GLES
	std::string message(gl_message, gl_length);
	std::string type, severity, source;
	static int sev_counter = 0;
	switch(gl_type) {
		case GL_DEBUG_TYPE_ERROR_ARB               : type = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB : type = "Deprecated bhv"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB  : type = "Undefined bhv"; break;
		case GL_DEBUG_TYPE_PORTABILITY_ARB         : type = "Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE_ARB         : type = "Perf"; break;
		case GL_DEBUG_TYPE_OTHER_ARB               : type = "Others"; break;
		default                                    : type = "TTT"; break;
	}
	switch(gl_severity) {
		case GL_DEBUG_SEVERITY_HIGH_ARB   : severity = "High"; sev_counter++; break;
		case GL_DEBUG_SEVERITY_MEDIUM_ARB : severity = "Mid"; break;
		case GL_DEBUG_SEVERITY_LOW_ARB    : severity = "Low"; break;
		default                           : severity = "Info"; break;
	}
	switch(gl_source) {
		case GL_DEBUG_SOURCE_API_ARB             : source = "API"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB   : source = "WINDOW"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB : source = "COMPILER"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY_ARB     : source = "3rdparty"; break;
		case GL_DEBUG_SOURCE_APPLICATION_ARB     : source = "Application"; break;
		case GL_DEBUG_SOURCE_OTHER_ARB           : source = "Others"; break;
		default                                  : source = "???"; break;
	}

	#ifdef LOUD_DEBUGGING
	fprintf(stderr,"Type:%s\tID:%d\tSeverity:%s\tMessage:%s\n", type.c_str(), g_draw_count, severity.c_str(), message.c_str());
	#endif

	FILE* f = fopen("Debug.txt","a");
	if(f)
	{
		fprintf(f,"Type:%s\tID:%d\tSeverity:%s\tMessage:%s\n", type.c_str(), g_draw_count, severity.c_str(), message.c_str());
		fclose(f);
	}
	ASSERT(sev_counter < 5);
#endif
}

// (A - B) * C + D
// A: Cs/Cd/0
// B: Cs/Cd/0
// C: As/Ad/FIX
// D: Cs/Cd/0

// bogus: 0100, 0110, 0120, 0200, 0210, 0220, 1001, 1011, 1021
// tricky: 1201, 1211, 1221

// Source.rgb = float3(1, 1, 1);
// 1201 Cd*(1 + As) => Source * Dest color + Dest * Source alpha
// 1211 Cd*(1 + Ad) => Source * Dest color + Dest * Dest alpha
// 1221 Cd*(1 + F) => Source * Dest color + Dest * Factor

// Copy Dx blend table and convert it to ogl
#define D3DBLENDOP_ADD			GL_FUNC_ADD
#define D3DBLENDOP_SUBTRACT		GL_FUNC_SUBTRACT
#define D3DBLENDOP_REVSUBTRACT	GL_FUNC_REVERSE_SUBTRACT

#define D3DBLEND_ONE			GL_ONE
#define D3DBLEND_ZERO			GL_ZERO
#define D3DBLEND_INVDESTALPHA	GL_ONE_MINUS_DST_ALPHA
#define D3DBLEND_DESTALPHA		GL_DST_ALPHA
#define D3DBLEND_DESTCOLOR		GL_DST_COLOR
#define D3DBLEND_BLENDFACTOR	GL_CONSTANT_COLOR
#define D3DBLEND_INVBLENDFACTOR GL_ONE_MINUS_CONSTANT_COLOR

#ifdef ENABLE_GLES
#define D3DBLEND_SRCALPHA		GL_SRC_ALPHA
#define D3DBLEND_INVSRCALPHA	GL_ONE_MINUS_SRC_ALPHA
#else
#define D3DBLEND_SRCALPHA		GL_SRC1_ALPHA
#define D3DBLEND_INVSRCALPHA	GL_ONE_MINUS_SRC1_ALPHA
#endif

const GSDeviceOGL::D3D9Blend GSDeviceOGL::m_blendMapD3D9[3*3*3*3] =
{
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_ZERO},						// 0000: (Cs - Cs)*As + Cs ==> Cs
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ONE},						// 0001: (Cs - Cs)*As + Cd ==> Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ZERO},						// 0002: (Cs - Cs)*As +  0 ==> 0
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_ZERO},						// 0010: (Cs - Cs)*Ad + Cs ==> Cs
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ONE},						// 0011: (Cs - Cs)*Ad + Cd ==> Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ZERO},						// 0012: (Cs - Cs)*Ad +  0 ==> 0
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_ZERO},						// 0020: (Cs - Cs)*F  + Cs ==> Cs
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ONE},						// 0021: (Cs - Cs)*F  + Cd ==> Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ZERO},						// 0022: (Cs - Cs)*F  +  0 ==> 0
	{1, D3DBLENDOP_SUBTRACT, D3DBLEND_SRCALPHA, D3DBLEND_SRCALPHA},			//*0100: (Cs - Cd)*As + Cs ==> Cs*(As + 1) - Cd*As
	{0, D3DBLENDOP_ADD, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA},			// 0101: (Cs - Cd)*As + Cd ==> Cs*As + Cd*(1 - As)
	{0, D3DBLENDOP_SUBTRACT, D3DBLEND_SRCALPHA, D3DBLEND_SRCALPHA},			// 0102: (Cs - Cd)*As +  0 ==> Cs*As - Cd*As
	{1, D3DBLENDOP_SUBTRACT, D3DBLEND_DESTALPHA, D3DBLEND_DESTALPHA},		//*0110: (Cs - Cd)*Ad + Cs ==> Cs*(Ad + 1) - Cd*Ad
	{0, D3DBLENDOP_ADD, D3DBLEND_DESTALPHA, D3DBLEND_INVDESTALPHA},			// 0111: (Cs - Cd)*Ad + Cd ==> Cs*Ad + Cd*(1 - Ad)
	{0, D3DBLENDOP_SUBTRACT, D3DBLEND_DESTALPHA, D3DBLEND_DESTALPHA},		// 0112: (Cs - Cd)*Ad +  0 ==> Cs*Ad - Cd*Ad
	{1, D3DBLENDOP_SUBTRACT, D3DBLEND_BLENDFACTOR, D3DBLEND_BLENDFACTOR},	//*0120: (Cs - Cd)*F  + Cs ==> Cs*(F + 1) - Cd*F
	{0, D3DBLENDOP_ADD, D3DBLEND_BLENDFACTOR, D3DBLEND_INVBLENDFACTOR},		// 0121: (Cs - Cd)*F  + Cd ==> Cs*F + Cd*(1 - F)
	{0, D3DBLENDOP_SUBTRACT, D3DBLEND_BLENDFACTOR, D3DBLEND_BLENDFACTOR},	// 0122: (Cs - Cd)*F  +  0 ==> Cs*F - Cd*F
	{1, D3DBLENDOP_ADD, D3DBLEND_SRCALPHA, D3DBLEND_ZERO},					//*0200: (Cs -  0)*As + Cs ==> Cs*(As + 1)
	{0, D3DBLENDOP_ADD, D3DBLEND_SRCALPHA, D3DBLEND_ONE},					// 0201: (Cs -  0)*As + Cd ==> Cs*As + Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_SRCALPHA, D3DBLEND_ZERO},					// 0202: (Cs -  0)*As +  0 ==> Cs*As
	{1, D3DBLENDOP_ADD, D3DBLEND_DESTALPHA, D3DBLEND_ZERO},					//*0210: (Cs -  0)*Ad + Cs ==> Cs*(Ad + 1)
	{0, D3DBLENDOP_ADD, D3DBLEND_DESTALPHA, D3DBLEND_ONE},					// 0211: (Cs -  0)*Ad + Cd ==> Cs*Ad + Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_DESTALPHA, D3DBLEND_ZERO},					// 0212: (Cs -  0)*Ad +  0 ==> Cs*Ad
	{1, D3DBLENDOP_ADD, D3DBLEND_BLENDFACTOR, D3DBLEND_ZERO},				//*0220: (Cs -  0)*F  + Cs ==> Cs*(F + 1)
	{0, D3DBLENDOP_ADD, D3DBLEND_BLENDFACTOR, D3DBLEND_ONE},				// 0221: (Cs -  0)*F  + Cd ==> Cs*F + Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_BLENDFACTOR, D3DBLEND_ZERO},				// 0222: (Cs -  0)*F  +  0 ==> Cs*F
	{0, D3DBLENDOP_ADD, D3DBLEND_INVSRCALPHA, D3DBLEND_SRCALPHA},			// 1000: (Cd - Cs)*As + Cs ==> Cd*As + Cs*(1 - As)
	{1, D3DBLENDOP_REVSUBTRACT, D3DBLEND_SRCALPHA, D3DBLEND_SRCALPHA},		//*1001: (Cd - Cs)*As + Cd ==> Cd*(As + 1) - Cs*As
	{0, D3DBLENDOP_REVSUBTRACT, D3DBLEND_SRCALPHA, D3DBLEND_SRCALPHA},		// 1002: (Cd - Cs)*As +  0 ==> Cd*As - Cs*As
	{0, D3DBLENDOP_ADD, D3DBLEND_INVDESTALPHA, D3DBLEND_DESTALPHA},			// 1010: (Cd - Cs)*Ad + Cs ==> Cd*Ad + Cs*(1 - Ad)
	{1, D3DBLENDOP_REVSUBTRACT, D3DBLEND_DESTALPHA, D3DBLEND_DESTALPHA},	//*1011: (Cd - Cs)*Ad + Cd ==> Cd*(Ad + 1) - Cs*Ad
	{0, D3DBLENDOP_REVSUBTRACT, D3DBLEND_DESTALPHA, D3DBLEND_DESTALPHA},	// 1012: (Cd - Cs)*Ad +  0 ==> Cd*Ad - Cs*Ad
	{0, D3DBLENDOP_ADD, D3DBLEND_INVBLENDFACTOR, D3DBLEND_BLENDFACTOR},		// 1020: (Cd - Cs)*F  + Cs ==> Cd*F + Cs*(1 - F)
	{1, D3DBLENDOP_REVSUBTRACT, D3DBLEND_BLENDFACTOR, D3DBLEND_BLENDFACTOR},//*1021: (Cd - Cs)*F  + Cd ==> Cd*(F + 1) - Cs*F
	{0, D3DBLENDOP_REVSUBTRACT, D3DBLEND_BLENDFACTOR, D3DBLEND_BLENDFACTOR},// 1022: (Cd - Cs)*F  +  0 ==> Cd*F - Cs*F
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_ZERO},						// 1100: (Cd - Cd)*As + Cs ==> Cs
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ONE},						// 1101: (Cd - Cd)*As + Cd ==> Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ZERO},						// 1102: (Cd - Cd)*As +  0 ==> 0
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_ZERO},						// 1110: (Cd - Cd)*Ad + Cs ==> Cs
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ONE},						// 1111: (Cd - Cd)*Ad + Cd ==> Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ZERO},						// 1112: (Cd - Cd)*Ad +  0 ==> 0
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_ZERO},						// 1120: (Cd - Cd)*F  + Cs ==> Cs
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ONE},						// 1121: (Cd - Cd)*F  + Cd ==> Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ZERO},						// 1122: (Cd - Cd)*F  +  0 ==> 0
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_SRCALPHA},					// 1200: (Cd -  0)*As + Cs ==> Cs + Cd*As
	{2, D3DBLENDOP_ADD, D3DBLEND_DESTCOLOR, D3DBLEND_SRCALPHA},				//#1201: (Cd -  0)*As + Cd ==> Cd*(1 + As)  // ffxii main menu background glow effect
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_SRCALPHA},					// 1202: (Cd -  0)*As +  0 ==> Cd*As
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_DESTALPHA},					// 1210: (Cd -  0)*Ad + Cs ==> Cs + Cd*Ad
	{2, D3DBLENDOP_ADD, D3DBLEND_DESTCOLOR, D3DBLEND_DESTALPHA},			//#1211: (Cd -  0)*Ad + Cd ==> Cd*(1 + Ad)
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_DESTALPHA},					// 1212: (Cd -  0)*Ad +  0 ==> Cd*Ad
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_BLENDFACTOR},				// 1220: (Cd -  0)*F  + Cs ==> Cs + Cd*F
	{2, D3DBLENDOP_ADD, D3DBLEND_DESTCOLOR, D3DBLEND_BLENDFACTOR},			//#1221: (Cd -  0)*F  + Cd ==> Cd*(1 + F)
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_BLENDFACTOR},				// 1222: (Cd -  0)*F  +  0 ==> Cd*F
	{0, D3DBLENDOP_ADD, D3DBLEND_INVSRCALPHA, D3DBLEND_ZERO},				// 2000: (0  - Cs)*As + Cs ==> Cs*(1 - As)
	{0, D3DBLENDOP_REVSUBTRACT, D3DBLEND_SRCALPHA, D3DBLEND_ONE},			// 2001: (0  - Cs)*As + Cd ==> Cd - Cs*As
	{0, D3DBLENDOP_REVSUBTRACT, D3DBLEND_SRCALPHA, D3DBLEND_ZERO},			// 2002: (0  - Cs)*As +  0 ==> 0 - Cs*As
	{0, D3DBLENDOP_ADD, D3DBLEND_INVDESTALPHA, D3DBLEND_ZERO},				// 2010: (0  - Cs)*Ad + Cs ==> Cs*(1 - Ad)
	{0, D3DBLENDOP_REVSUBTRACT, D3DBLEND_DESTALPHA, D3DBLEND_ONE},			// 2011: (0  - Cs)*Ad + Cd ==> Cd - Cs*Ad
	{0, D3DBLENDOP_REVSUBTRACT, D3DBLEND_DESTALPHA, D3DBLEND_ZERO},			// 2012: (0  - Cs)*Ad +  0 ==> 0 - Cs*Ad
	{0, D3DBLENDOP_ADD, D3DBLEND_INVBLENDFACTOR, D3DBLEND_ZERO},			// 2020: (0  - Cs)*F  + Cs ==> Cs*(1 - F)
	{0, D3DBLENDOP_REVSUBTRACT, D3DBLEND_BLENDFACTOR, D3DBLEND_ONE},		// 2021: (0  - Cs)*F  + Cd ==> Cd - Cs*F
	{0, D3DBLENDOP_REVSUBTRACT, D3DBLEND_BLENDFACTOR, D3DBLEND_ZERO},		// 2022: (0  - Cs)*F  +  0 ==> 0 - Cs*F
	{0, D3DBLENDOP_SUBTRACT, D3DBLEND_ONE, D3DBLEND_SRCALPHA},				// 2100: (0  - Cd)*As + Cs ==> Cs - Cd*As
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_INVSRCALPHA},				// 2101: (0  - Cd)*As + Cd ==> Cd*(1 - As)
	{0, D3DBLENDOP_SUBTRACT, D3DBLEND_ZERO, D3DBLEND_SRCALPHA},				// 2102: (0  - Cd)*As +  0 ==> 0 - Cd*As
	{0, D3DBLENDOP_SUBTRACT, D3DBLEND_ONE, D3DBLEND_DESTALPHA},				// 2110: (0  - Cd)*Ad + Cs ==> Cs - Cd*Ad
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_INVDESTALPHA},				// 2111: (0  - Cd)*Ad + Cd ==> Cd*(1 - Ad)
	{0, D3DBLENDOP_SUBTRACT, D3DBLEND_ONE, D3DBLEND_DESTALPHA},				// 2112: (0  - Cd)*Ad +  0 ==> 0 - Cd*Ad
	{0, D3DBLENDOP_SUBTRACT, D3DBLEND_ONE, D3DBLEND_BLENDFACTOR},			// 2120: (0  - Cd)*F  + Cs ==> Cs - Cd*F
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_INVBLENDFACTOR},			// 2121: (0  - Cd)*F  + Cd ==> Cd*(1 - F)
	{0, D3DBLENDOP_SUBTRACT, D3DBLEND_ONE, D3DBLEND_BLENDFACTOR},			// 2122: (0  - Cd)*F  +  0 ==> 0 - Cd*F
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_ZERO},						// 2200: (0  -  0)*As + Cs ==> Cs
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ONE},						// 2201: (0  -  0)*As + Cd ==> Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ZERO},						// 2202: (0  -  0)*As +  0 ==> 0
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_ZERO},						// 2210: (0  -  0)*Ad + Cs ==> Cs
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ONE},						// 2211: (0  -  0)*Ad + Cd ==> Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ZERO},						// 2212: (0  -  0)*Ad +  0 ==> 0
	{0, D3DBLENDOP_ADD, D3DBLEND_ONE, D3DBLEND_ZERO},						// 2220: (0  -  0)*F  + Cs ==> Cs
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ONE},						// 2221: (0  -  0)*F  + Cd ==> Cd
	{0, D3DBLENDOP_ADD, D3DBLEND_ZERO, D3DBLEND_ZERO},						// 2222: (0  -  0)*F  +  0 ==> 0
};
