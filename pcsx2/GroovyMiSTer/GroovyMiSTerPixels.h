// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"
#include "common/Pcsx2Defs.h"

#include <vector>

namespace GroovyMiSTer
{
	/// Bytes per pixel on the wire.
	constexpr u32 BytesPerPixel(GroovyMiSTerRgbMode mode)
	{
		switch (mode)
		{
			case GroovyMiSTerRgbMode::RGB565: return 2;
			case GroovyMiSTerRgbMode::RGBA8888: return 4;
			default: return 3;
		}
	}

	/// Convert a PCSX2 readback (RGBA8: byte 0 = R, 1 = G, 2 = B, 3 = A) into the MiSTer's
	/// wire layout, which is not what the format names suggest:
	///
	///     RGB888   -> B, G, R
	///     RGBA8888 -> B, G, R, A   (4th byte ignored by the core)
	///     RGB565   -> little-endian u16, (r << 11) | (g << 5) | b
	///
	/// From the FPGA RTL (Groovy.sv, decode_pixel), which unpacks a pixel as
	/// `{r,g,b} <= word64[0 +: 24]`: a Verilog concatenation, so `b` occupies the
	/// least-significant byte, and DDR being little-endian makes stream byte 0 blue. The
	/// source is RGBA, so every mode needs a channel swap and none is a memcpy. Covered by
	/// tests/ctest/core/groovy_mister_tests.cpp.
	///
	/// `src_pitch` is in bytes and may exceed width * 4 (download textures are often padded).
	void PackFrame(GroovyMiSTerRgbMode mode, const u8* src, u32 src_pitch, u32 width, u32 height,
		std::vector<u8>& dst);
} // namespace GroovyMiSTer
