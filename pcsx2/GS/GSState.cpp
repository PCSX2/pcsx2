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

#include "PrecompiledHeader.h"
#include "GSState.h"
#include "GSGL.h"
#include "GSUtil.h"
#include "common/StringUtil.h"

#include <algorithm> // clamp
#include <cfloat> // FLT_MAX
#include <fstream>
#include <iomanip> // Dump Verticles

int GSState::s_n = 0;

static __fi bool IsAutoFlushEnabled()
{
	return (GSConfig.Renderer == GSRendererType::SW) ? GSConfig.AutoFlushSW : GSConfig.UserHacks_AutoFlush;
}

static __fi bool IsFirstProvokingVertex()
{
	return (GSConfig.Renderer != GSRendererType::SW && !g_gs_device->Features().provoking_vertex_last);
}

GSState::GSState()
	: m_version(STATE_VERSION)
	, m_gsc(NULL)
	, m_skip(0)
	, m_skip_offset(0)
	, m_q(1.0f)
	, m_scanmask_used(false)
	, tex_flushed(true)
	, m_vt(this, IsFirstProvokingVertex())
	, m_regs(NULL)
	, m_crc(0)
	, m_options(0)
	, m_frameskip(0)
{
	// m_nativeres seems to be a hack. Unfortunately it impacts draw call number which make debug painful in the replayer.
	// Let's keep it disabled to ease debug.
	m_nativeres = GSConfig.UpscaleMultiplier == 1;
	m_mipmap = GSConfig.Mipmap;

	s_n = 0;
	s_dump = theApp.GetConfigB("dump");
	s_save = theApp.GetConfigB("save");
	s_savet = theApp.GetConfigB("savet");
	s_savez = theApp.GetConfigB("savez");
	s_savef = theApp.GetConfigB("savef");
	s_saven = theApp.GetConfigI("saven");
	s_savel = theApp.GetConfigI("savel");
	m_dump_root = "";
#if defined(__unix__)
	if (s_dump)
	{
		GSmkdir(root_hw.c_str());
		GSmkdir(root_sw.c_str());
	}
#endif

	m_crc_hack_level = GSConfig.CRCHack;
	if (m_crc_hack_level == CRCHackLevel::Automatic)
		m_crc_hack_level = GSUtil::GetRecommendedCRCHackLevel(GSConfig.Renderer);

	memset(&m_v, 0, sizeof(m_v));
	memset(&m_vertex, 0, sizeof(m_vertex));
	memset(&m_index, 0, sizeof(m_index));

	m_v.RGBAQ.Q = 1.0f;

	GrowVertexBuffer();

	m_sssize = 0;

	m_sssize += sizeof(m_version);
	m_sssize += sizeof(m_env.PRIM);
	m_sssize += sizeof(m_env.PRMODECONT);
	m_sssize += sizeof(m_env.TEXCLUT);
	m_sssize += sizeof(m_env.SCANMSK);
	m_sssize += sizeof(m_env.TEXA);
	m_sssize += sizeof(m_env.FOGCOL);
	m_sssize += sizeof(m_env.DIMX);
	m_sssize += sizeof(m_env.DTHE);
	m_sssize += sizeof(m_env.COLCLAMP);
	m_sssize += sizeof(m_env.PABE);
	m_sssize += sizeof(m_env.BITBLTBUF);
	m_sssize += sizeof(m_env.TRXDIR);
	m_sssize += sizeof(m_env.TRXPOS);
	m_sssize += sizeof(m_env.TRXREG);
	m_sssize += sizeof(m_env.TRXREG); // obsolete

	for (int i = 0; i < 2; i++)
	{
		m_sssize += sizeof(m_env.CTXT[i].XYOFFSET);
		m_sssize += sizeof(m_env.CTXT[i].TEX0);
		m_sssize += sizeof(m_env.CTXT[i].TEX1);
		m_sssize += sizeof(m_env.CTXT[i].CLAMP);
		m_sssize += sizeof(m_env.CTXT[i].MIPTBP1);
		m_sssize += sizeof(m_env.CTXT[i].MIPTBP2);
		m_sssize += sizeof(m_env.CTXT[i].SCISSOR);
		m_sssize += sizeof(m_env.CTXT[i].ALPHA);
		m_sssize += sizeof(m_env.CTXT[i].TEST);
		m_sssize += sizeof(m_env.CTXT[i].FBA);
		m_sssize += sizeof(m_env.CTXT[i].FRAME);
		m_sssize += sizeof(m_env.CTXT[i].ZBUF);
	}

	m_sssize += sizeof(m_v.RGBAQ);
	m_sssize += sizeof(m_v.ST);
	m_sssize += sizeof(m_v.UV);
	m_sssize += sizeof(m_v.FOG);
	m_sssize += sizeof(m_v.XYZ);
	m_sssize += sizeof(GIFReg); // obsolete

	m_sssize += sizeof(m_tr.x);
	m_sssize += sizeof(m_tr.y);
	m_sssize += m_mem.m_vmsize;
	m_sssize += (sizeof(m_path[0].tag) + sizeof(m_path[0].reg)) * std::size(m_path);
	m_sssize += sizeof(m_q);

	PRIM = &m_env.PRIM;
	//CSR->rREV = 0x20;
	m_env.PRMODECONT.AC = 1;
	m_last_prim.U32[0] = PRIM->U32[0];

	Reset(false);

	ResetHandlers();
}

GSState::~GSState()
{
	if (m_vertex.buff)
		_aligned_free(m_vertex.buff);
	if (m_index.buff)
		_aligned_free(m_index.buff);
}

void GSState::SetFrameSkip(int skip)
{
	if (m_frameskip == skip)
		return;

	m_frameskip = skip;

	if (skip)
	{
		m_fpGIFPackedRegHandlers[GIF_REG_XYZF2] = &GSState::GIFPackedRegHandlerNOP;
		m_fpGIFPackedRegHandlers[GIF_REG_XYZ2] = &GSState::GIFPackedRegHandlerNOP;
		m_fpGIFPackedRegHandlers[GIF_REG_XYZF3] = &GSState::GIFPackedRegHandlerNOP;
		m_fpGIFPackedRegHandlers[GIF_REG_XYZ3] = &GSState::GIFPackedRegHandlerNOP;

		m_fpGIFRegHandlers[GIF_A_D_REG_XYZF2] = &GSState::GIFRegHandlerNOP;
		m_fpGIFRegHandlers[GIF_A_D_REG_XYZ2] = &GSState::GIFRegHandlerNOP;
		m_fpGIFRegHandlers[GIF_A_D_REG_XYZF3] = &GSState::GIFRegHandlerNOP;
		m_fpGIFRegHandlers[GIF_A_D_REG_XYZ3] = &GSState::GIFRegHandlerNOP;

		m_fpGIFPackedRegHandlersC[GIF_REG_STQRGBAXYZF2] = &GSState::GIFPackedRegHandlerNOP;
		m_fpGIFPackedRegHandlersC[GIF_REG_STQRGBAXYZ2] = &GSState::GIFPackedRegHandlerNOP;
	}
	else
	{
		UpdateVertexKick();
	}
}

void GSState::Reset(bool hardware_reset)
{
	// FIXME: bios logo not shown cut in half after reset, missing graphics in GoW after first FMV
	if (hardware_reset)
		memset(m_mem.m_vm8, 0, m_mem.m_vmsize);
	memset(&m_path, 0, sizeof(m_path));
	memset(&m_v, 0, sizeof(m_v));

	m_env.Reset();

	PRIM = &m_env.PRIM;

	UpdateContext();

	UpdateVertexKick();

	m_env.UpdateDIMX();

	for (size_t i = 0; i < 2; i++)
	{
		m_env.CTXT[i].UpdateScissor();

		m_env.CTXT[i].offset.fb = m_mem.GetOffset(m_env.CTXT[i].FRAME.Block(), m_env.CTXT[i].FRAME.FBW, m_env.CTXT[i].FRAME.PSM);
		m_env.CTXT[i].offset.zb = m_mem.GetOffset(m_env.CTXT[i].ZBUF.Block(), m_env.CTXT[i].FRAME.FBW, m_env.CTXT[i].ZBUF.PSM);
		m_env.CTXT[i].offset.tex = m_mem.GetOffset(m_env.CTXT[i].TEX0.TBP0, m_env.CTXT[i].TEX0.TBW, m_env.CTXT[i].TEX0.PSM);
		m_env.CTXT[i].offset.fzb = m_mem.GetPixelOffset(m_env.CTXT[i].FRAME, m_env.CTXT[i].ZBUF);
		m_env.CTXT[i].offset.fzb4 = m_mem.GetPixelOffset4(m_env.CTXT[i].FRAME, m_env.CTXT[i].ZBUF);
	}

	UpdateScissor();

	m_vertex.head = 0;
	m_vertex.tail = 0;
	m_vertex.next = 0;
	m_index.tail = 0;
	m_scanmask_used = false;
}

template<bool auto_flush, bool index_swap>
void GSState::SetPrimHandlers()
{
#define SetHandlerXYZ(P, auto_flush, index_swap) \
	m_fpGIFPackedRegHandlerXYZ[P][0] = &GSState::GIFPackedRegHandlerXYZF2<P, 0, auto_flush, index_swap>; \
	m_fpGIFPackedRegHandlerXYZ[P][1] = &GSState::GIFPackedRegHandlerXYZF2<P, 1, auto_flush, index_swap>; \
	m_fpGIFPackedRegHandlerXYZ[P][2] = &GSState::GIFPackedRegHandlerXYZ2<P, 0, auto_flush, index_swap>; \
	m_fpGIFPackedRegHandlerXYZ[P][3] = &GSState::GIFPackedRegHandlerXYZ2<P, 1, auto_flush, index_swap>; \
	m_fpGIFRegHandlerXYZ[P][0] = &GSState::GIFRegHandlerXYZF2<P, 0, auto_flush, index_swap>; \
	m_fpGIFRegHandlerXYZ[P][1] = &GSState::GIFRegHandlerXYZF2<P, 1, auto_flush, index_swap>; \
	m_fpGIFRegHandlerXYZ[P][2] = &GSState::GIFRegHandlerXYZ2<P, 0, auto_flush, index_swap>; \
	m_fpGIFRegHandlerXYZ[P][3] = &GSState::GIFRegHandlerXYZ2<P, 1, auto_flush, index_swap>; \
	m_fpGIFPackedRegHandlerSTQRGBAXYZF2[P] = &GSState::GIFPackedRegHandlerSTQRGBAXYZF2<P, auto_flush, index_swap>; \
	m_fpGIFPackedRegHandlerSTQRGBAXYZ2[P] = &GSState::GIFPackedRegHandlerSTQRGBAXYZ2<P, auto_flush, index_swap>;

	SetHandlerXYZ(GS_POINTLIST, true, false);
	SetHandlerXYZ(GS_LINELIST, auto_flush, index_swap);
	SetHandlerXYZ(GS_LINESTRIP, auto_flush, index_swap);
	SetHandlerXYZ(GS_TRIANGLELIST, auto_flush, index_swap);
	SetHandlerXYZ(GS_TRIANGLESTRIP, auto_flush, index_swap);
	SetHandlerXYZ(GS_TRIANGLEFAN, auto_flush, index_swap);
	SetHandlerXYZ(GS_SPRITE, auto_flush, false);
	SetHandlerXYZ(GS_INVALID, auto_flush, false);

#undef SetHandlerXYZ
}

void GSState::ResetHandlers()
{
	std::fill(std::begin(m_fpGIFPackedRegHandlers), std::end(m_fpGIFPackedRegHandlers), &GSState::GIFPackedRegHandlerNull);

	m_fpGIFPackedRegHandlers[GIF_REG_PRIM] = (GIFPackedRegHandler)(GIFRegHandler)&GSState::GIFRegHandlerPRIM;
	m_fpGIFPackedRegHandlers[GIF_REG_RGBA] = &GSState::GIFPackedRegHandlerRGBA;
	m_fpGIFPackedRegHandlers[GIF_REG_STQ] = &GSState::GIFPackedRegHandlerSTQ;
	m_fpGIFPackedRegHandlers[GIF_REG_UV] = GSConfig.UserHacks_WildHack ? &GSState::GIFPackedRegHandlerUV_Hack : &GSState::GIFPackedRegHandlerUV;
	m_fpGIFPackedRegHandlers[GIF_REG_TEX0_1] = (GIFPackedRegHandler)(GIFRegHandler)&GSState::GIFRegHandlerTEX0<0>;
	m_fpGIFPackedRegHandlers[GIF_REG_TEX0_2] = (GIFPackedRegHandler)(GIFRegHandler)&GSState::GIFRegHandlerTEX0<1>;
	m_fpGIFPackedRegHandlers[GIF_REG_CLAMP_1] = (GIFPackedRegHandler)(GIFRegHandler)&GSState::GIFRegHandlerCLAMP<0>;
	m_fpGIFPackedRegHandlers[GIF_REG_CLAMP_2] = (GIFPackedRegHandler)(GIFRegHandler)&GSState::GIFRegHandlerCLAMP<1>;
	m_fpGIFPackedRegHandlers[GIF_REG_FOG] = &GSState::GIFPackedRegHandlerFOG;
	m_fpGIFPackedRegHandlers[GIF_REG_A_D] = &GSState::GIFPackedRegHandlerA_D;
	m_fpGIFPackedRegHandlers[GIF_REG_NOP] = &GSState::GIFPackedRegHandlerNOP;

	// swap first/last indices when the provoking vertex is the first (D3D/Vulkan)
	if (IsAutoFlushEnabled())
		IsFirstProvokingVertex() ? SetPrimHandlers<true, true>() : SetPrimHandlers<true, false>();
	else
		IsFirstProvokingVertex() ? SetPrimHandlers<false, true>() : SetPrimHandlers<false, false>();

	std::fill(std::begin(m_fpGIFRegHandlers), std::end(m_fpGIFRegHandlers), &GSState::GIFRegHandlerNull);

	m_fpGIFRegHandlers[GIF_A_D_REG_PRIM] = &GSState::GIFRegHandlerPRIM;
	m_fpGIFRegHandlers[GIF_A_D_REG_RGBAQ] = &GSState::GIFRegHandlerRGBAQ;
	m_fpGIFRegHandlers[GIF_A_D_REG_RGBAQ + 0x10] = &GSState::GIFRegHandlerRGBAQ;
	m_fpGIFRegHandlers[GIF_A_D_REG_ST] = &GSState::GIFRegHandlerST;
	m_fpGIFRegHandlers[GIF_A_D_REG_UV] = GSConfig.UserHacks_WildHack ? &GSState::GIFRegHandlerUV_Hack : &GSState::GIFRegHandlerUV;
	m_fpGIFRegHandlers[GIF_A_D_REG_TEX0_1] = &GSState::GIFRegHandlerTEX0<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_TEX0_2] = &GSState::GIFRegHandlerTEX0<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_CLAMP_1] = &GSState::GIFRegHandlerCLAMP<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_CLAMP_2] = &GSState::GIFRegHandlerCLAMP<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_FOG] = &GSState::GIFRegHandlerFOG;
	m_fpGIFRegHandlers[GIF_A_D_REG_NOP] = &GSState::GIFRegHandlerNOP;
	m_fpGIFRegHandlers[GIF_A_D_REG_TEX1_1] = &GSState::GIFRegHandlerTEX1<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_TEX1_2] = &GSState::GIFRegHandlerTEX1<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_TEX2_1] = &GSState::GIFRegHandlerTEX2<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_TEX2_2] = &GSState::GIFRegHandlerTEX2<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_XYOFFSET_1] = &GSState::GIFRegHandlerXYOFFSET<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_XYOFFSET_2] = &GSState::GIFRegHandlerXYOFFSET<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_PRMODECONT] = &GSState::GIFRegHandlerPRMODECONT;
	m_fpGIFRegHandlers[GIF_A_D_REG_PRMODE] = &GSState::GIFRegHandlerPRMODE;
	m_fpGIFRegHandlers[GIF_A_D_REG_TEXCLUT] = &GSState::GIFRegHandlerTEXCLUT;
	m_fpGIFRegHandlers[GIF_A_D_REG_SCANMSK] = &GSState::GIFRegHandlerSCANMSK;
	m_fpGIFRegHandlers[GIF_A_D_REG_MIPTBP1_1] = &GSState::GIFRegHandlerMIPTBP1<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_MIPTBP1_2] = &GSState::GIFRegHandlerMIPTBP1<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_MIPTBP2_1] = &GSState::GIFRegHandlerMIPTBP2<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_MIPTBP2_2] = &GSState::GIFRegHandlerMIPTBP2<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_TEXA] = &GSState::GIFRegHandlerTEXA;
	m_fpGIFRegHandlers[GIF_A_D_REG_FOGCOL] = &GSState::GIFRegHandlerFOGCOL;
	m_fpGIFRegHandlers[GIF_A_D_REG_TEXFLUSH] = &GSState::GIFRegHandlerTEXFLUSH;
	m_fpGIFRegHandlers[GIF_A_D_REG_SCISSOR_1] = &GSState::GIFRegHandlerSCISSOR<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_SCISSOR_2] = &GSState::GIFRegHandlerSCISSOR<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_ALPHA_1] = &GSState::GIFRegHandlerALPHA<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_ALPHA_2] = &GSState::GIFRegHandlerALPHA<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_DIMX] = &GSState::GIFRegHandlerDIMX;
	m_fpGIFRegHandlers[GIF_A_D_REG_DTHE] = &GSState::GIFRegHandlerDTHE;
	m_fpGIFRegHandlers[GIF_A_D_REG_COLCLAMP] = &GSState::GIFRegHandlerCOLCLAMP;
	m_fpGIFRegHandlers[GIF_A_D_REG_TEST_1] = &GSState::GIFRegHandlerTEST<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_TEST_2] = &GSState::GIFRegHandlerTEST<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_PABE] = &GSState::GIFRegHandlerPABE;
	m_fpGIFRegHandlers[GIF_A_D_REG_FBA_1] = &GSState::GIFRegHandlerFBA<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_FBA_2] = &GSState::GIFRegHandlerFBA<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_FRAME_1] = &GSState::GIFRegHandlerFRAME<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_FRAME_2] = &GSState::GIFRegHandlerFRAME<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_ZBUF_1] = &GSState::GIFRegHandlerZBUF<0>;
	m_fpGIFRegHandlers[GIF_A_D_REG_ZBUF_2] = &GSState::GIFRegHandlerZBUF<1>;
	m_fpGIFRegHandlers[GIF_A_D_REG_BITBLTBUF] = &GSState::GIFRegHandlerBITBLTBUF;
	m_fpGIFRegHandlers[GIF_A_D_REG_TRXPOS] = &GSState::GIFRegHandlerTRXPOS;
	m_fpGIFRegHandlers[GIF_A_D_REG_TRXREG] = &GSState::GIFRegHandlerTRXREG;
	m_fpGIFRegHandlers[GIF_A_D_REG_TRXDIR] = &GSState::GIFRegHandlerTRXDIR;
	m_fpGIFRegHandlers[GIF_A_D_REG_HWREG] = &GSState::GIFRegHandlerHWREG;

	m_fpGIFRegHandlers[GIF_A_D_REG_SIGNAL] = &GSState::GIFRegHandlerNull;
	m_fpGIFRegHandlers[GIF_A_D_REG_FINISH] = &GSState::GIFRegHandlerNull;
	m_fpGIFRegHandlers[GIF_A_D_REG_LABEL] = &GSState::GIFRegHandlerNull;
}

void GSState::UpdateSettings(const Pcsx2Config::GSOptions& old_config)
{
	m_mipmap = GSConfig.Mipmap;

	if (
		GSConfig.AutoFlushSW != old_config.AutoFlushSW ||
		GSConfig.UserHacks_AutoFlush != old_config.UserHacks_AutoFlush ||
		GSConfig.UserHacks_WildHack != old_config.UserHacks_WildHack)
	{
		ResetHandlers();
	}
}

bool GSState::isinterlaced()
{
	return !!m_regs->SMODE2.INT;
}

GSVideoMode GSState::GetVideoMode()
{
	// TODO: Get confirmation of videomode from SYSCALL ? not necessary but would be nice.
	// Other videomodes can't be detected on our side without the help of the data from core
	// You can only identify a limited number of video modes based on the info from CRTC registers.

	const u8 Colorburst = m_regs->SMODE1.CMOD; // Subcarrier frequency
	const u8 PLL_Divider = m_regs->SMODE1.LC;  // Phased lock loop divider

	switch (Colorburst)
	{
		case 0:
			if (isinterlaced() && PLL_Divider == 22)
				return GSVideoMode::HDTV_1080I;
			else if (!isinterlaced() && PLL_Divider == 22)
				return GSVideoMode::HDTV_720P;
			else if (!isinterlaced() && PLL_Divider == 32)
				return GSVideoMode::SDTV_480P; // TODO: 576P will also be reported as 480P, find some way to differeniate.
			else
				return GSVideoMode::VESA;
		case 2:
			return GSVideoMode::NTSC;
		case 3:
			return GSVideoMode::PAL;
		default:
			return GSVideoMode::Unknown;
	}

	__assume(0); // unreachable
}

bool GSState::IsAnalogue()
{
	return GetVideoMode() == GSVideoMode::NTSC || GetVideoMode() == GSVideoMode::PAL || GetVideoMode() == GSVideoMode::HDTV_1080I;
}

GSVector4i GSState::GetFrameMagnifiedRect(int i)
{
	GSVector4i rectangle = { 0, 0, 0, 0 };

	if (!IsEnabled(i))
		return rectangle;

	const int videomode = static_cast<int>(GetVideoMode()) - 1;
	const auto& DISP = m_regs->DISP[i].DISPLAY;
	const bool ignore_offset = !GSConfig.PCRTCOffsets;

	const u32 DW = DISP.DW + 1;
	const u32 DH = DISP.DH + 1;
;
	// The number of sub pixels to draw are given in DH and DW, the MAGH/V relates to the size of the original square in the FB
	// but the size it's drawn to uses the default size of the display mode (for PAL/NTSC this is a MAGH of 3)
	// so for example a game will have a DW of 2559 and a MAGH of 4 to make it 512 (from the FB), but because it's drawing 2560 subpixels
	// it will cover the entire 640 wide of the screen (2560 / (3+1)).
	int width;
	int height;
	if (ignore_offset)
	{
		width = (DW / (DISP.MAGH + 1));
		height = (DH / (DISP.MAGV + 1));
	}
	else
	{
		width = (DW / (VideoModeDividers[videomode].x + 1));
		height = (DH / (VideoModeDividers[videomode].y + 1));
	}

	int res_multi = 1;

	if (isinterlaced() && m_regs->SMODE2.FFMD && height > 1)
		res_multi = 2;

	// Set up the display rectangle based on the values obtained from DISPLAY registers
	rectangle.right = width;
	rectangle.bottom = height / res_multi;

	return rectangle;
}

int GSState::GetDisplayHMagnification()
{
	// Pick one of the DISPLAY's and hope that they are both the same. Favour DISPLAY[1]
	for (int i = 1; i >= 0; i--)
	{
		if (IsEnabled(i))
			return m_regs->DISP[i].DISPLAY.MAGH + 1;
	}

	// If neither DISPLAY is enabled, fallback to resolution offset (should never happen)
	const int videomode = static_cast<int>(GetVideoMode()) - 1;
	return VideoModeDividers[videomode].x + 1;
}

GSVector4i GSState::GetDisplayRect(int i)
{
	GSVector4i rectangle = { 0, 0, 0, 0 };

	if (i == -1)
	{
		return GetDisplayRect(0).runion(GetDisplayRect(1));
	}

	if (!IsEnabled(i))
		return rectangle;

	const auto& DISP = m_regs->DISP[i].DISPLAY;

	const u32 DW = DISP.DW + 1;
	const u32 DH = DISP.DH + 1;
	const u32 MAGH = DISP.MAGH + 1;
	const u32 MAGV = DISP.MAGV + 1;

	const GSVector2i magnification(MAGH, MAGV);

	const int width = DW / magnification.x;
	const int height = DH / magnification.y;

	GSVector2i offsets = GetResolutionOffset(i);

	// Set up the display rectangle based on the values obtained from DISPLAY registers
	rectangle.left = offsets.x;
	rectangle.top = offsets.y;

	rectangle.right = rectangle.left + width;
	rectangle.bottom = rectangle.top + height;

	return rectangle;
}

GSVector2i GSState::GetResolutionOffset(int i)
{
	GSVector2i offset = { 0, 0 };

	if (!IsEnabled(i))
		return offset;

	const int videomode = static_cast<int>(GetVideoMode()) - 1;
	const auto& DISP = m_regs->DISP[i].DISPLAY;

	const auto& SMODE2 = m_regs->SMODE2;
	const int res_multi = (SMODE2.INT + 1);

	offset.x = (static_cast<int>(DISP.DX) - VideoModeOffsets[videomode].z) / (VideoModeDividers[videomode].x + 1);
	offset.y = (static_cast<int>(DISP.DY) - (VideoModeOffsets[videomode].w * ((IsAnalogue() && res_multi) ? res_multi : 1))) / (VideoModeDividers[videomode].y + 1);

	return offset;
}

GSVector2i GSState::GetResolution()
{
	const int videomode = static_cast<int>(GetVideoMode()) - 1;
	const bool ignore_offset = !GSConfig.PCRTCOffsets;

	GSVector2i resolution(VideoModeOffsets[videomode].x, VideoModeOffsets[videomode].y);

	if (isinterlaced() && !m_regs->SMODE2.FFMD)
		resolution.y *= 2;

	if (ignore_offset)
	{
		// Ideally we'd just cut the width at the resolution, but of course we have to hack the hack...
		// Some games (Mortal Kombat Armageddon) render the image at 834 pixels then shrink it to 624 pixels
		// which does fit, but when we ignore offsets we go on framebuffer size and some other games
		// such as Johnny Mosleys Mad Trix and Transformers render too much but design it to go off the screen.
		int magnified_width = (VideoModeDividers[videomode].z + 1) / GetDisplayHMagnification();

		GSVector4i total_rect = GetDisplayRect(0).runion(GetDisplayRect(1));
		total_rect.z = total_rect.z - total_rect.x;
		total_rect.w = total_rect.w - total_rect.y;
		total_rect.z = std::min(total_rect.z, magnified_width);
		total_rect.w = std::min(total_rect.w, resolution.y);
		resolution.x = total_rect.z;
		resolution.y = total_rect.w;
	}

	return resolution;
}

GSVector4i GSState::GetFrameRect(int i)
{
	// If no specific context is requested then pass the merged rectangle as return value
	if (i == -1)
		return GetFrameRect(0).runion(GetFrameRect(1));

	GSVector4i rectangle = { 0, 0, 0, 0 };

	if (!IsEnabled(i))
		return rectangle;

	const auto& DISP = m_regs->DISP[i].DISPLAY;
	
	const u32 DW = DISP.DW + 1;
	const u32 DH = DISP.DH + 1;
	const GSVector2i magnification(DISP.MAGH+1, DISP.MAGV + 1);

	const u32 DBX = m_regs->DISP[i].DISPFB.DBX;
	const u32 DBY = m_regs->DISP[i].DISPFB.DBY;

	int w = DW / magnification.x;
	int h = DH / magnification.y;

	rectangle.left = DBX;
	rectangle.top = DBY;

	rectangle.right = rectangle.left + w;
	rectangle.bottom = rectangle.top + h;

	if (isinterlaced() && m_regs->SMODE2.FFMD && h > 1)
	{
		rectangle.bottom += 1;
		rectangle.bottom >>= 1;
	}

	return rectangle;
}

int GSState::GetFramebufferHeight()
{
	// Framebuffer height is 11 bits max
	constexpr int height_limit = (1 << 11);

	const GSVector4i disp1_rect = GetFrameRect(0);
	const GSVector4i disp2_rect = GetFrameRect(1);

	const GSVector4i combined = disp1_rect.runion(disp2_rect);

	// DBY isn't an offset to the frame memory but rather an offset to read output circuit inside
	// the frame memory, hence the top offset should also be calculated for the total height of the
	// frame memory. Also we need to wrap the value only when we're dealing with values with range of the
	// frame memory (offset + read output circuit height, IOW bottom of merged_output)
	const int max_height = std::max(disp1_rect.height(), disp2_rect.height());
	const int frame_memory_height = std::max(max_height, combined.bottom % height_limit);

	if (frame_memory_height > 1024)
		GL_PERF("Massive framebuffer height detected! (height:%d)", frame_memory_height);

	return frame_memory_height;
}

bool GSState::IsEnabled(int i)
{
	ASSERT(i >= 0 && i < 2);

	const auto& DISP = m_regs->DISP[i].DISPLAY;

	const bool disp1_enabled = m_regs->PMODE.EN1;
	const bool disp2_enabled = m_regs->PMODE.EN2;

	if ((i == 0 && disp1_enabled) || (i == 1 && disp2_enabled))
		return DISP.DW && DISP.DH;

	return false;
}

float GSState::GetTvRefreshRate()
{
	const GSVideoMode videomode = GetVideoMode();

	//TODO: Check vertical frequencies for VESA video modes, old ones were untested.

	switch (videomode)
	{
		case GSVideoMode::NTSC:
		case GSVideoMode::SDTV_480P:
			return (60 / 1.001f);
		case GSVideoMode::PAL:
			return 50;
		case GSVideoMode::HDTV_720P:
		case GSVideoMode::HDTV_1080I:
			return 60;
		default:
			Console.Error("GS: Unknown video mode. Please report: https://github.com/PCSX2/pcsx2/issues");
			return 0;
	}

	__assume(0); // unreachable
}

void GSState::DumpVertices(const std::string& filename)
{
	std::ofstream file(filename);

	if (!file.is_open())
		return;

	size_t count = m_index.tail;
	GSVertex* buffer = &m_vertex.buff[0];

	const char* DEL = ", ";

	file << "VERTEX COORDS (XYZ)" << std::endl;
	file << std::fixed << std::setprecision(4);
	for (size_t i = 0; i < count; ++i)
	{
		file << "\t" << "v" << i << ": ";
		GSVertex v = buffer[m_index.buff[i]];

		float x = (v.XYZ.X - (int)m_context->XYOFFSET.OFX) / 16.0f;
		float y = (v.XYZ.Y - (int)m_context->XYOFFSET.OFY) / 16.0f;

		file << x << DEL;
		file << y << DEL;
		file << v.XYZ.Z;
		file << std::endl;
	}

	file << std::endl;

	file << "VERTEX COLOR (RGBA)" << std::endl;
	file << std::fixed << std::setprecision(6);
	for (size_t i = 0; i < count; ++i)
	{
		file << "\t" << "v" << i << ": ";
		GSVertex v = buffer[m_index.buff[i]];

		file << std::setfill('0') << std::setw(3) << unsigned(v.RGBAQ.R) << DEL;
		file << std::setfill('0') << std::setw(3) << unsigned(v.RGBAQ.G) << DEL;
		file << std::setfill('0') << std::setw(3) << unsigned(v.RGBAQ.B) << DEL;
		file << std::setfill('0') << std::setw(3) << unsigned(v.RGBAQ.A);
		file << std::endl;
	}

	file << std::endl;

	bool use_uv = PRIM->FST;
	std::string qualifier = use_uv ? "UV" : "STQ";

	file << "TEXTURE COORDS (" << qualifier << ")" << std::endl;;
	for (size_t i = 0; i < count; ++i)
	{
		file << "\t" << "v" << i << ": ";
		GSVertex v = buffer[m_index.buff[i]];

		// note
		// Yes, technically as far as the GS is concerned Q belongs
		// to RGBAQ. However, the purpose of this dump is to print
		// our data in a more human readable format and typically Q
		// is associated with STQ.
		if (use_uv)
		{
			float uv_U = v.U / 16.0f;
			float uv_V = v.V / 16.0f;

			file << uv_U << DEL << uv_V;
		}
		else
			file << v.ST.S << DEL << v.ST.T << DEL << v.RGBAQ.Q;

		file << std::endl;
	}

	file << std::endl;

	file << "TRACER" << std::endl;

	GSVector4i v = m_vt.m_min.c;
	file << "\tmin c (x,y,z,w): " << v.x << DEL << v.y << DEL << v.z << DEL << v.w << std::endl;
	v = m_vt.m_max.c;
	file << "\tmax c (x,y,z,w): " << v.x << DEL << v.y << DEL << v.z << DEL << v.w << std::endl;

	GSVector4 v2 = m_vt.m_min.p;
	file << "\tmin p (x,y,z,w): " << v2.x << DEL << v2.y << DEL << v2.z << DEL << v2.w << std::endl;
	v2 = m_vt.m_max.p;
	file << "\tmax p (x,y,z,w): " << v2.x << DEL << v2.y << DEL << v2.z << DEL << v2.w << std::endl;
	v2 = m_vt.m_min.t;
	file << "\tmin t (x,y,z,w): " << v2.x << DEL << v2.y << DEL << v2.z << DEL << v2.w << std::endl;
	v2 = m_vt.m_max.t;
	file << "\tmax t (x,y,z,w): " << v2.x << DEL << v2.y << DEL << v2.z << DEL << v2.w << std::endl;

	file.close();
}

__inline void GSState::CheckFlushes()
{
	if (m_primflush)
		Flush();

	if ((m_context->FRAME.FBMSK & GSLocalMemory::m_psm[m_context->FRAME.PSM].fmsk) != GSLocalMemory::m_psm[m_context->FRAME.PSM].fmsk)
		m_mem.m_clut.Invalidate(m_context->FRAME.Block());
}

void GSState::GIFPackedRegHandlerNull(const GIFPackedReg* RESTRICT r)
{
}

void GSState::GIFPackedRegHandlerRGBA(const GIFPackedReg* RESTRICT r)
{
	const GSVector4i mask = GSVector4i::load(0x0c080400);
	const GSVector4i v = GSVector4i::load<false>(r).shuffle8(mask);

	m_v.RGBAQ.U32[0] = (u32)GSVector4i::store(v);

	m_v.RGBAQ.Q = m_q;
}

void GSState::GIFPackedRegHandlerSTQ(const GIFPackedReg* RESTRICT r)
{
	const GSVector4i st = GSVector4i::loadl(&r->U64[0]);

	GSVector4i q = GSVector4i::loadl(&r->U64[1]);
	GSVector4i::storel(&m_v.ST, st);

	// Vexx (character shadow)
	// q = 0 (st also 0 on the first 16 vertices), setting it to 1.0f to avoid div by zero later
	q = q.blend8(GSVector4i::cast(GSVector4::m_one), q == GSVector4i::zero());

	// Suikoden 4
	// creates some nan for Q. Let's avoid undefined behavior (See GIFRegHandlerRGBAQ)
	q = GSVector4i::cast(GSVector4::cast(q).replace_nan(GSVector4::m_max));

	GSVector4::store(&m_q, GSVector4::cast(q));

	// hide behind a define for now to avoid spam in the above cases for users
#if defined(PCSX2_DEVBUILD) || defined(_DEBUG)
	if (std::isnan(m_v.ST.S) || std::isnan(m_v.ST.T))
		Console.Warning("S or T is nan");
#endif
}

void GSState::GIFPackedRegHandlerUV(const GIFPackedReg* RESTRICT r)
{
	const GSVector4i v = GSVector4i::loadl(r) & GSVector4i::x00003fff();

	m_v.UV = (u32)GSVector4i::store(v.ps32(v));
}

void GSState::GIFPackedRegHandlerUV_Hack(const GIFPackedReg* RESTRICT r)
{
	const GSVector4i v = GSVector4i::loadl(r) & GSVector4i::x00003fff();

	m_v.UV = (u32)GSVector4i::store(v.ps32(v));

	m_isPackedUV_HackFlag = true;
}

template <u32 prim, u32 adc, bool auto_flush, bool index_swap>
void GSState::GIFPackedRegHandlerXYZF2(const GIFPackedReg* RESTRICT r)
{
	CheckFlushes();

	GSVector4i xy = GSVector4i::loadl(&r->U64[0]);
	GSVector4i zf = GSVector4i::loadl(&r->U64[1]);

	xy = xy.upl16(xy.srl<4>()).upl32(GSVector4i::load((int)m_v.UV));
	zf = zf.srl32(4) & GSVector4i::x00ffffff().upl32(GSVector4i::x000000ff());

	m_v.m[1] = xy.upl32(zf);

	VertexKick<prim, auto_flush, index_swap>(adc ? 1 : r->XYZF2.Skip());
}

template <u32 prim, u32 adc, bool auto_flush, bool index_swap>
void GSState::GIFPackedRegHandlerXYZ2(const GIFPackedReg* RESTRICT r)
{
	CheckFlushes();

	const GSVector4i xy = GSVector4i::loadl(&r->U64[0]);
	const GSVector4i z = GSVector4i::loadl(&r->U64[1]);
	const GSVector4i xyz = xy.upl16(xy.srl<4>()).upl32(z);

	m_v.m[1] = xyz.upl64(GSVector4i::loadl(&m_v.UV));

	VertexKick<prim, auto_flush, index_swap>(adc ? 1 : r->XYZ2.Skip());
}

void GSState::GIFPackedRegHandlerFOG(const GIFPackedReg* RESTRICT r)
{
	m_v.FOG = r->FOG.F;
}

void GSState::GIFPackedRegHandlerA_D(const GIFPackedReg* RESTRICT r)
{
	(this->*m_fpGIFRegHandlers[r->A_D.ADDR & 0x7F])(&r->r);
}

void GSState::GIFPackedRegHandlerNOP(const GIFPackedReg* RESTRICT r)
{
}

template <u32 prim, bool auto_flush, bool index_swap>
void GSState::GIFPackedRegHandlerSTQRGBAXYZF2(const GIFPackedReg* RESTRICT r, u32 size)
{
	ASSERT(size > 0 && size % 3 == 0);

	CheckFlushes();

	const GIFPackedReg* RESTRICT r_end = r + size;

	while (r < r_end)
	{
		GSVector4i st = GSVector4i::loadl(&r[0].U64[0]);
		GSVector4i q = GSVector4i::loadl(&r[0].U64[1]);
		GSVector4i rgba = (GSVector4i::load<false>(&r[1]) & GSVector4i::x000000ff()).ps32().pu16();

		q = q.blend8(GSVector4i::cast(GSVector4::m_one), q == GSVector4i::zero()); // see GIFPackedRegHandlerSTQ

		m_v.m[0] = st.upl64(rgba.upl32(q)); // TODO: only store the last one

		GSVector4i xy = GSVector4i::loadl(&r[2].U64[0]);
		GSVector4i zf = GSVector4i::loadl(&r[2].U64[1]);
		xy = xy.upl16(xy.srl<4>()).upl32(GSVector4i::load((int)m_v.UV));
		zf = zf.srl32(4) & GSVector4i::x00ffffff().upl32(GSVector4i::x000000ff());

		m_v.m[1] = xy.upl32(zf); // TODO: only store the last one

		VertexKick<prim, auto_flush, index_swap>(r[2].XYZF2.Skip());

		r += 3;
	}

	m_q = r[-3].STQ.Q; // remember the last one, STQ outputs this to the temp Q each time
}

template <u32 prim, bool auto_flush, bool index_swap>
void GSState::GIFPackedRegHandlerSTQRGBAXYZ2(const GIFPackedReg* RESTRICT r, u32 size)
{
	ASSERT(size > 0 && size % 3 == 0);

	CheckFlushes();

	const GIFPackedReg* RESTRICT r_end = r + size;

	while (r < r_end)
	{
		GSVector4i st = GSVector4i::loadl(&r[0].U64[0]);
		GSVector4i q = GSVector4i::loadl(&r[0].U64[1]);
		GSVector4i rgba = (GSVector4i::load<false>(&r[1]) & GSVector4i::x000000ff()).ps32().pu16();

		q = q.blend8(GSVector4i::cast(GSVector4::m_one), q == GSVector4i::zero()); // see GIFPackedRegHandlerSTQ

		m_v.m[0] = st.upl64(rgba.upl32(q)); // TODO: only store the last one

		GSVector4i xy = GSVector4i::loadl(&r[2].U64[0]);
		GSVector4i z = GSVector4i::loadl(&r[2].U64[1]);
		GSVector4i xyz = xy.upl16(xy.srl<4>()).upl32(z);

		m_v.m[1] = xyz.upl64(GSVector4i::loadl(&m_v.UV)); // TODO: only store the last one

		VertexKick<prim, auto_flush, index_swap>(r[2].XYZ2.Skip());

		r += 3;
	}

	m_q = r[-3].STQ.Q; // remember the last one, STQ outputs this to the temp Q each time
}

void GSState::GIFPackedRegHandlerNOP(const GIFPackedReg* RESTRICT r, u32 size)
{
}

void GSState::GIFRegHandlerNull(const GIFReg* RESTRICT r)
{
}

__forceinline void GSState::ApplyPRIM(u32 prim)
{

	if (m_env.PRMODECONT.AC == 1)
	{
		m_env.PRIM.U32[0] = prim;

		UpdateContext();
	}
	else
	{
		m_env.PRIM.PRIM = prim & 0x7;
	}

	m_primflush = false;
	u32 prim_mask = 0x7ff;

	// Same class of draw so we don't need to flush
	if (GSUtil::GetPrimClass(m_last_prim.PRIM) == GSUtil::GetPrimClass(m_env.PRIM.PRIM))
		prim_mask &= ~0x7;

	if (GSConfig.UseHardwareRenderer() && GSUtil::GetPrimClass(prim & 7) == GS_TRIANGLE_CLASS)
		prim_mask &= ~0x80; // Mask out AA1.

	if ((m_last_prim.U32[0] ^ m_env.PRIM.U32[0]) & prim_mask)
		m_primflush = true;

	UpdateVertexKick();

	ASSERT(m_index.tail == 0 || !g_gs_device->Features().provoking_vertex_last || m_index.buff[m_index.tail - 1] + 1 == m_vertex.next);

	if (m_index.tail == 0)
	{
		m_vertex.next = 0;
		m_primflush = false;
		m_last_prim.U32[0] = m_env.PRIM.U32[0];
	}

	m_vertex.head = m_vertex.tail = m_vertex.next; // remove unused vertices from the end of the vertex buffer
}

void GSState::GIFRegHandlerPRIM(const GIFReg* RESTRICT r)
{
	ALIGN_STACK(32);

	ApplyPRIM(r->PRIM.U32[0]);
}

void GSState::GIFRegHandlerRGBAQ(const GIFReg* RESTRICT r)
{
	GSVector4i rgbaq = (GSVector4i)r->RGBAQ;

	GSVector4i q = rgbaq.blend8(GSVector4i::cast(GSVector4::m_one), rgbaq == GSVector4i::zero()).yyyy(); // see GIFPackedRegHandlerSTQ

	// Silent Hill output a nan in Q to emulate the flash light. Unfortunately it
	// breaks GSVertexTrace code that rely on min/max.

	q = GSVector4i::cast(GSVector4::cast(q).replace_nan(GSVector4::m_max));

	m_v.RGBAQ = rgbaq.upl32(q);
}

void GSState::GIFRegHandlerST(const GIFReg* RESTRICT r)
{
	m_v.ST = (GSVector4i)r->ST;

#if defined(PCSX2_DEVBUILD) || defined(_DEBUG)
	if (std::isnan(m_v.ST.S) || std::isnan(m_v.ST.T))
		Console.Warning("S or T is nan");
#endif
}

void GSState::GIFRegHandlerUV(const GIFReg* RESTRICT r)
{
	m_v.UV = r->UV.U32[0] & 0x3fff3fff;
}

void GSState::GIFRegHandlerUV_Hack(const GIFReg* RESTRICT r)
{
	m_v.UV = r->UV.U32[0] & 0x3fff3fff;

	m_isPackedUV_HackFlag = false;
}

template <u32 prim, u32 adc, bool auto_flush, bool index_swap>
void GSState::GIFRegHandlerXYZF2(const GIFReg* RESTRICT r)
{
	CheckFlushes();

	GSVector4i xyzf = GSVector4i::loadl(&r->XYZF);
	GSVector4i xyz = xyzf & (GSVector4i::xffffffff().upl32(GSVector4i::x00ffffff()));
	GSVector4i uvf = GSVector4i::load((int)m_v.UV).upl32(xyzf.srl32(24).srl<4>());

	m_v.m[1] = xyz.upl64(uvf);

	VertexKick<prim, auto_flush, index_swap>(adc);
}

template <u32 prim, u32 adc, bool auto_flush, bool index_swap>
void GSState::GIFRegHandlerXYZ2(const GIFReg* RESTRICT r)
{
	CheckFlushes();

	m_v.m[1] = GSVector4i::load(&r->XYZ, &m_v.UV);

	VertexKick<prim, auto_flush, index_swap>(adc);
}

template <int i>
void GSState::ApplyTEX0(GIFRegTEX0& TEX0)
{
	// TODO: Paletted Formats
	// 8-bit and 4 bit formats need to be addressed with a buffer width divisible 2.
	// However, not doing so is possible and does have a behavior on the GS.
	// When implementing such code care must be taken not to apply it unless it is
	// used for a draw. Galaxy Angel will send TEX0 with a PSM of T8 and a TBW of 7
	// only to immediately update it to CT32 with TEX2. The old code used to apply a
	// correction on the TEX0 setting which caused the game to draw the CT32 texture
	// with an incorrect buffer width.
	//
	// Bouken Jidai Katsugeki Goemon apparently uses a TBW of 1 but this game is currently
	// extremely broken for the same reasons as MLB Power Pros in that it spams TEX0 with
	// complete garbage making for a nice 1G heap of GSOffset.

	GL_REG("Apply TEX0_%d = 0x%x_%x", i, TEX0.U32[1], TEX0.U32[0]);

	// even if TEX0 did not change, a new palette may have been uploaded and will overwrite the currently queued for drawing
	const bool wt = m_mem.m_clut.WriteTest(TEX0, m_env.TEXCLUT);

	// clut loading already covered with WriteTest, for drawing only have to check CPSM and CSA (MGS3 intro skybox would be drawn piece by piece without this)

	constexpr u64 mask = 0x1f78001fffffffffull; // TBP0 TBW PSM TW TH TCC TFX CPSM CSA

	if (wt || (PRIM->CTXT == i || m_primflush) && ((TEX0.U64 ^ m_env.CTXT[i].TEX0.U64) & mask))
		Flush();

	TEX0.CPSM &= 0xa; // 1010b

	if ((TEX0.U32[0] ^ m_env.CTXT[i].TEX0.U32[0]) & 0x3ffffff) // TBP0 TBW PSM
		m_env.CTXT[i].offset.tex = m_mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);

	m_env.CTXT[i].TEX0 = (GSVector4i)TEX0;

	if (wt)
	{
		GIFRegBITBLTBUF BITBLTBUF;
		GSVector4i r;

		if (TEX0.CSM == 0)
		{
			BITBLTBUF.SBP = TEX0.CBP;
			BITBLTBUF.SBW = 1;
			BITBLTBUF.SPSM = TEX0.CSM;

			r.left = 0;
			r.top = 0;
			r.right = GSLocalMemory::m_psm[TEX0.CPSM].bs.x;
			r.bottom = GSLocalMemory::m_psm[TEX0.CPSM].bs.y;

			int blocks = 4;

			if (GSLocalMemory::m_psm[TEX0.CPSM].bpp == 16)
				blocks >>= 1;

			if (GSLocalMemory::m_psm[TEX0.PSM].bpp == 4)
				blocks >>= 1;

			for (int j = 0; j < blocks; j++, BITBLTBUF.SBP++)
				InvalidateLocalMem(BITBLTBUF, r, true);
		}
		else
		{
			BITBLTBUF.SBP = TEX0.CBP;
			BITBLTBUF.SBW = m_env.TEXCLUT.CBW;
			BITBLTBUF.SPSM = TEX0.CSM;

			r.left = m_env.TEXCLUT.COU;
			r.top = m_env.TEXCLUT.COV;
			r.right = r.left + GSLocalMemory::m_psm[TEX0.CPSM].pal;
			r.bottom = r.top + 1;

			InvalidateLocalMem(BITBLTBUF, r, true);
		}

		m_mem.m_clut.Write(m_env.CTXT[i].TEX0, m_env.TEXCLUT);
	}
}

template <int i>
void GSState::GIFRegHandlerTEX0(const GIFReg* RESTRICT r)
{
	GL_REG("TEX0_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	GIFRegTEX0 TEX0 = r->TEX0;
	GIFRegMIPTBP1 temp_MIPTBP1;
	bool MTBAReloaded = false;
	// Max allowed MTBA size for 32bit swizzled textures (including 8H 4HL etc) is 512, 16bit and normal 8/4bit formats can be 1024
	const u32 maxTex = (GSLocalMemory::m_psm[TEX0.PSM].bpp < 32) ? 10 : 9;

	// Spec max is 10
	//
	// Yakuza (minimap)
	// Sets TW/TH to 0
	// Drawn using solid colors, the texture is really a 1x1 white texel,
	// modulated by the vertex color. Cannot change the dimension because S/T are normalized.
	//
	// Tokyo Xtreme Racer Drift 2 (text)
	// Sets TW/TH to 0
	// there used to be a case to force this to 10
	// but GetSizeFixedTEX0 sorts this now
	TEX0.TW = std::clamp<u32>(TEX0.TW, 0, 10);
	TEX0.TH = std::clamp<u32>(TEX0.TH, 0, 10);

	// MTBA loads are triggered by writes to TEX0 (but not TEX2!)
	// Textures MUST be a minimum width of 32 pixels
	// Format must be a color, Z formats do not trigger MTBA (but are valid for Mipmapping)
	if (m_env.CTXT[i].TEX1.MTBA && TEX0.TW >= 5 && TEX0.TW <= maxTex && (TEX0.PSM & 0x30) != 0x30)
	{
		// NOTE 1: TEX1.MXL must not be automatically set to 3 here and it has no effect on MTBA.
		// NOTE 2: Mipmap levels are packed with a minimum distance between them of 1 block, even down at 4bit textures under 16x16.
		// NOTE 3: Everything is derrived from the width of the texture, TBW and TH are completely ignored (useful for handling non-rectangular ones)
		// NOTE 4: Cartoon Network Racing's menu is VERY sensitive to this as it uses 4bit sized textures for the sky.
		u32 bp = TEX0.TBP0;
		u32 bw = std::max(1u, (1u << TEX0.TW) >> 6);

		// Address is calculated as a 4bit address space, then converted (/8) to 32bit address space
		// ((w * w * bpp) / 8) / 64. No the 'w' is not a typo ;)
		const u32 bpp = GSLocalMemory::m_psm[TEX0.PSM].bpp >> 2;
		u32 tex_size = ((1u << TEX0.TW) * (1u << TEX0.TW) * bpp) >> 9;

		bp += tex_size;
		bw = std::max<u32>(bw >> 1, 1);
		tex_size = std::max<u32>(tex_size >> 2, 1);

		temp_MIPTBP1.TBP1 = bp;
		temp_MIPTBP1.TBW1 = bw;

		bp += tex_size;
		bw = std::max<u32>(bw >> 1, 1);
		tex_size = std::max<u32>(tex_size >> 2, 1);

		temp_MIPTBP1.TBP2 = bp;
		temp_MIPTBP1.TBW2 = bw;

		bp += tex_size;
		bw = std::max<u32>(bw >> 1, 1);

		temp_MIPTBP1.TBP3 = bp;
		temp_MIPTBP1.TBW3 = bw;

		if (temp_MIPTBP1 != m_env.CTXT[i].MIPTBP1)
			Flush();

		MTBAReloaded = true;
	}

	ApplyTEX0<i>(TEX0);

	if (MTBAReloaded)
		m_env.CTXT[i].MIPTBP1 = temp_MIPTBP1;
}

template <int i>
void GSState::GIFRegHandlerCLAMP(const GIFReg* RESTRICT r)
{
	GL_REG("CLAMP_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	if ((PRIM->CTXT == i || m_primflush) && r->CLAMP != m_env.CTXT[i].CLAMP)
		Flush();

	m_env.CTXT[i].CLAMP = (GSVector4i)r->CLAMP;
}

void GSState::GIFRegHandlerFOG(const GIFReg* RESTRICT r)
{
	m_v.FOG = r->FOG.F;
}

void GSState::GIFRegHandlerNOP(const GIFReg* RESTRICT r)
{
}

template <int i>
void GSState::GIFRegHandlerTEX1(const GIFReg* RESTRICT r)
{
	GL_REG("TEX1_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	if ((PRIM->CTXT == i || m_primflush) && r->TEX1 != m_env.CTXT[i].TEX1)
		Flush();

	m_env.CTXT[i].TEX1 = (GSVector4i)r->TEX1;
}

template <int i>
void GSState::GIFRegHandlerTEX2(const GIFReg* RESTRICT r)
{
	GL_REG("TEX2_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	// TEX2 is a masked write to TEX0, for performing CLUT swaps (palette swaps).
	// It only applies the following fields:
	//    CLD, CSA, CSM, CPSM, CBP, PSM.
	// It ignores these fields (uses existing values in the context):
	//    TFX, TCC, TH, TW, TBW, and TBP0

	constexpr u64 mask = 0xFFFFFFE003F00000ull; // TEX2 bits

	GIFRegTEX0 TEX0;

	TEX0.U64 = (m_env.CTXT[i].TEX0.U64 & ~mask) | (r->U64 & mask);

	ApplyTEX0<i>(TEX0);
}

template <int i>
void GSState::GIFRegHandlerXYOFFSET(const GIFReg* RESTRICT r)
{
	GL_REG("XYOFFSET_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	const GSVector4i o = (GSVector4i)r->XYOFFSET & GSVector4i::x0000ffff();

	if (!o.eq(m_env.CTXT[i].XYOFFSET))
		Flush();

	m_env.CTXT[i].XYOFFSET = o;

	m_env.CTXT[i].UpdateScissor();

	UpdateScissor();
}

void GSState::GIFRegHandlerPRMODECONT(const GIFReg* RESTRICT r)
{
	GL_REG("PRMODECONT = 0x%x_%x", r->U32[1], r->U32[0]);

	m_env.PRMODECONT.AC = r->PRMODECONT.AC;
}

void GSState::GIFRegHandlerPRMODE(const GIFReg* RESTRICT r)
{
	GL_REG("PRMODE = 0x%x_%x", r->U32[1], r->U32[0]);

	// We're in PRIM mode, need to ignore any writes
	if (m_env.PRMODECONT.AC)
		return;

	const u32 _PRIM = m_env.PRIM.PRIM;
	m_env.PRIM = (GSVector4i)r->PRMODE;
	m_env.PRIM.PRIM = _PRIM;

	u32 prim_mask = 0x7ff;
	
	// Same class of draw so we don't need to flush
	if (GSUtil::GetPrimClass(m_last_prim.PRIM) == GSUtil::GetPrimClass(m_env.PRIM.PRIM))
		prim_mask &= ~0x7;

	if (GSConfig.UseHardwareRenderer() && GSUtil::GetPrimClass(m_env.PRIM.PRIM) == GS_TRIANGLE_CLASS)
		prim_mask &= ~0x80; // Mask out AA1.

	m_primflush = false;
	if ((m_last_prim.U32[0] ^ m_env.PRIM.U32[0]) & prim_mask)
		m_primflush = true;

	if (m_index.tail == 0)
	{
		m_primflush = false;
		m_last_prim.U32[0] = m_env.PRIM.U32[0];
	}

	UpdateContext();
}

void GSState::GIFRegHandlerTEXCLUT(const GIFReg* RESTRICT r)
{
	GL_REG("TEXCLUT = 0x%x_%x", r->U32[1], r->U32[0]);

	if (r->TEXCLUT != m_env.TEXCLUT)
		Flush();

	m_env.TEXCLUT = (GSVector4i)r->TEXCLUT;
}

void GSState::GIFRegHandlerSCANMSK(const GIFReg* RESTRICT r)
{
	if (r->SCANMSK != m_env.SCANMSK)
		Flush();

	m_env.SCANMSK = (GSVector4i)r->SCANMSK;
	if (m_env.SCANMSK.MSK & 2)
		m_scanmask_used = true;
}

template <int i>
void GSState::GIFRegHandlerMIPTBP1(const GIFReg* RESTRICT r)
{
	GL_REG("MIPTBP1_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	if ((PRIM->CTXT == i || m_primflush) && r->MIPTBP1 != m_env.CTXT[i].MIPTBP1)
		Flush();

	m_env.CTXT[i].MIPTBP1 = (GSVector4i)r->MIPTBP1;
}

template <int i>
void GSState::GIFRegHandlerMIPTBP2(const GIFReg* RESTRICT r)
{
	GL_REG("MIPTBP2_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	if ((PRIM->CTXT == i || m_primflush) && r->MIPTBP2 != m_env.CTXT[i].MIPTBP2)
		Flush();

	m_env.CTXT[i].MIPTBP2 = (GSVector4i)r->MIPTBP2;
}

void GSState::GIFRegHandlerTEXA(const GIFReg* RESTRICT r)
{
	GL_REG("TEXA = 0x%x_%x", r->U32[1], r->U32[0]);
	if (r->TEXA != m_env.TEXA)
		Flush();

	m_env.TEXA = (GSVector4i)r->TEXA;
}

void GSState::GIFRegHandlerFOGCOL(const GIFReg* RESTRICT r)
{
	GL_REG("FOGCOL = 0x%x_%x", r->U32[1], r->U32[0]);

	if (r->FOGCOL != m_env.FOGCOL)
		Flush();

	m_env.FOGCOL = (GSVector4i)r->FOGCOL;
}

void GSState::GIFRegHandlerTEXFLUSH(const GIFReg* RESTRICT r)
{
	GL_REG("TEXFLUSH = 0x%x_%x PRIM TME %x", r->U32[1], r->U32[0], PRIM->TME);

	// Some games do a single sprite draw to itself, then flush the texture cache, then use that texture again.
	// This won't get picked up by the new autoflush logic (which checks for page crossings for the PS2 Texture Cache flush)
	// so we need to do it here.
	if (IsAutoFlushEnabled())
		Flush();
}

template <int i>
void GSState::GIFRegHandlerSCISSOR(const GIFReg* RESTRICT r)
{
	if ((PRIM->CTXT == i || m_primflush) && r->SCISSOR != m_env.CTXT[i].SCISSOR)
		Flush();

	m_env.CTXT[i].SCISSOR = (GSVector4i)r->SCISSOR;

	m_env.CTXT[i].UpdateScissor();

	UpdateScissor();
}

template <int i>
void GSState::GIFRegHandlerALPHA(const GIFReg* RESTRICT r)
{
	GL_REG("ALPHA = 0x%x_%x", r->U32[1], r->U32[0]);
	if ((PRIM->CTXT == i || m_primflush) && r->ALPHA != m_env.CTXT[i].ALPHA)
		Flush();

	m_env.CTXT[i].ALPHA = (GSVector4i)r->ALPHA;

	// value of 3 is not allowed by the spec
	// acts like 2 on real hw, so just clamp it
	m_env.CTXT[i].ALPHA.A = std::clamp<u32>(r->ALPHA.A, 0, 2);
	m_env.CTXT[i].ALPHA.B = std::clamp<u32>(r->ALPHA.B, 0, 2);
	m_env.CTXT[i].ALPHA.C = std::clamp<u32>(r->ALPHA.C, 0, 2);
	m_env.CTXT[i].ALPHA.D = std::clamp<u32>(r->ALPHA.D, 0, 2);
}

void GSState::GIFRegHandlerDIMX(const GIFReg* RESTRICT r)
{
	bool update = false;

	if (r->DIMX != m_env.DIMX)
	{
		Flush();

		update = true;
	}

	m_env.DIMX = (GSVector4i)r->DIMX;

	if (update)
		m_env.UpdateDIMX();
}

void GSState::GIFRegHandlerDTHE(const GIFReg* RESTRICT r)
{
	if (r->DTHE != m_env.DTHE)
		Flush();

	m_env.DTHE = (GSVector4i)r->DTHE;
}

void GSState::GIFRegHandlerCOLCLAMP(const GIFReg* RESTRICT r)
{
	if (r->COLCLAMP != m_env.COLCLAMP)
		Flush();

	m_env.COLCLAMP = (GSVector4i)r->COLCLAMP;
}

template <int i>
void GSState::GIFRegHandlerTEST(const GIFReg* RESTRICT r)
{
	if ((PRIM->CTXT == i || m_primflush) && r->TEST != m_env.CTXT[i].TEST)
		Flush();

	m_env.CTXT[i].TEST = (GSVector4i)r->TEST;
}

void GSState::GIFRegHandlerPABE(const GIFReg* RESTRICT r)
{
	if (r->PABE != m_env.PABE)
		Flush();

	m_env.PABE = (GSVector4i)r->PABE;
}

template <int i>
void GSState::GIFRegHandlerFBA(const GIFReg* RESTRICT r)
{
	if ((PRIM->CTXT == i || m_primflush) && r->FBA != m_env.CTXT[i].FBA)
		Flush();

	m_env.CTXT[i].FBA = (GSVector4i)r->FBA;
}

template <int i>
void GSState::GIFRegHandlerFRAME(const GIFReg* RESTRICT r)
{
	GL_REG("FRAME_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	GIFRegFRAME NewFrame = r->FRAME;
	// FBW is clamped between 1 and 32, however this is wrong, FBW of 0 *should* work and does on Dobiestation
	// However there is some issues so even software mode is incorrect on PCSX2, but this works better..
	NewFrame.FBW = std::clamp(NewFrame.FBW, 1U, 32U);

	if ((PRIM->CTXT == i || m_primflush) && NewFrame != m_env.CTXT[i].FRAME)
		Flush();

	if ((NewFrame.PSM & 0x30) == 0x30)
		m_env.CTXT[i].ZBUF.PSM &= ~0x30;
	else
		m_env.CTXT[i].ZBUF.PSM |= 0x30;

	if ((m_env.CTXT[i].FRAME.U32[0] ^ NewFrame.U32[0]) & 0x3f3f01ff) // FBP FBW PSM
	{
		m_env.CTXT[i].offset.fb = m_mem.GetOffset(NewFrame.Block(), NewFrame.FBW, NewFrame.PSM);
		m_env.CTXT[i].offset.zb = m_mem.GetOffset(m_env.CTXT[i].ZBUF.Block(), NewFrame.FBW, m_env.CTXT[i].ZBUF.PSM);
		m_env.CTXT[i].offset.fzb = m_mem.GetPixelOffset(NewFrame, m_env.CTXT[i].ZBUF);
		m_env.CTXT[i].offset.fzb4 = m_mem.GetPixelOffset4(NewFrame, m_env.CTXT[i].ZBUF);
	}

	m_env.CTXT[i].FRAME = (GSVector4i)NewFrame;

	switch (m_env.CTXT[i].FRAME.PSM)
	{
		case PSM_PSMT8H:
			// Berserk uses the format to only update the alpha channel
			GL_INS("CORRECT FRAME FORMAT replaces PSM_PSMT8H by PSM_PSMCT32/0x00FF_FFFF");
			m_env.CTXT[i].FRAME.PSM = PSM_PSMCT32;
			m_env.CTXT[i].FRAME.FBMSK = 0x00FFFFFF;
			break;
		case PSM_PSMT4HH: // Not tested. Based on PSM_PSMT8H behavior
			GL_INS("CORRECT FRAME FORMAT replaces PSM_PSMT4HH by PSM_PSMCT32/0x0FFF_FFFF");
			m_env.CTXT[i].FRAME.PSM = PSM_PSMCT32;
			m_env.CTXT[i].FRAME.FBMSK = 0x0FFFFFFF;
			break;
		case PSM_PSMT4HL: // Not tested. Based on PSM_PSMT8H behavior
			GL_INS("CORRECT FRAME FORMAT replaces PSM_PSMT4HL by PSM_PSMCT32/0xF0FF_FFFF");
			m_env.CTXT[i].FRAME.PSM = PSM_PSMCT32;
			m_env.CTXT[i].FRAME.FBMSK = 0xF0FFFFFF;
			break;
		default:
			break;
	}
}

template <int i>
void GSState::GIFRegHandlerZBUF(const GIFReg* RESTRICT r)
{
	GL_REG("ZBUF_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	GIFRegZBUF ZBUF = r->ZBUF;

	// We tested this on the PS2 and it seems to be that when the FRAME is a Z format,
	// the Z buffer is forced to use color swizzling.
	// Powerdrome relies on this behavior to clear the z buffer by drawing 32 pixel wide strips, skipping 32,
	// causing the FRAME to do one strip and the Z to do the other 32 due to the block arrangement.
	// Other games listed here also hit this Color/Z swap behaviour without masking Z so could be problematic:
	// Black, Driver Parallel Lines, Driv3r, Dropship, DT Racer, Scarface, The Simpsons, THP8
	if ((m_env.CTXT[i].FRAME.PSM & 0x30) == 0x30)
		ZBUF.PSM &= ~0x30;
	else
		ZBUF.PSM |= 0x30;

	if ((PRIM->CTXT == i || m_primflush) && ZBUF != m_env.CTXT[i].ZBUF)
		Flush();

	if ((m_env.CTXT[i].ZBUF.U32[0] ^ ZBUF.U32[0]) & 0x3f0001ff) // ZBP PSM
	{
		m_env.CTXT[i].offset.zb = m_mem.GetOffset(ZBUF.Block(), m_env.CTXT[i].FRAME.FBW, ZBUF.PSM);
		m_env.CTXT[i].offset.fzb = m_mem.GetPixelOffset(m_env.CTXT[i].FRAME, ZBUF);
		m_env.CTXT[i].offset.fzb4 = m_mem.GetPixelOffset4(m_env.CTXT[i].FRAME, ZBUF);
	}

	m_env.CTXT[i].ZBUF = (GSVector4i)ZBUF;
}

void GSState::GIFRegHandlerBITBLTBUF(const GIFReg* RESTRICT r)
{
	// TODO: Paletted formats
	// There is a memory bug on the GS as it relates to the transfering of
	// 8-bit and 4-bit formats needing an even buffer width due to the
	// second half of the page being addressed by TBW/2
	//
	// namcoXcapcom: Apparently uses DBW of 5 and 11 (and refers to them
	// in TEX0 later as 4 and 10 respectively). However I can find no
	// documentation on this problem, nothing in the game to suggest
	// it is broken and the code here for it was likely incorrect to begin with.

	GL_REG("BITBLTBUF = 0x%x_%x", r->U32[1], r->U32[0]);

	if (r->BITBLTBUF != m_env.BITBLTBUF)
		FlushWrite();

	m_env.BITBLTBUF = (GSVector4i)r->BITBLTBUF;
}

void GSState::GIFRegHandlerTRXPOS(const GIFReg* RESTRICT r)
{
	GL_REG("TRXPOS = 0x%x_%x", r->U32[1], r->U32[0]);

	if (r->TRXPOS != m_env.TRXPOS)
		FlushWrite();

	m_env.TRXPOS = (GSVector4i)r->TRXPOS;
}

void GSState::GIFRegHandlerTRXREG(const GIFReg* RESTRICT r)
{
	GL_REG("TRXREG = 0x%x_%x", r->U32[1], r->U32[0]);
	if (r->TRXREG != m_env.TRXREG)
		FlushWrite();

	m_env.TRXREG = (GSVector4i)r->TRXREG;
}

void GSState::GIFRegHandlerTRXDIR(const GIFReg* RESTRICT r)
{
	GL_REG("TRXDIR = 0x%x_%x", r->U32[1], r->U32[0]);

	Flush();

	m_env.TRXDIR = (GSVector4i)r->TRXDIR;

	switch (m_env.TRXDIR.XDIR)
	{
		case 0: // host -> local
			m_tr.Init(m_env.TRXPOS.DSAX, m_env.TRXPOS.DSAY, m_env.BITBLTBUF);
			break;
		case 1: // local -> host
			m_tr.Init(m_env.TRXPOS.SSAX, m_env.TRXPOS.SSAY, m_env.BITBLTBUF);
			break;
		case 2: // local -> local
			Move();
			break;
		default: // 3 deactivated as stated by manual. Tested on hardware and no transfers happen.
			break;
	}
}

void GSState::GIFRegHandlerHWREG(const GIFReg* RESTRICT r)
{
	GL_REG("HWREG = 0x%x_%x", r->U32[1], r->U32[0]);

	// don't bother if not host -> local
	// real hw ignores
	if (m_env.TRXDIR.XDIR != 0)
		return;

	Write(reinterpret_cast<const u8*>(r), 8); // haunting ground
}

void GSState::Flush()
{
	FlushWrite();
	FlushPrim();
}

void GSState::FlushWrite()
{
	const int len = m_tr.end - m_tr.start;

	if (len <= 0)
		return;

	GSVector4i r;

	r.left = m_env.TRXPOS.DSAX;
	r.top = m_env.TRXPOS.DSAY;
	r.right = r.left + m_env.TRXREG.RRW;
	r.bottom = r.top + m_env.TRXREG.RRH;

	InvalidateVideoMem(m_env.BITBLTBUF, r);

	const GSLocalMemory::writeImage wi = GSLocalMemory::m_psm[m_env.BITBLTBUF.DPSM].wi;

	(m_mem.*wi)(m_tr.x, m_tr.y, &m_tr.buff[m_tr.start], len, m_env.BITBLTBUF, m_env.TRXPOS, m_env.TRXREG);

	m_tr.start += len;

	g_perfmon.Put(GSPerfMon::Swizzle, len);
}

void GSState::FlushPrim()
{
	const u32 new_prim = PRIM->U32[0];

	if (m_primflush)
	{
		// We need to restore the old PRIM and update the Context (in case it changed)
		m_env.PRIM.U32[0] = m_last_prim.U32[0];
		UpdateContext();
	}

	if (m_index.tail > 0)
	{
		GL_REG("FlushPrim ctxt %d", PRIM->CTXT);

		// internal frame rate detection based on sprite blits to the display framebuffer
		{
			const u32 FRAME_FBP = m_context->FRAME.FBP;
			if ((m_regs->DISP[0].DISPFB.FBP == FRAME_FBP && m_regs->PMODE.EN1) ||
				(m_regs->DISP[1].DISPFB.FBP == FRAME_FBP && m_regs->PMODE.EN2))
			{
				g_perfmon.AddDisplayFramebufferSpriteBlit();
			}
		}

		GSVertex buff[2];
		s_n++;

		size_t head = m_vertex.head;
		size_t tail = m_vertex.tail;
		size_t next = m_vertex.next;
		size_t unused = 0;

		if (tail > head)
		{
			switch (PRIM->PRIM)
			{
				case GS_POINTLIST:
					ASSERT(0);
					break;
				case GS_LINELIST:
				case GS_LINESTRIP:
				case GS_SPRITE:
					unused = 1;
					buff[0] = m_vertex.buff[tail - 1];
					break;
				case GS_TRIANGLELIST:
				case GS_TRIANGLESTRIP:
					unused = std::min<size_t>(tail - head, 2);
					memcpy(buff, &m_vertex.buff[tail - unused], sizeof(GSVertex) * 2);
					break;
				case GS_TRIANGLEFAN:
					buff[0] = m_vertex.buff[head];
					unused = 1;
					if (tail - 1 > head)
					{
						buff[1] = m_vertex.buff[tail - 1];
						unused = 2;
					}
					break;
				case GS_INVALID:
					break;
				default:
					__assume(0);
			}

			ASSERT((int)unused < GSUtil::GetVertexCount(PRIM->PRIM));
		}

		// If the PSM format of Z is invalid, but it is masked (no write) and ZTST is set to ALWAYS pass (no test, just allow)
		// we can ignore the Z format, since it won't be used in the draw (Star Ocean 3 transitions)
		const bool ignoreZ = m_context->ZBUF.ZMSK && m_context->TEST.ZTST == 1;

		if (GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt >= 3 || (GSLocalMemory::m_psm[m_context->ZBUF.PSM].fmt >= 3 && !ignoreZ))
		{
			Console.Warning("GS: Possible invalid draw, Frame PSM %x ZPSM %x", m_context->FRAME.PSM, m_context->ZBUF.PSM);
		}

		m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail, m_index.tail, GSUtil::GetPrimClass(PRIM->PRIM));

		m_context->SaveReg();

		try
		{
			Draw();
		}
		catch (GSRecoverableError&)
		{
			// could be an unsupported draw call
		}
		catch (const std::bad_alloc&)
		{
			// Texture Out Of Memory
			PurgePool();
			Console.Error("GS: Memory allocation failure.");
		}

		m_context->RestoreReg();

		g_perfmon.Put(GSPerfMon::Draw, 1);
		g_perfmon.Put(GSPerfMon::Prim, m_index.tail / GSUtil::GetVertexCount(PRIM->PRIM));

		m_index.tail = 0;
		m_vertex.head = 0;

		if (unused > 0)
		{
			memcpy(m_vertex.buff, buff, sizeof(GSVertex) * unused);

			m_vertex.tail = unused;
			m_vertex.next = next > head ? next - head : 0;
		}
		else
		{
			m_vertex.tail = 0;
			m_vertex.next = 0;
		}
	}

	if (m_primflush)
	{
		// Restore the backup
		PRIM->U32[0] = new_prim;
		UpdateContext();
	}

	m_primflush = false;
	m_last_prim.U32[0] = new_prim;
}

void GSState::Write(const u8* mem, int len)
{
	int w = m_env.TRXREG.RRW;
	int h = m_env.TRXREG.RRH;

	GIFRegBITBLTBUF& blit = m_tr.m_blit;
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[blit.DPSM];

	// The game uses a resolution of 512x244. RT is located at 0x700 and depth at 0x0
	//
	// #Bug number 1. (bad top bar)
	// The game saves the depth buffer in the EE but with a resolution of
	// 512x255. So it is ending to 0x7F8, ouch it saves the top of the RT too.
	//
	// #Bug number 2. (darker screen)
	// The game will restore the previously saved buffer at position 0x0 to
	// 0x7F8.  Because of the extra RT pixels, GS will partialy invalidate
	// the texture located at 0x700. Next access will generate a cache miss
	//
	// The no-solution: instead to handle garbage (aka RT) at the end of the
	// depth buffer. Let's reduce the size of the transfer

	if (m_game.title == CRC::SMTNocturne) // TODO: hack
	{
		if (blit.DBP == 0 && blit.DPSM == PSM_PSMZ32 && w == 512 && h > 224)
		{
			h = 224;
			m_env.TRXREG.RRH = 224;
		}
	}

	if (!m_tr.Update(w, h, psm.trbpp, len))
		return;

	GL_CACHE("Write! ...  => 0x%x W:%d F:%s (DIR %d%d), dPos(%d %d) size(%d %d)",
			 blit.DBP, blit.DBW, psm_str(blit.DPSM),
			 m_env.TRXPOS.DIRX, m_env.TRXPOS.DIRY,
			 m_env.TRXPOS.DSAX, m_env.TRXPOS.DSAY, w, h);

	if ((PRIM->TME || (m_primflush && m_last_prim.TME)) && (blit.DBP == m_context->TEX0.TBP0 || blit.DBP == m_context->TEX0.CBP)) // TODO: hmmmm
		FlushPrim();

	if (m_tr.end == 0 && len >= m_tr.total)
	{
		// received all data in one piece, no need to buffer it
		GSVector4i r;

		r.left = m_env.TRXPOS.DSAX;
		r.top = m_env.TRXPOS.DSAY;
		r.right = r.left + m_env.TRXREG.RRW;
		r.bottom = r.top + m_env.TRXREG.RRH;

		InvalidateVideoMem(blit, r);

		(m_mem.*psm.wi)(m_tr.x, m_tr.y, mem, m_tr.total, blit, m_env.TRXPOS, m_env.TRXREG);

		m_tr.start = m_tr.end = m_tr.total;

		g_perfmon.Put(GSPerfMon::Swizzle, len);
	}
	else
	{
		memcpy(&m_tr.buff[m_tr.end], mem, len);

		m_tr.end += len;

		if (m_tr.end >= m_tr.total)
			FlushWrite();
	}

	m_mem.m_clut.Invalidate();
}

void GSState::InitReadFIFO(u8* mem, int len)
{
	if (len <= 0)
		return;

	const int sx = m_env.TRXPOS.SSAX;
	const int sy = m_env.TRXPOS.SSAY;
	const int w = m_env.TRXREG.RRW;
	const int h = m_env.TRXREG.RRH;

	const u16 bpp = GSLocalMemory::m_psm[m_env.BITBLTBUF.SPSM].trbpp;

	if (!m_tr.Update(w, h, bpp, len))
		return;

	if (m_tr.x == sx && m_tr.y == sy)
		InvalidateLocalMem(m_env.BITBLTBUF, GSVector4i(sx, sy, sx + w, sy + h));
}

// NOTE: called from outside MTGS
void GSState::Read(u8* mem, int len)
{
	if (len <= 0)
		return;

	const int sx = m_env.TRXPOS.SSAX;
	const int sy = m_env.TRXPOS.SSAY;
	const int w = m_env.TRXREG.RRW;
	const int h = m_env.TRXREG.RRH;

	const GSVector4i r(sx, sy, sx + w, sy + h);

	const u16 bpp = GSLocalMemory::m_psm[m_env.BITBLTBUF.SPSM].trbpp;

	if (!m_tr.Update(w, h, bpp, len))
		return;

	m_mem.ReadImageX(m_tr.x, m_tr.y, mem, len, m_env.BITBLTBUF, m_env.TRXPOS, m_env.TRXREG);

	if (s_dump && s_save && s_n >= s_saven)
	{
		std::string s = m_dump_root + StringUtil::StdStringFromFormat(
			"%05d_read_%05x_%d_%d_%d_%d_%d_%d.bmp",
			s_n, (int)m_env.BITBLTBUF.SBP, (int)m_env.BITBLTBUF.SBW, (int)m_env.BITBLTBUF.SPSM,
			r.left, r.top, r.right, r.bottom);

		m_mem.SaveBMP(s, m_env.BITBLTBUF.SBP, m_env.BITBLTBUF.SBW, m_env.BITBLTBUF.SPSM, r.right, r.bottom);
	}
}

void GSState::Move()
{
	// ffxii uses this to move the top/bottom of the scrolling menus offscreen and then blends them back over the text to create a shading effect
	// guitar hero copies the far end of the board to do a similar blend too

	int sx = m_env.TRXPOS.SSAX;
	int sy = m_env.TRXPOS.SSAY;
	int dx = m_env.TRXPOS.DSAX;
	int dy = m_env.TRXPOS.DSAY;

	const int w = m_env.TRXREG.RRW;
	const int h = m_env.TRXREG.RRH;

	GL_CACHE("Move! 0x%x W:%d F:%s => 0x%x W:%d F:%s (DIR %d%d), sPos(%d %d) dPos(%d %d) size(%d %d)",
			 m_env.BITBLTBUF.SBP, m_env.BITBLTBUF.SBW, psm_str(m_env.BITBLTBUF.SPSM),
			 m_env.BITBLTBUF.DBP, m_env.BITBLTBUF.DBW, psm_str(m_env.BITBLTBUF.DPSM),
			 m_env.TRXPOS.DIRX, m_env.TRXPOS.DIRY,
			 sx, sy, dx, dy, w, h);

	InvalidateLocalMem(m_env.BITBLTBUF, GSVector4i(sx, sy, sx + w, sy + h));
	InvalidateVideoMem(m_env.BITBLTBUF, GSVector4i(dx, dy, dx + w, dy + h));

	int xinc = 1;
	int yinc = 1;

	if (m_env.TRXPOS.DIRX)
	{
		sx += w - 1;
		dx += w - 1;
		xinc = -1;
	}
	if (m_env.TRXPOS.DIRY)
	{
		sy += h - 1;
		dy += h - 1;
		yinc = -1;
	}

	const GSLocalMemory::psm_t& spsm = GSLocalMemory::m_psm[m_env.BITBLTBUF.SPSM];
	const GSLocalMemory::psm_t& dpsm = GSLocalMemory::m_psm[m_env.BITBLTBUF.DPSM];

	// TODO: unroll inner loops (width has special size requirement, must be multiples of 1 << n, depending on the format)

	int sbp = m_env.BITBLTBUF.SBP;
	int sbw = m_env.BITBLTBUF.SBW;
	int dbp = m_env.BITBLTBUF.DBP;
	int dbw = m_env.BITBLTBUF.DBW;
	GSOffset spo = m_mem.GetOffset(sbp, sbw, m_env.BITBLTBUF.SPSM);
	GSOffset dpo = m_mem.GetOffset(dbp, dbw, m_env.BITBLTBUF.DPSM);

	auto genericCopy = [=](const GSOffset& dpo, const GSOffset& spo, auto&& getPAHelper, auto&& pxCopyFn)
	{
		int _sy = sy, _dy = dy; // Faster with local copied variables, compiler optimizations are dumb
		if (xinc > 0)
		{
			const int page_width = GSLocalMemory::m_psm[m_env.BITBLTBUF.DPSM].pgs.x;
			const int page_height = GSLocalMemory::m_psm[m_env.BITBLTBUF.DPSM].pgs.y;
			const int xpage = sx & ~(page_width - 1);
			const int ypage = _sy & ~(page_height - 1);
			// Copying from itself to itself (rotating textures) used in Gitaroo Man stage 8
			// What probably happens is because the copy is buffered, the source stays just ahead of the destination.
			if (sbp == dbp && (((_sy < _dy) && ((ypage + page_height) > _dy)) || ((sx < dx) && ((xpage + page_width) > dx))))
			{
				int starty = (yinc > 0) ? 0 : h-1;
				int endy = (yinc > 0) ? h : -1;
				int y_inc = yinc;

				if (((_sy < _dy) && ((ypage + page_height) > _dy)) && yinc > 0)
				{
					_sy += h;
					_dy += h;
					starty = h-1;
					endy = -1;
					y_inc = -y_inc;
				}
			
				for (int y = starty; y != endy; y+= y_inc, _sy += y_inc, _dy += y_inc)
				{
					auto s = getPAHelper(spo, sx, _sy);
					auto d = getPAHelper(dpo, dx, _dy);

					if (((sx < dx) && ((xpage + page_width) > dx)))
					{
						for (int x = w - 1; x >= 0; x--)
						{
							pxCopyFn(d, s, x);
						}
					}
					else
					{
						for (int x = 0; x < w; x++)
						{
							pxCopyFn(d, s, x);
						}
					}
				}
			}
			else
			{
				for (int y = 0; y < h; y++, _sy += yinc, _dy += yinc)
				{
					auto s = getPAHelper(spo, sx, _sy);
					auto d = getPAHelper(dpo, dx, _dy);

					for (int x = 0; x < w; x++)
					{
						pxCopyFn(d, s, x);
					}
				}
			}
		}
		else
		{
			for (int y = 0; y < h; y++, _sy += yinc, _dy += yinc)
			{
				auto s = getPAHelper(spo, sx, _sy);
				auto d = getPAHelper(dpo, dx, _dy);

				for (int x = 0; x < w; x++)
				{
					pxCopyFn(d, s, -x);
				}
			}
		}
	};

	auto copy = [=](const GSOffset& dpo, const GSOffset& spo, auto&& pxCopyFn)
	{
		genericCopy(dpo, spo,
			[](const GSOffset& o, int x, int y) { return o.paMulti(x, y); },
			[=](const GSOffset::PAHelper& d, const GSOffset::PAHelper& s, int x)
			{
				return pxCopyFn(d.value(x), s.value(x));
			});
	};

	auto copyFast = [=](auto* vm, const GSOffset& dpo, const GSOffset& spo, auto&& pxCopyFn)
	{
		genericCopy(dpo, spo,
			[=](const GSOffset& o, int x, int y) { return o.paMulti(vm, x, y); },
			[=](const auto& d, const auto& s, int x)
			{
				return pxCopyFn(d.value(x), s.value(x));
			});
	};

	if (spsm.trbpp == dpsm.trbpp && spsm.trbpp >= 16)
	{
		if (spsm.trbpp == 32)
		{
			copyFast(m_mem.vm32(), dpo.assertSizesMatch(GSLocalMemory::swizzle32), spo.assertSizesMatch(GSLocalMemory::swizzle32), [](u32* d, u32* s)
			{
				*d = *s;
			});
		}
		else if (spsm.trbpp == 24)
		{
			copyFast(m_mem.vm32(), dpo.assertSizesMatch(GSLocalMemory::swizzle32), spo.assertSizesMatch(GSLocalMemory::swizzle32), [](u32* d, u32* s)
			{
				*d = (*d & 0xff000000) | (*s & 0x00ffffff);
			});
		}
		else // if(spsm.trbpp == 16)
		{
			copyFast(m_mem.vm16(), dpo.assertSizesMatch(GSLocalMemory::swizzle16), spo.assertSizesMatch(GSLocalMemory::swizzle16), [](u16* d, u16* s)
			{
				*d = *s;
			});
		}
	}
	else if (m_env.BITBLTBUF.SPSM == PSM_PSMT8 && m_env.BITBLTBUF.DPSM == PSM_PSMT8)
	{
		copyFast(m_mem.m_vm8, GSOffset::fromKnownPSM(dbp, dbw, PSM_PSMT8), GSOffset::fromKnownPSM(sbp, sbw, PSM_PSMT8), [](u8* d, u8* s)
		{
			*d = *s;
		});
	}
	else if (m_env.BITBLTBUF.SPSM == PSM_PSMT4 && m_env.BITBLTBUF.DPSM == PSM_PSMT4)
	{
		copy(GSOffset::fromKnownPSM(dbp, dbw, PSM_PSMT4), GSOffset::fromKnownPSM(sbp, sbw, PSM_PSMT4), [&](u32 doff, u32 soff)
		{
			m_mem.WritePixel4(doff, m_mem.ReadPixel4(soff));
		});
	}
	else
	{
		copy(dpo, spo, [&](u32 doff, u32 soff)
		{
			(m_mem.*dpsm.wpa)(doff, (m_mem.*spsm.rpa)(soff));
		});
	}
}

void GSState::SoftReset(u32 mask)
{
	if (mask & 1)
	{
		memset(&m_path[0], 0, sizeof(GIFPath));
		memset(&m_path[3], 0, sizeof(GIFPath));
	}

	if (mask & 2)
		memset(&m_path[1], 0, sizeof(GIFPath));

	if (mask & 4)
		memset(&m_path[2], 0, sizeof(GIFPath));

	m_env.TRXDIR.XDIR = 3; //-1 ; set it to invalid value

	m_q = 1.0f;
}

void GSState::ReadFIFO(u8* mem, int size)
{
	Flush();

	size *= 16;

	Read(mem, size);

	if (m_dump)
		m_dump->ReadFIFO(size);
}

void GSState::ReadLocalMemoryUnsync(u8* mem, int qwc, GIFRegBITBLTBUF BITBLTBUF, GIFRegTRXPOS TRXPOS, GIFRegTRXREG TRXREG)
{
	const int sx = TRXPOS.SSAX;
	const int sy = TRXPOS.SSAY;
	const int w = TRXREG.RRW;
	const int h = TRXREG.RRH;

	const u16 bpp = GSLocalMemory::m_psm[BITBLTBUF.SPSM].trbpp;

	GSTransferBuffer tb;
	tb.Init(TRXPOS.SSAX, TRXPOS.SSAY, BITBLTBUF);

	int len = qwc * 16;
	if (!tb.Update(w, h, bpp, len))
		return;

	m_mem.ReadImageX(tb.x, tb.y, mem, len, BITBLTBUF, TRXPOS, TRXREG);
}

template void GSState::Transfer<0>(const u8* mem, u32 size);
template void GSState::Transfer<1>(const u8* mem, u32 size);
template void GSState::Transfer<2>(const u8* mem, u32 size);
template void GSState::Transfer<3>(const u8* mem, u32 size);

template <int index>
void GSState::Transfer(const u8* mem, u32 size)
{
	const u8* start = mem;

	GIFPath& path = m_path[index];

	while (size > 0)
	{
		if (path.nloop == 0)
		{
			path.SetTag(mem);

			mem += sizeof(GIFTag);
			size--;

			// eeuser 7.2.2. GIFtag:
			// "... when NLOOP is 0, the GIF does not output anything, and values other than the EOP field are disregarded."
			if (path.nloop > 0)
			{
				m_q = 1.0f;

				// ASSERT(!(path.tag.PRE && path.tag.FLG == GIF_FLG_REGLIST)); // kingdom hearts

				if (path.tag.PRE && path.tag.FLG == GIF_FLG_PACKED)
					ApplyPRIM(path.tag.PRIM);
			}
		}
		else
		{
			u32 total;

			switch (path.tag.FLG)
			{
				case GIF_FLG_PACKED:
					// get to the start of the loop
					if (path.reg != 0)
					{
						do
						{
							(this->*m_fpGIFPackedRegHandlers[path.GetReg()])((GIFPackedReg*)mem);

							mem += sizeof(GIFPackedReg);
							size--;
						} while (path.StepReg() && size > 0 && path.reg != 0);
					}

					// all data available? usually is

					total = path.nloop * path.nreg;

					if (size >= total)
					{
						size -= total;

						switch (path.type)
						{
							case GIFPath::TYPE_UNKNOWN:
							{
								u32 reg = 0;

								do
								{
									(this->*m_fpGIFPackedRegHandlers[path.GetReg(reg++)])((GIFPackedReg*)mem);

									mem += sizeof(GIFPackedReg);

									reg = reg & ((int)(reg - path.nreg) >> 31); // resets reg back to 0 when it becomes equal to path.nreg
								} while (--total > 0);
							}
							break;
							case GIFPath::TYPE_ADONLY: // very common
								do
								{
									(this->*m_fpGIFRegHandlers[((GIFPackedReg*)mem)->A_D.ADDR & 0x7F])(&((GIFPackedReg*)mem)->r);

									mem += sizeof(GIFPackedReg);
								} while (--total > 0);

								break;
							case GIFPath::TYPE_STQRGBAXYZF2: // majority of the vertices are formatted like this
								(this->*m_fpGIFPackedRegHandlersC[GIF_REG_STQRGBAXYZF2])((GIFPackedReg*)mem, total);

								mem += total * sizeof(GIFPackedReg);

								break;
							case GIFPath::TYPE_STQRGBAXYZ2:
								(this->*m_fpGIFPackedRegHandlersC[GIF_REG_STQRGBAXYZ2])((GIFPackedReg*)mem, total);

								mem += total * sizeof(GIFPackedReg);

								break;
							default:
								__assume(0);
						}

						path.nloop = 0;
					}
					else
					{
						do
						{
							(this->*m_fpGIFPackedRegHandlers[path.GetReg()])((GIFPackedReg*)mem);

							mem += sizeof(GIFPackedReg);
							size--;
						} while (path.StepReg() && size > 0);
					}

					break;
				case GIF_FLG_REGLIST:
					// TODO: do it similar to packed operation

					size *= 2;

					do
					{
						(this->*m_fpGIFRegHandlers[path.GetReg() & 0x7F])((GIFReg*)mem);

						mem += sizeof(GIFReg);
						size--;
					} while (path.StepReg() && size > 0);

					if (size & 1)
						mem += sizeof(GIFReg);

					size /= 2;

					break;
				case GIF_FLG_IMAGE2:
					// hmmm
					// Fall through here fixes a crash in Wallace and Gromit Project Zoo
					// and according to Pseudonym we shouldn't even land in this code. So hmm indeed. (rama)
				case GIF_FLG_IMAGE:
				{
					int len = (int)std::min(size, path.nloop);

					switch (m_env.TRXDIR.XDIR)
					{
					case 0:
						Write(mem, len * 16);
						break;
					case 2:
						Move();
						break;
					default: // 1 and 3
						// 1 is invalid because downloads can only be done
						// with a reverse fifo operation (vif)
						// 3 is spec prohibited, it's behavior is not known
						// lets do nothing for now
						break;
					}

					mem += len * 16;
					path.nloop -= len;
					size -= len;

					break;
				}
				default:
					__assume(0);
			}
		}

		if (index == 0)
		{
			if (path.tag.EOP && path.nloop == 0)
				break;
		}
	}

	if (m_dump && mem > start)
		m_dump->Transfer(index, start, mem - start);

	if (index == 0)
	{
		if (size == 0 && path.nloop > 0)
		{
			// Hackfix for BIOS, which sends an incomplete packet when it does an XGKICK without
			// having an EOP specified anywhere in VU1 memory.  Needed until PCSX2 is fixed to
			// handle it more properly (ie, without looping infinitely).

			path.nloop = 0;
		}
	}
}

template <class T>
static void WriteState(u8*& dst, T* src, size_t len = sizeof(T))
{
	memcpy(dst, src, len);
	dst += len;
}

template <class T>
static void ReadState(T* dst, u8*& src, size_t len = sizeof(T))
{
	memcpy(dst, src, len);
	src += len;
}

int GSState::Freeze(freezeData* fd, bool sizeonly)
{
	if (sizeonly)
	{
		fd->size = m_sssize;
		return 0;
	}

	if (!fd->data || fd->size < m_sssize)
		return -1;

	Flush();

    u8* data = fd->data;

	WriteState(data, &m_version);
	WriteState(data, &m_env.PRIM);
	WriteState(data, &m_env.PRMODECONT);
	WriteState(data, &m_env.TEXCLUT);
	WriteState(data, &m_env.SCANMSK);
	WriteState(data, &m_env.TEXA);
	WriteState(data, &m_env.FOGCOL);
	WriteState(data, &m_env.DIMX);
	WriteState(data, &m_env.DTHE);
	WriteState(data, &m_env.COLCLAMP);
	WriteState(data, &m_env.PABE);
	WriteState(data, &m_env.BITBLTBUF);
	WriteState(data, &m_env.TRXDIR);
	WriteState(data, &m_env.TRXPOS);
	WriteState(data, &m_env.TRXREG);
	WriteState(data, &m_env.TRXREG); // obsolete

	for (int i = 0; i < 2; i++)
	{
		WriteState(data, &m_env.CTXT[i].XYOFFSET);
		WriteState(data, &m_env.CTXT[i].TEX0);
		WriteState(data, &m_env.CTXT[i].TEX1);
		WriteState(data, &m_env.CTXT[i].CLAMP);
		WriteState(data, &m_env.CTXT[i].MIPTBP1);
		WriteState(data, &m_env.CTXT[i].MIPTBP2);
		WriteState(data, &m_env.CTXT[i].SCISSOR);
		WriteState(data, &m_env.CTXT[i].ALPHA);
		WriteState(data, &m_env.CTXT[i].TEST);
		WriteState(data, &m_env.CTXT[i].FBA);
		WriteState(data, &m_env.CTXT[i].FRAME);
		WriteState(data, &m_env.CTXT[i].ZBUF);
	}

	WriteState(data, &m_v.RGBAQ);
	WriteState(data, &m_v.ST);
	WriteState(data, &m_v.UV);
	WriteState(data, &m_v.FOG);
	WriteState(data, &m_v.XYZ);
	data += sizeof(GIFReg); // obsolite
	WriteState(data, &m_tr.x);
	WriteState(data, &m_tr.y);
	WriteState(data, m_mem.m_vm8, m_mem.m_vmsize);

	for (GIFPath& path : m_path)
	{
		path.tag.NREG = path.nreg;
		path.tag.NLOOP = path.nloop;
		path.tag.REGS = 0;

		for (size_t j = 0; j < std::size(path.regs.U8); j++)
		{
			path.tag.U32[2 + (j >> 3)] |= path.regs.U8[j] << ((j & 7) << 2);
		}

		WriteState(data, &path.tag);
		WriteState(data, &path.reg);
	}

	WriteState(data, &m_q);

	return 0;
}

int GSState::Defrost(const freezeData* fd)
{
	if (!fd || !fd->data || fd->size == 0)
		return -1;

	if (fd->size < m_sssize)
		return -1;

	u8* data = fd->data;

	u32 version;

	ReadState(&version, data);

	if (version > m_version)
	{
		Console.Error("GS: Savestate version is incompatible.  Load aborted.");
		return -1;
	}

	Flush();

	Reset(false);

	ReadState(&m_env.PRIM, data);

	if (version <= 6)
		data += sizeof(GIFRegPRMODE);

	ReadState(&m_env.PRMODECONT, data);
	ReadState(&m_env.TEXCLUT, data);
	ReadState(&m_env.SCANMSK, data);
	ReadState(&m_env.TEXA, data);
	ReadState(&m_env.FOGCOL, data);
	ReadState(&m_env.DIMX, data);
	ReadState(&m_env.DTHE, data);
	ReadState(&m_env.COLCLAMP, data);
	ReadState(&m_env.PABE, data);
	ReadState(&m_env.BITBLTBUF, data);
	ReadState(&m_env.TRXDIR, data);
	ReadState(&m_env.TRXPOS, data);
	ReadState(&m_env.TRXREG, data);
	ReadState(&m_env.TRXREG, data); // obsolete
	// Technically this value ought to be saved like m_tr.x/y (break
	// compatibility) but so far only a single game (Motocross Mania) really
	// depends on this value (i.e != BITBLTBUF) Savestates are likely done at
	// VSYNC, so not in the middle of a texture transfer, therefore register
	// will be set again properly
	m_tr.m_blit = m_env.BITBLTBUF;

	for (int i = 0; i < 2; i++)
	{
		ReadState(&m_env.CTXT[i].XYOFFSET, data);
		ReadState(&m_env.CTXT[i].TEX0, data);
		ReadState(&m_env.CTXT[i].TEX1, data);

		if (version <= 6)
			data += sizeof(GIFRegTEX2);

		ReadState(&m_env.CTXT[i].CLAMP, data);
		ReadState(&m_env.CTXT[i].MIPTBP1, data);
		ReadState(&m_env.CTXT[i].MIPTBP2, data);
		ReadState(&m_env.CTXT[i].SCISSOR, data);
		ReadState(&m_env.CTXT[i].ALPHA, data);
		ReadState(&m_env.CTXT[i].TEST, data);
		ReadState(&m_env.CTXT[i].FBA, data);
		ReadState(&m_env.CTXT[i].FRAME, data);
		ReadState(&m_env.CTXT[i].ZBUF, data);

		m_env.CTXT[i].XYOFFSET.OFX &= 0xffff;
		m_env.CTXT[i].XYOFFSET.OFY &= 0xffff;

		if (version <= 4)
			data += sizeof(u32) * 7; // skip
	}

	ReadState(&m_v.RGBAQ, data);
	ReadState(&m_v.ST, data);
	ReadState(&m_v.UV, data);
	ReadState(&m_v.FOG, data);
	ReadState(&m_v.XYZ, data);
	data += sizeof(GIFReg); // obsolite
	ReadState(&m_tr.x, data);
	ReadState(&m_tr.y, data);
	ReadState(m_mem.m_vm8, data, m_mem.m_vmsize);

	m_tr.total = 0; // TODO: restore transfer state

	for (GIFPath& path : m_path)
	{
		ReadState(&path.tag, data);
		ReadState(&path.reg, data);

		path.SetTag(&path.tag); // expand regs
	}

	ReadState(&m_q, data);

	PRIM = &m_env.PRIM;

	UpdateContext();

	UpdateVertexKick();

	m_env.UpdateDIMX();

	for (size_t i = 0; i < 2; i++)
	{
		m_env.CTXT[i].UpdateScissor();

		m_env.CTXT[i].offset.fb = m_mem.GetOffset(m_env.CTXT[i].FRAME.Block(), m_env.CTXT[i].FRAME.FBW, m_env.CTXT[i].FRAME.PSM);
		m_env.CTXT[i].offset.zb = m_mem.GetOffset(m_env.CTXT[i].ZBUF.Block(), m_env.CTXT[i].FRAME.FBW, m_env.CTXT[i].ZBUF.PSM);
		m_env.CTXT[i].offset.tex = m_mem.GetOffset(m_env.CTXT[i].TEX0.TBP0, m_env.CTXT[i].TEX0.TBW, m_env.CTXT[i].TEX0.PSM);
		m_env.CTXT[i].offset.fzb = m_mem.GetPixelOffset(m_env.CTXT[i].FRAME, m_env.CTXT[i].ZBUF);
		m_env.CTXT[i].offset.fzb4 = m_mem.GetPixelOffset4(m_env.CTXT[i].FRAME, m_env.CTXT[i].ZBUF);
	}

	UpdateScissor();

	g_perfmon.SetFrame(5000);

	return 0;
}

void GSState::SetGameCRC(u32 crc, int options)
{
	m_crc = crc;
	m_options = options;
	m_game = CRC::Lookup(m_crc_hack_level != CRCHackLevel::Off ? crc : 0);
	SetupCrcHack();
}

//

void GSState::UpdateContext()
{
	const bool ctx_switch = (m_context != &m_env.CTXT[PRIM->CTXT]);

	if (ctx_switch)
		GL_REG("Context Switch %d", PRIM->CTXT);

	m_context = &m_env.CTXT[PRIM->CTXT];

	UpdateScissor();
}

void GSState::UpdateScissor()
{
	m_scissor = m_context->scissor.ex;
	m_ofxy = m_context->scissor.ofxy;
}

void GSState::UpdateVertexKick()
{
	if (m_frameskip)
		return;

	const u32 prim = PRIM->PRIM;

	m_fpGIFPackedRegHandlers[GIF_REG_XYZF2] = m_fpGIFPackedRegHandlerXYZ[prim][0];
	m_fpGIFPackedRegHandlers[GIF_REG_XYZF3] = m_fpGIFPackedRegHandlerXYZ[prim][1];
	m_fpGIFPackedRegHandlers[GIF_REG_XYZ2] = m_fpGIFPackedRegHandlerXYZ[prim][2];
	m_fpGIFPackedRegHandlers[GIF_REG_XYZ3] = m_fpGIFPackedRegHandlerXYZ[prim][3];

	m_fpGIFRegHandlers[GIF_A_D_REG_XYZF2] = m_fpGIFRegHandlerXYZ[prim][0];
	m_fpGIFRegHandlers[GIF_A_D_REG_XYZF3] = m_fpGIFRegHandlerXYZ[prim][1];
	m_fpGIFRegHandlers[GIF_A_D_REG_XYZ2] = m_fpGIFRegHandlerXYZ[prim][2];
	m_fpGIFRegHandlers[GIF_A_D_REG_XYZ3] = m_fpGIFRegHandlerXYZ[prim][3];

	m_fpGIFPackedRegHandlersC[GIF_REG_STQRGBAXYZF2] = m_fpGIFPackedRegHandlerSTQRGBAXYZF2[prim];
	m_fpGIFPackedRegHandlersC[GIF_REG_STQRGBAXYZ2] = m_fpGIFPackedRegHandlerSTQRGBAXYZ2[prim];
}

void GSState::GrowVertexBuffer()
{
	const size_t maxcount = std::max<size_t>(m_vertex.maxcount * 3 / 2, 10000);

	GSVertex* vertex = (GSVertex*)_aligned_malloc(sizeof(GSVertex) * maxcount, 32);
	u32* index = (u32*)_aligned_malloc(sizeof(u32) * maxcount * 3, 32); // worst case is slightly less than vertex number * 3

	if (vertex == NULL || index == NULL)
	{
		const size_t vert_byte_count = sizeof(GSVertex) * maxcount;
		const size_t idx_byte_count = sizeof(u32) * maxcount * 3;

		Console.Error("GS: failed to allocate %zu bytes for verticles and %zu for indices.",
			vert_byte_count, idx_byte_count);

		throw GSError();
	}

	if (m_vertex.buff != NULL)
	{
		memcpy(vertex, m_vertex.buff, sizeof(GSVertex) * m_vertex.tail);

		_aligned_free(m_vertex.buff);
	}

	if (m_index.buff != NULL)
	{
		memcpy(index, m_index.buff, sizeof(u32) * m_index.tail);

		_aligned_free(m_index.buff);
	}

	m_vertex.buff = vertex;
	m_vertex.maxcount = maxcount - 3; // -3 to have some space at the end of the buffer before DrawingKick can grow it
	m_index.buff = index;
}

GSState::PRIM_OVERLAP GSState::PrimitiveOverlap()
{
	// Either 1 triangle or 1 line or 3 POINTs
	// It is bad for the POINTs but low probability that they overlap
	if (m_vertex.next < 4)
		return PRIM_OVERLAP_NO;

	if (m_vt.m_primclass != GS_SPRITE_CLASS)
		return PRIM_OVERLAP_UNKNOW; // maybe, maybe not

	// Check intersection of sprite primitive only
	const size_t count = m_vertex.next;
	PRIM_OVERLAP overlap = PRIM_OVERLAP_NO;
	const GSVertex* v = m_vertex.buff;

	m_drawlist.clear();
	size_t i = 0;
	while (i < count)
	{
		// In order to speed up comparison a bounding-box is accumulated. It removes a
		// loop so code is much faster (check game virtua fighter). Besides it allow to check
		// properly the Y order.

		// .x = min(v[i].XYZ.X, v[i+1].XYZ.X)
		// .y = min(v[i].XYZ.Y, v[i+1].XYZ.Y)
		// .z = max(v[i].XYZ.X, v[i+1].XYZ.X)
		// .w = max(v[i].XYZ.Y, v[i+1].XYZ.Y)
		GSVector4i all = GSVector4i(v[i].m[1]).upl16(GSVector4i(v[i + 1].m[1])).upl16().xzyw();
		all = all.xyxy().blend(all.zwzw(), all > all.zwxy());

		size_t j = i + 2;
		while (j < count)
		{
			GSVector4i sprite = GSVector4i(v[j].m[1]).upl16(GSVector4i(v[j + 1].m[1])).upl16().xzyw();
			sprite = sprite.xyxy().blend(sprite.zwzw(), sprite > sprite.zwxy());

			// Be sure to get vertex in good order, otherwise .r* function doesn't
			// work as expected.
			ASSERT(sprite.x <= sprite.z);
			ASSERT(sprite.y <= sprite.w);
			ASSERT(all.x <= all.z);
			ASSERT(all.y <= all.w);

			if (all.rintersect(sprite).rempty())
			{
				all = all.runion_ordered(sprite);
			}
			else
			{
				overlap = PRIM_OVERLAP_YES;
				break;
			}
			j += 2;
		}
		m_drawlist.push_back((j - i) >> 1); // Sprite count
		i = j;
	}

#if 0
	// Old algo: less constraint but O(n^2) instead of O(n) as above

	// You have no guarantee on the sprite order, first vertex can be either top-left or bottom-left
	// There is a high probability that the draw call will uses same ordering for all vertices.
	// In order to keep a small performance impact only the first sprite will be checked
	//
	// Some safe-guard will be added in the outer-loop to avoid corruption with a limited perf impact
	if (v[1].XYZ.Y < v[0].XYZ.Y) {
		// First vertex is Top-Left
		for (size_t i = 0; i < count; i += 2) {
			if (v[i + 1].XYZ.Y > v[i].XYZ.Y) {
				return PRIM_OVERLAP_UNKNOW;
			}
			GSVector4i vi(v[i].XYZ.X, v[i + 1].XYZ.Y, v[i + 1].XYZ.X, v[i].XYZ.Y);
			for (size_t j = i + 2; j < count; j += 2) {
				GSVector4i vj(v[j].XYZ.X, v[j + 1].XYZ.Y, v[j + 1].XYZ.X, v[j].XYZ.Y);
				GSVector4i inter = vi.rintersect(vj);
				if (!inter.rempty()) {
					return PRIM_OVERLAP_YES;
				}
			}
		}
	}
	else {
		// First vertex is Bottom-Left
		for (size_t i = 0; i < count; i += 2) {
			if (v[i + 1].XYZ.Y < v[i].XYZ.Y) {
				return PRIM_OVERLAP_UNKNOW;
			}
			GSVector4i vi(v[i].XYZ.X, v[i].XYZ.Y, v[i + 1].XYZ.X, v[i + 1].XYZ.Y);
			for (size_t j = i + 2; j < count; j += 2) {
				GSVector4i vj(v[j].XYZ.X, v[j].XYZ.Y, v[j + 1].XYZ.X, v[j + 1].XYZ.Y);
				GSVector4i inter = vi.rintersect(vj);
				if (!inter.rempty()) {
					return PRIM_OVERLAP_YES;
				}
			}
		}
	}
#endif

	// fprintf(stderr, "%d: Yes, code can be optimized (draw of %d vertices)\n", s_n, count);
	return overlap;
}

__forceinline void GSState::HandleAutoFlush()
{
	const u32 frame_mask = GSLocalMemory::m_psm[m_context->TEX0.PSM].fmsk;
	const bool frame_hit = (m_context->FRAME.Block() == m_context->TEX0.TBP0) && !(m_context->TEST.ATE && m_context->TEST.ATST == 0 && m_context->TEST.AFAIL == 2) && ((m_context->FRAME.FBMSK & frame_mask) != frame_mask);
	// There's a strange behaviour we need to test on a PS2 here, if the FRAME is a Z format, like Powerdrome something swaps over, and it seems Alpha Fail of "FB Only" writes to the Z.. it's odd.
	const bool zbuf_hit = (m_context->ZBUF.Block() == m_context->TEX0.TBP0) && !(m_context->TEST.ATE && m_context->TEST.ATST == 0 && m_context->TEST.AFAIL != 2) && !m_context->ZBUF.ZMSK;
	const u32 frame_z_psm = frame_hit ? m_context->FRAME.PSM : m_context->ZBUF.PSM;
	const u32 frame_z_bp = frame_hit ? m_context->FRAME.Block() : m_context->ZBUF.Block();

	// To briefly explain what's going on here, what we are checking for is draws over a texture when the source and destination are themselves.
	// Because one page of the texture gets buffered in the Texture Cache (the PS2's one) if any of those pixels are overwritten, you still read the old data.
	// So we need to calculate if a page boundary is being crossed for the format it is in and if the same part of the texture being written and read inside the draw.
	if (PRIM->TME && (frame_hit || zbuf_hit) && GSUtil::HasSharedBits(frame_z_bp, frame_z_psm, m_context->TEX0.TBP0, m_context->TEX0.PSM))
	{
		size_t n = 1;

		switch (GSUtil::GetPrimClass(PRIM->PRIM))
		{
			case GS_POINT_CLASS:
			case GS_INVALID_CLASS:
				n = 1;
				break;
			case GS_LINE_CLASS:
			case GS_SPRITE_CLASS:
				n = 2;
				break;
			case GS_TRIANGLE_CLASS:
				n = 3;
				break;
		}

		const int current_tex_end = (int)(m_vertex.tail - (m_vertex.tail % n)) - 1;

		// Make sure including the new vert we have the whole rect to be drawn
		// Example being a sprite which is 2 verts
		// Tail = 3 (aka we have one sprite and 1 vert already saved, plus the incoming one)
		// current_tex_end will be 1 ((3 - (3 % 2 == 1) == 2) - 1), meaning 3-1 = 2, so we have enough data for the full rect.
		if (((m_vertex.tail - current_tex_end) < n) && PRIM->PRIM != GS_POINTLIST)
			return;

		const int page_mask_x = ~(GSLocalMemory::m_psm[m_context->TEX0.PSM].pgs.x - 1);
		const int page_mask_y = ~(GSLocalMemory::m_psm[m_context->TEX0.PSM].pgs.y - 1);
		const GSVector4i page_mask = { page_mask_x, page_mask_y, page_mask_x, page_mask_y };

		GSVector4i tex_coord;
		// Prepare the currently processed vertex.
		if (PRIM->FST)
		{
			tex_coord.x = m_v.U >> 4;
			tex_coord.y = m_v.V >> 4;
		}
		else
		{
			tex_coord.x = (int)((1 << m_context->TEX0.TW) * (m_v.ST.S / m_v.RGBAQ.Q));
			tex_coord.y = (int)((1 << m_context->TEX0.TH) * (m_v.ST.T / m_v.RGBAQ.Q));
		}

		GSVector4i tex_rect = tex_coord.xyxy();

		// Get the rest of the rect.
		for (int i = m_vertex.tail - 1; i > current_tex_end; i--)
		{
			const GSVertex* v = &m_vertex.buff[i];

			if (PRIM->FST)
			{
				tex_coord.x = v->U >> 4;
				tex_coord.y = v->V >> 4;
			}
			else
			{
				tex_coord.x = (int)((1 << m_context->TEX0.TW) * (v->ST.S / v->RGBAQ.Q));
				tex_coord.y = (int)((1 << m_context->TEX0.TH) * (v->ST.T / v->RGBAQ.Q));
			}

			tex_rect.x = std::min(tex_rect.x, tex_coord.x);
			tex_rect.z = std::max(tex_rect.z, tex_coord.x);
			tex_rect.y = std::min(tex_rect.y, tex_coord.y);
			tex_rect.w = std::max(tex_rect.w, tex_coord.y);
		}

		// Get the last texture position from the last draw.
		const GSVertex* v = &m_vertex.buff[m_index.buff[m_index.tail - 1]];

		if (PRIM->FST)
		{
			tex_coord.x = v->U >> 4;
			tex_coord.y = v->V >> 4;
		}
		else
		{
			tex_coord.x = (int)((1 << m_context->TEX0.TW) * (v->ST.S / v->RGBAQ.Q));
			tex_coord.y = (int)((1 << m_context->TEX0.TH) * (v->ST.T / v->RGBAQ.Q));
		}

		const GSVector4i pages = tex_rect & page_mask;

		tex_coord = tex_coord & page_mask;

		bool page_crossed = false;

		if (!pages.xyzw().eq(tex_coord.xyxy()))
			page_crossed = true;

		if(page_crossed)
		{
			// Make sure the format matches, otherwise the coordinates aren't gonna match, so the draws won't intersect.
			if (GSUtil::HasCompatibleBits(m_context->TEX0.PSM, frame_z_psm) && (m_context->FRAME.FBW == m_context->TEX0.TBW))
			{
				// Update the vertex trace, scissor it (important for Jak 3!) and intersect with the current texture.
				m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail - m_vertex.head, m_index.tail, GSUtil::GetPrimClass(PRIM->PRIM));

				// Intersect goes on space inside the rect
				GSVector4i area_out = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p));
				// Scissor output
				area_out = area_out.rintersect(GSVector4i(m_context->scissor.in));
				// Intersect with texture
				if (!area_out.rintersect(tex_rect).rempty())
					Flush();
			}
			else // Storage of the TEX and FRAME/Z is different, so uhh, just fall back to flushing each page. It's slower, sorry.
			{
				if (m_context->FRAME.FBW == m_context->TEX0.TBW)
				{
					//We know we've changed page, so let's set the dimension to cover the page they're in (for different pixel orders)
					tex_rect = tex_rect & page_mask;
					tex_rect += GSVector4i(0, 0, 1, 1); // Intersect goes on space inside the rect
					tex_rect.z += GSLocalMemory::m_psm[m_context->TEX0.PSM].pgs.x;
					tex_rect.w += GSLocalMemory::m_psm[m_context->TEX0.PSM].pgs.y;

					m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail - m_vertex.head, m_index.tail, GSUtil::GetPrimClass(PRIM->PRIM));

					GSVector4i area_out = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(m_context->scissor.in));
					area_out = area_out & page_mask;
					area_out += GSVector4i(0, 0, 1, 1); // Intersect goes on space inside the rect
					area_out.z += GSLocalMemory::m_psm[m_context->TEX0.PSM].pgs.x;
					area_out.w += GSLocalMemory::m_psm[m_context->TEX0.PSM].pgs.y;

					if (!area_out.rintersect(tex_rect).rempty())
						Flush();
				}
				else // Page width is different, so it's much more difficult to calculate where it's modifying.
					Flush();
			}
		}
	}
}

template <u32 prim, bool auto_flush, bool index_swap>
__forceinline void GSState::VertexKick(u32 skip)
{
	size_t n = 0;

	switch (prim)
	{
		case GS_POINTLIST:
		case GS_INVALID:
			n = 1;
			break;
		case GS_LINELIST:
		case GS_SPRITE:
		case GS_LINESTRIP:
			n = 2;
			break;
		case GS_TRIANGLELIST:
		case GS_TRIANGLESTRIP:
		case GS_TRIANGLEFAN:
			n = 3;
			break;
	}

	ASSERT(m_vertex.tail < m_vertex.maxcount + 3);

	if (auto_flush && m_index.tail >= n && !skip)
	{
		HandleAutoFlush();
	}

	size_t head = m_vertex.head;
	size_t tail = m_vertex.tail;
	size_t next = m_vertex.next;
	size_t xy_tail = m_vertex.xy_tail;

	// callers should write XYZUVF to m_v.m[1] in one piece to have this load store-forwarded, either by the cpu or the compiler when this function is inlined

	GSVector4i v0(m_v.m[0]);
	GSVector4i v1(m_v.m[1]);

	GSVector4i* RESTRICT tailptr = (GSVector4i*)&m_vertex.buff[tail];

	tailptr[0] = v0;
	tailptr[1] = v1;

	const GSVector4i xy = v1.xxxx().u16to32().sub32(m_ofxy);

	GSVector4i::storel(&m_vertex.xy[xy_tail & 3], xy.blend16<0xf0>(xy.sra32(4)).ps32());

	m_vertex.tail = ++tail;
	m_vertex.xy_tail = ++xy_tail;

	size_t m = tail - head;

	if (m < n)
		return;

	if (skip == 0 && (prim != GS_TRIANGLEFAN || m <= 4)) // m_vertex.xy only knows about the last 4 vertices, head could be far behind for fan
	{
		GSVector4i v0, v1, v2, v3, pmin, pmax;

		v0 = GSVector4i::loadl(&m_vertex.xy[(xy_tail + 1) & 3]); // T-3
		v1 = GSVector4i::loadl(&m_vertex.xy[(xy_tail + 2) & 3]); // T-2
		v2 = GSVector4i::loadl(&m_vertex.xy[(xy_tail + 3) & 3]); // T-1
		v3 = GSVector4i::loadl(&m_vertex.xy[(xy_tail - m) & 3]); // H

		switch (prim)
		{
			case GS_POINTLIST:
				pmin = v2;
				pmax = v2;
				break;
			case GS_LINELIST:
			case GS_LINESTRIP:
			case GS_SPRITE:
				pmin = v2.min_i16(v1);
				pmax = v2.max_i16(v1);
				break;
			case GS_TRIANGLELIST:
			case GS_TRIANGLESTRIP:
				pmin = v2.min_i16(v1.min_i16(v0));
				pmax = v2.max_i16(v1.max_i16(v0));
				break;
			case GS_TRIANGLEFAN:
				pmin = v2.min_i16(v1.min_i16(v3));
				pmax = v2.max_i16(v1.max_i16(v3));
				break;
			default:
				break;
		}

		GSVector4i test = pmax.lt16(m_scissor) | pmin.gt16(m_scissor.zwzwl());

		switch (prim)
		{
			case GS_TRIANGLELIST:
			case GS_TRIANGLESTRIP:
			case GS_TRIANGLEFAN:
			case GS_SPRITE:
				// FIXME: GREG I don't understand the purpose of the m_nativeres check
				// It impacts badly the number of draw call in the HW renderer.
				test |= m_nativeres ? pmin.eq16(pmax).zwzwl() : pmin.eq16(pmax);
				break;
			default:
				break;
		}

		switch (prim)
		{
			case GS_TRIANGLELIST:
			case GS_TRIANGLESTRIP:
				// TODO: any way to do a 16-bit integer cross product?
				// cross product is zero most of the time because either of the vertices are the same
				test = (test | v0 == v1) | (v1 == v2 | v0 == v2);
				break;
			case GS_TRIANGLEFAN:
				test = (test | v3 == v1) | (v1 == v2 | v3 == v2);
				break;
			default:
				break;
		}

		skip |= test.mask() & 15;
	}

	if (skip != 0)
	{
		switch (prim)
		{
			case GS_POINTLIST:
			case GS_LINELIST:
			case GS_TRIANGLELIST:
			case GS_SPRITE:
			case GS_INVALID:
				m_vertex.tail = head; // no need to check or grow the buffer length
				break;
			case GS_LINESTRIP:
			case GS_TRIANGLESTRIP:
				m_vertex.head = head + 1;
				[[fallthrough]];
			case GS_TRIANGLEFAN:
				if (tail >= m_vertex.maxcount)
					GrowVertexBuffer(); // in case too many vertices were skipped
				break;
			default:
				__assume(0);
		}

		return;
	}

	if (tail >= m_vertex.maxcount)
		GrowVertexBuffer();

	u32* RESTRICT buff = &m_index.buff[m_index.tail];

	switch (prim)
	{
		case GS_POINTLIST:
			buff[0] = head + 0;
			m_vertex.head = head + 1;
			m_vertex.next = head + 1;
			m_index.tail += 1;
			break;
		case GS_LINELIST:
			buff[0] = head + (index_swap ? 1 : 0);
			buff[1] = head + (index_swap ? 0 : 1);
			m_vertex.head = head + 2;
			m_vertex.next = head + 2;
			m_index.tail += 2;
			break;
		case GS_LINESTRIP:
			if (next < head)
			{
				m_vertex.buff[next + 0] = m_vertex.buff[head + 0];
				m_vertex.buff[next + 1] = m_vertex.buff[head + 1];
				head = next;
				m_vertex.tail = next + 2;
			}
			buff[0] = head + (index_swap ? 1 : 0);
			buff[1] = head + (index_swap ? 0 : 1);
			m_vertex.head = head + 1;
			m_vertex.next = head + 2;
			m_index.tail += 2;
			break;
		case GS_TRIANGLELIST:
			buff[0] = head + (index_swap ? 2 : 0);
			buff[1] = head + 1;
			buff[2] = head + (index_swap ? 0 : 2);
			m_vertex.head = head + 3;
			m_vertex.next = head + 3;
			m_index.tail += 3;
			break;
		case GS_TRIANGLESTRIP:
			if (next < head)
			{
				m_vertex.buff[next + 0] = m_vertex.buff[head + 0];
				m_vertex.buff[next + 1] = m_vertex.buff[head + 1];
				m_vertex.buff[next + 2] = m_vertex.buff[head + 2];
				head = next;
				m_vertex.tail = next + 3;
			}
			buff[0] = head + (index_swap ? 2 : 0);
			buff[1] = head + 1;
			buff[2] = head + (index_swap ? 0 : 2);
			m_vertex.head = head + 1;
			m_vertex.next = head + 3;
			m_index.tail += 3;
			break;
		case GS_TRIANGLEFAN:
			// TODO: remove gaps, next == head && head < tail - 3 || next > head && next < tail - 2 (very rare)
			buff[0] = index_swap ? (tail - 1) : (head + 0);
			buff[1] = tail - 2;
			buff[2] = index_swap ? (head + 0) : (tail - 1);
			m_vertex.next = tail;
			m_index.tail += 3;
			break;
		case GS_SPRITE:
			buff[0] = head + 0;
			buff[1] = head + 1;
			m_vertex.head = head + 2;
			m_vertex.next = head + 2;
			m_index.tail += 2;
			break;
		case GS_INVALID:
			m_vertex.tail = head;
			break;
		default:
			__assume(0);
	}
}

/// Checks if region repeat is used (applying it does something to at least one of the values in min...max)
/// Also calculates the real min and max values seen after applying the region repeat to all values in min...max
static bool UsesRegionRepeat(int fix, int msk, int min, int max, int* min_out, int* max_out)
{
	if ((min < 0) != (max < 0))
	{
		// Algorithm doesn't work properly if bits overflow when incrementing (happens on the -1 → 0 crossing)
		// Conveniently, crossing zero guarantees you use the full range
		*min_out = fix;
		*max_out = (fix | msk) + 1;
		return true;
	}

	const int cleared_bits = ~msk & ~fix; // Bits that are always cleared by applying msk and fix
	const int set_bits = fix; // Bits that are always set by applying msk and fix
	unsigned long msb;
	int variable_bits = min ^ max;
	if (_BitScanReverse(&msb, variable_bits))
		variable_bits |= (1 << msb) - 1; // Fill in all lower bits

	const int always_set = min & ~variable_bits;   // Bits that are set in every value in min...max
	const int sometimes_set = min | variable_bits; // Bits that are set in at least one value in min...max

	const bool sets_bits = (set_bits | always_set) != always_set; // At least one bit in min...max is set by applying msk and fix
	const bool clears_bits = (cleared_bits & sometimes_set) != 0; // At least one bit in min...max is cleared by applying msk and fix

	const int overwritten_variable_bits = (cleared_bits | set_bits) & variable_bits;
	// A variable bit that's `0` in `min` will at some point switch to a `1` (because it's variable)
	// When it does, all bits below it will switch to a `0` (that's how incrementing works)
	// If the 0 to 1 switch is reflected in the final output (not masked and not replaced by a fixed value),
	// the final value would be larger than the previous.  Otherwise, the final value will be less.
	// The true minimum value is `min` with all bits below the most significant replaced variable `0` bit cleared
	const int min_overwritten_variable_zeros = ~min & overwritten_variable_bits;
	if (_BitScanReverse(&msb, min_overwritten_variable_zeros))
		min &= (~0u << msb);
	// Similar thing for max, but the first masked `1` bit
	const int max_overwritten_variable_ones = max & overwritten_variable_bits;
	if (_BitScanReverse(&msb, max_overwritten_variable_ones))
		max |= (1 << msb) - 1;

	*min_out = (msk & min) | fix;
	*max_out = ((msk & max) | fix) + 1;

	return sets_bits || clears_bits;
}

GSState::TextureMinMaxResult GSState::GetTextureMinMax(const GIFRegTEX0& TEX0, const GIFRegCLAMP& CLAMP, bool linear)
{
	// TODO: some of the +1s can be removed if linear == false

	const int tw = TEX0.TW;
	const int th = TEX0.TH;

	const int w = 1 << tw;
	const int h = 1 << th;
	const int tw_mask = w - 1;
	const int th_mask = h - 1;

	GSVector4i tr(0, 0, w, h);

	const int wms = CLAMP.WMS;
	const int wmt = CLAMP.WMT;

	const int minu = (int)CLAMP.MINU;
	const int minv = (int)CLAMP.MINV;
	const int maxu = (int)CLAMP.MAXU;
	const int maxv = (int)CLAMP.MAXV;

	GSVector4i vr = tr;

	switch (wms)
	{
		case CLAMP_REPEAT:
			break;
		case CLAMP_CLAMP:
			break;
		case CLAMP_REGION_CLAMP:
			if (vr.x < minu)
				vr.x = minu;
			if (vr.z > maxu + 1)
				vr.z = maxu + 1;
			break;
		case CLAMP_REGION_REPEAT:
			vr.x = maxu;
			vr.z = (maxu | minu) + 1;
			break;
		default:
			__assume(0);
	}

	switch (wmt)
	{
		case CLAMP_REPEAT:
			break;
		case CLAMP_CLAMP:
			break;
		case CLAMP_REGION_CLAMP:
			if (vr.y < minv)
				vr.y = minv;
			if (vr.w > maxv + 1)
				vr.w = maxv + 1;
			break;
		case CLAMP_REGION_REPEAT:
			vr.y = maxv;
			vr.w = (maxv | minv) + 1;
			break;
		default:
			__assume(0);
	}

	u8 uses_border = 0;

	if (m_vt.m_max.t.x >= FLT_MAX || m_vt.m_min.t.x <= -FLT_MAX ||
		m_vt.m_max.t.y >= FLT_MAX || m_vt.m_min.t.y <= -FLT_MAX)
	{
		// If any of the min/max values are +-FLT_MAX we can't rely on them
		// so just assume full texture.
		uses_border = 0xF;
	}
	else
	{
		// Optimisation aims to reduce the amount of texture loaded to only the bit which will be read
		GSVector4 st = m_vt.m_min.t.xyxy(m_vt.m_max.t);
		if (linear)
			st += GSVector4(-0.5f, 0.5f).xxyy();

		// Adjust texture range when sprites get scissor clipped. Since we linearly interpolate, this
		// optimization doesn't work when perspective correction is enabled.
		if (m_vt.m_primclass == GS_SPRITE_CLASS && PRIM->FST == 1 && m_index.tail < 3)
		{
			// When coordinates are fractional, GS appears to draw to the right/bottom (effectively
			// taking the ceiling), not to the top/left (taking the floor). 
			const GSVector4i int_rc(m_vt.m_min.p.ceil().xyxy(m_vt.m_max.p.floor()));
			const GSVector4i scissored_rc(int_rc.rintersect(GSVector4i(m_context->scissor.in)));
			if (!int_rc.eq(scissored_rc))
			{
				// draw will get scissored, adjust UVs to suit
				const GSVector2 pos_range(m_vt.m_max.p.x - m_vt.m_min.p.x, m_vt.m_max.p.y - m_vt.m_min.p.y);
				const GSVector2 uv_range(m_vt.m_max.t.x - m_vt.m_min.t.x, m_vt.m_max.t.y - m_vt.m_min.t.y);
				const GSVector2 grad(uv_range / pos_range);

				const GSVertex* vert_first = &m_vertex.buff[m_index.buff[0]];
				const GSVertex* vert_second = &m_vertex.buff[m_index.buff[1]];

				// we need to check that it's not going to repeat over the non-clipped part
				if (wms != CLAMP_REGION_REPEAT && (wms != CLAMP_REPEAT || (static_cast<int>(st.x) & ~tw_mask) == (static_cast<int>(st.z) & ~tw_mask)))
				{
					// Check if the UV coords are going in a different direction to the verts, if they match direction, no need to swap
					const bool u_forward = vert_first->U < vert_second->U;
					const bool x_forward = vert_first->XYZ.X < vert_second->XYZ.X;
					const bool swap_x = u_forward != x_forward;

					if (int_rc.left < scissored_rc.left)
					{
						if(!swap_x)
							st.x += floor(static_cast<float>(scissored_rc.left - int_rc.left) * grad.x);
						else
							st.z -= floor(static_cast<float>(scissored_rc.left - int_rc.left) * grad.x);
					}
					if (int_rc.right > scissored_rc.right)
					{
						if (!swap_x)
							st.z -= floor(static_cast<float>(int_rc.right - scissored_rc.right) * grad.x);
						else
							st.x += floor(static_cast<float>(int_rc.right - scissored_rc.right) * grad.x);
					}
				}
				if (wmt != CLAMP_REGION_REPEAT && (wmt != CLAMP_REPEAT || (static_cast<int>(st.y) & ~th_mask) == (static_cast<int>(st.w) & ~th_mask)))
				{
					// Check if the UV coords are going in a different direction to the verts, if they match direction, no need to swap
					const bool v_forward = vert_first->V < vert_second->V;
					const bool y_forward = vert_first->XYZ.Y < vert_second->XYZ.Y;
					const bool swap_y = v_forward != y_forward;

					if (int_rc.top < scissored_rc.top)
					{
						if (!swap_y)
							st.y += floor(static_cast<float>(scissored_rc.top - int_rc.top) * grad.y);
						else
							st.w -= floor(static_cast<float>(scissored_rc.top - int_rc.top) * grad.y);
					}
					if (int_rc.bottom > scissored_rc.bottom)
					{
						if (!swap_y)
							st.w -= floor(static_cast<float>(int_rc.bottom - scissored_rc.bottom) * grad.y);
						else
							st.y += floor(static_cast<float>(int_rc.bottom - scissored_rc.bottom) * grad.y);
					}
				}
			}
		}

		GSVector4i uv = GSVector4i(st.floor());
		uses_border = GSVector4::cast((uv < vr).blend32<0xc>(uv >= vr)).mask();

		// Roughly cut out the min/max of the read (Clamp)

		switch (wms)
		{
			case CLAMP_REPEAT:
				if ((uv.x & ~tw_mask) == (uv.z & ~tw_mask))
				{
					vr.x = std::max(vr.x, uv.x & tw_mask);
					vr.z = std::min(vr.z, (uv.z & tw_mask) + 1);
				}
				break;
			case CLAMP_CLAMP:
			case CLAMP_REGION_CLAMP:
				if (vr.x < uv.x)
					vr.x = std::min(uv.x, vr.z - 1);
				if (vr.z > (uv.z + 1))
					vr.z = std::max(uv.z, vr.x) + 1;
				break;
			case CLAMP_REGION_REPEAT:
				if (UsesRegionRepeat(maxu, minu, uv.x, uv.z, &vr.x, &vr.z) || maxu >= tw)
					uses_border |= TextureMinMaxResult::USES_BOUNDARY_U;
				break;
		}

		switch (wmt)
		{
			case CLAMP_REPEAT:
				if ((uv.y & ~th_mask) == (uv.w & ~th_mask))
				{
					vr.y = std::max(vr.y, uv.y & th_mask);
					vr.w = std::min(vr.w, (uv.w & th_mask) + 1);
				}
				break;
			case CLAMP_CLAMP:
			case CLAMP_REGION_CLAMP:
				if (vr.y < uv.y)
					vr.y = std::min(uv.y, vr.w - 1);
				if (vr.w > (uv.w + 1))
					vr.w = std::max(uv.w, vr.y) + 1;
				break;
			case CLAMP_REGION_REPEAT:
				if (UsesRegionRepeat(maxv, minv, uv.y, uv.w, &vr.y, &vr.w) || maxv >= th)
					uses_border |= TextureMinMaxResult::USES_BOUNDARY_V;
				break;
		}
	}

	vr = vr.rintersect(tr);

	// This really shouldn't happen now except with the clamping region set entirely outside the texture,
	// special handling should be written for that case.
	if (vr.rempty())
	{
		// NOTE: this can happen when texcoords are all outside the texture or clamping area is zero, but we can't
		// let the texture cache update nothing, the sampler will still need a single texel from the border somewhere
		// examples:
		// - THPS (no visible problems)
		// - NFSMW (strange rectangles on screen, might be unrelated)
		// - Lupin 3rd (huge problems, textures sizes seem to be randomly specified)

		vr = (vr + GSVector4i(-1, +1).xxyy()).rintersect(tr);
	}

	return { vr, uses_border };
}

void GSState::CalcAlphaMinMax()
{
	if (m_vt.m_alpha.valid)
		return;

	const GSDrawingEnvironment& env = m_env;
	const GSDrawingContext* context = m_context;

	GSVector4i a = m_vt.m_min.c.uph32(m_vt.m_max.c).zzww();

	if (PRIM->TME && context->TEX0.TCC)
	{
		switch (GSLocalMemory::m_psm[context->TEX0.PSM].fmt)
		{
			case 0:
				a.y = 0;
				a.w = 0xff;
				break;
			case 1:
				a.y = env.TEXA.AEM ? 0 : env.TEXA.TA0;
				a.w = env.TEXA.TA0;
				break;
			case 2:
				a.y = env.TEXA.AEM ? 0 : std::min(env.TEXA.TA0, env.TEXA.TA1);
				a.w = std::max(env.TEXA.TA0, env.TEXA.TA1);
				break;
			case 3:
				m_mem.m_clut.GetAlphaMinMax32(a.y, a.w);
				break;
			default:
				__assume(0);
		}

		switch (context->TEX0.TFX)
		{
			case TFX_MODULATE:
				a.x = (a.x * a.y) >> 7;
				a.z = (a.z * a.w) >> 7;
				if (a.x > 0xff)
					a.x = 0xff;
				if (a.z > 0xff)
					a.z = 0xff;
				break;
			case TFX_DECAL:
				a.x = a.y;
				a.z = a.w;
				break;
			case TFX_HIGHLIGHT:
				a.x = a.x + a.y;
				a.z = a.z + a.w;
				if (a.x > 0xff)
					a.x = 0xff;
				if (a.z > 0xff)
					a.z = 0xff;
				break;
			case TFX_HIGHLIGHT2:
				a.x = a.y;
				a.z = a.w;
				break;
			default:
				__assume(0);
		}
	}

	m_vt.m_alpha.min = a.x;
	m_vt.m_alpha.max = a.z;
	m_vt.m_alpha.valid = true;
}

bool GSState::TryAlphaTest(u32& fm, const u32 fm_mask, u32& zm)
{
	// Shortcut for the easy case
	if (m_context->TEST.ATST == ATST_ALWAYS)
		return true;

	const u32 framemask = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmsk;
	const u32 framemaskalpha = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmsk & 0xFF000000;
	// Alpha test can only control the write of some channels. If channels are already masked
	// the alpha test is therefore a nop.
	switch (m_context->TEST.AFAIL)
	{
		case AFAIL_KEEP:
			break;
		case AFAIL_FB_ONLY:
			if (zm == 0xFFFFFFFF)
				return true;
			break;
		case AFAIL_ZB_ONLY:
			if ((fm & framemask) == framemask)
				return true;
			break;
		case AFAIL_RGB_ONLY:
			if (zm == 0xFFFFFFFF && ((fm & framemaskalpha) == framemaskalpha || GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt == 1))
				return true;
			break;
		default:
			__assume(0);
	}

	bool pass = true;

	if (m_context->TEST.ATST == ATST_NEVER)
	{
		pass = false; // Shortcut to avoid GetAlphaMinMax below
	}
	else
	{
		const GSVertexTrace::VertexAlpha& aminmax = GetAlphaMinMax();
		const int amin = aminmax.min;
		const int amax = aminmax.max;

		const int aref = m_context->TEST.AREF;

		switch (m_context->TEST.ATST)
		{
			case ATST_NEVER:
				pass = false;
				break;
			case ATST_ALWAYS:
				pass = true;
				break;
			case ATST_LESS:
				if (amax < aref)
					pass = true;
				else if (amin >= aref)
					pass = false;
				else
					return false;
				break;
			case ATST_LEQUAL:
				if (amax <= aref)
					pass = true;
				else if (amin > aref)
					pass = false;
				else
					return false;
				break;
			case ATST_EQUAL:
				if (amin == aref && amax == aref)
					pass = true;
				else if (amin > aref || amax < aref)
					pass = false;
				else
					return false;
				break;
			case ATST_GEQUAL:
				if (amin >= aref)
					pass = true;
				else if (amax < aref)
					pass = false;
				else
					return false;
				break;
			case ATST_GREATER:
				if (amin > aref)
					pass = true;
				else if (amax <= aref)
					pass = false;
				else
					return false;
				break;
			case ATST_NOTEQUAL:
				if (amin == aref && amax == aref)
					pass = false;
				else if (amin > aref || amax < aref)
					pass = true;
				else
					return false;
				break;
			default:
				__assume(0);
		}
	}

	if (!pass)
	{
		switch (m_context->TEST.AFAIL)
		{
			case AFAIL_KEEP:
				fm = zm = 0xffffffff;
				break;
			case AFAIL_FB_ONLY:
				zm = 0xffffffff;
				break;
			case AFAIL_ZB_ONLY:
				fm = 0xffffffff;
				break;
			case AFAIL_RGB_ONLY:
				fm |= 0xff000000;
				zm = 0xffffffff;
				break;
			default:
				__assume(0);
		}
	}

	return true;
}

bool GSState::IsOpaque()
{
	if (PRIM->AA1)
		return false;

	if (!PRIM->ABE)
		return true;

	const GSDrawingContext* context = m_context;

	int amin = 0;
	int amax = 0xff;

	if (context->ALPHA.A != context->ALPHA.B)
	{
		if (context->ALPHA.C == 0)
		{
			amin = GetAlphaMinMax().min;
			amax = GetAlphaMinMax().max;
		}
		else if (context->ALPHA.C == 1)
		{
			if (context->FRAME.PSM == PSM_PSMCT24 || context->FRAME.PSM == PSM_PSMZ24)
				amin = amax = 0x80;
		}
		else if (context->ALPHA.C == 2)
		{
			amin = amax = context->ALPHA.FIX;
		}
	}

	return context->ALPHA.IsOpaque(amin, amax);
}

bool GSState::IsMipMapDraw()
{
	return m_context->TEX1.MXL > 0 && m_context->TEX1.MMIN >= 2 && m_context->TEX1.MMIN <= 5 && m_vt.m_lod.y > 0;
}

bool GSState::IsMipMapActive()
{
	return m_mipmap && IsMipMapDraw();
}

GIFRegTEX0 GSState::GetTex0Layer(u32 lod)
{
	// Shortcut
	if (lod == 0)
		return m_context->TEX0;

	GIFRegTEX0 TEX0 = m_context->TEX0;

	switch (lod)
	{
		case 1:
			TEX0.TBP0 = m_context->MIPTBP1.TBP1;
			TEX0.TBW = m_context->MIPTBP1.TBW1;
			break;
		case 2:
			TEX0.TBP0 = m_context->MIPTBP1.TBP2;
			TEX0.TBW = m_context->MIPTBP1.TBW2;
			break;
		case 3:
			TEX0.TBP0 = m_context->MIPTBP1.TBP3;
			TEX0.TBW = m_context->MIPTBP1.TBW3;
			break;
		case 4:
			TEX0.TBP0 = m_context->MIPTBP2.TBP4;
			TEX0.TBW = m_context->MIPTBP2.TBW4;
			break;
		case 5:
			TEX0.TBP0 = m_context->MIPTBP2.TBP5;
			TEX0.TBW = m_context->MIPTBP2.TBW5;
			break;
		case 6:
			TEX0.TBP0 = m_context->MIPTBP2.TBP6;
			TEX0.TBW = m_context->MIPTBP2.TBW6;
			break;
		default:
			Console.Error("GS: Invalid guest lod setting. Please report: https://github.com/PCSX2/pcsx2/issues");
	}

	// Correct the texture size
	if (TEX0.TH <= lod)
		TEX0.TH = 0;
	else
		TEX0.TH -= lod;

	if (TEX0.TW <= lod)
		TEX0.TW = 0;
	else
		TEX0.TW -= lod;

	return TEX0;
}

// GSTransferBuffer

GSState::GSTransferBuffer::GSTransferBuffer()
{
	x = y = 0;
	overflow = false;
	start = end = total = 0;

	constexpr size_t alloc_size = 1024 * 1024 * 4;
	buff = reinterpret_cast<u8*>(_aligned_malloc(alloc_size, 32));
}

GSState::GSTransferBuffer::~GSTransferBuffer()
{
	_aligned_free(buff);
}

void GSState::GSTransferBuffer::Init(int tx, int ty, const GIFRegBITBLTBUF& blit)
{
	x = tx;
	y = ty;
	total = 0;
	m_blit = blit;
}

bool GSState::GSTransferBuffer::Update(int tw, int th, int bpp, int& len)
{
	if (total == 0)
	{
		start = end = 0;
		total = std::min<int>((tw * bpp >> 3) * th, 1024 * 1024 * 4);
		overflow = false;
	}

	const int remaining = total - end;

	if (len > remaining)
	{
		if (!overflow)
		{
			overflow = true;
#if defined(PCSX2_DEVBUILD) || defined(_DEBUG)
			Console.Warning("GS transfer buffer overflow");
#endif
		}

		len = remaining;
	}

	return len > 0;
}
