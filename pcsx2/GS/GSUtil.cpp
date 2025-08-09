// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/GS.h"
#include "GS/GSExtra.h"
#include "GS/GSUtil.h"
#include "GS/GSLocalMemory.h"
#include "GS/GSVector.h"
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
	u32 CompatibleBitsField[64][2] = {};
	u32 SharedBitsField[64][2] = {};
	u32 SwizzleField[64][2] = {};

	constexpr GSUtilMaps()
	{
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

u32 GSUtil::GetChannelMask(u32 spsm, u32 fbmsk)
{
	const u32 mask = GSLocalMemory::m_psm[spsm].fmsk & ~fbmsk;
	return
		((mask & 0xFF)       ? 1 : 0) |
		((mask & 0xFF00)     ? 2 : 0) |
		((mask & 0xFF0000)   ? 4 : 0) |
		((mask & 0xFF000000) ? 8 : 0);
}

template <Align_Mode mode>
GSVector4i GSUtil::GetAlignedUnits(GSVector4i rect, const GSVector2i align)
{
	rect = rect.ralign<mode>(align);
	rect.x /= align.x;
	rect.y /= align.y;
	rect.z /= align.x;
	rect.w /= align.y;
	return rect;
}

template GSVector4i GSUtil::GetAlignedUnits<Align_Outside>(GSVector4i rect, const GSVector2i align);
template GSVector4i GSUtil::GetAlignedUnits<Align_Inside>(GSVector4i rect, const GSVector2i align);
template GSVector4i GSUtil::GetAlignedUnits<Align_NegInf>(GSVector4i rect, const GSVector2i align);
template GSVector4i GSUtil::GetAlignedUnits<Align_PosInf>(GSVector4i rect, const GSVector2i align);

GSVector2i GSUtil::ConvertRangeDepthFormat(const GSVector2i range, const int pg_size)
{
	const int start = range.x;
	const int end = range.y;
	const int half_pg_size = pg_size / 2;
	const int start_pg = start & ~(pg_size - 1);
	const int start_mid_pg = start_pg + half_pg_size;
	const int end_pg = (end + pg_size - 1) & ~(pg_size - 1);
	const int end_mid_pg = end_pg - half_pg_size;

	if (start_pg == end_pg)
	{
		// Empty interval exactly on a page; ambiguous.
		return GSVector2i(start_pg, end_pg);
	}

	int new_start;
	int new_end;

	if (start > start_mid_pg)
		new_start = start - half_pg_size; // Only upper half of start page covered.
	else if (end > start_mid_pg)
		new_start = start_pg; // Interval crosses start half-page.
	else if (start < start_mid_pg)
		new_start = start + half_pg_size; // Interval fully inside lower half page.
	else
		new_start = start; // Empty interval exactly on a half page; ambiguous.

	if (end < end_mid_pg)
		new_end = end + half_pg_size; // Only lower half of end page covered.
	else if (start < end_mid_pg)
		new_end = end_pg; // Interval crosses end half-page.
	else if (end > end_mid_pg)
		new_end = end - half_pg_size; // Interval fully inside upper half page.
	else
		new_end = end; // Empty interval exactly on a half page; ambiguous.

	return GSVector2i(new_start, new_end);
};

GSVector4i GSUtil::ConvertBBoxDepthFormat(const GSVector4i bbox, const GSVector2i pg_size)
{
	const GSVector2i pt0 = ConvertRangeDepthFormat(GSVector2i(bbox.x, bbox.z), pg_size.x);
	const GSVector2i pt1 = ConvertRangeDepthFormat(GSVector2i(bbox.y, bbox.w), pg_size.y);
	return GSVector4i(pt0.x, pt1.x, pt0.y, pt1.y);
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
