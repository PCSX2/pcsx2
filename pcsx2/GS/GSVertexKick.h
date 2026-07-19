// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/GSRegs.h"
#include "GS/GSVector.h"

#include <cfloat>

// Pure kernels backing the fused GIF packed vertex handlers and the per-prim
// accept/cull decision in GSState::VertexKick. Factored out of GSState.cpp so the
// gs_vertex_tests oracle suite can drive them standalone. (During the GV campaign
// the optimized kernels were additionally crosschecked against the legacy ones
// per vertex/prim over live replays — that plumbing is gone; recover the
// GS_VERTEX_CROSSCHECK machinery from git history if a divergence hunt ever
// needs it again.)
//
// Anything here must stay a pure function of its arguments: no GSState members,
// no config reads. Behavioral contract for the legacy kernels is pinned by
// tests/ctest/core/gs/gs_vertex_tests.cpp against an independent scalar model.
namespace GSVertexKernels
{
	// Parse one packed {STQ, RGBAQ, XYZF2} record (r[0..2]) into GSVertex m[0]/m[1].
	// uv = the latched UV register value (packed XYZF2 does not write UV). Q == +0.0
	// (integer compare, so -0.0 passes through) is rewritten to FLT_MIN to avoid
	// divides by zero downstream — matches GIFPackedRegHandlerSTQ.
	__forceinline_odr void ParsePackedSTQRGBAXYZF2(const GIFPackedReg* RESTRICT r, u32 uv, GSVector4i& m0, GSVector4i& m1)
	{
		const GSVector4i st = GSVector4i::loadl(&r[0].U64[0]);
		GSVector4i q = GSVector4i::loadl(&r[0].U64[1]);
		const GSVector4i rgba = (GSVector4i::load<false>(&r[1]) & GSVector4i::x000000ff()).ps32().pu16();

		q = q.blend8(GSVector4i::cast(GSVector4(FLT_MIN)), q == GSVector4i::zero());

		m0 = st.upl64(rgba.upl32(q));

		GSVector4i xy = GSVector4i::loadl(&r[2].U64[0]);
		GSVector4i zf = GSVector4i::loadl(&r[2].U64[1]);
		xy = xy.upl16(xy.srl<4>()).upl32(GSVector4i::load((int)uv));
		zf = zf.srl32<4>() & GSVector4i::x00ffffff().upl32(GSVector4i::x000000ff());

		m1 = xy.upl32(zf);
	}

	// Parse one packed {STQ, RGBAQ, XYZ2} record. Z is the full 32 bits; UV and FOG
	// are both preserved from the current vertex state (passed packed as {UV, FOG}).
	__forceinline_odr void ParsePackedSTQRGBAXYZ2(const GIFPackedReg* RESTRICT r, u64 uvfog, GSVector4i& m0, GSVector4i& m1)
	{
		const GSVector4i st = GSVector4i::loadl(&r[0].U64[0]);
		GSVector4i q = GSVector4i::loadl(&r[0].U64[1]);
		const GSVector4i rgba = (GSVector4i::load<false>(&r[1]) & GSVector4i::x000000ff()).ps32().pu16();

		q = q.blend8(GSVector4i::cast(GSVector4(FLT_MIN)), q == GSVector4i::zero());

		m0 = st.upl64(rgba.upl32(q));

		const GSVector4i xy = GSVector4i::loadl(&r[2].U64[0]);
		const GSVector4i z = GSVector4i::loadl(&r[2].U64[1]);
		const GSVector4i xyz = xy.upl16(xy.srl<4>()).upl32(z);

		m1 = xyz.upl64(GSVector4i::loadl(&uvfog));
	}

#ifdef ARCH_ARM64
	// aarch64-native parse: the whole vertex build is byte movement, so one TBL
	// gathers each qword (the legacy path spends ~11 NEON ops on the RGBA
	// pack chain alone). Out-of-range TBL indices read as zero, which provides the
	// 24-bit Z and 8-bit F masks for free. Bit-identical to the legacy kernels —
	// pinned by gs_vertex_tests.
	__forceinline_odr void ParsePackedSTQRGBAXYZF2_Neon(const GIFPackedReg* RESTRICT r, u32 uv, GSVector4i& m0, GSVector4i& m1)
	{
		// m0 = {S, T, RGBA, Q}: S/T = r0 bytes 0-7, RGBA = r1 bytes 0/4/8/12, Q = r0 bytes 8-11.
		alignas(16) static constexpr u8 pat_m0[16] = {0, 1, 2, 3, 4, 5, 6, 7, 16, 20, 24, 28, 8, 9, 10, 11};
		// m1 = {X|Y<<16, Z, UV, F}: X/Y = r2 bytes 0-1/4-5, Z = (r2>>4) bytes 8-10, F = (r2>>4) byte 12.
		alignas(16) static constexpr u8 pat_m1[16] = {0, 1, 4, 5, 24, 25, 26, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 28, 0xFF, 0xFF, 0xFF};
		// Q == +0.0 (integer compare) rewrites to FLT_MIN; other lanes OR with 0.
		alignas(16) static constexpr u32 q_fixup[4] = {0, 0, 0, 0x00800000};

		const uint8x16x2_t st_rgba = {vld1q_u8(reinterpret_cast<const u8*>(r + 0)), vld1q_u8(reinterpret_cast<const u8*>(r + 1))};
		uint32x4_t v0 = vreinterpretq_u32_u8(vqtbl2q_u8(st_rgba, vld1q_u8(pat_m0)));
		v0 = vorrq_u32(v0, vandq_u32(vceqzq_u32(v0), vld1q_u32(q_fixup)));

		const uint8x16_t xyzf = vld1q_u8(reinterpret_cast<const u8*>(r + 2));
		const uint8x16x2_t xyzf_pair = {xyzf, vreinterpretq_u8_u32(vshrq_n_u32(vreinterpretq_u32_u8(xyzf), 4))};
		uint32x4_t v1 = vreinterpretq_u32_u8(vqtbl2q_u8(xyzf_pair, vld1q_u8(pat_m1)));
		v1 = vsetq_lane_u32(uv, v1, 2);

		m0 = GSVector4i(vreinterpretq_s32_u32(v0));
		m1 = GSVector4i(vreinterpretq_s32_u32(v1));
	}

	__forceinline_odr void ParsePackedSTQRGBAXYZ2_Neon(const GIFPackedReg* RESTRICT r, u64 uvfog, GSVector4i& m0, GSVector4i& m1)
	{
		alignas(16) static constexpr u8 pat_m0[16] = {0, 1, 2, 3, 4, 5, 6, 7, 16, 20, 24, 28, 8, 9, 10, 11};
		// m1 low half = {X|Y<<16, Z32} straight out of r2; high half = {UV, FOG} verbatim.
		alignas(16) static constexpr u8 pat_xyz[16] = {0, 1, 4, 5, 8, 9, 10, 11, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
		alignas(16) static constexpr u32 q_fixup[4] = {0, 0, 0, 0x00800000};

		const uint8x16x2_t st_rgba = {vld1q_u8(reinterpret_cast<const u8*>(r + 0)), vld1q_u8(reinterpret_cast<const u8*>(r + 1))};
		uint32x4_t v0 = vreinterpretq_u32_u8(vqtbl2q_u8(st_rgba, vld1q_u8(pat_m0)));
		v0 = vorrq_u32(v0, vandq_u32(vceqzq_u32(v0), vld1q_u32(q_fixup)));

		const uint8x16_t xyz = vqtbl1q_u8(vld1q_u8(reinterpret_cast<const u8*>(r + 2)), vld1q_u8(pat_xyz));
		const uint64x2_t v1 = vsetq_lane_u64(uvfog, vreinterpretq_u64_u8(xyz), 1);

		m0 = GSVector4i(vreinterpretq_s32_u32(v0));
		m1 = GSVector4i(vreinterpretq_s32_u64(v1));
	}
#endif // ARCH_ARM64

	// Dispatchers the fused handlers call: aarch64 takes the TBL kernels, x86
	// keeps the legacy path.
	__forceinline_odr void ParsePackedSTQRGBAXYZF2_Fast(const GIFPackedReg* RESTRICT r, u32 uv, GSVector4i& m0, GSVector4i& m1)
	{
#ifdef ARCH_ARM64
		ParsePackedSTQRGBAXYZF2_Neon(r, uv, m0, m1);
#else
		ParsePackedSTQRGBAXYZF2(r, uv, m0, m1);
#endif
	}

	__forceinline_odr void ParsePackedSTQRGBAXYZ2_Fast(const GIFPackedReg* RESTRICT r, u64 uvfog, GSVector4i& m0, GSVector4i& m1)
	{
#ifdef ARCH_ARM64
		ParsePackedSTQRGBAXYZ2_Neon(r, uvfog, m0, m1);
#else
		ParsePackedSTQRGBAXYZ2(r, uvfog, m0, m1);
#endif
	}

	// Bounding box of one completed prim's window entries with the class rounding
	// applied — the bbox half of the legacy CullTest, shared by the scalar-outcode
	// fast path (which only needs it for accepted prims).
	template <u32 n, int primclass>
	__forceinline_odr GSVector4i ComputeCullBBox(const GSVector4i& v0, const GSVector4i& v1, const GSVector4i& v2,
		bool nativeres, bool aa1_expand)
	{
		GSVector4i bbox;
		if constexpr (n == 1)
		{
			bbox = v0;
		}
		else if constexpr (n == 2)
		{
			bbox = v0.runion(v1);
		}
		else
		{
			static_assert(n == 3);
			bbox = v0.runion(v1).runion(v2);
		}

		if constexpr (primclass == GS_TRIANGLE_CLASS || primclass == GS_SPRITE_CLASS)
		{
			if (nativeres)
			{
				// For triangles and sprites at native res take the interior pixel centers.
				const GSVector4i interior = (bbox + GSVector4i(0xF, 0xF, -1, -1)) & GSVector4i(~0xF);
				bbox = interior + GSVector4i(0, 0, 1, 1); // +1 to bottom/right so empty test works correctly.
			}
			else
			{
				// For upscaling, remove bottom/right subtexels.
				bbox -= ((bbox & GSVector4i(0xF)) == GSVector4i(0)) & GSVector4i(0, 0, 1, 1);
			}

			// For AA1 triangles and lines, expand the bounds by 1 pixel on all sides.
			if (aa1_expand)
			{
				bbox += GSVector4i(-0x10, -0x10, 0x10, 0x10);
			}
		}

		return bbox;
	}

	// Accept/cull test for one completed prim. v0/v1/v2 are the window entries for
	// the prim's vertices ({x, y, x, y} offset-subtracted 12.4 fixed-point, v0 most
	// recent), scissor_cull = context scissor in cull form. nativeres selects the
	// interior-pixel-center rounding; aa1_expand is the caller-evaluated
	// "PRIM->AA1 && IsCoverageAlphaSupported()" (only for triangle/sprite classes).
	// Returns nonzero to skip the prim; bbox receives the rounded bounding box the
	// accepted-prim draw_rect update consumes.
	template <u32 n, int primclass>
	__forceinline_odr u32 CullTest(const GSVector4i& v0, const GSVector4i& v1, const GSVector4i& v2,
		const GSVector4i& scissor_cull, bool nativeres, bool aa1_expand, GSVector4i& bbox)
	{
		bbox = ComputeCullBBox<n, primclass>(v0, v1, v2, nativeres, aa1_expand);

		// Do scissor test.
		const GSVector4i bbox_ex = bbox + GSVector4i(0, 0, 1, 1); // Exclusive coords for the scissor test.
		u32 test = static_cast<u32>(!bbox_ex.rintersects(scissor_cull));

		// Test for empty bbox.
		if constexpr (primclass == GS_TRIANGLE_CLASS || primclass == GS_SPRITE_CLASS)
		{
			test |= static_cast<u32>(bbox.rempty());
		}

		// Test for degenerate triangle.
		if constexpr (primclass == GS_TRIANGLE_CLASS)
		{
			test |= static_cast<u32>(v0.eq(v1)) | static_cast<u32>(v1.eq(v2)) | static_cast<u32>(v0.eq(v2));
		}

		return test;
	}

	// ------------------------------------------------------------------------
	// Scalar-outcode cull: exact scalar reformulation of CullTest for the cases
	// the fused handlers hit hottest (point/line always; triangle strips/lists
	// and sprites at native res without AA1 expansion). The NEON formulation
	// pays 3x umaxv + 2x uminp + 5x NEON->GPR moves of exposed latency per prim
	// on in-order cores; this one is a handful of ALU ops on per-vertex
	// precomputed metadata.
	//
	// Derivation (bit-exact, pinned by gs_vertex_tests):
	// - The prim bbox is the min/max of the vertices, so "bbox beyond scissor
	//   edge" == "every vertex beyond that edge": an AND of per-vertex 4-bit
	//   outcodes replaces the bbox build + saturate + empty chain.
	// - For triangle/sprite at native res, the ceil16/floor16 interior rounding
	//   and the cull rect's +/-8 fold into pixel-band bounds: with band(v) =
	//   (v-1)>>4, reject-left <=> all band(x) < (cull.x+14)>>4, reject-right <=>
	//   all band(x) >= (cull.z-1)>>4 (same for y). Point/line compare raw 12.4
	//   coords against cull directly (no rounding, no empty test).
	// - Interior-empty: since (v+15)>>4 == ((v-1)>>4)+1 identically,
	//   ceil16(min) > floor16strict(max) <=> all vertices share one band on
	//   that axis — a pure equality test on the packed bands.
	// - Degenerate triangle: the legacy 128-bit eq on {x,y,x,y} entries is xy
	//   equality — one u64 compare on the packed position.
	//
	// Window coords are offset-subtracted s32 (the offset subtract uses the full
	// 32-bit XYOFFSET lane, pad bits included, to match the NEON ring exactly), so
	// bands are stored as 28-bit fields — exact for any s32 coord, no truncation
	// aliasing.
	// ------------------------------------------------------------------------

	constexpr u64 kCullMetaBandXMask = 0xFFFFFFFull;
	constexpr u64 kCullMetaBandYMask = 0xFFFFFFFull << 28;
	constexpr u64 kCullMetaOutcodeMask = 0xFull << 56;

	// One mirror-ring slot: packed window position + derived cull metadata.
	// xyp = (u64)(u32)wy << 32 | (u32)wx; meta = bandx:28 | bandy:28 | outcode:4.
	struct CullMirrorEntry
	{
		u64 xyp;
		u64 meta;
	};

	// Pre-adjusted per-class scissor bounds, derived from scissor.cull whenever the
	// scissor changes. Outcode bits: 1 = out-left (< l), 2 = out-right (>= r),
	// 4 = out-top (< t), 8 = out-bottom (>= b).
	struct CullBounds
	{
		int l, t, r, b;
	};

	// Band-space bounds for triangle/sprite at native res (rounding folded in).
	__forceinline_odr CullBounds MakeBandedCullBounds(const GSVector4i& cull)
	{
		return {(cull.x + 14) >> 4, (cull.y + 14) >> 4, (cull.z - 1) >> 4, (cull.w - 1) >> 4};
	}

	// Raw 12.4 bounds for point/line (no rounding, exclusive bbox test folded in).
	__forceinline_odr CullBounds MakeRawCullBounds(const GSVector4i& cull)
	{
		return {cull.x, cull.y, cull.z, cull.w};
	}

	// Build one mirror entry from a vertex's window position. banded selects which
	// coordinate space the outcode compares in (bands for triangle/sprite native
	// res, raw 12.4 for point/line); bands are packed regardless so the entry
	// shape is uniform.
	template <bool banded>
	__forceinline_odr CullMirrorEntry MakeCullMirrorEntry(int wx, int wy, const CullBounds& bounds)
	{
		const int bx = (wx - 1) >> 4;
		const int by = (wy - 1) >> 4;
		const int cx = banded ? bx : wx;
		const int cy = banded ? by : wy;

		u32 oc = 0;
		oc |= (cx < bounds.l) ? 1u : 0u;
		oc |= (cx >= bounds.r) ? 2u : 0u;
		oc |= (cy < bounds.t) ? 4u : 0u;
		oc |= (cy >= bounds.b) ? 8u : 0u;

		CullMirrorEntry e;
		e.xyp = static_cast<u64>(static_cast<u32>(wx)) | (static_cast<u64>(static_cast<u32>(wy)) << 32);
		e.meta = (static_cast<u64>(static_cast<u32>(bx)) & kCullMetaBandXMask) |
		         ((static_cast<u64>(static_cast<u32>(by)) << 28) & kCullMetaBandYMask) |
		         (static_cast<u64>(oc) << 56);
		return e;
	}

	// The scalar decision. e0 is the most recent vertex. Unused entries (n < 3)
	// may alias e0. Bit-equivalent to CullTest's return under the fast-path gate
	// (point/line always; triangle non-fan / sprite when nativeres && !aa1).
	template <u32 n, int primclass>
	__forceinline_odr u32 CullTestScalar(const CullMirrorEntry& e0, const CullMirrorEntry& e1, const CullMirrorEntry& e2)
	{
		u64 all_out = e0.meta;
		if constexpr (n >= 2)
			all_out &= e1.meta;
		if constexpr (n == 3)
			all_out &= e2.meta;

		if ((all_out & kCullMetaOutcodeMask) != 0)
			return 1;

		if constexpr (primclass == GS_TRIANGLE_CLASS || primclass == GS_SPRITE_CLASS)
		{
			// Interior-empty: all vertices in one pixel band on either axis.
			u64 diff = e0.meta ^ e1.meta;
			if constexpr (n == 3)
				diff |= e0.meta ^ e2.meta;

			if ((diff & kCullMetaBandXMask) == 0 || (diff & kCullMetaBandYMask) == 0)
				return 1;
		}

		if constexpr (primclass == GS_TRIANGLE_CLASS)
		{
			if (e0.xyp == e1.xyp || e1.xyp == e2.xyp || e0.xyp == e2.xyp)
				return 1;
		}

		return 0;
	}

	// ------------------------------------------------------------------------
	// Fused vertex-trace bounds: accumulate GSVertexTraceFMM::FindMinMax's
	// min/max at index-emission time over each newly-referenced vertex (the data
	// is register/L1-hot in the kick), so the flush doesn't re-walk the index
	// list (strip vertices up to 3x redundant) with a non-pipelined FDIV per
	// vertex pair. The raw material is accumulated env-blind; the finish step
	// reproduces the legacy tail bit-exactly or declines (caller then runs the
	// legacy FindMinMax).
	//
	// Exactness notes (pinned by gs_vertex_tests):
	// - Each accumulate step performs the exact per-vertex op sequence the
	//   legacy walk performs for that vertex (same lane values through the same
	//   IEEE ops — the legacy pairs two vertices per 4-lane op, but per-lane
	//   the scalars are identical). min/max (and the NaN-blend-masked min/max
	//   of the STQ path, where a masked lane is an identity step) are
	//   idempotent, associative and commutative, so accumulating the referenced
	//   vertex SET once (dedup'd by the caller's watermark) equals the legacy
	//   walk over the index list with its duplicates, in any order.
	// - STQ (!FST) divides per new vertex at accumulate time — one 4-lane FDIV
	//   per unique vertex vs the legacy walk's one per index-list pair (strips
	//   reference vertices up to 3x) — and reproduces the legacy per-lane NaN
	//   masking and tnan reporting verbatim, so there are no decline cases.
	// ------------------------------------------------------------------------

	struct FmmAcc
	{
		GSVector4i pmin, pmax; // u32 min/max of {x, y, z, fog-word} (the legacy p vectors)
		GSVector4i tmin, tmax; // FST: u16 min/max of raw m[1] (elements 4/5 = U/V).
		                       // !FST: the legacy NaN-blend-masked float min/max chains
		                       //       over per-vertex {S/Q, T/Q, Q, Q}.
		GSVector4i tnan;       // !FST: accumulated per-lane NaN masks (legacy tnan)
		GSVector4i cmin, cmax; // u8 min/max of m[0] (bytes 8-11 = RGBA); flat shading
		                       // accumulates provoking vertices only.
	};

	// {x, y, z, fog-word} exactly as the legacy kernel builds its p vectors.
	__forceinline_odr GSVector4i FmmPos(const GSVector4i& m1)
	{
		return m1.upl16().blend32<0xc>(m1.ywyw());
	}

	__forceinline_odr void FmmAccReset(FmmAcc& a, bool tme, bool fst)
	{
		a.pmin = GSVector4i::xffffffff();
		a.pmax = GSVector4i::zero();
		if (tme && !fst)
		{
			a.tmin = GSVector4i::cast(GSVector4(FLT_MAX));
			a.tmax = GSVector4i::cast(GSVector4(-FLT_MAX));
		}
		else
		{
			a.tmin = GSVector4i::xffffffff();
			a.tmax = GSVector4i::zero();
		}
		a.tnan = GSVector4i::zero();
		a.cmin = GSVector4i::xffffffff();
		a.cmax = GSVector4i::zero();
	}

	// accumulate_color = iip || provoking, evaluated by the caller (flat shading
	// only takes the provoking vertex's color; the provoking vertex is always the
	// last-emitted index of the prim).
	__forceinline_odr void FmmAccumVertex(FmmAcc& a, const GSVector4i& m0, const GSVector4i& m1,
		bool tme, bool fst, bool accumulate_color)
	{
		const GSVector4i p = FmmPos(m1);
		a.pmin = a.pmin.min_u32(p);
		a.pmax = a.pmax.max_u32(p);

		if (tme)
		{
			if (fst)
			{
				a.tmin = a.tmin.min_u16(m1);
				a.tmax = a.tmax.max_u16(m1);
			}
			else
			{
				// Single-vertex transcription of the legacy STQ step: build
				// {S/Q, T/Q, Q, Q}, mask NaN lanes out of the min/max chains,
				// record them in tnan.
				const GSVector4 stq_raw = GSVector4::cast(m0);
				const GSVector4 stq = (stq_raw / stq_raw.wwww()).xyww(stq_raw);

				const GSVector4i nan = GSVector4i::cast(stq != stq);
				const GSVector4 keep = GSVector4::cast(~nan);

				GSVector4 tmin = GSVector4::cast(a.tmin);
				GSVector4 tmax = GSVector4::cast(a.tmax);
				a.tmin = GSVector4i::cast(tmin.blend32(tmin.min(stq), keep));
				a.tmax = GSVector4i::cast(tmax.blend32(tmax.max(stq), keep));
				a.tnan |= nan;
			}
		}

		if (accumulate_color)
		{
			a.cmin = a.cmin.min_u8(m0);
			a.cmax = a.cmax.max_u8(m0);
		}
	}

	struct FmmResult
	{
		GSVector4 min_p, max_p, min_t, max_t;
		GSVector4i min_c, max_c;
		u32 nan_value;  // only meaningful when write_nan
		bool write_nan; // legacy leaves vt.nan untouched for TME && FST draws
	};

	// Reproduce the legacy FindMinMax tail from the accumulators. tw/th are the
	// draw context's TEX0.TW/TH.
	__forceinline_odr void FmmFinish(const FmmAcc& a, bool tme, bool fst, bool color,
		const GIFRegXYOFFSET& ofs, u32 tw, u32 th, FmmResult& out)
	{
		out.write_nan = !(tme && fst);
		out.nan_value = 0;

		const GSVector4 o(ofs);
		const GSVector4 s(1.0f / 16, 1.0f / 16, 2.0f, 1.0f);

		out.min_p = (GSVector4(a.pmin) - o) * s;
		out.max_p = (GSVector4(a.pmax) - o) * s;

		// Fix signed int conversion of the Z lane, as the legacy tail does.
		out.min_p = out.min_p.insert32<0, 2>(GSVector4::load(static_cast<float>(static_cast<u32>(a.pmin.extract32<2>()))));
		out.max_p = out.max_p.insert32<0, 2>(GSVector4::load(static_cast<float>(static_cast<u32>(a.pmax.extract32<2>()))));

		if (tme)
		{
			if (fst)
			{
				// Legacy converts each vertex's {U, V} u16s to float and min/maxes
				// against FLT_MAX sentinels; u16 -> float is monotone and exact and
				// the sentinels never survive, so min-in-u16-then-convert matches.
				const GSVector4i uvmin(a.tmin.U16[4], a.tmin.U16[5], a.tmin.U16[4], a.tmin.U16[5]);
				const GSVector4i uvmax(a.tmax.U16[4], a.tmax.U16[5], a.tmax.U16[4], a.tmax.U16[5]);
				const GSVector4 sc = GSVector4(1.0f / 16, 1.0f).xxyy();
				out.min_t = GSVector4(uvmin) * sc;
				out.max_t = GSVector4(uvmax) * sc;
			}
			else
			{
				const GSVector4 sc = GSVector4(1 << static_cast<int>(tw), 1 << static_cast<int>(th), 1, 1);
				out.min_t = GSVector4::cast(a.tmin) * sc;
				out.max_t = GSVector4::cast(a.tmax) * sc;
				out.nan_value = static_cast<u32>(a.tnan.mask()) & ~4u;
			}
		}
		else
		{
			out.min_t = GSVector4::zero();
			out.max_t = GSVector4::zero();
		}

		if (color)
		{
			out.min_c = a.cmin.zzzz().u8to32();
			out.max_c = a.cmax.zzzz().u8to32();
		}
		else
		{
			out.min_c = GSVector4i::zero();
			out.max_c = GSVector4i::zero();
		}
	}
} // namespace GSVertexKernels
