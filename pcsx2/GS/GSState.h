// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/GS.h"
#include "GS/GSPerfMon.h"
#include "GS/GSLocalMemory.h"
#include "GS/GSDrawingContext.h"
#include "GS/GSDrawingEnvironment.h"
#include "GS/Renderers/Common/GSVertex.h"
#include "GS/Renderers/Common/GSVertexTrace.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GS/GSVector.h"
#include "GSAlignedClass.h"

class GSDumpBase;

class GSState : public GSAlignedClass<32>
{
public:
	GSState();
	virtual ~GSState();

	static constexpr int GetSaveStateSize(int version);

private:
	// RESTRICT prevents multiple loads of the same part of the register when accessing its bitfields (the compiler is happy to know that memory writes in-between will not go there)

	typedef void (GSState::*GIFPackedRegHandler)(const GIFPackedReg* RESTRICT r);

	GIFPackedRegHandler m_fpGIFPackedRegHandlers[16] = {};
	GIFPackedRegHandler m_fpGIFPackedRegHandlerXYZ[8][4] = {};

	void CheckFlushes();

	void GIFPackedRegHandlerNull(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerRGBA(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerSTQ(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerUV(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerUV_Hack(const GIFPackedReg* RESTRICT r);
	template<u32 prim, u32 adc, bool auto_flush> void GIFPackedRegHandlerXYZF2(const GIFPackedReg* RESTRICT r);
	template<u32 prim, u32 adc, bool auto_flush> void GIFPackedRegHandlerXYZ2(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerFOG(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerA_D(const GIFPackedReg* RESTRICT r);
	void GIFPackedRegHandlerNOP(const GIFPackedReg* RESTRICT r);

	typedef void (GSState::*GIFRegHandler)(const GIFReg* RESTRICT r);

	GIFRegHandler m_fpGIFRegHandlers[256] = {};
	GIFRegHandler m_fpGIFRegHandlerXYZ[8][4] = {};

	typedef void (GSState::*GIFPackedRegHandlerC)(const GIFPackedReg* RESTRICT r, u32 size);

	GIFPackedRegHandlerC m_fpGIFPackedRegHandlersC[2] = {};
	GIFPackedRegHandlerC m_fpGIFPackedRegHandlerSTQRGBAXYZF2[8] = {};
	GIFPackedRegHandlerC m_fpGIFPackedRegHandlerSTQRGBAXYZ2[8] = {};

	template<u32 prim, bool auto_flush> void GIFPackedRegHandlerSTQRGBAXYZF2(const GIFPackedReg* RESTRICT r, u32 size);
	template<u32 prim, bool auto_flush> void GIFPackedRegHandlerSTQRGBAXYZ2(const GIFPackedReg* RESTRICT r, u32 size);
	void GIFPackedRegHandlerNOP(const GIFPackedReg* RESTRICT r, u32 size);

	template<int i> void ApplyTEX0(GIFRegTEX0& TEX0);
	void ApplyPRIM(u32 prim);

	void GIFRegHandlerNull(const GIFReg* RESTRICT r);
	void GIFRegHandlerPRIM(const GIFReg* RESTRICT r);
	void GIFRegHandlerRGBAQ(const GIFReg* RESTRICT r);
	void GIFRegHandlerST(const GIFReg* RESTRICT r);
	void GIFRegHandlerUV(const GIFReg* RESTRICT r);
	void GIFRegHandlerUV_Hack(const GIFReg* RESTRICT r);
	template<u32 prim, u32 adc, bool auto_flush> void GIFRegHandlerXYZF2(const GIFReg* RESTRICT r);
	template<u32 prim, u32 adc, bool auto_flush> void GIFRegHandlerXYZ2(const GIFReg* RESTRICT r);
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

	template<bool auto_flush> void SetPrimHandlers();

	struct GSTransferBuffer
	{
		int x = 0, y = 0;
		int w = 0, h = 0;
		int start = 0, end = 0, total = 0;
		u8* buff = nullptr;
		GSVector4i rect = GSVector4i::zero();
		GIFRegBITBLTBUF m_blit = {};
		GIFRegTRXPOS m_pos = {};
		GIFRegTRXREG m_reg = {};
		bool write = false;

		GSTransferBuffer();
		~GSTransferBuffer();

		void Init(GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG, const GIFRegBITBLTBUF& blit, bool is_write);
		bool Update(int tw, int th, int bpp, int& len);

	} m_tr;

protected:
	static constexpr int INVALID_ALPHA_MINMAX = 500;

	GSVertex m_v = {};
	float m_q = 1.0f;
	GSVector4i m_scissor_cull_min = {};
	GSVector4i m_scissor_cull_max = {};
	GSVector4i m_xyof = {};

	struct
	{
		GSVertex* buff;
		GSVertex* buff_copy;            // same size buffer to copy/modify the original buffer
		u32 head, tail, next, maxcount; // head: first vertex, tail: last vertex + 1, next: last indexed + 1
		u32 xy_tail;
		GSVector4i xy[4];
		GSVector4i xyhead;
	} m_vertex = {};

	struct
	{
		u16* buff;
		u32 tail;
	} m_index = {};

	struct
	{
		GSVertex* buff;
		u32 head, tail, next, maxcount; // head: first vertex, tail: last vertex + 1, next: last indexed + 1
		u32 xy_tail;
		GSVector4i xy[4];
		GSVector4i xyhead;
	} m_draw_vertex = {};

	struct
	{
		u16* buff;
		u32 tail;
	} m_draw_index = {};

	void UpdateContext();
	void UpdateScissor();

	void UpdateVertexKick();

	void GrowVertexBuffer();
	bool IsAutoFlushDraw(u32 prim, int& tex_layer);
	template<u32 prim> void HandleAutoFlush();
	bool EarlyDetectShuffle(u32 prim);
	void CheckCLUTValidity(u32 prim);

	template <u32 prim, bool auto_flush> void VertexKick(u32 skip);

	// following functions need m_vt to be initialized

	GSVertexTrace m_vt;
	GSVertexTrace::VertexAlpha& GetAlphaMinMax()
	{
		if (!m_vt.m_alpha.valid)
			CalcAlphaMinMax(0, INVALID_ALPHA_MINMAX);
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
	TextureMinMaxResult GetTextureMinMax(GIFRegTEX0 TEX0, GIFRegCLAMP CLAMP, bool linear, bool clamp_to_tsize);
	bool TryAlphaTest(u32& fm, u32& zm);
	bool IsOpaque();
	bool IsMipMapDraw();
	bool IsMipMapActive();
	bool IsCoverageAlpha();
	void CalcAlphaMinMax(const int tex_min, const int tex_max);
	void CorrectATEAlphaMinMax(const u32 atst, const int aref);

public:
	enum EEGS_TransferType
	{
		EE_to_GS,
		GS_to_GS,
		GS_to_EE
	};

	struct GSUploadQueue
	{
		GIFRegBITBLTBUF blit;
		GSVector4i rect;
		u64 draw;
		bool zero_clear;
		EEGS_TransferType transfer_type;
	};

	enum NoGapsType
	{
		Uninitialized = 0,
		GapsFound,
		SpriteNoGaps,
		FullCover,
	};

	GIFPath m_path[4] = {};
	const GIFRegPRIM* PRIM = nullptr;
	GSPrivRegSet* m_regs = nullptr;
	GSLocalMemory m_mem;
	GSDrawingEnvironment m_env = {};
	GSDrawingEnvironment m_prev_env = {};
	const GSDrawingEnvironment* m_draw_env = &m_env;
	GSDrawingContext* m_context = nullptr;
	GSVector4i temp_draw_rect = {};
	std::unique_ptr<GSDumpBase> m_dump;
	bool m_scissor_invalid = false;
	bool m_quad_check_valid = false;
	bool m_quad_check_valid_shuffle = false;
	bool m_are_quads = false;
	bool m_are_quads_shuffle = false;
	bool m_nativeres = false;
	bool m_mipmap = false;
	bool m_texflush_flag = false;
	bool m_isPackedUV_HackFlag = false;
	bool m_channel_shuffle = false;
	bool m_using_temp_z = false;
	bool m_temp_z_full_copy = false;
	bool m_in_target_draw = false;
	bool m_channel_shuffle_finish = false;

	u32 m_target_offset = 0;
	u8 m_scanmask_used = 0;
	u32 m_dirty_gs_regs = 0;
	int m_backed_up_ctx = 0;
	std::vector<GSUploadQueue> m_draw_transfers;
	NoGapsType m_primitive_covers_without_gaps;
	GSVector4i m_r = {};
	GSVector4i m_r_no_scissor = {};

	static u64 s_n;
	static u64 s_last_transfer_draw_n;
	static u64 s_transfer_n;

	GSPerfMon m_perfmon_frame; // Track stat across a frame.
	GSPerfMon m_perfmon_draw;  // Track stat across a draw.

	static constexpr u32 STATE_VERSION = 9;

	#define PRIM_REG_MASK 0x7FF
	#define MIPTBP_REG_MASK ((1ULL << 60) - 1ULL)
	#define CLAMP_REG_MASK ((1ULL << 44) - 1ULL)
	#define TEX1_REG_MASK 0xFFF001803FDULL
	#define XYOFFSET_REG_MASK 0x0000FFFF0000FFFFULL
	#define TEXA_REG_MASK 0xFF000080FFULL
	#define FOGCOL_REG_MASK 0xFFFFFF
	#define SCISSOR_REG_MASK 0x7FF07FF07FF07FFULL
	#define ALPHA_REG_MASK 0xFF000000FFULL
	#define DIMX_REG_MASK 0x7777777777777777ULL
	#define FRAME_REG_MASK 0xFFFFFFFF3F3F01FFULL
	#define ZBUF_REG_MASK 0x10F0001FFULL
	#define TEST_REG_MASK 0x7FFFF

	enum REG_DIRTY
	{
		DIRTY_REG_ALPHA,
		DIRTY_REG_CLAMP,
		DIRTY_REG_COLCLAMP,
		DIRTY_REG_DIMX,
		DIRTY_REG_DTHE,
		DIRTY_REG_FBA,
		DIRTY_REG_FOGCOL,
		DIRTY_REG_FRAME,
		DIRTY_REG_MIPTBP1,
		DIRTY_REG_MIPTBP2,
		DIRTY_REG_PABE,
		DIRTY_REG_PRIM,
		DIRTY_REG_SCANMSK,
		DIRTY_REG_SCISSOR,
		DIRTY_REG_TEST,
		DIRTY_REG_TEX0,
		DIRTY_REG_TEX1,
		DIRTY_REG_TEXA,
		DIRTY_REG_XYOFFSET,
		DIRTY_REG_ZBUF
	};

	enum GSFlushReason
	{
		UNKNOWN = 1 << 0,
		RESET = 1 << 1,
		CONTEXTCHANGE = 1 << 2,
		CLUTCHANGE = 1 << 3,
		GSTRANSFER = 1 << 4,
		UPLOADDIRTYTEX = 1 << 5,
		UPLOADDIRTYFRAME = 1 << 6,
		UPLOADDIRTYZBUF = 1 << 7,
		LOCALTOLOCALMOVE = 1 << 8,
		DOWNLOADFIFO = 1 << 9,
		SAVESTATE = 1 << 10,
		LOADSTATE = 1 << 11,
		AUTOFLUSH = 1 << 12,
		VSYNC  = 1 << 13,
		GSREOPEN = 1 << 14,
		VERTEXCOUNT = 1 << 15,
	};

	GSFlushReason m_state_flush_reason = UNKNOWN;

	enum PRIM_OVERLAP
	{
		PRIM_OVERLAP_UNKNOW,
		PRIM_OVERLAP_YES,
		PRIM_OVERLAP_NO
	};

	PRIM_OVERLAP m_prim_overlap = PRIM_OVERLAP_UNKNOW;
	std::vector<size_t> m_drawlist;
	std::vector<GSVector4i> m_drawlist_bbox;

	struct GSPCRTCRegs
	{
		struct PCRTCDisplay
		{
			bool enabled;
			int FBP;
			int FBW;
			int PSM;
			int DBY;
			int DBX;
			GSRegDISPFB prevFramebufferReg;
			GSVector2i prevDisplayOffset;
			GSVector2i displayOffset;
			GSVector4i displayRect;
			GSVector2i magnification;
			GSVector2i prevFramebufferOffsets;
			GSVector2i framebufferOffsets;
			GSVector4i framebufferRect;

			__fi int Block() const { return FBP << 5; }
		};

		int videomode = 0;
		int interlaced = 0;
		int FFMD = 0;
		bool PCRTCSameSrc = false;
		bool toggling_field = false;
		PCRTCDisplay PCRTCDisplays[2] = {};

		bool IsAnalogue();

		// Calculates which display is closest to matching zero offsets in either direction.
		GSVector2i NearestToZeroOffset();

		void SetVideoMode(GSVideoMode videoModeIn);

		// Enable each of the displays.
		void EnableDisplays(GSRegPMODE pmode, GSRegSMODE2 smode2, bool smodetoggle);

		void CheckSameSource();
		
		bool FrameWrap();

		// If the start point of both frames match, we can do a single read
		bool FrameRectMatch();

		GSVector2i GetResolution();

		GSVector4i GetFramebufferRect(int display);

		int GetFramebufferBitDepth();

		GSVector2i GetFramebufferSize(int display);

		// Sets up the rectangles for both the framebuffer read and the displays for the merge circuit.
		void SetRects(int display, GSRegDISPLAY displayReg, GSRegDISPFB framebufferReg);

		// Calculate framebuffer read offsets, should be considered if only one circuit is enabled, or difference is more than 1 line.
		// Only considered if "Anti-blur" is enabled.
		void CalculateFramebufferOffset(bool scanmask, GSRegDISPFB framebuffer0Reg, GSRegDISPFB framebuffer1Reg);

		// Used in software mode to align the buffer when reading. Offset is accounted for (block aligned) by GetOutput.
		void RemoveFramebufferOffset(int display);

		// If the two displays are offset from each other, move them to the correct offsets.
		// If using screen offsets, calculate the positions here.
		void CalculateDisplayOffset(bool scanmask);
	} PCRTCDisplays;

public:
	/// Returns the appropriate directory for draw dumping.
	static std::string GetDrawDumpPath(const char* format, ...);

	/// Expands dither matrix, suitable for software renderer.
	static void ExpandDIMX(GSVector4i* dimx, const GIFRegDIMX DIMX);

	/// Returns a string representing the flush reason.
	static const char* GetFlushReasonString(GSFlushReason reason);

	void ResetHandlers();
	void ResetPCRTC();

	GSVideoMode GetVideoMode();

	bool isinterlaced();
	bool isReallyInterlaced();

	float GetTvRefreshRate();

	virtual void Reset(bool hardware_reset);
	virtual void UpdateSettings(const Pcsx2Config::GSOptions& old_config);

	void Flush(GSFlushReason reason);
	u32 CalcMask(int exp, int max_exp);
	void FlushPrim();
	bool TestDrawChanged();
	void FlushWrite();
	virtual void Draw() = 0;
	virtual void PurgeTextureCache(bool sources, bool targets, bool hash_cache);
	virtual void ReadbackTextureCache();
	virtual void InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r) {}
	virtual void InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut = false) {}

	virtual void Move();

	GSVector4i GetTEX0Rect();
	void CheckWriteOverlap(bool req_write, bool req_read);
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

	u8* GetRegsMem() const { return reinterpret_cast<u8*>(m_regs); }
	void SetRegsMem(u8* basemem) { m_regs = reinterpret_cast<GSPrivRegSet*>(basemem); }

	void DumpDrawInfo(bool dump_regs, bool dump_verts, bool dump_transfers);
	void DumpVertices(const std::string& filename);
	void DumpTransferList(const std::string& filename);
	void DumpTransferImages();
	
	template<bool shuffle_check>
	bool TrianglesAreQuadsImpl();
	bool TrianglesAreQuads(bool shuffle_check = false);
	template <u32 primclass>
	PRIM_OVERLAP GetPrimitiveOverlapDrawlistImpl(bool save_drawlist = false, bool save_bbox = false, float bbox_scale = 1.0f);
	PRIM_OVERLAP GetPrimitiveOverlapDrawlist(bool save_drawlist = false, bool save_bbox = false, float bbox_scale = 1.0f);
	PRIM_OVERLAP PrimitiveOverlap(bool save_drawlist = false);
	bool SpriteDrawWithoutGaps();
	void CalculatePrimitiveCoversWithoutGaps();
	GIFRegTEX0 GetTex0Layer(u32 lod);
};

// We put this in the header because of Multi-ISA.
inline void GSState::ExpandDIMX(GSVector4i* dimx, const GIFRegDIMX DIMX)
{
	dimx[1] = GSVector4i(DIMX.DM00, 0, DIMX.DM01, 0, DIMX.DM02, 0, DIMX.DM03, 0);
	dimx[0] = dimx[1].xxzzlh();
	dimx[3] = GSVector4i(DIMX.DM10, 0, DIMX.DM11, 0, DIMX.DM12, 0, DIMX.DM13, 0);
	dimx[2] = dimx[3].xxzzlh();
	dimx[5] = GSVector4i(DIMX.DM20, 0, DIMX.DM21, 0, DIMX.DM22, 0, DIMX.DM23, 0);
	dimx[4] = dimx[5].xxzzlh();
	dimx[7] = GSVector4i(DIMX.DM30, 0, DIMX.DM31, 0, DIMX.DM32, 0, DIMX.DM33, 0);
	dimx[6] = dimx[7].xxzzlh();
}
