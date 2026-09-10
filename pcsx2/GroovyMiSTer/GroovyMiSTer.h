// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

class GSTexture;
class GSVector4i;

// =====================================================================================
//  GroovyMiSTer - low-latency streaming output to a MiSTer FPGA
// =====================================================================================
//
// Streams the finished PS2 frame and the mixed SPU2 audio to a MiSTer over UDP, and reads
// MiSTer-side controllers back. The MiSTer raster-chases the CRT, so a frame arrives
// microseconds before it is scanned out.
//
// This header is the whole seam into upstream PCSX2 - everything else lives under
// pcsx2/GroovyMiSTer/ and pcsx2/Input/ - so add a function here rather than reaching into
// upstream code from the module. Call sites:
//
//   GS/GS.cpp                          -> Open() / Close()
//   GS/Renderers/Common/GSRenderer.cpp -> IsActive() / OnVSync()
//   SPU2/spu2.cpp                      -> IsAudioActive() / OnAudioChunk()
//   Input/InputManager.cpp             -> registers GroovyMiSTerInputSource
//
namespace GroovyMiSTer
{
	/// Bring the output up if EmuCore/GroovyMiSTer/Enabled is set. Safe to call when
	/// disabled and safe to call twice. Called on the GS thread from GSopen(); connection
	/// failures are non-fatal and retried in the background.
	void Open();

	/// Stop the sender, tell the MiSTer we are going away so it returns to its
	/// connection-search screen, and release GPU resources. Called on the GS thread from
	/// GSclose().
	void Close();

	/// True when streaming. The guard on every hot-path hook, so it stays a single relaxed
	/// atomic load.
	bool IsActive();

	/// Hand over the finished frame. Called on the GS thread from GSRenderer::VSync(), after
	/// Merge() and before presentation, so the GPU work and network send do not queue behind
	/// the host window's present.
	///
	/// `current`  the merged frame (g_gs_device->GetCurrent()).
	/// `src_rect` the valid sub-rectangle of `current`, from CalculateDrawSrcRect().
	///            `current` can be larger than the video mode when a game flips a
	///            render-target surface bigger than it displays, and the host window
	///            blits only this sub-rect. Capturing the whole texture streams the
	///            stale region as a duplicated, offset image.
	/// `field`    the interlace field for this vsync (0/1).
	void OnVSync(GSTexture* current, const GSVector4i& src_rect, u32 field);

	/// True when audio should be mirrored to the MiSTer. Guard for OnAudioChunk().
	bool IsAudioActive();

	/// Hand over one chunk of mixed audio. Called on the SPU2 thread from spu2Output() with
	/// the same buffer SPU2 gives its own host backend: `frames` interleaved stereo float
	/// samples. Converts to s16 into a ring; the sender thread owns the socket.
	void OnAudioChunk(const float* samples, u32 frames);

	/// The console sample rate changed (PS2 48kHz <-> PS1 44.1kHz). The rate is part of the
	/// CMD_INIT handshake, so this forces a reconnect. Called from
	/// SPU2::UpdateSampleRate().
	void OnSampleRateChanged();
} // namespace GroovyMiSTer
