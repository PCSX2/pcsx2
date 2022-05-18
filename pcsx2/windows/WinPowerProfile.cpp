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
#include "common/RedtapeWindows.h"

#include <powrprof.h>
#include <wil/com.h>

using unique_any_guidptr = wil::unique_any<GUID*, decltype(&::LocalFree), ::LocalFree>;

// Checks the current active power plan
// This function fails silently
void CheckIsUserOnHighPerfPowerPlan()
{
	unique_any_guidptr pPwrGUID;
	DWORD ret = PowerGetActiveScheme(NULL, pPwrGUID.put());
	if (ret != ERROR_SUCCESS)
		return;

	UCHAR aBuffer[2048];
	DWORD aBufferSize = sizeof(aBuffer);
	ret = PowerReadFriendlyName(NULL, pPwrGUID.get(), &NO_SUBGROUP_GUID, NULL, aBuffer, &aBufferSize);
	std::string friendlyName(StringUtil::WideStringToUTF8String((wchar_t*)aBuffer));
	if (ret != ERROR_SUCCESS)
		return;

	DWORD acMax = 0,acMin = 0,dcMax = 0,dcMin = 0;

	if (PowerReadACValueIndex(NULL, pPwrGUID.get(), &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MAXIMUM, &acMax) ||
		PowerReadACValueIndex(NULL, pPwrGUID.get(), &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MINIMUM, &acMin) ||
		PowerReadDCValueIndex(NULL, pPwrGUID.get(), &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MAXIMUM, &dcMax) ||
		PowerReadDCValueIndex(NULL, pPwrGUID.get(), &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MINIMUM, &dcMin))
		return;

	Console.WriteLn("The current power profile is '%s'.\nThe current min / max processor states\nAC: %d%% / %d%%\nBattery: %d%% / %d%%\n", friendlyName.c_str(), acMin, acMax, dcMin, dcMax);
}
