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

#pragma once

#include "Pcsx2Defs.h"
#include <string>
#include <vector>

class SettingsInterface
{
public:
	virtual ~SettingsInterface() = default;

	virtual bool Save() = 0;
	virtual void Clear() = 0;

	virtual bool GetIntValue(const char* section, const char* key, int* value) const = 0;
	virtual bool GetUIntValue(const char* section, const char* key, uint* value) const = 0;
	virtual bool GetFloatValue(const char* section, const char* key, float* value) const = 0;
	virtual bool GetDoubleValue(const char* section, const char* key, double* value) const = 0;
	virtual bool GetBoolValue(const char* section, const char* key, bool* value) const = 0;
	virtual bool GetStringValue(const char* section, const char* key, std::string* value) const = 0;

	virtual void SetIntValue(const char* section, const char* key, int value) = 0;
	virtual void SetUIntValue(const char* section, const char* key, uint value) = 0;
	virtual void SetFloatValue(const char* section, const char* key, float value) = 0;
	virtual void SetDoubleValue(const char* section, const char* key, double value) = 0;
	virtual void SetBoolValue(const char* section, const char* key, bool value) = 0;
	virtual void SetStringValue(const char* section, const char* key, const char* value) = 0;

	virtual std::vector<std::string> GetStringList(const char* section, const char* key) = 0;
	virtual void SetStringList(const char* section, const char* key, const std::vector<std::string>& items) = 0;
	virtual bool RemoveFromStringList(const char* section, const char* key, const char* item) = 0;
	virtual bool AddToStringList(const char* section, const char* key, const char* item) = 0;

	virtual void DeleteValue(const char* section, const char* key) = 0;
	virtual void ClearSection(const char* section) = 0;

	__fi int GetIntValue(const char* section, const char* key, int default_value = 0) const
	{
		int value;
		return GetIntValue(section, key, &value) ? value : default_value;
	}

	__fi uint GetUIntValue(const char* section, const char* key, uint default_value = 0) const
	{
		uint value;
		return GetUIntValue(section, key, &value) ? value : default_value;
	}

	__fi float GetFloatValue(const char* section, const char* key, float default_value = 0.0f) const
	{
		float value;
		return GetFloatValue(section, key, &value) ? value : default_value;
	}

	__fi float GetDoubleValue(const char* section, const char* key, double default_value = 0.0) const
	{
		double value;
		return GetDoubleValue(section, key, &value) ? value : default_value;
	}

	__fi bool GetBoolValue(const char* section, const char* key, bool default_value = false) const
	{
		bool value;
		return GetBoolValue(section, key, &value) ? value : default_value;
	}

	__fi std::string GetStringValue(const char* section, const char* key, const char* default_value = "") const
	{
		std::string value;
		if (!GetStringValue(section, key, &value))
			value.assign(default_value);
		return value;
	}
};
