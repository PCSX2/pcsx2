// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/GSRegs.h"
#include "GS/GSVector.h"

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
} // namespace GSBackQueue
