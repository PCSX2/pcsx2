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
#include "common/DynamicLibrary.h"
#include <windows.h>
#include <setupapi.h>
#include "hidapi.h"

_HidD_GetHidGuid HidD_GetHidGuid = nullptr;
_HidD_GetAttributes HidD_GetAttributes = nullptr;
_HidD_GetPreparsedData HidD_GetPreparsedData = nullptr;
_HidP_GetCaps HidP_GetCaps = nullptr;
_HidD_FreePreparsedData HidD_FreePreparsedData = nullptr;
_HidD_GetFeature HidD_GetFeature = nullptr;
_HidD_SetFeature HidD_SetFeature = nullptr;
_HidP_GetSpecificButtonCaps HidP_GetSpecificButtonCaps = nullptr;
_HidP_GetButtonCaps HidP_GetButtonCaps = nullptr;
_HidP_GetUsages HidP_GetUsages = nullptr;
_HidP_GetValueCaps HidP_GetValueCaps = nullptr;
_HidP_GetUsageValue HidP_GetUsageValue = nullptr;
_HidD_GetProductString HidD_GetProductString = nullptr;

static Common::DynamicLibrary s_hid_lib;

int InitHid()
{
	if (s_hid_lib.IsOpen())
		return 1;

	if (!s_hid_lib.Open("hid"))
		return 0;

	HidP_GetUsages = s_hid_lib.GetSymbol<_HidP_GetUsages>("HidP_GetUsages");
	HidP_GetCaps   = s_hid_lib.GetSymbol<_HidP_GetCaps>("HidP_GetCaps");

	HidD_GetHidGuid     = s_hid_lib.GetSymbol<_HidD_GetHidGuid>("HidD_GetHidGuid");
	HidD_GetAttributes  = s_hid_lib.GetSymbol<_HidD_GetAttributes>("HidD_GetAttributes");

	HidP_GetButtonCaps = s_hid_lib.GetSymbol<_HidP_GetButtonCaps>("HidP_GetButtonCaps");
	HidP_GetValueCaps = s_hid_lib.GetSymbol<_HidP_GetValueCaps>("HidP_GetValueCaps");

	HidD_GetPreparsedData  = s_hid_lib.GetSymbol<_HidD_GetPreparsedData>("HidD_GetPreparsedData");
	HidD_FreePreparsedData = s_hid_lib.GetSymbol<_HidD_FreePreparsedData>("HidD_FreePreparsedData");

	HidP_GetSpecificButtonCaps = s_hid_lib.GetSymbol<_HidP_GetSpecificButtonCaps>("HidP_GetSpecificButtonCaps");

	HidP_GetUsageValue    = s_hid_lib.GetSymbol<_HidP_GetUsageValue>("HidP_GetUsageValue");
	HidD_GetProductString = s_hid_lib.GetSymbol<_HidD_GetProductString>("HidD_GetProductString");

	HidD_GetFeature = s_hid_lib.GetSymbol<_HidD_GetFeature>("HidD_GetFeature");
	HidD_SetFeature = s_hid_lib.GetSymbol<_HidD_SetFeature>("HidD_SetFeature");

	const bool loaded =
		HidP_GetUsages && HidP_GetCaps && HidD_GetHidGuid &&
		HidD_GetAttributes && HidP_GetButtonCaps && HidP_GetValueCaps &&
		HidD_GetPreparsedData && HidD_FreePreparsedData && HidP_GetSpecificButtonCaps &&
		HidP_GetUsageValue && HidD_GetProductString && HidD_GetFeature && HidD_SetFeature;

	if(loaded)
		return 1;

	UninitHid();

	return 0;
}

void UninitHid()
{
	HidP_GetUsages = nullptr;
	HidP_GetCaps = nullptr;
	HidD_GetHidGuid = nullptr;

	HidD_GetAttributes = nullptr;
	HidP_GetButtonCaps = nullptr;
	HidP_GetValueCaps = nullptr;
	HidD_GetPreparsedData = nullptr;

	HidD_FreePreparsedData = nullptr;
	HidP_GetSpecificButtonCaps = nullptr;
	HidP_GetUsageValue = nullptr;
	HidD_GetProductString = nullptr;

	HidD_GetFeature = nullptr;
	HidD_SetFeature = nullptr;

	s_hid_lib.Close();
}
