// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// SPU2/spu2_neon.h — ARM64 SPU2 backend registration API.
//
// Declares RegisterNEONBackend(), which overrides the reverb FIR function
// pointers (ReverbDownsample / ReverbUpsample, see SPU2/defs.h) with NEON
// implementations at runtime. Only meaningful on ARM64; on other targets
// spu2_neon.cpp produces no object code and this entry point is never called.
//
// Call once from SPU2::InternalReset(), AFTER the Multi-ISA defaults are
// assigned, and only when the user has opted in via the "NeonReverbSIMD"
// setting (gated by the caller).

#pragma once

namespace SPU2
{
	// Point ReverbDownsample / ReverbUpsample at the NEON FIR implementations.
	// Safe to call repeatedly; falls back to the scalar reference if the NEON
	// path is unavailable.
	void RegisterNEONBackend();
} // namespace SPU2
