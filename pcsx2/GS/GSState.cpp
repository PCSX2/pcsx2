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
#include "common/Path.h"
#include "common/StringUtil.h"

#include <algorithm> // clamp
#include <cfloat> // FLT_MAX
#include <fstream>
#include <iomanip> // Dump Verticles

int GSState::s_n = 0;
int GSState::s_last_transfer_draw_n = 0;
int GSState::s_transfer_n = 0;

static __fi bool IsAutoFlushEnabled()
{
	return (GSConfig.Renderer == GSRendererType::SW) ? GSConfig.AutoFlushSW : (GSConfig.UserHacks_AutoFlush != GSHWAutoFlushLevel::Disabled);
}

static __fi bool IsFirstProvokingVertex()
{
	return (GSConfig.Renderer != GSRendererType::SW && !g_gs_device->Features().provoking_vertex_last);
}

constexpr int GSState::GetSaveStateSize()
{
	int size = 0;

	size += sizeof(STATE_VERSION);
	size += sizeof(m_env.PRIM);
	size += sizeof(m_env.PRMODECONT);
	size += sizeof(m_env.TEXCLUT);
	size += sizeof(m_env.SCANMSK);
	size += sizeof(m_env.TEXA);
	size += sizeof(m_env.FOGCOL);
	size += sizeof(m_env.DIMX);
	size += sizeof(m_env.DTHE);
	size += sizeof(m_env.COLCLAMP);
	size += sizeof(m_env.PABE);
	size += sizeof(m_env.BITBLTBUF);
	size += sizeof(m_env.TRXDIR);
	size += sizeof(m_env.TRXPOS);
	size += sizeof(m_env.TRXREG);
	size += sizeof(m_env.TRXREG); // obsolete

	for (int i = 0; i < 2; i++)
	{
		size += sizeof(m_env.CTXT[i].XYOFFSET);
		size += sizeof(m_env.CTXT[i].TEX0);
		size += sizeof(m_env.CTXT[i].TEX1);
		size += sizeof(m_env.CTXT[i].CLAMP);
		size += sizeof(m_env.CTXT[i].MIPTBP1);
		size += sizeof(m_env.CTXT[i].MIPTBP2);
		size += sizeof(m_env.CTXT[i].SCISSOR);
		size += sizeof(m_env.CTXT[i].ALPHA);
		size += sizeof(m_env.CTXT[i].TEST);
		size += sizeof(m_env.CTXT[i].FBA);
		size += sizeof(m_env.CTXT[i].FRAME);
		size += sizeof(m_env.CTXT[i].ZBUF);
	}

	size += sizeof(m_v.RGBAQ);
	size += sizeof(m_v.ST);
	size += sizeof(m_v.UV);
	size += sizeof(m_v.FOG);
	size += sizeof(m_v.XYZ);
	size += sizeof(GIFReg); // obsolete

	size += sizeof(m_tr.x);
	size += sizeof(m_tr.y);
	size += GSLocalMemory::m_vmsize;
	size += (sizeof(GIFPath::tag) + sizeof(GIFPath::reg)) * 4 /* std::size(GSState::m_path) */; // std::size won't work without an instance.
	size += sizeof(m_q);

	return size;
}

GSState::GSState()
	: m_vt(this, IsFirstProvokingVertex())
{
	// m_nativeres seems to be a hack. Unfortunately it impacts draw call number which make debug painful in the replayer.
	// Let's keep it disabled to ease debug.
	m_nativeres = GSConfig.UpscaleMultiplier == 1.0f;
	m_mipmap = GSConfig.Mipmap;

	s_n = 0;
	s_transfer_n = 0;

	memset(&m_v, 0, sizeof(m_v));
	memset(&m_vertex, 0, sizeof(m_vertex));
	memset(&m_index, 0, sizeof(m_index));
	memset(m_mem.m_vm8, 0, m_mem.m_vmsize);

	m_v.RGBAQ.Q = 1.0f;

	GrowVertexBuffer();

	m_draw_transfers.clear();
	m_draw_transfers_double_buff.clear();

	PRIM = &m_env.PRIM;
	//CSR->rREV = 0x20;
	m_env.PRMODECONT.AC = 1;
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

std::string GSState::GetDrawDumpPath(const char* format, ...)
{
	std::va_list ap;
	va_start(ap, format);
	const std::string& base = GSConfig.UseHardwareRenderer() ? GSConfig.HWDumpDirectory : GSConfig.SWDumpDirectory;
	std::string ret(Path::Combine(base, StringUtil::StdStringFromFormatV(format, ap)));
	va_end(ap);
	return ret;
}

void GSState::Reset(bool hardware_reset)
{
	Flush(GSFlushReason::RESET);

	// FIXME: bios logo not shown cut in half after reset, missing graphics in GoW after first FMV
	memset(&m_path, 0, sizeof(m_path));
	memset(&m_v, 0, sizeof(m_v));

	m_env.Reset();

	PRIM = &m_env.PRIM;

	UpdateContext();

	UpdateVertexKick();

	for (u32 i = 0; i < 2; i++)
	{
		m_env.CTXT[i].UpdateScissor();

		// What is this nonsense? Basically, GOW does a 32x448 draw after resetting the GS, thinking the PSM for the framebuffer is going
		// to be set to C24, therefore the alpha bits get left alone. Because of the reset, in PCSX2, it ends up as C32, and the TC gets
		// confused, leading to a later texture load using this render target instead of local memory. It's a problem because the game
		// uploads texture data on startup to the beginning of VRAM, and never overwrites it.
		//
		// In the software renderer, if we let the draw happen, it gets scissored to 1x1 (because the scissor is inclusive of the
		// upper bounds). This doesn't seem to destroy the chest texture, presumably it's further out in memory.
		//
		// Hardware test show that VRAM gets corrupted on CSR reset, but the first page remains intact. We're guessing this has something
		// to do with DRAM refresh, and perhaps the internal counters used for refresh also getting reset. We're obviously not going
		// to emulate this, but to work around the aforementioned issue, in the hardware renderers, we set the scissor to an out of
		// bounds value. This means that draws get skipped until the game sets a proper scissor up, which is definitely going to happen
		// after reset (otherwise it'd only ever render 1x1).
		//
		if (!hardware_reset && GSConfig.UseHardwareRenderer())
			m_env.CTXT[i].scissor.cull = GSVector4i::xffffffff();

		m_env.CTXT[i].offset.fb = m_mem.GetOffset(m_env.CTXT[i].FRAME.Block(), m_env.CTXT[i].FRAME.FBW, m_env.CTXT[i].FRAME.PSM);
		m_env.CTXT[i].offset.zb = m_mem.GetOffset(m_env.CTXT[i].ZBUF.Block(), m_env.CTXT[i].FRAME.FBW, m_env.CTXT[i].ZBUF.PSM);
		m_env.CTXT[i].offset.fzb4 = m_mem.GetPixelOffset4(m_env.CTXT[i].FRAME, m_env.CTXT[i].ZBUF);
	}

	UpdateScissor();

	m_vertex.head = 0;
	m_vertex.tail = 0;
	m_vertex.next = 0;
	m_index.tail = 0;
	m_scanmask_used = 0;
	m_texflush_flag = false;
	m_dirty_gs_regs = 0;
	m_backed_up_ctx = -1;

	memcpy(&m_prev_env, &m_env, sizeof(m_prev_env));
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

void GSState::ResetPCRTC()
{
	PCRTCDisplays.SetVideoMode(GetVideoMode());
	PCRTCDisplays.EnableDisplays(m_regs->PMODE, m_regs->SMODE2, isReallyInterlaced());
	PCRTCDisplays.SetRects(0, m_regs->DISP[0].DISPLAY, m_regs->DISP[0].DISPFB);
	PCRTCDisplays.SetRects(1, m_regs->DISP[1].DISPLAY, m_regs->DISP[1].DISPFB);
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

bool GSState::isReallyInterlaced()
{
	// The FIELD register only flips if the CMOD field in SMODE1 is set to anything but 0 and Front Porch bottom bit in SYNCV is set.
	return (m_regs->SYNCV.VFP & 0x1) && m_regs->SMODE1.CMOD;
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

const char* GSState::GetFlushReasonString(GSFlushReason reason)
{
	switch (reason)
	{
		case GSFlushReason::RESET:
			return "RESET";
		case GSFlushReason::CONTEXTCHANGE:
			return "CONTEXT CHANGE";
		case GSFlushReason::CLUTCHANGE:
			return "CLUT CHANGE (RELOAD REQ)";
		case GSFlushReason::GSTRANSFER:
			return "GS TRANSFER";
		case GSFlushReason::UPLOADDIRTYTEX:
			return "GS UPLOAD OVERWRITES CURRENT TEXTURE OR CLUT";
		case GSFlushReason::UPLOADDIRTYFRAME:
			return "GS UPLOAD OVERWRITES CURRENT FRAME BUFFER";
		case GSFlushReason::UPLOADDIRTYZBUF:
			return "GS UPLOAD OVERWRITES CURRENT ZBUFFER";
		case GSFlushReason::LOCALTOLOCALMOVE:
			return "GS LOCAL TO LOCAL OVERWRITES CURRENT TEXTURE OR CLUT";
		case GSFlushReason::DOWNLOADFIFO:
			return "DOWNLOAD FIFO";
		case GSFlushReason::SAVESTATE:
			return "SAVESTATE";
		case GSFlushReason::LOADSTATE:
			return "LOAD SAVESTATE";
		case GSFlushReason::AUTOFLUSH:
			return "AUTOFLUSH OVERLAP DETECTED";
		case GSFlushReason::VSYNC:
			return "VSYNC";
		case GSFlushReason::GSREOPEN:
			return "GS REOPEN";
		case GSFlushReason::UNKNOWN:
		default:
			return "UNKNOWN";
	}
}

void GSState::DumpVertices(const std::string& filename)
{
	std::ofstream file(filename);

	if (!file.is_open())
		return;

	file << "FLUSH REASON: " << GetFlushReasonString(m_state_flush_reason);

	if (m_state_flush_reason != GSFlushReason::CONTEXTCHANGE && m_dirty_gs_regs)
		file << " AND POSSIBLE CONTEXT CHANGE";

	file << std::endl << std::endl;

	const u32 count = m_index.tail;
	GSVertex* buffer = &m_vertex.buff[0];

	const char* DEL = ", ";

	file << "VERTEX COORDS (XYZ)" << std::endl;
	file << std::fixed << std::setprecision(4);
	for (u32 i = 0; i < count; ++i)
	{
		file << "\t" << "v" << i << ": ";
		GSVertex v = buffer[m_index.buff[i]];

		const float x = (v.XYZ.X - (int)m_context->XYOFFSET.OFX) / 16.0f;
		const float y = (v.XYZ.Y - (int)m_context->XYOFFSET.OFY) / 16.0f;

		file << x << DEL;
		file << y << DEL;
		file << v.XYZ.Z;
		file << std::endl;
	}

	file << std::endl;

	file << "VERTEX COLOR (RGBA)" << std::endl;
	file << std::fixed << std::setprecision(6);
	for (u32 i = 0; i < count; ++i)
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

	const bool use_uv = PRIM->FST;
	const std::string qualifier = use_uv ? "UV" : "STQ";

	file << "TEXTURE COORDS (" << qualifier << ")" << std::endl;;
	for (u32 i = 0; i < count; ++i)
	{
		file << "\t" << "v" << i << ": ";
		const GSVertex v = buffer[m_index.buff[i]];

		// note
		// Yes, technically as far as the GS is concerned Q belongs
		// to RGBAQ. However, the purpose of this dump is to print
		// our data in a more human readable format and typically Q
		// is associated with STQ.
		if (use_uv)
		{
			const float uv_U = v.U / 16.0f;
			const float uv_V = v.V / 16.0f;

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
	if (m_dirty_gs_regs && m_index.tail > 0)
	{
		if (TestDrawChanged())
			Flush(GSFlushReason::CONTEXTCHANGE);
	}
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
		const GSVector4i st = GSVector4i::loadl(&r[0].U64[0]);
		GSVector4i q = GSVector4i::loadl(&r[0].U64[1]);
		const GSVector4i rgba = (GSVector4i::load<false>(&r[1]) & GSVector4i::x000000ff()).ps32().pu16();

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
		const GSVector4i st = GSVector4i::loadl(&r[0].U64[0]);
		GSVector4i q = GSVector4i::loadl(&r[0].U64[1]);
		const GSVector4i rgba = (GSVector4i::load<false>(&r[1]) & GSVector4i::x000000ff()).ps32().pu16();

		q = q.blend8(GSVector4i::cast(GSVector4::m_one), q == GSVector4i::zero()); // see GIFPackedRegHandlerSTQ

		m_v.m[0] = st.upl64(rgba.upl32(q)); // TODO: only store the last one

		const GSVector4i xy = GSVector4i::loadl(&r[2].U64[0]);
		const GSVector4i z = GSVector4i::loadl(&r[2].U64[1]);
		const GSVector4i xyz = xy.upl16(xy.srl<4>()).upl32(z);

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
		m_env.PRIM.PRIM = prim & 0x7;

	if (m_prev_env.PRIM.U32[0] ^ m_env.PRIM.U32[0])
		m_dirty_gs_regs |= (1 << DIRTY_REG_PRIM);
	else
		m_dirty_gs_regs &= ~(1<< DIRTY_REG_PRIM);

	UpdateVertexKick();

	ASSERT(m_index.tail == 0 || !g_gs_device->Features().provoking_vertex_last || m_index.buff[m_index.tail - 1] + 1 == m_vertex.next);

	if (m_index.tail == 0)
		m_vertex.next = 0;

	m_vertex.head = m_vertex.tail = m_vertex.next; // remove unused vertices from the end of the vertex buffer
}

void GSState::GIFRegHandlerPRIM(const GIFReg* RESTRICT r)
{
	ALIGN_STACK(32);

	ApplyPRIM(r->PRIM.U32[0]);
}

void GSState::GIFRegHandlerRGBAQ(const GIFReg* RESTRICT r)
{
	const GSVector4i rgbaq = (GSVector4i)r->RGBAQ;

	GSVector4i q = rgbaq.blend8(GSVector4i::cast(GSVector4::m_one), rgbaq == GSVector4i::zero()).yyyy(); // see GIFPackedRegHandlerSTQ

	// Silent Hill output a nan in Q to emulate the flash light. Unfortunately it
	// breaks GSVertexTrace code that rely on min/max.

	q = GSVector4i::cast(GSVector4::cast(q).replace_nan(GSVector4::m_max));

	m_v.RGBAQ = rgbaq.upl32(q);
}

void GSState::GIFRegHandlerST(const GIFReg* RESTRICT r)
{
	m_v.ST = r->ST;

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

	const GSVector4i xyzf = GSVector4i::loadl(&r->XYZF);
	const GSVector4i xyz = xyzf & (GSVector4i::xffffffff().upl32(GSVector4i::x00ffffff()));
	const GSVector4i uvf = GSVector4i::load((int)m_v.UV).upl32(xyzf.srl32(24).srl<4>());

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

	// Even if TEX0 did not change, a new palette may have been uploaded and will overwrite the currently queued for drawing.
	const bool wt = m_mem.m_clut.WriteTest(TEX0, m_env.TEXCLUT);

	if (wt)
	{
		m_mem.m_clut.SetNextCLUTTEX0(TEX0.U64);
		if (TEX0.CBP != m_mem.m_clut.GetCLUTCBP())
		{
			m_mem.m_clut.ClearDrawInvalidity();
			CLUTAutoFlush(PRIM->PRIM);
		}

		Flush(GSFlushReason::CLUTCHANGE);

		// Abort any channel shuffle skipping, since this is likely part of a new shuffle.
		// Test case: Tomb Raider series. This is gated by the CBP actually changing, because
		// Urban Chaos writes to the memory backing the CLUT in the middle of a shuffle, and
		// it's unclear whether the CLUT would actually get reloaded in that case.
		if (TEX0.CBP != m_mem.m_clut.GetCLUTCBP())
			m_channel_shuffle = false;
	}

	TEX0.CPSM &= 0xa; // 1010b

	m_env.CTXT[i].TEX0 = TEX0;

	if (wt)
	{
		GIFRegBITBLTBUF BITBLTBUF;
		GSVector4i r;

		if (TEX0.CSM == 0)
		{
			BITBLTBUF.SBP = TEX0.CBP;
			BITBLTBUF.SBW = 1;
			BITBLTBUF.SPSM = TEX0.CPSM;

			r.left = 0;
			r.top = 0;
			r.right = GSLocalMemory::m_psm[TEX0.CPSM].bs.x;
			r.bottom = GSLocalMemory::m_psm[TEX0.CPSM].bs.y;

			int blocks = 4;

			if (GSLocalMemory::m_psm[TEX0.CPSM].trbpp == 16)
				blocks >>= 1;

			if (GSLocalMemory::m_psm[TEX0.PSM].trbpp == 4)
				blocks >>= 1;

			// Invalidating videomem is slow, so *only* do it when it's definitely a CLUT draw in HW mode.
			for (int j = 0; j < blocks; j++, BITBLTBUF.SBP++)
				InvalidateLocalMem(BITBLTBUF, r, true);
		}
		else
		{
			BITBLTBUF.SBP = TEX0.CBP;
			BITBLTBUF.SBW = m_env.TEXCLUT.CBW;
			BITBLTBUF.SPSM = TEX0.CPSM;

			r.left = m_env.TEXCLUT.COU;
			r.top = m_env.TEXCLUT.COV;
			r.right = r.left + GSLocalMemory::m_psm[TEX0.CPSM].pal;
			r.bottom = r.top + 1;

			InvalidateLocalMem(BITBLTBUF, r, true);
		}

		m_mem.m_clut.Write(m_env.CTXT[i].TEX0, m_env.TEXCLUT);
	}

	constexpr u64 mask = 0x1f78001fffffffffull; // TBP0 TBW PSM TW TH TCC TFX CPSM CSA
	if (i == m_prev_env.PRIM.CTXT)
	{
		if ((m_prev_env.CTXT[i].TEX0.U64 ^ m_env.CTXT[i].TEX0.U64) & mask)
			m_dirty_gs_regs |= (1 << DIRTY_REG_TEX0);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_TEX0);
	}
}

template <int i>
void GSState::GIFRegHandlerTEX0(const GIFReg* RESTRICT r)
{
	GL_REG("TEX0_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	GIFRegTEX0 TEX0 = r->TEX0;
	// Max allowed MTBA size for 32bit swizzled textures (including 8H 4HL etc) is 512, 16bit and normal 8/4bit formats can be 1024
	const u32 maxTex = (GSLocalMemory::m_psm[TEX0.PSM].bpp < 32) ? 10 : 9;

	// Spec max is 10, but bitfield allows for up to 15
	// However STQ calculations expect the written size to be used for denormalization (Simple 2000 Series Vol 105 The Maid)
	// This is clamped to 10 in the FixedTEX0 functions so texture sizes don't exceed 1024x1024, but STQ can calculate properly (with invalid_tex0)
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
	TEX0.TW = std::clamp<u32>(TEX0.TW, 0, 15);
	TEX0.TH = std::clamp<u32>(TEX0.TH, 0, 15);

	// MTBA loads are triggered by writes to TEX0 (but not TEX2!)
	// Textures MUST be a minimum width of 32 pixels
	// Format must be a color, Z formats do not trigger MTBA (but are valid for Mipmapping)
	if (m_env.CTXT[i].TEX1.MTBA && TEX0.TW >= 5 && TEX0.TW <= maxTex && (TEX0.PSM & 0x30) != 0x30)
	{
		GIFRegMIPTBP1& mip_tbp1 = m_env.CTXT[i].MIPTBP1;
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

		mip_tbp1.TBP1 = bp;
		mip_tbp1.TBW1 = bw;

		bp += tex_size;
		bw = std::max<u32>(bw >> 1, 1);
		tex_size = std::max<u32>(tex_size >> 2, 1);

		mip_tbp1.TBP2 = bp;
		mip_tbp1.TBW2 = bw;

		bp += tex_size;
		bw = std::max<u32>(bw >> 1, 1);

		mip_tbp1.TBP3 = bp;
		mip_tbp1.TBW3 = bw;

		if (i == m_prev_env.PRIM.CTXT)
		{
			if (m_prev_env.CTXT[i].MIPTBP1.U64 ^ mip_tbp1.U64)
				m_dirty_gs_regs |= (1 << DIRTY_REG_MIPTBP1);
			else
				m_dirty_gs_regs &= ~(1 << DIRTY_REG_MIPTBP1);
		}
	}

	ApplyTEX0<i>(TEX0);
}

template <int i>
void GSState::GIFRegHandlerCLAMP(const GIFReg* RESTRICT r)
{
	GL_REG("CLAMP_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	m_env.CTXT[i].CLAMP = r->CLAMP;

	if (i == m_prev_env.PRIM.CTXT)
	{
		if (m_prev_env.CTXT[i].CLAMP.U64 ^ m_env.CTXT[i].CLAMP.U64)
			m_dirty_gs_regs |= (1 << DIRTY_REG_CLAMP);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_CLAMP);
	}
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

	m_env.CTXT[i].TEX1 = r->TEX1;

	if (i == m_prev_env.PRIM.CTXT)
	{
		if (m_prev_env.CTXT[i].TEX1.U64 ^ m_env.CTXT[i].TEX1.U64)
			m_dirty_gs_regs |= (1 << DIRTY_REG_TEX1);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_TEX1);
	}
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

	GIFRegTEX0 TEX0{};

	TEX0.U64 = (m_env.CTXT[i].TEX0.U64 & ~mask) | (r->U64 & mask);

	ApplyTEX0<i>(TEX0);
}

template <int i>
void GSState::GIFRegHandlerXYOFFSET(const GIFReg* RESTRICT r)
{
	GL_REG("XYOFFSET_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	const u64 r_masked = r->U64 & 0x0000FFFF0000FFFFu;

	if (i == m_prev_env.PRIM.CTXT)
	{
		if (m_prev_env.CTXT[i].XYOFFSET.U64 != r_masked)
			m_dirty_gs_regs |= (1 << DIRTY_REG_XYOFFSET);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_XYOFFSET);
	}

	if (m_env.CTXT[i].XYOFFSET.U64 == r_masked)
		return;

	m_env.CTXT[i].XYOFFSET.U64 = r_masked;

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
	m_env.PRIM = r->PRMODE;
	m_env.PRIM.PRIM = _PRIM;

	if (m_prev_env.PRIM.U32[0] ^ m_env.PRIM.U32[0])
		m_dirty_gs_regs |= (1 << DIRTY_REG_PRIM);
	else
		m_dirty_gs_regs &= ~(1 << DIRTY_REG_PRIM);

	UpdateContext();
}

void GSState::GIFRegHandlerTEXCLUT(const GIFReg* RESTRICT r)
{
	GL_REG("TEXCLUT = 0x%x_%x", r->U32[1], r->U32[0]);

	m_env.TEXCLUT = r->TEXCLUT;
}

void GSState::GIFRegHandlerSCANMSK(const GIFReg* RESTRICT r)
{
	m_env.SCANMSK = r->SCANMSK;

	if (m_env.SCANMSK.MSK & 2)
		m_scanmask_used = 2;

	if (m_prev_env.SCANMSK.MSK != m_env.SCANMSK.MSK)
		m_dirty_gs_regs |= (1 << DIRTY_REG_SCANMSK);
	else
		m_dirty_gs_regs &= ~(1 << DIRTY_REG_SCANMSK);
}

template <int i>
void GSState::GIFRegHandlerMIPTBP1(const GIFReg* RESTRICT r)
{
	GL_REG("MIPTBP1_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	m_env.CTXT[i].MIPTBP1 = r->MIPTBP1;

	if (i == m_prev_env.PRIM.CTXT)
	{
		if (m_prev_env.CTXT[i].MIPTBP1.U64 != m_env.CTXT[i].MIPTBP1.U64)
			m_dirty_gs_regs |= (1 << DIRTY_REG_MIPTBP1);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_MIPTBP1);
	}
}

template <int i>
void GSState::GIFRegHandlerMIPTBP2(const GIFReg* RESTRICT r)
{
	GL_REG("MIPTBP2_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	m_env.CTXT[i].MIPTBP2 = r->MIPTBP2;

	if (i == m_prev_env.PRIM.CTXT)
	{
		if (m_prev_env.CTXT[i].MIPTBP2.U64 != m_env.CTXT[i].MIPTBP2.U64)
			m_dirty_gs_regs |= (1 << DIRTY_REG_MIPTBP2);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_MIPTBP2);
	}
}

void GSState::GIFRegHandlerTEXA(const GIFReg* RESTRICT r)
{
	GL_REG("TEXA = 0x%x_%x", r->U32[1], r->U32[0]);

	m_env.TEXA = r->TEXA;

	if (m_prev_env.TEXA != m_env.TEXA)
		m_dirty_gs_regs |= (1 << DIRTY_REG_TEXA);
	else
		m_dirty_gs_regs &= ~(1 << DIRTY_REG_TEXA);
}

void GSState::GIFRegHandlerFOGCOL(const GIFReg* RESTRICT r)
{
	GL_REG("FOGCOL = 0x%x_%x", r->U32[1], r->U32[0]);

	m_env.FOGCOL = r->FOGCOL;

	if (m_prev_env.FOGCOL != m_env.FOGCOL)
		m_dirty_gs_regs |= (1 << DIRTY_REG_FOGCOL);
	else
		m_dirty_gs_regs &= ~(1 << DIRTY_REG_FOGCOL);
}

void GSState::GIFRegHandlerTEXFLUSH(const GIFReg* RESTRICT r)
{
	GL_REG("TEXFLUSH = 0x%x_%x PRIM TME %x", r->U32[1], r->U32[0], PRIM->TME);
	m_texflush_flag = true;
}

template <int i>
void GSState::GIFRegHandlerSCISSOR(const GIFReg* RESTRICT r)
{
	if (i == m_prev_env.PRIM.CTXT)
	{
		if (m_prev_env.CTXT[i].SCISSOR.U64 != r->SCISSOR.U64)
			m_dirty_gs_regs |= (1 << DIRTY_REG_SCISSOR);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_SCISSOR);
	}

	if (m_env.CTXT[i].SCISSOR.U64 == r->SCISSOR.U64)
		return;

	m_env.CTXT[i].SCISSOR = r->SCISSOR;
	m_env.CTXT[i].UpdateScissor();

	UpdateScissor();
}

template <int i>
void GSState::GIFRegHandlerALPHA(const GIFReg* RESTRICT r)
{
	GL_REG("ALPHA = 0x%x_%x", r->U32[1], r->U32[0]);

	m_env.CTXT[i].ALPHA = r->ALPHA;

	// value of 3 is not allowed by the spec
	// acts like 2 on real hw, so just clamp it
	m_env.CTXT[i].ALPHA.A = std::clamp<u32>(r->ALPHA.A, 0, 2);
	m_env.CTXT[i].ALPHA.B = std::clamp<u32>(r->ALPHA.B, 0, 2);
	m_env.CTXT[i].ALPHA.C = std::clamp<u32>(r->ALPHA.C, 0, 2);
	m_env.CTXT[i].ALPHA.D = std::clamp<u32>(r->ALPHA.D, 0, 2);

	if (i == m_prev_env.PRIM.CTXT)
	{
		if (m_prev_env.CTXT[i].ALPHA.U64 != m_env.CTXT[i].ALPHA.U64)
			m_dirty_gs_regs |= (1 << DIRTY_REG_ALPHA);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_ALPHA);
	}
}

void GSState::GIFRegHandlerDIMX(const GIFReg* RESTRICT r)
{
	m_env.DIMX = r->DIMX;

	if (m_prev_env.DIMX != m_env.DIMX)
		m_dirty_gs_regs |= (1 << DIRTY_REG_DIMX);
	else
		m_dirty_gs_regs &= ~(1 << DIRTY_REG_DIMX);
}

void GSState::GIFRegHandlerDTHE(const GIFReg* RESTRICT r)
{
	m_env.DTHE = r->DTHE;

	if (m_prev_env.DTHE != m_env.DTHE)
		m_dirty_gs_regs |= (1 << DIRTY_REG_DTHE);
	else
		m_dirty_gs_regs &= ~(1 << DIRTY_REG_DTHE);
}

void GSState::GIFRegHandlerCOLCLAMP(const GIFReg* RESTRICT r)
{
	m_env.COLCLAMP = r->COLCLAMP;

	if (m_prev_env.COLCLAMP != m_env.COLCLAMP)
		m_dirty_gs_regs |= (1 << DIRTY_REG_COLCLAMP);
	else
		m_dirty_gs_regs &= ~(1 << DIRTY_REG_COLCLAMP);
}

template <int i>
void GSState::GIFRegHandlerTEST(const GIFReg* RESTRICT r)
{
	m_env.CTXT[i].TEST = r->TEST;

	if (i == m_prev_env.PRIM.CTXT)
	{
		if (m_prev_env.CTXT[i].TEST != m_env.CTXT[i].TEST)
			m_dirty_gs_regs |= (1 << DIRTY_REG_TEST);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_TEST);
	}
}

void GSState::GIFRegHandlerPABE(const GIFReg* RESTRICT r)
{
	m_env.PABE = r->PABE;

	if (m_prev_env.PABE != m_env.PABE)
		m_dirty_gs_regs |= (1 << DIRTY_REG_PABE);
	else
		m_dirty_gs_regs &= ~(1 << DIRTY_REG_PABE);
}

template <int i>
void GSState::GIFRegHandlerFBA(const GIFReg* RESTRICT r)
{
	m_env.CTXT[i].FBA = r->FBA;

	if (i == m_prev_env.PRIM.CTXT)
	{
		if (m_prev_env.CTXT[i].FBA != m_env.CTXT[i].FBA)
			m_dirty_gs_regs |= (1 << DIRTY_REG_FBA);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_FBA);
	}
}

template <int i>
void GSState::GIFRegHandlerFRAME(const GIFReg* RESTRICT r)
{
	GL_REG("FRAME_%d = 0x%x_%x", i, r->U32[1], r->U32[0]);

	GIFRegFRAME NewFrame = r->FRAME;
	// FBW is clamped to 32
	NewFrame.FBW = std::min(NewFrame.FBW, 32U);

	if ((NewFrame.PSM & 0x30) == 0x30)
		m_env.CTXT[i].ZBUF.PSM &= ~0x30;
	else
		m_env.CTXT[i].ZBUF.PSM |= 0x30;

	if ((m_env.CTXT[i].FRAME.U32[0] ^ NewFrame.U32[0]) & 0x3f3f01ff) // FBP FBW PSM
	{
		m_env.CTXT[i].offset.fb = m_mem.GetOffset(NewFrame.Block(), NewFrame.FBW, NewFrame.PSM);
		m_env.CTXT[i].offset.zb = m_mem.GetOffset(m_env.CTXT[i].ZBUF.Block(), NewFrame.FBW, m_env.CTXT[i].ZBUF.PSM);
		m_env.CTXT[i].offset.fzb4 = m_mem.GetPixelOffset4(NewFrame, m_env.CTXT[i].ZBUF);
	}

	m_env.CTXT[i].FRAME = NewFrame;

	switch (m_env.CTXT[i].FRAME.PSM)
	{
		case PSMT8H:
			// Berserk uses the format to only update the alpha channel
			GL_INS("CORRECT FRAME FORMAT replaces PSMT8H by PSMCT32/0x00FF_FFFF");
			m_env.CTXT[i].FRAME.PSM = PSMCT32;
			m_env.CTXT[i].FRAME.FBMSK = 0x00FFFFFF;
			break;
		case PSMT4HH: // Not tested. Based on PSMT8H behavior
			GL_INS("CORRECT FRAME FORMAT replaces PSMT4HH by PSMCT32/0x0FFF_FFFF");
			m_env.CTXT[i].FRAME.PSM = PSMCT32;
			m_env.CTXT[i].FRAME.FBMSK = 0x0FFFFFFF;
			break;
		case PSMT4HL: // Not tested. Based on PSMT8H behavior
			GL_INS("CORRECT FRAME FORMAT replaces PSMT4HL by PSMCT32/0xF0FF_FFFF");
			m_env.CTXT[i].FRAME.PSM = PSMCT32;
			m_env.CTXT[i].FRAME.FBMSK = 0xF0FFFFFF;
			break;
		default:
			break;
	}

	if (i == m_prev_env.PRIM.CTXT)
	{
		if (m_prev_env.CTXT[i].FRAME != m_env.CTXT[i].FRAME)
			m_dirty_gs_regs |= (1 << DIRTY_REG_FRAME);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_FRAME);
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

	if ((m_env.CTXT[i].ZBUF.U32[0] ^ ZBUF.U32[0]) & 0x3f0001ff) // ZBP PSM
	{
		m_env.CTXT[i].offset.zb = m_mem.GetOffset(ZBUF.Block(), m_env.CTXT[i].FRAME.FBW, ZBUF.PSM);
		m_env.CTXT[i].offset.fzb4 = m_mem.GetPixelOffset4(m_env.CTXT[i].FRAME, ZBUF);
	}

	m_env.CTXT[i].ZBUF = ZBUF;

	if (i == m_prev_env.PRIM.CTXT)
	{
		if (m_prev_env.CTXT[i].ZBUF != m_env.CTXT[i].ZBUF)
			m_dirty_gs_regs |= (1 << DIRTY_REG_ZBUF);
		else
			m_dirty_gs_regs &= ~(1 << DIRTY_REG_ZBUF);
	}
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

	m_env.BITBLTBUF = r->BITBLTBUF;
}

void GSState::GIFRegHandlerTRXPOS(const GIFReg* RESTRICT r)
{
	GL_REG("TRXPOS = 0x%x_%x", r->U32[1], r->U32[0]);

	if (r->TRXPOS != m_env.TRXPOS)
		FlushWrite();

	m_env.TRXPOS = r->TRXPOS;
}

void GSState::GIFRegHandlerTRXREG(const GIFReg* RESTRICT r)
{
	GL_REG("TRXREG = 0x%x_%x", r->U32[1], r->U32[0]);
	if (r->TRXREG != m_env.TRXREG)
		FlushWrite();

	m_env.TRXREG = r->TRXREG;
}

void GSState::GIFRegHandlerTRXDIR(const GIFReg* RESTRICT r)
{
	GL_REG("TRXDIR = 0x%x_%x", r->U32[1], r->U32[0]);

	Flush(GSFlushReason::GSTRANSFER);

	m_env.TRXDIR = r->TRXDIR;

	switch (m_env.TRXDIR.XDIR)
	{
		case 0: // host -> local
			m_tr.Init(m_env.TRXPOS.DSAX, m_env.TRXPOS.DSAY, m_env.BITBLTBUF, true);
			break;
		case 1: // local -> host
			m_tr.Init(m_env.TRXPOS.SSAX, m_env.TRXPOS.SSAY, m_env.BITBLTBUF, false);
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

void GSState::Flush(GSFlushReason reason)
{
	FlushWrite();

	if (m_index.tail > 0)
	{
		m_state_flush_reason = reason;

		if (m_dirty_gs_regs)
		{
			m_draw_env = &m_prev_env;
			PRIM = &m_prev_env.PRIM;
			UpdateContext();

			FlushPrim();

			m_draw_env = &m_env;
			PRIM = &m_env.PRIM;
			UpdateContext();

			m_backed_up_ctx = -1;
		}
		else
		{
			FlushPrim();
		}

		m_dirty_gs_regs = 0;
	}

	m_state_flush_reason = GSFlushReason::UNKNOWN;
}

void GSState::FlushWrite()
{
	if (!m_tr.write)
		return;

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

	wi(m_mem, m_tr.x, m_tr.y, &m_tr.buff[m_tr.start], len, m_env.BITBLTBUF, m_env.TRXPOS, m_env.TRXREG);

	m_tr.start += len;

	g_perfmon.Put(GSPerfMon::Swizzle, len);
	s_transfer_n++;
}

// This function decides if the context has changed in a way which warrants flushing the draw.
inline bool GSState::TestDrawChanged()
{
	// Check if PRIM has changed we need to check if it's just a different triangle or the context is changing.
	if (m_dirty_gs_regs & (1 << DIRTY_REG_PRIM))
	{
		u32 prim_mask = 0x7ff;

		if (GSUtil::GetPrimClass(m_prev_env.PRIM.PRIM) == GSUtil::GetPrimClass(m_env.PRIM.PRIM))
			prim_mask &= ~0x7;
		else
			return true;

		if ((m_env.PRIM.U32[0] ^ m_prev_env.PRIM.U32[0]) & prim_mask)
			return true;

		m_dirty_gs_regs &= ~(1 << DIRTY_REG_PRIM);

		// Shortcut, a bunch of games just change the prim reg
		if (!m_dirty_gs_regs)
			return false;
	}

	if ((m_dirty_gs_regs & ((1 << DIRTY_REG_TEST) | (1 << DIRTY_REG_SCISSOR) | (1 << DIRTY_REG_XYOFFSET) | (1 << DIRTY_REG_SCANMSK) | (1 << DIRTY_REG_DTHE))) || ((m_dirty_gs_regs & (1 << DIRTY_REG_DIMX)) && m_prev_env.DTHE.DTHE))
		return true;

	if (m_env.PRIM.ABE && (m_dirty_gs_regs & ((1 << DIRTY_REG_ALPHA) | (1 << DIRTY_REG_PABE))))
		return true;

	if (m_env.PRIM.FGE && (m_dirty_gs_regs & (1 << DIRTY_REG_FOGCOL)))
		return true;

	const int context = m_env.PRIM.CTXT;

	// If the frame is getting updated check the FRAME, otherwise, we can ignore it
	if ((m_env.CTXT[context].TEST.ATST != ATST_NEVER) || !m_env.CTXT[context].TEST.ATE || (m_env.CTXT[context].TEST.AFAIL & 1) || m_env.CTXT[context].TEST.DATE)
	{
		if ((m_dirty_gs_regs & ((1 << DIRTY_REG_FRAME) | (1 << DIRTY_REG_COLCLAMP) | (1 << DIRTY_REG_FBA))))
			return true;
	}

	if ((m_env.CTXT[context].TEST.ATST != ATST_NEVER) || !m_env.CTXT[context].TEST.ATE || m_env.CTXT[context].TEST.AFAIL == AFAIL_ZB_ONLY)
	{
		if (m_dirty_gs_regs & (1 << DIRTY_REG_ZBUF))
			return true;
	}

	if (m_env.PRIM.TME)
	{
		if (m_dirty_gs_regs & ((1 << DIRTY_REG_TEX0) | (1 << DIRTY_REG_TEX1) | (1 << DIRTY_REG_CLAMP) | (1 << DIRTY_REG_TEXA)))
			return true;

		if(m_env.CTXT[context].TEX1.MXL > 0 && (m_dirty_gs_regs & ((1 << DIRTY_REG_MIPTBP1) | (1 << DIRTY_REG_MIPTBP2))))
			return true;
	}

	m_dirty_gs_regs = 0;

	return false;
}

void GSState::FlushPrim()
{
	if (m_index.tail > 0)
	{
		GL_REG("FlushPrim ctxt %d", PRIM->CTXT);

		// clear texture cache flushed flag, since we're reading from it
		m_texflush_flag = PRIM->TME ? false : m_texflush_flag;

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

		const u32 head = m_vertex.head;
		const u32 tail = m_vertex.tail;
		const u32 next = m_vertex.next;
		u32 unused = 0;

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
					unused = std::min<u32>(tail - head, 2);
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
#ifdef PCSX2_DEVBUILD
		const bool ignoreZ = m_context->ZBUF.ZMSK && m_context->TEST.ZTST == 1;
		if (GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt >= 3 || (GSLocalMemory::m_psm[m_context->ZBUF.PSM].fmt >= 3 && !ignoreZ))
		{
			Console.Warning("GS: Possible invalid draw, Frame PSM %x ZPSM %x", m_context->FRAME.PSM, m_context->ZBUF.PSM);
		}
#endif

		m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail, m_index.tail, GSUtil::GetPrimClass(PRIM->PRIM));

		try
		{
			// Skip draw if Z test is enabled, but set to fail all pixels.
			const bool skip_draw = (m_context->TEST.ZTE && m_context->TEST.ZTST == ZTST_NEVER);

			if (!skip_draw)
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

		g_perfmon.Put(GSPerfMon::Draw, 1);
		g_perfmon.Put(GSPerfMon::Prim, m_index.tail / GSUtil::GetVertexCount(PRIM->PRIM));

		m_index.tail = 0;
		m_vertex.head = 0;

		if (unused > 0)
		{
			memcpy(m_vertex.buff, buff, sizeof(GSVertex) * unused);

			m_vertex.tail = unused;
			m_vertex.next = next > head ? next - head : 0;

			// If it's a Triangle fan the XY buffer needs to be updated to point to the correct head vert
			// Jak 3 shadows get spikey (with autoflush) if you don't.
			if (PRIM->PRIM == GS_TRIANGLEFAN)
			{
				for (u32 i = 0; i < unused; i++)
				{
					GSVector4i* RESTRICT vert_ptr = (GSVector4i*)&m_vertex.buff[i];
					GSVector4i v = vert_ptr[1];
					v = v.xxxx().u16to32().sub32(m_xyof);
					v = v.blend32<12>(v.sra32(4));
					m_vertex.xy[i & 3] = v;
					m_vertex.xy_tail = unused;
				}
			}
		}
		else
		{
			m_vertex.tail = 0;
			m_vertex.next = 0;
		}
	}
}

void GSState::Write(const u8* mem, int len)
{
	const int w = m_env.TRXREG.RRW;
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
		if (blit.DBP == 0 && blit.DPSM == PSMZ32 && w == 512 && h > 224)
		{
			h = 224;
			m_env.TRXREG.RRH = 224;
		}
	}

	if (!m_tr.Update(w, h, psm.trbpp, len))
		return;

	if (m_tr.end == 0)
	{
		const GSDrawingContext& prev_ctx = m_prev_env.CTXT[m_prev_env.PRIM.CTXT];
		const u32 write_start_bp = m_mem.m_psm[blit.DPSM].info.bn(m_env.TRXPOS.DSAX, m_env.TRXPOS.DSAY, blit.DBP, blit.DBW);
		const u32 write_end_bp = m_mem.m_psm[blit.DPSM].info.bn(m_env.TRXPOS.DSAX + w - 1, m_env.TRXPOS.DSAY + h - 1, blit.DBP, blit.DBW);

		// Only flush on a NEW transfer if a pending one is using the same address or overlap.
		// Check Fast & Furious (Hardare mode) and Assault Suits Valken (either renderer) and Tomb Raider - Angel of Darkness menu (TBP != DBP but overlaps).
		if (m_index.tail > 0)
		{
			const u32 tex_end_bp = m_mem.m_psm[prev_ctx.TEX0.PSM].info.bn((1 << prev_ctx.TEX0.TW) - 1, (1 << prev_ctx.TEX0.TH) - 1, prev_ctx.TEX0.TBP0, prev_ctx.TEX0.TBW);

			if (m_prev_env.PRIM.TME && write_end_bp > prev_ctx.TEX0.TBP0 && write_start_bp <= tex_end_bp)
				Flush(GSFlushReason::UPLOADDIRTYTEX);
			else
			{
				const u32 frame_mask = GSLocalMemory::m_psm[prev_ctx.FRAME.PSM].fmsk;
				const bool frame_required = (!(prev_ctx.TEST.ATE && prev_ctx.TEST.ATST == 0 && prev_ctx.TEST.AFAIL == 2) && ((prev_ctx.FRAME.FBMSK & frame_mask) != frame_mask)) || (prev_ctx.TEST.ATE && prev_ctx.TEST.ATST > ATST_ALWAYS);
				if (frame_required)
				{
					const u32 draw_start_bp = m_mem.m_psm[prev_ctx.FRAME.PSM].info.bn(temp_draw_rect.x, temp_draw_rect.y, prev_ctx.FRAME.Block(), prev_ctx.FRAME.FBW);
					const u32 draw_end_bp = m_mem.m_psm[prev_ctx.FRAME.PSM].info.bn(temp_draw_rect.z - 1, temp_draw_rect.w - 1, prev_ctx.FRAME.Block(), prev_ctx.FRAME.FBW);

					if (write_end_bp > draw_start_bp && write_start_bp <= draw_end_bp)
						Flush(GSFlushReason::UPLOADDIRTYFRAME);
				}

				const bool zbuf_required = (!(prev_ctx.TEST.ATE && prev_ctx.TEST.ATST == 0 && prev_ctx.TEST.AFAIL != 2) && !prev_ctx.ZBUF.ZMSK) || (prev_ctx.TEST.ZTE && prev_ctx.TEST.ZTST > ZTST_ALWAYS);
				if (zbuf_required)
				{
					const u32 draw_start_bp = m_mem.m_psm[prev_ctx.ZBUF.PSM].info.bn(temp_draw_rect.x, temp_draw_rect.y, prev_ctx.ZBUF.Block(), prev_ctx.FRAME.FBW);
					const u32 draw_end_bp = m_mem.m_psm[prev_ctx.ZBUF.PSM].info.bn(temp_draw_rect.z - 1, temp_draw_rect.w - 1, prev_ctx.ZBUF.Block(), prev_ctx.FRAME.FBW);

					if (write_end_bp > draw_start_bp && write_start_bp <= draw_end_bp)
						Flush(GSFlushReason::UPLOADDIRTYZBUF);
				}
			}
		}
		// Invalid the CLUT if it crosses paths.
		m_mem.m_clut.InvalidateRange(write_start_bp, write_end_bp);

		GSVector4i r;

		r.left = m_env.TRXPOS.DSAX;
		r.top = m_env.TRXPOS.DSAY;
		r.right = r.left + m_env.TRXREG.RRW;
		r.bottom = r.top + m_env.TRXREG.RRH;

		s_last_transfer_draw_n = s_n;
		// Store the transfer for preloading new RT's.
		if ((m_draw_transfers.size() > 0 && blit.DBP == m_draw_transfers.back().blit.DBP))
		{
			// Same BP, let's update the rect.
			GSUploadQueue transfer = m_draw_transfers.back();
			m_draw_transfers.pop_back();
			transfer.rect = transfer.rect.runion(r);
			m_draw_transfers.push_back(transfer);
		}
		else
		{
			GSUploadQueue new_transfer = { blit, r, s_n };
			m_draw_transfers.push_back(new_transfer);
		}

		GL_CACHE("Write! %u ...  => 0x%x W:%d F:%s (DIR %d%d), dPos(%d %d) size(%d %d)", s_transfer_n,
				blit.DBP, blit.DBW, psm_str(blit.DPSM),
				m_env.TRXPOS.DIRX, m_env.TRXPOS.DIRY,
				m_env.TRXPOS.DSAX, m_env.TRXPOS.DSAY, w, h);

		if (len >= m_tr.total)
		{
			// received all data in one piece, no need to buffer it
			InvalidateVideoMem(blit, r);

			psm.wi(m_mem, m_tr.x, m_tr.y, mem, m_tr.total, blit, m_env.TRXPOS, m_env.TRXREG);

			m_tr.start = m_tr.end = m_tr.total;

			g_perfmon.Put(GSPerfMon::Swizzle, len);
			s_transfer_n++;

			return;
		}
	}

	memcpy(&m_tr.buff[m_tr.end], mem, len);

	m_tr.end += len;

	if (m_tr.end >= m_tr.total)
		FlushWrite();
}

void GSState::InitReadFIFO(u8* mem, int len)
{
	// No size or already a transfer in progress.
	if (len <= 0 || m_tr.total != 0)
		return;

	const int w = m_env.TRXREG.RRW;
	const int h = m_env.TRXREG.RRH;

	const u16 bpp = GSLocalMemory::m_psm[m_env.BITBLTBUF.SPSM].trbpp;

	if (!m_tr.Update(w, h, bpp, len))
		return;

	const int sx = m_env.TRXPOS.SSAX;
	const int sy = m_env.TRXPOS.SSAY;
	const GSVector4i r(sx, sy, sx + w, sy + h);

	if (m_tr.x == sx && m_tr.y == sy)
		InvalidateLocalMem(m_env.BITBLTBUF, r);

	// Read the image all in one go.
	m_mem.ReadImageX(m_tr.x, m_tr.y, m_tr.buff, m_tr.total, m_env.BITBLTBUF, m_env.TRXPOS, m_env.TRXREG);

	if (GSConfig.DumpGSData && GSConfig.SaveRT && s_n >= GSConfig.SaveN)
	{
		const std::string s(GetDrawDumpPath(
			"%05d_read_%05x_%d_%d_%d_%d_%d_%d.bmp",
			s_n, (int)m_env.BITBLTBUF.SBP, (int)m_env.BITBLTBUF.SBW, (int)m_env.BITBLTBUF.SPSM,
			r.left, r.top, r.right, r.bottom));

		m_mem.SaveBMP(s, m_env.BITBLTBUF.SBP, m_env.BITBLTBUF.SBW, m_env.BITBLTBUF.SPSM, r.right, r.bottom);
	}
}

// NOTE: called from outside MTGS
void GSState::Read(u8* mem, int len)
{
	if (len <= 0 || m_tr.total == 0)
		return;

	const int w = m_env.TRXREG.RRW;
	const int h = m_env.TRXREG.RRH;
	const u16 bpp = GSLocalMemory::m_psm[m_env.BITBLTBUF.SPSM].trbpp;

	if (!m_tr.Update(w, h, bpp, len))
		return;

	// If it wraps memory, we need to break it up so we don't read out of bounds.
	if ((m_tr.end + len) > m_mem.m_vmsize)
	{
		const int first_transfer = m_mem.m_vmsize - m_tr.end;
		const int second_transfer = len - first_transfer;
		memcpy(mem, &m_tr.buff[m_tr.end], first_transfer);
		m_tr.end = 0;
		memcpy(&mem[first_transfer], &m_tr.buff, second_transfer);
		m_tr.end = second_transfer;
	}
	else
	{
		memcpy(mem, &m_tr.buff[m_tr.end], len);
		m_tr.end += len;
	}
}

void GSState::Move()
{
	// ffxii uses this to move the top/bottom of the scrolling menus offscreen and then blends them back over the text to create a shading effect
	// guitar hero copies the far end of the board to do a similar blend too
	s_transfer_n++;

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

	const int sbp = m_env.BITBLTBUF.SBP;
	const int sbw = m_env.BITBLTBUF.SBW;
	const int dbp = m_env.BITBLTBUF.DBP;
	const int dbw = m_env.BITBLTBUF.DBW;
	const GSOffset spo = m_mem.GetOffset(sbp, sbw, m_env.BITBLTBUF.SPSM);
	const GSOffset dpo = m_mem.GetOffset(dbp, dbw, m_env.BITBLTBUF.DPSM);

	const GSDrawingContext& prev_ctx = m_prev_env.CTXT[m_prev_env.PRIM.CTXT];
	const u32 write_start_bp = m_mem.m_psm[m_env.BITBLTBUF.DPSM].info.bn(m_env.TRXPOS.DSAX, m_env.TRXPOS.DSAY, dbp, dbw);
	const u32 write_end_bp = m_mem.m_psm[m_env.BITBLTBUF.DPSM].info.bn(m_env.TRXPOS.DSAX + w - 1, m_env.TRXPOS.DSAY + h - 1, dbp, dbw);

	// Only flush on a NEW transfer if a pending one is using the same address or overlap.
	// Unknown if games use this one, but best to be safe.
	if (m_index.tail > 0)
	{
		const u32 tex_end_bp = m_mem.m_psm[prev_ctx.TEX0.PSM].info.bn((1 << prev_ctx.TEX0.TW) - 1, (1 << prev_ctx.TEX0.TH) - 1, prev_ctx.TEX0.TBP0, prev_ctx.TEX0.TBW);

		if (m_prev_env.PRIM.TME && write_end_bp >= prev_ctx.TEX0.TBP0 && write_start_bp <= tex_end_bp)
			Flush(GSFlushReason::LOCALTOLOCALMOVE);
		else
		{
			const u32 frame_mask = GSLocalMemory::m_psm[prev_ctx.FRAME.PSM].fmsk;
			const bool frame_required = (!(prev_ctx.TEST.ATE && prev_ctx.TEST.ATST == 0 && prev_ctx.TEST.AFAIL == 2) && ((prev_ctx.FRAME.FBMSK & frame_mask) != frame_mask)) || (prev_ctx.TEST.ATE && prev_ctx.TEST.ATST > ATST_ALWAYS);
			if (frame_required)
			{
				const u32 draw_start_bp = m_mem.m_psm[prev_ctx.FRAME.PSM].info.bn(temp_draw_rect.x, temp_draw_rect.y, prev_ctx.FRAME.Block(), prev_ctx.FRAME.FBW);
				const u32 draw_end_bp = m_mem.m_psm[prev_ctx.FRAME.PSM].info.bn(temp_draw_rect.z - 1, temp_draw_rect.w - 1, prev_ctx.FRAME.Block(), prev_ctx.FRAME.FBW);

				if (write_end_bp > draw_start_bp && write_start_bp <= draw_end_bp)
					Flush(GSFlushReason::UPLOADDIRTYFRAME);
			}

			const bool zbuf_required = (!(prev_ctx.TEST.ATE && prev_ctx.TEST.ATST == 0 && prev_ctx.TEST.AFAIL != 2) && !prev_ctx.ZBUF.ZMSK) || (prev_ctx.TEST.ZTE && prev_ctx.TEST.ZTST > ZTST_ALWAYS);
			if (zbuf_required)
			{
				const u32 draw_start_bp = m_mem.m_psm[prev_ctx.ZBUF.PSM].info.bn(temp_draw_rect.x, temp_draw_rect.y, prev_ctx.ZBUF.Block(), prev_ctx.FRAME.FBW);
				const u32 draw_end_bp = m_mem.m_psm[prev_ctx.ZBUF.PSM].info.bn(temp_draw_rect.z - 1, temp_draw_rect.w - 1, prev_ctx.ZBUF.Block(), prev_ctx.FRAME.FBW);

				if (write_end_bp > draw_start_bp && write_start_bp <= draw_end_bp)
					Flush(GSFlushReason::UPLOADDIRTYZBUF);
			}
		}
	}

	GSVector4i r;
	r.left = m_env.TRXPOS.DSAX;
	r.top = m_env.TRXPOS.DSAY;
	r.right = r.left + m_env.TRXREG.RRW;
	r.bottom = r.top + m_env.TRXREG.RRH;

	s_last_transfer_draw_n = s_n;
	// Store the transfer for preloading new RT's.
	if ((m_draw_transfers.size() > 0 && m_env.BITBLTBUF.DBP == m_draw_transfers.back().blit.DBP))
	{
		// Same BP, let's update the rect.
		GSUploadQueue transfer = m_draw_transfers.back();
		m_draw_transfers.pop_back();
		transfer.rect = transfer.rect.runion(r);
		m_draw_transfers.push_back(transfer);
	}
	else
	{
		GSUploadQueue new_transfer = { m_env.BITBLTBUF, r, s_n };
		m_draw_transfers.push_back(new_transfer);
	}

	// Invalid the CLUT if it crosses paths.
	m_mem.m_clut.InvalidateRange(write_start_bp, write_end_bp);

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
			// No need to do all this if the copy source/destination don't intersect, however.
			const bool intersect = !(GSVector4i(sx, sy, sx + w, sy + h).rintersect(GSVector4i(dx, dy, dx + w, dy + h)).rempty());

			if (intersect && sbp == dbp && (((_sy < _dy) && ((ypage + page_height) > _dy)) || ((sx < dx) && ((xpage + page_width) > dx))))
			{
				int starty = (yinc > 0) ? 0 : h-1;
				int endy = (yinc > 0) ? h : -1;
				int y_inc = yinc;

				if (((_sy < _dy) && ((ypage + page_height) > _dy)) && yinc > 0)
				{
					_sy += h-1;
					_dy += h-1;
					starty = h-1;
					endy = -1;
					y_inc = -y_inc;
				}

				for (int y = starty; y != endy; y+= y_inc, _sy += y_inc, _dy += y_inc)
				{
					auto s = getPAHelper(spo, 0, _sy);
					auto d = getPAHelper(dpo, 0, _dy);

					if (((sx < dx) && ((xpage + page_width) > dx)))
					{
						for (int x = w - 1; x >= 0; x--)
						{
							pxCopyFn(d, s, (dx + x) & 2047, (sx + x) & 2047);
						}
					}
					else
					{
						for (int x = 0; x < w; x++)
						{
							pxCopyFn(d, s, (dx + x) & 2047, (sx + x) & 2047);
						}
					}
				}
			}
			else
			{
				for (int y = 0; y < h; y++, _sy += yinc, _dy += yinc)
				{
					auto s = getPAHelper(spo, 0, _sy);
					auto d = getPAHelper(dpo, 0, _dy);

					for (int x = 0; x < w; x++)
					{
						pxCopyFn(d, s, (dx + x) & 2047, (sx + x) & 2047);
					}
				}
			}
		}
		else
		{
			for (int y = 0; y < h; y++, _sy += yinc, _dy += yinc)
			{
				auto s = getPAHelper(spo, 0, _sy);
				auto d = getPAHelper(dpo, 0, _dy);

				for (int x = 0; x < w; x++)
				{
					pxCopyFn(d, s, (dx - x) & 2047, (sx - x) & 2047);
				}
			}
		}
	};

	auto copy = [=](const GSOffset& dpo, const GSOffset& spo, auto&& pxCopyFn)
	{
		genericCopy(dpo, spo,
			[](const GSOffset& o, int x, int y) { return o.paMulti(x, y); },
			[=](const GSOffset::PAHelper& d, const GSOffset::PAHelper& s, int dx, int sx)
			{
				return pxCopyFn(d.value(dx), s.value(sx));
			});
	};

	auto copyFast = [=](auto* vm, const GSOffset& dpo, const GSOffset& spo, auto&& pxCopyFn)
	{
		genericCopy(dpo, spo,
			[=](const GSOffset& o, int x, int y) { return o.paMulti(vm, x, y); },
			[=](const auto& d, const auto& s, int dx, int sx)
			{
				return pxCopyFn(d.value(dx), s.value(sx));
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
	else if (m_env.BITBLTBUF.SPSM == PSMT8 && m_env.BITBLTBUF.DPSM == PSMT8)
	{
		copyFast(m_mem.m_vm8, GSOffset::fromKnownPSM(dbp, dbw, PSMT8), GSOffset::fromKnownPSM(sbp, sbw, PSMT8), [](u8* d, u8* s)
		{
			*d = *s;
		});
	}
	else if (m_env.BITBLTBUF.SPSM == PSMT4 && m_env.BITBLTBUF.DPSM == PSMT4)
	{
		copy(GSOffset::fromKnownPSM(dbp, dbw, PSMT4), GSOffset::fromKnownPSM(sbp, sbw, PSMT4), [&](u32 doff, u32 soff)
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
	Flush(GSFlushReason::DOWNLOADFIFO);

	size *= 16;

	Read(mem, size);

	if (m_dump)
		m_dump->ReadFIFO(size / 16);
}

void GSState::ReadLocalMemoryUnsync(u8* mem, int qwc, GIFRegBITBLTBUF BITBLTBUF, GIFRegTRXPOS TRXPOS, GIFRegTRXREG TRXREG)
{
	const int w = TRXREG.RRW;
	const int h = TRXREG.RRH;

	const u16 bpp = GSLocalMemory::m_psm[BITBLTBUF.SPSM].trbpp;

	GSTransferBuffer tb;

	if(m_tr.end >= m_tr.total || m_tr.write == true)
		tb.Init(TRXPOS.SSAX, TRXPOS.SSAY, BITBLTBUF, false);

	int len = qwc * 16;
	if (!tb.Update(w, h, bpp, len))
		return;

	if (m_tr.start == 0)
	{
		m_mem.ReadImageX(tb.x, tb.y, m_tr.buff, m_tr.total, BITBLTBUF, TRXPOS, TRXREG);
		m_tr.start += m_tr.total;
	}

	if ((m_tr.end + len) > m_mem.m_vmsize)
	{
		const int masked_end = m_tr.end & 0x3FFFFF; // 4mb.
		const int first_transfer = m_mem.m_vmsize - masked_end;
		const int second_transfer = len - first_transfer;
		memcpy(mem, &m_tr.buff[masked_end], first_transfer);
		memcpy(&mem[first_transfer], &m_tr.buff, second_transfer);
		m_tr.end += len;
	}
	else
	{
		memcpy(mem, &m_tr.buff[m_tr.end], len);
		m_tr.end += len;
	}
}

void GSState::PurgePool()
{
	g_gs_device->PurgePool();
}

void GSState::PurgeTextureCache()
{
}

void GSState::ReadbackTextureCache()
{
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
					const int len = (int)std::min(size, path.nloop);

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
		fd->size = GetSaveStateSize();
		return 0;
	}

	if (!fd->data || fd->size < GetSaveStateSize())
		return -1;

	Flush(GSFlushReason::SAVESTATE);

	if (GSConfig.UserHacks_ReadTCOnClose)
		ReadbackTextureCache();

	u8* data = fd->data;
	const u32 version = STATE_VERSION;

	WriteState(data, &version);
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

	if (fd->size < GetSaveStateSize())
		return -1;

	u8* data = fd->data;

	u32 version;

	ReadState(&version, data);

	if (version > STATE_VERSION)
	{
		Console.Error("GS: Savestate version is incompatible.  Load aborted.");
		return -1;
	}

	Flush(GSFlushReason::LOADSTATE);

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

	for (u32 i = 0; i < 2; i++)
	{
		m_env.CTXT[i].UpdateScissor();

		m_env.CTXT[i].offset.fb = m_mem.GetOffset(m_env.CTXT[i].FRAME.Block(), m_env.CTXT[i].FRAME.FBW, m_env.CTXT[i].FRAME.PSM);
		m_env.CTXT[i].offset.zb = m_mem.GetOffset(m_env.CTXT[i].ZBUF.Block(), m_env.CTXT[i].FRAME.FBW, m_env.CTXT[i].ZBUF.PSM);
		m_env.CTXT[i].offset.fzb4 = m_mem.GetPixelOffset4(m_env.CTXT[i].FRAME, m_env.CTXT[i].ZBUF);
	}

	UpdateScissor();

	g_perfmon.SetFrame(5000);

	ResetPCRTC();

	return 0;
}

void GSState::SetGameCRC(u32 crc)
{
	m_crc = crc;
	UpdateCRCHacks();
}

void GSState::UpdateCRCHacks()
{
	m_game = CRC::Lookup(GSConfig.UserHacks_DisableRenderFixes ? 0 : m_crc);
}

//

void GSState::UpdateContext()
{
	const bool ctx_switch = (m_context != &m_draw_env->CTXT[PRIM->CTXT]);

	if (ctx_switch)
		GL_REG("Context Switch %d", PRIM->CTXT);

	m_context = const_cast<GSDrawingContext*>(&m_draw_env->CTXT[PRIM->CTXT]);

	UpdateScissor();
}

void GSState::UpdateScissor()
{
	m_scissor_cull_min = m_context->scissor.cull.xyxy();
	m_scissor_cull_max = m_context->scissor.cull.zwzw();
	m_xyof = m_context->scissor.xyof;
	m_scissor_invalid = !m_context->scissor.in.gt32(m_context->scissor.in.zwzw()).allfalse();
}

void GSState::UpdateVertexKick()
{
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
	const u32 maxcount = std::max<u32>(m_vertex.maxcount * 3 / 2, 10000);

	GSVertex* vertex = static_cast<GSVertex*>(_aligned_malloc(sizeof(GSVertex) * maxcount, 32));
	// Worst case index list is a list of points with vs expansion, 6 indices per point
	u16* index = static_cast<u16*>(_aligned_malloc(sizeof(u16) * maxcount * 6, 32));

	if (!vertex || !index)
	{
		const u32 vert_byte_count = sizeof(GSVertex) * maxcount;
		const u32 idx_byte_count = sizeof(u16) * maxcount * 3;

		Console.Error("GS: failed to allocate %zu bytes for vertices and %zu for indices.",
			vert_byte_count, idx_byte_count);

		throw GSError();
	}

	if (m_vertex.buff)
	{
		std::memcpy(vertex, m_vertex.buff, sizeof(GSVertex) * m_vertex.tail);

		_aligned_free(m_vertex.buff);
	}

	if (m_index.buff)
	{
		std::memcpy(index, m_index.buff, sizeof(u16) * m_index.tail);

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
	const u32 count = m_vertex.next;
	PRIM_OVERLAP overlap = PRIM_OVERLAP_NO;
	const GSVertex* v = m_vertex.buff;

	m_drawlist.clear();
	u32 i = 0;
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

		u32 j = i + 2;
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
		for (u32 i = 0; i < count; i += 2) {
			if (v[i + 1].XYZ.Y > v[i].XYZ.Y) {
				return PRIM_OVERLAP_UNKNOW;
			}
			GSVector4i vi(v[i].XYZ.X, v[i + 1].XYZ.Y, v[i + 1].XYZ.X, v[i].XYZ.Y);
			for (u32 j = i + 2; j < count; j += 2) {
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
		for (u32 i = 0; i < count; i += 2) {
			if (v[i + 1].XYZ.Y < v[i].XYZ.Y) {
				return PRIM_OVERLAP_UNKNOW;
			}
			GSVector4i vi(v[i].XYZ.X, v[i].XYZ.Y, v[i + 1].XYZ.X, v[i + 1].XYZ.Y);
			for (u32 j = i + 2; j < count; j += 2) {
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

__forceinline bool GSState::IsAutoFlushDraw(u32 prim)
{
	if (!PRIM->TME || (GSConfig.UserHacks_AutoFlush == GSHWAutoFlushLevel::SpritesOnly && prim != GS_SPRITE))
		return false;

	const u32 frame_mask = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmsk;
	const bool frame_hit = m_context->FRAME.Block() == m_context->TEX0.TBP0 && !(m_context->TEST.ATE && m_context->TEST.ATST == 0 && m_context->TEST.AFAIL == 2) && ((m_context->FRAME.FBMSK & frame_mask) != frame_mask);
	// There's a strange behaviour we need to test on a PS2 here, if the FRAME is a Z format, like Powerdrome something swaps over, and it seems Alpha Fail of "FB Only" writes to the Z.. it's odd.
	const bool zbuf_hit = (m_context->ZBUF.Block() == m_context->TEX0.TBP0) && !(m_context->TEST.ATE && m_context->TEST.ATST == 0 && m_context->TEST.AFAIL != 2) && !m_context->ZBUF.ZMSK;
	const u32 frame_z_psm = frame_hit ? m_context->FRAME.PSM : m_context->ZBUF.PSM;
	const u32 frame_z_bp = frame_hit ? m_context->FRAME.Block() : m_context->ZBUF.Block();

	if ((frame_hit || zbuf_hit) && GSUtil::HasSharedBits(frame_z_bp, frame_z_psm, m_context->TEX0.TBP0, m_context->TEX0.PSM))
		return true;
	
	return false;
}

__forceinline void GSState::CLUTAutoFlush(u32 prim)
{
	if (m_mem.m_clut.IsInvalid() & 2)
		return;

	u32 n = 1;

	switch (prim)
	{
		case GS_POINTLIST:
			n = 1;
			break;
		case GS_LINELIST:
		case GS_LINESTRIP:
		case GS_SPRITE:
			n = 2;
			break;
		case GS_TRIANGLELIST:
		case GS_TRIANGLESTRIP:
			n = 3;
			break;
		case GS_TRIANGLEFAN:
			n = 3;
			break;
		case GS_INVALID:
		default:
			break;
	}

	if ((m_index.tail > 0 || (m_vertex.tail == n - 1)) && (GSLocalMemory::m_psm[m_context->TEX0.PSM].pal == 0 || !PRIM->TME))
	{
		const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[m_context->FRAME.PSM];

		if ((m_context->FRAME.FBMSK & psm.fmsk) != psm.fmsk && GSLocalMemory::m_psm[m_mem.m_clut.GetCLUTCPSM()].bpp == psm.bpp)
		{
			const u32 startbp = psm.info.bn(temp_draw_rect.x, temp_draw_rect.y, m_context->FRAME.Block(), m_context->FRAME.FBW);

			// If it's a point, then we only have one coord, so the address for start and end will be the same, which is bad for the following check.
			u32 endbp = startbp;
			// otherwise calculate the end.
			if (prim != GS_POINTLIST || (m_index.tail > 1))
				endbp = psm.info.bn(temp_draw_rect.z - 1, temp_draw_rect.w - 1, m_context->FRAME.Block(), m_context->FRAME.FBW);

			m_mem.m_clut.InvalidateRange(startbp, endbp, true);
		}
	}
}

template<u32 prim, bool index_swap>
__forceinline void GSState::HandleAutoFlush()
{
	// Kind of a cheat, making the assumption that 2 consecutive fan/strip triangles won't overlap each other (*should* be safe)
	if ((m_index.tail & 1) && (prim == GS_TRIANGLESTRIP || prim == GS_TRIANGLEFAN) && !m_texflush_flag)
		return;

	// To briefly explain what's going on here, what we are checking for is draws over a texture when the source and destination are themselves.
	// Because one page of the texture gets buffered in the Texture Cache (the PS2's one) if any of those pixels are overwritten, you still read the old data.
	// So we need to calculate if a page boundary is being crossed for the format it is in and if the same part of the texture being written and read inside the draw.
	if (IsAutoFlushDraw(prim))
	{
		int  n = 1;
		u32 buff[3];
		const u32 head = m_vertex.head;
		const u32 tail = m_vertex.tail;

		switch (prim)
		{
			case GS_POINTLIST:
				buff[0] = tail - 1;
				n = 1;
				break;
			case GS_LINELIST:
			case GS_LINESTRIP:
			case GS_SPRITE:
				buff[0] = tail - 1;
				n = 2;
				break;
			case GS_TRIANGLELIST:
			case GS_TRIANGLESTRIP:
				buff[0] = tail - 2;
				buff[1] = tail - 1;
				n = 3;
				break;
			case GS_TRIANGLEFAN:
				buff[0] = head;
				buff[1] = tail - 1;
				n = 3;
				break;
			case GS_INVALID:
			default:
				break;
		}

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
			const float s = std::min((m_v.ST.S / m_v.RGBAQ.Q), 1.0f);
			const float t = std::min((m_v.ST.T / m_v.RGBAQ.Q), 1.0f);

			tex_coord.x = (int)((1 << m_context->TEX0.TW) * s);
			tex_coord.y = (int)((1 << m_context->TEX0.TH) * t);
		}

		GSVector4i tex_rect = tex_coord.xyxy();

		// Get the rest of the rect.
		for (int i = 0; i < (n - 1); i++)
		{
			const GSVertex* v = &m_vertex.buff[buff[i]];

			if (PRIM->FST)
			{
				tex_coord.x = v->U >> 4;
				tex_coord.y = v->V >> 4;
			}
			else
			{
				const float s = std::min((v->ST.S / v->RGBAQ.Q), 1.0f);
				const float t = std::min((v->ST.T / v->RGBAQ.Q), 1.0f);

				tex_coord.x = (int)((1 << m_context->TEX0.TW) * s);
				tex_coord.y = (int)((1 << m_context->TEX0.TH) * t);
			}

			tex_rect.x = std::min(tex_rect.x, tex_coord.x);
			tex_rect.z = std::max(tex_rect.z, tex_coord.x);
			tex_rect.y = std::min(tex_rect.y, tex_coord.y);
			tex_rect.w = std::max(tex_rect.w, tex_coord.y);
		}

		// Get the last texture position from the last draw.
		const GSVertex* v = &m_vertex.buff[m_index.buff[m_index.tail - (index_swap ? n : 1)]];

		if (PRIM->FST)
		{
			tex_coord.x = v->U >> 4;
			tex_coord.y = v->V >> 4;
		}
		else
		{
			const float s = std::min((v->ST.S / v->RGBAQ.Q), 1.0f);
			const float t = std::min((v->ST.T / v->RGBAQ.Q), 1.0f);

			tex_coord.x = (int)((1 << m_context->TEX0.TW) * s);
			tex_coord.y = (int)((1 << m_context->TEX0.TH) * t);
		}

		const GSVector4i last_tex_page = tex_coord.xyxy() & page_mask;
		const GSVector4i tex_page = tex_rect.xyxy() & page_mask;

		// Crossed page since last draw end
		if (!tex_page.eq(last_tex_page) || m_texflush_flag)
		{
			const u32 frame_mask = GSLocalMemory::m_psm[m_context->TEX0.PSM].fmsk;
			const bool frame_hit = (m_context->FRAME.Block() == m_context->TEX0.TBP0) && !(m_context->TEST.ATE && m_context->TEST.ATST == 0 && m_context->TEST.AFAIL == 2) && ((m_context->FRAME.FBMSK & frame_mask) != frame_mask);
			const u32 frame_z_psm = frame_hit ? m_context->FRAME.PSM : m_context->ZBUF.PSM;
			// Make sure the format matches, otherwise the coordinates aren't gonna match, so the draws won't intersect.
			if (GSUtil::HasCompatibleBits(m_context->TEX0.PSM, frame_z_psm) && (m_context->FRAME.FBW == m_context->TEX0.TBW))
			{
				// If the draw was 1 line thick, make it larger as rects are exclusive of ends.
				if (tex_rect.x == tex_rect.z)
					tex_rect += GSVector4i(0, 0, 1, 0);
				if (tex_rect.y == tex_rect.w)
					tex_rect += GSVector4i(0, 0, 0, 1);

				const GSVector2i offset = GSVector2i(m_context->XYOFFSET.OFX, m_context->XYOFFSET.OFY);
				const GSVector4i scissor = m_context->scissor.in;
				GSVector4i old_tex_rect = GSVector4i::zero();
				int current_draw_end = m_index.tail;

				while (current_draw_end >= n)
				{
					for (int i = current_draw_end - 1; i >= current_draw_end - n; i--)
					{
						const GSVertex* v = &m_vertex.buff[m_index.buff[i]];

						tex_coord.x = (static_cast<int>(v->XYZ.X) - offset.x) >> 4;
						tex_coord.y = (static_cast<int>(v->XYZ.Y) - offset.y) >> 4;

						if (i == (current_draw_end - 1))
						{
							old_tex_rect = tex_coord.xyxy();
						}
						else
						{
							old_tex_rect.x = std::min(old_tex_rect.x, tex_coord.x);
							old_tex_rect.z = std::max(old_tex_rect.z, tex_coord.x);
							old_tex_rect.y = std::min(old_tex_rect.y, tex_coord.y);
							old_tex_rect.w = std::max(old_tex_rect.w, tex_coord.y);
						}
					}

					if (old_tex_rect.x == old_tex_rect.z)
						old_tex_rect += GSVector4i(0, 0, 1, 0);
					if (old_tex_rect.y == old_tex_rect.w)
						old_tex_rect += GSVector4i(0, 0, 0, 1);

					old_tex_rect = tex_rect.rintersect(old_tex_rect);
					if (!old_tex_rect.rintersect(scissor).rempty())
					{
						Flush(GSFlushReason::AUTOFLUSH);
						return;
					}

					current_draw_end -= n;
				}
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

					GSVector4i area_out = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(m_context->scissor.in);
					area_out = area_out & page_mask;
					area_out += GSVector4i(0, 0, 1, 1); // Intersect goes on space inside the rect
					area_out.z += GSLocalMemory::m_psm[m_context->TEX0.PSM].pgs.x;
					area_out.w += GSLocalMemory::m_psm[m_context->TEX0.PSM].pgs.y;

					if (!area_out.rintersect(tex_rect).rempty())
						Flush(GSFlushReason::AUTOFLUSH);
				}
				else // Page width is different, so it's much more difficult to calculate where it's modifying.
					Flush(GSFlushReason::AUTOFLUSH);
			}
		}
	}
}

static constexpr u32 NumIndicesForPrim(u32 prim)
{
	switch (prim)
	{
		case GS_POINTLIST:
		case GS_INVALID:
			return 1;
		case GS_LINELIST:
		case GS_SPRITE:
		case GS_LINESTRIP:
			return 2;
		case GS_TRIANGLELIST:
		case GS_TRIANGLESTRIP:
		case GS_TRIANGLEFAN:
			return 3;
		default:
			return 0;
	}
}

static constexpr u32 MaxVerticesForPrim(u32 prim)
{
	switch (prim)
	{
			// Four indices per 1 vertex.
		case GS_POINTLIST:
		case GS_INVALID:

			// Indices are shifted left by 2 to form quads.
		case GS_LINELIST:
		case GS_LINESTRIP:
			return (std::numeric_limits<u16>::max() / 4) - 4;

			// Four indices per two vertices.
		case GS_SPRITE:
			return (std::numeric_limits<u16>::max() / 2) - 2;

		case GS_TRIANGLELIST:
		case GS_TRIANGLESTRIP:
		case GS_TRIANGLEFAN:
		default:
			return (std::numeric_limits<u16>::max() - 3);
	}
}

template <u32 prim, bool auto_flush, bool index_swap>
__forceinline void GSState::VertexKick(u32 skip)
{
	constexpr u32 n = NumIndicesForPrim(prim);
	static_assert(n > 0);

	ASSERT(m_vertex.tail < m_vertex.maxcount + 3);

	if (auto_flush && skip == 0 && m_index.tail > 0 && ((m_vertex.tail + 1) - m_vertex.head) >= n)
	{
		HandleAutoFlush<prim, index_swap>();
	}

	u32 head = m_vertex.head;
	u32 tail = m_vertex.tail;
	u32 next = m_vertex.next;
	u32 xy_tail = m_vertex.xy_tail;

	// callers should write XYZUVF to m_v.m[1] in one piece to have this load store-forwarded, either by the cpu or the compiler when this function is inlined

	const GSVector4i new_v0(m_v.m[0]);
	const GSVector4i new_v1(m_v.m[1]);

	GSVector4i* RESTRICT tailptr = (GSVector4i*)&m_vertex.buff[tail];

	tailptr[0] = new_v0;
	tailptr[1] = new_v1;

	// We maintain the X/Y coordinates for the last 4 vertices, as well as the head for triangle fans, so we can compute
	// the min/max, and cull degenerate triangles, which saves draws in some cases. Why 4? Mod 4 is cheaper than Mod 3.
	// These vertices are a full vector containing <X_Fixed_Point, Y_Fixed_Point, X_Integer, Y_Integer>. We use the
	// integer coordinates for culling at native resolution, and the fixed point for all others. The XY offset has to be
	// applied, then we split it into the fixed/integer portions.
	const GSVector4i xy_ofs = new_v1.xxxx().u16to32().sub32(m_xyof);
	const GSVector4i xy = xy_ofs.blend32<12>(xy_ofs.sra32(4));
	m_vertex.xy[xy_tail & 3] = xy;

	// Backup head for triangle fans so we can read it later, otherwise it'll get lost after the 4th vertex.
	if (prim == GS_TRIANGLEFAN && tail == head)
		m_vertex.xyhead = xy;

	m_vertex.tail = ++tail;
	m_vertex.xy_tail = ++xy_tail;

	const u32 m = tail - head;

	if (m < n)
		return;


	// Skip draws when scissor is out of range (i.e. bottom-right is less than top-left), since everything will get clipped.
	skip |= static_cast<u32>(m_scissor_invalid);

	GSVector4i pmin, pmax;
	if (skip == 0)
	{
		const GSVector4i v0 = m_vertex.xy[(xy_tail - 1) & 3];
		const GSVector4i v1 = m_vertex.xy[(xy_tail - 2) & 3];
		const GSVector4i v2 = (prim == GS_TRIANGLEFAN) ? m_vertex.xyhead : m_vertex.xy[(xy_tail - 3) & 3];

		switch (prim)
		{
			case GS_POINTLIST:
				pmin = v0;
				pmax = v0;
				break;
			case GS_LINELIST:
			case GS_LINESTRIP:
			case GS_SPRITE:
				pmin = v0.min_i32(v1);
				pmax = v0.max_i32(v1);
				break;
			case GS_TRIANGLELIST:
			case GS_TRIANGLESTRIP:
			case GS_TRIANGLEFAN:
				pmin = v0.min_i32(v1.min_i32(v2));
				pmax = v0.max_i32(v1.max_i32(v2));
				break;
			default:
				break;
		}

		GSVector4i test = pmax.lt32(m_scissor_cull_min) | pmin.gt32(m_scissor_cull_max);

		switch (prim)
		{
			case GS_TRIANGLELIST:
			case GS_TRIANGLESTRIP:
			case GS_TRIANGLEFAN:
			case GS_SPRITE:
			{
				// Discard degenerate triangles which don't cover at least one pixel. Since the vertices are in native
				// resolution space, we can use the integer locations. When upscaling, we can't, because a primitive which
				// does not span a single pixel at 1x may span multiple pixels at higher resolutions.
				const GSVector4i degen_test = pmin.eq32(pmax);
				test |= m_nativeres ? degen_test.zwzw() : degen_test;
			}
			break;
			default:
				break;
		}

		switch (prim)
		{
			case GS_TRIANGLELIST:
			case GS_TRIANGLESTRIP:
			case GS_TRIANGLEFAN:
				test = (test | v0.eq64(v1)) | (v1.eq64(v2) | v0.eq64(v2));
				break;
			default:
				break;
		}

		// We only care about the xy passing the skip test. zw is the offset coordinates for native culling.
		skip |= test.mask() & 0xff;
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

	if (m_index.tail == 0 && ((m_backed_up_ctx != m_env.PRIM.CTXT) || m_dirty_gs_regs))
	{
		const int ctx = m_env.PRIM.CTXT;
		std::memcpy(&m_prev_env, &m_env, 88);
		std::memcpy(&m_prev_env.CTXT[ctx], &m_env.CTXT[ctx], 96);
		std::memcpy(&m_prev_env.CTXT[ctx].offset, &m_env.CTXT[ctx].offset, sizeof(m_env.CTXT[ctx].offset));
		std::memcpy(&m_prev_env.CTXT[ctx].scissor, &m_env.CTXT[ctx].scissor, sizeof(m_env.CTXT[ctx].scissor));
		m_dirty_gs_regs = 0;
		m_backed_up_ctx = m_env.PRIM.CTXT;
	}

	u16* RESTRICT buff = &m_index.buff[m_index.tail];

	switch (prim)
	{
		case GS_POINTLIST:
			buff[0] = static_cast<u16>(head + 0);
			m_vertex.head = head + 1;
			m_vertex.next = head + 1;
			m_index.tail += 1;
			break;
		case GS_LINELIST:
			buff[0] = static_cast<u16>(head + (index_swap ? 1 : 0));
			buff[1] = static_cast<u16>(head + (index_swap ? 0 : 1));
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
			buff[0] = static_cast<u16>(head + (index_swap ? 1 : 0));
			buff[1] = static_cast<u16>(head + (index_swap ? 0 : 1));
			m_vertex.head = head + 1;
			m_vertex.next = head + 2;
			m_index.tail += 2;
			break;
		case GS_TRIANGLELIST:
			buff[0] = static_cast<u16>(head + (index_swap ? 2 : 0));
			buff[1] = static_cast<u16>(head + 1);
			buff[2] = static_cast<u16>(head + (index_swap ? 0 : 2));
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
			buff[0] = static_cast<u16>(head + (index_swap ? 2 : 0));
			buff[1] = static_cast<u16>(head + 1);
			buff[2] = static_cast<u16>(head + (index_swap ? 0 : 2));
			m_vertex.head = head + 1;
			m_vertex.next = head + 3;
			m_index.tail += 3;
			break;
		case GS_TRIANGLEFAN:
			// TODO: remove gaps, next == head && head < tail - 3 || next > head && next < tail - 2 (very rare)
			buff[0] = static_cast<u16>(index_swap ? (tail - 1) : (head + 0));
			buff[1] = static_cast<u16>(tail - 2);
			buff[2] = static_cast<u16>(index_swap ? (head + 0) : (tail - 1));
			m_vertex.next = tail;
			m_index.tail += 3;
			break;
		case GS_SPRITE:
			buff[0] = static_cast<u16>(head + 0);
			buff[1] = static_cast<u16>(head + 1);
			m_vertex.head = head + 2;
			m_vertex.next = head + 2;
			m_index.tail += 2;
			break;
		case GS_INVALID:
			m_vertex.tail = head;
			return;
		default:
			__assume(0);
	}

	// Update rectangle for the current draw. We can use the re-integer coordinates from min/max here.
	const GSVector4i draw_min = pmin.zwzw();
	const GSVector4i draw_max = pmax;
	if (m_vertex.tail != n)
		temp_draw_rect = temp_draw_rect.min_i32(draw_min).blend32<12>(temp_draw_rect.max_i32(draw_max));
	else
		temp_draw_rect = draw_min.blend32<12>(draw_max);
	temp_draw_rect = temp_draw_rect.rintersect(m_context->scissor.in);

	CLUTAutoFlush(prim);

	constexpr u32 max_vertices = MaxVerticesForPrim(prim);
	if (max_vertices != 0 && m_vertex.tail >= max_vertices)
		Flush(VERTEXCOUNT);
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

GSState::TextureMinMaxResult GSState::GetTextureMinMax(GIFRegTEX0 TEX0, GIFRegCLAMP CLAMP, bool linear, bool clamp_to_tsize)
{
	// TODO: some of the +1s can be removed if linear == false

	const int tw = TEX0.TW;
	const int th = TEX0.TH;

	const int w = 1 << tw;
	const int h = 1 << th;
	const int tw_mask = (1 << tw) - 1;
	const int th_mask = (1 << th) - 1;

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
			vr.x = minu;
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
			vr.y = minv;
			vr.w = maxv + 1;
			break;
		case CLAMP_REGION_REPEAT:
			vr.y = maxv;
			vr.w = (maxv | minv) + 1;
			break;
		default:
			__assume(0);
	}

	// Software renderer fixes TEX0 so that TW/TH contain MAXU/MAXV.
	// Hardware renderer doesn't, and handles it in the texture cache, so don't clamp here.
	if (clamp_to_tsize)
		vr = vr.rintersect(tr);
	else
		tr = tr.runion(vr);

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
		{
			st += GSVector4(-0.5f, 0.5f).xxyy();
			
			// If it's the start of the texture and our little adjustment is all that pushed it over, clamp it to 0.
			// This stops the border check failing when using repeat but needed less than the full texture
			// since this was making it take the full texture even though it wasn't needed.
			if (((m_vt.m_min.t == GSVector4::zero()).mask() & 0x3) == 0x3)
				st = st.max(GSVector4::zero());
		}
		else
		{
			// When drawing a sprite with point sampling, the UV range sampled is exclusive of the ending
			// coordinate. Except, when the position is also offset backwards. We only do this for the
			// hardware renderers currently, it does create some issues in software. Hardware needs the
			// UVs to be within the target size, otherwise it can't translate sub-targets (see 10 Pin -
			// Champions Alley and Miami Vice).
			if (!clamp_to_tsize && m_vt.m_primclass == GS_SPRITE_CLASS && PRIM->FST == 1)
			{
				const int mask = (m_vt.m_min.p.floor() != m_vt.m_min.p).mask();
				if (!(mask & 0x1))
					st.z = std::max(st.x, st.z - 0.5f);
				if (!(mask & 0x2))
					st.w = std::max(st.y, st.w - 0.5f);
			}
		}

		// Adjust texture range when sprites get scissor clipped. Since we linearly interpolate, this
		// optimization doesn't work when perspective correction is enabled.
		if (m_vt.m_primclass == GS_SPRITE_CLASS && PRIM->FST == 1 && m_index.tail < 3)
		{
			// When coordinates are fractional, GS appears to draw to the right/bottom (effectively
			// taking the ceiling), not to the top/left (taking the floor).
			const GSVector4i int_rc(m_vt.m_min.p.ceil().xyxy(m_vt.m_max.p.floor()));
			const GSVector4i scissored_rc(int_rc.rintersect(m_context->scissor.in));
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

		const GSVector4i uv = GSVector4i(st.floor());
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

	int min = 0, max = 0;

	if (IsCoverageAlpha())
	{
		min = 128;
		max = 128;
	}
	else
	{
		const GSDrawingContext* context = m_context;
		GSVector4i a = m_vt.m_min.c.uph32(m_vt.m_max.c).zzww();
		if (PRIM->TME && context->TEX0.TCC)
		{
			const GSDrawingEnvironment& env = *m_draw_env;

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
		min = a.x;
		max = a.z;
	}

	m_vt.m_alpha.min = min;
	m_vt.m_alpha.max = max;
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
			if (context->FRAME.PSM == PSMCT24 || context->FRAME.PSM == PSMZ24)
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

bool GSState::IsCoverageAlpha()
{
	return !PRIM->ABE && PRIM->AA1 && (m_vt.m_primclass == GS_LINE_CLASS || m_vt.m_primclass == GS_TRIANGLE_CLASS);
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
	constexpr size_t alloc_size = 1024 * 1024 * 4;
	buff = reinterpret_cast<u8*>(_aligned_malloc(alloc_size, 32));
}

GSState::GSTransferBuffer::~GSTransferBuffer()
{
	_aligned_free(buff);
}

void GSState::GSTransferBuffer::Init(int tx, int ty, const GIFRegBITBLTBUF& blit, bool is_write)
{
	x = tx;
	y = ty;
	total = 0;
	start = 0;
	end = 0;
	m_blit = blit;
	write = is_write;
}

bool GSState::GSTransferBuffer::Update(int tw, int th, int bpp, int& len)
{
	int tex_size = (((tw * th * bpp) + 7) >> 3); // Round to nearest byte
	int packet_size = (tex_size + 15) & ~0xF; // Round up to the nearest quadword

	if (total == 0)
		total = std::min<int>(tex_size, 1024 * 1024 * 4);

	const int remaining = total - end;

	if (len > remaining)
	{
		if (len > packet_size)
		{
#if defined(PCSX2_DEVBUILD) || defined(_DEBUG)
			Console.Warning("GS transfer buffer overflow len %d remaining %d, tex_size %d tw %d th %d bpp %d", len, remaining, tex_size, tw, th, bpp);
#endif
		}

		len = remaining;
	}

	return len > 0;
}
