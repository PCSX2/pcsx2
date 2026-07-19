// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/GS.h"
#include "GS/GSPerfMon.h"
#include "GS/GSLocalMemory.h"
#include "GS/GSVertexKick.h"
#include "GS/GSBackQueue.h"
#include "GS/GSDrawingContext.h"
#include "GS/GSDrawingEnvironment.h"
#include "GS/Renderers/Common/GSVertex.h"
#include "GS/Renderers/Common/GSVertexTrace.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GS/GSVector.h"
#include "GSAlignedClass.h"

#include "common/Threading.h"

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

class GSDumpBase;

class GSState : public GSAlignedClass<32>
{
	// GSVertexTrace::Update consumes the per-buffer fused FindMinMax accumulator
	// (m_vertex->fmm_*) directly.
	friend class GSVertexTrace;
	// GV7-1d-ii: the front parser object delegates protected queries/seams to
	// the back renderer through a GSState*.
	friend class GSFrontState;

public:
	// GV7-1d-ii: shared_chan aims this object at another GSState's channel — the
	// front parser object of the two-object split passes the back object's
	// channel so its records land in the consumed ring. Default (nullptr) uses
	// this object's own channel storage, exactly as before.
	GSState(GSBackQueue::Channel* shared_chan = nullptr);
	virtual ~GSState();

	// GV7-1d-ii: channel/back-thread visibility for the front-object lifecycle
	// in GS.cpp (create the front only when the back thread actually engaged).
	GSBackQueue::Channel* GetBackChannel() { return m_chan; }
	bool IsBackThreadRunning() const { return m_chan->consumer_running; }

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
	// Executor-owned HOST->LOCAL write cursor (advanced by wi() across transfer
	// slices; mirrored back into m_tr.x/y inline for savestate coherence).
	int m_exec_tr_x = 0;
	int m_exec_tr_y = 0;

	static constexpr int INVALID_ALPHA_MINMAX = 500;
	static constexpr int MAX_DRAW_BUFFERS = 3;

	GSVertex m_v = {};
	float m_q = 1.0f;
	GSVector4i m_xyof = {};
	int  m_used_buffers_idx = 0;
	int m_current_buffer_idx = 0;
	bool m_recent_buffer_switch = false;

	// Definitions hoisted to GSBackQueue.h (DRAW record payload types).
	using GSVertexBuff = GSBackQueue::VertexBuff;
	using GSIndexBuff = GSBackQueue::IndexBuff;

	GSVertexBuff m_vertex_buffers[MAX_DRAW_BUFFERS];
	GSVertexBuff* m_vertex = nullptr;

	GSIndexBuff m_index_buffers[MAX_DRAW_BUFFERS];

	GSIndexBuff* m_index;

	GSVertexBuff m_draw_vertex = {};

	struct
	{
		u16* buff;
		u32 tail;
	} m_draw_index = {};

	struct GSDrawBufferEnv
	{
		GSDrawingEnvironment m_env;
		int m_backed_up_ctx = 0;
		u32 m_dirty_regs = 0;
		GSVector4i draw_rect = GSVector4i::zero();
		bool related_draw = false;
	};

	GSDrawBufferEnv m_env_buffers[MAX_DRAW_BUFFERS] = {};

	void UpdateContext();
	void UpdateScissor();

	void UpdateVertexKick();

	void GrowVertexBuffer();
	bool IsAutoFlushDraw(u32 prim, int& tex_layer);
	template<u32 prim> void HandleAutoFlush();
	bool EarlyDetectShuffle(u32 prim);
	void CheckCLUTValidity(u32 prim);
	bool CheckOverlapVerts(u32 n);
	bool CheckOverlapVertsSlow(u32 n);

	void ApplyDepthClamp(u32& z);
	GSLimit24BitDepth GetDepthClampMode() const;

	static __fi void ApplyDepthClampMode(GSLimit24BitDepth mode, u32& z)
	{
		if (mode == GSLimit24BitDepth::PrioritizeUpper)
			z = ((z >> 8) & ~0xFF) | (z & 0xFF);
		else if (mode == GSLimit24BitDepth::PrioritizeLower)
			z &= 0x00FFFFFF;
	}

	// Batch cursor: caches the hot vertex/index buffer fields in locals so they live
	// in registers across a fused packed-handler batch instead of round-tripping
	// through m_vertex/m_index per vertex. Store() must run before ANY call that can
	// flush, grow or switch draw buffers (Flush, GrowVertexBuffer,
	// CheckOverlapVertsSlow, HandleAutoFlush — GrowVertexBuffer reads tail for the
	// preserved-copy size), and Load() again after. buff/maxcount are only ever
	// changed by those callees, so Store() never writes them back.
	struct VertexKickCursor
	{
		GSVertexBuff* vb;
		GSIndexBuff* ib;
		GSVertex* vbuff;
		u16* ibuff;
		u32 head, tail, next, xy_tail, maxcount, itail;

		// Deferred draw_rect accumulation: accepted prims union their (already
		// subpixel-shifted, exclusive) rects here; Store() folds the result into
		// temp_draw_rect with one scissor clamp. Exact because rintersect is
		// monotone and idempotent, so clamping once over the union equals the
		// per-prim clamp-then-union chain, and because a draw's first prim (which
		// replaces temp_draw_rect instead of unioning) can only be the first
		// accumulated after a seam — the index buffer only empties behind
		// flush seams.
		GSVector4i acc_rect;
		u32 acc_state; // 0 = empty, 1 = union into temp_draw_rect, 2 = replace it
		GSVector4i* temp_rect;
		const GSVector4i* scissor_in;

		__fi void Load(GSState& s)
		{
			vb = s.m_vertex;
			ib = s.m_index;
			vbuff = vb->buff;
			ibuff = ib->buff;
			head = vb->head;
			tail = vb->tail;
			next = vb->next;
			xy_tail = vb->xy_tail;
			maxcount = vb->maxcount;
			itail = ib->tail;
			acc_state = 0;
			temp_rect = &s.temp_draw_rect;
			scissor_in = &s.m_context->scissor.in;
		}

		__fi void Store() const
		{
			vb->head = head;
			vb->tail = tail;
			vb->next = next;
			vb->xy_tail = xy_tail;
			ib->tail = itail;

			if (acc_state != 0)
			{
				const GSVector4i merged = (acc_state == 2) ? acc_rect : temp_rect->runion(acc_rect);
				*temp_rect = merged.rintersect(*scissor_in);
			}
		}
	};

	// Pre-adjusted scissor bounds for the scalar-outcode cull (GSVertexKick.h),
	// re-derived when the cull rect changes. band = triangle/sprite native-res
	// space, raw = point/line 12.4 space. m_cull_bounds_src is the cull rect the
	// bounds were derived from (poison-initialized so the first update always
	// refreshes).
	GSVector4i m_cull_bounds_src = GSVector4i::cxpr(-2, -2, -2, -2);
	GSVertexKernels::CullBounds m_cull_bounds_band = {};
	GSVertexKernels::CullBounds m_cull_bounds_raw = {};

	void RefreshKickMirror();

	template <u32 prim, bool auto_flush> void VertexKick(u32 skip);
	template <u32 prim, bool auto_flush> void VertexKickDirect(u32 skip, u32 xraw, u32 yraw, const GSVector4i& v0, const GSVector4i& v1, VertexKickCursor& c);

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
	bool IsFlatShaded();
	bool IsOpaque();
	bool IsMipMapDraw();
	bool IsMipMapActive();
	bool IsCoverageAlpha();
	bool IsCoverageAlphaFixedOne();
	virtual bool IsCoverageAlphaSupported();
	// GV7-1d-ii: back-half of the split front's kick-time coverage-alpha query
	// (HW only): cached-ctx/alpha-minmax from this object's last executed draw,
	// the caller's live ALPHA passed in.
	virtual bool IsRTWrittenLive(const GIFRegALPHA& ALPHA);
	void CalcAlphaMinMax(const int tex_min, const int tex_max);
	void CorrectATEAlphaMinMax(const u32 atst, const int aref);

	// Utility functions for getting position/texture coordinates.
	GSVector4 GetXYWindow(const GSVertex& v);
	template<bool fst>
	GSVector4 GetTexCoordsImpl(const GSVertex& v, float q);
	template<bool fst>
	GSVector4 GetTexCoordsImpl(const GSVertex& v);
	GSVector4 GetTexCoords(const GSVertex& v, float q);
	GSVector4 GetTexCoords(const GSVertex& v);

	// Utility functions to detect and get corners of quads.
	template<u32 primclass, bool tme = false, bool fst = false>
	static bool GetQuadCornersImpl(const GSVertex* v, const u16* i, GSVertex& vout0, GSVertex& vout1);
	bool GetQuadCorners(const GSVertex* v, const u16* i, GSVertex& vout0, GSVertex& vout1);

	// Utility functions to get window/texture coordinates of a quad.
	template<u32 primclass>
	void GetQuadBBoxWindowImpl(const GSVertex& v0, const GSVertex& v1, GSVector4& xyout);
	template<u32 primclass, bool tme = false, bool fst = false>
	void GetQuadBBoxWindowImpl(const GSVertex& v0, const GSVertex& v1, GSVector4& xyout, GSVector4& texout, bool keep_tex_order = true);
	void GetQuadBBoxWindow(const GSVertex& v0, const GSVertex& v1, GSVector4& xyout);
	void GetQuadBBoxWindow(const GSVertex& v0, const GSVertex& v1, GSVector4& xyout, GSVector4& texout, bool keep_tex_order = true);

	// Adjusts a quad so that it contains exactly the centers of the pixels that the GS would rasterize.
	static void GetQuadRasterizedPoints(GSVector4& xy, bool keep_order = true);
	static void GetQuadRasterizedPoints(GSVector4& xy, GSVector4& tex, bool keep_order = true);

public:
	enum EEGS_TransferType
	{
		EE_to_GS,
		GS_to_GS,
		GS_to_EE,
		Clear
	};

	struct GSUploadQueue
	{
		GIFRegBITBLTBUF blit;
		u64 draw;
		GSVector4i rect;
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
	GSDrawingEnvironment m_temp_env = {};
	const GSDrawingEnvironment* m_draw_env = &m_env;
	GSDrawingContext* m_context = nullptr;
	GSVector4i temp_draw_rect;
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

	// Definition hoisted to GSBackQueue.h (PCRTC_SYNC record payload type).
	using GSPCRTCRegs = GSBackQueue::GSPCRTCRegs;

	GSPCRTCRegs PCRTCDisplays;

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

	void ResetDrawBuffers();
	void ResetDrawBufferIdx();
	void FlushBuffers(bool flush_base_only = false, bool use_flush_reason = false, GSFlushReason flush_reason = GSFlushReason::CONTEXTCHANGE);
	void PushBuffer();
	void SetDrawBufferEnv();
	void SetDrawBuffDirty();
	bool CanBufferNewDraw();
	void Flush(GSFlushReason reason);
	void FlushDraw(GSFlushReason reason);
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

	// GV-7 front/back seam (SEAM-AUDIT.md): the front builds a self-contained
	// record, the Exec*Record executor consumes it — inline today, on the back
	// thread once GV7-1 lands. The executor owns the HOST->LOCAL write cursor
	// across transfer slices.
	void ExecTransferRecord(const GSBackQueue::TransferRecord& rec);
	void SubmitMove();
	void ExecMoveRecord(const GSBackQueue::MoveRecord& rec);
	void SubmitClutLoad(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);
	void ExecClutLoadRecord(const GSBackQueue::ClutLoadRecord& rec);
	void ExecDrawRecord(const GSBackQueue::DrawRecord& rec);
	void DrawRecordTail(u64 draw_serial);
	void SubmitPcrtcSync();
	void ExecPcrtcSyncRecord(const GSBackQueue::PcrtcSyncRecord& rec);

	// GV7-1: sampled from GSConfig.BackThreadMode at construction (the option is
	// restart-required, so it can't change under a live GSState). Off = the
	// front-side seam functions skip the record round-trip entirely and call the
	// executor tails against live state; any other mode builds records.
	bool m_back_records = false;

	// GV7-1d-ii: the front<->back channel (record ring + wake semaphore + pool
	// arenas/free rings, GSBackQueue.h). Single-object modes use this object's
	// own storage; the two-object pipelined split points the front parser
	// object's m_chan at the back object's channel. The destructor frees
	// m_chan_storage's pooled arrays — only ever this object's own storage, so
	// a front pointing elsewhere frees nothing it doesn't own.
	GSBackQueue::Channel m_chan_storage;
	GSBackQueue::Channel* m_chan = &m_chan_storage;

	// GV7-1d-ii: the object owning local memory, the CLUT palette, and the
	// texture cache for this session. Single-object modes: this. On the front
	// parser object it points at the back renderer, so the drained seams
	// (readbacks, savestates) reach the authoritative m_mem/TC while every
	// register decision stays front-side. Only ever dereferenced after a drain.
	GSState* m_mem_target = this;

	// GV7-1d-ii: set on the back renderer when a front parser object exists.
	// The draw executor then aims m_draw_env/PRIM/m_context around the tail
	// itself (on a single object FlushDraw owns that aiming, and the front's
	// carry-over rebuild depends on FlushDraw's restore happening after).
	bool m_split_back = false;

	// GV7-1c: draw-node pool. Acquire is front-side (free ring first, then arena
	// growth up to the ring capacity, then backpressure); Release is the consume
	// site (inline modes: FlushPrim right after the executor returns; pipelined:
	// the back thread after DrawRecordTail).
	GSBackQueue::DrawNode* AcquireDrawNode();
	void ReleaseDrawNode(GSBackQueue::DrawNode* node);

	// GV7-1c: transfer payload pool (record modes only; mode 0 keeps
	// GSTransferBuffer's own allocation untouched). m_tr.buff aliases the
	// current node's 4MB buffer; RotateTransferPayload runs at transfer Init and
	// swaps to a fresh node once records reference the current one.
	// AdoptTransferBuffer (run by the staging object at construction) hands
	// m_tr's original buffer to the channel as node 0 (the dtor nulls m_tr.buff
	// before the arena walk so it isn't freed twice).
	GSBackQueue::PayloadNode* m_tr_payload_node = nullptr;
	bool m_tr_payload_referenced = false;
	void AdoptTransferBuffer();
	GSBackQueue::PayloadNode* AcquirePayloadNode();
	void RotateTransferPayload();
	void ExecReleasePayloadRecord(const GSBackQueue::ReleasePayloadRecord& rec);

	// GV7-1d: the back thread (modes Lockstep and, for now, Pipelined — true
	// pipelining needs the front-object split, so Pipelined runs lockstep until
	// then). Lockstep = drain after every push, which is what makes executing
	// against the shared single-object state safe. VSYNC records are NOT queued:
	// present runs on the MTGS thread after a drain, so the back thread never
	// touches the GSDevice on present paths (and for SW, at all). Queued modes
	// engage only for Vulkan and SW renderers — a GL device is context-bound to
	// the MTGS thread and HW draws would issue GL calls from the wrong thread.
	bool m_back_queued = false;
	bool m_back_lockstep = false;
	std::thread m_back_thread;
	std::atomic<bool> m_back_thread_exit{false};

	void StartBackThread();
	void StopBackThread();
	void DrainBackQueue();
	void BackThreadLoop();
	void ExecRecordSlot(const GSBackQueue::RecordSlot& slot);
	virtual void ExecVsyncRecord(const GSBackQueue::VsyncRecord& rec);

	template <typename T>
	void PushRecord(GSBackQueue::RecordType type, const T& rec)
	{
		for (;;)
		{
			GSBackQueue::RecordSlot* slot = m_chan->ring.BeginPush();
			if (slot)
			{
				slot->type = type;
				std::memcpy(slot->As<T>(), &rec, sizeof(T));
				m_chan->ring.CommitPush();
				m_chan->sema.NotifyOfWork();
				break;
			}
			std::this_thread::yield(); // ring full — backpressure
		}

		// Spin-then-sleep: records usually execute in microseconds, so the spin
		// catches nearly every drain without the futex round-trip. Lockstep is
		// still per-record synchronization and inherently slow (measured 30->6
		// fps on MQ65 with plain WaitForEmpty) — it's the bisect rung, not a
		// shipping mode.
		if (m_back_lockstep)
			m_chan->sema.WaitForEmptyWithSpin();
	}

	GSVector4i GetTEX0Rect(GSDrawingContext prev_ctx);
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
	PRIM_OVERLAP GetPrimitiveOverlapDrawlistImpl(bool save_drawlist = false, bool save_bbox = false,
		float bbox_scale = 1.0f, u32* max_size = nullptr);
	PRIM_OVERLAP GetPrimitiveOverlapDrawlist(bool save_drawlist = false, bool save_bbox = false,
		float bbox_scale = 1.0f, u32* max_size = nullptr);
	PRIM_OVERLAP PrimitiveOverlap(bool save_drawlist = false);
	bool SpriteDrawWithoutGaps();
	void CalculatePrimitiveCoversWithoutGaps();
	GIFRegTEX0 GetTex0Layer(u32 lod);
};

// GV7-1d-ii: the front parser object of the two-object pipelined split
// (SEAM-AUDIT.md §7). Owns all parse state (env, vertex kick, draw buffering,
// transfer staging, CLUT decision) and emits records into the back renderer's
// channel; the back object executes them on the back thread, installing record
// state into its own members. The front never draws, and reaches the
// authoritative local memory / texture cache only through m_mem_target after a
// drain. Created by GS.cpp only when the back thread engaged under
// GSBackThreadMode::Pipelined.
class GSFrontState final : public GSState
{
public:
	GSFrontState(GSState* back);
	~GSFrontState() override;

	void Draw() override;

	// Parse-path virtuals must answer exactly as the back renderer would (they
	// steer kick/flush decisions); the overridden implementations only read
	// session-constant config/device caps, so cross-object calls are safe.
	bool IsCoverageAlphaSupported() override;

	// Once per frame, after the (drained) vsync executed on the back object:
	// re-mirror present-side state the back mutated (Merge's scanmask
	// decrement) so next frame's front digestion sees what a single object
	// would have.
	void MirrorPostVsyncState();

private:
	GSState* m_back;
};

extern std::unique_ptr<GSFrontState> g_gs_front;

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
