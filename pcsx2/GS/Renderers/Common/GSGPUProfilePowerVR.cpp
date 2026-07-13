// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Common/GSGPUProfilePrivate.h"

namespace GpuProfileDetail
{
bool LooksLikePowerVR(std::string_view lowered_hints)
{
	// "img" is far too broad for a substring search (it appears in unrelated model/property values).
	return ContainsAny(lowered_hints, {"imagination technologies", "imgtec", "powervr"});
}

ResolvedGpuProfile ResolvePowerVRProfile(std::string_view lowered_hints)
{
	ResolvedGpuProfile resolved;
	resolved.gpu.architecture = MobileGpuArchitecture::PowerVR;
	resolved.gpu.name = "PowerVR";
	resolved.tuning = MakeMobileGsTuning(72, 6, 72, 5);

	if (ContainsAny(lowered_hints, {"b-series", "bxs", "bxt", "d-series", "dxt", "dmtp"}))
	{
		resolved.gpu.recognized = true;
		resolved.gpu.name = ContainsAny(lowered_hints, {"d-series", "dxt", "dmtp"}) ?
			"PowerVR D-Series" : "PowerVR B-Series";
		resolved.tuning = MakeMobileGsTuning(112, 8, 112, 7);
	}
	return resolved;
}
} // namespace GpuProfileDetail
