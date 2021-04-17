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
#include "StringUtils.h"

#ifdef _WIN32

#include <Windows.h>

std::string StringUtils::UTF8::narrow(const wchar_t* str)
{
	return narrow(std::wstring(str));
}

std::string StringUtils::UTF8::narrow(const std::wstring& str)
{
	if (str.empty())
	{
		return std::string();
	}

	auto const size = WideCharToMultiByte(CP_UTF8, 0, str.data(), str.size(),
										  nullptr, 0, nullptr, nullptr);

	std::string output(size, '\0');

	if (size != WideCharToMultiByte(CP_UTF8, 0, str.data(), str.size(),
									output.data(), output.size(), nullptr, nullptr))
	{
		return std::string();
	}

	return output;
}

std::wstring StringUtils::UTF8::widen(const char* str)
{
	return widen(std::string(str));
}

std::wstring StringUtils::UTF8::widen(const std::string& str)
{
	auto const size =
		MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(), nullptr, 0);

	std::wstring output;
	output.resize(size);

	if (size == 0 ||
		size != MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(),
									&output[0], output.size()))
	{
		output.clear();
	}

	return output;
}

#endif

std::string StringUtils::UTF8::fromWxString(const wxString& str)
{
#ifdef _WIN32
	return narrow(str.ToStdWstring());
#else
	return str.ToStdString();
#endif
}
