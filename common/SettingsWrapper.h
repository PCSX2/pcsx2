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

#include "SettingsInterface.h"

// Helper class which loads or saves depending on the derived class.
class SettingsWrapper
{
public:
	SettingsWrapper(SettingsInterface& si);

	virtual bool IsLoading() const = 0;
	virtual bool IsSaving() const = 0;

	virtual void Entry(const char* section, const char* var, int& value, const int defvalue = 0) = 0;
	virtual void Entry(const char* section, const char* var, uint& value, const uint defvalue = 0) = 0;
	virtual void Entry(const char* section, const char* var, bool& value, const bool defvalue = false) = 0;
	virtual void Entry(const char* section, const char* var, double& value, const double defvalue = 0.0) = 0;
	virtual void Entry(const char* section, const char* var, std::string& value, const std::string& default_value = std::string()) = 0;

	// This special form of Entry is provided for bitfields, which cannot be passed by reference.
	virtual bool EntryBitBool(const char* section, const char* var, bool value, const bool defvalue = false) = 0;
	virtual int EntryBitfield(const char* section, const char* var, int value, const int defvalue = 0) = 0;

	template <typename T>
	void EnumEntry(const char* section, const char* var, T& value, const char* const* enumArray = nullptr, const T defvalue = (T)0)
	{
		int tstore = (int)value;
		auto defaultvalue = enum_cast(defvalue);
		if (enumArray == NULL)
			Entry(section, var, tstore, defaultvalue);
		else
			_EnumEntry(section, var, tstore, enumArray, defaultvalue);
		value = (T)tstore;
	}

protected:
	virtual void _EnumEntry(const char* section, const char* var, int& value, const char* const* enumArray, int defvalue) = 0;

	SettingsInterface& m_si;
};

class SettingsLoadWrapper final : public SettingsWrapper
{
public:
	SettingsLoadWrapper(SettingsInterface& si);

	bool IsLoading() const override;
	bool IsSaving() const override;

	void Entry(const char* section, const char* var, int& value, const int defvalue = 0) override;
	void Entry(const char* section, const char* var, uint& value, const uint defvalue = 0) override;
	void Entry(const char* section, const char* var, bool& value, const bool defvalue = false) override;
	void Entry(const char* section, const char* var, double& value, const double defvalue = 0.0) override;

	void Entry(const char* section, const char* var, std::string& value, const std::string& default_value = std::string()) override;
	bool EntryBitBool(const char* section, const char* var, bool value, const bool defvalue = false) override;
	int EntryBitfield(const char* section, const char* var, int value, const int defvalue = 0) override;

protected:
	void _EnumEntry(const char* section, const char* var, int& value, const char* const* enumArray, int defvalue) override;
};

class SettingsSaveWrapper final : public SettingsWrapper
{
public:
	SettingsSaveWrapper(SettingsInterface& si);

	bool IsLoading() const override;
	bool IsSaving() const override;

	void Entry(const char* section, const char* var, int& value, const int defvalue = 0) override;
	void Entry(const char* section, const char* var, uint& value, const uint defvalue = 0) override;
	void Entry(const char* section, const char* var, bool& value, const bool defvalue = false) override;
	void Entry(const char* section, const char* var, double& value, const double defvalue = 0.0) override;

	void Entry(const char* section, const char* var, std::string& value, const std::string& default_value = std::string()) override;
	bool EntryBitBool(const char* section, const char* var, bool value, const bool defvalue = false) override;
	int EntryBitfield(const char* section, const char* var, int value, const int defvalue = 0) override;

protected:
	void _EnumEntry(const char* section, const char* var, int& value, const char* const* enumArray, int defvalue) override;
};

#define SettingsWrapSection(section) const char* CURRENT_SETTINGS_SECTION = section;
#define SettingsWrapEntry(var) wrap.Entry(CURRENT_SETTINGS_SECTION, #var, var, var)
#define SettingsWrapEntryEx(var, name) wrap.Entry(CURRENT_SETTINGS_SECTION, name, var, var)
#define SettingsWrapBitfield(varname) varname = wrap.EntryBitfield(CURRENT_SETTINGS_SECTION, #varname, varname, varname)
#define SettingsWrapBitBool(varname) varname = wrap.EntryBitBool(CURRENT_SETTINGS_SECTION, #varname, !!varname, varname)
#define SettingsWrapBitfieldEx(varname, textname) varname = wrap.EntryBitfield(CURRENT_SETTINGS_SECTION, textname, varname, varname)
#define SettingsWrapBitBoolEx(varname, textname) varname = wrap.EntryBitBool(CURRENT_SETTINGS_SECTION, textname, !!varname, varname)
#define SettingsWrapEnumEx(varname, textname, names) wrap.EnumEntry(CURRENT_SETTINGS_SECTION, textname, varname, names, varname)
#define SettingsWrapIntEnumEx(varname, textname) varname = static_cast<decltype(varname)>(wrap.EntryBitfield(CURRENT_SETTINGS_SECTION, textname, static_cast<int>(varname), static_cast<int>(varname)))

