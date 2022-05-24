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
#include "SPU2/Global.h"
#include "Dialogs.h"
#include "Config.h"
#include "gui/StringHelpers.h"

#if defined(__unix__) || defined(__APPLE__)
#include <SDL.h>
#include <SDL_audio.h>
#include "SPU2/wx/wxConfig.h"
#endif

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
	// For some reason this can be called before we know what ini file we're writing to.
	// Let's not try to read it if that happens.
	if (!pathSet)
		initIni();

	Interpolation = CfgReadInt(L"MIXING", L"Interpolation", 5);
	FinalVolume = ((float)CfgReadInt(L"MIXING", L"FinalVolume", 100)) / 100;
	if (FinalVolume > 1.0f)
		FinalVolume = 1.0f;

	AdvancedVolumeControl = CfgReadBool(L"MIXING", L"AdvancedVolumeControl", false);
	VolumeAdjustCdb = CfgReadFloat(L"MIXING", L"VolumeAdjustC(dB)", 0);
	VolumeAdjustFLdb = CfgReadFloat(L"MIXING", L"VolumeAdjustFL(dB)", 0);
	VolumeAdjustFRdb = CfgReadFloat(L"MIXING", L"VolumeAdjustFR(dB)", 0);
	VolumeAdjustBLdb = CfgReadFloat(L"MIXING", L"VolumeAdjustBL(dB)", 0);
	VolumeAdjustBRdb = CfgReadFloat(L"MIXING", L"VolumeAdjustBR(dB)", 0);
	VolumeAdjustSLdb = CfgReadFloat(L"MIXING", L"VolumeAdjustSL(dB)", 0);
	VolumeAdjustSRdb = CfgReadFloat(L"MIXING", L"VolumeAdjustSR(dB)", 0);
	VolumeAdjustLFEdb = CfgReadFloat(L"MIXING", L"VolumeAdjustLFE(dB)", 0);
	VolumeAdjustC = powf(10, VolumeAdjustCdb / 10);
	VolumeAdjustFL = powf(10, VolumeAdjustFLdb / 10);
	VolumeAdjustFR = powf(10, VolumeAdjustFRdb / 10);
	VolumeAdjustBL = powf(10, VolumeAdjustBLdb / 10);
	VolumeAdjustBR = powf(10, VolumeAdjustBRdb / 10);
	VolumeAdjustSL = powf(10, VolumeAdjustSLdb / 10);
	VolumeAdjustSR = powf(10, VolumeAdjustSRdb / 10);
	VolumeAdjustLFE = powf(10, VolumeAdjustLFEdb / 10);

#ifdef SPU2X_CUBEB
	const SndOutModule* const defaultModule = CubebOut;
#else
	const SndOutModule* const defaultModule = NullOut;
#endif

	wxString temp;
	CfgReadStr(L"OUTPUT", L"Output_Module", temp, StringUtil::UTF8StringToWxString(defaultModule->GetIdent()).wc_str());
	OutputModule = FindOutputModuleById(temp.ToUTF8()); // Find the driver index of this module...

	SndOutLatencyMS = CfgReadInt(L"OUTPUT", L"Latency", 100);
	SynchMode = CfgReadInt(L"OUTPUT", L"Synch_Mode", 0);
	numSpeakers = CfgReadInt(L"OUTPUT", L"SpeakerConfiguration", 0);

	SoundtouchCfg::ReadSettings();
	DebugConfig::ReadSettings();

	// Sanity Checks
	// -------------

	Clampify(SndOutLatencyMS, LATENCY_MIN, LATENCY_MAX);

	if (mods[OutputModule] == nullptr)
	{
		Console.Warning("* SPU2: Unknown output module '%s' specified in configuration file.", temp.wc_str());
		Console.Warning("* SPU2: Defaulting to %s (%s).", defaultModule->GetLongName(), defaultModule->GetIdent());
		OutputModule = FindOutputModuleById(defaultModule->GetIdent());
	}

	WriteSettings();
	spuConfig->Flush();
}

/*****************************************************************************/

void WriteSettings()
{
	if (!pathSet)
	{
		FileLog("Write called without the path set.\n");
		return;
	}

	CfgWriteInt(L"MIXING", L"Interpolation", Interpolation);
	CfgWriteInt(L"MIXING", L"FinalVolume", (int)(FinalVolume * 100));

	CfgWriteBool(L"MIXING", L"AdvancedVolumeControl", AdvancedVolumeControl);
	CfgWriteFloat(L"MIXING", L"VolumeAdjustC(dB)", VolumeAdjustCdb);
	CfgWriteFloat(L"MIXING", L"VolumeAdjustFL(dB)", VolumeAdjustFLdb);
	CfgWriteFloat(L"MIXING", L"VolumeAdjustFR(dB)", VolumeAdjustFRdb);
	CfgWriteFloat(L"MIXING", L"VolumeAdjustBL(dB)", VolumeAdjustBLdb);
	CfgWriteFloat(L"MIXING", L"VolumeAdjustBR(dB)", VolumeAdjustBRdb);
	CfgWriteFloat(L"MIXING", L"VolumeAdjustSL(dB)", VolumeAdjustSLdb);
	CfgWriteFloat(L"MIXING", L"VolumeAdjustSR(dB)", VolumeAdjustSRdb);
	CfgWriteFloat(L"MIXING", L"VolumeAdjustLFE(dB)", VolumeAdjustLFEdb);

	CfgWriteStr(L"OUTPUT", L"Output_Module", mods[OutputModule]->GetIdent());
	CfgWriteInt(L"OUTPUT", L"Latency", SndOutLatencyMS);
	CfgWriteInt(L"OUTPUT", L"Synch_Mode", SynchMode);
	CfgWriteInt(L"OUTPUT", L"SpeakerConfiguration", numSpeakers);

	SoundtouchCfg::WriteSettings();
	DebugConfig::WriteSettings();
}

void configure()
{
	auto* dialog = new Dialog;

	initIni();
	ReadSettings();
	dialog->Display();
	WriteSettings();
	delete spuConfig;
	spuConfig = nullptr;
	wxDELETE(dialog);
}
