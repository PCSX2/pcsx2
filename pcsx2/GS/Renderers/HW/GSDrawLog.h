// SPDX-FileCopyrightText: 2026 ARMSX2 Contributors
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSDevice.h"
#include "GS/GSRegs.h"
#include "common/Pcsx2Defs.h"

#include <string>

/// Per-draw ledger for GS performance triage.
///
/// Answers "which draws were expensive, and what PS2 state made them expensive" over a
/// whole scene. The existing per-draw facility (GSHWDrawConfig::DumpConfig, driven by
/// SaveHWConfig) writes one text file per draw, which is the right shape for inspecting
/// a single suspicious draw and the wrong shape for profiling: a 900-draw frame produces
/// 900 files. This produces one append-only table instead.
///
/// I/O discipline is the whole design. At ~900 draws/frame and 60 fps this sees roughly
/// 54k rows/sec; formatting a row costs microseconds and writing it costs bandwidth, and
/// both would land on the GS thread -- the thread under investigation -- turning the
/// ledger into a measurement of itself. So capture records a packed POD into a
/// preallocated arena (a memcpy-class store, no formatting, no I/O) and serialisation to
/// CSV happens once, afterwards.
///
/// The arena is bounded, so a long session yields the last N frames rather than an
/// unusable multi-gigabyte file. Truncation is reported rather than silent.
///
/// A row is assembled from two points in the draw, because the field sets do not
/// overlap: the PS2 view (registers, live at the top of GSRendererHW::Draw) and the
/// backend view (GSHWDrawConfig, only finalised at submit).
///
/// This is an attribution tool, never a comparison tool. It is not free -- roughly 0.3%
/// of a core plus the cache pressure of streaming writes -- so an A/B with the ledger
/// enabled on one arm is invalid.
namespace GSDrawLog
{
	/// Packed to keep the arena small and the per-draw store cheap. Enum-ish fields are
	/// stored raw and named at serialisation time.
	struct Record
	{
		u32 frame;
		u32 draw; // GS draw serial (s_n)

		u32 frame_block;
		u32 frame_fbmsk;
		u32 z_block;
		u32 tex_tbp0;

		u16 prim_count;
		u16 alpha; // ALPHA A/B/C/D, 2 bits each

		u8 prim_type;
		u8 frame_psm;
		u8 frame_fbw;
		u8 z_psm;
		u8 z_ztst;
		u8 tex_psm;
		u8 tex_tbw;
		u8 tex_tw;
		u8 tex_th;
		u8 atst;
		u8 afail;
		u8 datm;
		u8 flags;

		// Backend view, filled at submit. Zeroed if the draw returned early.
		u8 topology;
		u8 tex_hazard;
		u8 destination_alpha;
		u8 colormask;
		u8 barrier; // 0 none, 1 one-barrier, 2 full-barrier

		s16 area_x;
		s16 area_y;
		s16 area_z;
		s16 area_w;
	};

	enum Flags : u8
	{
		FlagTextured = 1 << 0,
		FlagBlend = 1 << 1,
		FlagAlphaTest = 1 << 2,
		FlagDate = 1 << 3,
		FlagZTest = 1 << 4,
		FlagZMask = 1 << 5,
		FlagSubmitted = 1 << 6, ///< reached the backend; absent means the draw was skipped
	};

	/// Whether recording is currently active. Cheap enough to test per draw.
	bool IsActive();

	/// Allocates the arena and begins recording. Safe to call when already active.
	void Start();

	/// Stops recording. Retains the arena so it can still be written out.
	void Stop();

	/// Begins a row from the PS2 register state. Returns without effect if inactive or
	/// the arena is full.
	void BeginDraw(const Record& ps2_state);

	/// Completes the row opened by BeginDraw with the backend view. No-op if BeginDraw
	/// did not record one (inactive, or arena full).
	void EndDraw(const GSHWDrawConfig& config);

	/// Closes any row left open by a draw that returned before submit, so skipped draws
	/// still appear. Called on every exit from GSRendererHW::Draw.
	void FinishDraw();

	/// Serialises the arena to CSV. Returns false on I/O failure.
	bool WriteCSV(const std::string& path);

	/// Number of rows recorded, and whether the arena filled up and dropped rows.
	size_t GetRecordCount();
	bool WasTruncated();

	/// Drops all recorded rows and frees the arena.
	void Reset();
} // namespace GSDrawLog
