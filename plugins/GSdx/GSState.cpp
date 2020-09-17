/*
 *	Copyright (C) 2007-2016 Gabest
 *	http://www.gabest.org
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
#include "GSState.h"
#include "GSdx.h"
#include "GSUtil.h"

//#define Offset_ST  // Fixes Persona3 mini map alignment which is off even in software rendering

int GSState::s_n = 0;

GSState::GSState()
	: m_version(6)
	, m_mt(false)
	, m_irq(NULL)
	, m_path3hack(0)
	, m_init_read_fifo_supported(false)
	, m_gsc(NULL)
	, m_skip(0)
	, m_skip_offset(0)
	, m_q(1.0f)
	, m_texflush(true)
	, m_vt(this)
	, m_regs(NULL)
	, m_crc(0)
	, m_options(0)
	, m_frameskip(0)
{
	// m_nativeres seems to be a hack. Unfortunately it impacts draw call number which make debug painful in the replayer.
	// Let's keep it disabled to ease debug.
	m_nativeres             = theApp.GetConfigI("upscale_multiplier") == 1 || GLLoader::in_replayer;
	m_mipmap                = theApp.GetConfigI("mipmap");
	m_NTSC_Saturation       = theApp.GetConfigB("NTSC_Saturation");
	m_clut_load_before_draw = theApp.GetConfigB("clut_load_before_draw");
	if (theApp.GetConfigB("UserHacks"))
	{
		m_userhacks_auto_flush      = theApp.GetConfigB("UserHacks_AutoFlush");
		m_userhacks_wildhack        = theApp.GetConfigB("UserHacks_WildHack");
		m_userhacks_skipdraw        = theApp.GetConfigI("UserHacks_SkipDraw");
		m_userhacks_skipdraw_offset = theApp.GetConfigI("UserHacks_SkipDraw_Offset");
	}
	else
	{
		m_userhacks_auto_flush      = false;
		m_userhacks_wildhack        = false;
		m_userhacks_skipdraw        = 0;
		m_userhacks_skipdraw_offset = 0;
	}

	s_n = 0;
	s_dump  = theApp.GetConfigB("dump");
	s_save  = theApp.GetConfigB("save");
	s_savet = theApp.GetConfigB("savet");
	s_savez = theApp.GetConfigB("savez");
	s_savef = theApp.GetConfigB("savef");
	s_saven = theApp.GetConfigI("saven");
	s_savel = theApp.GetConfigI("savel");
	m_dump_root = "";
#if defined(__unix__)
	if (s_dump) {
		GSmkdir(root_hw.c_str());
		GSmkdir(root_sw.c_str());
	}
#endif

	//s_dump = 1;
	//s_save = 1;
	//s_savez = 1;
	//s_savet = 1;
	//s_savef = 1;
	//s_saven = 0;
	//s_savel = 0;

	m_crc_hack_level = theApp.GetConfigT<CRCHackLevel>("crc_hack_level");
	if (m_crc_hack_level == CRCHackLevel::Automatic)
		m_crc_hack_level = GSUtil::GetRecommendedCRCHackLevel(theApp.GetCurrentRendererType());

	memset(&m_v, 0, sizeof(m_v));
	memset(&m_vertex, 0, sizeof(m_vertex));
	memset(&m_index, 0, sizeof(m_index));

	m_v.RGBAQ.Q = 1.0f;

	GrowVertexBuffer();

	m_sssize = 0;

	m_sssize += sizeof(m_version);
	m_sssize += sizeof(m_env.PRIM);
	m_sssize += sizeof(m_env.PRMODE);
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

	for(int i = 0; i < 2; i++)
	{
		m_sssize += sizeof(m_env.CTXT[i].XYOFFSET);
		m_sssize += sizeof(m_env.CTXT[i].TEX0);
		m_sssize += sizeof(m_env.CTXT[i].TEX1);
		m_sssize += sizeof(m_env.CTXT[i].TEX2);
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
	m_sssize += (sizeof(m_path[0].tag) + sizeof(m_path[0].reg)) * countof(m_path);
	m_sssize += sizeof(m_q);

	PRIM = &m_env.PRIM;
//	CSR->rREV = 0x20;
	m_env.PRMODECONT.AC = 1;

	Reset();

	ResetHandlers();
}

GSState::~GSState()
{
	if(m_vertex.buff) _aligned_free(m_vertex.buff);
	if(m_index.buff) _aligned_free(m_index.buff);
}

void GSState::SetRegsMem(uint8* basemem)
{
	ASSERT(basemem);

	m_regs = (GSPrivRegSet*)basemem;
}

void GSState::SetIrqCallback(void (*irq)())
{
	m_irq = irq;
}

void GSState::SetMultithreaded(bool mt)
{
	// Some older versions of PCSX2 didn't properly set the irq callback to NULL
	// in multithreaded mode (possibly because ZeroGS itself would assert in such
	// cases), and didn't bind them to a dummy callback either.  PCSX2 handles all
	// IRQs internally when multithreaded anyway -- so let's ignore them here:

	m_mt = mt;

	if(mt)
	{
		m_fpGIFRegHandlers[GIF_A_D_REG_SIGNAL] = &GSState::GIFRegHandlerNull;
		m_fpGIFRegHandlers[GIF_A_D_REG_FINISH] = &GSState::GIFRegHandlerNull;
		m_fpGIFRegHandlers[GIF_A_D_REG_LABEL] = &GSState::GIFRegHandlerNull;
	}
	else
	{
		m_fpGIFRegHandlers[GIF_A_D_REG_SIGNAL] = &GSState::GIFRegHandlerSIGNAL;
		m_fpGIFRegHandlers[GIF_A_D_REG_FINISH] = &GSState::GIFRegHandlerFINISH;
		m_fpGIFRegHandlers[GIF_A_D_REG_LABEL] = &GSState::GIFRegHandlerLABEL;
	}
}

void GSState::SetFrameSkip(int skip)
{
	if(m_frameskip == skip) return;

	m_frameskip = skip;

	if(skip)
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

void GSState::Reset()
{
	//printf("GSdx info: GS reset\n");

	// FIXME: memset(m_mem.m_vm8, 0, m_mem.m_vmsize); // bios logo not shown cut in half after reset, missing graphics in GoW after first FMV
	memset(&m_path[0], 0, sizeof(m_path[0]) * countof(m_path));
	memset(&m_v, 0, sizeof(m_v));

//	PRIM = &m_env.PRIM;
//	m_env.PRMODECONT.AC = 1;

	m_env.Reset();

	PRIM = !m_env.PRMODECONT.AC ? (GIFRegPRIM*)&m_env.PRMODE : &m_env.PRIM;

	UpdateContext();

	UpdateVertexKick();

	m_env.UpdateDIMX();

	for(size_t i = 0; i < 2; i++)
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

	m_texflush = true;
}

void GSState::ResetHandlers()
{
	for(size_t i = 0; i < countof(m_fpGIFPackedRegHandlers); i++)
	{
		m_fpGIFPackedRegHandlers[i] = &GSState::GIFPackedRegHandlerNull;
	}

	m_fpGIFPackedRegHandlers[GIF_REG_PRIM] = (GIFPackedRegHandler)(GIFRegHandler)&GSState::GIFRegHandlerPRIM;
	m_fpGIFPackedRegHandlers[GIF_REG_RGBA] = &GSState::GIFPackedRegHandlerRGBA;
	m_fpGIFPackedRegHandlers[GIF_REG_STQ] = &GSState::GIFPackedRegHandlerSTQ;
	m_fpGIFPackedRegHandlers[GIF_REG_UV] = m_userhacks_wildhack ? &GSState::GIFPackedRegHandlerUV_Hack : &GSState::GIFPackedRegHandlerUV;
	m_fpGIFPackedRegHandlers[GIF_REG_TEX0_1] = (GIFPackedRegHandler)(GIFRegHandler)&GSState::GIFRegHandlerTEX0<0>;
	m_fpGIFPackedRegHandlers[GIF_REG_TEX0_2] = (GIFPackedRegHandler)(GIFRegHandler)&GSState::GIFRegHandlerTEX0<1>;
	m_fpGIFPackedRegHandlers[GIF_REG_CLAMP_1] = (GIFPackedRegHandler)(GIFRegHandler)&GSState::GIFRegHandlerCLAMP<0>;
	m_fpGIFPackedRegHandlers[GIF_REG_CLAMP_2] = (GIFPackedRegHandler)(GIFRegHandler)&GSState::GIFRegHandlerCLAMP<1>;
	m_fpGIFPackedRegHandlers[GIF_REG_FOG] = &GSState::GIFPackedRegHandlerFOG;
	m_fpGIFPackedRegHandlers[GIF_REG_A_D] = &GSState::GIFPackedRegHandlerA_D;
	m_fpGIFPackedRegHandlers[GIF_REG_NOP] = &GSState::GIFPackedRegHandlerNOP;

	#define SetHandlerXYZ(P, auto_flush) \
		m_fpGIFPackedRegHandlerXYZ[P][0] = &GSState::GIFPackedRegHandlerXYZF2<P, 0, auto_flush>; \
		m_fpGIFPackedRegHandlerXYZ[P][1] = &GSState::GIFPackedRegHandlerXYZF2<P, 1, auto_flush>; \
		m_fpGIFPackedRegHandlerXYZ[P][2] = &GSState::GIFPackedRegHandlerXYZ2<P, 0, auto_flush>; \
		m_fpGIFPackedRegHandlerXYZ[P][3] = &GSState::GIFPackedRegHandlerXYZ2<P, 1, auto_flush>; \
		m_fpGIFRegHandlerXYZ[P][0] = &GSState::GIFRegHandlerXYZF2<P, 0, auto_flush>; \
		m_fpGIFRegHandlerXYZ[P][1] = &GSState::GIFRegHandlerXYZF2<P, 1, auto_flush>; \
		m_fpGIFRegHandlerXYZ[P][2] = &GSState::GIFRegHandlerXYZ2<P, 0, auto_flush>; \
		m_fpGIFRegHandlerXYZ[P][3] = &GSState::GIFRegHandlerXYZ2<P, 1, auto_flush>; \
		m_fpGIFPackedRegHandlerSTQRGBAXYZF2[P] = &GSState::GIFPackedRegHandlerSTQRGBAXYZF2<P, auto_flush>; \
		m_fpGIFPackedRegHandlerSTQRGBAXYZ2[P] = &GSState::GIFPackedRegHandlerSTQRGBAXYZ2<P, auto_flush>; \

	if (m_userhacks_auto_flush) {
		SetHandlerXYZ(GS_POINTLIST, true);
		SetHandlerXYZ(GS_LINELIST, true);
		SetHandlerXYZ(GS_LINESTRIP, true);
		SetHandlerXYZ(GS_TRIANGLELIST, true);
		SetHandlerXYZ(GS_TRIANGLESTRIP, true);
		SetHandlerXYZ(GS_TRIANGLEFAN, true);
		SetHandlerXYZ(GS_SPRITE, true);
		SetHandlerXYZ(GS_INVALID, true);
	} else {
		SetHandlerXYZ(GS_POINTLIST, false);
		SetHandlerXYZ(GS_LINELIST, false);
		SetHandlerXYZ(GS_LINESTRIP, false);
		SetHandlerXYZ(GS_TRIANGLELIST, false);
		SetHandlerXYZ(GS_TRIANGLESTRIP, false);
		SetHandlerXYZ(GS_TRIANGLEFAN, false);
		SetHandlerXYZ(GS_SPRITE, false);
		SetHandlerXYZ(GS_INVALID, false);
	}

	for(size_t i = 0; i < countof(m_fpGIFRegHandlers); i++)
	{
		m_fpGIFRegHandlers[i] = &GSState::GIFRegHandlerNull;
	}

	m_fpGIFRegHandlers[GIF_A_D_REG_PRIM] = &GSState::GIFRegHandlerPRIM;
	m_fpGIFRegHandlers[GIF_A_D_REG_RGBAQ] = &GSState::GIFRegHandlerRGBAQ;
	m_fpGIFRegHandlers[GIF_A_D_REG_RGBAQ + 0x10] = &GSState::GIFRegHandlerRGBAQ;
	m_fpGIFRegHandlers[GIF_A_D_REG_ST] = &GSState::GIFRegHandlerST;
	m_fpGIFRegHandlers[GIF_A_D_REG_UV] = m_userhacks_wildhack ? &GSState::GIFRegHandlerUV_Hack : &GSState::GIFRegHandlerUV;
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

	SetMultithreaded(m_mt);
}

bool GSState::isinterlaced()
{
	return !!m_regs->SMODE2.INT;
}

GSVideoMode GSState::GetVideoMode()
{
	// TODO: Get confirmation of videomode from SYSCALL ? not necessary but would be nice.
	// Other videomodes can't be detected on the plugin side without the help of the data from core
	// You can only identify a limited number of video modes based on the info from CRTC registers.

	GSVideoMode videomode = GSVideoMode::Unknown;
	uint8 Colorburst = m_regs->SMODE1.CMOD; // Subcarrier frequency
	uint8 PLL_Divider = m_regs->SMODE1.LC; // Phased lock loop divider

	switch (Colorburst)
	{
	case 0:
		if (isinterlaced() && PLL_Divider == 22)
			videomode = GSVideoMode::HDTV_1080I;

		else if (!isinterlaced() && PLL_Divider == 22)
			videomode = GSVideoMode::HDTV_720P;

		else if (!isinterlaced() && PLL_Divider == 32)
			videomode = GSVideoMode::SDTV_480P; // TODO: 576P will also be reported as 480P, find some way to differeniate.

		else
			videomode = GSVideoMode::VESA;
		break;

	case 2:
		videomode = GSVideoMode::NTSC; break;

	case 3:
		videomode = GSVideoMode::PAL; break;
	}

	return videomode;
}

// There are some cases where the PS2 seems to saturate the output circuit size when the developer requests for a higher
// unsupported value with respect to the current video mode via the DISP registers, the following function handles such cases.
// NOTE: This function is totally hacky as there are no documents related to saturation of output dimensions, function is
// generally just based on technical and intellectual guesses.
void GSState::SaturateOutputSize(GSVector4i& r)
{
	const GSVideoMode videomode = GetVideoMode();

	//Some games (such as Pool Paradise) use alternate line reading and provide a massive height which is really half.
	if (r.height() > 640 && (videomode == GSVideoMode::NTSC || videomode == GSVideoMode::PAL))
	{
		r.bottom = r.top + (r.height() / 2);
		return;
	}

	//  Limit games to standard NTSC resolutions. games with 512X512 (PAL resolution) on NTSC video mode produces black border on the bottom.
	//  512 X 448 is the resolution generally used by NTSC, saturating the height value seems to get rid of the black borders.
	//  Though it's quite a bad hack as it affects binaries which are patched to run on a non-native video mode.
	const bool interlaced_field = m_regs->SMODE2.INT && !m_regs->SMODE2.FFMD;
	const bool single_frame_output = m_regs->SMODE2.INT && m_regs->SMODE2.FFMD && (m_regs->PMODE.EN1 ^ m_regs->PMODE.EN2);
	const bool unsupported_output_size = r.height() > 448 && r.width() < 640;
	if (m_NTSC_Saturation && videomode == GSVideoMode::NTSC && (interlaced_field || single_frame_output) && unsupported_output_size)
	{
		r.bottom = r.top + 448;
	}
}

GSVector4i GSState::GetDisplayRect(int i)
{
	if (!IsEnabled(0) && !IsEnabled(1))
		return GSVector4i(0);

	// If no specific context is requested then pass the merged rectangle as return value
	if (i == -1)
	{
		if (m_regs->PMODE.EN1 & m_regs->PMODE.EN2)
		{
			GSVector4i r[2] = { GetDisplayRect(0), GetDisplayRect(1) };
			GSVector4i r_intersect = r[0].rintersect(r[1]);
			GSVector4i r_union = r[0].runion_ordered(r[1]);

			// If the conditions for passing the merged rectangle is unsatisfied, then
			// pass the rectangle with the bigger size.
			bool can_be_merged = !r_intersect.width() || !r_intersect.height() || r_intersect.xyxy().eq(r_union.xyxy());
			return (can_be_merged) ? r_union : r[r[1].rarea() > r[0].rarea()];
		}
		i = m_regs->PMODE.EN2;
	}

	GSVector2i magnification (m_regs->DISP[i].DISPLAY.MAGH + 1, m_regs->DISP[i].DISPLAY.MAGV + 1);
	int width = (m_regs->DISP[i].DISPLAY.DW + 1) / magnification.x;
	int height = (m_regs->DISP[i].DISPLAY.DH + 1) / magnification.y;

	// Set up the display rectangle based on the values obtained from DISPLAY registers
	GSVector4i rectangle;
	rectangle.left = m_regs->DISP[i].DISPLAY.DX / magnification.x;
	rectangle.top = m_regs->DISP[i].DISPLAY.DY / magnification.y;
	rectangle.right = rectangle.left + width;
	rectangle.bottom = rectangle.top + height;

	SaturateOutputSize(rectangle);
	return rectangle;
}

GSVector4i GSState::GetFrameRect(int i)
{
	// If no specific context is requested then pass the merged rectangle as return value
	if (i == -1)
		return GetFrameRect(0).runion(GetFrameRect(1));

	GSVector4i rectangle = GetDisplayRect(i);

	int w = rectangle.width();
	int h = rectangle.height();

	if (isinterlaced() && m_regs->SMODE2.FFMD && h > 1)
		h >>= 1;

	rectangle.left = m_regs->DISP[i].DISPFB.DBX;
	rectangle.top = m_regs->DISP[i].DISPFB.DBY;
	rectangle.right = rectangle.left + w;
	rectangle.bottom = rectangle.top + h;

#ifdef ENABLE_PCRTC_DEBUG
	static GSVector4i old_r[2] = { GSVector4i(0), GSVector4i(0) };
	if (!old_r[i].eq(rectangle))
		printf("Frame rectangle [%d] update!\nwidth: %d  height: %d  left: %d  top: %d  right: %d  bottom: %d\n",
			i,w,h, rectangle.left, rectangle.top, rectangle.right, rectangle.bottom);
	old_r[i] = rectangle;
#endif

	return rectangle;
}

int GSState::GetFramebufferHeight()
{
	// Framebuffer height is 11 bits max according to GS user manual
	const int height_limit = (1 << 11);
	const GSVector4i output[2] = { GetFrameRect(0), GetFrameRect(1) };
	const GSVector4i merged_output = output[0].runion(output[1]);

	int max_height = std::max(output[0].height(), output[1].height());
	// DBY isn't an offset to the frame memory but rather an offset to read output circuit inside
	// the frame memory, hence the top offset should also be calculated for the total height of the
	// frame memory. Also we need to wrap the value only when we're dealing with values with range of the
	// frame memory (offset + read output circuit height, IOW bottom of merged_output)
	int frame_memory_height = std::max(max_height, merged_output.bottom % height_limit);

	if (frame_memory_height > 1024)
		GL_PERF("Massive framebuffer height detected! (height:%d)", frame_memory_height);

	return frame_memory_height;
}

bool GSState::IsEnabled(int i)
{
	ASSERT(i >= 0 && i < 2);

	if ((i == 0 && m_regs->PMODE.EN1) || (i == 1 && m_regs->PMODE.EN2))
	{
		return m_regs->DISP[i].DISPLAY.DW && m_regs->DISP[i].DISPLAY.DH;
	}

	return false;
}

float GSState::GetTvRefreshRate()
{
	float vertical_frequency = 0;
	GSVideoMode videomode = GetVideoMode();

	//TODO: Check vertical frequencies for VESA video modes, old ones were untested.

	switch (videomode)
	{
	case GSVideoMode::NTSC: case GSVideoMode::SDTV_480P:
		vertical_frequency = (60 / 1.001f); break;

	case GSVideoMode::PAL:
		vertical_frequency = 50; break;

	case GSVideoMode::HDTV_720P: case GSVideoMode::HDTV_1080I:
		vertical_frequency = 60; break;

	default:
		ASSERT(videomode != GSVideoMode::Unknown);
	}

	return vertical_frequency;
}

// GIFPackedRegHandler*

void GSState::GIFPackedRegHandlerNull(const GIFPackedReg* RESTRICT r)
{
	// ASSERT(0);
}

void GSState::GIFPackedRegHandlerRGBA(const GIFPackedReg* RESTRICT r)
{
	#if _M_SSE >= 0x301

	GSVector4i mask = GSVector4i::load(0x0c080400);
	GSVector4i v = GSVector4i::load<false>(r).shuffle8(mask);

	m_v.RGBAQ.u32[0] = (uint32)GSVector4i::store(v);

	#else

	GSVector4i v = GSVector4i::load<false>(r) & GSVector4i::x000000ff();

	m_v.RGBAQ.u32[0] = v.rgba32();

	#endif

	m_v.RGBAQ.Q = m_q;
}

void GSState::GIFPackedRegHandlerSTQ(const GIFPackedReg* RESTRICT r)
{
	GSVector4i st = GSVector4i::loadl(&r->u64[0]);
	GSVector4i q = GSVector4i::loadl(&r->u64[1]);

	GSVector4i::storel(&m_v.ST, st);

	// character shadow in Vexx, q = 0 (st also 0 on the first 16 vertices), setting it to 1.0f to avoid div by zero later
	q = q.blend8(GSVector4i::cast(GSVector4::m_one), q == GSVector4i::zero());
	// Suikoden 4 creates some nan for Q. Let's avoid undefined behavior (See GIFRegHandlerRGBAQ)
	q = GSVector4i::cast(GSVector4::cast(q).replace_nan(GSVector4::m_max));

	GSVector4::store(&m_q, GSVector4::cast(q));

	ASSERT(!std::isnan(m_v.ST.S)); // See GIFRegHandlerRGBAQ
	ASSERT(!std::isnan(m_v.ST.T)); // See GIFRegHandlerRGBAQ

#ifdef Offset_ST
	GIFRegTEX0 TEX0 = m_context->TEX0;
	m_v.ST.S -= 0.02f * m_q / (1 << TEX0.TW);
	m_v.ST.T -= 0.02f * m_q / (1 << TEX0.TH);
#endif
}

void GSState::GIFPackedRegHandlerUV(const GIFPackedReg* RESTRICT r)
{
	GSVector4i v = GSVector4i::loadl(r) & GSVector4i::x00003fff();

	m_v.UV = (uint32)GSVector4i::store(v.ps32(v));
}

void GSState::GIFPackedRegHandlerUV_Hack(const GIFPackedReg* RESTRICT r)
{
	GSVector4i v = GSVector4i::loadl(r) & GSVector4i::x00003fff();

	m_v.UV = (uint32)GSVector4i::store(v.ps32(v));

	m_isPackedUV_HackFlag = true;
}

template<uint32 prim, uint32 adc, bool auto_flush>
void GSState::GIFPackedRegHandlerXYZF2(const GIFPackedReg* RESTRICT r)
{
	/*
	m_v.XYZ.X = r->XYZF2.X;
	m_v.XYZ.Y = r->XYZF2.Y;
	m_v.XYZ.Z = r->XYZF2.Z;
	m_v.FOG = r->XYZF2.F;
	*/
	GSVector4i xy = GSVector4i::loadl(&r->u64[0]);
	GSVector4i zf = GSVector4i::loadl(&r->u64[1]);
	xy = xy.upl16(xy.srl<4>()).upl32(GSVector4i::load((int)m_v.UV));
	zf = zf.srl32(4) & GSVector4i::x00ffffff().upl32(GSVector4i::x000000ff());

	m_v.m[1] = xy.upl32(zf);

	VertexKick<prim, auto_flush>(adc ? 1 : r->XYZF2.Skip());
}

template<uint32 prim, uint32 adc, bool auto_flush>
void GSState::GIFPackedRegHandlerXYZ2(const GIFPackedReg* RESTRICT r)
{
/*
	m_v.XYZ.X = r->XYZ2.X;
	m_v.XYZ.Y = r->XYZ2.Y;
	m_v.XYZ.Z = r->XYZ2.Z;
*/
	GSVector4i xy = GSVector4i::loadl(&r->u64[0]);
	GSVector4i z = GSVector4i::loadl(&r->u64[1]);
	GSVector4i xyz = xy.upl16(xy.srl<4>()).upl32(z);

	m_v.m[1] = xyz.upl64(GSVector4i::loadl(&m_v.UV));

	VertexKick<prim, auto_flush>(adc ? 1 : r->XYZ2.Skip());
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

template<uint32 prim, bool auto_flush>
void GSState::GIFPackedRegHandlerSTQRGBAXYZF2(const GIFPackedReg* RESTRICT r, uint32 size)
{
	ASSERT(size > 0 && size % 3 == 0);

	const GIFPackedReg* RESTRICT r_end = r + size;

	while(r < r_end)
	{
		GSVector4i st = GSVector4i::loadl(&r[0].u64[0]);
		GSVector4i q = GSVector4i::loadl(&r[0].u64[1]);
		GSVector4i rgba = (GSVector4i::load<false>(&r[1]) & GSVector4i::x000000ff()).ps32().pu16();
		/*
		GSVector4i rg = GSVector4i::loadl(&r[1].u64[0]);
		GSVector4i ba = GSVector4i::loadl(&r[1].u64[1]);
		GSVector4i rbga = rg.upl8(ba);
		GSVector4i rgba = rbga.upl8(rbga.zzzz());
		*/
		q = q.blend8(GSVector4i::cast(GSVector4::m_one), q == GSVector4i::zero()); // see GIFPackedRegHandlerSTQ

		m_v.m[0] = st.upl64(rgba.upl32(q)); // TODO: only store the last one

		GSVector4i xy = GSVector4i::loadl(&r[2].u64[0]);
		GSVector4i zf = GSVector4i::loadl(&r[2].u64[1]);
		xy = xy.upl16(xy.srl<4>()).upl32(GSVector4i::load((int)m_v.UV));
		zf = zf.srl32(4) & GSVector4i::x00ffffff().upl32(GSVector4i::x000000ff());

		m_v.m[1] = xy.upl32(zf); // TODO: only store the last one

		VertexKick<prim, auto_flush>(r[2].XYZF2.Skip());

		r += 3;
	}

	m_q = r[-3].STQ.Q; // remember the last one, STQ outputs this to the temp Q each time
}

template<uint32 prim, bool auto_flush>
void GSState::GIFPackedRegHandlerSTQRGBAXYZ2(const GIFPackedReg* RESTRICT r, uint32 size)
{
	ASSERT(size > 0 && size % 3 == 0);

	const GIFPackedReg* RESTRICT r_end = r + size;

	while(r < r_end)
	{
		GSVector4i st = GSVector4i::loadl(&r[0].u64[0]);
		GSVector4i q = GSVector4i::loadl(&r[0].u64[1]);
		GSVector4i rgba = (GSVector4i::load<false>(&r[1]) & GSVector4i::x000000ff()).ps32().pu16();
		/*
		GSVector4i rg = GSVector4i::loadl(&r[1].u64[0]);
		GSVector4i ba = GSVector4i::loadl(&r[1].u64[1]);
		GSVector4i rbga = rg.upl8(ba);
		GSVector4i rgba = rbga.upl8(rbga.zzzz());
		*/
		q = q.blend8(GSVector4i::cast(GSVector4::m_one), q == GSVector4i::zero()); // see GIFPackedRegHandlerSTQ

		m_v.m[0] = st.upl64(rgba.upl32(q)); // TODO: only store the last one

		GSVector4i xy = GSVector4i::loadl(&r[2].u64[0]);
		GSVector4i z = GSVector4i::loadl(&r[2].u64[1]);
		GSVector4i xyz = xy.upl16(xy.srl<4>()).upl32(z);

		m_v.m[1] = xyz.upl64(GSVector4i::loadl(&m_v.UV)); // TODO: only store the last one

		VertexKick<prim, auto_flush>(r[2].XYZ2.Skip());

		r += 3;
	}

	m_q = r[-3].STQ.Q; // remember the last one, STQ outputs this to the temp Q each time
}

void GSState::GIFPackedRegHandlerNOP(const GIFPackedReg* RESTRICT r, uint32 size)
{
}

// GIFRegHandler*

void GSState::GIFRegHandlerNull(const GIFReg* RESTRICT r)
{
	// ASSERT(0);
}

__forceinline void GSState::ApplyPRIM(uint32 prim)
{
	// ASSERT(r->PRIM.PRIM < 7);

	if(GSUtil::GetPrimClass(m_env.PRIM.PRIM) == GSUtil::GetPrimClass(prim & 7)) // NOTE: assume strips/fans are converted to lists
	{
		if((m_env.PRIM.u32[0] ^ prim) & 0x7f8) // all fields except PRIM
		{
			Flush();
		}
	}
	else
	{
		Flush();
	}

	m_env.PRIM.u32[0] = prim;
	m_env.PRMODE._PRIM = prim;

	UpdateContext();

	UpdateVertexKick();

	ASSERT(m_index.tail == 0 || m_index.buff[m_index.tail - 1] + 1 == m_vertex.next);

	if(m_index.tail == 0)
	{
		m_vertex.next = 0;
	}

	m_vertex.head = m_vertex.tail = m_vertex.next; // remove unused vertices from the end of the vertex buffer
}

void GSState::GIFRegHandlerPRIM(const GIFReg* RESTRICT r)
{
	ALIGN_STACK(32);

	ApplyPRIM(r->PRIM.u32[0]);
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

	ASSERT(!std::isnan(m_v.ST.S)); // See GIFRegHandlerRGBAQ
	ASSERT(!std::isnan(m_v.ST.T)); // See GIFRegHandlerRGBAQ

#ifdef Offset_ST
	GIFRegTEX0 TEX0 = m_context->TEX0;
	m_v.ST.S -= 0.02f * m_q / (1 << TEX0.TW);
	m_v.ST.T -= 0.02f * m_q / (1 << TEX0.TH);
#endif
}

void GSState::GIFRegHandlerUV(const GIFReg* RESTRICT r)
{
	m_v.UV = r->UV.u32[0] & 0x3fff3fff;
}

void GSState::GIFRegHandlerUV_Hack(const GIFReg* RESTRICT r)
{
	m_v.UV = r->UV.u32[0] & 0x3fff3fff;

	m_isPackedUV_HackFlag = false;
}

template<uint32 prim, uint32 adc, bool auto_flush>
void GSState::GIFRegHandlerXYZF2(const GIFReg* RESTRICT r)
{
/*
	m_v.XYZ.X = r->XYZF.X;
	m_v.XYZ.Y = r->XYZF.Y;
	m_v.XYZ.Z = r->XYZF.Z;
	m_v.FOG.F = r->XYZF.F;
*/
	
/*
	m_v.XYZ.u32[0] = r->XYZF.u32[0];
	m_v.XYZ.u32[1] = r->XYZF.u32[1] & 0x00ffffff;
	m_v.FOG = r->XYZF.u32[1] >> 24;
*/

	GSVector4i xyzf = GSVector4i::loadl(&r->XYZF);
	GSVector4i xyz = xyzf & (GSVector4i::xffffffff().upl32(GSVector4i::x00ffffff()));
	GSVector4i uvf = GSVector4i::load((int)m_v.UV).upl32(xyzf.srl32(24).srl<4>());
	
	m_v.m[1] = xyz.upl64(uvf);

	VertexKick<prim, auto_flush>(adc);
}

template<uint32 prim, uint32 adc, bool auto_flush>
void GSState::GIFRegHandlerXYZ2(const GIFReg* RESTRICT r)
{
	// m_v.XYZ = (GSVector4i)r->XYZ;

	m_v.m[1] = GSVector4i::load(&r->XYZ, &m_v.UV);

	VertexKick<prim, auto_flush>(adc);
}

template<int i> void GSState::ApplyTEX0(GIFRegTEX0& TEX0)
{
	GL_REG("Apply TEX0_%d = 0x%x_%x", i, TEX0.u32[1], TEX0.u32[0]);

	// even if TEX0 did not change, a new palette may have been uploaded and will overwrite the currently queued for drawing
	bool wt = m_mem.m_clut.WriteTest(TEX0, m_env.TEXCLUT);

	// clut loading already covered with WriteTest, for drawing only have to check CPSM and CSA (MGS3 intro skybox would be drawn piece by piece without this)

	uint64 mask = 0x1f78001c3fffffffull; // TBP0 TBW PSM TW TCC TFX CPSM CSA

	if(wt || PRIM->CTXT == i && ((TEX0.u64 ^ m_env.CTXT[i].TEX0.u64) & mask))
	{
		Flush();
	}

	TEX0.CPSM &= 0xa; // 1010b

	if((TEX0.u32[0] ^ m_env.CTXT[i].TEX0.u32[0]) & 0x3ffffff) // TBP0 TBW PSM
	{
		m_env.CTXT[i].offset.tex = m_mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);
	}

	m_env.CTXT[i].TEX0 = (GSVector4i)TEX0;

	if(wt)
	{
		GIFRegBITBLTBUF BITBLTBUF;
		GSVector4i r;

		if(TEX0.CSM == 0)
		{
			BITBLTBUF.SBP = TEX0.CBP;
			BITBLTBUF.SBW = 1;
			BITBLTBUF.SPSM = TEX0.CSM;

			r.left = 0;
			r.top = 0;
			r.right = GSLocalMemory::m_psm[TEX0.CPSM].bs.x;
			r.bottom = GSLocalMemory::m_psm[TEX0.CPSM].bs.y;

			int blocks = 4;

			if(GSLocalMemory::m_psm[TEX0.CPSM].bpp == 16)
			{
				blocks >>= 1;
			}

			if(GSLocalMemory::m_psm[TEX0.PSM].bpp == 4)
			{
				blocks >>= 1;
			}
		
			for(int j = 0; j < blocks; j++, BITBLTBUF.SBP++)
			{
				InvalidateLocalMem(BITBLTBUF, r, true);
			}
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

template<int i> void GSState::GIFRegHandlerTEX0(const GIFReg* RESTRICT r)
{
	GL_REG("TEX0_%d = 0x%x_%x", i, r->u32[1], r->u32[0]);
	GIFRegTEX0 TEX0 = r->TEX0;

	int tw = (int)TEX0.TW;
	int th = (int)TEX0.TH;

	if(tw > 10) tw = 10;
	if(th > 10) th = 10;

	if(PRIM->FST)
	{
		// Tokyo Xtreme Racer Drift 2, TW/TH == 0
		// Just setting the max texture size to make the texture cache allocate some surface. 
		// The vertex trace will narrow the updated area down to the minimum, upper-left 8x8 
		// for a single letter, but it may address the whole thing if it wants to.

		if(tw == 0) tw = 10;
		if(th == 0) th = 10;
	}
	else
	{
		// Yakuza, TW/TH == 0
		// The minimap is drawn using solid colors, the texture is really a 1x1 white texel, 
		// modulated by the vertex color. Cannot change the dimension because S/T are normalized.
	}

	TEX0.TW = tw;
	TEX0.TH = th;

	if((TEX0.TBW & 1) && (TEX0.PSM == PSM_PSMT8 || TEX0.PSM == PSM_PSMT4))
	{
		ASSERT(TEX0.TBW == 1); // TODO // Bouken Jidai Katsugeki Goemon

		TEX0.TBW &= ~1; // GS User 2.6
	}

	ApplyTEX0<i>(TEX0);

	if(m_env.CTXT[i].TEX1.MTBA)
	{
		// NOTE 1: TEX1.MXL must not be automatically set to 3 here.
		// NOTE 2: Mipmap levels are tightly packed, if (tbw << 6) > (1 << tw) then the left-over space to the right is used. (common for PSM_PSMT4)
		// NOTE 3: Non-rectangular textures are treated as rectangular when calculating the occupied space (height is extended, not sure about width)

		uint32 bp = TEX0.TBP0;
		uint32 bw = TEX0.TBW;
		uint32 w = 1u << TEX0.TW;
		uint32 h = 1u << TEX0.TH;
		uint32 bpp = GSLocalMemory::m_psm[TEX0.PSM].bpp;

		if(h < w) h = w;

		bp += ((w * h * bpp >> 3) + 255) >> 8;
		bw = std::max<uint32>(bw >> 1, 1);
		w = std::max<uint32>(w >> 1, 1);
		h = std::max<uint32>(h >> 1, 1);

		m_env.CTXT[i].MIPTBP1.TBP1 = bp;
		m_env.CTXT[i].MIPTBP1.TBW1 = bw;

		bp += ((w * h * bpp >> 3) + 255) >> 8;
		bw = std::max<uint32>(bw >> 1, 1);
		w = std::max<uint32>(w >> 1, 1);
		h = std::max<uint32>(h >> 1, 1);

		m_env.CTXT[i].MIPTBP1.TBP2 = bp;
		m_env.CTXT[i].MIPTBP1.TBW2 = bw;

		bp += ((w * h * bpp >> 3) + 255) >> 8;
		bw = std::max<uint32>(bw >> 1, 1);
		w = std::max<uint32>(w >> 1, 1);
		h = std::max<uint32>(h >> 1, 1);

		m_env.CTXT[i].MIPTBP1.TBP3 = bp;
		m_env.CTXT[i].MIPTBP1.TBW3 = bw;

		// printf("MTBA\n");
	}
}

template<int i> void GSState::GIFRegHandlerCLAMP(const GIFReg* RESTRICT r)
{
	GL_REG("CLAMP_%d = 0x%x_%x", i, r->u32[1], r->u32[0]);
	if(PRIM->CTXT == i && r->CLAMP != m_env.CTXT[i].CLAMP)
	{
		Flush();
	}

	m_env.CTXT[i].CLAMP = (GSVector4i)r->CLAMP;
}

void GSState::GIFRegHandlerFOG(const GIFReg* RESTRICT r)
{
	m_v.FOG = r->FOG.F;
}

void GSState::GIFRegHandlerNOP(const GIFReg* RESTRICT r)
{
}

template<int i> void GSState::GIFRegHandlerTEX1(const GIFReg* RESTRICT r)
{
	GL_REG("TEX1_%d = 0x%x_%x", i, r->u32[1], r->u32[0]);
	if(PRIM->CTXT == i && r->TEX1 != m_env.CTXT[i].TEX1)
	{
		Flush();
	}

	m_env.CTXT[i].TEX1 = (GSVector4i)r->TEX1;
}

template<int i> void GSState::GIFRegHandlerTEX2(const GIFReg* RESTRICT r)
{
	GL_REG("TEX2_%d = 0x%x_%x", i, r->u32[1], r->u32[0]);
	// m_env.CTXT[i].TEX2 = r->TEX2; // not used

	// TEX2 is a masked write to TEX0, for performing CLUT swaps (palette swaps).
	// It only applies the following fields:
	//    CLD, CSA, CSM, CPSM, CBP, PSM.
	// It ignores these fields (uses existing values in the context):
	//    TFX, TCC, TH, TW, TBW, and TBP0

	uint64 mask = 0xFFFFFFE003F00000ull; // TEX2 bits

	GIFRegTEX0 TEX0;
	
	TEX0.u64 = (m_env.CTXT[i].TEX0.u64 & ~mask) | (r->u64 & mask);

	ApplyTEX0<i>(TEX0);
}

template<int i> void GSState::GIFRegHandlerXYOFFSET(const GIFReg* RESTRICT r)
{
	GL_REG("XYOFFSET_%d = 0x%x_%x", i, r->u32[1], r->u32[0]);
	GSVector4i o = (GSVector4i)r->XYOFFSET & GSVector4i::x0000ffff();

	if(!o.eq(m_env.CTXT[i].XYOFFSET))
	{
		Flush();
	}

	m_env.CTXT[i].XYOFFSET = o;

	m_env.CTXT[i].UpdateScissor();

	UpdateScissor();
}

void GSState::GIFRegHandlerPRMODECONT(const GIFReg* RESTRICT r)
{
	GL_REG("PRMODECONT = 0x%x_%x", r->u32[1], r->u32[0]);
	if(r->PRMODECONT != m_env.PRMODECONT)
	{
		Flush();
	}

	m_env.PRMODECONT.AC = r->PRMODECONT.AC;

	PRIM = m_env.PRMODECONT.AC ? &m_env.PRIM : (GIFRegPRIM*)&m_env.PRMODE;

	// if(PRIM->PRIM == 7) printf("Invalid PRMODECONT/PRIM\n");

	UpdateContext();

	UpdateVertexKick();
}

void GSState::GIFRegHandlerPRMODE(const GIFReg* RESTRICT r)
{
	GL_REG("PRMODE = 0x%x_%x", r->u32[1], r->u32[0]);
	if(!m_env.PRMODECONT.AC)
	{
		Flush();
	}

	uint32 _PRIM = m_env.PRMODE._PRIM;
	m_env.PRMODE = (GSVector4i)r->PRMODE;
	m_env.PRMODE._PRIM = _PRIM;

	UpdateContext();

	UpdateVertexKick();
}

void GSState::GIFRegHandlerTEXCLUT(const GIFReg* RESTRICT r)
{
	GL_REG("TEXCLUT = 0x%x_%x", r->u32[1], r->u32[0]);
	if(r->TEXCLUT != m_env.TEXCLUT)
	{
		Flush();
	}

	m_env.TEXCLUT = (GSVector4i)r->TEXCLUT;
}

void GSState::GIFRegHandlerSCANMSK(const GIFReg* RESTRICT r)
{
	if(r->SCANMSK != m_env.SCANMSK)
	{
		Flush();
	}

	m_env.SCANMSK = (GSVector4i)r->SCANMSK;
}

template<int i> void GSState::GIFRegHandlerMIPTBP1(const GIFReg* RESTRICT r)
{
	GL_REG("MIPTBP1_%d = 0x%x_%x", i, r->u32[1], r->u32[0]);
	if(PRIM->CTXT == i && r->MIPTBP1 != m_env.CTXT[i].MIPTBP1)
	{
		Flush();
	}

	m_env.CTXT[i].MIPTBP1 = (GSVector4i)r->MIPTBP1;
}

template<int i> void GSState::GIFRegHandlerMIPTBP2(const GIFReg* RESTRICT r)
{
	GL_REG("MIPTBP2_%d = 0x%x_%x", i, r->u32[1], r->u32[0]);
	if(PRIM->CTXT == i && r->MIPTBP2 != m_env.CTXT[i].MIPTBP2)
	{
		Flush();
	}

	m_env.CTXT[i].MIPTBP2 = (GSVector4i)r->MIPTBP2;
}

void GSState::GIFRegHandlerTEXA(const GIFReg* RESTRICT r)
{
	GL_REG("TEXA = 0x%x_%x", r->u32[1], r->u32[0]);
	if(r->TEXA != m_env.TEXA)
	{
		Flush();
	}

	m_env.TEXA = (GSVector4i)r->TEXA;
}

void GSState::GIFRegHandlerFOGCOL(const GIFReg* RESTRICT r)
{
	GL_REG("FOGCOL = 0x%x_%x", r->u32[1], r->u32[0]);
	if(r->FOGCOL != m_env.FOGCOL)
	{
		Flush();
	}

	m_env.FOGCOL = (GSVector4i)r->FOGCOL;
}

void GSState::GIFRegHandlerTEXFLUSH(const GIFReg* RESTRICT r)
{
	GL_REG("TEXFLUSH = 0x%x_%x", r->u32[1], r->u32[0]);
	m_texflush = true;
}

template<int i> void GSState::GIFRegHandlerSCISSOR(const GIFReg* RESTRICT r)
{
	if(PRIM->CTXT == i && r->SCISSOR != m_env.CTXT[i].SCISSOR)
	{
		Flush();
	}

	m_env.CTXT[i].SCISSOR = (GSVector4i)r->SCISSOR;

	m_env.CTXT[i].UpdateScissor();

	UpdateScissor();
}

template<int i> void GSState::GIFRegHandlerALPHA(const GIFReg* RESTRICT r)
{
	ASSERT(r->ALPHA.A != 3);
	ASSERT(r->ALPHA.B != 3);
	ASSERT(r->ALPHA.C != 3);
	ASSERT(r->ALPHA.D != 3);

	if(PRIM->CTXT == i && r->ALPHA != m_env.CTXT[i].ALPHA)
	{
		Flush();
	}

	m_env.CTXT[i].ALPHA = (GSVector4i)r->ALPHA;

	// A/B/C/D == 3? => 2

	m_env.CTXT[i].ALPHA.u32[0] = ((~m_env.CTXT[i].ALPHA.u32[0] >> 1) | 0xAA) & m_env.CTXT[i].ALPHA.u32[0];
}

void GSState::GIFRegHandlerDIMX(const GIFReg* RESTRICT r)
{
	bool update = false;

	if(r->DIMX != m_env.DIMX)
	{
		Flush();

		update = true;
	}

	m_env.DIMX = (GSVector4i)r->DIMX;

	if(update)
	{
		m_env.UpdateDIMX();
	}
}

void GSState::GIFRegHandlerDTHE(const GIFReg* RESTRICT r)
{
	if(r->DTHE != m_env.DTHE)
	{
		Flush();
	}

	m_env.DTHE = (GSVector4i)r->DTHE;
}

void GSState::GIFRegHandlerCOLCLAMP(const GIFReg* RESTRICT r)
{
	if(r->COLCLAMP != m_env.COLCLAMP)
	{
		Flush();
	}

	m_env.COLCLAMP = (GSVector4i)r->COLCLAMP;
#ifdef DISABLE_COLCLAMP
	m_env.COLCLAMP.CLAMP = 1;
#endif
}

template<int i> void GSState::GIFRegHandlerTEST(const GIFReg* RESTRICT r)
{
	if(PRIM->CTXT == i && r->TEST != m_env.CTXT[i].TEST)
	{
		Flush();
	}

	m_env.CTXT[i].TEST = (GSVector4i)r->TEST;
#ifdef DISABLE_DATE
	m_env.CTXT[i].TEST.DATE = 0;
#endif
}

void GSState::GIFRegHandlerPABE(const GIFReg* RESTRICT r)
{
	if(r->PABE != m_env.PABE)
	{
		Flush();
	}

	m_env.PABE = (GSVector4i)r->PABE;
}

template<int i> void GSState::GIFRegHandlerFBA(const GIFReg* RESTRICT r)
{
	if(PRIM->CTXT == i && r->FBA != m_env.CTXT[i].FBA)
	{
		Flush();
	}

	m_env.CTXT[i].FBA = (GSVector4i)r->FBA;
}

template<int i> void GSState::GIFRegHandlerFRAME(const GIFReg* RESTRICT r)
{
	GL_REG("FRAME_%d = 0x%x_%x", i, r->u32[1], r->u32[0]);
	if(PRIM->CTXT == i && r->FRAME != m_env.CTXT[i].FRAME)
	{
		Flush();
	}

	if((m_env.CTXT[i].FRAME.u32[0] ^ r->FRAME.u32[0]) & 0x3f3f01ff) // FBP FBW PSM
	{
		m_env.CTXT[i].offset.fb = m_mem.GetOffset(r->FRAME.Block(), r->FRAME.FBW, r->FRAME.PSM);
		m_env.CTXT[i].offset.zb = m_mem.GetOffset(m_env.CTXT[i].ZBUF.Block(), r->FRAME.FBW, m_env.CTXT[i].ZBUF.PSM);
		m_env.CTXT[i].offset.fzb = m_mem.GetPixelOffset(r->FRAME, m_env.CTXT[i].ZBUF);
		m_env.CTXT[i].offset.fzb4 = m_mem.GetPixelOffset4(r->FRAME, m_env.CTXT[i].ZBUF);
	}

	m_env.CTXT[i].FRAME = (GSVector4i)r->FRAME;

	switch (m_env.CTXT[i].FRAME.PSM) {
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

#ifdef DISABLE_BITMASKING
	m_env.CTXT[i].FRAME.FBMSK = GSVector4i::store(GSVector4i::load((int)m_env.CTXT[i].FRAME.FBMSK).eq8(GSVector4i::xffffffff()));
#endif
}

template<int i> void GSState::GIFRegHandlerZBUF(const GIFReg* RESTRICT r)
{
	GL_REG("ZBUF_%d = 0x%x_%x", i, r->u32[1], r->u32[0]);
	GIFRegZBUF ZBUF = r->ZBUF;

	if(ZBUF.u32[0] == 0)
	{
		// during startup all regs are cleared to 0 (by the bios or something), so we mask z until this register becomes valid
		// edit: breaks Grandia Xtreme and sounds like a bad idea generally. What was the intend?
		// edit2: should be set only before any serious drawing happens, grandia extreme nulls out this register throughout the whole game, 
		//        I already forgot what it fixed, that game never masked the zbuffer, but assumed it was set by default
		//ZBUF.ZMSK = 1;
	}

	ZBUF.PSM |= 0x30;

	if(ZBUF.PSM != PSM_PSMZ32
	&& ZBUF.PSM != PSM_PSMZ24
	&& ZBUF.PSM != PSM_PSMZ16
	&& ZBUF.PSM != PSM_PSMZ16S)
	{
		ZBUF.PSM = PSM_PSMZ32;
	}

	if(PRIM->CTXT == i && ZBUF != m_env.CTXT[i].ZBUF)
	{
		Flush();
	}

	if((m_env.CTXT[i].ZBUF.u32[0] ^ ZBUF.u32[0]) & 0x3f0001ff) // ZBP PSM
	{
		m_env.CTXT[i].offset.zb = m_mem.GetOffset(ZBUF.Block(), m_env.CTXT[i].FRAME.FBW, ZBUF.PSM);
		m_env.CTXT[i].offset.fzb = m_mem.GetPixelOffset(m_env.CTXT[i].FRAME, ZBUF);
		m_env.CTXT[i].offset.fzb4 = m_mem.GetPixelOffset4(m_env.CTXT[i].FRAME, ZBUF);
	}

	m_env.CTXT[i].ZBUF = (GSVector4i)ZBUF;
}

void GSState::GIFRegHandlerBITBLTBUF(const GIFReg* RESTRICT r)
{
	GL_REG("BITBLTBUF = 0x%x_%x", r->u32[1], r->u32[0]);
	if(r->BITBLTBUF != m_env.BITBLTBUF)
	{
		FlushWrite();
	}

	m_env.BITBLTBUF = (GSVector4i)r->BITBLTBUF;

	if((m_env.BITBLTBUF.SBW & 1) && (m_env.BITBLTBUF.SPSM == PSM_PSMT8 || m_env.BITBLTBUF.SPSM == PSM_PSMT4))
	{
		m_env.BITBLTBUF.SBW &= ~1;
	}

	if((m_env.BITBLTBUF.DBW & 1) && (m_env.BITBLTBUF.DPSM == PSM_PSMT8 || m_env.BITBLTBUF.DPSM == PSM_PSMT4))
	{
		m_env.BITBLTBUF.DBW &= ~1; // namcoXcapcom: 5, 11, refered to as 4, 10 in TEX0.TBW later
	}
}

void GSState::GIFRegHandlerTRXPOS(const GIFReg* RESTRICT r)
{
	GL_REG("TRXPOS = 0x%x_%x", r->u32[1], r->u32[0]);
	if(r->TRXPOS != m_env.TRXPOS)
	{
		FlushWrite();
	}

	m_env.TRXPOS = (GSVector4i)r->TRXPOS;
}

void GSState::GIFRegHandlerTRXREG(const GIFReg* RESTRICT r)
{
	GL_REG("TRXREG = 0x%x_%x", r->u32[1], r->u32[0]);
	if(r->TRXREG != m_env.TRXREG)
	{
		FlushWrite();
	}

	m_env.TRXREG = (GSVector4i)r->TRXREG;
}

void GSState::GIFRegHandlerTRXDIR(const GIFReg* RESTRICT r)
{
	GL_REG("TRXDIR = 0x%x_%x", r->u32[1], r->u32[0]);
	Flush();

	m_env.TRXDIR = (GSVector4i)r->TRXDIR;

	switch(m_env.TRXDIR.XDIR)
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
	case 3:
		ASSERT(0);
		break;
	default:
		__assume(0);
	}
}

void GSState::GIFRegHandlerHWREG(const GIFReg* RESTRICT r)
{
	GL_REG("HWREG = 0x%x_%x", r->u32[1], r->u32[0]);
	ASSERT(m_env.TRXDIR.XDIR == 0); // host => local

	Write((uint8*)r, 8); // haunting ground
}

void GSState::GIFRegHandlerSIGNAL(const GIFReg* RESTRICT r)
{
	GL_REG("SIGNAL = 0x%x_%x", r->u32[1], r->u32[0]);
	m_regs->SIGLBLID.SIGID = (m_regs->SIGLBLID.SIGID & ~r->SIGNAL.IDMSK) | (r->SIGNAL.ID & r->SIGNAL.IDMSK);

	if(m_regs->CSR.wSIGNAL) m_regs->CSR.rSIGNAL = 1;
	if(!m_regs->IMR.SIGMSK && m_irq) m_irq();
}

void GSState::GIFRegHandlerFINISH(const GIFReg* RESTRICT r)
{
	GL_REG("FINISH = 0x%x_%x", r->u32[1], r->u32[0]);
	if(m_regs->CSR.wFINISH) m_regs->CSR.rFINISH = 1;
	if(!m_regs->IMR.FINISHMSK && m_irq) m_irq();
}

void GSState::GIFRegHandlerLABEL(const GIFReg* RESTRICT r)
{
	GL_REG("LABEL = 0x%x_%x", r->u32[1], r->u32[0]);
	m_regs->SIGLBLID.LBLID = (m_regs->SIGLBLID.LBLID & ~r->LABEL.IDMSK) | (r->LABEL.ID & r->LABEL.IDMSK);
}

//

void GSState::Flush()
{
	FlushWrite();

	FlushPrim();
}

void GSState::FlushWrite()
{
	int len = m_tr.end - m_tr.start;

	if(len <= 0) return;

	GSVector4i r;

	r.left = m_env.TRXPOS.DSAX;
	r.top = m_env.TRXPOS.DSAY;
	r.right = r.left + m_env.TRXREG.RRW;
	r.bottom = r.top + m_env.TRXREG.RRH;

	InvalidateVideoMem(m_env.BITBLTBUF, r);

	//int y = m_tr.y;

	GSLocalMemory::writeImage wi = GSLocalMemory::m_psm[m_env.BITBLTBUF.DPSM].wi;

	(m_mem.*wi)(m_tr.x, m_tr.y, &m_tr.buff[m_tr.start], len, m_env.BITBLTBUF, m_env.TRXPOS, m_env.TRXREG);

	m_tr.start += len;

	m_perfmon.Put(GSPerfMon::Swizzle, len);

	/*
	GSVector4i r;

	r.left = m_env.TRXPOS.DSAX;
	r.top = y;
	r.right = r.left + m_env.TRXREG.RRW;
	r.bottom = std::min<int>(r.top + m_env.TRXREG.RRH, m_tr.x == r.left ? m_tr.y : m_tr.y + 1);

	InvalidateVideoMem(m_env.BITBLTBUF, r);
	*/
/*
	static int n = 0;
	std::string s;
	s = format("c:\\temp1\\[%04d]_%05x_%d_%d_%d_%d_%d_%d.bmp",
		n++, (int)m_env.BITBLTBUF.DBP, (int)m_env.BITBLTBUF.DBW, (int)m_env.BITBLTBUF.DPSM,
		r.left, r.top, r.right, r.bottom);
	m_mem.SaveBMP(s, m_env.BITBLTBUF.DBP, m_env.BITBLTBUF.DBW, m_env.BITBLTBUF.DPSM, r.right, r.bottom);
*/
}

void GSState::FlushPrim()
{
	if(m_index.tail > 0)
	{
		GL_REG("FlushPrim ctxt %d", PRIM->CTXT);

		// Some games (Harley Davidson/Virtua Fighter) do dirty trick with multiple contexts cluts
		// In doubt, always reload the clut before a draw.
		// Note: perf impact is likely slow enough as WriteTest will likely be false.
		if (m_clut_load_before_draw) {
			if (m_mem.m_clut.WriteTest(m_context->TEX0, m_env.TEXCLUT)) {
				m_mem.m_clut.Write(m_context->TEX0, m_env.TEXCLUT);
			}
		}

		GSVertex buff[2];
		s_n++;

		size_t head = m_vertex.head;
		size_t tail = m_vertex.tail;
		size_t next = m_vertex.next;
		size_t unused = 0;

		if(tail > head)
		{
			switch(PRIM->PRIM)
			{
			case GS_POINTLIST:
				ASSERT(0);
				break;
			case GS_LINELIST:
			case GS_LINESTRIP:
			case GS_SPRITE:
			case GS_TRIANGLELIST:
			case GS_TRIANGLESTRIP:
				unused = tail - head;
				memcpy(buff, &m_vertex.buff[head], sizeof(GSVertex) * unused);
				break;
			case GS_TRIANGLEFAN:
				buff[0] = m_vertex.buff[head]; unused = 1;
				if(tail - 1 > head) {buff[1] = m_vertex.buff[tail - 1]; unused = 2;}
				break;
			case GS_INVALID:
				break;
			default:
				__assume(0);
			}

			ASSERT((int)unused < GSUtil::GetVertexCount(PRIM->PRIM));
		}

#ifdef ENABLE_OGL_DEBUG
		// Validate PSM format
		switch (m_context->TEX0.PSM) {
			case PSM_PSMCT32:
			case PSM_PSMCT24:
			case PSM_PSMCT16:
			case PSM_PSMCT16S:
			case PSM_PSMT8:
			case PSM_PSMT4:
			case PSM_PSMT8H:
			case PSM_PSMT4HL:
			case PSM_PSMT4HH:
			case PSM_PSMZ32:
			case PSM_PSMZ24:
			case PSM_PSMZ16:
			case PSM_PSMZ16S:
				break;
			default:
				fprintf(stderr, "%d:INVALID PSM 0x%x !!!\n", s_n, m_context->TEX0.PSM);
				break;
		}
#endif

		if(GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt < 3 && GSLocalMemory::m_psm[m_context->ZBUF.PSM].fmt < 3)
		{
			m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail, m_index.tail, GSUtil::GetPrimClass(PRIM->PRIM));

			m_context->SaveReg();

			try {
				Draw();
			} catch (GSDXRecoverableError&) {
				// could be an unsupported draw call
			} catch (const std::bad_alloc& e) {
				// Texture Out Of Memory
				PurgePool();
				fprintf(stderr, "GSDX OUT OF MEMORY\n");
			}

			m_context->RestoreReg();

			m_perfmon.Put(GSPerfMon::Draw, 1);
			m_perfmon.Put(GSPerfMon::Prim, m_index.tail / GSUtil::GetVertexCount(PRIM->PRIM));
		}
		else
		{
#ifdef ENABLE_OGL_DEBUG
			fprintf(stderr, "%d:Skip draw call due to invalid format %x/%x\n", s_n, m_context->FRAME.PSM, m_context->ZBUF.PSM);
#endif
		}

		m_index.tail = 0;

		m_vertex.head = 0;

		if(unused > 0)
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
}

//

void GSState::Write(const uint8* mem, int len)
{
	int w = m_env.TRXREG.RRW;
	int h = m_env.TRXREG.RRH;

	GIFRegBITBLTBUF& blit = m_tr.m_blit;
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[blit.DPSM];

	/*
	 *  The game uses a resolution of 512x244. RT is located at 0x700 and depth at 0x0
	 *
	 * #Bug number 1. (bad top bar)
	 * The game saves the depth buffer in the EE but with a resolution of
	 * 512x255. So it is ending to 0x7F8, ouch it saves the top of the RT too.
	 *
	 * #Bug number 2. (darker screen)
	 * The game will restore the previously saved buffer at position 0x0 to
	 * 0x7F8.  Because of the extra RT pixels, GSdx will partialy invalidate
	 * the texture located at 0x700. Next access will generate a cache miss
	 *
	 * The no-solution: instead to handle garbage (aka RT) at the end of the
	 * depth buffer. Let's reduce the size of the transfer
	 */
	if (m_game.title == CRC::SMTNocturne) {
		if (blit.DBP == 0 && blit.DPSM == PSM_PSMZ32 && w == 512 && h > 224) {
			h = 224;
			m_env.TRXREG.RRH = 224;
		}
	}

	// printf("Write len=%d DBP=%05x DBW=%d DPSM=%d DSAX=%d DSAY=%d RRW=%d RRH=%d\n", len, blit.DBP, blit.DBW, blit.DPSM, m_env.TRXPOS.DSAX, m_env.TRXPOS.DSAY, m_env.TRXREG.RRW, m_env.TRXREG.RRH);

	if(!m_tr.Update(w, h, psm.trbpp, len))
	{
		return;
	}

	GL_CACHE("Write! ...  => 0x%x W:%d F:%s (DIR %d%d), dPos(%d %d) size(%d %d)",
		blit.DBP, blit.DBW, psm_str(blit.DPSM),
		m_env.TRXPOS.DIRX, m_env.TRXPOS.DIRY,
		m_env.TRXPOS.DSAX, m_env.TRXPOS.DSAY, w, h);

	if(PRIM->TME && (blit.DBP == m_context->TEX0.TBP0 || blit.DBP == m_context->TEX0.CBP)) // TODO: hmmmm
	{
		FlushPrim();
	}

	if(m_tr.end == 0 && len >= m_tr.total)
	{
		// received all data in one piece, no need to buffer it

		// printf("%d >= %d\n", len, m_tr.total);

		GSVector4i r;

		r.left = m_env.TRXPOS.DSAX;
		r.top = m_env.TRXPOS.DSAY;
		r.right = r.left + m_env.TRXREG.RRW;
		r.bottom = r.top + m_env.TRXREG.RRH;

		InvalidateVideoMem(blit, r);

		(m_mem.*psm.wi)(m_tr.x, m_tr.y, mem, m_tr.total, blit, m_env.TRXPOS, m_env.TRXREG);

		m_tr.start = m_tr.end = m_tr.total;

		m_perfmon.Put(GSPerfMon::Swizzle, len);

		/*
		static int n = 0;
		std::string s;
		s = format("c:\\temp1\\[%04d]_%05x_%d_%d_%d_%d_%d_%d.bmp",
			n++, (int)blit.DBP, (int)blit.DBW, (int)blit.DPSM,
			r.left, r.top, r.right, r.bottom);
		m_mem.SaveBMP(s, blit.DBP, blit.DBW, blit.DPSM, r.right, r.bottom);
		*/
	}
	else
	{
		// printf("%d += %d (%d)\n", m_tr.end, len, m_tr.total);

		memcpy(&m_tr.buff[m_tr.end], mem, len);

		m_tr.end += len;

		if(m_tr.end >= m_tr.total)
		{
			FlushWrite();
		}
	}

	m_mem.m_clut.Invalidate();
}

void GSState::InitReadFIFO(uint8* mem, int len)
{
	if(len <= 0) return;

	// Allow to keep compatibility with older PCSX2
	m_init_read_fifo_supported = true;

	int sx = m_env.TRXPOS.SSAX;
	int sy = m_env.TRXPOS.SSAY;
	int w = m_env.TRXREG.RRW;
	int h = m_env.TRXREG.RRH;

	// printf("Read len=%d SBP=%05x SBW=%d SPSM=%d SSAX=%d SSAY=%d RRW=%d RRH=%d\n", len, (int)m_env.BITBLTBUF.SBP, (int)m_env.BITBLTBUF.SBW, (int)m_env.BITBLTBUF.SPSM, sx, sy, w, h);

	if(!m_tr.Update(w, h, GSLocalMemory::m_psm[m_env.BITBLTBUF.SPSM].trbpp, len))
	{
		return;
	}

	if(m_tr.x == sx && m_tr.y == sy)
	{
		InvalidateLocalMem(m_env.BITBLTBUF, GSVector4i(sx, sy, sx + w, sy + h));
	}
}

void GSState::Read(uint8* mem, int len)
{
	if(len <= 0) return;

	int sx = m_env.TRXPOS.SSAX;
	int sy = m_env.TRXPOS.SSAY;
	int w = m_env.TRXREG.RRW;
	int h = m_env.TRXREG.RRH;
	GSVector4i r(sx, sy, sx + w, sy + h);

	// Function is called from the EE thread. Unforunately gl stuff can only be used from a single thread (AKA MTGS)
	if (GLLoader::in_replayer) {
		GL_CACHE("Read! len=%d SBP=%05x SBW=%d SPSM=%s SSAX=%d SSAY=%d RRW=%d RRH=%d",
				len, (int)m_env.BITBLTBUF.SBP, (int)m_env.BITBLTBUF.SBW, psm_str(m_env.BITBLTBUF.SPSM), sx, sy, w, h);
	}

	if(!m_tr.Update(w, h, GSLocalMemory::m_psm[m_env.BITBLTBUF.SPSM].trbpp, len))
	{
		return;
	}

	if(!m_init_read_fifo_supported)
	{
		if(m_tr.x == sx && m_tr.y == sy)
		{
			InvalidateLocalMem(m_env.BITBLTBUF, r);
		}
	}

	m_mem.ReadImageX(m_tr.x, m_tr.y, mem, len, m_env.BITBLTBUF, m_env.TRXPOS, m_env.TRXREG);

	if(s_dump && s_save && s_n >= s_saven) {
		std::string s = m_dump_root + format("%05d_read_%05x_%d_%d_%d_%d_%d_%d.bmp",
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
	int w = m_env.TRXREG.RRW;
	int h = m_env.TRXREG.RRH;

	GL_CACHE("Move! 0x%x W:%d F:%s => 0x%x W:%d F:%s (DIR %d%d), sPos(%d %d) dPos(%d %d) size(%d %d)",
		m_env.BITBLTBUF.SBP, m_env.BITBLTBUF.SBW, psm_str(m_env.BITBLTBUF.SPSM),
		m_env.BITBLTBUF.DBP, m_env.BITBLTBUF.DBW, psm_str(m_env.BITBLTBUF.DPSM),
		m_env.TRXPOS.DIRX, m_env.TRXPOS.DIRY,
		sx, sy, dx, dy, w, h);

	InvalidateLocalMem(m_env.BITBLTBUF, GSVector4i(sx, sy, sx + w, sy + h));
	InvalidateVideoMem(m_env.BITBLTBUF, GSVector4i(dx, dy, dx + w, dy + h));

	int xinc = 1;
	int yinc = 1;

	if(m_env.TRXPOS.DIRX) {sx += w - 1; dx += w - 1; xinc = -1;}
	if(m_env.TRXPOS.DIRY) {sy += h - 1; dy += h - 1; yinc = -1;}
/*
	printf("%05x %d %d => %05x %d %d (%d%d), %d %d %d %d %d %d\n",
		m_env.BITBLTBUF.SBP, m_env.BITBLTBUF.SBW, m_env.BITBLTBUF.SPSM,
		m_env.BITBLTBUF.DBP, m_env.BITBLTBUF.DBW, m_env.BITBLTBUF.DPSM,
		m_env.TRXPOS.DIRX, m_env.TRXPOS.DIRY,
		sx, sy, dx, dy, w, h);
*/
/*
	GSLocalMemory::readPixel rp = GSLocalMemory::m_psm[m_env.BITBLTBUF.SPSM].rp;
	GSLocalMemory::writePixel wp = GSLocalMemory::m_psm[m_env.BITBLTBUF.DPSM].wp;

	for(int y = 0; y < h; y++, sy += yinc, dy += yinc, sx -= xinc*w, dx -= xinc*w)
		for(int x = 0; x < w; x++, sx += xinc, dx += xinc)
			(m_mem.*wp)(dx, dy, (m_mem.*rp)(sx, sy, m_env.BITBLTBUF.SBP, m_env.BITBLTBUF.SBW), m_env.BITBLTBUF.DBP, m_env.BITBLTBUF.DBW);
*/

	const GSLocalMemory::psm_t& spsm = GSLocalMemory::m_psm[m_env.BITBLTBUF.SPSM];
	const GSLocalMemory::psm_t& dpsm = GSLocalMemory::m_psm[m_env.BITBLTBUF.DPSM];

	// TODO: unroll inner loops (width has special size requirement, must be multiples of 1 << n, depending on the format)

	GSOffset* RESTRICT spo = m_mem.GetOffset(m_env.BITBLTBUF.SBP, m_env.BITBLTBUF.SBW, m_env.BITBLTBUF.SPSM);
	GSOffset* RESTRICT dpo = m_mem.GetOffset(m_env.BITBLTBUF.DBP, m_env.BITBLTBUF.DBW, m_env.BITBLTBUF.DPSM);

	if(spsm.trbpp == dpsm.trbpp && spsm.trbpp >= 16)
	{
		int* RESTRICT scol = &spo->pixel.col[0][sx];
		int* RESTRICT dcol = &dpo->pixel.col[0][dx];

		if(spsm.trbpp == 32)
		{
			if(xinc > 0)
			{
				for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
				{
					uint32* RESTRICT s = &m_mem.m_vm32[spo->pixel.row[sy]];
					uint32* RESTRICT d = &m_mem.m_vm32[dpo->pixel.row[dy]];

					for(int x = 0; x < w; x++) d[dcol[x]] = s[scol[x]];
				}
			}
			else
			{
				for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
				{
					uint32* RESTRICT s = &m_mem.m_vm32[spo->pixel.row[sy]];
					uint32* RESTRICT d = &m_mem.m_vm32[dpo->pixel.row[dy]];

					for(int x = 0; x > -w; x--) d[dcol[x]] = s[scol[x]];
				}
			}
		}
		else if(spsm.trbpp == 24)
		{
			if(xinc > 0)
			{
				for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
				{
					uint32* RESTRICT s = &m_mem.m_vm32[spo->pixel.row[sy]];
					uint32* RESTRICT d = &m_mem.m_vm32[dpo->pixel.row[dy]];

					for(int x = 0; x < w; x++) d[dcol[x]] = (d[dcol[x]] & 0xff000000) | (s[scol[x]] & 0x00ffffff);
				}
			}
			else
			{
				for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
				{
					uint32* RESTRICT s = &m_mem.m_vm32[spo->pixel.row[sy]];
					uint32* RESTRICT d = &m_mem.m_vm32[dpo->pixel.row[dy]];

					for(int x = 0; x > -w; x--) d[dcol[x]] = (d[dcol[x]] & 0xff000000) | (s[scol[x]] & 0x00ffffff);
				}
			}
		}
		else // if(spsm.trbpp == 16)
		{
			if(xinc > 0)
			{
				for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
				{
					uint16* RESTRICT s = &m_mem.m_vm16[spo->pixel.row[sy]];
					uint16* RESTRICT d = &m_mem.m_vm16[dpo->pixel.row[dy]];

					for(int x = 0; x < w; x++) d[dcol[x]] = s[scol[x]];
				}
			}
			else
			{
				for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
				{
					uint16* RESTRICT s = &m_mem.m_vm16[spo->pixel.row[sy]];
					uint16* RESTRICT d = &m_mem.m_vm16[dpo->pixel.row[dy]];

					for(int x = 0; x > -w; x--) d[dcol[x]] = s[scol[x]];
				}
			}
		}
	}
	else if(m_env.BITBLTBUF.SPSM == PSM_PSMT8 && m_env.BITBLTBUF.DPSM == PSM_PSMT8)
	{
		if(xinc > 0)
		{
			for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
			{
				uint8* RESTRICT s = &m_mem.m_vm8[spo->pixel.row[sy]];
				uint8* RESTRICT d = &m_mem.m_vm8[dpo->pixel.row[dy]];

				int* RESTRICT scol = &spo->pixel.col[sy & 7][sx];
				int* RESTRICT dcol = &dpo->pixel.col[dy & 7][dx];

				for(int x = 0; x < w; x++) d[dcol[x]] = s[scol[x]];
			}
		}
		else
		{
			for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
			{
				uint8* RESTRICT s = &m_mem.m_vm8[spo->pixel.row[sy]];
				uint8* RESTRICT d = &m_mem.m_vm8[dpo->pixel.row[dy]];

				int* RESTRICT scol = &spo->pixel.col[sy & 7][sx];
				int* RESTRICT dcol = &dpo->pixel.col[dy & 7][dx];

				for(int x = 0; x > -w; x--) d[dcol[x]] = s[scol[x]];
			}
		}
	}
	else if(m_env.BITBLTBUF.SPSM == PSM_PSMT4 && m_env.BITBLTBUF.DPSM == PSM_PSMT4)
	{
		if(xinc > 0)
		{
			for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
			{
				uint32 sbase = spo->pixel.row[sy];
				uint32 dbase = dpo->pixel.row[dy];

				int* RESTRICT scol = &spo->pixel.col[sy & 7][sx];
				int* RESTRICT dcol = &dpo->pixel.col[dy & 7][dx];

				for(int x = 0; x < w; x++) m_mem.WritePixel4(dbase + dcol[x], m_mem.ReadPixel4(sbase + scol[x]));
			}
		}
		else
		{
			for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
			{
				uint32 sbase = spo->pixel.row[sy];
				uint32 dbase = dpo->pixel.row[dy];

				int* RESTRICT scol = &spo->pixel.col[sy & 7][sx];
				int* RESTRICT dcol = &dpo->pixel.col[dy & 7][dx];

				for(int x = 0; x > -w; x--) m_mem.WritePixel4(dbase + dcol[x], m_mem.ReadPixel4(sbase + scol[x]));
			}
		}
	}
	else
	{
		if(xinc > 0)
		{
			for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
			{
				uint32 sbase = spo->pixel.row[sy];
				uint32 dbase = dpo->pixel.row[dy];

				int* RESTRICT scol = &spo->pixel.col[sy & 7][sx];
				int* RESTRICT dcol = &dpo->pixel.col[dy & 7][dx];

				for(int x = 0; x < w; x++) (m_mem.*dpsm.wpa)(dbase + dcol[x], (m_mem.*spsm.rpa)(sbase + scol[x]));
			}
		}
		else
		{
			for(int y = 0; y < h; y++, sy += yinc, dy += yinc)
			{
				uint32 sbase = spo->pixel.row[sy];
				uint32 dbase = dpo->pixel.row[dy];

				int* RESTRICT scol = &spo->pixel.col[sy & 7][sx];
				int* RESTRICT dcol = &dpo->pixel.col[dy & 7][dx];

				for(int x = 0; x > -w; x--) (m_mem.*dpsm.wpa)(dbase + dcol[x], (m_mem.*spsm.rpa)(sbase + scol[x]));
			}
		}
	}
}

void GSState::SoftReset(uint32 mask)
{
	if(mask & 1)
	{
		memset(&m_path[0], 0, sizeof(GIFPath));
		memset(&m_path[3], 0, sizeof(GIFPath));
	}

	if(mask & 2) memset(&m_path[1], 0, sizeof(GIFPath));
	if(mask & 4) memset(&m_path[2], 0, sizeof(GIFPath));

	m_env.TRXDIR.XDIR = 3; //-1 ; set it to invalid value

	m_q = 1.0f;
}

void GSState::ReadFIFO(uint8* mem, int size)
{
	GSPerfMonAutoTimer pmat(&m_perfmon);

	Flush();

	size *= 16;

	Read(mem, size);

	if(m_dump)
	{
		m_dump->ReadFIFO(size);
	}
}

template void GSState::Transfer<0>(const uint8* mem, uint32 size);
template void GSState::Transfer<1>(const uint8* mem, uint32 size);
template void GSState::Transfer<2>(const uint8* mem, uint32 size);
template void GSState::Transfer<3>(const uint8* mem, uint32 size);

template<int index> void GSState::Transfer(const uint8* mem, uint32 size)
{
	GSPerfMonAutoTimer pmat(&m_perfmon);

	const uint8* start = mem;

	GIFPath& path = m_path[index];

	while(size > 0)
	{
		if(path.nloop == 0)
		{
			path.SetTag(mem);

			mem += sizeof(GIFTag);
			size--;

			if(path.nloop > 0) // eeuser 7.2.2. GIFtag: "... when NLOOP is 0, the GIF does not output anything, and values other than the EOP field are disregarded."
			{
				m_q = 1.0f;

				// ASSERT(!(path.tag.PRE && path.tag.FLG == GIF_FLG_REGLIST)); // kingdom hearts

				if(path.tag.PRE && path.tag.FLG == GIF_FLG_PACKED)
				{
					ApplyPRIM(path.tag.PRIM);
				}
			}
		}
		else
		{
			uint32 total;

			switch(path.tag.FLG)
			{
			case GIF_FLG_PACKED:

				// get to the start of the loop

				if(path.reg != 0)
				{
					do
					{
						(this->*m_fpGIFPackedRegHandlers[path.GetReg()])((GIFPackedReg*)mem);

						mem += sizeof(GIFPackedReg);
						size--;
					}
					while(path.StepReg() && size > 0 && path.reg != 0);
				}

				// all data available? usually is

				total = path.nloop * path.nreg;

				if(size >= total)
				{
					size -= total;

					switch(path.type)
					{
					case GIFPath::TYPE_UNKNOWN:

						{
							uint32 reg = 0;

							do
							{
								(this->*m_fpGIFPackedRegHandlers[path.GetReg(reg++)])((GIFPackedReg*)mem);

								mem += sizeof(GIFPackedReg);

								reg = reg & ((int)(reg - path.nreg) >> 31); // resets reg back to 0 when it becomes equal to path.nreg
							}
							while(--total > 0);
						}

						break;

					case GIFPath::TYPE_ADONLY: // very common

						do
						{
							(this->*m_fpGIFRegHandlers[((GIFPackedReg*)mem)->A_D.ADDR & 0x7F])(&((GIFPackedReg*)mem)->r);

							mem += sizeof(GIFPackedReg);
						}
						while(--total > 0);

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
					}
					while(path.StepReg() && size > 0);
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
				}
				while(path.StepReg() && size > 0);

				if(size & 1) mem += sizeof(GIFReg);

				size /= 2;

				break;

			case GIF_FLG_IMAGE2: // hmmm // Fall through here fixes a crash in Wallace and Gromit Project Zoo
				// and according to Pseudonym we shouldn't even land in this code. So hmm indeed. (rama)
				
				/*ASSERT(0);

				path.nloop = 0;

				break;*/

			case GIF_FLG_IMAGE:

				{
					int len = (int)std::min(size, path.nloop);

					//ASSERT(!(len&3));

					switch(m_env.TRXDIR.XDIR)
					{
					case 0:
						Write(mem, len * 16);
						break;
					case 1:
						// This can't happen; downloads can not be started or performed as part of
						// a GIFtag operation.  They're an entirely separate process that can only be
						// done through the ReverseFIFO transfer (aka ReadFIFO). --air
						ASSERT(0);
						//Read(mem, len * 16);
						break;
					case 2:
						Move();
						break;
					case 3:
						ASSERT(0);
						break;
					default:
						__assume(0);
					}

					mem += len * 16;
					path.nloop -= len;
					size -= len;
				}

				break;

			default:
				__assume(0);
			}
		}

		if(index == 0)
		{
			if(path.tag.EOP && path.nloop == 0)
			{
				break;
			}
		}
	}

	if(m_dump && mem > start)
	{
		m_dump->Transfer(index, start, mem - start);
	}

	if(index == 0)
	{
		if(size == 0 && path.nloop > 0)
		{
			if(m_mt)
			{
				// Hackfix for BIOS, which sends an incomplete packet when it does an XGKICK without
				// having an EOP specified anywhere in VU1 memory.  Needed until PCSX2 is fixed to
				// handle it more properly (ie, without looping infinitely).

				path.nloop = 0;
			}
			else
			{
				// Unused in 0.9.7 and above, but might as well keep this for now; allows GSdx
				// to work with legacy editions of PCSX2.

				Transfer<0>(mem - 0x4000, 0x4000 / 16);
			}
		}
	}
}

template<class T> static void WriteState(uint8*& dst, T* src, size_t len = sizeof(T))
{
	memcpy(dst, src, len);
	dst += len;
}

template<class T> static void ReadState(T* dst, uint8*& src, size_t len = sizeof(T))
{
	memcpy(dst, src, len);
	src += len;
}

int GSState::Freeze(GSFreezeData* fd, bool sizeonly)
{
	if(sizeonly)
	{
		fd->size = m_sssize;
		return 0;
	}

	if(!fd->data || fd->size < m_sssize)
	{
		return -1;
	}

	Flush();

	uint8* data = fd->data;

	WriteState(data, &m_version);
	WriteState(data, &m_env.PRIM);
	WriteState(data, &m_env.PRMODE);
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

	for(int i = 0; i < 2; i++)
	{
		WriteState(data, &m_env.CTXT[i].XYOFFSET);
		WriteState(data, &m_env.CTXT[i].TEX0);
		WriteState(data, &m_env.CTXT[i].TEX1);
		WriteState(data, &m_env.CTXT[i].TEX2);
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

	for(size_t i = 0; i < countof(m_path); i++)
	{
		m_path[i].tag.NREG = m_path[i].nreg;
		m_path[i].tag.NLOOP = m_path[i].nloop;
		m_path[i].tag.REGS = 0;

		for(size_t j = 0; j < countof(m_path[i].regs.u8); j++)
		{
			m_path[i].tag.u32[2 + (j >> 3)] |= m_path[i].regs.u8[j] << ((j & 7) << 2);
		}

		WriteState(data, &m_path[i].tag);
		WriteState(data, &m_path[i].reg);
	}

	WriteState(data, &m_q);

	return 0;
}

int GSState::Defrost(const GSFreezeData* fd)
{
	if(!fd || !fd->data || fd->size == 0)
	{
		return -1;
	}

	if(fd->size < m_sssize)
	{
		return -1;
	}

	uint8* data = fd->data;

	int version;

	ReadState(&version, data);

	if(version > m_version)
	{
		printf("GSdx: Savestate version is incompatible.  Load aborted.\n" );

		return -1;
	}

	Flush();

	Reset();

	ReadState(&m_env.PRIM, data);
	ReadState(&m_env.PRMODE, data);
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

	for(int i = 0; i < 2; i++)
	{
		ReadState(&m_env.CTXT[i].XYOFFSET, data);
		ReadState(&m_env.CTXT[i].TEX0, data);
		ReadState(&m_env.CTXT[i].TEX1, data);
		ReadState(&m_env.CTXT[i].TEX2, data);
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

		if(version <= 4)
		{
			data += sizeof(uint32) * 7; // skip
		}
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

	for(size_t i = 0; i < countof(m_path); i++)
	{
		ReadState(&m_path[i].tag, data);
		ReadState(&m_path[i].reg, data);

		m_path[i].SetTag(&m_path[i].tag); // expand regs
	}

	ReadState(&m_q, data);

	PRIM = !m_env.PRMODECONT.AC ? (GIFRegPRIM*)&m_env.PRMODE : &m_env.PRIM;

	UpdateContext();

	UpdateVertexKick();

	m_env.UpdateDIMX();

	for(size_t i = 0; i < 2; i++)
	{
		m_env.CTXT[i].UpdateScissor();

		m_env.CTXT[i].offset.fb = m_mem.GetOffset(m_env.CTXT[i].FRAME.Block(), m_env.CTXT[i].FRAME.FBW, m_env.CTXT[i].FRAME.PSM);
		m_env.CTXT[i].offset.zb = m_mem.GetOffset(m_env.CTXT[i].ZBUF.Block(), m_env.CTXT[i].FRAME.FBW, m_env.CTXT[i].ZBUF.PSM);
		m_env.CTXT[i].offset.tex = m_mem.GetOffset(m_env.CTXT[i].TEX0.TBP0, m_env.CTXT[i].TEX0.TBW, m_env.CTXT[i].TEX0.PSM);
		m_env.CTXT[i].offset.fzb = m_mem.GetPixelOffset(m_env.CTXT[i].FRAME, m_env.CTXT[i].ZBUF);
		m_env.CTXT[i].offset.fzb4 = m_mem.GetPixelOffset4(m_env.CTXT[i].FRAME, m_env.CTXT[i].ZBUF);
	}

	UpdateScissor();

m_perfmon.SetFrame(5000);

	return 0;
}

void GSState::SetGameCRC(uint32 crc, int options)
{
	m_crc = crc;
	m_options = options;
	m_game = CRC::Lookup(m_crc_hack_level != CRCHackLevel::None ? crc : 0);
	SetupCrcHack();

	// Until we find a solution that work for all games.
	// (if  a solution does exist)
	if (m_game.title == CRC::HarleyDavidson) {
		m_clut_load_before_draw = true;
	}
}

//

void GSState::UpdateContext()
{
	bool ctx_switch = (m_context != &m_env.CTXT[PRIM->CTXT]);
	if (ctx_switch) {
		GL_REG("Context Switch %d", PRIM->CTXT);
	}

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
	if(m_frameskip) return;

	uint32 prim = PRIM->PRIM;

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
	int maxcount = std::max<int>(m_vertex.maxcount * 3 / 2, 10000);

	GSVertex* vertex = (GSVertex*)_aligned_malloc(sizeof(GSVertex) * maxcount, 32);
	uint32* index = (uint32*)_aligned_malloc(sizeof(uint32) * maxcount * 3, 32); // worst case is slightly less than vertex number * 3

	if(vertex == NULL || index == NULL)
	{
		printf("GSdx: failed to allocate %d bytes for verticles and %d for indices.\n", (int)sizeof(GSVertex) * maxcount, (int)sizeof(uint32) * maxcount * 3);
		throw GSDXError();
	}

	if(m_vertex.buff != NULL)
	{
		memcpy(vertex, m_vertex.buff, sizeof(GSVertex) * m_vertex.tail);

		_aligned_free(m_vertex.buff);
	}

	if(m_index.buff != NULL)
	{
		memcpy(index, m_index.buff, sizeof(uint32) * m_index.tail);
		
		_aligned_free(m_index.buff);
	}

	m_vertex.buff = vertex;
	m_vertex.maxcount = maxcount - 3; // -3 to have some space at the end of the buffer before DrawingKick can grow it
	m_index.buff = index;
}

template<uint32 prim, bool auto_flush>
__forceinline void GSState::VertexKick(uint32 skip)
{
	ASSERT(m_vertex.tail < m_vertex.maxcount + 3);

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

	GSVector4i xy = v1.xxxx().u16to32().sub32(m_ofxy);

	#if _M_SSE >= 0x401
	GSVector4i::storel(&m_vertex.xy[xy_tail & 3], xy.blend16<0xf0>(xy.sra32(4)).ps32());
	#else
	GSVector4i::storel(&m_vertex.xy[xy_tail & 3], xy.upl64(xy.sra32(4).zwzw()).ps32());
	#endif

	m_vertex.tail = ++tail;
	m_vertex.xy_tail = ++xy_tail;

	size_t n = 0;

	switch(prim)
	{
	case GS_POINTLIST: n = 1; break;
	case GS_LINELIST: n = 2; break;
	case GS_LINESTRIP: n = 2; break;
	case GS_TRIANGLELIST: n = 3; break;
	case GS_TRIANGLESTRIP: n = 3; break;
	case GS_TRIANGLEFAN: n = 3; break;
	case GS_SPRITE: n = 2; break;
	case GS_INVALID: n = 1; break;
	}

	size_t m = tail - head;

	if(m < n)
	{
		return;
	}

	if(skip == 0 && (prim != GS_TRIANGLEFAN || m <= 4)) // m_vertex.xy only knows about the last 4 vertices, head could be far behind for fan
	{
		GSVector4i v0, v1, v2, v3, pmin, pmax;

		v0 = GSVector4i::loadl(&m_vertex.xy[(xy_tail + 1) & 3]); // T-3
		v1 = GSVector4i::loadl(&m_vertex.xy[(xy_tail + 2) & 3]); // T-2
		v2 = GSVector4i::loadl(&m_vertex.xy[(xy_tail + 3) & 3]); // T-1
		v3 = GSVector4i::loadl(&m_vertex.xy[(xy_tail - m) & 3]); // H

		GSVector4 cross;

		switch(prim)
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
		
		switch(prim)
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

		switch(prim)
		{
		case GS_TRIANGLELIST:
		case GS_TRIANGLESTRIP:
			// TODO: any way to do a 16-bit integer cross product?
			// cross product is zero most of the time because either of the vertices are the same
			/*
			cross = GSVector4(v2.xyxyl().i16to32().sub32(v0.upl32(v1).i16to32())); // x20, y20, x21, y21
			cross = cross * cross.wzwz(); // x20 * y21, y20 * x21
			test |= GSVector4i::cast(cross == cross.yxwz());
			*/
			test = (test | v0 == v1) | (v1 == v2 | v0 == v2); 
			break;
		case GS_TRIANGLEFAN:
			/*
			cross = GSVector4(v2.xyxyl().i16to32().sub32(v3.upl32(v1).i16to32())); // x23, y23, x21, y21
			cross = cross * cross.wzwz(); // x23 * y21, y23 * x21
			test |= GSVector4i::cast(cross == cross.yxwz());
			*/
			test = (test | v3 == v1) | (v1 == v2 | v3 == v2); 
			break;
		default:
			break;
		}
		
		skip |= test.mask() & 15;
	}

	if(skip != 0)
	{
		switch(prim)
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
			// fall through
		case GS_TRIANGLEFAN:
			if(tail >= m_vertex.maxcount) GrowVertexBuffer(); // in case too many vertices were skipped
			break;
		default: 
			__assume(0);
		}

		return;
	}

	if(tail >= m_vertex.maxcount) GrowVertexBuffer();

	uint32* RESTRICT buff = &m_index.buff[m_index.tail];

	switch(prim)
	{
	case GS_POINTLIST:
		buff[0] = head + 0;
		m_vertex.head = head + 1;
		m_vertex.next = head + 1;
		m_index.tail += 1;
		break;
	case GS_LINELIST:
		buff[0] = head + 0;
		buff[1] = head + 1;
		m_vertex.head = head + 2;
		m_vertex.next = head + 2;
		m_index.tail += 2;
		break;
	case GS_LINESTRIP:
		if(next < head) 
		{
			m_vertex.buff[next + 0] = m_vertex.buff[head + 0];
			m_vertex.buff[next + 1] = m_vertex.buff[head + 1];
			head = next; 
			m_vertex.tail = next + 2;
		}
		buff[0] = head + 0;
		buff[1] = head + 1;
		m_vertex.head = head + 1;
		m_vertex.next = head + 2;
		m_index.tail += 2;
		break;
	case GS_TRIANGLELIST:
		buff[0] = head + 0;
		buff[1] = head + 1;
		buff[2] = head + 2;
		m_vertex.head = head + 3;
		m_vertex.next = head + 3;
		m_index.tail += 3;
		break;
	case GS_TRIANGLESTRIP:
		if(next < head) 
		{
			m_vertex.buff[next + 0] = m_vertex.buff[head + 0];
			m_vertex.buff[next + 1] = m_vertex.buff[head + 1];
			m_vertex.buff[next + 2] = m_vertex.buff[head + 2];
			head = next; 
			m_vertex.tail = next + 3;
		}
		buff[0] = head + 0;
		buff[1] = head + 1;
		buff[2] = head + 2;
		m_vertex.head = head + 1;
		m_vertex.next = head + 3;
		m_index.tail += 3;
		break;
	case GS_TRIANGLEFAN:
		// TODO: remove gaps, next == head && head < tail - 3 || next > head && next < tail - 2 (very rare)
		buff[0] = head + 0;
		buff[1] = tail - 2;
		buff[2] = tail - 1;
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

	if (auto_flush && PRIM->TME && (m_context->FRAME.Block() == m_context->TEX0.TBP0))
		FlushPrim();
}

void GSState::GetTextureMinMax(GSVector4i& r, const GIFRegTEX0& TEX0, const GIFRegCLAMP& CLAMP, bool linear)
{
	// TODO: some of the +1s can be removed if linear == false

	int tw = TEX0.TW;
	int th = TEX0.TH;

	int w = 1 << tw;
	int h = 1 << th;

	GSVector4i tr(0, 0, w, h);

	int wms = CLAMP.WMS;
	int wmt = CLAMP.WMT;

	int minu = (int)CLAMP.MINU;
	int minv = (int)CLAMP.MINV;
	int maxu = (int)CLAMP.MAXU;
	int maxv = (int)CLAMP.MAXV;

	GSVector4i vr = tr;

	switch(wms)
	{
	case CLAMP_REPEAT:
		break;
	case CLAMP_CLAMP:
		break;
	case CLAMP_REGION_CLAMP:
		if(vr.x < minu) vr.x = minu;
		if(vr.z > maxu + 1) vr.z = maxu + 1;
		break;
	case CLAMP_REGION_REPEAT:
		vr.x = maxu;
		vr.z = vr.x + (minu + 1);
		break;
	default:
		__assume(0);
	}

	switch(wmt)
	{
	case CLAMP_REPEAT:
		break;
	case CLAMP_CLAMP:
		break;
	case CLAMP_REGION_CLAMP:
		if(vr.y < minv) vr.y = minv;
		if(vr.w > maxv + 1) vr.w = maxv + 1;
		break;
	case CLAMP_REGION_REPEAT:
		vr.y = maxv;
		vr.w = vr.y + (minv + 1);
		break;
	default:
		__assume(0);
	}

	if(wms != CLAMP_REGION_REPEAT || wmt != CLAMP_REGION_REPEAT)
	{
		GSVector4 st = m_vt.m_min.t.xyxy(m_vt.m_max.t);

		if(linear)
		{
			st += GSVector4(-0.5f, 0.5f).xxyy();
		}

		GSVector4i uv = GSVector4i(st.floor());

		GSVector4i u, v;

		int mask = 0;

		// See commented code below for the meaning of mask

		if(wms == CLAMP_REPEAT || wmt == CLAMP_REPEAT)
		{
			u = uv & GSVector4i::xffffffff().srl32(32 - tw);
			v = uv & GSVector4i::xffffffff().srl32(32 - th);

			GSVector4i uu = uv.sra32(tw);
			GSVector4i vv = uv.sra32(th);

			mask = (uu.upl32(vv) == uu.uph32(vv)).mask();
		}

		uv = uv.rintersect(tr);

		switch(wms)
		{
		case CLAMP_REPEAT:
			// This commented code cannot be used directly because it needs uv before the intersection
			/*if (uv_.x >> tw == uv_.z >> tw)
			{
				vr.x = std::max(vr.x, (uv_.x & ((1 << tw) - 1)));
				vr.z = std::min(vr.z, (uv_.z & ((1 << tw) - 1)) + 1);
			}*/
			if(mask & 0x000f) {if(vr.x < u.x) vr.x = u.x; if(vr.z > u.z + 1) vr.z = u.z + 1;}
			break;
		case CLAMP_CLAMP:
		case CLAMP_REGION_CLAMP:
			if(vr.x > uv.z) vr.z = vr.x + 1;
			else if(vr.z < uv.x) vr.x = vr.z - 1;
			else
			{
				if(vr.x < uv.x) vr.x = uv.x;
				if(vr.z > uv.z + 1) vr.z = uv.z + 1;
			}
			break;
		case CLAMP_REGION_REPEAT:
			break;
		default:
			__assume(0);
		}

		switch(wmt)
		{
		case CLAMP_REPEAT:
			/*if (uv_.y >> th == uv_.w >> th)
			{
				vr.y = max(vr.y, (uv_.y & ((1 << th) - 1)));
				vr.w = min(vr.w, (uv_.w & ((1 << th) - 1)) + 1);
			}*/
			if(mask & 0xf000) {if(vr.y < v.y) vr.y = v.y; if(vr.w > v.w + 1) vr.w = v.w + 1;}
			break;
		case CLAMP_CLAMP:
		case CLAMP_REGION_CLAMP:
			if(vr.y > uv.w) vr.w = vr.y + 1;
			else if(vr.w < uv.y) vr.y = vr.w - 1;
			else
			{
				if(vr.y < uv.y) vr.y = uv.y;
				if(vr.w > uv.w + 1) vr.w = uv.w + 1;
			}
			break;
		case CLAMP_REGION_REPEAT:
			break;
		default:
			__assume(0);
		}
	}

	vr = vr.rintersect(tr);

	// This really shouldn't happen now except with the clamping region set entirely outside the texture,
	// special handling should be written for that case.

	if(vr.rempty())
	{
		// NOTE: this can happen when texcoords are all outside the texture or clamping area is zero, but we can't 
		// let the texture cache update nothing, the sampler will still need a single texel from the border somewhere
		// examples: 
		// - THPS (no visible problems)
		// - NFSMW (strange rectangles on screen, might be unrelated)
		// - Lupin 3rd (huge problems, textures sizes seem to be randomly specified)

		vr = (vr + GSVector4i(-1, +1).xxyy()).rintersect(tr);
	}

	r = vr;
}

void GSState::GetAlphaMinMax()
{
	if(m_vt.m_alpha.valid)
	{
		return;
	}

	const GSDrawingEnvironment& env = m_env;
	const GSDrawingContext* context = m_context;

	GSVector4i a = m_vt.m_min.c.uph32(m_vt.m_max.c).zzww();

	if(PRIM->TME && context->TEX0.TCC)
	{
		switch(GSLocalMemory::m_psm[context->TEX0.PSM].fmt)
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

		switch(context->TEX0.TFX)
		{
		case TFX_MODULATE:
			a.x = (a.x * a.y) >> 7;
			a.z = (a.z * a.w) >> 7;
			if(a.x > 0xff) a.x = 0xff;
			if(a.z > 0xff) a.z = 0xff;
			break;
		case TFX_DECAL:
			a.x = a.y;
			a.z = a.w;
			break;
		case TFX_HIGHLIGHT:
			a.x = a.x + a.y;
			a.z = a.z + a.w;
			if(a.x > 0xff) a.x = 0xff;
			if(a.z > 0xff) a.z = 0xff;
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

bool GSState::TryAlphaTest(uint32& fm, uint32& zm)
{
	// Shortcut for the easy case
	if(m_context->TEST.ATST == ATST_ALWAYS)
		return true;

	// Alpha test can only control the write of some channels. If channels are already masked
	// the alpha test is therefore a nop.
	switch (m_context->TEST.AFAIL) {
		case AFAIL_KEEP:
			break;

		case AFAIL_FB_ONLY:
			if (zm == 0xFFFFFFFF)
				return true;

			break;

		case AFAIL_ZB_ONLY:
			if (fm == 0xFFFFFFFF)
				return true;

			break;

		case AFAIL_RGB_ONLY:
			if (zm == 0xFFFFFFFF && ((fm & 0xFF000000) == 0xFF000000 || GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt == 1))
				return true;
	}

	bool pass = true;

	if(m_context->TEST.ATST == ATST_NEVER)
	{
		pass = false; // Shortcut to avoid GetAlphaMinMax below
	}
	else
	{
		GetAlphaMinMax();

		int amin = m_vt.m_alpha.min;
		int amax = m_vt.m_alpha.max;

		int aref = m_context->TEST.AREF;

		switch(m_context->TEST.ATST)
		{
		case ATST_NEVER:
			pass = false;
			break;
		case ATST_ALWAYS:
			pass = true;
			break;
		case ATST_LESS:
			if(amax < aref) pass = true;
			else if(amin >= aref) pass = false;
			else return false;
			break;
		case ATST_LEQUAL:
			if(amax <= aref) pass = true;
			else if(amin > aref) pass = false;
			else return false;
			break;
		case ATST_EQUAL:
			if(amin == aref && amax == aref) pass = true;
			else if(amin > aref || amax < aref) pass = false;
			else return false;
			break;
		case ATST_GEQUAL:
			if(amin >= aref) pass = true;
			else if(amax < aref) pass = false;
			else return false;
			break;
		case ATST_GREATER:
			if(amin > aref) pass = true;
			else if(amax <= aref) pass = false;
			else return false;
			break;
		case ATST_NOTEQUAL:
			if(amin == aref && amax == aref) pass = false;
			else if(amin > aref || amax < aref) pass = true;
			else return false;
			break;
		default:
			__assume(0);
		}
	}

	if(!pass)
	{
		switch(m_context->TEST.AFAIL)
		{
		case AFAIL_KEEP: fm = zm = 0xffffffff; break;
		case AFAIL_FB_ONLY: zm = 0xffffffff; break;
		case AFAIL_ZB_ONLY: fm = 0xffffffff; break;
		case AFAIL_RGB_ONLY: fm |= 0xff000000; zm = 0xffffffff; break;
		default: __assume(0);
		}
	}

	return true;
}

bool GSState::IsOpaque()
{
	if(PRIM->AA1)
	{
		return false;
	}

	if(!PRIM->ABE)
	{
		return true;
	}

	const GSDrawingContext* context = m_context;

	int amin = 0, amax = 0xff;

	if(context->ALPHA.A != context->ALPHA.B)
	{
		if(context->ALPHA.C == 0)
		{
			GetAlphaMinMax();

			amin = m_vt.m_alpha.min;
			amax = m_vt.m_alpha.max;
		}
		else if(context->ALPHA.C == 1)
		{
			if(context->FRAME.PSM == PSM_PSMCT24 || context->FRAME.PSM == PSM_PSMZ24)
			{
				amin = amax = 0x80;
			}
		}
		else if(context->ALPHA.C == 2)
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

GIFRegTEX0 GSState::GetTex0Layer(uint32 lod)
{
	// Shortcut
	if (lod == 0) {
		return m_context->TEX0;
	}

	GIFRegTEX0 TEX0 = m_context->TEX0;

	switch(lod)
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
			fprintf(stderr, "GetTex0Layer bad parameter. Fix your code!\n");
			lod = 6;
			TEX0.TBP0 = m_context->MIPTBP2.TBP6;
			TEX0.TBW = m_context->MIPTBP2.TBW6;
	}

	// Correct the texture size
	if (TEX0.TH <= lod) {
		TEX0.TH = 1;
	} else {
		TEX0.TH -= lod;
	}
	if (TEX0.TW <= lod) {
		TEX0.TW = 1;
	} else {
		TEX0.TW -= lod;
	}

	return TEX0;
}

// GSTransferBuffer

GSState::GSTransferBuffer::GSTransferBuffer()
{
	x = y = 0;
	overflow = false;
	start = end = total = 0;
	buff = (uint8*)_aligned_malloc(1024 * 1024 * 4, 32);
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
	if(total == 0)
	{
		start = end = 0;
		total = std::min<int>((tw * bpp >> 3) * th, 1024 * 1024 * 4);
		overflow = false;
	}

	int remaining = total - end;

	if(len > remaining)
	{
		if(!overflow)
		{
			overflow = true;

			// printf("GS transfer overflow\n");
		}

		len = remaining;
	}

	return len > 0;
}
