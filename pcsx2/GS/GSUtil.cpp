// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/GS.h"
#include "GS/GSExtra.h"
#include "GS/GSUtil.h"
#include "MultiISA.h"
#include "common/StringUtil.h"

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

static class GSUtilMaps
{
public:
	u8 PrimClassField[8];
	u8 VertexCountField[8];
	u8 ClassVertexCountField[4];
	u32 CompatibleBitsField[64][2];
	u32 SharedBitsField[64][2];
	u32 SwizzleField[64][2];

	// Defer init to avoid AVX2 illegal instructions
	void Init()
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

		memset(CompatibleBitsField, 0, sizeof(CompatibleBitsField));

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

		memset(SwizzleField, 0, sizeof(SwizzleField));

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

		memset(SharedBitsField, 0, sizeof(SharedBitsField));

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

} s_maps;

void GSUtil::Init()
{
	s_maps.Init();
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
	mask &= (fbmsk & 0xFF) ? (~0x1 & 0xf) : 0xf;
	mask &= (fbmsk & 0xFF00) ? (~0x2 & 0xf) : 0xf;
	mask &= (fbmsk & 0xFF0000) ? (~0x4 & 0xf) : 0xf;
	mask &= (fbmsk & 0xFF000000) ? (~0x8 & 0xf) : 0xf;
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

const char* psm_str(int psm)
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
