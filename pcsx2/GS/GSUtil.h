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

	// Determines ordering of two triangles in parallel if both are right.
	// More efficient than calling IsTriangleRight twice.
	template <u32 tme, u32 fst>
	static bool AreTrianglesRight(const GSVertex* RESTRICT vin, const u16* index0, const u16* index1,
		TriangleOrdering* out_triangle0, TriangleOrdering* out_triangle1);

	// Determines ordering of a single triangle
	template <u32 tme, u32 fst>
	static bool IsTriangleRight(const GSVertex* RESTRICT vin, const u16* index, TriangleOrdering* out_triangle);
};
