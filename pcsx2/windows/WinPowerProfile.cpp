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
#include <powrprof.h>

// Checks the current active power plan
// If the power plan isn't named 'high performance', put a little message in the console
// This function fails silently
void CheckIsUserOnHighPerfPowerPlan()
{
	GUID* pPwrGUID;
	DWORD ret = PowerGetActiveScheme(NULL, &pPwrGUID);
	if (ret != ERROR_SUCCESS)
		return;

	UCHAR aBuffer[2048];
	DWORD aBufferSize = sizeof(aBuffer);
	ret = PowerReadFriendlyName(NULL, pPwrGUID, &NO_SUBGROUP_GUID, NULL, aBuffer, &aBufferSize);
	if (ret != ERROR_SUCCESS)
		goto cleanup;

	DWORD acMax = 0,acMin = 0,dcMax = 0,dcMin = 0;

	if (PowerReadACValueIndex(NULL, pPwrGUID, &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MAXIMUM, &acMax) ||
		PowerReadACValueIndex(NULL, pPwrGUID, &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MINIMUM, &acMin) ||
		PowerReadDCValueIndex(NULL, pPwrGUID, &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MAXIMUM, &dcMax) ||
		PowerReadDCValueIndex(NULL, pPwrGUID, &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MINIMUM, &dcMin))
		goto cleanup;

	Console.WriteLn("The current power profile is '%S'.\nThe current min / max processor states\nAC: %d%% / %d%%\nBattery: %d%% / %d%%\n", aBuffer,acMin,acMax,dcMin,dcMax);

cleanup:
	LocalFree(pPwrGUID);
	return;
}
