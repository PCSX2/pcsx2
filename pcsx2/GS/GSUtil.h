// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS.h"
#include "GSRegs.h"
#include "GS/Renderers/Common/GSVertex.h"

class GSUtil
{
public:
	static const char* GetATSTName(u32 atst);
	static const char* GetAFAILName(u32 afail);
	static const char* GetPSMName(int psm);
	static const char* GetWMName(u32 wm);
	static const char* GetZTSTName(u32 ztst);
	static const char* GetPrimName(u32 prim);
	static const char* GetPrimClassName(u32 primclass);
	static const char* GetMMAGName(u32 mmag);
	static const char* GetMMINName(u32 mmin);
	static const char* GetMTBAName(u32 mtba);
	static const char* GetLCMName(u32 lcm);
	static const char* GetSCANMSKName(u32 scanmsk);
	static const char* GetDATMName(u32 datm);
	static const char* GetTFXName(u32 tfx);
	static const char* GetTCCName(u32 tcc);
	static const char* GetACName(u32 ac);

	static const u32* HasSharedBitsPtr(u32 dpsm);
	static bool HasSharedBits(u32 spsm, const u32* ptr);
	static bool HasSharedBits(u32 spsm, u32 dpsm);
	static bool HasSharedBits(u32 sbp, u32 spsm, u32 dbp, u32 dpsm);
	static bool HasCompatibleBits(u32 spsm, u32 dpsm);
	static bool HasSameSwizzleBits(u32 spsm, u32 dpsm);
	static u32 GetChannelMask(u32 spsm);
	static u32 GetChannelMask(u32 spsm, u32 fbmsk);

	static GSRendererType GetPreferredRenderer();

	static constexpr GS_PRIM_CLASS GetPrimClass(u32 prim)
	{
		switch (prim)
		{
			case GS_POINTLIST:
				return GS_POINT_CLASS;
			case GS_LINELIST:
			case GS_LINESTRIP:
				return GS_LINE_CLASS;
			case GS_TRIANGLELIST:
			case GS_TRIANGLESTRIP:
			case GS_TRIANGLEFAN:
				return GS_TRIANGLE_CLASS;
			case GS_SPRITE:
				return GS_SPRITE_CLASS;
			default:
				return GS_INVALID_CLASS;
		}
	}

	static constexpr int GetClassVertexCount(u32 primclass)
	{
		switch (primclass)
		{
			case GS_POINT_CLASS:    return 1;
			case GS_LINE_CLASS:     return 2;
			case GS_TRIANGLE_CLASS: return 3;
			case GS_SPRITE_CLASS:   return 2;
			default:                return -1;
		}
	}

	static constexpr int GetVertexCount(u32 prim)
	{
		return GetClassVertexCount(GetPrimClass(prim));
	}

	// For returning order of vertices to form a right triangle
	struct TriangleOrdering
	{
		// Describes a right triangle laid out in one of the following orientations
		// b   c | c  b | a     |     a
		// a     |    a | b   c | c   b
		u32 a; // Same x as b
		u32 b; // Same x as a, same y as c
		u32 c; // Same y as b
	};

	struct alignas(2) TriangleOrderingBC
	{
		u8 b;
		u8 c;
	};

	alignas(16) static constexpr TriangleOrderingBC triangle_order_lut[6] =
	{
		TriangleOrderingBC{/*a=0,*/ 1, 2},
		TriangleOrderingBC{/*a=0,*/ 2, 1},
		TriangleOrderingBC{/*a=1,*/ 0, 2},
		TriangleOrderingBC{/*a=1,*/ 2, 0},
		TriangleOrderingBC{/*a=2,*/ 0, 1},
		TriangleOrderingBC{/*a=2,*/ 1, 0},
	};

	// Helper struct for IsTriangleRight and AreTrianglesRight
	static constexpr u8 TriangleFinalCmp(u8 value) { return value & 3; }
		
	static constexpr TriangleOrdering TriangleFinalOrder(u8 value)
	{
		u32 order = static_cast<u32>(value) >> 2;
		TriangleOrderingBC bc = triangle_order_lut[order];
		return {order >> 1, bc.b, bc.c};
	}

	// Helper table for IsTriangleRight/AreTrianglesRight functions
	static constexpr u8 triangle_comparison_lut[16] =
	{
		0 | (0 << 2), // 0000 => None equal, no sprite possible
		2 | (0 << 2), // 0001 => x0 = x1, requires y1 = y2
		1 | (5 << 2), // 0010 => y0 = y1, requires x1 = x2
		2 | (0 << 2), // 0011 => x0 = x1, y0 = y1, (no area) requires x1 = x2 or y1 = y2
		2 | (1 << 2), // 0100 => x0 = x2, requires y1 = y2
		2 | (0 << 2), // 0101 => x0 = x1, x0 = x2, (no area) requires y1 = y2
		0 | (4 << 2), // 0110 => y0 = y1, x0 = x2, requires nothing
		0 | (4 << 2), // 0111 => x0 = y1, y0 = y1, x0 = x2, (no area) requires nothing
		1 | (3 << 2), // 1000 => y0 = y2, requires x1 = x2
		0 | (2 << 2), // 1001 => x0 = x1, y0 = y2, requires nothing
		1 | (3 << 2), // 1010 => y0 = y1, y0 = y2, (no area) requires x1 = x2
		0 | (2 << 2), // 1011 => x0 = x1, y0 = y1, y0 = y2, (unlikely) requires nothing
		2 | (1 << 2), // 1100 => x0 = x2, y0 = y2, (no area) requires x1 = x2 or y1 = y2
		0 | (2 << 2), // 1101 => x0 = x1, x0 = x2, y0 = y2, (no area) requires nothing
		0 | (4 << 2), // 1110 => y0 = y1, x0 = x2, y0 = y2, (no area) requires nothing
		0 | (2 << 2), // 1111 => x0 = x1, y0 = y1, x0 = x2, y0 = y2, (no area) requires nothing
	};

	// Determines ordering of two triangles in parallel if both are right.
	// More efficient than calling IsTriangleRight twice.
	template <u32 tme, u32 fst>
	__forceinline static bool AreTrianglesRight(const GSVertex* RESTRICT vin, const u16* RESTRICT index0, const u16* RESTRICT index1,
		TriangleOrdering* out_triangle0, TriangleOrdering* out_triangle1)
	{
		GSVector4i mask;
		if (tme && fst)
		{
			// Compare xy and uv together
			mask = GSVector4i::cxpr8(
				(s8)0, (s8)1, (s8)8,  (s8)9,
				(s8)2, (s8)3, (s8)10, (s8)11,
				(s8)0, (s8)1, (s8)8,  (s8)9,
				(s8)2, (s8)3, (s8)10, (s8)11);
		}
		else
		{
			// ignore uv, compare st instead later
			mask = GSVector4i::cxpr8(
				(s8)0, (s8)1, (s8)0x80, (s8)0x80,
				(s8)2, (s8)3, (s8)0x80, (s8)0x80,
				(s8)0, (s8)1, (s8)0x80, (s8)0x80,
				(s8)2, (s8)3, (s8)0x80, (s8)0x80);
		}
		GSVector4i xy0 = GSVector4i(vin[index0[0]].m[1]).shuffle8(mask); // Triangle 0 vertex 0
		GSVector4i xy1 = GSVector4i(vin[index0[1]].m[1]).shuffle8(mask); // Triangle 0 vertex 1
		GSVector4i xy2 = GSVector4i(vin[index0[2]].m[1]).shuffle8(mask); // Triangle 0 vertex 2
		GSVector4i xy3 = GSVector4i(vin[index1[0]].m[1]).shuffle8(mask); // Triangle 1 vertex 0
		GSVector4i xy4 = GSVector4i(vin[index1[1]].m[1]).shuffle8(mask); // Triangle 1 vertex 1
		GSVector4i xy5 = GSVector4i(vin[index1[2]].m[1]).shuffle8(mask); // Triangle 1 vertex 2
		GSVector4i vcmp0 = xy0.eq32(xy1.upl64(xy2));
		GSVector4i vcmp1 = xy3.eq32(xy4.upl64(xy5));
		GSVector4i vcmp2 = xy1.upl64(xy4).eq32(xy2.upl64(xy5));
		if (tme && !fst)
		{
			// do the st comparisons
			GSVector4 st0 = GSVector4::cast(GSVector4i(vin[index0[0]].m[0]));
			GSVector4 st1 = GSVector4::cast(GSVector4i(vin[index0[1]].m[0]));
			GSVector4 st2 = GSVector4::cast(GSVector4i(vin[index0[2]].m[0]));
			GSVector4 st3 = GSVector4::cast(GSVector4i(vin[index1[0]].m[0]));
			GSVector4 st4 = GSVector4::cast(GSVector4i(vin[index1[1]].m[0]));
			GSVector4 st5 = GSVector4::cast(GSVector4i(vin[index1[2]].m[0]));

			vcmp0 = vcmp0 & GSVector4i::cast(st0.xyxy() == st1.upld(st2));
			vcmp1 = vcmp1 & GSVector4i::cast(st3.xyxy() == st4.upld(st5));
			vcmp2 = vcmp2 & GSVector4i::cast(st1.upld(st4) == st2.upld(st5));
		}
		int cmp0 = GSVector4::cast(vcmp0).mask();
		int cmp1 = GSVector4::cast(vcmp1).mask();
		int cmp2 = GSVector4::cast(vcmp2).mask();
		if (!cmp0 || !cmp1) // Either triangle 0 or triangle 1 isn't a right triangle
			return false;
		u8 triangle0cmp = triangle_comparison_lut[cmp0];
		u8 triangle1cmp = triangle_comparison_lut[cmp1];
		int required_cmp2 = TriangleFinalCmp(triangle0cmp) | (TriangleFinalCmp(triangle1cmp) << 2);
		if ((cmp2 & required_cmp2) != required_cmp2)
			return false;
		// Both t0 and t1 are right triangles!
		*out_triangle0 = TriangleFinalOrder(triangle0cmp);
		*out_triangle1 = TriangleFinalOrder(triangle1cmp);
		return true;
	}
	
	template <u32 tme, u32 fst>
	__forceinline static bool IsTriangleRight(const GSVertex* RESTRICT vin, const u16* RESTRICT index, TriangleOrdering* out_triangle)
	{
		GSVector4i mask;
		if (tme && fst)
		{
			// Compare xy and uv together
			mask = GSVector4i::cxpr8(
				(s8)0, (s8)1, (s8)8, (s8)9,
				(s8)2, (s8)3, (s8)10, (s8)11,
				(s8)0, (s8)1, (s8)8, (s8)9,
				(s8)2, (s8)3, (s8)10, (s8)11);
		}
		else
		{
			// ignore uv, compare st instead later
			mask = GSVector4i::cxpr8(
				(s8)0, (s8)1, (s8)0x80, (s8)0x80,
				(s8)2, (s8)3, (s8)0x80, (s8)0x80,
				(s8)0, (s8)1, (s8)0x80, (s8)0x80,
				(s8)2, (s8)3, (s8)0x80, (s8)0x80);
		}
		GSVector4i xy0 = GSVector4i(vin[index[0]].m[1]).shuffle8(mask); // Triangle 0 vertex 0
		GSVector4i xy1 = GSVector4i(vin[index[1]].m[1]).shuffle8(mask); // Triangle 0 vertex 1
		GSVector4i xy2 = GSVector4i(vin[index[2]].m[1]).shuffle8(mask); // Triangle 0 vertex 2
		GSVector4i vcmp0 = xy0.eq32(xy1.upl64(xy2));
		GSVector4i vcmp1 = xy1.eq32(xy2); // ignore top 64 bits
		if (tme && !fst)
		{
			// do the st comparisons
			GSVector4 st0 = GSVector4::cast(GSVector4i(vin[index[0]].m[0]));
			GSVector4 st1 = GSVector4::cast(GSVector4i(vin[index[1]].m[0]));
			GSVector4 st2 = GSVector4::cast(GSVector4i(vin[index[2]].m[0]));

			vcmp0 = vcmp0 & GSVector4i::cast(st0.xyxy() == st1.upld(st2));
			vcmp1 = vcmp1 & GSVector4i::cast(st1 == st2); // ignore top 64 bits
		}
		int cmp0 = GSVector4::cast(vcmp0).mask();
		int cmp1 = GSVector4::cast(vcmp1).mask() & 0x3;
		if (!cmp0) // Either triangle 0 or triangle 1 isn't a right triangle
			return false;
		u8 trianglecmp = triangle_comparison_lut[cmp0];
		int required_cmp1 = TriangleFinalCmp(trianglecmp);
		if (cmp1 != required_cmp1)
			return false;
		// Both t0 and t1 are right triangles!
		*out_triangle = TriangleFinalOrder(trianglecmp);
		return true;
	}
	
	// Determines whether the triangle are right and form a quad
	template <u32 tme, u32 fst>
	__forceinline static bool AreTrianglesQuad(const GSVertex* RESTRICT vin, const u16* RESTRICT index0, const u16* RESTRICT index1,
		TriangleOrdering* out_triangle0, TriangleOrdering* out_triangle1)
	{
		if (!AreTrianglesRight<tme, fst>(vin, index0, index1, out_triangle0, out_triangle1))
			return false;

		// The two triangles are now laid out in one of these four orderings:
		// b   c | c  b | a     |     a
		// a     |    a | b   c | c   b
		// To form a quad we must have a0 == c1 and a1 == c0
		bool are_quad = vin[index0[out_triangle0->a]].XYZ.U32[0] == vin[index1[out_triangle1->c]].XYZ.U32[0] &&
		                vin[index0[out_triangle0->c]].XYZ.U32[0] == vin[index1[out_triangle1->a]].XYZ.U32[0];

		if (tme)
		{
			if (fst)
			{
				const u32 uv_a0 = vin[index0[out_triangle0->a]].UV;
				const u32 uv_c0 = vin[index0[out_triangle0->c]].UV;
				const u32 uv_a1 = vin[index1[out_triangle1->a]].UV;
				const u32 uv_c1 = vin[index1[out_triangle1->c]].UV;
				are_quad = are_quad && uv_a0 == uv_c1 && uv_c0 == uv_a1;
			}
			else
			{
				const u64 st_a0 = vin[index0[out_triangle0->a]].ST.U64;
				const u64 st_c0 = vin[index0[out_triangle0->c]].ST.U64;
				const u64 st_a1 = vin[index1[out_triangle1->a]].ST.U64;
				const u64 st_c1 = vin[index1[out_triangle1->c]].ST.U64;
				are_quad = are_quad && st_a0 == st_c1 && st_c0 == st_a1;
			}
		}
		return are_quad;
	}

	__forceinline static bool AreTrianglesQuadNonAA(const GSVertex* RESTRICT vin, const u16* RESTRICT index0, const u16* index1)
	{
		u32 v0[3] = {
			vin[index0[0]].XYZ.U32[0],
			vin[index0[1]].XYZ.U32[0],
			vin[index0[2]].XYZ.U32[0],
		};

		u32 v1[3] = {
			vin[index1[0]].XYZ.U32[0],
			vin[index1[1]].XYZ.U32[0],
			vin[index1[2]].XYZ.U32[0],
		};

		// Pack vertices to represent edges XY are stored in a single u32. Reverse the order
		// for some of the fields to allow check for different vertex order in the same instruction.
		GSVector4i e0[3] = {
			GSVector4i(v0[0], v0[1]).xyxy(),
			GSVector4i(v0[1], v0[2]).xyxy(),
			GSVector4i(v0[2], v0[0]).xyxy(),
		};
		GSVector4i e1[3] = {
			GSVector4i(v1[0], v1[1]).xyyx(),
			GSVector4i(v1[1], v1[2]).xyyx(),
			GSVector4i(v1[2], v1[0]).xyyx(),
		};

		// Hope this is unrolled.
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				const int m = (e0[i] == e1[j]).mask();
				if (m == 0x00FF || m == 0xFF00)
				{
					// Shared vertices
					const int xs0 = static_cast<int>((v0[(i + 0) % 3] >> 0) & 0xFFFF);
					const int ys0 = static_cast<int>((v0[(i + 0) % 3] >> 16) & 0xFFFF);
					const int xs1 = static_cast<int>((v0[(i + 1) % 3] >> 0) & 0xFFFF);
					const int ys1 = static_cast<int>((v0[(i + 1) % 3] >> 16) & 0xFFFF);

					// Non-shared vertices
					const int xn0 = static_cast<int>((v0[(i + 2) % 3] >> 0) & 0xFFFF);
					const int yn0 = static_cast<int>((v0[(i + 2) % 3] >> 16) & 0xFFFF);
					const int xn1 = static_cast<int>((v1[(j + 2) % 3] >> 0) & 0xFFFF);
					const int yn1 = static_cast<int>((v1[(j + 2) % 3] >> 16) & 0xFFFF);

					// Deltas of the edges
					const int dxs = xs1 - xs0;
					const int dys = ys1 - ys0;

					const int dx0 = xn0 - xs0;
					const int dy0 = yn0 - ys0;

					const int dx1 = xn1 - xs0;
					const int dy1 = yn1 - ys0;

					// Cross products
					const int cross0 = dx0 * dys - dy0 * dxs;
					const int cross1 = dx1 * dys - dy1 * dxs;

					// Check if opposite sides of the shared edge
					return (cross0 < 0) != (cross1 < 0);
				}
			}
		}

		return false;
	}
};