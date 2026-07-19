// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Oracle suite for the GS vertex front-end kernels (GS/GSVertexKick.h).
//
// Each kernel is checked against an independent scalar model of the documented
// GIF/GS semantics, over directed edge cases plus large randomized sweeps. The
// scalar models are deliberately written in plain integer C — no GSVector — so
// the vector kernels and the models can only agree by both being right. When
// GV-1/GV-3 land optimized kernel implementations, they are held to the same
// model (and to the legacy kernels via GS_VERTEX_CROSSCHECK replay builds).

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <random>

#include "GS/GSVertexKick.h"

namespace
{
	struct RefVertex
	{
		u32 m0[4];
		u32 m1[4];
	};

	u32 Rd32(const u8* rec, int dword)
	{
		u32 v;
		std::memcpy(&v, rec + dword * 4, 4);
		return v;
	}

	// Scalar model of packed {STQ, RGBAQ, XYZF2} parsing. rec = 48 bytes (3 qwords).
	RefVertex RefParseXYZF2(const u8* rec, u32 uv)
	{
		RefVertex o;
		o.m0[0] = Rd32(rec, 0); // S
		o.m0[1] = Rd32(rec, 1); // T
		o.m0[2] = (Rd32(rec, 4) & 0xff) | ((Rd32(rec, 5) & 0xff) << 8) |
		          ((Rd32(rec, 6) & 0xff) << 16) | ((Rd32(rec, 7) & 0xff) << 24); // RGBA
		const u32 q = Rd32(rec, 2);
		o.m0[3] = (q == 0) ? 0x00800000u : q; // integer-compare Q==+0.0 -> FLT_MIN; -0.0 passes

		o.m1[0] = (Rd32(rec, 8) & 0xffff) | ((Rd32(rec, 9) & 0xffff) << 16); // X | Y<<16
		o.m1[1] = (Rd32(rec, 10) >> 4) & 0x00ffffff; // Z (24-bit)
		o.m1[2] = uv;
		o.m1[3] = (Rd32(rec, 11) >> 4) & 0xff; // F
		return o;
	}

	// Scalar model of packed {STQ, RGBAQ, XYZ2} parsing: full 32-bit Z, UV and FOG
	// both preserved from the current vertex state.
	RefVertex RefParseXYZ2(const u8* rec, u64 uvfog)
	{
		RefVertex o = RefParseXYZF2(rec, static_cast<u32>(uvfog));
		o.m1[1] = Rd32(rec, 10); // Z, unshifted
		o.m1[3] = static_cast<u32>(uvfog >> 32); // FOG
		return o;
	}

	struct RefRect
	{
		int x, y, z, w;
	};

	// Scalar model of the legacy accept/cull decision. Window entries are the
	// degenerate rects {x, y, x, y}; scissor is the cull-form rect.
	u32 RefCullTest(int n, int primclass, const RefRect& v0, const RefRect& v1, const RefRect& v2,
		const RefRect& scissor, bool nativeres, bool aa1_expand)
	{
		auto runion = [](const RefRect& a, const RefRect& b) {
			return RefRect{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.z, b.z), std::max(a.w, b.w)};
		};

		RefRect bbox = v0;
		if (n >= 2)
			bbox = runion(bbox, v1);
		if (n >= 3)
			bbox = runion(bbox, v2);

		const bool rounded = (primclass == GS_TRIANGLE_CLASS || primclass == GS_SPRITE_CLASS);
		if (rounded)
		{
			if (nativeres)
			{
				bbox.x = (bbox.x + 0xF) & ~0xF;
				bbox.y = (bbox.y + 0xF) & ~0xF;
				bbox.z = ((bbox.z - 1) & ~0xF) + 1;
				bbox.w = ((bbox.w - 1) & ~0xF) + 1;
			}
			else
			{
				// mask {0,0,1,1}: only the bottom/right lanes can be decremented
				bbox.z -= ((bbox.z & 0xF) == 0) ? 1 : 0;
				bbox.w -= ((bbox.w & 0xF) == 0) ? 1 : 0;
			}

			if (aa1_expand)
			{
				bbox.x -= 0x10;
				bbox.y -= 0x10;
				bbox.z += 0x10;
				bbox.w += 0x10;
			}
		}

		const RefRect ex{bbox.x, bbox.y, bbox.z + 1, bbox.w + 1};
		// !rintersects: intersection {max,max,min,min} empty (x>=z || y>=w)
		u32 test = (std::max(ex.x, scissor.x) >= std::min(ex.z, scissor.z) ||
					   std::max(ex.y, scissor.y) >= std::min(ex.w, scissor.w)) ?
		               1 :
		               0;

		if (rounded)
			test |= (bbox.x >= bbox.z || bbox.y >= bbox.w) ? 1 : 0;

		if (primclass == GS_TRIANGLE_CLASS)
		{
			auto eq = [](const RefRect& a, const RefRect& b) { return a.x == b.x && a.y == b.y; };
			test |= (eq(v0, v1) || eq(v1, v2) || eq(v0, v2)) ? 1 : 0;
		}

		return test;
	}

	GSVector4i WindowEntry(int x, int y)
	{
		return GSVector4i(x, y, x, y);
	}

	void ExpectVertexEq(const RefVertex& ref, const GSVector4i& m0, const GSVector4i& m1, const char* what)
	{
		for (int i = 0; i < 4; i++)
		{
			EXPECT_EQ(ref.m0[i], m0.U32[i]) << what << " m0 lane " << i;
			EXPECT_EQ(ref.m1[i], m1.U32[i]) << what << " m1 lane " << i;
		}
	}
} // namespace

TEST(GsVertexParse, Xyzf2DirectedEdges)
{
	// Q == +0.0 rewrites to FLT_MIN; Q == -0.0 (0x80000000) must pass through
	// untouched (the rewrite is an integer compare).
	alignas(16) u8 rec[48] = {};
	const u32 minus_zero = 0x80000000u;
	std::memcpy(rec + 8, &minus_zero, 4); // r[0].U32[2] = Q

	GSVector4i m0, m1;
	GSVertexKernels::ParsePackedSTQRGBAXYZF2(reinterpret_cast<const GIFPackedReg*>(rec), 0xdeadbeefu, m0, m1);
	EXPECT_EQ(minus_zero, m0.U32[3]) << "-0.0 Q must not be rewritten";
	EXPECT_EQ(0xdeadbeefu, m1.U32[2]) << "UV must be preserved";

	std::memset(rec + 8, 0, 4); // Q = +0.0
	GSVertexKernels::ParsePackedSTQRGBAXYZF2(reinterpret_cast<const GIFPackedReg*>(rec), 0, m0, m1);
	EXPECT_EQ(0x00800000u, m0.U32[3]) << "+0.0 Q must become FLT_MIN";

	// RGBA byte gather ignores the upper 24 bits of each dword; Z/F take the >>4
	// shift and their masks.
	alignas(16) u8 rec2[48];
	for (size_t i = 0; i < sizeof(rec2); i++)
		rec2[i] = static_cast<u8>(0xA0 + i);
	const RefVertex ref = RefParseXYZF2(rec2, 0x11223344u);
	GSVertexKernels::ParsePackedSTQRGBAXYZF2(reinterpret_cast<const GIFPackedReg*>(rec2), 0x11223344u, m0, m1);
	ExpectVertexEq(ref, m0, m1, "patterned record");
}

TEST(GsVertexParse, Xyzf2RandomSweep)
{
	std::mt19937_64 rng(0x67766f31); // fixed seed: deterministic suite
	alignas(16) u8 rec[48];

	for (int iter = 0; iter < 1000000; iter++)
	{
		for (int i = 0; i < 6; i++)
		{
			const u64 v = rng();
			std::memcpy(rec + i * 8, &v, 8);
		}
		// Bias some iterations toward the Q==0 edge.
		if ((iter & 15) == 0)
			std::memset(rec + 8, 0, 4);

		const u32 uv = static_cast<u32>(rng());
		const RefVertex ref = RefParseXYZF2(rec, uv);

		GSVector4i m0, m1;
		GSVertexKernels::ParsePackedSTQRGBAXYZF2(reinterpret_cast<const GIFPackedReg*>(rec), uv, m0, m1);

		ASSERT_TRUE(std::memcmp(ref.m0, &m0, 16) == 0 && std::memcmp(ref.m1, &m1, 16) == 0)
			<< "XYZF2 divergence at iter " << iter;
	}
}

TEST(GsVertexParse, Xyz2RandomSweep)
{
	std::mt19937_64 rng(0x67766f32);
	alignas(16) u8 rec[48];

	for (int iter = 0; iter < 1000000; iter++)
	{
		for (int i = 0; i < 6; i++)
		{
			const u64 v = rng();
			std::memcpy(rec + i * 8, &v, 8);
		}
		if ((iter & 15) == 0)
			std::memset(rec + 8, 0, 4);

		const u64 uvfog = rng();
		const RefVertex ref = RefParseXYZ2(rec, uvfog);

		GSVector4i m0, m1;
		GSVertexKernels::ParsePackedSTQRGBAXYZ2(reinterpret_cast<const GIFPackedReg*>(rec), uvfog, m0, m1);

		ASSERT_TRUE(std::memcmp(ref.m0, &m0, 16) == 0 && std::memcmp(ref.m1, &m1, 16) == 0)
			<< "XYZ2 divergence at iter " << iter;
	}
}

#ifdef ARCH_ARM64
TEST(GsVertexParse, Xyzf2NeonRandomSweep)
{
	std::mt19937_64 rng(0x67766f33);
	alignas(16) u8 rec[48];

	for (int iter = 0; iter < 1000000; iter++)
	{
		for (int i = 0; i < 6; i++)
		{
			const u64 v = rng();
			std::memcpy(rec + i * 8, &v, 8);
		}
		if ((iter & 15) == 0)
			std::memset(rec + 8, 0, 4);

		const u32 uv = static_cast<u32>(rng());
		const RefVertex ref = RefParseXYZF2(rec, uv);

		GSVector4i m0, m1;
		GSVertexKernels::ParsePackedSTQRGBAXYZF2_Neon(reinterpret_cast<const GIFPackedReg*>(rec), uv, m0, m1);

		ASSERT_TRUE(std::memcmp(ref.m0, &m0, 16) == 0 && std::memcmp(ref.m1, &m1, 16) == 0)
			<< "NEON XYZF2 divergence at iter " << iter;
	}
}

TEST(GsVertexParse, Xyz2NeonRandomSweep)
{
	std::mt19937_64 rng(0x67766f34);
	alignas(16) u8 rec[48];

	for (int iter = 0; iter < 1000000; iter++)
	{
		for (int i = 0; i < 6; i++)
		{
			const u64 v = rng();
			std::memcpy(rec + i * 8, &v, 8);
		}
		if ((iter & 15) == 0)
			std::memset(rec + 8, 0, 4);

		const u64 uvfog = rng();
		const RefVertex ref = RefParseXYZ2(rec, uvfog);

		GSVector4i m0, m1;
		GSVertexKernels::ParsePackedSTQRGBAXYZ2_Neon(reinterpret_cast<const GIFPackedReg*>(rec), uvfog, m0, m1);

		ASSERT_TRUE(std::memcmp(ref.m0, &m0, 16) == 0 && std::memcmp(ref.m1, &m1, 16) == 0)
			<< "NEON XYZ2 divergence at iter " << iter;
	}
}
#endif // ARCH_ARM64

namespace
{
	template <u32 n, int primclass>
	void RunCullSweep(u64 seed, int iters)
	{
		std::mt19937_64 rng(seed);

		// Offset-subtracted 12.4 coords land in roughly [-0x10000, 0x10000); mix a
		// full-range band with a narrow band so equality/16-boundary collisions and
		// tiny-prim cases actually occur.
		auto coord = [&](int band) {
			if (band == 0)
				return static_cast<int>(rng() % 0x20000) - 0x10000;
			return static_cast<int>(rng() % 0x40) - 0x20;
		};

		for (int iter = 0; iter < iters; iter++)
		{
			const int band = iter & 1;
			int x0 = coord(band), y0 = coord(band);
			int x1 = coord(band), y1 = coord(band);
			int x2 = coord(band), y2 = coord(band);
			// Force exact-duplicate vertices regularly (strip restarts do this).
			if ((iter & 7) == 0)
			{
				x1 = x0;
				y1 = y0;
			}
			if ((iter & 31) == 0)
			{
				x2 = x1;
				y2 = y1;
			}
			// Force 16-aligned coords regularly (the interior-rounding edge).
			if ((iter & 3) == 0)
			{
				x0 &= ~0xF;
				y2 &= ~0xF;
			}

			int sx = coord(0), sy = coord(0);
			int sz = sx + static_cast<int>(rng() % 0x8000);
			int sw = sy + static_cast<int>(rng() % 0x8000);

			const bool nativeres = (rng() & 1) != 0;
			const bool aa1 = (rng() & 7) == 0;

			const RefRect rv0{x0, y0, x0, y0}, rv1{x1, y1, x1, y1}, rv2{x2, y2, x2, y2};
			const RefRect rs{sx, sy, sz, sw};
			const u32 expected =
				RefCullTest(n, primclass, rv0, rv1, rv2, rs, nativeres, aa1) ? 1u : 0u;

			GSVector4i bbox;
			const u32 got = GSVertexKernels::CullTest<n, primclass>(WindowEntry(x0, y0), WindowEntry(x1, y1),
				WindowEntry(x2, y2), GSVector4i(sx, sy, sz, sw), nativeres, aa1, bbox);

			ASSERT_EQ(expected, got != 0 ? 1u : 0u)
				<< "cull divergence at iter " << iter << " n=" << n << " class=" << primclass
				<< " v0=(" << x0 << "," << y0 << ") v1=(" << x1 << "," << y1 << ") v2=(" << x2 << "," << y2
				<< ") scissor=(" << sx << "," << sy << "," << sz << "," << sw << ") native=" << nativeres
				<< " aa1=" << aa1;
		}
	}
} // namespace

namespace
{
	// Scalar-outcode cull vs the legacy kernel (itself pinned against RefCullTest
	// above). Production-shaped inputs: scissor cull rects are 16k-8 .. 16k+8 with
	// SCAX0 <= SCAX1 (empty scissors take the m_scissor_invalid path and never
	// reach the cull decision), coords are offset-subtracted 12.4. The fast-path
	// gate is baked in: banded classes run nativeres, no AA1.
	template <u32 n, int primclass>
	void RunScalarCullSweep(u64 seed, int iters)
	{
		constexpr bool banded = (primclass == GS_TRIANGLE_CLASS || primclass == GS_SPRITE_CLASS);
		std::mt19937_64 rng(seed);

		for (int iter = 0; iter < iters; iter++)
		{
			// GS-shaped scissor: registers in [0, 2047], min <= max.
			int sax0 = static_cast<int>(rng() % 2048), sax1 = static_cast<int>(rng() % 2048);
			int say0 = static_cast<int>(rng() % 2048), say1 = static_cast<int>(rng() % 2048);
			if (sax0 > sax1)
				std::swap(sax0, sax1);
			if (say0 > say1)
				std::swap(say0, say1);
			const GSVector4i cull(sax0 * 16 - 8, say0 * 16 - 8, sax1 * 16 + 8, say1 * 16 + 8);

			// Coords: full range / narrow cluster / snapped near a scissor edge so
			// the band boundaries and the +/-8 cull margin both get exercised.
			auto coord = [&](int lo16, int hi16) {
				switch (rng() % 4)
				{
					case 0:
						return static_cast<int>(rng() % 0x20000) - 0x10000;
					case 1:
						return static_cast<int>(rng() % 0x40) - 0x20;
					case 2:
						return lo16 + static_cast<int>(rng() % 41) - 20;
					default:
						return hi16 + static_cast<int>(rng() % 41) - 20;
				}
			};

			int x0 = coord(sax0 * 16, sax1 * 16), y0 = coord(say0 * 16, say1 * 16);
			int x1 = coord(sax0 * 16, sax1 * 16), y1 = coord(say0 * 16, say1 * 16);
			int x2 = coord(sax0 * 16, sax1 * 16), y2 = coord(say0 * 16, say1 * 16);
			if ((iter & 7) == 0)
			{
				x1 = x0;
				y1 = y0;
			}
			if ((iter & 31) == 0)
			{
				x2 = x1;
				y2 = y1;
			}
			if ((iter & 3) == 0)
			{
				x0 &= ~0xF;
				y2 &= ~0xF;
			}

			GSVector4i bbox;
			const u32 expected = GSVertexKernels::CullTest<n, primclass>(WindowEntry(x0, y0), WindowEntry(x1, y1),
				WindowEntry(x2, y2), cull, banded ? true : ((rng() & 1) != 0), false, bbox);

			const GSVertexKernels::CullBounds bounds =
				banded ? GSVertexKernels::MakeBandedCullBounds(cull) : GSVertexKernels::MakeRawCullBounds(cull);
			const GSVertexKernels::CullMirrorEntry e0 = GSVertexKernels::MakeCullMirrorEntry<banded>(x0, y0, bounds);
			const GSVertexKernels::CullMirrorEntry e1 = GSVertexKernels::MakeCullMirrorEntry<banded>(x1, y1, bounds);
			const GSVertexKernels::CullMirrorEntry e2 = GSVertexKernels::MakeCullMirrorEntry<banded>(x2, y2, bounds);

			const u32 got = GSVertexKernels::CullTestScalar<n, primclass>(e0, e1, e2);

			ASSERT_EQ(expected != 0 ? 1u : 0u, got != 0 ? 1u : 0u)
				<< "scalar cull divergence at iter " << iter << " n=" << n << " class=" << primclass
				<< " v0=(" << x0 << "," << y0 << ") v1=(" << x1 << "," << y1 << ") v2=(" << x2 << "," << y2
				<< ") scissor=(" << sax0 << "," << say0 << "," << sax1 << "," << say1 << ")";
		}
	}
} // namespace

TEST(GsVertexCull, TriangleSweep)
{
	RunCullSweep<3, GS_TRIANGLE_CLASS>(0x67763301, 1000000);
}

TEST(GsVertexCull, SpriteSweep)
{
	RunCullSweep<2, GS_SPRITE_CLASS>(0x67763302, 500000);
}

TEST(GsVertexCull, LineSweep)
{
	RunCullSweep<2, GS_LINE_CLASS>(0x67763303, 500000);
}

TEST(GsVertexCull, ScalarTriangleSweep)
{
	RunScalarCullSweep<3, GS_TRIANGLE_CLASS>(0x67763311, 1000000);
}

TEST(GsVertexCull, ScalarSpriteSweep)
{
	RunScalarCullSweep<2, GS_SPRITE_CLASS>(0x67763312, 500000);
}

TEST(GsVertexCull, ScalarLineSweep)
{
	RunScalarCullSweep<2, GS_LINE_CLASS>(0x67763313, 500000);
}

TEST(GsVertexCull, ScalarPointSweep)
{
	RunScalarCullSweep<1, GS_POINT_CLASS>(0x67763314, 500000);
}

TEST(GsVertexCull, PointSweep)
{
	RunCullSweep<1, GS_POINT_CLASS>(0x67763304, 500000);
}
