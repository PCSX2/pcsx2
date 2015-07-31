/*
 *	Copyright (C) 2011-2011 Gregory hainaut
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
#include "GSTables.h"

static const uint32 g_vs_cb_index = 20;
static const uint32 g_ps_cb_index = 21;
static const uint32 g_gs_cb_index = 22;

void GSDeviceOGL::CreateTextureFX()
{
	GL_PUSH("CreateTextureFX");

	m_vs_cb = new GSUniformBufferOGL(g_vs_cb_index, sizeof(VSConstantBuffer));
	m_ps_cb = new GSUniformBufferOGL(g_ps_cb_index, sizeof(PSConstantBuffer));

	// warning 1 sampler by image unit. So you cannot reuse m_ps_ss...
	m_palette_ss = CreateSampler(false, false, false);
	gl_BindSampler(1, m_palette_ss);

	// Pre compile all Geometry & Vertex Shader
	// It might cost a seconds at startup but it would reduce benchmark pollution
	for (uint32 key = 0; key < countof(m_gs); key++) {
		GSSelector sel(key);
		if (sel.point == sel.sprite)
			m_gs[key] = 0;
		else
			m_gs[key] = CompileGS(GSSelector(key));
	}

	for (uint32 key = 0; key < countof(m_vs); key++) {
		// wildhack is only useful if both TME and FST are enabled.
		VSSelector sel(key);
		if (sel.wildhack && (!sel.tme || !sel.fst))
			m_vs[key] = 0;
		else
			m_vs[key] = CompileVS(sel, !GLLoader::found_GL_ARB_clip_control);
	}

	// Enable all bits for stencil operations. Technically 1 bit is
	// enough but buffer is polluted with noise. Clear will be limited
	// to the mask.
	glStencilMask(0xFF);
	for (uint32 key = 0; key < countof(m_om_dss); key++) {
		m_om_dss[key] = CreateDepthStencil(OMDepthStencilSelector(key));
	}

	// Help to debug FS in apitrace
	m_apitrace = CompilePS(PSSelector());

	GL_POP();
}

GSDepthStencilOGL* GSDeviceOGL::CreateDepthStencil(OMDepthStencilSelector dssel)
{
	GSDepthStencilOGL* dss = new GSDepthStencilOGL();

	if (dssel.date)
	{
		dss->EnableStencil();
		dss->SetStencil(GL_EQUAL, GL_KEEP);
	}

	if(dssel.ztst != ZTST_ALWAYS || dssel.zwe)
	{
		static const GLenum ztst[] =
		{
			GL_NEVER,
			GL_ALWAYS,
			GL_GEQUAL,
			GL_GREATER
		};
		dss->EnableDepth();
		dss->SetDepth(ztst[dssel.ztst], dssel.zwe);
	}

	return dss;
}

void GSDeviceOGL::SetupCB(const VSConstantBuffer* vs_cb, const PSConstantBuffer* ps_cb)
{
	GL_PUSH("UBO");
	if(m_vs_cb_cache.Update(vs_cb)) {
		m_vs_cb->upload(vs_cb);
	}

	if(m_ps_cb_cache.Update(ps_cb)) {
		m_ps_cb->upload(ps_cb);
	}
	GL_POP();
}

void GSDeviceOGL::SetupVS(VSSelector sel)
{
	m_shader->VS(m_vs[sel]);
}

void GSDeviceOGL::SetupGS(GSSelector sel)
{
	m_shader->GS(m_gs[sel]);
}

void GSDeviceOGL::SetupPS(PSSelector sel)
{
	// *************************************************************
	// Static
	// *************************************************************
	GLuint ps;
	auto i = m_ps.find(sel);

	if (i == m_ps.end()) {
		ps = CompilePS(sel);
		m_ps[sel] = ps;
	} else {
		ps = i->second;
	}

	// *************************************************************
	// Dynamic
	// *************************************************************
	m_shader->PS(ps);
}

void GSDeviceOGL::SetupSampler(PSSamplerSelector ssel)
{
	PSSetSamplerState(m_ps_ss[ssel]);
}

GLuint GSDeviceOGL::GetSamplerID(PSSamplerSelector ssel)
{
	return m_ps_ss[ssel];
}

GLuint GSDeviceOGL::GetPaletteSamplerID()
{
	return m_palette_ss;
}

void GSDeviceOGL::SetupOM(OMDepthStencilSelector dssel)
{
	GSDepthStencilOGL* dss = m_om_dss[dssel];

	OMSetDepthStencilState(dss, 1);
}
