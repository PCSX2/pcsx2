/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include "common/Assertions.h"
#include "common/Path.h"
#include "wxDirName.h"
#include "StringHelpers.h"
#include <wx/config.h>
#include <wx/gdicmn.h>
#include <string>

// --------------------------------------------------------------------------------------
//  IniInterface (abstract base class)
// --------------------------------------------------------------------------------------
// This is used as an interchangable interface for both loading and saving options from an
// ini/configuration file.  The LoadSave code takes an IniInterface, and the interface
// implementation defines whether the options are read or written.
//
// See also: IniLoader, IniSaver
//
class IniInterface
{
protected:
	wxConfigBase* m_Config;

public:
	virtual ~IniInterface();
	explicit IniInterface();
	explicit IniInterface(wxConfigBase& config);
	explicit IniInterface(wxConfigBase* config);

	void SetPath(const wxString& path);
	void Flush();

	wxConfigBase& GetConfig()
	{
		pxAssert(m_Config);
		return *m_Config;
	}
	bool IsOk() const { return m_Config != NULL; }

	virtual bool IsLoading() const = 0;
	bool IsSaving() const { return !IsLoading(); }

	virtual void Entry(const wxString& var, int& value, const int defvalue = 0) = 0;
	virtual void Entry(const wxString& var, uint& value, const uint defvalue = 0) = 0;
	virtual void Entry(const wxString& var, bool& value, const bool defvalue = false) = 0;
	virtual void Entry(const wxString& var, double& value, const double defvalue = 0.0) = 0;
	virtual void Entry(const wxString& var, std::string& value, const std::string& default_value = std::string()) = 0;

	// This special form of Entry is provided for bitfields, which cannot be passed by reference.
	virtual bool EntryBitBool(const wxString& var, bool value, const bool defvalue = false) = 0;
	virtual int EntryBitfield(const wxString& var, int value, const int defvalue = 0) = 0;

	virtual void Entry(const wxString& var, wxString& value, const wxString defvalue = wxString()) = 0;
	virtual void Entry(const wxString& var, wxDirName& value, const wxDirName defvalue = wxDirName(), bool isAllowRelative = false) = 0;
	virtual void Entry(const wxString& var, wxFileName& value, const wxFileName defvalue = wxFileName(), bool isAllowRelative = false) = 0;

	virtual void Entry(const wxString& var, wxPoint& value, const wxPoint defvalue = wxDefaultPosition) = 0;
	virtual void Entry(const wxString& var, wxSize& value, const wxSize defvalue = wxDefaultSize) = 0;
	virtual void Entry(const wxString& var, wxRect& value, const wxRect defvalue = wxDefaultRect) = 0;

	template <typename T>
	void EnumEntry(const wxString& var, T& value, const wxChar* const* enumArray = NULL, const T defvalue = (T)0)
	{
		int tstore = (int)value;
		auto defaultvalue = enum_cast(defvalue);
		if (enumArray == NULL)
			Entry(var, tstore, defaultvalue);
		else
			_EnumEntry(var, tstore, enumArray, defaultvalue);
		value = (T)tstore;
	}

protected:
	virtual void _EnumEntry(const wxString& var, int& value, const wxChar* const* enumArray, int defvalue) = 0;
};

// --------------------------------------------------------------------------------------
//  ScopedIniGroup
// --------------------------------------------------------------------------------------
class ScopedIniGroup
{
protected:
	IniInterface& m_mom;

public:
	ScopedIniGroup(IniInterface& mommy, const wxString& group);
	virtual ~ScopedIniGroup();
};

// --------------------------------------------------------------------------------------
//  IniLoader
// --------------------------------------------------------------------------------------
// Implementation of the IniInterface base class, which maps ini actions to loading from
// an ini source file.
//
// See also: IniInterface
//
class IniLoader : public IniInterface
{
public:
	virtual ~IniLoader() = default;
	explicit IniLoader();
	explicit IniLoader(wxConfigBase& config);
	explicit IniLoader(wxConfigBase* config);

	bool IsLoading() const override { return true; }

	void Entry(const wxString& var, int& value, const int defvalue = 0) override;
	void Entry(const wxString& var, uint& value, const uint defvalue = 0) override;
	void Entry(const wxString& var, bool& value, const bool defvalue = false) override;
	void Entry(const wxString& var, double& value, const double defvalue = 0.0) override;
	void Entry(const wxString& var, std::string& value, const std::string& default_value = std::string()) override;

	bool EntryBitBool(const wxString& var, bool value, const bool defvalue = false) override;
	int EntryBitfield(const wxString& var, int value, const int defvalue = 0) override;

	void Entry(const wxString& var, wxString& value, const wxString defvalue = wxEmptyString) override;
	void Entry(const wxString& var, wxDirName& value, const wxDirName defvalue = wxDirName(), bool isAllowRelative = false) override;
	void Entry(const wxString& var, wxFileName& value, const wxFileName defvalue = wxFileName(), bool isAllowRelative = false) override;

	void Entry(const wxString& var, wxPoint& value, const wxPoint defvalue = wxDefaultPosition) override;
	void Entry(const wxString& var, wxSize& value, const wxSize defvalue = wxDefaultSize) override;
	void Entry(const wxString& var, wxRect& value, const wxRect defvalue = wxDefaultRect) override;


protected:
	void _EnumEntry(const wxString& var, int& value, const wxChar* const* enumArray, int defvalue) override;
};

// --------------------------------------------------------------------------------------
//  IniSaver
// --------------------------------------------------------------------------------------
// Implementation of the IniInterface base class, which maps ini actions to saving to
// an ini dest file.
//
// See also: IniInterface
//
class IniSaver : public IniInterface
{
public:
	virtual ~IniSaver() = default;
	explicit IniSaver();
	explicit IniSaver(wxConfigBase& config);
	explicit IniSaver(wxConfigBase* config);

	bool IsLoading() const override { return false; }

	void Entry(const wxString& var, int& value, const int defvalue = 0) override;
	void Entry(const wxString& var, uint& value, const uint defvalue = 0) override;
	void Entry(const wxString& var, bool& value, const bool defvalue = false) override;
	void Entry(const wxString& var, double& value, const double defvalue = 0.0) override;
	void Entry(const wxString& var, std::string& value, const std::string& default_value = std::string()) override;

	bool EntryBitBool(const wxString& var, bool value, const bool defvalue = false) override;
	int EntryBitfield(const wxString& var, int value, const int defvalue = 0) override;

	void Entry(const wxString& var, wxString& value, const wxString defvalue = wxString()) override;
	void Entry(const wxString& var, wxDirName& value, const wxDirName defvalue = wxDirName(), bool isAllowRelative = false) override;
	void Entry(const wxString& var, wxFileName& value, const wxFileName defvalue = wxFileName(), bool isAllowRelative = false) override;

	void Entry(const wxString& var, wxPoint& value, const wxPoint defvalue = wxDefaultPosition) override;
	void Entry(const wxString& var, wxSize& value, const wxSize defvalue = wxDefaultSize) override;
	void Entry(const wxString& var, wxRect& value, const wxRect defvalue = wxDefaultRect) override;


protected:
	void _EnumEntry(const wxString& var, int& value, const wxChar* const* enumArray, int defvalue) override;
};

// ------------------------------------------------------------------------
// GCC Note: wxT() macro is required when using string token pasting.  For some reason L generates
// syntax errors. >_<
//
#define IniEntry(varname) ini.Entry(wxT(#varname), varname, varname)
#define IniEntryDirFile(varname, isAllowRelative) ini.Entry(wxT(#varname), varname, varname, isAllowRelative)
#define IniBitfield(varname) varname = ini.EntryBitfield(wxT(#varname), varname, varname)
#define IniBitBool(varname) varname = ini.EntryBitBool(wxT(#varname), !!varname, varname)

#define IniBitfieldEx(varname, textname) varname = ini.EntryBitfield(wxT(textname), varname, varname)
#define IniBitBoolEx(varname, textname) varname = ini.EntryBitBool(wxT(textname), !!varname, varname)

//used for filenames and folder names as ini values.
//Set to app root folder, so all files and folders which are inside appRoot will be written as relative.
void SetFullBaseDir(wxDirName appRoot);
