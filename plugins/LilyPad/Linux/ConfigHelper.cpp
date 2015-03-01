/*  LilyPad - Pad plugin for PS2 Emulator
 *  Copyright (C) 2002-2014  PCSX2 Dev Team/ChickenLiver
 *
 *  File imported from SPU2-X (and tranformed to object)
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU Lesser General Public License as published by the Free
 *  Software Found- ation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with PCSX2.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Linux/ConfigHelper.h"
#include <wx/fileconf.h>

wxString CfgHelper::m_path = L"inis/LilyPad.ini";

void CfgHelper::SetSettingsDir(const char* dir)
{
	m_path = wxString::FromAscii(dir) + L"/LilyPad.ini";
}

CfgHelper::CfgHelper()
{
	m_config = new wxFileConfig(L"", L"", m_path, L"", wxCONFIG_USE_LOCAL_FILE);
}

CfgHelper::~CfgHelper()
{
	delete m_config;
}

void CfgHelper::setIni(const wchar_t* Section)
{
	m_config->SetPath(wxsFormat(L"/%s", Section));
}


void CfgHelper::WriteBool(const wchar_t* Section, const wchar_t* Name, bool Value)
{
	setIni(Section);
	m_config->Write(Name, Value);
}

void CfgHelper::WriteInt(const wchar_t* Section, const wchar_t* Name, int Value)
{
	setIni(Section);
	m_config->Write(Name, Value);
}

void CfgHelper::WriteFloat(const wchar_t* Section, const wchar_t* Name, float Value)
{
	setIni(Section);
	m_config->Write(Name, (double)Value);
}

void CfgHelper::WriteStr(const wchar_t* Section, const wchar_t* Name, const wxString& Data)
{
	setIni(Section);
	m_config->Write(Name, Data);
}

bool CfgHelper::ReadBool(const wchar_t *Section,const wchar_t* Name, bool Default)
{
	bool ret;

	setIni(Section);
	m_config->Read(Name, &ret, Default);

	return ret;
}

int CfgHelper::ReadInt(const wchar_t* Section, const wchar_t* Name,int Default)
{
	int ret;

	setIni(Section);
	m_config->Read(Name, &ret, Default);

	return ret;
}

float CfgHelper::ReadFloat(const wchar_t* Section, const wchar_t* Name, float Default)
{
	double ret;

	setIni(Section);
	m_config->Read(Name, &ret, (double)Default);

	return (float)ret;
}

int CfgHelper::ReadStr(const wchar_t* Section, const wchar_t* Name, wchar_t* Data, const wchar_t* Default)
{
	setIni(Section);
	wcscpy(Data, m_config->Read(Name, Default).wc_str());
	return wcslen(Data);
}

int CfgHelper::ReadStr(const wchar_t* Section, const wchar_t* Name, wxString& Data, const wchar_t* Default)
{
	setIni(Section);
	Data = m_config->Read(Name, Default);
	return Data.size();
}
