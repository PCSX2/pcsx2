// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Enums that are used by shaders, to be included by both C++ and shader code
// Assume that cstdint types are available, but don't actually include the header (it's not available in shaders)
// (Currently only used by Metal)

#pragma once

namespace GSShader {

enum class VSExpand : uint8_t
{
	None        = 0,
	Point       = 1,
	Line        = 2,
	Sprite      = 3,
	LineAA1     = 4,
	TriangleAA1 = 5,
};

enum class PS_ATST : uint32_t
{
	NONE = 0,
	LEQUAL = 1,
	GEQUAL = 2,
	EQUAL = 3,
	NOTEQUAL = 4
};

// Identical to the usual GS enum except for RGB_ONLY_DSB and RGB_ONLY_SW_Z
enum class PS_AFAIL : uint32_t
{
	KEEP = 0,          ///< Hardware discard
	FB_ONLY = 1,       ///< FB only with software Z discard
	ZB_ONLY = 2,       ///< ZB only with software RGBA discard
	RGB_ONLY = 3,      ///< RGB only with hardware Z discard and software A discard
	RGB_ONLY_DSB = 4,  ///< RGB only with dual source blend
	RGB_ONLY_SW_Z = 5, ///< RGB only with software Z discard
};

// Identical to GS_ZTST
enum class ZTST : uint32_t
{
	NEVER   = 0,
	ALWAYS  = 1,
	GEQUAL  = 2,
	GREATER = 3,
};

enum class PS_AA1 : uint32_t
{
	NONE          = 0, ///< No AA1
	LINE          = 1, ///< AA1 lines
	TRIANGLE      = 2, ///< AA1 triangles
	TRIANGLE_SW_Z = 3, ///< AA1 triangles with software Z discard
};

} // namespace GSShader
