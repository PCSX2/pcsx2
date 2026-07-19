// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/GS.h"
#include "GS/GSRegs.h"
#include "GS/GSVector.h"
#include "GS/GSDrawingContext.h"
#include "GS/GSDrawingEnvironment.h"
#include "GS/GSVertexKick.h"
#include "GS/Renderers/Common/GSVertex.h"

// GV-7: self-contained records crossing the GS front (GIF parse / vertex kick /
// draw buffering) → back (local memory, texture cache, draw, present) boundary.
// Every record carries its register snapshot, so the consumer needs no live
// register state machine. Records are built by the front-side seam functions
// (FlushWrite / Move / ...) and consumed by GSState::Exec*Record — executed
// inline today, and on the back thread once the GV7-1 queue lands.
// Seam classification: scratchpad/gv7-2026-07/SEAM-AUDIT.md.

namespace GSBackQueue
{
	// One slice of a HOST->LOCAL transfer (today's FlushWrite body, or the
	// whole-packet fast path in GSState::Write). A logical transfer is one
	// first_slice record followed by zero or more continuation slices; the
	// executor owns the write cursor across slices.
	struct TransferRecord
	{
		GIFRegBITBLTBUF blit; // m_tr.m_blit: wi() blit argument + partial-end fixup
		GIFRegBITBLTBUF env_blit; // invalidate rect + wi selection: m_env.BITBLTBUF in
		                          // FlushWrite, m_tr.m_blit on the Write fast path —
		                          // they diverge if BITBLTBUF is rewritten mid-transfer
		GIFRegTRXPOS pos;
		GIFRegTRXREG reg;
		GSVector4i rect; // m_tr.rect
		const u8* payload;
		int len; // bytes handed to wi()
		int stat_len; // bytes counted for the Swizzle perfmon stat (the Write
		              // fast path counts the raw packet length, which can
		              // exceed the transfer total — preserved exactly)
		int end; // m_tr.end as of this slice (partial-end fixup input)
		int total; // m_tr.total
		int init_x, init_y; // write-cursor init, consumed when first_slice
		u64 draw_serial; // s_n at build time (upload-queue entry stamping)
		bool first_slice; // initialises the cursor + pushes the upload-queue entry
	};

	// LOCAL->LOCAL blit. The executor installs these registers and runs the
	// virtual Move chain (HW hack -> TC move -> software blit) unchanged.
	struct MoveRecord
	{
		GIFRegBITBLTBUF blit;
		GIFRegTRXPOS pos;
		GIFRegTRXREG reg;
		u64 draw_serial; // consumed once GV7-0d makes serials record-carried
	};

	// CLUT palette load. The decision chain (WriteTest / CanLoadCLUT /
	// InvalidateRange dirty tracking) is register/address-only and stays
	// front-side; this record triggers the back-side palette-byte read from
	// local memory into the CLUT buffer.
	struct ClutLoadRecord
	{
		GIFRegTEX0 TEX0; // post-CPSM-mask, as installed in m_env.CTXT[i]
		GIFRegTEXCLUT TEXCLUT;
	};

	// Vertex/index buffer sets — the DRAW record payload. Hoisted from GSState
	// (front fills them at kick time, the draw executor consumes and mutates
	// them in place); the GV7-1 pool hands ownership across the boundary.
	struct VertexBuff
	{
		GSVertex* buff;
		GSVertex* buff_copy; // same size buffer to copy/modify the original buffer
		u32 head, tail, next, maxcount; // head: first vertex, tail: last vertex + 1, next: last indexed + 1
		u32 xy_tail;
		GSVector4i xy[4];
		GSVector4i xyhead;
		// Scalar mirror of xy[] for the outcode cull fast path: written wherever
		// xy[] is written, outcodes re-derived on scissor change (RefreshKickMirror).
		GSVertexKernels::CullMirrorEntry kick_ring[4];
		// Fused vertex-trace bounds (aarch64 only): FindMinMax min/max accumulated
		// at index emission over this buffer's referenced vertices. fmm_watermark is
		// the first vertex position not yet folded in (clamped on rewinds/compaction
		// so re-referenced positions re-accumulate); fmm_valid means the accumulator
		// covers every emitted index of the pending draw. Reset lazily at the first
		// emission of a draw (itail == n).
		GSVertexKernels::FmmAcc fmm_acc;
		u32 fmm_watermark;
		bool fmm_valid;
	};

	struct IndexBuff
	{
		u16* buff;
		u32 tail;
	};

	// PCRTC digest state — hoisted from GSState (GSvsync writes it once per
	// frame from the privileged registers; the Draw() heuristics and the Merge
	// circuit read it back-side, so it ships whole in PCRTC_SYNC records).
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
	};

	// Once-per-frame PCRTC digest, shipped BEFORE the vsync-flushed draw
	// records so those draws see the fresh display state, exactly like today
	// (GSvsync digests, then flushes). Mid-frame draws keep seeing the previous
	// frame's digest, also like today.
	struct PcrtcSyncRecord
	{
		GSPCRTCRegs displays;
		u8 scanmask_used; // pre-decrement value; Merge's decrement stays back-side
	};

	// End of frame: the whole VSync() body (Merge -> present -> capture ->
	// perfmon frame tick) runs back-side.
	struct VsyncRecord
	{
		u32 field;
		bool registers_written;
		bool idle_frame;
	};

	// One flushed draw (today's FlushPrim tail: vertex trace -> texel rounding ->
	// Draw() -> perfmon). Self-contained: the executor installs the env snapshots
	// and scalars, then runs the tail against the referenced buffers, which it
	// owns and may mutate in place (texel rounding, HW draw rewrites). The
	// carry-over window is captured front-side before the record is built.
	struct DrawRecord
	{
		// Installed into the consumer's m_prev_env: the draw's own environment,
		// exactly as FlushBuffers staged it before the flush.
		GSDrawingEnvironment draw_env;
		// Next-draw peek for the HW look-ahead heuristics: the live m_env/m_v at
		// build time — bit-exact with today because FlushBuffers installs buffer
		// i+1's env into m_env before flushing buffer i.
		GSDrawingEnvironment next_env;
		GSVertex next_v;
		GSVector4i draw_rect; // temp_draw_rect at flush
		VertexBuff* vertex; // record-owned; pool handles once GV7-1 lands
		IndexBuff* index;
		u64 draw_serial; // front-assigned s_n
		int backed_up_ctx;
		u32 dirty_gs_regs;
		int flush_reason; // GSState::GSFlushReason (class-scoped enum, stored widened)
		bool channel_shuffle_finish;
		bool packed_uv_hack_flag;
	};
} // namespace GSBackQueue
