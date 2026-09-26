// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

// DLSS-NR post-processing filter, through libframe (3rdparty/dlss-nr-on-vulkan, libdlssnr).
// One RGB frame in, one RGB frame out, the same way src/ref/nr_frame_main.c drives the library,
// with the previous output and input kept as the temporal history.
//
// Only available in builds configured with -DUSE_DLSSNR=ON, which link the model weights
// the owner supplies into the library. Needs proper testing; it is slow (the whole network
// runs every frame on the CPU-visible path).
namespace GSDLSSNR
{
	/// True when this build links libdlssnr.
	bool IsAvailable();

	/// Filters an RGBA8 image in place (alpha is kept). Opens the model on first use.
	/// Returns false if the filter isn't available or failed; the image is then untouched.
	bool Process(u8* rgba, u32 width, u32 height, u32 stride, float intensity);

	/// Forgets the temporal history (e.g. after a resolution change or when toggled).
	void ResetHistory();

	/// Closes the model and releases its device.
	void Shutdown();
} // namespace GSDLSSNR
