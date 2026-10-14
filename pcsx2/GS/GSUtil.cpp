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

#include <algorithm>
#include <bit>

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
#elif defined(_WIN32) && defined(ARCH_ARM64)
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

bool GSUtil::IsValidPSM(int psm)
{
	switch (psm)
	{
		case PSMCT32:
		case PSMCT24:
		case PSMCT16:
		case PSMCT16S:
		case PSMT8:
		case PSMT4:
		case PSMT8H:
		case PSMT4HL:
		case PSMT4HH:
		case PSMZ32:
		case PSMZ24:
		case PSMZ16:
		case PSMZ16S:
		case PSGPU24:
			return true;
		default:
			return false;
	}
}

template<bool swap_xy, typename T>
static __forceinline T SwapXY(T rect)
{
	return swap_xy ? rect.yxwz() : rect;
}

template<typename T>
static __forceinline T ExpandRect(const T& rect, const int expand)
{
	return rect + T(-expand, -expand, expand, expand);
}

// Type representing a W x H grid with boolean entries, as a bitfield.
template<u32 W, u32 H, typename BitfieldType>
requires ((W <= sizeof(BitfieldType) * 8) && (W % (sizeof(BitfieldType) * 8) == 0))
class BitGrid
{
public:
	static constexpr u32 BITWIDTH = sizeof(BitfieldType) * 8;
	static constexpr BitfieldType ALLMASK = ~static_cast<BitfieldType>(0);

	// A single boolean at (x, y).
	bool Get(u32 x, u32 y) const
	{
		return GetBitfield(y, x) & (1 << (x & (BITWIDTH - 1)));
	}

	// The run of BITWIDTH booleans at (x, y). x is rounded.
	BitfieldType& GetBitfield(u32 x, u32 y)
	{
		return grid[(W / BITWIDTH) * y + (x / BITWIDTH)];
	}

	// The run of BITWIDTH booleans at (x, y). x is rounded.
	BitfieldType GetBitfield(u32 x, u32 y) const
	{
		return grid[(W / BITWIDTH) * y + (x / BITWIDTH)];
	}
private:
	std::array<BitfieldType, W * H / (sizeof(BitfieldType) * 8)> grid{};
};


// Get the coarse coverage for a single scanline.
template<u32 W, u32 H, typename BitfieldType>
static __forceinline void CoarseRasterizeScanline(const u32 x0, const u32 x1, const u32 y,
	BitGrid<W, H, BitfieldType>& grid)
{
	constexpr BitfieldType ALLMASK = BitGrid<W, H, BitfieldType>::ALLMASK;
	constexpr u32 BITWIDTH = BitGrid<W, H, BitfieldType>::BITWIDTH;

	const BitfieldType first_mask = (ALLMASK << (x0 & (BITWIDTH - 1)));
	const BitfieldType last_mask = (ALLMASK >> (BITWIDTH - (x1 & (BITWIDTH - 1))));

	if constexpr (W <= BITWIDTH)
	{
		// There's only one bitfield per scanline here.
		grid.GetBitfield(0, y) |= first_mask & (x1 < BITWIDTH ? last_mask : ALLMASK);
		return;
	}

	const u32 x0_floor = x0 & ~(BITWIDTH - 1);
	const u32 x1_ceil = (x1 + (BITWIDTH - 1)) & ~(BITWIDTH - 1);
	for (u32 x = x0_floor; x < x1_ceil; x += BITWIDTH)
	{
		const BitfieldType mask0 = (x <= x0) ? first_mask : ALLMASK;
		const BitfieldType mask1 = (x >= (x1 & ~(BITWIDTH - 1))) ? last_mask : ALLMASK;
		grid.GetBitfield(y, x) |= mask0 & mask1;
	}
}

// Get the coarse coverage for a triangle section.
template<u32 W, u32 H, typename BitfieldType>
static void CoarseRasterizeTriangleSection(int top, int bottom,
	const GSVector2& x_top, const float y_top, const GSVector2& ddx,
	const GSVector2i& tile_size, const GSVector2i& tile_shift, const int expand, BitGrid<W, H, BitfieldType>& grid)
{
	top -= expand;
	bottom += expand;

	const int top_floor = top & ~(tile_size.y - 1);
	const int bottom_ceil = (bottom + tile_size.y - 1) & ~(tile_size.y - 1);

	for (int y = top_floor; y < bottom_ceil; y += tile_size.y)
	{
		const int y0 = std::clamp(y, top, bottom - 1);
		const int y1 = std::clamp(y + tile_size.y, top, bottom - 1);

		const float dy0 = static_cast<float>(y0) - y_top;
		const float dy1 = static_cast<float>(y1) - y_top;

		const GSVector2 x0 = x_top + ddx * dy0;
		const GSVector2 x1 = x_top + ddx * dy1;

		const int x0_left = static_cast<int>(std::floor(x0.x)) - expand;
		const int x0_right = static_cast<int>(std::ceil(x0.y)) + expand;
		const int x1_left = static_cast<int>(std::floor(x1.x)) - expand;
		const int x1_right = static_cast<int>(std::ceil(x1.y)) + expand;

		int x_left = std::min(x0_left, x1_left);
		int x_right = std::max(x0_right, x1_right);

		x_left = std::clamp<int>(x_left >> tile_shift.x, 0, W);
		x_right = std::clamp<int>((x_right >> tile_shift.x) + 1, 0, W);

		const int y_tile = y0 >> tile_shift.y;

		if (0 < x_right && x_left < x_right && x_left < W && 0 <= y_tile && y_tile < H)
			CoarseRasterizeScanline<W, H, BitfieldType>(x_left, x_right, y_tile, grid);
	}
}

// Get the coarse coverage for a given rect.
template<u32 W, u32 H, typename BitfieldType>
static __forceinline void CoarseRasterizeRect(GSVector4i rect, const GSVector2i& tile_size,
	const GSVector2i& tile_shift, const int expand, BitGrid<W, H, BitfieldType>& grid)
{
	rect = ExpandRect(rect, expand).ralign<Align_Outside>(tile_size);

	const u32 x0 = std::clamp<int>(rect.x >> tile_shift.x, 0, W);
	const u32 y0 = std::clamp<int>(rect.y >> tile_shift.y, 0, H);
	const u32 x1 = std::clamp<int>(rect.z >> tile_shift.x, 0, W);
	const u32 y1 = std::clamp<int>(rect.w >> tile_shift.y, 0, H);

	for (u32 y = y0; y < y1; y++)
		CoarseRasterizeScanline<W, H, BitfieldType>(x0, x1, y, grid);
}

// Lookup table to sort vertices by Y.
static const u8 s_ysort[8][4] =
{
	{0, 1, 2, 0}, // y0 <= y1 <= y2
	{1, 0, 2, 0}, // y1 < y0 <= y2
	{0, 0, 0, 0},
	{1, 2, 0, 0}, // y1 <= y2 < y0
	{0, 2, 1, 0}, // y0 <= y2 < y1
	{0, 0, 0, 0},
	{2, 0, 1, 0}, // y2 < y0 <= y1
	{2, 1, 0, 0}, // y2 < y1 < y0
};

// Get the coarse coverage for a triangle (code modified from SW renderer).
template<u32 W, u32 H, typename BitfieldType>
static __forceinline void CoarseRasterizeTriangle(const GSVector4* RESTRICT p,
	const GSVector2i& tile_size, const GSVector2i& tile_shift,
	const int expand, BitGrid<W, H, BitfieldType>& grid)
{
	GSVector4 y0011 = GSVector4::loadl(&p[0]).yyyy(GSVector4::loadl(&p[1]));
	GSVector4 y1221 = GSVector4::loadl(&p[1]).yyyy(GSVector4::loadl(&p[2])).xzzx();

	int m1 = (y0011 > y1221).mask() & 7;

	int i[3];

	i[0] = s_ysort[m1][0];
	i[1] = s_ysort[m1][1];
	i[2] = s_ysort[m1][2];

	const GSVector4 v0 = GSVector4::loadl(&p[i[0]]);
	const GSVector4 v1 = GSVector4::loadl(&p[i[1]]);
	const GSVector4 v2 = GSVector4::loadl(&p[i[2]]);

	y0011 = v0.yyyy(v1);
	y1221 = v1.yyyy(v2).xzzx();

	m1 = (y0011 == y1221).mask() & 7;

	// if (i == 0) => y0 < y1 < y2
	// if (i == 1) => y0 == y1 < y2
	// if (i == 4) => y0 < y1 == y2

	if (m1 == 7)
		return; // y0 == y1 == y2

	const GSVector4 tbf = y0011.xzxz(y1221).ceil();
	const GSVector4 tbmax = tbf.max(GSVector4(0));
	const GSVector4 tbmin = tbf.min(GSVector4(H << tile_shift.y));
	const GSVector4i tb = GSVector4i(tbmax.xzyw(tbmin)); // max(y0, t) max(y1, t) min(y1, b) min(y2, b)

	GSVector4 dv0 = v1 - v0;
	GSVector4 dv1 = v2 - v0;
	GSVector4 dv2 = v2 - v1;

	GSVector4 cross = dv0 * dv1.yxwz();

	cross = (cross - cross.yxwz()).yyyy();

	int m2 = cross.upl(cross == GSVector4::zero()).mask();

	if (m2 & 2)
		return; // Degenerate triangle

	m2 &= 1;

	GSVector4 dxy01 = dv0.xyxy(dv1);

	GSVector4 dx = dxy01.xzxy(dv2);
	GSVector4 dy = dxy01.ywyx(dv2);

	GSVector4 ddx[3];

	ddx[0] = dx / dy;
	ddx[1] = ddx[0].yxzw();
	ddx[2] = ddx[0].xzyw();

	if (m1 & 1)
	{
		if (tb.y < tb.w)
		{
			const GSVector2 x0(p[i[1 - m2]].x, p[i[m2]].x);
			const float y0 = p[i[1 - m2]].y;
			const GSVector2 ddx1(ddx[!m2 << 1].y, ddx[!m2 << 1].z);

			CoarseRasterizeTriangleSection(tb.x, tb.w, x0, y0, ddx1, tile_size, tile_shift, expand, grid);
		}
	}
	else
	{
		if (tb.x < tb.z)
		{
			const GSVector2 x0(v0.x, v0.x);
			const float y0 = v0.y;
			const GSVector2 ddx1(ddx[m2].x, ddx[m2].y);

			CoarseRasterizeTriangleSection(tb.x, tb.z, x0, y0, ddx1, tile_size, tile_shift, expand, grid);
		}

		if (tb.y < tb.w)
		{
			const GSVector2 x0 = GSVector2(v0.x, v0.x) + GSVector2(ddx[m2].x, ddx[m2].y) * GSVector2(dv0.y);
			const float y0 = v1.y;
			const GSVector2 ddx1(ddx[!m2 << 1].y, ddx[!m2 << 1].z);

			CoarseRasterizeTriangleSection(tb.y, tb.w, x0, y0, ddx1, tile_size, tile_shift, expand, grid);
		}
	}
}

// Convert the grid of booleans to a list of tiles.
template<u32 W, u32 H, typename BitfieldType, bool SWAP_XY>
static __forceinline void ConvertGridToTiles(const GSVector4i& area, const GSVector2i& tile_shift,
	const BitGrid<W, H, BitfieldType>& grid, std::vector<GSVector4i>& tiles_out)
{
	tiles_out.reserve(W * H);
	const GSVector4i tileoffset = area.xyxy();
	constexpr u32 BITWIDTH = BitGrid<W, H, BitfieldType>::BITWIDTH;
	for (u32 y = 0; y < H; y++)
	{
		for (u32 x = 0; x < W; x += BITWIDTH)
		{
			if (grid.GetBitfield(x, y))
			{
				const BitfieldType bitfield = grid.GetBitfield(x, y);
				u32 x0 = 0;
				while (x0 <= BITWIDTH)
				{
					x0 += std::countr_zero<BitfieldType>(bitfield >> x0);
					const u32 x1 = x0 + std::countr_one<BitfieldType>(bitfield >> x0);

					if (x0 < x1)
					{
						GSVector4i tile(x0 << tile_shift.x, y << tile_shift.y, x1 << tile_shift.x, (y + 1) << tile_shift.y);
						tile = (tile + tileoffset).rintersect(area);
						if (!tile.rempty())
							tiles_out.push_back(SwapXY<SWAP_XY>(tile));
						x0 = x1;
					}
					else
					{
						break;
					}
				}
			}
		}
	}
}

static constexpr u32 MIN_TILE_SHIFT = 4; // Minimum of 16x16 tile size.

template<u32 W, u32 H>
static __forceinline void RoundTileSize(const GSVector4i& area, GSVector2i& tile_size, GSVector2i& tile_shift)
{
	tile_size = GSVector2i((area.width() + W - 1) / W, (area.height() + H - 1) / H);
	tile_shift = GSVector2i(MIN_TILE_SHIFT);
	while ((1 << tile_shift.x) < tile_size.x)
		tile_shift.x++;
	while ((1 << tile_shift.y) < tile_size.y)
		tile_shift.y++;
	tile_size = GSVector2i(1 << tile_shift.x, 1 << tile_shift.y);
}

template<u32 W, u32 H, typename BitfieldType, bool SWAP_XY>
static __forceinline void CoarseRasterizeRects(GSVector4i area, const GSVector4i* rects, const u32 num_rects,
	const int expand, std::vector<GSVector4i>& tiles_out)
{
	area = SwapXY<SWAP_XY>(area);

	// Get the tile size.
	GSVector2i tile_size, tile_shift;
	RoundTileSize<W, H>(area, tile_size, tile_shift);

	// Rasterize rects into the bit grid.
	BitGrid<W, H, BitfieldType> grid;
	const GSVector4i offset = area.xyxy();
	for (u32 i = 0; i < num_rects; i++)
	{
		const GSVector4i rect = SwapXY<SWAP_XY>(rects[i]) - offset;
		CoarseRasterizeRect<W, H, BitfieldType>(rect, tile_size, tile_shift, expand, grid);
	}

	// Convert grid to tiles.
	ConvertGridToTiles<W, H, BitfieldType, SWAP_XY>(area, tile_shift, grid, tiles_out);
}

template<u32 W, u32 H, typename BitfieldType, bool SWAP_XY>
static __forceinline void CoarseRasterizeTriangles(
	GSVector4i area, const GSVector4* RESTRICT pos, const u32 npos,
	const int expand, std::vector<GSVector4i>& tiles_out)
{
	area = SwapXY<SWAP_XY>(area);

	// Get the tile size.
	GSVector2i tile_size, tile_shift;
	RoundTileSize<W, H>(area, tile_size, tile_shift);

	// Rasterize triangles into the bit grid.
	BitGrid<W, H, BitfieldType> grid;
	GSVector4 offset(area.xyxy());
	for (u32 i = 0; i < npos; i += 3)
	{
		const GSVector4 p[3] = {
			SwapXY<SWAP_XY>(pos[i + 0]).xyxy() - offset,
			SwapXY<SWAP_XY>(pos[i + 1]).xyxy() - offset,
			SwapXY<SWAP_XY>(pos[i + 2]).xyxy() - offset,
		};

		GSVector4 bbox = p[0].min(p[1]).xyzw(p[0].max(p[1]));
		bbox = bbox.min(p[2]).xyzw(bbox.max(p[2]));
		GSVector4i bboxi(bbox.floor().xyzw(bbox.ceil()));

		if (bboxi.width() <= tile_size.x || bboxi.height() <= tile_size.y)
		{
			// Don't rasterize small triangles; the extra accuracy is likely wasted.
			CoarseRasterizeRect<W, H, BitfieldType>(bboxi, tile_size, tile_shift, expand, grid);
		}
		else
		{
			CoarseRasterizeTriangle<W, H, BitfieldType>(p, tile_size, tile_shift, expand, grid);
		}
	}
	
	// Convert grid to tiles.
	ConvertGridToTiles<W, H, BitfieldType, SWAP_XY>(area, tile_shift, grid, tiles_out);
}

void GSUtil::CoarseRasterizeRects(const GSVector4i& area, const GSVector4i* rects, const u32 num_rects,
	const u32 grid_size, const int expand, std::vector<GSVector4i>& tiles_out)
{
	switch (grid_size)
	{
		case 8:
			if (area.width() >= area.height())
				::CoarseRasterizeRects<8, 8, u8, false>(area, rects, num_rects, expand, tiles_out);
			else
				::CoarseRasterizeRects<8, 8, u8, true>(area, rects, num_rects, expand, tiles_out);
			break;
		case 16:
			if (area.width() >= area.height())
				::CoarseRasterizeRects<16, 16, u16, false>(area, rects, num_rects, expand, tiles_out);
			else
				::CoarseRasterizeRects<16, 16, u16, true>(area, rects, num_rects, expand, tiles_out);
			break;
		case 32:
			if (area.width() >= area.height())
				::CoarseRasterizeRects<32, 32, u32, false>(area, rects, num_rects, expand, tiles_out);
			else
				::CoarseRasterizeRects<32, 32, u32, true>(area, rects, num_rects, expand, tiles_out);
			break;
		case 64:
			if (area.width() >= area.height())
				::CoarseRasterizeRects<64, 64, u64, false>(area, rects, num_rects, expand, tiles_out);
			else
				::CoarseRasterizeRects<64, 64, u64, true>(area, rects, num_rects, expand, tiles_out);
			break;
		default:
			pxAssert(false);
			break;
	}
}

void GSUtil::CoarseRasterizeTriangles(GSVector4i area, const GSVector4* RESTRICT pos, const u32 num_pos,
	const u32 grid_size, const int expand, std::vector<GSVector4i>& tiles_out)
{
	switch (grid_size)
	{
		case 8:
			if (area.width() >= area.height())
				::CoarseRasterizeTriangles<8, 8, u8, false>(area, pos, num_pos, expand, tiles_out);
			else
				::CoarseRasterizeTriangles<8, 8, u8, true>(area, pos, num_pos, expand, tiles_out);
			break;
		case 16:
			if (area.width() >= area.height())
				::CoarseRasterizeTriangles<16, 16, u16, false>(area, pos, num_pos, expand, tiles_out);
			else
				::CoarseRasterizeTriangles<16, 16, u16, true>(area, pos, num_pos, expand, tiles_out);
			break;
		case 32:
			if (area.width() >= area.height())
				::CoarseRasterizeTriangles<32, 32, u32, false>(area, pos, num_pos, expand, tiles_out);
			else
				::CoarseRasterizeTriangles<32, 32, u32, true>(area, pos, num_pos, expand, tiles_out);
			break;
		case 64:
			if (area.width() >= area.height())
				::CoarseRasterizeTriangles<64, 64, u64, false>(area, pos, num_pos, expand, tiles_out);
			else
				::CoarseRasterizeTriangles<64, 64, u64, true>(area, pos, num_pos, expand, tiles_out);
			break;
		default:
			pxAssert(false);
			break;
	}
}