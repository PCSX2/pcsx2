/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2019  PCSX2 Dev Team
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

#pragma once

#include "PadData.h"


#ifndef DISABLE_RECORDING
class RecordingInputManager
{
public:
	RecordingInputManager();

	void ControllerInterrupt(u8 &data, u8 &port, u16 & BufCount, u8 buf[]);
	// Handles normal keys
	void SetButtonState(int port, PadData_NormalButton button, int pressure);
	// Handles analog sticks
	void UpdateAnalog(int port, PadData_AnalogVector vector, int value);
	void SetVirtualPadReading(int port, bool read);

protected:
	PadData pad;
	bool virtualPad[2];
};

extern RecordingInputManager g_RecordingInput;
#endif
