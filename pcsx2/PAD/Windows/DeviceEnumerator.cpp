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
#include "Global.h"
#include "InputManager.h"

#include "DeviceEnumerator.h"
#ifdef _WIN32
#include "WindowsMessaging.h"
#include "DirectInput.h"
#include "PADRawInput.h"
#include "XInputEnum.h"
#include "HidDevice.h"
#include "DualShock3.h"
#endif

#ifdef __linux__
#include "Linux/KeyboardMouse.h"
#include "Linux/JoyEvdev.h"
#endif

void EnumDevices(int hideDXXinput)
{
	// Needed for enumeration of some device types.
	dm->ReleaseInput();
	InputDeviceManager* oldDm = dm;
	dm = new InputDeviceManager();

#ifdef _MSC_VER
	EnumWindowsMessagingDevices();
	EnumRawInputDevices();
	EnumDualShock3s();
	EnumXInputDevices();
	EnumDirectInputDevices(hideDXXinput);
#else
	EnumLnx();
	EnumJoystickEvdev();
#endif

	dm->CopyBindings(oldDm->numDevices, oldDm->devices);

	delete oldDm;
}
