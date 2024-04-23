// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <algorithm>

#include "SettingsWrapper.h"
#include "Console.h"

static int _calcEnumLength(const char* const* enumArray)
{
	int cnt = 0;
	while (*enumArray != nullptr)
	{
		enumArray++;
		cnt++;
	}

	return cnt;
}

SettingsWrapper::SettingsWrapper(SettingsInterface& si)
	: m_si(si)
{
}

SettingsLoadWrapper::SettingsLoadWrapper(SettingsInterface& si)
	: SettingsWrapper(si)
{
}

bool SettingsLoadWrapper::IsLoading() const
{
	return true;
}

bool SettingsLoadWrapper::IsSaving() const
{
	return false;
}

void SettingsLoadWrapper::Entry(const char* section, const char* var, int& value, const int defvalue /*= 0*/)
{
	value = m_si.GetIntValue(section, var, defvalue);
}

void SettingsLoadWrapper::Entry(const char* section, const char* var, uint& value, const uint defvalue /*= 0*/)
{
	value = m_si.GetUIntValue(section, var, defvalue);
}

void SettingsLoadWrapper::Entry(const char* section, const char* var, bool& value, const bool defvalue /*= false*/)
{
	value = m_si.GetBoolValue(section, var, defvalue);
}

void SettingsLoadWrapper::Entry(const char* section, const char* var, float& value, const float defvalue /*= 0.0*/)
{
	value = m_si.GetFloatValue(section, var, defvalue);
}

void SettingsLoadWrapper::Entry(const char* section, const char* var, std::string& value, const std::string& default_value /*= std::string()*/)
{
	if (!m_si.GetStringValue(section, var, &value) && &value != &default_value)
		value = default_value;
}

void SettingsLoadWrapper::Entry(const char* section, const char* var, SmallStringBase& value, std::string_view default_value /* = std::string_view() */)
{
	if (!m_si.GetStringValue(section, var, &value) && value.data() != default_value.data())
		value = default_value;
}

void SettingsLoadWrapper::_EnumEntry(const char* section, const char* var, int& value, const char* const* enumArray, int defvalue)
{
	const int cnt = _calcEnumLength(enumArray);
	defvalue = std::clamp(defvalue, 0, cnt);

	const std::string retval(m_si.GetStringValue(section, var, enumArray[defvalue]));

	int i = 0;
	while (enumArray[i] != nullptr && (retval != enumArray[i]))
		i++;

	if (enumArray[i] == nullptr)
	{
		Console.Warning("(LoadSettings) Warning: Unrecognized value '%s' on key '%s'\n\tUsing the default setting of '%s'.",
			retval.c_str(), var, enumArray[defvalue]);
		value = defvalue;
	}
	else
	{
		value = i;
	}
}

bool SettingsLoadWrapper::EntryBitBool(const char* section, const char* var, bool value, const bool defvalue /*= false*/)
{
	return m_si.GetBoolValue(section, var, defvalue);
}

int SettingsLoadWrapper::EntryBitfield(const char* section, const char* var, int value, const int defvalue /*= 0*/)
{
	return m_si.GetIntValue(section, var, defvalue);
}

SettingsSaveWrapper::SettingsSaveWrapper(SettingsInterface& si)
	: SettingsWrapper(si)
{
}

bool SettingsSaveWrapper::IsLoading() const
{
	return false;
}

bool SettingsSaveWrapper::IsSaving() const
{
	return true;
}

void SettingsSaveWrapper::Entry(const char* section, const char* var, int& value, const int defvalue /*= 0*/)
{
	m_si.SetIntValue(section, var, value);
}

void SettingsSaveWrapper::Entry(const char* section, const char* var, uint& value, const uint defvalue /*= 0*/)
{
	m_si.SetUIntValue(section, var, value);
}

void SettingsSaveWrapper::Entry(const char* section, const char* var, bool& value, const bool defvalue /*= false*/)
{
	m_si.SetBoolValue(section, var, value);
}

void SettingsSaveWrapper::Entry(const char* section, const char* var, float& value, const float defvalue /*= 0.0*/)
{
	m_si.SetFloatValue(section, var, value);
}

void SettingsSaveWrapper::Entry(const char* section, const char* var, std::string& value, const std::string& default_value /*= std::string()*/)
{
	m_si.SetStringValue(section, var, value.c_str());
}

void SettingsSaveWrapper::Entry(const char* section, const char* var, SmallStringBase& value, std::string_view default_value /* = std::string_view() */)
{
	m_si.SetStringValue(section, var, value.c_str());
}

bool SettingsSaveWrapper::EntryBitBool(const char* section, const char* var, bool value, const bool defvalue /*= false*/)
{
	m_si.SetBoolValue(section, var, value);
	return value;
}

int SettingsSaveWrapper::EntryBitfield(const char* section, const char* var, int value, const int defvalue /*= 0*/)
{
	m_si.SetIntValue(section, var, value);
	return value;
}

void SettingsSaveWrapper::_EnumEntry(const char* section, const char* var, int& value, const char* const* enumArray, int defvalue)
{
	const int cnt = _calcEnumLength(enumArray);
	const int index = (value < 0 || value >= cnt) ? defvalue : value;
	m_si.SetStringValue(section, var, enumArray[index]);
}

SettingsClearWrapper::SettingsClearWrapper(SettingsInterface& si)
	: SettingsWrapper(si)
{
}

bool SettingsClearWrapper::IsLoading() const
{
	return false;
}

bool SettingsClearWrapper::IsSaving() const
{
	return true;
}

void SettingsClearWrapper::Entry(const char* section, const char* var, int& value, const int defvalue /*= 0*/)
{
	m_si.DeleteValue(section, var);
}

void SettingsClearWrapper::Entry(const char* section, const char* var, uint& value, const uint defvalue /*= 0*/)
{
	m_si.DeleteValue(section, var);
}

void SettingsClearWrapper::Entry(const char* section, const char* var, bool& value, const bool defvalue /*= false*/)
{
	m_si.DeleteValue(section, var);
}

void SettingsClearWrapper::Entry(const char* section, const char* var, float& value, const float defvalue /*= 0.0*/)
{
	m_si.DeleteValue(section, var);
}

void SettingsClearWrapper::Entry(const char* section, const char* var, std::string& value, const std::string& default_value /*= std::string()*/)
{
	m_si.DeleteValue(section, var);
}

void SettingsClearWrapper::Entry(const char* section, const char* var, SmallStringBase& value, std::string_view default_value /* = std::string_view() */)
{
	m_si.DeleteValue(section, var);
}

bool SettingsClearWrapper::EntryBitBool(const char* section, const char* var, bool value, const bool defvalue /*= false*/)
{
	m_si.DeleteValue(section, var);
	return defvalue;
}

int SettingsClearWrapper::EntryBitfield(const char* section, const char* var, int value, const int defvalue /*= 0*/)
{
	m_si.DeleteValue(section, var);
	return defvalue;
}

void SettingsClearWrapper::_EnumEntry(const char* section, const char* var, int& value, const char* const* enumArray, int defvalue)
{
	m_si.DeleteValue(section, var);
}
