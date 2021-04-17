/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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
#include "SoundTouch.h"

namespace SoundtouchCfg
{
	// Timestretch Slider Bounds, Min/Max
	const int SequenceLen_Min = 20;
	const int SequenceLen_Max = 100;

	const int SeekWindow_Min = 10;
	const int SeekWindow_Max = 30;

	const int Overlap_Min = 5;
	const int Overlap_Max = 15;

	int SequenceLenMS = 30;
	int SeekWindowMS = 20;
	int OverlapMS = 10;

	static void ClampValues()
	{
		Clampify(SequenceLenMS, SequenceLen_Min, SequenceLen_Max);
		Clampify(SeekWindowMS, SeekWindow_Min, SeekWindow_Max);
		Clampify(OverlapMS, Overlap_Min, Overlap_Max);
	}

	void ApplySettings(soundtouch::SoundTouch& sndtouch)
	{
		sndtouch.setSetting(SETTING_SEQUENCE_MS, SequenceLenMS);
		sndtouch.setSetting(SETTING_SEEKWINDOW_MS, SeekWindowMS);
		sndtouch.setSetting(SETTING_OVERLAP_MS, OverlapMS);
	}

	void ReadSettings()
	{
		SequenceLenMS = CfgReadInt(L"SOUNDTOUCH", L"SequenceLengthMS", 30);
		SeekWindowMS = CfgReadInt(L"SOUNDTOUCH", L"SeekWindowMS", 20);
		OverlapMS = CfgReadInt(L"SOUNDTOUCH", L"OverlapMS", 10);

		ClampValues();
		WriteSettings();
	}

	void WriteSettings()
	{
		CfgWriteInt(L"SOUNDTOUCH", L"SequenceLengthMS", SequenceLenMS);
		CfgWriteInt(L"SOUNDTOUCH", L"SeekWindowMS", SeekWindowMS);
		CfgWriteInt(L"SOUNDTOUCH", L"OverlapMS", OverlapMS);
	}

} // namespace SoundtouchCfg
