// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
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

const char* GSUtil::GetWMName(u32 wm)
{
	static constexpr const char* names[] = {"REPEAT", "CLAMP", "REGION_CLAMP", "REGION_REPEAT"};
	return (wm < std::size(names)) ? names[wm] : "";
}

const char* GSUtil::GetZTSTName(u32 ztst)
{
	static constexpr const char* names[] = {
		"NEVER", "ALWAYS", "GEQUAL", "GREATER"};
	return (ztst < std::size(names)) ? names[ztst] : "";
}

const char* GSUtil::GetPrimName(u32 prim)
{
	static constexpr const char* names[] = {
		"POINT", "LINE", "LINESTRIP", "TRIANGLE", "TRIANGLESTRIP", "TRIANGLEFAN", "SPRITE", "INVALID"};
	return (prim < std::size(names)) ? names[prim] : "";
}

const char* GSUtil::GetPrimClassName(u32 primclass)
{
	static constexpr const char* names[] = {
		"POINT", "LINE", "TRIANGLE", "SPRITE", "INVALID"};
	return (primclass < std::size(names)) ? names[primclass] : "";
}

const char* GSUtil::GetMMAGName(u32 mmag)
{
	static constexpr const char* names[] = {"NEAREST", "LINEAR"};
	return (mmag < std::size(names)) ? names[mmag] : "";
}

const char* GSUtil::GetMMINName(u32 mmin)
{
	static constexpr const char* names[8] = {"NEAREST", "LINEAR", "NEAREST_MIPMAP_NEAREST", "NEAREST_MIPMAP_LINEAR",
		"LINEAR_MIPMAP_NEAREST", "LINEAR_MIPMAP_LINEAR"};
	return (mmin < std::size(names)) ? names[mmin] : "";
}

const char* GSUtil::GetMTBAName(u32 mtba)
{
	static constexpr const char* names[] = {"MIPTBP1", "AUTO"};
	return (mtba < std::size(names)) ? names[mtba] : "";
}

const char* GSUtil::GetLCMName(u32 lcm)
{
	static constexpr const char* names[] = {"Formula", "K"};
	return (lcm < std::size(names)) ? names[lcm] : "";
}

const char* GSUtil::GetSCANMSKName(u32 scanmsk)
{
	static constexpr const char* names[] = {"Normal", "Reserved", "Even prohibited", "Odd prohibited"};
	return (scanmsk < std::size(names)) ? names[scanmsk] : "";
}

const char* GSUtil::GetDATMName(u32 datm)
{
	static constexpr const char* names[] = {"0 pass", "1 pass"};
	return (datm < std::size(names)) ? names[datm] : "";
}

const char* GSUtil::GetTFXName(u32 tfx)
{
	static constexpr const char* names[] = {"MODULATE", "DECAL", "HIGHLIGHT", "HIGHLIGHT2"};
	return (tfx < std::size(names)) ? names[tfx] : "";
}

const char* GSUtil::GetTCCName(u32 tcc)
{
	static constexpr const char* names[] = {"RGB", "RGBA"};
	return (tcc < std::size(names)) ? names[tcc] : "";
}

const char* GSUtil::GetACName(u32 ac)
{
	static constexpr const char* names[] = {"PRMODE", "PRIM"};
	return (ac < std::size(names)) ? names[ac] : "";
}

const char* GSUtil::GetPerfMonCounterName(GSPerfMon::counter_t counter, bool hw)
{
	if (hw)
	{
		static constexpr const char* names_hw[GSPerfMon::CounterLastHW] = {
			"Prim",
			"Draw",
			"DrawCalls",
			"Readbacks",
			"Swizzle",
			"Unswizzle",
			"TextureCopies",
			"TextureUploads",
			"Barriers",
			"RenderPasses"
		};
		return counter < std::size(names_hw) ? names_hw[counter] : "";
	}
	else
	{
		static constexpr const char* names_sw[GSPerfMon::CounterLastSW] = {
			"Prim",
			"Draw",
			"DrawCalls",
			"Readbacks",
			"Swizzle",
			"Unswizzle",
			"Fillrate",
			"SyncPoint"
		};
		return counter < std::size(names_sw) ? names_sw[counter] : "";
	}
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
