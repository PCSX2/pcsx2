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
	uint32 FBP;
	uint32 FPSM;
	uint32 FBMSK;
	uint32 TBP0;
	uint32 TPSM;
	uint32 TZTST;
	bool TME;
};

typedef bool (*GetSkipCount)(const GSFrameInfo& fi, int& skip);

class GSState : public GSAlignedClass<32>
{
	// RESTRICT prevents multiple loads of the same part of the register when accessing its bitfields (the compiler is happy to know that memory writes in-between will not go there)

	typedef void (GSState::*GIFPackedRegHandler)(const GIFPackedReg* RESTRICT r);

	GIFPackedRegHandler m_fpGIFPackedRegHandlers[16];
	GIFPackedRegHandler m_fpGIFPackedRegHandlerXYZ[8][4];

	void GIFPackedRegHandlerNull(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerRGBA(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerSTQ(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerUV(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerUV_Hack(const GIFPackedReg* RESTRICT r);
	template<uint32 prim, uint32 adc, bool auto_flush> void GIFPackedRegHandlerXYZF2(const GIFPackedReg* RESTRICT r);
	template<uint32 prim, uint32 adc, bool auto_flush> void GIFPackedRegHandlerXYZ2(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerFOG(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerA_D(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerNOP(const GIFPackedReg* RESTRICT r);

	typedef void (GSState::*GIFRegHandler)(const GIFReg* RESTRICT r);

	GIFRegHandler m_fpGIFRegHandlers[256];
	GIFRegHandler m_fpGIFRegHandlerXYZ[8][4];

	typedef void (GSState::*GIFPackedRegHandlerC)(const GIFPackedReg* RESTRICT r, uint32 size);

	GIFPackedRegHandlerC m_fpGIFPackedRegHandlersC[2];
	GIFPackedRegHandlerC m_fpGIFPackedRegHandlerSTQRGBAXYZF2[8];
	GIFPackedRegHandlerC m_fpGIFPackedRegHandlerSTQRGBAXYZ2[8];

	template<uint32 prim, bool auto_flush> void GIFPackedRegHandlerSTQRGBAXYZF2(const GIFPackedReg* RESTRICT r, uint32 size);
	template<uint32 prim, bool auto_flush> void GIFPackedRegHandlerSTQRGBAXYZ2(const GIFPackedReg* RESTRICT r, uint32 size);
	void GIFPackedRegHandlerNOP(const GIFPackedReg* RESTRICT r, uint32 size);

	template<int i> void ApplyTEX0(GIFRegTEX0& TEX0);
	void ApplyPRIM(uint32 prim);

	void GIFRegHandlerNull(const GIFReg* RESTRICT r);
	void GIFRegHandlerPRIM(const GIFReg* RESTRICT r);
	void GIFRegHandlerRGBAQ(const GIFReg* RESTRICT r);
	void GIFRegHandlerST(const GIFReg* RESTRICT r);
	void GIFRegHandlerUV(const GIFReg* RESTRICT r);
	void GIFRegHandlerUV_Hack(const GIFReg* RESTRICT r);
	template<uint32 prim, uint32 adc, bool auto_flush> void GIFRegHandlerXYZF2(const GIFReg* RESTRICT r);
	template<uint32 prim, uint32 adc, bool auto_flush> void GIFRegHandlerXYZ2(const GIFReg* RESTRICT r);
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
	void GIFRegHandlerSIGNAL(const GIFReg* RESTRICT r);
	void GIFRegHandlerFINISH(const GIFReg* RESTRICT r);
	void GIFRegHandlerLABEL(const GIFReg* RESTRICT r);

	int m_version;
	int m_sssize;

	bool m_mt;
	void (*m_irq)();
	bool m_path3hack;
	bool m_init_read_fifo_supported;

	struct GSTransferBuffer
	{
		int x, y;
		int start, end, total;
		bool overflow;
		uint8* buff;
		GIFRegBITBLTBUF m_blit;

		GSTransferBuffer();
		virtual ~GSTransferBuffer();

		void Init(int tx, int ty, const GIFRegBITBLTBUF& blit);
		bool Update(int tw, int th, int bpp, int& len);

	} m_tr;

protected:
	bool IsBadFrame();
	void SetupCrcHack();

	bool m_userhacks_wildhack;
	bool m_isPackedUV_HackFlag;
	CRCHackLevel m_crc_hack_level;
	GetSkipCount m_gsc;
	int m_skip;
	int m_skip_offset;
	int m_userhacks_skipdraw;
	int m_userhacks_skipdraw_offset;
	bool m_userhacks_auto_flush;

	GSVertex m_v;
	float m_q;
	GSVector4i m_scissor;
	GSVector4i m_ofxy;
	bool m_texflush;

	struct
	{
		GSVertex* buff;
		size_t head, tail, next, maxcount; // head: first vertex, tail: last vertex + 1, next: last indexed + 1
		size_t xy_tail;
		uint64 xy[4];
	} m_vertex;

	struct
	{
		uint32* buff;
		size_t tail;
	} m_index;

	void UpdateContext();
	void UpdateScissor();

	void UpdateVertexKick();

	void GrowVertexBuffer();

	template <uint32 prim, bool auto_flush>
	void VertexKick(uint32 skip);

	// following functions need m_vt to be initialized

	GSVertexTrace m_vt;

	void GetTextureMinMax(GSVector4i& r, const GIFRegTEX0& TEX0, const GIFRegCLAMP& CLAMP, bool linear);
	void GetAlphaMinMax();
	bool TryAlphaTest(uint32& fm, uint32& zm);
	bool IsOpaque();
	bool IsMipMapDraw();
	bool IsMipMapActive();
	GIFRegTEX0 GetTex0Layer(uint32 lod);

public:
	GIFPath m_path[4];
	GIFRegPRIM* PRIM;
	GSPrivRegSet* m_regs;
	GSLocalMemory m_mem;
	GSDrawingEnvironment m_env;
	GSDrawingContext* m_context;
	GSPerfMon m_perfmon;
	uint32 m_crc;
	CRC::Game m_game;
	std::unique_ptr<GSDumpBase> m_dump;
	int m_options;
	int m_frameskip;
	bool m_NTSC_Saturation;
	bool m_nativeres;
	int m_mipmap;

	static int s_n;
	bool s_dump;
	bool s_save;
	bool s_savet;
	bool s_savez;
	bool s_savef;
	int s_saven;
	int s_savel;
	std::string m_dump_root;

public:
	GSState();
	virtual ~GSState();

	void ResetHandlers();

	int GetFramebufferHeight();
	void SaturateOutputSize(GSVector4i& r);
	GSVector4i GetDisplayRect(int i = -1);
	GSVector4i GetFrameRect(int i = -1);
	GSVideoMode GetVideoMode();

	bool IsEnabled(int i);
	bool isinterlaced();

	float GetTvRefreshRate();

	virtual void Reset();
	void Flush();
	void FlushPrim();
	void FlushWrite();
	virtual void Draw() = 0;
	virtual void PurgePool() = 0;
	virtual void InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r) {}
	virtual void InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut = false) {}

	void Move();
	void Write(const uint8* mem, int len);
	void Read(uint8* mem, int len);
	void InitReadFIFO(uint8* mem, int len);

	void SoftReset(uint32 mask);
	void WriteCSR(uint32 csr) { m_regs->CSR.u32[1] = csr; }
	void ReadFIFO(uint8* mem, int size);
	template<int index> void Transfer(const uint8* mem, uint32 size);
	int Freeze(freezeData* fd, bool sizeonly);
	int Defrost(const freezeData* fd);
	void GetLastTag(uint32* tag)
	{
		*tag = m_path3hack;
		m_path3hack = 0;
	}
	virtual void SetGameCRC(uint32 crc, int options);
	void SetFrameSkip(int skip);
	void SetRegsMem(uint8* basemem);
	void SetIrqCallback(void (*irq)());
	void SetMultithreaded(bool mt = true);
};
