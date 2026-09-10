// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"
#include "common/Pcsx2Defs.h"

namespace GroovyMiSTer
{
	// A CRT modeline, in the shape gmw_switchres() wants.
	struct Modeline
	{
		double pclock = 0.0; // pixel clock, MHz
		u16 h_active = 0, h_begin = 0, h_end = 0, h_total = 0;
		u16 v_active = 0, v_begin = 0, v_end = 0, v_total = 0;
		u8 interlace = 0; // 0 progressive, 1 interlaced field, 2 progressive FB over interlaced

		bool operator==(const Modeline& r) const
		{
			return pclock == r.pclock && h_active == r.h_active && h_begin == r.h_begin &&
			       h_end == r.h_end && h_total == r.h_total && v_active == r.v_active &&
			       v_begin == r.v_begin && v_end == r.v_end && v_total == r.v_total &&
			       interlace == r.interlace;
		}
		bool operator!=(const Modeline& r) const { return !(*this == r); }
	};

	/// Structural sanity: zero-sized, or blanking that does not enclose the active area.
	/// Such a modeline drives the FPGA's PLL into an undefined state, so it is rejected
	/// regardless of the safety-cap setting.
	constexpr bool IsWellFormed(const Modeline& m)
	{
		return m.pclock > 0.0 &&
		       m.h_active > 0 && m.v_active > 0 &&
		       m.h_begin >= m.h_active && m.h_end >= m.h_begin && m.h_total > m.h_end &&
		       m.v_begin >= m.v_active && m.v_end >= m.v_begin && m.v_total > m.v_end;
	}

	/// Bytes the FPGA client will stream for one blit of this modeline.
	///
	/// A true-field stream (interlace == 1) sends one half-height field per blit; the other
	/// two modes send every line every time. `bytes_per_pixel` is the wire format's, from
	/// GroovyMiSTer::BytesPerPixel() - a parameter so this header stays dependency-free.
	constexpr u32 BlitBytes(const Modeline& m, u32 bytes_per_pixel)
	{
		const u32 lines = (m.interlace == static_cast<u8>(GroovyMiSTerInterlace::Field)) ?
			(static_cast<u32>(m.v_active) / 2u) : static_cast<u32>(m.v_active);
		return static_cast<u32>(m.h_active) * lines * bytes_per_pixel;
	}

	/// Does one blit fit the client's fixed-size buffer?
	///
	/// The vendored client allocates its blit buffers once at BUFFER_SIZE and derives the
	/// stream length from the modeline it is given, with no clamp in between, so an
	/// oversized mode walks off the end of a RIO-registered allocation. A property of the
	/// client rather than of the display, so it is always enforced (see MAX_BLIT_BYTES).
	constexpr bool FitsBlitBuffer(const Modeline& m, u32 bytes_per_pixel)
	{
		return BlitBytes(m, bytes_per_pixel) <= Pcsx2Config::GroovyMiSTerOptions::MAX_BLIT_BYTES;
	}

	/// The CRT safety cap.
	///
	/// An arcade or consumer CRT driven far outside its designed envelope can be damaged;
	/// the horizontal output stage and flyback are what let go. Conservative and on by
	/// default (EmuCore/GroovyMiSTer CrtSafetyCap). A multisync or genuine 31kHz display
	/// can turn it off, with the UI asking for acknowledgement first.
	constexpr bool IsWithinCrtSafeEnvelope(const Modeline& m)
	{
		return m.v_active <= Pcsx2Config::GroovyMiSTerOptions::MAX_SAFE_V_ACTIVE &&
		       m.h_active <= Pcsx2Config::GroovyMiSTerOptions::MAX_SAFE_H_ACTIVE;
	}

	/// Full gate applied before any modeline is sent to the FPGA.
	///
	/// Well-formedness and the blit-buffer budget are structural - an undefined PLL state
	/// and a heap overrun respectively - and never optional. Only the CRT envelope cap
	/// follows the CrtSafetyCap setting.
	constexpr bool IsModelineAcceptable(const Modeline& m, u32 bytes_per_pixel, bool enforce_cap)
	{
		return IsWellFormed(m) && FitsBlitBuffer(m, bytes_per_pixel) &&
		       (!enforce_cap || IsWithinCrtSafeEnvelope(m));
	}
} // namespace GroovyMiSTer
