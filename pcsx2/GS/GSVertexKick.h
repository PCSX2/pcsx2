// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/GSRegs.h"
#include "GS/GSVector.h"

#include <cfloat>

// Pure kernels backing the fused GIF packed vertex handlers and the per-prim
// accept/cull decision in GSState::VertexKick. Factored out of GSState.cpp so the
// gs_vertex_tests oracle suite can drive them standalone, and so optimized
// implementations can be crosschecked against the legacy ones per vertex/prim
// (GS_VERTEX_CROSSCHECK builds) without touching the surrounding state machine.
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
	// pinned by gs_vertex_tests and by GS_VERTEX_CROSSCHECK replay builds.
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

	// Dispatchers the fused handlers call: aarch64 takes the TBL kernels (optionally
	// crosschecked against the legacy ones per vertex), x86 keeps the legacy path.
	__forceinline_odr void ParsePackedSTQRGBAXYZF2_Fast(const GIFPackedReg* RESTRICT r, u32 uv, GSVector4i& m0, GSVector4i& m1)
	{
#ifdef ARCH_ARM64
		ParsePackedSTQRGBAXYZF2_Neon(r, uv, m0, m1);
#ifdef GS_VERTEX_CROSSCHECK
		GSVector4i c0, c1;
		ParsePackedSTQRGBAXYZF2(r, uv, c0, c1);
		pxAssertRel(c0.eq(m0) && c1.eq(m1), "GS_VERTEX_CROSSCHECK: packed XYZF2 parse divergence");
#endif
#else
		ParsePackedSTQRGBAXYZF2(r, uv, m0, m1);
#endif
	}

	__forceinline_odr void ParsePackedSTQRGBAXYZ2_Fast(const GIFPackedReg* RESTRICT r, u64 uvfog, GSVector4i& m0, GSVector4i& m1)
	{
#ifdef ARCH_ARM64
		ParsePackedSTQRGBAXYZ2_Neon(r, uvfog, m0, m1);
#ifdef GS_VERTEX_CROSSCHECK
		GSVector4i c0, c1;
		ParsePackedSTQRGBAXYZ2(r, uvfog, c0, c1);
		pxAssertRel(c0.eq(m0) && c1.eq(m1), "GS_VERTEX_CROSSCHECK: packed XYZ2 parse divergence");
#endif
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
} // namespace GSVertexKernels
