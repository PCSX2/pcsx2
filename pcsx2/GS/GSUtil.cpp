// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/GS.h"
#include "GS/GSExtra.h"
#include "GS/GSUtil.h"
#include "MultiISA.h"
#include "common/StringUtil.h"

#include <array>

#ifdef ENABLE_VULKAN
#include "GS/Renderers/Vulkan/GSDeviceVK.h"
#endif

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <d3dcommon.h>
#include <dxgi.h>
#include <VersionHelpers.h>
#include "GS/Renderers/DX11/D3D.h"
#include <wil/com.h>
#endif

namespace {
struct GSUtilMaps
{
	u8 PrimClassField[8] = {};
	u8 VertexCountField[8] = {};
	u8 ClassVertexCountField[4] = {};
	u32 CompatibleBitsField[64][2] = {};
	u32 SharedBitsField[64][2] = {};
	u32 SwizzleField[64][2] = {};

	constexpr GSUtilMaps()
	{
		PrimClassField[GS_POINTLIST] = GS_POINT_CLASS;
		PrimClassField[GS_LINELIST] = GS_LINE_CLASS;
		PrimClassField[GS_LINESTRIP] = GS_LINE_CLASS;
		PrimClassField[GS_TRIANGLELIST] = GS_TRIANGLE_CLASS;
		PrimClassField[GS_TRIANGLESTRIP] = GS_TRIANGLE_CLASS;
		PrimClassField[GS_TRIANGLEFAN] = GS_TRIANGLE_CLASS;
		PrimClassField[GS_SPRITE] = GS_SPRITE_CLASS;
		PrimClassField[GS_INVALID] = GS_INVALID_CLASS;

		VertexCountField[GS_POINTLIST] = 1;
		VertexCountField[GS_LINELIST] = 2;
		VertexCountField[GS_LINESTRIP] = 2;
		VertexCountField[GS_TRIANGLELIST] = 3;
		VertexCountField[GS_TRIANGLESTRIP] = 3;
		VertexCountField[GS_TRIANGLEFAN] = 3;
		VertexCountField[GS_SPRITE] = 2;
		VertexCountField[GS_INVALID] = 1;

		ClassVertexCountField[GS_POINT_CLASS] = 1;
		ClassVertexCountField[GS_LINE_CLASS] = 2;
		ClassVertexCountField[GS_TRIANGLE_CLASS] = 3;
		ClassVertexCountField[GS_SPRITE_CLASS] = 2;

		for (int i = 0; i < 64; i++)
		{
			CompatibleBitsField[i][i >> 5] |= 1U << (i & 0x1f);
		}

		CompatibleBitsField[PSMCT32][PSMCT24 >> 5] |= 1 << (PSMCT24 & 0x1f);
		CompatibleBitsField[PSMCT24][PSMCT32 >> 5] |= 1 << (PSMCT32 & 0x1f);
		CompatibleBitsField[PSMCT16][PSMCT16S >> 5] |= 1 << (PSMCT16S & 0x1f);
		CompatibleBitsField[PSMCT16S][PSMCT16 >> 5] |= 1 << (PSMCT16 & 0x1f);
		CompatibleBitsField[PSMZ32][PSMZ24 >> 5] |= 1 << (PSMZ24 & 0x1f);
		CompatibleBitsField[PSMZ24][PSMZ32 >> 5] |= 1 << (PSMZ32 & 0x1f);
		CompatibleBitsField[PSMZ16][PSMZ16S >> 5] |= 1 << (PSMZ16S & 0x1f);
		CompatibleBitsField[PSMZ16S][PSMZ16 >> 5] |= 1 << (PSMZ16 & 0x1f);

		for (int i = 0; i < 64; i++)
		{
			SwizzleField[i][i >> 5] |= 1U << (i & 0x1f);
		}

		SwizzleField[PSMCT32][PSMCT24 >> 5] |= 1 << (PSMCT24 & 0x1f);
		SwizzleField[PSMCT24][PSMCT32 >> 5] |= 1 << (PSMCT32 & 0x1f);
		SwizzleField[PSMT8H][PSMCT32 >> 5] |= 1 << (PSMCT32 & 0x1f);
		SwizzleField[PSMCT32][PSMT8H >> 5] |= 1 << (PSMT8H & 0x1f);
		SwizzleField[PSMT4HL][PSMCT32 >> 5] |= 1 << (PSMCT32 & 0x1f);
		SwizzleField[PSMCT32][PSMT4HL >> 5] |= 1 << (PSMT4HL & 0x1f);
		SwizzleField[PSMT4HH][PSMCT32 >> 5] |= 1 << (PSMCT32 & 0x1f);
		SwizzleField[PSMCT32][PSMT4HH >> 5] |= 1 << (PSMT4HH & 0x1f);
		SwizzleField[PSMZ32][PSMZ24 >> 5] |= 1 << (PSMZ24 & 0x1f);
		SwizzleField[PSMZ24][PSMZ32 >> 5] |= 1 << (PSMZ32 & 0x1f);

		SharedBitsField[PSMCT24][PSMT8H >> 5] |= 1 << (PSMT8H & 0x1f);
		SharedBitsField[PSMCT24][PSMT4HL >> 5] |= 1 << (PSMT4HL & 0x1f);
		SharedBitsField[PSMCT24][PSMT4HH >> 5] |= 1 << (PSMT4HH & 0x1f);
		SharedBitsField[PSMZ24][PSMT8H >> 5] |= 1 << (PSMT8H & 0x1f);
		SharedBitsField[PSMZ24][PSMT4HL >> 5] |= 1 << (PSMT4HL & 0x1f);
		SharedBitsField[PSMZ24][PSMT4HH >> 5] |= 1 << (PSMT4HH & 0x1f);
		SharedBitsField[PSMT8H][PSMCT24 >> 5] |= 1 << (PSMCT24 & 0x1f);
		SharedBitsField[PSMT8H][PSMZ24 >> 5] |= 1 << (PSMZ24 & 0x1f);
		SharedBitsField[PSMT4HL][PSMCT24 >> 5] |= 1 << (PSMCT24 & 0x1f);
		SharedBitsField[PSMT4HL][PSMZ24 >> 5] |= 1 << (PSMZ24 & 0x1f);
		SharedBitsField[PSMT4HL][PSMT4HH >> 5] |= 1 << (PSMT4HH & 0x1f);
		SharedBitsField[PSMT4HH][PSMCT24 >> 5] |= 1 << (PSMCT24 & 0x1f);
		SharedBitsField[PSMT4HH][PSMZ24 >> 5] |= 1 << (PSMZ24 & 0x1f);
		SharedBitsField[PSMT4HH][PSMT4HL >> 5] |= 1 << (PSMT4HL & 0x1f);
	}
};
}

static constexpr const GSUtilMaps s_maps;

const char* GSUtil::GetATSTName(u32 atst)
{
	static constexpr const char* names[] = {
		"NEVER", "ALWAYS", "LESS", "LEQUAL", "EQUAL", "GEQUAL", "GREATER", "NOTEQUAL" };
	return (atst < std::size(names)) ? names[atst] : "";
}

const char* GSUtil::GetAFAILName(u32 afail)
{
	static constexpr const char* names[] = {"KEEP", "FB_ONLY", "ZB_ONLY", "RGB_ONLY"};
	return (afail < std::size(names)) ? names[afail] : "";
}

GS_PRIM_CLASS GSUtil::GetPrimClass(u32 prim)
{
	return (GS_PRIM_CLASS)s_maps.PrimClassField[prim];
}

int GSUtil::GetVertexCount(u32 prim)
{
	return s_maps.VertexCountField[prim];
}

int GSUtil::GetClassVertexCount(u32 primclass)
{
	return s_maps.ClassVertexCountField[primclass];
}

const u32* GSUtil::HasSharedBitsPtr(u32 dpsm)
{
	return s_maps.SharedBitsField[dpsm];
}

bool GSUtil::HasSharedBits(u32 spsm, const u32* RESTRICT ptr)
{
	return (ptr[spsm >> 5] & (1 << (spsm & 0x1f))) == 0;
}

// Pixels can NOT coexist in the same 32bits of space.
// Example: Using PSMT8H or PSMT4HL/HH with CT24 would fail this check.
bool GSUtil::HasSharedBits(u32 spsm, u32 dpsm)
{
	return (s_maps.SharedBitsField[dpsm][spsm >> 5] & (1 << (spsm & 0x1f))) == 0;
}

// Pixels can NOT coexist in the same 32bits of space.
// Example: Using PSMT8H or PSMT4HL/HH with CT24 would fail this check.
// SBP and DBO must match.
bool GSUtil::HasSharedBits(u32 sbp, u32 spsm, u32 dbp, u32 dpsm)
{
	return ((sbp ^ dbp) | (s_maps.SharedBitsField[dpsm][spsm >> 5] & (1 << (spsm & 0x1f)))) == 0;
}

// Shares bit depths, only detects 16/24/32 bit formats.
// 24/32bit cross compatible, 16bit compatbile with 16bit.
bool GSUtil::HasCompatibleBits(u32 spsm, u32 dpsm)
{
	return (s_maps.CompatibleBitsField[spsm][dpsm >> 5] & (1 << (dpsm & 0x1f))) != 0;
}

bool GSUtil::HasSameSwizzleBits(u32 spsm, u32 dpsm)
{
	return (s_maps.SwizzleField[spsm][dpsm >> 5] & (1 << (dpsm & 0x1f))) != 0;
}

u32 GSUtil::GetChannelMask(u32 spsm)
{
	switch (spsm)
	{
		case PSMCT24:
		case PSMZ24:
			return 0x7;
		case PSMT8H:
		case PSMT4HH: // This sucks, I'm sorry, but we don't have a way to do half channels
		case PSMT4HL: // So uuhh TODO I guess.
			return 0x8;
		default:
			return 0xf;
	}
}

u32 GSUtil::GetChannelMask(u32 spsm, u32 fbmsk)
{
	u32 mask = GetChannelMask(spsm);
	mask &= ((fbmsk & 0xFF) == 0xFF) ? (~0x1 & 0xf) : 0xf;
	mask &= ((fbmsk & 0xFF00) == 0xFF00) ? (~0x2 & 0xf) : 0xf;
	mask &= ((fbmsk & 0xFF0000) == 0xFF0000) ? (~0x4 & 0xf) : 0xf;
	mask &= ((fbmsk & 0xFF000000) == 0xFF000000) ? (~0x8 & 0xf) : 0xf;
	return mask;
}

GSRendererType GSUtil::GetPreferredRenderer()
{
	// Memorize the value, so we don't keep re-querying it.
	static GSRendererType preferred_renderer = GSRendererType::Auto;
	if (preferred_renderer == GSRendererType::Auto)
	{
#if defined(__APPLE__)
		// Mac: Prefer Metal hardware.
		preferred_renderer = GSRendererType::Metal;
#elif defined(_WIN32) && defined(_M_ARM64)
		// Default to DX12 on Windows-on-ARM.
		preferred_renderer = GSRendererType::DX12;
#elif defined(_WIN32)
		// Use D3D device info to select renderer.
		preferred_renderer = D3D::GetPreferredRenderer();
#else
		// Linux: Prefer Vulkan if the driver isn't buggy.
#if defined(ENABLE_VULKAN)
		if (GSDeviceVK::IsSuitableDefaultRenderer())
			preferred_renderer = GSRendererType::VK;
#endif

			// Otherwise, whatever is available.
	if (preferred_renderer == GSRendererType::Auto) // If it's still auto, VK wasn't selected.
#if defined(ENABLE_OPENGL)
		preferred_renderer = GSRendererType::OGL;
#elif defined(ENABLE_VULKAN)
		preferred_renderer = GSRendererType::VK;
#else
		preferred_renderer = GSRendererType::SW;
#endif
#endif
	}

	return preferred_renderer;
}

// Helper struct for IsTriangleRight and AreTrianglesRight
struct ComparisonResult
{
	u8 value;
	u8 FinalCmp() const { return value & 3; }
	constexpr ComparisonResult(u8 final_cmp, u8 final_order)
		: value(final_cmp | (final_order << 2))
	{
	}
	GSUtil::TriangleOrdering FinalOrder() const
	{
		struct alignas(2) TriangleOrderingBC
		{
			u8 b;
			u8 c;
		};
		alignas(16) static constexpr TriangleOrderingBC order_lut[6] =
			{
				TriangleOrderingBC{/*a=0,*/ 1, 2},
				TriangleOrderingBC{/*a=0,*/ 2, 1},
				TriangleOrderingBC{/*a=1,*/ 0, 2},
				TriangleOrderingBC{/*a=1,*/ 2, 0},
				TriangleOrderingBC{/*a=2,*/ 0, 1},
				TriangleOrderingBC{/*a=2,*/ 1, 0},
			};
		u32 order = static_cast<u32>(value) >> 2;
		TriangleOrderingBC bc = order_lut[order];
		return {order >> 1, bc.b, bc.c};
	}
};

// Helper table for IsTriangleRight/AreTrianglesRight functions
static constexpr ComparisonResult comparison_lut[16] =
{
	ComparisonResult(0, 0), // 0000 => None equal, no sprite possible
	ComparisonResult(2, 0), // 0001 => x0 = x1, requires y1 = y2
	ComparisonResult(1, 5), // 0010 => y0 = y1, requires x1 = x2
	ComparisonResult(2, 0), // 0011 => x0 = x1, y0 = y1, (no area) requires x1 = x2 or y1 = y2
	ComparisonResult(2, 1), // 0100 => x0 = x2, requires y1 = y2
	ComparisonResult(2, 0), // 0101 => x0 = x1, x0 = x2, (no area) requires y1 = y2
	ComparisonResult(0, 4), // 0110 => y0 = y1, x0 = x2, requires nothing
	ComparisonResult(0, 4), // 0111 => x0 = y1, y0 = y1, x0 = x2, (no area) requires nothing
	ComparisonResult(1, 3), // 1000 => y0 = y2, requires x1 = x2
	ComparisonResult(0, 2), // 1001 => x0 = x1, y0 = y2, requires nothing
	ComparisonResult(1, 3), // 1010 => y0 = y1, y0 = y2, (no area) requires x1 = x2
	ComparisonResult(0, 2), // 1011 => x0 = x1, y0 = y1, y0 = y2, (unlikely) requires nothing
	ComparisonResult(2, 1), // 1100 => x0 = x2, y0 = y2, (no area) requires x1 = x2 or y1 = y2
	ComparisonResult(0, 2), // 1101 => x0 = x1, x0 = x2, y0 = y2, (no area) requires nothing
	ComparisonResult(0, 4), // 1110 => y0 = y1, x0 = x2, y0 = y2, (no area) requires nothing
	ComparisonResult(0, 2), // 1111 => x0 = x1, y0 = y1, x0 = x2, y0 = y2, (no area) requires nothing
};

template <u32 tme, u32 fst>
bool GSUtil::AreTrianglesRight(const GSVertex* RESTRICT vin, const u16* index0, const u16* index1,
	TriangleOrdering* out_triangle0, TriangleOrdering* out_triangle1)
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
	ComparisonResult triangle0cmp = comparison_lut[cmp0];
	ComparisonResult triangle1cmp = comparison_lut[cmp1];
	int required_cmp2 = triangle0cmp.FinalCmp() | (triangle1cmp.FinalCmp() << 2);
	if ((cmp2 & required_cmp2) != required_cmp2)
		return false;
	// Both t0 and t1 are right triangles!
	*out_triangle0 = triangle0cmp.FinalOrder();
	*out_triangle1 = triangle1cmp.FinalOrder();
	return true;
}

template <u32 tme, u32 fst>
bool GSUtil::IsTriangleRight(const GSVertex* RESTRICT vin, const u16* index, TriangleOrdering* out_triangle)
{
	GSVector4i mask;
	if (tme && fst)
	{
		// Compare xy and uv together
		mask = GSVector4i::cxpr8(
			(s8)0, (s8)1, (s8) 8, (s8) 9,
			(s8)2, (s8)3, (s8)10, (s8)11,
			(s8)0, (s8)1, (s8) 8, (s8) 9,
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
	ComparisonResult trianglecmp = comparison_lut[cmp0];
	int required_cmp1 = trianglecmp.FinalCmp();
	if (cmp1 != required_cmp1)
		return false;
	// Both t0 and t1 are right triangles!
	*out_triangle = trianglecmp.FinalOrder();
	return true;
}

// Instantiate the template functions for Is/AreTrianglesRight
template bool GSUtil::AreTrianglesRight<0, 0>(const GSVertex* RESTRICT, const u16*, const u16*, TriangleOrdering*, TriangleOrdering*);
template bool GSUtil::AreTrianglesRight<1, 0>(const GSVertex* RESTRICT, const u16*, const u16*, TriangleOrdering*, TriangleOrdering*);
template bool GSUtil::AreTrianglesRight<0, 1>(const GSVertex* RESTRICT, const u16*, const u16*, TriangleOrdering*, TriangleOrdering*);
template bool GSUtil::AreTrianglesRight<1, 1>(const GSVertex* RESTRICT, const u16*, const u16*, TriangleOrdering*, TriangleOrdering*);
template bool GSUtil::IsTriangleRight<0, 0>(const GSVertex* RESTRICT, const u16*, TriangleOrdering*);
template bool GSUtil::IsTriangleRight<1, 0>(const GSVertex* RESTRICT, const u16*, TriangleOrdering*);
template bool GSUtil::IsTriangleRight<0, 1>(const GSVertex* RESTRICT, const u16*, TriangleOrdering*);
template bool GSUtil::IsTriangleRight<1, 1>(const GSVertex* RESTRICT, const u16*, TriangleOrdering*);

const char* GSUtil::GetPSMName(int psm)
{
	switch (psm)
	{
		// Normal color
		case PSMCT32:  return "C_32";
		case PSMCT24:  return "C_24";
		case PSMCT16:  return "C_16";
		case PSMCT16S: return "C_16S";

		// Palette color
		case PSMT8:    return "P_8";
		case PSMT4:    return "P_4";
		case PSMT8H:   return "P_8H";
		case PSMT4HL:  return "P_4HL";
		case PSMT4HH:  return "P_4HH";

		// Depth
		case PSMZ32:   return "Z_32";
		case PSMZ24:   return "Z_24";
		case PSMZ16:   return "Z_16";
		case PSMZ16S:  return "Z_16S";

		case PSGPU24:  return "PS24";

		default:break;
	}
	return "BAD_PSM";
}
