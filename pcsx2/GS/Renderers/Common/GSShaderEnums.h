// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Enums that are used by shaders, to be included by both C++ and shader code
// Assume that cstdint types are available, but don't actually include the header (it's not available in shaders)
// (Currently only used by Metal)

#pragma once

namespace GSShader {

enum class VSExpand : uint8_t
{
	None   = 0,
	Point  = 1,
	Line   = 2,
	Sprite = 3,
};

enum class PS_ATST : uint32_t
{
	NONE = 0,
	LEQUAL = 1,
	GEQUAL = 2,
	EQUAL = 3,
	NOTEQUAL = 4
};

// Identical with the usual GS enum except for the RGB_ONLY_DSB
enum class PS_AFAIL : uint32_t
{
	KEEP = 0,
	FB_ONLY = 1,
	ZB_ONLY = 2,
	RGB_ONLY = 3,
	RGB_ONLY_DSB = 4 // RGB only with dual source blend.
};

} // namespace GSShader
