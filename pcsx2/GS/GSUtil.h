// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS.h"
#include "GSRegs.h"

class GSUtil
{
public:
	static const char* GetATSTName(u32 atst);
	static const char* GetAFAILName(u32 afail);
	static const char* GetPSMName(int psm);
	static const char* GetWMName(u32 wm);
	static const char* GetZTSTName(u32 ztst);
	static const char* GetPrimName(u32 prim);

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
};
