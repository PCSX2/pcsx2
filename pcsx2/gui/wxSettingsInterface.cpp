/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2021  PCSX2 Dev Team
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

#include "StringHelpers.h"
#include "wxSettingsInterface.h"
#include "common/Assertions.h"
#include "common/StringUtil.h"

wxSettingsInterface::wxSettingsInterface(wxConfigBase* config)
	: m_config(config)
{
	m_config->SetPath(wxEmptyString);
}

wxSettingsInterface::~wxSettingsInterface()
{
	m_config->SetPath(wxEmptyString);
}

void wxSettingsInterface::CheckPath(const char* section) const
{
	if (m_current_path.compare(section) == 0)
		return;

	m_current_path = section;
	m_config->SetPath(wxString::Format("/%s", section));
}

bool wxSettingsInterface::Save()
{
	pxFailRel("Not implemented");
	return false;
}

void wxSettingsInterface::Clear()
{
	pxFailRel("Not implemented");
}

bool wxSettingsInterface::GetIntValue(const char* section, const char* key, int* value) const
{
	CheckPath(section);
	return m_config->Read(key, value);
}

bool wxSettingsInterface::GetUIntValue(const char* section, const char* key, uint* value) const
{
	CheckPath(section);

	long lvalue;
	if (!m_config->Read(key, &lvalue))
		return false;

	*value = static_cast<uint>(lvalue);
	return true;
}

bool wxSettingsInterface::GetFloatValue(const char* section, const char* key, float* value) const
{
	CheckPath(section);
	return m_config->Read(key, value);
}

bool wxSettingsInterface::GetDoubleValue(const char* section, const char* key, double* value) const
{
	CheckPath(section);
	return m_config->Read(key, value);
}

bool wxSettingsInterface::GetBoolValue(const char* section, const char* key, bool* value) const
{
	CheckPath(section);
	wxString wxKey(key);
	if (!m_config->HasEntry(wxKey))
		return false;

	wxString ret = m_config->Read(wxKey);
	*value = (ret == wxT("enabled") || ret == wxT("1"));
	return true;
}

bool wxSettingsInterface::GetStringValue(const char* section, const char* key, std::string* value) const
{
	CheckPath(section);
	wxString wxKey(key);
	if (!m_config->HasEntry(wxKey))
		return false;

	wxString ret = m_config->Read(wxKey);
	*value = StringUtil::wxStringToUTF8String(ret);
	return true;
}

void wxSettingsInterface::SetIntValue(const char* section, const char* key, int value)
{
	CheckPath(section);
	m_config->Write(key, value);
}

void wxSettingsInterface::SetUIntValue(const char* section, const char* key, uint value)
{
	CheckPath(section);
	m_config->Write(key, static_cast<unsigned int>(value));
}

void wxSettingsInterface::SetFloatValue(const char* section, const char* key, float value)
{
	CheckPath(section);
	m_config->Write(key, value);
}

void wxSettingsInterface::SetDoubleValue(const char* section, const char* key, double value)
{
	CheckPath(section);
	m_config->Write(key, value);
}

void wxSettingsInterface::SetBoolValue(const char* section, const char* key, bool value)
{
	CheckPath(section);
	m_config->Write(key, value ? wxT("enabled") : wxT("disabled"));
}

void wxSettingsInterface::SetStringValue(const char* section, const char* key, const char* value)
{
	CheckPath(section);
	m_config->Write(key, wxString::FromUTF8(value));
}

std::vector<std::string> wxSettingsInterface::GetStringList(const char* section, const char* key)
{
	pxFailRel("Not implemented");
	return {};
}

void wxSettingsInterface::SetStringList(const char* section, const char* key, const std::vector<std::string>& items)
{
	pxFailRel("Not implemented");
}

bool wxSettingsInterface::RemoveFromStringList(const char* section, const char* key, const char* item)
{
	pxFailRel("Not implemented");
	return false;
}

bool wxSettingsInterface::AddToStringList(const char* section, const char* key, const char* item)
{
	return false;
}

void wxSettingsInterface::DeleteValue(const char* section, const char* key)
{
	CheckPath(section);
	m_config->DeleteEntry(key);
}

void wxSettingsInterface::ClearSection(const char* section)
{
	pxFailRel("Not implemented");
}
