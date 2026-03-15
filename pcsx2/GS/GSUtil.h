// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS.h"
#include "GSRegs.h"
#include "GSPerfMon.h"

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
	static const char* GetPerfMonCounterName(GSPerfMon::counter_t counter, bool hw = true);

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

// Class that represents an octogonal bounding area with sides at 45 degree increments.
class BoundingOct
{
private:
	GSVector4i bbox0; // Standard bbox.
	GSVector4i bbox1; // Bounding diamond (rotated 45 degrees axes and scaled, so (x, y) becomes (x + y, x - y)).

	// Assumes that v is of the form { x, y, x, y }.
	static GSVector4i Rotate45(const GSVector4i& v)
	{
		const GSVector4i swap = v.yxwz();
		return (v + swap).blend32<0xa>(swap - v);
	}

	BoundingOct(const GSVector4i& bbox0, const GSVector4i& bbox1)
		: bbox0(bbox0)
		, bbox1(bbox1)
	{
	}

public:
	// Initialize to null bounding area.
	BoundingOct()
		: bbox0(GSVector4i(INT_MAX, INT_MAX, INT_MIN, INT_MIN))
		, bbox1(GSVector4i(INT_MAX, INT_MAX, INT_MIN, INT_MIN))
	{
	}

	static BoundingOct FromPoint(GSVector4i v)
	{
		v = v.xyxy();
		return { v, Rotate45(v) };
	}

	// The two inputs are assumed to be diagonally opposite to each other in an axis-aligned quad (i.e. sprite).
	static BoundingOct FromSprite(GSVector4i v0, GSVector4i v1)
	{
		const GSVector4i min = v0.min_i32(v1);
		const GSVector4i max = v0.max_i32(v1);
		const GSVector4i bbox = min.upl64(max);
		// Rotate45(x, y) => (x + y, x - y), we want the (min, max) result for any pair of (x, y)
		// Min: (min.x + min.y, min.x - max.y)
		// Max: (max.x + max.y, max.x - min.y)
		const GSVector4i x = GSVector4i::cast(GSVector4::cast(min).xxxx(GSVector4::cast(max))); // Don't use bbox immediately to help the compiler optimize.
		const GSVector4i y = bbox.ywwy();
		return {
			bbox,
			(x + y).blend32<0xa>(x - y),
		};
	}

	BoundingOct Union(GSVector4i v) const
	{
		v = v.xyxy();
		return { bbox0.runion(v), bbox1.runion(Rotate45(v)) };
	}

	BoundingOct Union(const BoundingOct& other) const
	{
		return { bbox0.runion(other.bbox0), bbox1.runion(other.bbox1) };
	}

	BoundingOct UnionSprite(GSVector4i pt0, GSVector4i pt1) const
	{
		return Union(FromSprite(pt0, pt1));
	}

	bool Intersects(const BoundingOct& other) const
	{
		return bbox0.rintersects(other.bbox0) && bbox1.rintersects(other.bbox1);
	}

	BoundingOct FixDegenerate() const
	{
		return {
			bbox0.blend(bbox0 + GSVector4i(0, 0, 1, 1), bbox0.xyxy() == bbox0.zwzw()),
			bbox1.blend(bbox1 + GSVector4i(0, 0, 1, 1), bbox1.xyxy() == bbox1.zwzw()),
		};
	}

	BoundingOct ExpandOne() const
	{
		return {
			bbox0 + GSVector4i(-1, -1, 1, 1),
			bbox1 + GSVector4i(-1, -1, 1, 1),
		};
	}

	const GSVector4i& ToBBox()
	{
		return bbox0;
	}
};
