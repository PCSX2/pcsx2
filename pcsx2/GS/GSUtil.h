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

	static GS_PRIM_CLASS GetPrimClass(u32 prim);
	static int GetVertexCount(u32 prim);
	static int GetClassVertexCount(u32 primclass);

	static const u32* HasSharedBitsPtr(u32 dpsm);
	static bool HasSharedBits(u32 spsm, const u32* ptr);
	static bool HasSharedBits(u32 spsm, u32 dpsm);
	static bool HasSharedBits(u32 sbp, u32 spsm, u32 dbp, u32 dpsm);
	static bool HasCompatibleBits(u32 spsm, u32 dpsm);
	static bool HasSameSwizzleBits(u32 spsm, u32 dpsm);
	static u32 GetChannelMask(u32 spsm);
	static u32 GetChannelMask(u32 spsm, u32 fbmsk);

	static GSRendererType GetPreferredRenderer();

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
