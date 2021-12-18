/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2015  PCSX2 Dev Team
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

#ifdef _WIN32
#include "common/RedtapeWindows.h"

//This ensures that the nVidia graphics card is used for PCSX2 on an Optimus-enabled system.
//302 or higher driver required.
extern "C" {
	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

//This is the equivalent for an AMD system.
//13.35 or newer driver required.
extern "C" {
	_declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#endif
