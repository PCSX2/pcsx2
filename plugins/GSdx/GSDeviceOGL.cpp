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

//#define ONLY_LINES

// TODO port those value into PerfMon API
#ifdef ENABLE_OGL_DEBUG_MEM_BW
uint64 g_real_texture_upload_byte = 0;
uint64 g_vertex_upload_byte = 0;
uint64 g_uniform_upload_byte = 0;
#endif

static const uint32 g_merge_cb_index      = 10;
static const uint32 g_interlace_cb_index  = 11;
static const uint32 g_shadeboost_cb_index = 12;
static const uint32 g_fx_cb_index         = 14;
static const uint32 g_convert_index       = 15;

bool GSDeviceOGL::m_debug_gl_call = false;
int  GSDeviceOGL::s_n = 0;
FILE* GSDeviceOGL::m_debug_gl_file = NULL;

GSDeviceOGL::GSDeviceOGL()
	: m_msaa(0)
	, m_window(NULL)
	, m_fbo(0)
	, m_fbo_read(0)
	, m_va(NULL)
	, m_apitrace(0)
	, m_palette_ss(0)
	, m_vs_cb(NULL)
	, m_ps_cb(NULL)
	, m_shader(NULL)
{
	memset(&m_merge_obj, 0, sizeof(m_merge_obj));
	memset(&m_interlace, 0, sizeof(m_interlace));
	memset(&m_convert, 0, sizeof(m_convert));
	memset(&m_fxaa, 0, sizeof(m_fxaa));
	memset(&m_shaderfx, 0, sizeof(m_shaderfx));
	memset(&m_date, 0, sizeof(m_date));
	memset(&m_shadeboost, 0, sizeof(m_shadeboost));
	memset(&m_om_dss, 0, sizeof(m_om_dss));
	GLState::Clear();

	// Reset the debug file
	#ifdef ENABLE_OGL_DEBUG
	m_debug_gl_file = fopen("GSdx_opengl_debug.txt","w");
	#endif

	m_debug_gl_call =  theApp.GetConfig("debug_opengl", 0);
}

GSDeviceOGL::~GSDeviceOGL()
{
	if (m_debug_gl_file) {
		fclose(m_debug_gl_file);
		m_debug_gl_file = NULL;
	}

	// If the create function wasn't called nothing to do.
	if (m_shader == NULL)
		return;

	GL_PUSH("GSDeviceOGL destructor");

	// Clean vertex buffer state
	delete (m_va);

	// Clean m_merge_obj
	delete (m_merge_obj.cb);

	// Clean m_interlace
	delete (m_interlace.cb);

	// Clean m_convert
	delete m_convert.dss;
	delete m_convert.dss_write;
	delete m_convert.cb;

	// Clean m_fxaa
	delete m_fxaa.cb;

	// Clean m_shaderfx
	delete m_shaderfx.cb;

	// Clean m_date
	delete m_date.dss;

	// Clean shadeboost
	delete m_shadeboost.cb;

	// Clean various opengl allocation
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteFramebuffers(1, &m_fbo_read);

	// Delete HW FX
	delete m_vs_cb;
	delete m_ps_cb;
	glDeleteSamplers(1, &m_palette_ss);

	m_ps.clear();

	glDeleteSamplers(countof(m_ps_ss), m_ps_ss);

	for (uint32 key = 0; key < countof(m_om_dss); key++) delete m_om_dss[key];

	PboPool::Destroy();

	// Must be done after the destruction of all shader/program objects
	delete m_shader;
	m_shader = NULL;

	GL_POP();
}

GSTexture* GSDeviceOGL::CreateSurface(int type, int w, int h, bool msaa, int fmt)
{
	GL_PUSH("Create surface");

	// A wrapper to call GSTextureOGL, with the different kind of parameter
	GSTextureOGL* t = NULL;
	t = new GSTextureOGL(type, w, h, fmt, m_fbo_read);

	// NOTE: I'm not sure RenderTarget always need to be cleared. It could be costly for big upscale.
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

	GL_POP();
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
#ifdef ENABLE_OGL_DEBUG
	if (theApp.GetConfig("debug_opengl", 0)) {
		if (glDebugMessageCallback) {
			glDebugMessageCallback((GLDEBUGPROC)DebugOutputToFile, NULL);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		}
		if (glDebugMessageControl) {
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true);
			// Useless info message on Nvidia driver
			GLuint ids[] = {0x20004};
			glDebugMessageControl(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_OTHER_ARB, GL_DONT_CARE, countof(ids), ids, false);
		}
	}
#endif

	// WARNING it must be done after the control setup (at least on MESA)
	GL_PUSH("GSDeviceOGL::Create");

	// ****************************************************************
	// Various object
	// ****************************************************************
	GL_PUSH("GSDeviceOGL::Various");

	m_shader = new GSShaderOGL(!!theApp.GetConfig("debug_glsl_shader", 0));

	glGenFramebuffers(1, &m_fbo);
	// Always write to the first buffer
	OMSetFBO(m_fbo);
	GLenum target[1] = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(1, target);
	OMSetFBO(0);

	glGenFramebuffers(1, &m_fbo_read);
	// Always read from the first buffer
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	GL_POP();

	// ****************************************************************
	// Vertex buffer state
	// ****************************************************************
	GL_PUSH("GSDeviceOGL::Vertex Buffer");

	ASSERT(sizeof(GSVertexPT1) == sizeof(GSVertex));
	GSInputLayoutOGL il_convert[] =
	{
		{2 , GL_FLOAT          , GL_FALSE , sizeof(GSVertexPT1) , (const GLvoid*)(0) }  ,
		{2 , GL_FLOAT          , GL_FALSE , sizeof(GSVertexPT1) , (const GLvoid*)(16) } ,
		{4 , GL_UNSIGNED_BYTE  , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(8) }  ,
		{1 , GL_FLOAT          , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(12) } ,
		{2 , GL_UNSIGNED_SHORT , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(16) } ,
		{1 , GL_UNSIGNED_INT   , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(20) } ,
		{2 , GL_UNSIGNED_SHORT , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(24) } ,
		{4 , GL_UNSIGNED_BYTE  , GL_TRUE  , sizeof(GSVertex)    , (const GLvoid*)(28) } , // Only 1 byte is useful but hardware unit only support 4B
	};
	m_va = new GSVertexBufferStateOGL(il_convert, countof(il_convert));

	GL_POP();
	// ****************************************************************
	// Pre Generate the different sampler object
	// ****************************************************************
	GL_PUSH("GSDeviceOGL::Sampler");

	for (uint32 key = 0; key < countof(m_ps_ss); key++) {
		m_ps_ss[key] = CreateSampler(PSSamplerSelector(key));
	}

	GL_POP();

	// ****************************************************************
	// convert
	// ****************************************************************
	GL_PUSH("GSDeviceOGL::Convert");

	m_convert.cb = new GSUniformBufferOGL(g_convert_index, sizeof(ConvertConstantBuffer));
	// Upload once and forget about it
	ConvertConstantBuffer cb;
	cb.ScalingFactor = GSVector4i(theApp.GetConfig("upscale_multiplier", 1));
	m_convert.cb->upload(&cb);

	GLuint vs = m_shader->Compile("convert.glsl", "vs_main", GL_VERTEX_SHADER, convert_glsl);
	GLuint ps = 0;

	m_convert.vs = vs;
	for(size_t i = 0; i < countof(m_convert.ps); i++) {
		ps = m_shader->Compile("convert.glsl", format("ps_main%d", i), GL_FRAGMENT_SHADER, convert_glsl);
		m_convert.ps[i] = m_shader->LinkPipeline(vs, 0, ps);
	}

	PSSamplerSelector point;
	m_convert.pt = GetSamplerID(point);

	PSSamplerSelector bilinear;
	bilinear.ltf = true;
	m_convert.ln = GetSamplerID(bilinear);

	m_convert.dss = new GSDepthStencilOGL();
	m_convert.dss_write = new GSDepthStencilOGL();
	m_convert.dss_write->EnableDepth();
	m_convert.dss_write->SetDepth(GL_ALWAYS, true);

	GL_POP();

	// ****************************************************************
	// merge
	// ****************************************************************
	GL_PUSH("GSDeviceOGL::Merge");

	m_merge_obj.cb = new GSUniformBufferOGL(g_merge_cb_index, sizeof(MergeConstantBuffer));

	for(size_t i = 0; i < countof(m_merge_obj.ps); i++) {
		ps = m_shader->Compile("merge.glsl", format("ps_main%d", i), GL_FRAGMENT_SHADER, merge_glsl);
		m_merge_obj.ps[i] = m_shader->LinkPipeline(vs, 0, ps);
	}

	GL_POP();

	// ****************************************************************
	// interlace
	// ****************************************************************
	GL_PUSH("GSDeviceOGL::Interlace");

	m_interlace.cb = new GSUniformBufferOGL(g_interlace_cb_index, sizeof(InterlaceConstantBuffer));

	for(size_t i = 0; i < countof(m_interlace.ps); i++) {
		ps = m_shader->Compile("interlace.glsl", format("ps_main%d", i), GL_FRAGMENT_SHADER, interlace_glsl);
		m_interlace.ps[i] = m_shader->LinkPipeline(vs, 0, ps);
	}

	GL_POP();

	// ****************************************************************
	// Shade boost
	// ****************************************************************
	GL_PUSH("GSDeviceOGL::Shadeboost");

	m_shadeboost.cb = new GSUniformBufferOGL(g_shadeboost_cb_index, sizeof(ShadeBoostConstantBuffer));

	int ShadeBoost_Contrast = theApp.GetConfig("ShadeBoost_Contrast", 50);
	int ShadeBoost_Brightness = theApp.GetConfig("ShadeBoost_Brightness", 50);
	int ShadeBoost_Saturation = theApp.GetConfig("ShadeBoost_Saturation", 50);
	std::string shade_macro = format("#define SB_SATURATION %d.0\n", ShadeBoost_Saturation)
		+ format("#define SB_BRIGHTNESS %d.0\n", ShadeBoost_Brightness)
		+ format("#define SB_CONTRAST %d.0\n", ShadeBoost_Contrast);

	ps = m_shader->Compile("shadeboost.glsl", "ps_main", GL_FRAGMENT_SHADER, shadeboost_glsl, shade_macro);
	m_shadeboost.ps = m_shader->LinkPipeline(vs, 0, ps);

	GL_POP();

	// ****************************************************************
	// rasterization configuration
	// ****************************************************************
	GL_PUSH("GSDeviceOGL::Rasterization");

#ifdef ONLY_LINES
	glLineWidth(5.0);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#else
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
	glDisable(GL_CULL_FACE);
	glEnable(GL_SCISSOR_TEST);
	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DITHER); // Honestly I don't know!

	GL_POP();

	// ****************************************************************
	// DATE
	// ****************************************************************
	GL_PUSH("GSDeviceOGL::Date");

	m_date.dss = new GSDepthStencilOGL();
	m_date.dss->EnableStencil();
	m_date.dss->SetStencil(GL_ALWAYS, GL_REPLACE);

	GL_POP();

	// ****************************************************************
	// Use DX coordinate convention
	// ****************************************************************

	// VS gl_position.z => [-1,-1]
	// FS depth => [0, 1]
	// because of -1 we loose lot of precision for small GS value
	// This extension allow FS depth to range from -1 to 1. So
	// gl_position.z could range from [0, 1]
	// Change depth convention
	glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	// ****************************************************************
	// HW renderer shader
	// ****************************************************************
	GL_PUSH("GSDeviceOGL::CreateTextureFX");

	CreateTextureFX();

	GL_POP();

	// ****************************************************************
	// Pbo Pool allocation
	// ****************************************************************
	GL_PUSH("GSDeviceOGL::PBO");

	// Mesa seems to use it to compute the row length. In our case, we are
	// tightly packed so don't bother with this parameter and set it to the
	// minimum alignment (1 byte)
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	PboPool::Init();

	GL_POP();

	// Done !
	GL_POP();

	// ****************************************************************
	// Finish window setup and backbuffer
	// ****************************************************************
	if(!GSDevice::Create(wnd))
		return false;

	GSVector4i rect = wnd->GetClientRect();
	Reset(rect.z, rect.w);

	// Basic to ensure structures are correctly packed
	ASSERT(sizeof(VSSelector) == 4);
	ASSERT(sizeof(PSSelector) == 8);
	ASSERT(sizeof(PSSamplerSelector) == 4);
	ASSERT(sizeof(OMDepthStencilSelector) == 4);
	ASSERT(sizeof(OMColorMaskSelector) == 4);

	return true;
}

bool GSDeviceOGL::Reset(int w, int h)
{
	if(!GSDevice::Reset(w, h))
		return false;

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
	#ifdef ENABLE_OGL_DEBUG
	CheckDebugLog();
	#endif

	m_wnd->Flip();
}

void GSDeviceOGL::BeforeDraw()
{
}

void GSDeviceOGL::AfterDraw()
{
}

void GSDeviceOGL::DrawPrimitive()
{
	BeforeDraw();
	m_va->DrawPrimitive();
	AfterDraw();
}

void GSDeviceOGL::DrawPrimitive(int offset, int count)
{
	BeforeDraw();
	m_va->DrawPrimitive(offset, count);
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
	//ASSERT(offset + count <= (int)m_index.count);

	BeforeDraw();
	m_va->DrawIndexedPrimitive(offset, count);
	AfterDraw();
}

void GSDeviceOGL::ClearRenderTarget(GSTexture* t, const GSVector4& c)
{
	if (!t) return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);
	if (T->HasBeenCleaned() && !T->IsBackbuffer())
		return;

	GL_PUSH("Clear RT %d", T->GetID());

	// TODO: check size of scissor before toggling it
	glDisable(GL_SCISSOR_TEST);

	uint32 old_color_mask = GLState::wrgba;
	OMSetColorMaskState();

	if (T->IsBackbuffer()) {
		OMSetFBO(0);

		// glDrawBuffer(GL_BACK); // this is the default when there is no FB
		// 0 will select the first drawbuffer ie GL_BACK
		glClearBufferfv(GL_COLOR, 0, c.v);
	} else {
		OMSetFBO(m_fbo);
		OMAttachRt(T);

		glClearBufferfv(GL_COLOR, 0, c.v);

	}

	OMSetColorMaskState(OMColorMaskSelector(old_color_mask));

	glEnable(GL_SCISSOR_TEST);

	T->WasCleaned();

	GL_POP();
}

void GSDeviceOGL::ClearRenderTarget(GSTexture* t, uint32 c)
{
	if (!t) return;

	GSVector4 color = GSVector4::rgba32(c) * (1.0f / 255);
	ClearRenderTarget(t, color);
}

void GSDeviceOGL::ClearRenderTarget_i(GSTexture* t, int32 c)
{
	if (!t) return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);

	GL_PUSH("Clear RTi %d", T->GetID());

	uint32 old_color_mask = GLState::wrgba;
	OMSetColorMaskState();

	// Keep SCISSOR_TEST enabled on purpose to reduce the size
	// of clean in DATE (impact big upscaling)
	int32 col[4] = {c, c, c, c};

	OMSetFBO(m_fbo);
	OMAttachRt(T);

	// Blending is not supported when you render to an Integer texture
	if (GLState::blend) {
		glDisable(GL_BLEND);
	}

	glClearBufferiv(GL_COLOR, 0, col);

	OMSetColorMaskState(OMColorMaskSelector(old_color_mask));

	if (GLState::blend) {
		glEnable(GL_BLEND);
	}

	GL_POP();
}

void GSDeviceOGL::ClearDepth(GSTexture* t, float c)
{
	if (!t) return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);

	GL_PUSH("Clear Depth %d", T->GetID());

	OMSetFBO(m_fbo);
	OMAttachDs(T);

	// TODO: check size of scissor before toggling it
	glDisable(GL_SCISSOR_TEST);
	if (GLState::depth_mask) {
		glClearBufferfv(GL_DEPTH, 0, &c);
	} else {
		glDepthMask(true);
		glClearBufferfv(GL_DEPTH, 0, &c);
		glDepthMask(false);
	}
	glEnable(GL_SCISSOR_TEST);

	GL_POP();
}

void GSDeviceOGL::ClearStencil(GSTexture* t, uint8 c)
{
	if (!t) return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);

	GL_PUSH("Clear Stencil %d", T->GetID());

	// Keep SCISSOR_TEST enabled on purpose to reduce the size
	// of clean in DATE (impact big upscaling)
	OMSetFBO(m_fbo);
	OMAttachDs(T);
	GLint color = c;

	glClearBufferiv(GL_STENCIL, 0, &color);

	GL_POP();
}

GLuint GSDeviceOGL::CreateSampler(PSSamplerSelector sel)
{
	return CreateSampler(sel.ltf, sel.tau, sel.tav, sel.aniso);
}

GLuint GSDeviceOGL::CreateSampler(bool bilinear, bool tau, bool tav, bool aniso)
{
	GL_PUSH("Create Sampler");

	GLuint sampler;
	glCreateSamplers(1, &sampler);
	if (bilinear) {
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	if (tau)
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	else
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	if (tav)
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
	else
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, 0);
	glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, 6);

	int anisotropy = theApp.GetConfig("MaxAnisotropy", 0);
	if (GLLoader::found_GL_EXT_texture_filter_anisotropic && anisotropy && aniso)
		glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)anisotropy);

	GL_POP();
	return sampler;
}

void GSDeviceOGL::InitPrimDateTexture(GSTexture* rt)
{
	const GSVector2i& rtsize = rt->GetSize();

	// Create a texture to avoid the useless clean@0
	if (m_date.t == NULL)
		m_date.t = CreateTexture(rtsize.x, rtsize.y, GL_R32I);

	// Clean with the max signed value
	ClearRenderTarget_i(m_date.t, 0x7FFFFFFF);

	glBindImageTexture(2, static_cast<GSTextureOGL*>(m_date.t)->GetID(), 0, false, 0, GL_READ_WRITE, GL_R32I);
#ifdef ENABLE_OGL_DEBUG
	// Help to see the texture in apitrace
	PSSetShaderResource(2, m_date.t);
#endif
}

void GSDeviceOGL::RecycleDateTexture()
{
	if (m_date.t) {
		//static_cast<GSTextureOGL*>(m_date.t)->Save(format("/tmp/date_adv_%04ld.csv", s_n));

		Recycle(m_date.t);
		m_date.t = NULL;
	}
}

void GSDeviceOGL::Barrier(GLbitfield b)
{
	glMemoryBarrier(b);
}

/* Note: must be here because tfx_glsl is static */
GLuint GSDeviceOGL::CompileVS(VSSelector sel)
{
	std::string macro = format("#define VS_BPPZ %d\n", sel.bppz)
		+ format("#define VS_WILDHACK %d\n", sel.wildhack)
		;

	return m_shader->Compile("tfx_vgs.glsl", "vs_main", GL_VERTEX_SHADER, tfx_vgs_glsl, macro);
}

/* Note: must be here because tfx_glsl is static */
GLuint GSDeviceOGL::CompileGS(GSSelector sel)
{
	std::string macro = format("#define GS_POINT %d\n", sel.point);

	return m_shader->Compile("tfx_vgs.glsl", "gs_main", GL_GEOMETRY_SHADER, tfx_vgs_glsl, macro);
}

/* Note: must be here because tfx_glsl is static */
GLuint GSDeviceOGL::CompilePS(PSSelector sel)
{
	std::string macro = format("#define PS_FST %d\n", sel.fst)
		+ format("#define PS_WMS %d\n", sel.wms)
		+ format("#define PS_WMT %d\n", sel.wmt)
		+ format("#define PS_TEX_FMT %d\n", sel.tex_fmt)
		+ format("#define PS_DFMT %d\n", sel.dfmt)
		+ format("#define PS_AEM %d\n", sel.aem)
		+ format("#define PS_TFX %d\n", sel.tfx)
		+ format("#define PS_TCC %d\n", sel.tcc)
		+ format("#define PS_ATST %d\n", sel.atst)
		+ format("#define PS_FOG %d\n", sel.fog)
		+ format("#define PS_CLR1 %d\n", sel.clr1)
		+ format("#define PS_FBA %d\n", sel.fba)
		+ format("#define PS_LTF %d\n", sel.ltf)
		+ format("#define PS_COLCLIP %d\n", sel.colclip)
		+ format("#define PS_DATE %d\n", sel.date)
		+ format("#define PS_TCOFFSETHACK %d\n", sel.tcoffsethack)
		//+ format("#define PS_POINT_SAMPLER %d\n", sel.point_sampler)
		+ format("#define PS_BLEND_A %d\n", sel.blend_a)
		+ format("#define PS_BLEND_B %d\n", sel.blend_b)
		+ format("#define PS_BLEND_C %d\n", sel.blend_c)
		+ format("#define PS_BLEND_D %d\n", sel.blend_d)
		+ format("#define PS_IIP %d\n", sel.iip)
		+ format("#define PS_SHUFFLE %d\n", sel.shuffle)
		+ format("#define PS_READ_BA %d\n", sel.read_ba)
		+ format("#define PS_WRITE_RG %d\n", sel.write_rg)
		+ format("#define PS_FBMASK %d\n", sel.fbmask)
		+ format("#define PS_HDR %d\n", sel.hdr)
		+ format("#define PS_PABE %d\n", sel.pabe);
		;

	return m_shader->Compile("tfx.glsl", "ps_main", GL_FRAGMENT_SHADER, tfx_fs_all_glsl, macro);
}

void GSDeviceOGL::SelfShaderTest()
{
#define RUN_TEST \
	do { \
		GLuint p = CompilePS(sel); \
		nb_shader++; \
		perf += m_shader->DumpAsm(file, p); \
	} while(0);

#define PRINT_TEST(s) \
	do { \
		fprintf(stderr, "%s %d instructions for %d shaders (mean of %4.2f)\n", \
				s, perf, nb_shader, (float)perf/(float)nb_shader); \
		all += perf; \
		perf = 0; \
		nb_shader = 0; \
	} while(0);

	int nb_shader = 0;
	int perf = 0;
	int all = 0;
	// Test: SW blending
	for (int colclip = 0; colclip < 2; colclip++) {
		for (int fmt = 0; fmt < 3; fmt++) {
			for (int i = 0; i < 3; i++) {
				PSSelector sel;
				sel.atst = 1;
				sel.tfx = 4;

				int ib = (i + 1) % 3;
				sel.blend_a = i;
				sel.blend_b = ib;;
				sel.blend_c = i;
				sel.blend_d = i;
				sel.colclip = colclip;
				sel.dfmt    = fmt;

				std::string file = format("Shader_Blend_%d_%d_%d_%d__Cclip_%d__Dfmt_%d.glsl.asm",
						i, ib, i, i, colclip, fmt);
				RUN_TEST;
			}
		}
	}
	PRINT_TEST("Blend");

	// Test: alpha test
	for (int atst = 0; atst < 8; atst++) {
		PSSelector sel;
		sel.tfx = 4;

		sel.atst = atst;
		std::string file = format("Shader_Atst_%d.glsl.asm", atst);
		RUN_TEST;
	}
	PRINT_TEST("Alpha Tst");

	// Test: fbmask/fog/shuffle/read_ba
	for (int read_ba = 0; read_ba < 2; read_ba++) {
		PSSelector sel;
		sel.tfx = 4;
		sel.atst = 1;

		sel.fog = 1;
		sel.fbmask = 1;
		sel.shuffle = 1;
		sel.read_ba = read_ba;

		std::string file = format("Shader_Fog__Fbmask__Shuffle__Read_ba_%d.glsl.asm", read_ba);
		RUN_TEST;
	}
	PRINT_TEST("Fbmask/fog/shuffle/read_ba");

	// Test: Date
	for (int date = 1; date < 7; date++) {
		PSSelector sel;
		sel.tfx = 4;
		sel.atst = 1;

		sel.date = date;
		std::string file = format("Shader_Date_%d.glsl.asm", date);
		RUN_TEST;
	}
	PRINT_TEST("Date");

	// Test: FBA
	for (int fmt = 0; fmt < 3; fmt++) {
		PSSelector sel;
		sel.tfx = 4;
		sel.atst = 1;

		sel.fba = 1;
		sel.dfmt = fmt;
		sel.clr1 = 1;
		std::string file = format("Shader_Fba__Clr1__Dfmt_%d.glsl.asm", fmt);
		RUN_TEST;
	}
	PRINT_TEST("Fba/Clr1/Dfmt");

	// Test: Fst/Tc/IIP
	{
		PSSelector sel;
		sel.tfx = 1;
		sel.atst = 1;

		sel.fst = 0;
		sel.iip = 1;
		sel.tcoffsethack = 1;

		std::string file = format("Shader_Fst__TC__Iip.glsl.asm");
		RUN_TEST;
	}
	PRINT_TEST("Fst/Tc/IIp");

	// Test: tfx/tcc
	for (int tfx = 0; tfx < 5; tfx++) {
		for (int tcc = 0; tcc < 2; tcc++) {
			PSSelector sel;
			sel.atst = 1;
			sel.fst = 1;

			sel.tfx = tfx;
			sel.tcc = tcc;
			std::string file = format("Shader_Tfx_%d__Tcc_%d.glsl.asm", tfx, tcc);
			RUN_TEST;
		}
	}
	PRINT_TEST("Tfx/Tcc");

	// Test: Texture Sampling
	for (int fmt = 0; fmt < 16; fmt++) {
		if ((fmt & 3) == 3) continue;

		for (int ltf = 0; ltf < 2; ltf++) {
			for (int aem = 0; aem < 2; aem++) {
				for (int wms = 1; wms < 4; wms++) {
					for (int wmt = 1; wmt < 4; wmt++) {
						PSSelector sel;
						sel.atst = 1;
						sel.tfx  = 1;
						sel.tcc  = 1;
						sel.fst = 1;

						sel.ltf     = ltf;
						sel.aem     = aem;
						sel.tex_fmt = fmt;
						sel.wms     = wms;
						sel.wmt     = wmt;
						std::string file = format("Shader_Ltf_%d__Aem_%d__TFmt_%d__Wms_%d__Wmt_%d.glsl.asm",
								ltf, aem, fmt, wms, wmt);
						RUN_TEST;
					}
				}
			}
		}
	}
	PRINT_TEST("Texture Sampling");

	fprintf(stderr, "\nTotal %d\n", all);

#undef RUN_TEST
#undef PRINT_TEST
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
GSTexture* GSDeviceOGL::CopyOffscreen(GSTexture* src, const GSVector4& sRect, int w, int h, int format, int ps_shader)
{
	if (format == 0)
		format = GL_RGBA8;

	ASSERT(src);
	ASSERT(format == GL_RGBA8 || format == GL_R16UI || format == GL_R32UI);

	GSTexture* dst = CreateOffscreen(w, h, format);

	GSVector4 dRect(0, 0, w, h);

	StretchRect(src, sRect, dst, dRect, m_convert.ps[ps_shader]);

	return dst;
}

// Copy a sub part of texture (same as below but force a conversion)
void GSDeviceOGL::CopyRectConv(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, bool at_origin)
{
	ASSERT(sTex && dTex);
	if (!(sTex && dTex))
		return;

	const GLuint& sid = static_cast<GSTextureOGL*>(sTex)->GetID();
	const GLuint& did = static_cast<GSTextureOGL*>(dTex)->GetID();

	GL_PUSH(format("CopyRectConv from %d to %d", sid, did).c_str());

	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);

	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sid, 0);
	if (at_origin)
		glCopyTextureSubImage2D(did, GL_TEX_LEVEL_0, 0, 0, r.x, r.y, r.width(), r.height());
	else
		glCopyTextureSubImage2D(did, GL_TEX_LEVEL_0, r.x, r.y, r.x, r.y, r.width(), r.height());

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	GL_POP();
}

// Copy a sub part of a texture into another
void GSDeviceOGL::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r)
{
	ASSERT(sTex && dTex);
	if (!(sTex && dTex))
		return;

	const GLuint& sid = static_cast<GSTextureOGL*>(sTex)->GetID();
	const GLuint& did = static_cast<GSTextureOGL*>(dTex)->GetID();

	GL_PUSH("CopyRect from %d to %d", sid, did);

	if (GLLoader::found_GL_ARB_copy_image) {
		glCopyImageSubData( sid, GL_TEXTURE_2D,
				0, r.x, r.y, 0,
				did, GL_TEXTURE_2D,
				0, 0, 0, 0,
				r.width(), r.height(), 1);
	} else {
		// Slower copy (conversion is done)
		CopyRectConv(sTex, dTex, r, true);
	}

	GL_POP();
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, int shader, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[shader], linear);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, ps, m_NO_BLEND, linear);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, int bs, bool linear)
{
	if(!sTex || !dTex)
	{
		ASSERT(0);
		return;
	}

	bool draw_in_depth = (ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT32] || ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT24] ||
		ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT16] || ps == m_convert.ps[ShaderConvert_RGB5A1_TO_FLOAT16]);

	// Performance optimization. It might be faster to use a framebuffer blit for standard case
	// instead to emulate it with shader
	// see https://www.opengl.org/wiki/Framebuffer#Blitting

	GL_PUSH("StretchRect from %d to %d", sTex->GetID(), dTex->GetID());

	// ************************************
	// Init
	// ************************************

	BeginScene();

	GSVector2i ds = dTex->GetSize();

	m_shader->BindPipeline(ps);

	// ************************************
	// om
	// ************************************

	if (draw_in_depth)
		OMSetDepthStencilState(m_convert.dss_write);
	else
		OMSetDepthStencilState(m_convert.dss);

	if (draw_in_depth)
		OMSetRenderTargets(NULL, dTex);
	else
		OMSetRenderTargets(dTex, NULL);

	OMSetBlendState(bs);
	OMSetColorMaskState();

	// ************************************
	// ia
	// ************************************


	// Original code from DX
	float left = dRect.x * 2 / ds.x - 1.0f;
	float right = dRect.z * 2 / ds.x - 1.0f;
#if 0
	float top = 1.0f - dRect.y * 2 / ds.y;
	float bottom = 1.0f - dRect.w * 2 / ds.y;
#else
	// Opengl get some issues with the coordinate
	// I flip top/bottom to fix scaling of the internal resolution
	float top = -1.0f + dRect.y * 2 / ds.y;
	float bottom = -1.0f + dRect.w * 2 / ds.y;
#endif

	// Flip y axis only when we render in the backbuffer
	// By default everything is render in the wrong order (ie dx).
	// 1/ consistency between several pass rendering (interlace)
	// 2/ in case some GSdx code expect thing in dx order.
	// Only flipping the backbuffer is transparent (I hope)...
	GSVector4 flip_sr = sRect;
	if (static_cast<GSTextureOGL*>(dTex)->IsBackbuffer()) {
		flip_sr.y = sRect.w;
		flip_sr.w = sRect.y;
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

	PSSetShaderResource(0, sTex);
	PSSetSamplerState(linear ? m_convert.ln : m_convert.pt);

	// ************************************
	// Draw
	// ************************************
	DrawPrimitive();

	// ************************************
	// End
	// ************************************

	EndScene();

	GL_POP();
}

void GSDeviceOGL::DoMerge(GSTexture* sTex[2], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, bool slbg, bool mmod, const GSVector4& c)
{
	GL_PUSH("DoMerge");

	OMSetColorMaskState();

	ClearRenderTarget(dTex, c);

	if(sTex[1] && !slbg)
	{
		StretchRect(sTex[1], sRect[1], dTex, dRect[1], m_merge_obj.ps[0]);
	}

	if(sTex[0])
	{
		m_merge_obj.cb->upload(&c.v);

		StretchRect(sTex[0], sRect[0], dTex, dRect[0], m_merge_obj.ps[mmod ? 1 : 0], m_MERGE_BLEND);
	}

	GL_POP();
}

void GSDeviceOGL::DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset)
{
	GL_PUSH("DoInterlace");

	OMSetColorMaskState();

	GSVector4 s = GSVector4(dTex->GetSize());

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0.0f, yoffset, s.x, s.y + yoffset);

	InterlaceConstantBuffer cb;

	cb.ZrH = GSVector2(0, 1.0f / s.y);
	cb.hH = s.y / 2;

	m_interlace.cb->upload(&cb);

	StretchRect(sTex, sRect, dTex, dRect, m_interlace.ps[shader], linear);

	GL_POP();
}

void GSDeviceOGL::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	// Lazy compile
	if (!m_fxaa.ps) {
		if (!GLLoader::found_GL_ARB_gpu_shader5) { // GL4.0 extension
			return;
		}

		std::string fxaa_macro = "#define FXAA_GLSL_130 1\n";
		fxaa_macro += "#extension GL_ARB_gpu_shader5 : enable\n";
		GLuint ps = m_shader->Compile("fxaa.fx", "ps_main", GL_FRAGMENT_SHADER, fxaa_fx, fxaa_macro);
		m_fxaa.ps = m_shader->LinkPipeline(m_convert.vs, 0, ps);
	}

	GL_PUSH("DoFxaa");

	OMSetColorMaskState();

	GSVector2i s = dTex->GetSize();

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0, 0, s.x, s.y);

	StretchRect(sTex, sRect, dTex, dRect, m_fxaa.ps, true);

	GL_POP();
}

void GSDeviceOGL::DoExternalFX(GSTexture* sTex, GSTexture* dTex)
{
	// Lazy compile
	if (!m_shaderfx.ps) {
		if (!GLLoader::found_GL_ARB_gpu_shader5) { // GL4.0 extension
			return;
		}

		std::string   config_name(theApp.GetConfig("shaderfx_conf", "dummy.ini"));
		std::ifstream fconfig(config_name);
		std::stringstream config;
		if (fconfig.good())
			config << fconfig.rdbuf();
		else
			fprintf(stderr, "Warning failed to load '%s'. External Shader might be wrongly configured\n", config_name.c_str());

		std::string   shader_name(theApp.GetConfig("shaderfx_glsl", "dummy.glsl"));
		std::ifstream fshader(shader_name);
		std::stringstream shader;
		if (!fshader.good()) {
			fprintf(stderr, "Error failed to load '%s'. External Shader will be disabled !\n", shader_name.c_str());
			return;
		}
		shader << fshader.rdbuf();


		m_shaderfx.cb = new GSUniformBufferOGL(g_fx_cb_index, sizeof(ExternalFXConstantBuffer));
		GLuint ps = m_shader->Compile("Extra", "ps_main", GL_FRAGMENT_SHADER, shader.str().c_str(), config.str());
		m_shaderfx.ps = m_shader->LinkPipeline(m_convert.vs, 0, ps);
	}

	GL_PUSH("DoExternalFX");

	OMSetColorMaskState();

	GSVector2i s = dTex->GetSize();

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0, 0, s.x, s.y);

	ExternalFXConstantBuffer cb;

	cb.xyFrame = GSVector2(s.x, s.y);
	cb.rcpFrame = GSVector4(1.0f / s.x, 1.0f / s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_shaderfx.cb->upload(&cb);

	StretchRect(sTex, sRect, dTex, dRect, m_shaderfx.ps, true);

	GL_POP();
}

void GSDeviceOGL::DoShadeBoost(GSTexture* sTex, GSTexture* dTex)
{
	GL_PUSH("DoShadeBoost");

	OMSetColorMaskState();

	GSVector2i s = dTex->GetSize();

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0, 0, s.x, s.y);

	ShadeBoostConstantBuffer cb;

	cb.rcpFrame = GSVector4(1.0f / s.x, 1.0f / s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_shadeboost.cb->upload(&cb);

	StretchRect(sTex, sRect, dTex, dRect, m_shadeboost.ps, true);

	GL_POP();
}

void GSDeviceOGL::SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm)
{
	GL_PUSH("DATE First Pass");

	// sfex3 (after the capcom logo), vf4 (first menu fading in), ffxii shadows, rumble roses shadows, persona4 shadows

	BeginScene();

	ClearStencil(ds, 0);

	m_shader->BindPipeline(m_convert.ps[datm ? ShaderConvert_DATM_1 : ShaderConvert_DATM_0]);

	// om

	OMSetDepthStencilState(m_date.dss);
	if (GLState::blend) {
		glDisable(GL_BLEND);
	}
	OMSetRenderTargets(NULL, ds, &GLState::scissor);

	// ia

	IASetVertexBuffer(vertices, 4);
	IASetPrimitiveTopology(GL_TRIANGLE_STRIP);


	// Texture

	PSSetShaderResource(0, rt);
	PSSetSamplerState(m_convert.pt);

	DrawPrimitive();

	if (GLState::blend) {
		glEnable(GL_BLEND);
	}

	EndScene();

	GL_POP();
}

void GSDeviceOGL::EndScene()
{
	m_va->EndScene();
}

void GSDeviceOGL::IASetVertexBuffer(const void* vertices, size_t count)
{
	m_va->UploadVB(vertices, count);
}

void GSDeviceOGL::IASetIndexBuffer(const void* index, size_t count)
{
	m_va->UploadIB(index, count);
}

void GSDeviceOGL::IASetPrimitiveTopology(GLenum topology)
{
	m_va->SetTopology(topology);
}

void GSDeviceOGL::PSSetShaderResource(int i, GSTexture* sr)
{
	ASSERT(i < (int)countof(GLState::tex_unit));
	// Note: Nvidia debgger doesn't support the id 0 (ie the NULL texture)
	if (sr) {
		GLuint id = static_cast<GSTextureOGL*>(sr)->GetID();
		if (GLState::tex_unit[i] != id) {
			GLState::tex_unit[i] = id;
			glBindTextureUnit(i, id);
		}
	}
}

void GSDeviceOGL::PSSetShaderResources(GSTexture* sr0, GSTexture* sr1)
{
	PSSetShaderResource(0, sr0);
	PSSetShaderResource(1, sr1);
}

void GSDeviceOGL::PSSetSamplerState(GLuint ss)
{
	if (GLState::ps_ss != ss) {
		GLState::ps_ss = ss;
		glBindSampler(0, ss);
	}
}

void GSDeviceOGL::OMAttachRt(GSTextureOGL* rt)
{
	GLuint id;
	if (rt) {
		rt->WasAttached();
		id = rt->GetID();
	} else {
		id = 0;
	}

	if (GLState::rt != id) {
		GLState::rt = id;
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);
	}
}

void GSDeviceOGL::OMAttachDs(GSTextureOGL* ds)
{
	GLuint id;
	if (ds) {
		ds->WasAttached();
		id = ds->GetID();
	} else {
		id = 0;
	}

	if (GLState::ds != id) {
		GLState::ds = id;
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, id, 0);
	}
}

void GSDeviceOGL::OMSetFBO(GLuint fbo)
{
	if (GLState::fbo != fbo) {
		GLState::fbo = fbo;
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
	}
}

void GSDeviceOGL::OMSetDepthStencilState(GSDepthStencilOGL* dss)
{
	dss->SetupDepth();
	dss->SetupStencil();
}

void GSDeviceOGL::OMSetColorMaskState(OMColorMaskSelector sel)
{
	if (sel.wrgba != GLState::wrgba) {
		GLState::wrgba = sel.wrgba;

		glColorMaski(0, sel.wr, sel.wg, sel.wb, sel.wa);
	}
}

void GSDeviceOGL::OMSetBlendState(uint8 blend_index, uint8 blend_factor, bool is_blend_constant)
{
	if (blend_index) {
		if (!GLState::blend) {
			GLState::blend = true;
			glEnable(GL_BLEND);
		}

		if (is_blend_constant && GLState::bf != blend_factor) {
			GLState::bf = blend_factor;
			float bf = (float)blend_factor / 128.0f;
			gl_BlendColor(bf, bf, bf, bf);
		}

		const OGLBlend& b = m_blendMapOGL[blend_index];

		if (GLState::eq_RGB != b.op) {
			GLState::eq_RGB = b.op;
			if (glBlendEquationSeparateiARB)
				glBlendEquationSeparateiARB(0, b.op, GL_FUNC_ADD);
			else
				glBlendEquationSeparate(b.op, GL_FUNC_ADD);
		}

		if (GLState::f_sRGB != b.src || GLState::f_dRGB != b.dst) {
			GLState::f_sRGB = b.src;
			GLState::f_dRGB = b.dst;
			if (glBlendFuncSeparateiARB)
				glBlendFuncSeparateiARB(0, b.src, b.dst, GL_ONE, GL_ZERO);
			else
				glBlendFuncSeparate(b.src, b.dst, GL_ONE, GL_ZERO);
		}

	} else {
		if (GLState::blend) {
			GLState::blend = false;
			glDisable(GL_BLEND);
		}
	}
}

void GSDeviceOGL::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor)
{
	GSTextureOGL* RT = static_cast<GSTextureOGL*>(rt);
	GSTextureOGL* DS = static_cast<GSTextureOGL*>(ds);

	if (rt == NULL || !RT->IsBackbuffer()) {
		OMSetFBO(m_fbo);
		if (rt) {
			OMAttachRt(RT);
		} else {
			OMAttachRt();
		}

		// Note: it must be done after OMSetFBO
		if (ds)
			OMAttachDs(DS);
		else
			OMAttachDs();

	} else {
		// Render in the backbuffer
		OMSetFBO(0);
	}


	GSVector2i size = rt ? rt->GetSize() : ds ? ds->GetSize() : GLState::viewport;
	if(GLState::viewport != size)
	{
		GLState::viewport = size;
		// FIXME ViewportIndexedf or ViewportIndexedfv (GL4.1)
		glViewport(0, 0, size.x, size.y);
	}

	GSVector4i r = scissor ? *scissor : GSVector4i(size).zwxy();

	if(!GLState::scissor.eq(r))
	{
		GLState::scissor = r;
		// FIXME ScissorIndexedv (GL4.1)
		glScissor( r.x, r.y, r.width(), r.height() );
	}
}

void GSDeviceOGL::CheckDebugLog()
{
	if (!m_debug_gl_call) return;

	unsigned int count = 16; // max. num. of messages that will be read from the log
	int bufsize = 2048;
	unsigned int sources[16] = {};
	unsigned int types[16] = {};
	unsigned int ids[16]   = {};
	unsigned int severities[16] = {};
	int lengths[16] = {};
	char* messageLog = new char[bufsize];

	unsigned int retVal = glGetDebugMessageLogARB(count, bufsize, sources, types, ids, severities, lengths, messageLog);

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
}

// Note: used as a callback of DebugMessageCallback. Don't change the signature
void GSDeviceOGL::DebugOutputToFile(GLenum gl_source, GLenum gl_type, GLuint id, GLenum gl_severity, GLsizei gl_length, const GLchar *gl_message, const void* userParam)
{
	std::string message(gl_message, gl_length >= 0 ? gl_length : strlen(gl_message));
	std::string type, severity, source;
	static int sev_counter = 0;
	switch(gl_type) {
		case GL_DEBUG_TYPE_ERROR_ARB               : type = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB : type = "Deprecated bhv"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB  : type = "Undefined bhv"; break;
		case GL_DEBUG_TYPE_PORTABILITY_ARB         : type = "Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE_ARB         : type = "Perf"; break;
		case GL_DEBUG_TYPE_OTHER_ARB               : type = "Others"; break;
		case GL_DEBUG_TYPE_PUSH_GROUP              : return; // Don't print message injected by myself
		case GL_DEBUG_TYPE_POP_GROUP               : return; // Don't print message injected by myself
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

	#ifdef _DEBUG
	// Don't spam noisy information on the terminal
	if (gl_severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
		fprintf(stderr,"Type:%s\tID:%d\tSeverity:%s\tMessage:%s\n", type.c_str(), s_n, severity.c_str(), message.c_str());
	}
	#endif

	if (m_debug_gl_file)
		fprintf(m_debug_gl_file,"Type:%s\tID:%d\tSeverity:%s\tMessage:%s\n", type.c_str(), s_n, severity.c_str(), message.c_str());

#ifdef _DEBUG
	if (sev_counter >= 5) {
		// Close the file to flush the content on disk before exiting.
		if (m_debug_gl_file) {
			fclose(m_debug_gl_file);
			m_debug_gl_file = NULL;
		}
		ASSERT(0);
	}
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

// Special blending method table:
// # (tricky) => 1 * Cd + Cd * F => Use (Cd, F) as factor of color (1, Cd)
// * (bogus) => C * (1 + F ) + ... => factor is always bigger than 1 (except above case)
// ? => Cs * F + Cd => do the multiplication in shader and addition in blending unit. It is an optimization

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

#define D3DBLEND_SRCALPHA		GL_SRC1_ALPHA
#define D3DBLEND_INVSRCALPHA	GL_ONE_MINUS_SRC1_ALPHA

const int GSDeviceOGL::m_NO_BLEND = 0;
const int GSDeviceOGL::m_MERGE_BLEND = 3*3*3*3;

const GSDeviceOGL::OGLBlend GSDeviceOGL::m_blendMapOGL[3*3*3*3 + 1] =
{
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , // 0000: (Cs - Cs)*As + Cs ==> Cs
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ONE}            , // 0001: (Cs - Cs)*As + Cd ==> Cd
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ZERO}           , // 0002: (Cs - Cs)*As +  0 ==> 0
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , // 0010: (Cs - Cs)*Ad + Cs ==> Cs
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ONE}            , // 0011: (Cs - Cs)*Ad + Cd ==> Cd
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ZERO}           , // 0012: (Cs - Cs)*Ad +  0 ==> 0
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , // 0020: (Cs - Cs)*F  + Cs ==> Cs
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ONE}            , // 0021: (Cs - Cs)*F  + Cd ==> Cd
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ZERO}           , // 0022: (Cs - Cs)*F  +  0 ==> 0
	{ BLEND_A_MAX                , D3DBLENDOP_SUBTRACT    , D3DBLEND_ONE            , D3DBLEND_SRCALPHA}       , //*0100: (Cs - Cd)*As + Cs ==> Cs*(As + 1) - Cd*As
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_SRCALPHA       , D3DBLEND_INVSRCALPHA}    , // 0101: (Cs - Cd)*As + Cd ==> Cs*As + Cd*(1 - As)
	{ 0                          , D3DBLENDOP_SUBTRACT    , D3DBLEND_SRCALPHA       , D3DBLEND_SRCALPHA}       , // 0102: (Cs - Cd)*As +  0 ==> Cs*As - Cd*As
	{ BLEND_A_MAX                , D3DBLENDOP_SUBTRACT    , D3DBLEND_ONE            , D3DBLEND_DESTALPHA}      , //*0110: (Cs - Cd)*Ad + Cs ==> Cs*(Ad + 1) - Cd*Ad
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_DESTALPHA      , D3DBLEND_INVDESTALPHA}   , // 0111: (Cs - Cd)*Ad + Cd ==> Cs*Ad + Cd*(1 - Ad)
	{ 0                          , D3DBLENDOP_SUBTRACT    , D3DBLEND_DESTALPHA      , D3DBLEND_DESTALPHA}      , // 0112: (Cs - Cd)*Ad +  0 ==> Cs*Ad - Cd*Ad
	{ BLEND_A_MAX                , D3DBLENDOP_SUBTRACT    , D3DBLEND_ONE            , D3DBLEND_BLENDFACTOR}    , //*0120: (Cs - Cd)*F  + Cs ==> Cs*(F + 1) - Cd*F
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_BLENDFACTOR    , D3DBLEND_INVBLENDFACTOR} , // 0121: (Cs - Cd)*F  + Cd ==> Cs*F + Cd*(1 - F)
	{ 0                          , D3DBLENDOP_SUBTRACT    , D3DBLEND_BLENDFACTOR    , D3DBLEND_BLENDFACTOR}    , // 0122: (Cs - Cd)*F  +  0 ==> Cs*F - Cd*F
	{ BLEND_NO_BAR | BLEND_A_MAX , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , //*0200: (Cs -  0)*As + Cs ==> Cs*(As + 1)
	{ BLEND_ACCU                 , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ONE}            , //?0201: (Cs -  0)*As + Cd ==> Cs*As + Cd
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_SRCALPHA       , D3DBLEND_ZERO}           , // 0202: (Cs -  0)*As +  0 ==> Cs*As
	{ BLEND_A_MAX                , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , //*0210: (Cs -  0)*Ad + Cs ==> Cs*(Ad + 1)
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_DESTALPHA      , D3DBLEND_ONE}            , // 0211: (Cs -  0)*Ad + Cd ==> Cs*Ad + Cd
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_DESTALPHA      , D3DBLEND_ZERO}           , // 0212: (Cs -  0)*Ad +  0 ==> Cs*Ad
	{ BLEND_NO_BAR | BLEND_A_MAX , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , //*0220: (Cs -  0)*F  + Cs ==> Cs*(F + 1)
	{ BLEND_ACCU                 , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ONE}            , //?0221: (Cs -  0)*F  + Cd ==> Cs*F + Cd
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_BLENDFACTOR    , D3DBLEND_ZERO}           , // 0222: (Cs -  0)*F  +  0 ==> Cs*F
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_INVSRCALPHA    , D3DBLEND_SRCALPHA}       , // 1000: (Cd - Cs)*As + Cs ==> Cd*As + Cs*(1 - As)
	{ BLEND_A_MAX                , D3DBLENDOP_REVSUBTRACT , D3DBLEND_SRCALPHA       , D3DBLEND_ONE}            , //*1001: (Cd - Cs)*As + Cd ==> Cd*(As + 1) - Cs*As
	{ 0                          , D3DBLENDOP_REVSUBTRACT , D3DBLEND_SRCALPHA       , D3DBLEND_SRCALPHA}       , // 1002: (Cd - Cs)*As +  0 ==> Cd*As - Cs*As
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_INVDESTALPHA   , D3DBLEND_DESTALPHA}      , // 1010: (Cd - Cs)*Ad + Cs ==> Cd*Ad + Cs*(1 - Ad)
	{ BLEND_A_MAX                , D3DBLENDOP_REVSUBTRACT , D3DBLEND_DESTALPHA      , D3DBLEND_ONE}            , //*1011: (Cd - Cs)*Ad + Cd ==> Cd*(Ad + 1) - Cs*Ad
	{ 0                          , D3DBLENDOP_REVSUBTRACT , D3DBLEND_DESTALPHA      , D3DBLEND_DESTALPHA}      , // 1012: (Cd - Cs)*Ad +  0 ==> Cd*Ad - Cs*Ad
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_INVBLENDFACTOR , D3DBLEND_BLENDFACTOR}    , // 1020: (Cd - Cs)*F  + Cs ==> Cd*F + Cs*(1 - F)
	{ BLEND_A_MAX                , D3DBLENDOP_REVSUBTRACT , D3DBLEND_BLENDFACTOR    , D3DBLEND_ONE}            , //*1021: (Cd - Cs)*F  + Cd ==> Cd*(F + 1) - Cs*F
	{ 0                          , D3DBLENDOP_REVSUBTRACT , D3DBLEND_BLENDFACTOR    , D3DBLEND_BLENDFACTOR}    , // 1022: (Cd - Cs)*F  +  0 ==> Cd*F - Cs*F
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , // 1100: (Cd - Cd)*As + Cs ==> Cs
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ONE}            , // 1101: (Cd - Cd)*As + Cd ==> Cd
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ZERO}           , // 1102: (Cd - Cd)*As +  0 ==> 0
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , // 1110: (Cd - Cd)*Ad + Cs ==> Cs
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ONE}            , // 1111: (Cd - Cd)*Ad + Cd ==> Cd
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ZERO}           , // 1112: (Cd - Cd)*Ad +  0 ==> 0
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , // 1120: (Cd - Cd)*F  + Cs ==> Cs
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ONE}            , // 1121: (Cd - Cd)*F  + Cd ==> Cd
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ZERO}           , // 1122: (Cd - Cd)*F  +  0 ==> 0
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_SRCALPHA}       , // 1200: (Cd -  0)*As + Cs ==> Cs + Cd*As
	{ BLEND_C_CLR                , D3DBLENDOP_ADD         , D3DBLEND_DESTCOLOR      , D3DBLEND_SRCALPHA}       , //#1201: (Cd -  0)*As + Cd ==> Cd*(1 + As) // ffxii main menu background
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_SRCALPHA}       , // 1202: (Cd -  0)*As +  0 ==> Cd*As
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_DESTALPHA}      , // 1210: (Cd -  0)*Ad + Cs ==> Cs + Cd*Ad
	{ BLEND_C_CLR                , D3DBLENDOP_ADD         , D3DBLEND_DESTCOLOR      , D3DBLEND_DESTALPHA}      , //#1211: (Cd -  0)*Ad + Cd ==> Cd*(1 + Ad)
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_DESTALPHA}      , // 1212: (Cd -  0)*Ad +  0 ==> Cd*Ad
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_BLENDFACTOR}    , // 1220: (Cd -  0)*F  + Cs ==> Cs + Cd*F
	{ BLEND_C_CLR                , D3DBLENDOP_ADD         , D3DBLEND_DESTCOLOR      , D3DBLEND_BLENDFACTOR}    , //#1221: (Cd -  0)*F  + Cd ==> Cd*(1 + F)
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_BLENDFACTOR}    , // 1222: (Cd -  0)*F  +  0 ==> Cd*F
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_INVSRCALPHA    , D3DBLEND_ZERO}           , // 2000: (0  - Cs)*As + Cs ==> Cs*(1 - As)
	{ BLEND_ACCU                 , D3DBLENDOP_REVSUBTRACT , D3DBLEND_ONE            , D3DBLEND_ONE}            , // 2001: (0  - Cs)*As + Cd ==> Cd - Cs*As
	{ BLEND_NO_BAR               , D3DBLENDOP_REVSUBTRACT , D3DBLEND_SRCALPHA       , D3DBLEND_ZERO}           , // 2002: (0  - Cs)*As +  0 ==> 0 - Cs*As
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_INVDESTALPHA   , D3DBLEND_ZERO}           , // 2010: (0  - Cs)*Ad + Cs ==> Cs*(1 - Ad)
	{ 0                          , D3DBLENDOP_REVSUBTRACT , D3DBLEND_DESTALPHA      , D3DBLEND_ONE}            , // 2011: (0  - Cs)*Ad + Cd ==> Cd - Cs*Ad
	{ 0                          , D3DBLENDOP_REVSUBTRACT , D3DBLEND_DESTALPHA      , D3DBLEND_ZERO}           , // 2012: (0  - Cs)*Ad +  0 ==> 0 - Cs*Ad
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_INVBLENDFACTOR , D3DBLEND_ZERO}           , // 2020: (0  - Cs)*F  + Cs ==> Cs*(1 - F)
	{ BLEND_ACCU                 , D3DBLENDOP_REVSUBTRACT , D3DBLEND_ONE            , D3DBLEND_ONE}            , // 2021: (0  - Cs)*F  + Cd ==> Cd - Cs*F
	{ BLEND_NO_BAR               , D3DBLENDOP_REVSUBTRACT , D3DBLEND_BLENDFACTOR    , D3DBLEND_ZERO}           , // 2022: (0  - Cs)*F  +  0 ==> 0 - Cs*F
	{ 0                          , D3DBLENDOP_SUBTRACT    , D3DBLEND_ONE            , D3DBLEND_SRCALPHA}       , // 2100: (0  - Cd)*As + Cs ==> Cs - Cd*As
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_INVSRCALPHA}    , // 2101: (0  - Cd)*As + Cd ==> Cd*(1 - As)
	{ 0                          , D3DBLENDOP_SUBTRACT    , D3DBLEND_ZERO           , D3DBLEND_SRCALPHA}       , // 2102: (0  - Cd)*As +  0 ==> 0 - Cd*As
	{ 0                          , D3DBLENDOP_SUBTRACT    , D3DBLEND_ONE            , D3DBLEND_DESTALPHA}      , // 2110: (0  - Cd)*Ad + Cs ==> Cs - Cd*Ad
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_INVDESTALPHA}   , // 2111: (0  - Cd)*Ad + Cd ==> Cd*(1 - Ad)
	{ 0                          , D3DBLENDOP_SUBTRACT    , D3DBLEND_ONE            , D3DBLEND_DESTALPHA}      , // 2112: (0  - Cd)*Ad +  0 ==> 0 - Cd*Ad
	{ 0                          , D3DBLENDOP_SUBTRACT    , D3DBLEND_ONE            , D3DBLEND_BLENDFACTOR}    , // 2120: (0  - Cd)*F  + Cs ==> Cs - Cd*F
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_INVBLENDFACTOR} , // 2121: (0  - Cd)*F  + Cd ==> Cd*(1 - F)
	{ 0                          , D3DBLENDOP_SUBTRACT    , D3DBLEND_ONE            , D3DBLEND_BLENDFACTOR}    , // 2122: (0  - Cd)*F  +  0 ==> 0 - Cd*F
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , // 2200: (0  -  0)*As + Cs ==> Cs
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ONE}            , // 2201: (0  -  0)*As + Cd ==> Cd
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ZERO}           , // 2202: (0  -  0)*As +  0 ==> 0
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , // 2210: (0  -  0)*Ad + Cs ==> Cs
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ONE}            , // 2211: (0  -  0)*Ad + Cd ==> Cd
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ZERO}           , // 2212: (0  -  0)*Ad +  0 ==> 0
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ONE            , D3DBLEND_ZERO}           , // 2220: (0  -  0)*F  + Cs ==> Cs
	{ 0                          , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ONE}            , // 2221: (0  -  0)*F  + Cd ==> Cd
	{ BLEND_NO_BAR               , D3DBLENDOP_ADD         , D3DBLEND_ZERO           , D3DBLEND_ZERO}           , // 2222: (0  -  0)*F  +  0 ==> 0
	{ 0                          , D3DBLENDOP_ADD         , GL_SRC_ALPHA            , GL_ONE_MINUS_SRC_ALPHA}  , // extra for merge operation
};
