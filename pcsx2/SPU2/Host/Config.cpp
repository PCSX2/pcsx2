/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "common/StringUtil.h"

#include <cmath>

#include "SPU2/Global.h"
#include "SPU2/Host/Config.h"
#include "SPU2/Host/Dialogs.h"
#include "HostSettings.h"

int AutoDMAPlayRate[2] = {0, 0};

// Default settings.

// MIXING
int Interpolation = 5;
/* values:
		0: No interpolation (uses nearest)
		1. Linear interpolation
		2. Cubic interpolation
		3. Hermite interpolation
		4. Catmull-Rom interpolation
		5. Gaussian interpolation
*/

float FinalVolume; // global
bool AdvancedVolumeControl;
float VolumeAdjustFLdb; // Decibels settings, because audiophiles love that.
float VolumeAdjustCdb;
float VolumeAdjustFRdb;
float VolumeAdjustBLdb;
float VolumeAdjustBRdb;
float VolumeAdjustSLdb;
float VolumeAdjustSRdb;
float VolumeAdjustLFEdb;
float VolumeAdjustFL; // Linear coefficients calculated from decibels,
float VolumeAdjustC;
float VolumeAdjustFR;
float VolumeAdjustBL;
float VolumeAdjustBR;
float VolumeAdjustSL;
float VolumeAdjustSR;
float VolumeAdjustLFE;

bool _visual_debug_enabled = false; // Windows-only feature

// OUTPUT
u32 OutputModule = 0;
int SndOutLatencyMS = 100;
int SynchMode = 0; // Time Stretch, Async or Disabled.

int numSpeakers = 0;
int dplLevel = 0;
bool temp_debug_state;

/*****************************************************************************/

void ReadSettings()
{
	Interpolation = Host::GetIntSettingValue("SPU2/Mixing", "Interpolation", 5);
	FinalVolume = ((float)Host::GetIntSettingValue("SPU2/Mixing", "FinalVolume", 100)) / 100;
	if (FinalVolume > 1.0f)
		FinalVolume = 1.0f;

	AdvancedVolumeControl = Host::GetBoolSettingValue("SPU2/Mixing", "AdvancedVolumeControl", false);
	VolumeAdjustCdb = Host::GetFloatSettingValue("SPU2/Mixing", "VolumeAdjustC", 0);
	VolumeAdjustFLdb = Host::GetFloatSettingValue("SPU2/Mixing", "VolumeAdjustFL", 0);
	VolumeAdjustFRdb = Host::GetFloatSettingValue("SPU2/Mixing", "VolumeAdjustFR", 0);
	VolumeAdjustBLdb = Host::GetFloatSettingValue("SPU2/Mixing", "VolumeAdjustBL", 0);
	VolumeAdjustBRdb = Host::GetFloatSettingValue("SPU2/Mixing", "VolumeAdjustBR", 0);
	VolumeAdjustSLdb = Host::GetFloatSettingValue("SPU2/Mixing", "VolumeAdjustSL", 0);
	VolumeAdjustSRdb = Host::GetFloatSettingValue("SPU2/Mixing", "VolumeAdjustSR", 0);
	VolumeAdjustLFEdb = Host::GetFloatSettingValue("SPU2/Mixing", "VolumeAdjustLFE", 0);
	VolumeAdjustC = powf(10, VolumeAdjustCdb / 10);
	VolumeAdjustFL = powf(10, VolumeAdjustFLdb / 10);
	VolumeAdjustFR = powf(10, VolumeAdjustFRdb / 10);
	VolumeAdjustBL = powf(10, VolumeAdjustBLdb / 10);
	VolumeAdjustBR = powf(10, VolumeAdjustBRdb / 10);
	VolumeAdjustSL = powf(10, VolumeAdjustSLdb / 10);
	VolumeAdjustSR = powf(10, VolumeAdjustSRdb / 10);
	VolumeAdjustLFE = powf(10, VolumeAdjustLFEdb / 10);

	const std::string modname(Host::GetStringSettingValue("SPU2/Output", "OutputModule", "cubeb"));
	OutputModule = FindOutputModuleById(modname.c_str()); // Find the driver index of this module...

	SndOutLatencyMS = Host::GetIntSettingValue("SPU2/Output", "Latency", 100);
	SynchMode = Host::GetIntSettingValue("SPU2/Output", "SynchMode", 0);
	numSpeakers = Host::GetIntSettingValue("SPU2/Output", "SpeakerConfiguration", 0);

	SoundtouchCfg::ReadSettings();
	DebugConfig::ReadSettings();

	// Sanity Checks
	// -------------

	Clampify(SndOutLatencyMS, LATENCY_MIN, LATENCY_MAX);

	if (mods[OutputModule] == nullptr)
	{
		Console.Warning("* SPU2: Unknown output module '%s' specified in configuration file.", modname.c_str());
		Console.Warning("* SPU2: Defaulting to Cubeb (%s).", CubebOut->GetIdent());
		OutputModule = FindOutputModuleById(CubebOut->GetIdent());
	}
}
