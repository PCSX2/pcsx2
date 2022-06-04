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

#include "GS.h"
#include "GSLocalMemory.h"
#include "GSDrawingContext.h"
#include "GSDrawingEnvironment.h"
#include "Renderers/Common/GSVertex.h"
#include "Renderers/Common/GSVertexTrace.h"
#include "GSUtil.h"
#include "GSPerfMon.h"
#include "GSVector.h"
#include "Renderers/Common/GSDevice.h"
#include "GSCrc.h"
#include "GSAlignedClass.h"
#include "GSDump.h"

struct GSFrameInfo
{
	u32 FBP;
	u32 FPSM;
	u32 FBMSK;
	u32 TBP0;
	u32 TPSM;
	u32 TZTST;
	bool TME;
};

typedef bool (*GetSkipCount)(const GSFrameInfo& fi, int& skip);

class GSState : public GSAlignedClass<32>
{
	// RESTRICT prevents multiple loads of the same part of the register when accessing its bitfields (the compiler is happy to know that memory writes in-between will not go there)

	typedef void (GSState::*GIFPackedRegHandler)(const GIFPackedReg* RESTRICT r);

	GIFPackedRegHandler m_fpGIFPackedRegHandlers[16];
	GIFPackedRegHandler m_fpGIFPackedRegHandlerXYZ[8][4];

	void CheckFlushes();

	void GIFPackedRegHandlerNull(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerRGBA(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerSTQ(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerUV(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerUV_Hack(const GIFPackedReg* RESTRICT r);
	template<u32 prim, u32 adc, bool auto_flush, bool index_swap> void GIFPackedRegHandlerXYZF2(const GIFPackedReg* RESTRICT r);
	template<u32 prim, u32 adc, bool auto_flush, bool index_swap> void GIFPackedRegHandlerXYZ2(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerFOG(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerA_D(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerNOP(const GIFPackedReg* RESTRICT r);

	typedef void (GSState::*GIFRegHandler)(const GIFReg* RESTRICT r);

	GIFRegHandler m_fpGIFRegHandlers[256];
	GIFRegHandler m_fpGIFRegHandlerXYZ[8][4];

	typedef void (GSState::*GIFPackedRegHandlerC)(const GIFPackedReg* RESTRICT r, u32 size);

	GIFPackedRegHandlerC m_fpGIFPackedRegHandlersC[2];
	GIFPackedRegHandlerC m_fpGIFPackedRegHandlerSTQRGBAXYZF2[8];
	GIFPackedRegHandlerC m_fpGIFPackedRegHandlerSTQRGBAXYZ2[8];

	template<u32 prim, bool auto_flush, bool index_swap> void GIFPackedRegHandlerSTQRGBAXYZF2(const GIFPackedReg* RESTRICT r, u32 size);
	template<u32 prim, bool auto_flush, bool index_swap> void GIFPackedRegHandlerSTQRGBAXYZ2(const GIFPackedReg* RESTRICT r, u32 size);
	void GIFPackedRegHandlerNOP(const GIFPackedReg* RESTRICT r, u32 size);

	template<int i> void ApplyTEX0(GIFRegTEX0& TEX0);
	void ApplyPRIM(u32 prim);

	void GIFRegHandlerNull(const GIFReg* RESTRICT r);
	void GIFRegHandlerPRIM(const GIFReg* RESTRICT r);
	void GIFRegHandlerRGBAQ(const GIFReg* RESTRICT r);
	void GIFRegHandlerST(const GIFReg* RESTRICT r);
	void GIFRegHandlerUV(const GIFReg* RESTRICT r);
	void GIFRegHandlerUV_Hack(const GIFReg* RESTRICT r);
	template<u32 prim, u32 adc, bool auto_flush, bool index_swap> void GIFRegHandlerXYZF2(const GIFReg* RESTRICT r);
	template<u32 prim, u32 adc, bool auto_flush, bool index_swap> void GIFRegHandlerXYZ2(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerTEX0(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerCLAMP(const GIFReg* RESTRICT r);
	void GIFRegHandlerFOG(const GIFReg* RESTRICT r);
	void GIFRegHandlerNOP(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerTEX1(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerTEX2(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerXYOFFSET(const GIFReg* RESTRICT r);
	void GIFRegHandlerPRMODECONT(const GIFReg* RESTRICT r);
	void GIFRegHandlerPRMODE(const GIFReg* RESTRICT r);
	void GIFRegHandlerTEXCLUT(const GIFReg* RESTRICT r);
	void GIFRegHandlerSCANMSK(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerMIPTBP1(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerMIPTBP2(const GIFReg* RESTRICT r);
	void GIFRegHandlerTEXA(const GIFReg* RESTRICT r);
	void GIFRegHandlerFOGCOL(const GIFReg* RESTRICT r);
	void GIFRegHandlerTEXFLUSH(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerSCISSOR(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerALPHA(const GIFReg* RESTRICT r);
	void GIFRegHandlerDIMX(const GIFReg* RESTRICT r);
	void GIFRegHandlerDTHE(const GIFReg* RESTRICT r);
	void GIFRegHandlerCOLCLAMP(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerTEST(const GIFReg* RESTRICT r);
	void GIFRegHandlerPABE(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerFBA(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerFRAME(const GIFReg* RESTRICT r);
	template<int i> void GIFRegHandlerZBUF(const GIFReg* RESTRICT r);
	void GIFRegHandlerBITBLTBUF(const GIFReg* RESTRICT r);
	void GIFRegHandlerTRXPOS(const GIFReg* RESTRICT r);
	void GIFRegHandlerTRXREG(const GIFReg* RESTRICT r);
	void GIFRegHandlerTRXDIR(const GIFReg* RESTRICT r);
	void GIFRegHandlerHWREG(const GIFReg* RESTRICT r);

	template<bool auto_flush, bool index_swap>
	void SetPrimHandlers();

	u32 m_version;
	int m_sssize;

	struct GSTransferBuffer
	{
		int x, y;
		int start, end, total;
		bool overflow;
		u8* buff;
		GIFRegBITBLTBUF m_blit;

		GSTransferBuffer();
		virtual ~GSTransferBuffer();

		void Init(int tx, int ty, const GIFRegBITBLTBUF& blit);
		bool Update(int tw, int th, int bpp, int& len);

	} m_tr;

private:
	void CalcAlphaMinMax();

protected:
	bool IsBadFrame();
	void SetupCrcHack();

	bool m_isPackedUV_HackFlag;
	CRCHackLevel m_crc_hack_level;
	GetSkipCount m_gsc;
	int m_skip;
	int m_skip_offset;

	GSVertex m_v;
	float m_q;
	GSVector4i m_scissor;
	GSVector4i m_ofxy;

	bool m_scanmask_used;
	bool tex_flushed;

	struct
	{
		GSVertex* buff;
		size_t head, tail, next, maxcount; // head: first vertex, tail: last vertex + 1, next: last indexed + 1
		size_t xy_tail;
		u64 xy[4];
	} m_vertex;

	struct
	{
		u32* buff;
		size_t tail;
	} m_index;

	void UpdateContext();
	void UpdateScissor();

	void UpdateVertexKick();

	void GrowVertexBuffer();
	void HandleAutoFlush();
	
	template <u32 prim, bool auto_flush, bool index_swap>
	void VertexKick(u32 skip);

	// following functions need m_vt to be initialized

	GSVertexTrace m_vt;
	GSVertexTrace::VertexAlpha& GetAlphaMinMax()
	{
		if (!m_vt.m_alpha.valid)
			CalcAlphaMinMax();
		return m_vt.m_alpha;
	}
	struct TextureMinMaxResult
	{
		enum UsesBoundary
		{
			USES_BOUNDARY_LEFT   = 1 << 0,
			USES_BOUNDARY_TOP    = 1 << 1,
			USES_BOUNDARY_RIGHT  = 1 << 2,
			USES_BOUNDARY_BOTTOM = 1 << 3,
			USES_BOUNDARY_U = USES_BOUNDARY_LEFT | USES_BOUNDARY_RIGHT,
			USES_BOUNDARY_V = USES_BOUNDARY_TOP | USES_BOUNDARY_BOTTOM,
		};
		GSVector4i coverage; ///< Part of the texture used
		u8 uses_boundary;    ///< Whether or not the usage touches the left, top, right, or bottom edge (and therefore needs wrap modes preserved)
	};
	TextureMinMaxResult GetTextureMinMax(const GIFRegTEX0& TEX0, const GIFRegCLAMP& CLAMP, bool linear);
	bool TryAlphaTest(u32& fm, const u32 fm_mask, u32& zm);
	bool IsOpaque();
	bool IsMipMapDraw();
	bool IsMipMapActive();

public:
	GIFPath m_path[4];
	GIFRegPRIM* PRIM;
	GSPrivRegSet* m_regs;
	GSLocalMemory m_mem;
	GSDrawingEnvironment m_env;
	GSDrawingContext* m_context;
	u32 m_crc;
	CRC::Game m_game;
	std::unique_ptr<GSDumpBase> m_dump;
	int m_options;
	int m_frameskip;
	bool m_nativeres;
	bool m_mipmap;
	bool m_primflush;
	GIFRegPRIM m_last_prim;

	static int s_n;
	bool s_dump;
	bool s_save;
	bool s_savet;
	bool s_savez;
	bool s_savef;
	int s_saven;
	int s_savel;
	std::string m_dump_root;

	static constexpr u32 STATE_VERSION = 8;

	enum PRIM_OVERLAP
	{
		PRIM_OVERLAP_UNKNOW,
		PRIM_OVERLAP_YES,
		PRIM_OVERLAP_NO
	};

	PRIM_OVERLAP m_prim_overlap;
	std::vector<size_t> m_drawlist;

	// The horizontal offset values (under z) for PAL and NTSC have been tweaked
	// they should be apparently 632 and 652 respectively, but that causes a thick black line on the left
	// these values leave a small black line on the right in a bunch of games, but it's not so bad.
	// The only conclusion I can come to is there is horizontal overscan expected so there would normally
	// be black borders either side anyway, or both sides slightly covered.
	const GSVector4i VideoModeOffsets[6] = {
		GSVector4i(640, 224, 642, 25),
		GSVector4i(640, 256, 676, 36),
		GSVector4i(640, 480, 276, 34),
		GSVector4i(720, 480, 232, 35),
		GSVector4i(1280, 720, 302, 24),
		GSVector4i(1920, 540, 238, 40)
	};

	const GSVector4i VideoModeDividers[6] = {
		GSVector4i(3, 0, 2559, 239),
		GSVector4i(3, 0, 2559, 287),
		GSVector4i(1, 0, 1279, 479),
		GSVector4i(1, 0, 1439, 479),
		GSVector4i(0, 0, 1279, 719),
		GSVector4i(0, 0, 1919, 1079)
	};

public:
	GSState();
	virtual ~GSState();

	void ResetHandlers();

	int GetFramebufferHeight();
	int GetDisplayHMagnification();
	GSVector4i GetDisplayRect(int i = -1);
	GSVector4i GetFrameMagnifiedRect(int i = -1);
	GSVector2i GetResolutionOffset(int i = -1);
	GSVector2i GetResolution();
	GSVector4i GetFrameRect(int i = -1);
	GSVideoMode GetVideoMode();

	bool IsEnabled(int i);
	bool isinterlaced();
	bool IsAnalogue();

	float GetTvRefreshRate();

	virtual void Reset(bool hardware_reset);
	virtual void UpdateSettings(const Pcsx2Config::GSOptions& old_config);

	void Flush();
	void FlushPrim();
	void FlushWrite();
	virtual void Draw() = 0;
	virtual void PurgePool() = 0;
	virtual void InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r) {}
	virtual void InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut = false) {}

	virtual void Move();

	void Write(const u8* mem, int len);
	void Read(u8* mem, int len);
	void InitReadFIFO(u8* mem, int len);

	void SoftReset(u32 mask);
	void WriteCSR(u32 csr) { m_regs->CSR.U32[1] = csr; }
	void ReadFIFO(u8* mem, int size);
	void ReadLocalMemoryUnsync(u8* mem, int qwc, GIFRegBITBLTBUF BITBLTBUF, GIFRegTRXPOS TRXPOS, GIFRegTRXREG TRXREG);
	template<int index> void Transfer(const u8* mem, u32 size);
	int Freeze(freezeData* fd, bool sizeonly);
	int Defrost(const freezeData* fd);

	u32 GetGameCRC() const { return m_crc; }
	int GetGameCRCOptions() const { return m_options; }
	virtual void SetGameCRC(u32 crc, int options);

	u8* GetRegsMem() const { return reinterpret_cast<u8*>(m_regs); }
	void SetRegsMem(u8* basemem) { m_regs = reinterpret_cast<GSPrivRegSet*>(basemem); }

	void SetFrameSkip(int skip);
	void DumpVertices(const std::string& filename);

	PRIM_OVERLAP PrimitiveOverlap();
	GIFRegTEX0 GetTex0Layer(u32 lod);
};
