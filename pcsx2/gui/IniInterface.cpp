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

#include <wx/gdicmn.h>

#include "gui/IniInterface.h"
#include "gui/StringHelpers.h"
#include "common/Assertions.h"
#include "common/Console.h"

const wxRect wxDefaultRect(wxDefaultCoord, wxDefaultCoord, wxDefaultCoord, wxDefaultCoord);

wxDirName g_fullBaseDirName = wxDirName(L"");
void SetFullBaseDir(wxDirName appRoot)
{
	g_fullBaseDirName = appRoot;
}

static int _calcEnumLength(const wxChar* const* enumArray)
{
	int cnt = 0;
	while (*enumArray != NULL)
	{
		enumArray++;
		cnt++;
	}

	return cnt;
}

ScopedIniGroup::ScopedIniGroup(IniInterface& mommy, const wxString& group)
	: m_mom(mommy)
{
	pxAssertDev(wxStringTokenize(group, L"/").Count() <= 1, "Cannot nest more than one group deep per instance of ScopedIniGroup.");
	m_mom.SetPath(group);
}

ScopedIniGroup::~ScopedIniGroup()
{
	m_mom.SetPath(L"..");
}

// --------------------------------------------------------------------------------------
//  IniInterface (implementations)
// --------------------------------------------------------------------------------------
IniInterface::IniInterface(wxConfigBase& config)
{
	m_Config = &config;
}

IniInterface::IniInterface(wxConfigBase* config)
{
	m_Config = config;
}

IniInterface::IniInterface()
{
	m_Config = wxConfigBase::Get(false);
}

IniInterface::~IniInterface()
{
	Flush();
}

void IniInterface::SetPath(const wxString& path)
{
	if (m_Config)
		m_Config->SetPath(path);
}

void IniInterface::Flush()
{
	if (m_Config)
		m_Config->Flush();
}


// --------------------------------------------------------------------------------------
//  IniLoader  (implementations)
// --------------------------------------------------------------------------------------
IniLoader::IniLoader(wxConfigBase& config)
	: IniInterface(config)
{
}

IniLoader::IniLoader(wxConfigBase* config)
	: IniInterface(config)
{
}

IniLoader::IniLoader()
	: IniInterface()
{
}

void IniLoader::Entry(const wxString& var, wxString& value, const wxString defvalue)
{
	if (m_Config)
		m_Config->Read(var, &value, defvalue);
	else
		value = defvalue;
}

void IniLoader::Entry(const wxString& var, wxDirName& value, const wxDirName defvalue, bool isAllowRelative)
{
	wxString dest;
	if (m_Config)
		m_Config->Read(var, &dest, wxEmptyString);

	if (dest.IsEmpty())
		value = defvalue;
	else
	{
		value = dest;
		if (isAllowRelative)
			value = g_fullBaseDirName + value;

		if (value.IsAbsolute())
			value.Normalize();
	}
}

void IniLoader::Entry(const wxString& var, wxFileName& value, const wxFileName defvalue, bool isAllowRelative)
{
	wxString dest(defvalue.GetFullPath());
	if (m_Config)
		m_Config->Read(var, &dest, defvalue.GetFullPath());
	value = dest;
	if (isAllowRelative)
		value = g_fullBaseDirName + value;

	if (value.IsAbsolute())
		value.Normalize();

	if (value.HasVolume())
		value.SetVolume(value.GetVolume().Upper());
}

void IniLoader::Entry(const wxString& var, int& value, const int defvalue)
{
	if (m_Config)
		m_Config->Read(var, &value, defvalue);
	else
		value = defvalue;
}

void IniLoader::Entry(const wxString& var, uint& value, const uint defvalue)
{
	if (m_Config)
		m_Config->Read(var, (int*)&value, (int)defvalue);
	else
		value = defvalue;
}

void IniLoader::Entry(const wxString& var, bool& value, const bool defvalue)
{
	// TODO : Stricter value checking on enabled/disabled?
	wxString dest;
	if (defvalue)
		dest = wxString("enabled");
	else
		dest = wxString("disabled");

	if (m_Config)
		m_Config->Read(var, &dest, dest);
	value = (dest == L"enabled") || (dest == L"1");
}

bool IniLoader::EntryBitBool(const wxString& var, bool value, const bool defvalue)
{
	// Note: 'value' param is used by inisaver only.
	bool result;
	Entry(var, result, defvalue);
	return result;
}

int IniLoader::EntryBitfield(const wxString& var, int value, const int defvalue)
{
	int result;
	Entry(var, result, defvalue);
	return result;
}

void IniLoader::Entry(const wxString& var, double& value, const double defvalue)
{
	auto readval = wxString::FromCDouble(value);

	if (m_Config)
		m_Config->Read(var, &readval);

	if (!readval.ToCDouble(&value))
		value = 0.0;
}

void IniLoader::Entry(const wxString& var, wxPoint& value, const wxPoint defvalue)
{
	if (!m_Config)
	{
		value = defvalue;
		return;
	}
	TryParse(value, m_Config->Read(var, ToString(defvalue)), defvalue);
}

void IniLoader::Entry(const wxString& var, wxSize& value, const wxSize defvalue)
{
	if (!m_Config)
	{
		value = defvalue;
		return;
	}
	TryParse(value, m_Config->Read(var, ToString(defvalue)), defvalue);
}

void IniLoader::Entry(const wxString& var, wxRect& value, const wxRect defvalue)
{
	if (!m_Config)
	{
		value = defvalue;
		return;
	}
	TryParse(value, m_Config->Read(var, ToString(defvalue)), defvalue);
}

void IniLoader::_EnumEntry(const wxString& var, int& value, const wxChar* const* enumArray, int defvalue)
{
	// Confirm default value sanity...

	const int cnt = _calcEnumLength(enumArray);
	if (defvalue > cnt)
	{
		Console.Error("(LoadSettings) Default enumeration index is out of bounds. Truncating.");
		defvalue = cnt - 1;
	}

	// Sanity confirmed, proceed with craziness!

	if (!m_Config)
	{
		value = defvalue;
		return;
	}

	wxString retval;
	m_Config->Read(var, &retval, enumArray[defvalue]);

	int i = 0;
	while (enumArray[i] != NULL && (retval != enumArray[i]))
		i++;

	if (enumArray[i] == NULL)
	{
		Console.Warning("(LoadSettings) Warning: Unrecognized value '%ls' on key '%ls'\n\tUsing the default setting of '%ls'.",
			WX_STR(retval), WX_STR(var), enumArray[defvalue]);
		value = defvalue;
	}
	else
		value = i;
}

void IniLoader::Entry(const wxString& var, std::string& value, const std::string& default_value)
{
	if (m_Config)
	{
		wxString temp;
		m_Config->Read(var, &temp, fromUTF8(default_value));
		value = temp.ToStdString();
	}
	else if (&value != &default_value)
	{
		value.assign(default_value);
	}
}

// --------------------------------------------------------------------------------------
//  IniSaver  (implementations)
// --------------------------------------------------------------------------------------

IniSaver::IniSaver(wxConfigBase& config)
	: IniInterface(config)
{
}

IniSaver::IniSaver(wxConfigBase* config)
	: IniInterface(config)
{
}

IniSaver::IniSaver()
	: IniInterface()
{
}

void IniSaver::Entry(const wxString& var, wxString& value, const wxString defvalue)
{
	if (!m_Config)
		return;
	m_Config->Write(var, value);
}

void IniSaver::Entry(const wxString& var, wxDirName& value, const wxDirName defvalue, bool isAllowRelative)
{
	if (!m_Config)
		return;
	wxDirName res(value);

	if (res.IsAbsolute())
		res.Normalize();

	if (isAllowRelative)
		res = wxDirName::MakeAutoRelativeTo(res, g_fullBaseDirName.ToString());


	/*if( value == defvalue )
		m_Config->Write( var, wxString() );
	else*/
	m_Config->Write(var, res.ToString());
}

void IniSaver::Entry(const wxString& var, wxFileName& value, const wxFileName defvalue, bool isAllowRelative)
{
	if (!m_Config)
		return;
	wxFileName res(value);

	if (res.IsAbsolute())
		res.Normalize();

	if (isAllowRelative)
		res = wxDirName::MakeAutoRelativeTo(res, g_fullBaseDirName.ToString());

	m_Config->Write(var, res.GetFullPath());
}

void IniSaver::Entry(const wxString& var, int& value, const int defvalue)
{
	if (!m_Config)
		return;
	m_Config->Write(var, value);
}

void IniSaver::Entry(const wxString& var, uint& value, const uint defvalue)
{
	if (!m_Config)
		return;
	m_Config->Write(var, (int)value);
}

void IniSaver::Entry(const wxString& var, bool& value, const bool defvalue)
{
	if (!m_Config)
		return;
	m_Config->Write(var, value ? L"enabled" : L"disabled");
}

bool IniSaver::EntryBitBool(const wxString& var, bool value, const bool defvalue)
{
	if (m_Config)
		m_Config->Write(var, value ? L"enabled" : L"disabled");
	return value;
}

int IniSaver::EntryBitfield(const wxString& var, int value, const int defvalue)
{
	if (m_Config)
		m_Config->Write(var, value);
	return value;
}

void IniSaver::Entry(const wxString& var, double& value, const double defvalue)
{
	if (!m_Config)
		return;

	m_Config->Write(var, wxString::FromCDouble(value));
}

void IniSaver::Entry(const wxString& var, wxPoint& value, const wxPoint defvalue)
{
	if (!m_Config)
		return;
	m_Config->Write(var, ToString(value));
}

void IniSaver::Entry(const wxString& var, wxSize& value, const wxSize defvalue)
{
	if (!m_Config)
		return;
	m_Config->Write(var, ToString(value));
}

void IniSaver::Entry(const wxString& var, wxRect& value, const wxRect defvalue)
{
	if (!m_Config)
		return;
	m_Config->Write(var, ToString(value));
}

void IniSaver::_EnumEntry(const wxString& var, int& value, const wxChar* const* enumArray, int defvalue)
{
	const int cnt = _calcEnumLength(enumArray);

	// Confirm default value sanity...

	if (defvalue > cnt)
	{
		Console.Error("(SaveSettings) Default enumeration index is out of bounds. Truncating.");
		defvalue = cnt - 1;
	}

	if (!m_Config)
		return;

	if (value >= cnt)
	{
		Console.Warning("(SaveSettings) An illegal enumerated index was detected when saving '%ls'", WX_STR(var));
		Console.Indent().Warning(
			"Illegal Value: %d\n"
			"Using Default: %d (%ls)\n",
			value, defvalue, enumArray[defvalue]);

		// Cause a debug assertion, since this is a fully recoverable error.
		pxAssert(value < cnt);

		value = defvalue;
	}

	m_Config->Write(var, enumArray[value]);
}

void IniSaver::Entry(const wxString& var, std::string& value, const std::string& default_value)
{
	if (!m_Config)
		return;
	m_Config->Write(var, fromUTF8(value));
}
