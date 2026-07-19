// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

// Fixed-cap set of microMem byte offsets at which the dispatcher has
// handed off into a microProgram. Single-threaded per VU. `version`
// bumps on every new-PC insert so consumers can detect that the entry
// set has grown. Overflow beyond `kMax` silently drops; those entries
// retain JIT coverage but are not tracked here.
//
// Lives in a standalone header so tests can exercise the helper
// without pulling the full microVU-arm64.h surface.
struct MvuObservedEntries
{
	static constexpr u32 kMax = 32;
	u32 pcs[kMax];
	u8  count;
	u8  pad[3];
	u32 version;

	// Records `startPC_bytes` (microMem byte offset) as a new entry
	// point. Returns true if a fresh slot was filled (and `version`
	// bumped); false if `startPC_bytes` was already present or the
	// cap was hit.
	bool record(u32 startPC_bytes);

	// Reset to the empty state. Equivalent to memset-zero, exposed
	// so callers don't need to know the layout.
	void clear();
};
