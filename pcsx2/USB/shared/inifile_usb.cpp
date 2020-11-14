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
#include "inifile_usb.h"
#include <algorithm>
#include <iostream>
#include <fstream>

// Only valid if C++
#ifndef __cplusplus
#error C++ compiler required.
#endif

// In the event you want to trace the calls can define _TRACE_CINIFILE
#ifdef _TRACE_CINIFILE
#define _CINIFILE_DEBUG
#endif

// _CRLF is used in the Save() function
// The class will read the correct data regardless of how the file linefeeds are defined <CRLF> or <CR>
// It is best to use the linefeed that is default to the system. This reduces issues if needing to modify
// the file with ie. notepad.exe which doesn't recognize unix linefeeds.
#ifdef _WIN32 // Windows default is \r\n
#ifdef _FORCE_UNIX_LINEFEED
#define _CRLFA "\n"
#define _CRLFW L"\n"
#else
#define _CRLFA "\r\n"
#define _CRLFW L"\r\n"
#endif

#else // Standard format is \n for unix
#ifdef _FORCE_WINDOWS_LINEFEED
#define _CRLFA "\r\n"
#define _CRLFW L"\r\n"
#else
#define _CRLFA "\n"
#define _CRLFW L"\n"
#endif
#endif

// Convert wstring to string
std::string wstr_to_str(const std::wstring& arg)
{
	std::string res(arg.length(), '\0');
	wcstombs(const_cast<char*>(res.data()), arg.c_str(), arg.length());
	return res;
}

// Convert string to wstring
std::wstring str_to_wstr(const std::string& arg)
{
	std::wstring res(arg.length(), L'\0');
	mbstowcs(const_cast<wchar_t*>(res.data()), arg.c_str(), arg.length());
	return res;
}

// Helper Functions
void RTrim(std::string& str, const std::string& chars = " \t")
{
	str.erase(str.find_last_not_of(chars) + 1);
}

void LTrim(std::string& str, const std::string& chars = " \t")
{
	str.erase(0, str.find_first_not_of(chars));
}

void Trim(std::string& str, const std::string& chars = " \t")
{
	str.erase(str.find_last_not_of(chars) + 1);
	str.erase(0, str.find_first_not_of(chars));
}

// Stream Helpers

std::ostream& operator<<(std::ostream& output, CIniFileA& obj)
{
	obj.Save(output);
	return output;
}

std::istream& operator>>(std::istream& input, CIniFileA& obj)
{
	obj.Load(input);
	return input;
}

std::istream& operator>>(std::istream& input, CIniMergeA merger)
{
	return merger(input);
}

// CIniFileA class methods

CIniFileA::CIniFileA()
{
}

CIniFileA::~CIniFileA()
{
	RemoveAllSections();
}

const char* const CIniFileA::LF = _CRLFA;

void CIniFileA::Save(std::ostream& output)
{
	std::string sSection;

	for (SecIndexA::iterator itr = m_sections.begin(); itr != m_sections.end(); ++itr)
	{
		sSection = "[" + (*itr)->GetSectionName() + "]";

		output << sSection << _CRLFA;

		for (KeyIndexA::iterator klitr = (*itr)->m_keys.begin(); klitr != (*itr)->m_keys.end(); ++klitr)
		{
			std::string sKey = (*klitr)->GetKeyName() + "=" + (*klitr)->GetValue();
			output << sKey << _CRLFA;
		}
	}
}

bool CIniFileA::Save(const std::string& fileName)
{

	std::ofstream output;

	output.open(fileName.c_str(), std::ios::binary);

	if (!output.is_open())
		return false;

	Save(output);

	output.close();
	return true;
}


void CIniFileA::Load(std::istream& input, bool bMerge)
{
	if (!bMerge)
		RemoveAllSections();

	CIniSectionA* pSection = NULL;
	std::string sRead;
	enum
	{
		KEY,
		SECTION,
		COMMENT,
		OTHER
	};

	while (std::getline(input, sRead))
	{

		// Trim all whitespace on the left
		LTrim(sRead);
		// Trim any returns
		RTrim(sRead, "\n\r");

		if (!sRead.empty())
		{
			auto nType = (sRead.find_first_of("[") == 0 && (sRead[sRead.find_last_not_of(" \t\r\n")] == ']')) ? SECTION : OTHER;
			nType = ((nType == OTHER) && (sRead.find_first_of("=") != std::string::npos && sRead.find_first_of("=") > 0)) ? KEY : nType;
			nType = ((nType == OTHER) && (sRead.find_first_of("#") == 0)) ? COMMENT : nType;

			switch (nType)
			{
				case SECTION:
					pSection = AddSection(sRead.substr(1, sRead.size() - 2));
					break;

				case KEY:
				{
					// Check to ensure valid section... or drop the keys listed
					if (pSection)
					{
						size_t iFind = sRead.find_first_of("=");
						std::string sKey = sRead.substr(0, iFind);
						std::string sValue = sRead.substr(iFind + 1);
						CIniKeyA* pKey = pSection->AddKey(sKey);
						if (pKey)
						{
							pKey->SetValue(sValue);
						}
					}
				}
				break;
				case COMMENT:
					break;
				case OTHER:
					break;
			}
		}
	}
}

bool CIniFileA::Load(const std::string& fileName, bool bMerge)
{
	std::ifstream input;

	input.open(fileName.c_str(), std::ios::binary);

	if (!input.is_open())
		return false;

	Load(input, bMerge);

	input.close();
	return true;
}

const SecIndexA& CIniFileA::GetSections() const
{
	return m_sections;
}


CIniSectionA* CIniFileA::GetSection(std::string sSection) const
{
	Trim(sSection);
	SecIndexA::const_iterator itr = _find_sec(sSection);
	if (itr != m_sections.end())
		return *itr;
	return NULL;
}

CIniSectionA* CIniFileA::AddSection(std::string sSection)
{

	Trim(sSection);
	SecIndexA::const_iterator itr = _find_sec(sSection);
	if (itr == m_sections.end())
	{
		// Note constuctor doesn't trim the string so it is trimmed above
		CIniSectionA* pSection = new CIniSectionA(this, sSection);
		m_sections.insert(pSection);
		return pSection;
	}
	else
		return *itr;
}


std::string CIniFileA::GetKeyValue(const std::string& sSection, const std::string& sKey) const
{
	std::string sValue;
	CIniSectionA* pSec = GetSection(sSection);
	if (pSec)
	{
		CIniKeyA* pKey = pSec->GetKey(sKey);
		if (pKey)
			sValue = pKey->GetValue();
	}
	return sValue;
}

void CIniFileA::SetKeyValue(const std::string& sSection, const std::string& sKey, const std::string& sValue)
{
	CIniSectionA* pSec = AddSection(sSection);
	if (pSec)
	{
		CIniKeyA* pKey = pSec->AddKey(sKey);
		if (pKey)
			pKey->SetValue(sValue);
	}
}


void CIniFileA::RemoveSection(std::string sSection)
{
	Trim(sSection);
	SecIndexA::iterator itr = _find_sec(sSection);
	if (itr != m_sections.end())
	{
		delete *itr;
		m_sections.erase(itr);
	}
}

void CIniFileA::RemoveSection(CIniSectionA* pSection)
{
	// No trim since internal object not from user
	SecIndexA::iterator itr = _find_sec(pSection->m_sSectionName);
	if (itr != m_sections.end())
	{
		delete *itr;
		m_sections.erase(itr);
	}
}

void CIniFileA::RemoveAllSections()
{
	for (SecIndexA::iterator itr = m_sections.begin(); itr != m_sections.end(); ++itr)
	{
		delete *itr;
	}
	m_sections.clear();
}


bool CIniFileA::RenameSection(const std::string& sSectionName, const std::string& sNewSectionName)
{
	// Note string trims are done in lower calls.
	bool bRval = false;
	CIniSectionA* pSec = GetSection(sSectionName);
	if (pSec)
	{
		bRval = pSec->SetSectionName(sNewSectionName);
	}
	return bRval;
}

bool CIniFileA::RenameKey(const std::string& sSectionName, const std::string& sKeyName, const std::string& sNewKeyName)
{
	// Note string trims are done in lower calls.
	bool bRval = false;
	CIniSectionA* pSec = GetSection(sSectionName);
	if (pSec != NULL)
	{
		CIniKeyA* pKey = pSec->GetKey(sKeyName);
		if (pKey != NULL)
			bRval = pKey->SetKeyName(sNewKeyName);
	}
	return bRval;
}

// Returns a constant iterator to a section by name, string is not trimmed
SecIndexA::const_iterator CIniFileA::_find_sec(const std::string& sSection) const
{
	CIniSectionA bogus(NULL, sSection);
	return m_sections.find(&bogus);
}

// Returns an iterator to a section by name, string is not trimmed
SecIndexA::iterator CIniFileA::_find_sec(const std::string& sSection)
{
	CIniSectionA bogus(NULL, sSection);
	return m_sections.find(&bogus);
}

// CIniFileA functions end here

// CIniSectionA functions start here

CIniSectionA::CIniSectionA(CIniFileA* pIniFile, const std::string& sSectionName)
	: m_pIniFile(pIniFile)
	, m_sSectionName(sSectionName)
{
}


CIniSectionA::~CIniSectionA()
{
	RemoveAllKeys();
}

CIniKeyA* CIniSectionA::GetKey(std::string sKeyName) const
{
	Trim(sKeyName);
	KeyIndexA::const_iterator itr = _find_key(sKeyName);
	if (itr != m_keys.end())
		return *itr;
	return NULL;
}

void CIniSectionA::RemoveAllKeys()
{
	for (KeyIndexA::iterator itr = m_keys.begin(); itr != m_keys.end(); ++itr)
	{
		delete *itr;
	}
	m_keys.clear();
}

void CIniSectionA::RemoveKey(std::string sKey)
{
	Trim(sKey);
	KeyIndexA::iterator itr = _find_key(sKey);
	if (itr != m_keys.end())
	{
		delete *itr;
		m_keys.erase(itr);
	}
}

void CIniSectionA::RemoveKey(CIniKeyA* pKey)
{
	// No trim is done to improve efficiency since CIniKeyA* should already be trimmed
	KeyIndexA::iterator itr = _find_key(pKey->m_sKeyName);
	if (itr != m_keys.end())
	{
		delete *itr;
		m_keys.erase(itr);
	}
}

CIniKeyA* CIniSectionA::AddKey(std::string sKeyName)
{
	Trim(sKeyName);
	KeyIndexA::const_iterator itr = _find_key(sKeyName);
	if (itr == m_keys.end())
	{
		// Note constuctor doesn't trim the string so it is trimmed above
		CIniKeyA* pKey = new CIniKeyA(this, sKeyName);
		m_keys.insert(pKey);
		return pKey;
	}
	else
		return *itr;
}

bool CIniSectionA::SetSectionName(std::string sSectionName)
{
	Trim(sSectionName);
	// Does this already exist.
	if (m_pIniFile->_find_sec(sSectionName) == m_pIniFile->m_sections.end())
	{

		// Find the current section if one exists and remove it since we are renaming
		SecIndexA::iterator itr = m_pIniFile->_find_sec(m_sSectionName);

		// Just to be safe make sure the old section exists
		if (itr != m_pIniFile->m_sections.end())
			m_pIniFile->m_sections.erase(itr);

		// Change name prior to ensure tree balance
		m_sSectionName = sSectionName;

		// Set the new map entry we know should not exist
		m_pIniFile->m_sections.insert(this);

		return true;
	}
	else
	{
		return false;
	}
}

std::string CIniSectionA::GetSectionName() const
{
	return m_sSectionName;
}

const KeyIndexA& CIniSectionA::GetKeys() const
{
	return m_keys;
}

std::string CIniSectionA::GetKeyValue(std::string sKey) const
{
	std::string sValue;
	CIniKeyA* pKey = GetKey(sKey);
	if (pKey)
	{
		sValue = pKey->GetValue();
	}
	return sValue;
}

void CIniSectionA::SetKeyValue(std::string sKey, const std::string& sValue)
{
	CIniKeyA* pKey = AddKey(sKey);
	if (pKey)
	{
		pKey->SetValue(sValue);
	}
}

// Returns a constant iterator to a key by name, string is not trimmed
KeyIndexA::const_iterator CIniSectionA::_find_key(const std::string& sKey) const
{
	CIniKeyA bogus(NULL, sKey);
	return m_keys.find(&bogus);
}

// Returns an iterator to a key by name, string is not trimmed
KeyIndexA::iterator CIniSectionA::_find_key(const std::string& sKey)
{
	CIniKeyA bogus(NULL, sKey);
	return m_keys.find(&bogus);
}

// CIniSectionA function end here

// CIniKeyA Functions Start Here

CIniKeyA::CIniKeyA(CIniSectionA* pSection, const std::string& sKeyName)
	: m_pSection(pSection)
	, m_sKeyName(sKeyName)
{
}


CIniKeyA::~CIniKeyA()
{
}

void CIniKeyA::SetValue(const std::string& sValue)
{
	m_sValue = sValue;
}

std::string CIniKeyA::GetValue() const
{
	return m_sValue;
}

bool CIniKeyA::SetKeyName(std::string sKeyName)
{
	Trim(sKeyName);

	// Check for key name conflict
	if (m_pSection->_find_key(sKeyName) == m_pSection->m_keys.end())
	{
		KeyIndexA::iterator itr = m_pSection->_find_key(m_sKeyName);

		// Find the old map entry and remove it
		if (itr != m_pSection->m_keys.end())
			m_pSection->m_keys.erase(itr);

		// Change name prior to ensure tree balance
		m_sKeyName = sKeyName;

		// Make the new map entry
		m_pSection->m_keys.insert(this);
		return true;
	}
	else
	{
		return false;
	}
}

std::string CIniKeyA::GetKeyName() const
{
	return m_sKeyName;
}

// End of CIniKeyA Functions


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//              WIDE FUNCTIONS HERE
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Helper Functions
void RTrim(std::wstring& str, const std::wstring& chars = L" \t")
{
	str.erase(str.find_last_not_of(chars) + 1);
}

void LTrim(std::wstring& str, const std::wstring& chars = L" \t")
{
	str.erase(0, str.find_first_not_of(chars));
}

void Trim(std::wstring& str, const std::wstring& chars = L" \t")
{
	str.erase(str.find_last_not_of(chars) + 1);
	str.erase(0, str.find_first_not_of(chars));
}

// Stream Helpers
std::wostream& operator<<(std::wostream& output, CIniFileW& obj)
{
	obj.Save(output);
	return output;
}

std::wistream& operator>>(std::wistream& input, CIniFileW& obj)
{
	obj.Load(input);
	return input;
}

std::wistream& operator>>(std::wistream& input, CIniMergeW merger)
{
	return merger(input);
}

CIniFileW::CIniFileW()
{
}

CIniFileW::~CIniFileW()
{
	RemoveAllSections();
}

const wchar_t* const CIniFileW::LF = _CRLFW;

void CIniFileW::Save(std::wostream& output)
{
	std::wstring sSection;

	for (SecIndexW::iterator itr = m_sections.begin(); itr != m_sections.end(); ++itr)
	{
		sSection = L"[" + (*itr)->GetSectionName() + L"]";

		output << sSection << _CRLFA;

		for (KeyIndexW::iterator klitr = (*itr)->m_keys.begin(); klitr != (*itr)->m_keys.end(); ++klitr)
		{
			std::wstring sKey = (*klitr)->GetKeyName() + L"=" + (*klitr)->GetValue();
			output << sKey << _CRLFA;
		}
	}
}

bool CIniFileW::Save(const std::wstring& fileName)
{

	std::wofstream output;

#if defined(_MSC_VER) && (_MSC_VER >= 1300)
	output.open(fileName.c_str(), std::ios::binary);
#else
	output.open(wstr_to_str(fileName).c_str(), std::ios::binary);
#endif

	if (!output.is_open())
		return false;

	Save(output);

	output.close();
	return true;
}


void CIniFileW::Load(std::wistream& input, bool bMerge)
{
	if (!bMerge)
		RemoveAllSections();

	CIniSectionW* pSection = NULL;
	std::wstring sRead;
	enum
	{
		KEY,
		SECTION,
		COMMENT,
		OTHER
	};

	while (std::getline(input, sRead))
	{

		// Trim all whitespace on the left
		LTrim(sRead);
		// Trim any returns
		RTrim(sRead, L"\n\r");

		if (!sRead.empty())
		{
			auto nType = (sRead.find_first_of(L"[") == 0 && (sRead[sRead.find_last_not_of(L" \t\r\n")] == L']')) ? SECTION : OTHER;
			nType = ((nType == OTHER) && (sRead.find_first_of(L"=") != std::wstring::npos && sRead.find_first_of(L"=") > 0)) ? KEY : nType;
			nType = ((nType == OTHER) && (sRead.find_first_of(L"#") == 0)) ? COMMENT : nType;

			switch (nType)
			{
				case SECTION:
					pSection = AddSection(sRead.substr(1, sRead.size() - 2));
					break;

				case KEY:
				{
					// Check to ensure valid section... or drop the keys listed
					if (pSection)
					{
						size_t iFind = sRead.find_first_of(L"=");
						std::wstring sKey = sRead.substr(0, iFind);
						std::wstring sValue = sRead.substr(iFind + 1);
						CIniKeyW* pKey = pSection->AddKey(sKey);
						if (pKey)
						{
							pKey->SetValue(sValue);
						}
					}
				}
				break;
				case COMMENT:
					break;
				case OTHER:
					break;
			}
		}
	}
}

bool CIniFileW::Load(const std::wstring& fileName, bool bMerge)
{

	std::wifstream input;

#if defined(_MSC_VER) && (_MSC_VER >= 1300)
	input.open(fileName.c_str(), std::ios::binary);
#else
	input.open(wstr_to_str(fileName).c_str(), std::ios::binary);
#endif

	if (!input.is_open())
		return false;

	Load(input, bMerge);

	input.close();
	return true;
}

const SecIndexW& CIniFileW::GetSections() const
{
	return m_sections;
}


CIniSectionW* CIniFileW::GetSection(std::wstring sSection) const
{
	Trim(sSection);
	SecIndexW::const_iterator itr = _find_sec(sSection);
	if (itr != m_sections.end())
		return *itr;
	return NULL;
}

CIniSectionW* CIniFileW::AddSection(std::wstring sSection)
{
	Trim(sSection);
	SecIndexW::const_iterator itr = _find_sec(sSection);
	if (itr == m_sections.end())
	{
		// Note constuctor doesn't trim the string so it is trimmed above
		CIniSectionW* pSection = new CIniSectionW(this, sSection);
		m_sections.insert(pSection);
		return pSection;
	}
	else
		return *itr;
}


std::wstring CIniFileW::GetKeyValue(const std::wstring& sSection, const std::wstring& sKey) const
{
	std::wstring sValue;
	CIniSectionW* pSec = GetSection(sSection);
	if (pSec)
	{
		CIniKeyW* pKey = pSec->GetKey(sKey);
		if (pKey)
			sValue = pKey->GetValue();
	}
	return sValue;
}

void CIniFileW::SetKeyValue(const std::wstring& sSection, const std::wstring& sKey, const std::wstring& sValue)
{
	CIniSectionW* pSec = AddSection(sSection);
	if (pSec)
	{
		CIniKeyW* pKey = pSec->AddKey(sKey);
		if (pKey)
			pKey->SetValue(sValue);
	}
}


void CIniFileW::RemoveSection(std::wstring sSection)
{
	Trim(sSection);
	SecIndexW::iterator itr = _find_sec(sSection);
	if (itr != m_sections.end())
	{
		delete *itr;
		m_sections.erase(itr);
	}
}

void CIniFileW::RemoveSection(CIniSectionW* pSection)
{
	// No trim since internal object not from user
	SecIndexW::iterator itr = _find_sec(pSection->m_sSectionName);
	if (itr != m_sections.end())
	{
		delete *itr;
		m_sections.erase(itr);
	}
}

void CIniFileW::RemoveAllSections()
{
	for (SecIndexW::iterator itr = m_sections.begin(); itr != m_sections.end(); ++itr)
	{
		delete *itr;
	}
	m_sections.clear();
}


bool CIniFileW::RenameSection(const std::wstring& sSectionName, const std::wstring& sNewSectionName)
{
	// Note string trims are done in lower calls.
	bool bRval = false;
	CIniSectionW* pSec = GetSection(sSectionName);
	if (pSec)
	{
		bRval = pSec->SetSectionName(sNewSectionName);
	}
	return bRval;
}

bool CIniFileW::RenameKey(const std::wstring& sSectionName, const std::wstring& sKeyName, const std::wstring& sNewKeyName)
{
	// Note string trims are done in lower calls.
	bool bRval = false;
	CIniSectionW* pSec = GetSection(sSectionName);
	if (pSec != NULL)
	{
		CIniKeyW* pKey = pSec->GetKey(sKeyName);
		if (pKey != NULL)
			bRval = pKey->SetKeyName(sNewKeyName);
	}
	return bRval;
}

// Returns a constant iterator to a section by name, string is not trimmed
SecIndexW::const_iterator CIniFileW::_find_sec(const std::wstring& sSection) const
{
	CIniSectionW bogus(NULL, sSection);
	return m_sections.find(&bogus);
}

// Returns an iterator to a section by name, string is not trimmed
SecIndexW::iterator CIniFileW::_find_sec(const std::wstring& sSection)
{
	CIniSectionW bogus(NULL, sSection);
	return m_sections.find(&bogus);
}

// CIniFileW functions end here

// CIniSectionW functions start here

CIniSectionW::CIniSectionW(CIniFileW* pIniFile, const std::wstring& sSectionName)
	: m_pIniFile(pIniFile)
	, m_sSectionName(sSectionName)
{
}


CIniSectionW::~CIniSectionW()
{
	RemoveAllKeys();
}

CIniKeyW* CIniSectionW::GetKey(std::wstring sKeyName) const
{
	Trim(sKeyName);
	KeyIndexW::const_iterator itr = _find_key(sKeyName);
	if (itr != m_keys.end())
		return *itr;
	return NULL;
}

void CIniSectionW::RemoveAllKeys()
{
	for (KeyIndexW::iterator itr = m_keys.begin(); itr != m_keys.end(); ++itr)
	{
		delete *itr;
	}
	m_keys.clear();
}

void CIniSectionW::RemoveKey(std::wstring sKey)
{
	Trim(sKey);
	KeyIndexW::iterator itr = _find_key(sKey);
	if (itr != m_keys.end())
	{
		delete *itr;
		m_keys.erase(itr);
	}
}

void CIniSectionW::RemoveKey(CIniKeyW* pKey)
{
	// No trim is done to improve efficiency since CIniKeyW* should already be trimmed
	KeyIndexW::iterator itr = _find_key(pKey->m_sKeyName);
	if (itr != m_keys.end())
	{
		delete *itr;
		m_keys.erase(itr);
	}
}

CIniKeyW* CIniSectionW::AddKey(std::wstring sKeyName)
{
	Trim(sKeyName);
	KeyIndexW::const_iterator itr = _find_key(sKeyName);
	if (itr == m_keys.end())
	{
		// Note constuctor doesn't trim the string so it is trimmed above
		CIniKeyW* pKey = new CIniKeyW(this, sKeyName);
		m_keys.insert(pKey);
		return pKey;
	}
	else
		return *itr;
}

bool CIniSectionW::SetSectionName(std::wstring sSectionName)
{
	Trim(sSectionName);
	// Does this already exist.
	if (m_pIniFile->_find_sec(sSectionName) == m_pIniFile->m_sections.end())
	{
		// Find the current section if one exists and remove it since we are renaming
		SecIndexW::iterator itr = m_pIniFile->_find_sec(m_sSectionName);

		// Just to be safe make sure the old section exists
		if (itr != m_pIniFile->m_sections.end())
			m_pIniFile->m_sections.erase(itr);

		// Change name prior to ensure tree balance
		m_sSectionName = sSectionName;

		// Set the new map entry we know should not exist
		m_pIniFile->m_sections.insert(this);

		return true;
	}
	else
	{
		return false;
	}
}

std::wstring CIniSectionW::GetSectionName() const
{
	return m_sSectionName;
}

const KeyIndexW& CIniSectionW::GetKeys() const
{
	return m_keys;
}

std::wstring CIniSectionW::GetKeyValue(std::wstring sKey) const
{
	std::wstring sValue;
	CIniKeyW* pKey = GetKey(sKey);
	if (pKey)
	{
		sValue = pKey->GetValue();
	}
	return sValue;
}

void CIniSectionW::SetKeyValue(std::wstring sKey, const std::wstring& sValue)
{
	CIniKeyW* pKey = AddKey(sKey);
	if (pKey)
	{
		pKey->SetValue(sValue);
	}
}

// Returns a constant iterator to a key by name, string is not trimmed
KeyIndexW::const_iterator CIniSectionW::_find_key(const std::wstring& sKey) const
{
	CIniKeyW bogus(NULL, sKey);
	return m_keys.find(&bogus);
}

// Returns an iterator to a key by name, string is not trimmed
KeyIndexW::iterator CIniSectionW::_find_key(const std::wstring& sKey)
{
	CIniKeyW bogus(NULL, sKey);
	return m_keys.find(&bogus);
}

// CIniSectionW function end here

// CIniKeyW Functions Start Here

CIniKeyW::CIniKeyW(CIniSectionW* pSection, const std::wstring& sKeyName)
	: m_pSection(pSection)
	, m_sKeyName(sKeyName)
{
}


CIniKeyW::~CIniKeyW()
{
}

void CIniKeyW::SetValue(const std::wstring& sValue)
{
	m_sValue = sValue;
}

std::wstring CIniKeyW::GetValue() const
{
	return m_sValue;
}

bool CIniKeyW::SetKeyName(std::wstring sKeyName)
{
	Trim(sKeyName);

	// Check for key name conflict
	if (m_pSection->_find_key(sKeyName) == m_pSection->m_keys.end())
	{
		KeyIndexW::iterator itr = m_pSection->_find_key(m_sKeyName);

		// Find the old map entry and remove it
		if (itr != m_pSection->m_keys.end())
			m_pSection->m_keys.erase(itr);

		// Change name prior to ensure tree balance
		m_sKeyName = sKeyName;

		// Make the new map entry
		m_pSection->m_keys.insert(this);
		return true;
	}
	else
	{
		return false;
	}
}

std::wstring CIniKeyW::GetKeyName() const
{
	return m_sKeyName;
}

// End of CIniKeyW Functions
