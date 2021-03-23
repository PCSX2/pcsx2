/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2018 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE. See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>
#include <vector>
#include <cstdio>

#ifdef _WIN32
inline std::string convert_utf16_to_utf8(const std::wstring& utf16_string)
{
    const int size = WideCharToMultiByte(CP_UTF8, 0, utf16_string.c_str(), utf16_string.size(), nullptr, 0, nullptr, nullptr);
    std::string converted_string(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16_string.c_str(), utf16_string.size(), converted_string.data(), converted_string.size(), nullptr, nullptr);
    return converted_string;
}

inline std::wstring convert_utf8_to_utf16(const std::string &utf8_string)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8_string.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> converted_string(size);
    MultiByteToWideChar(CP_UTF8, 0, utf8_string.c_str(), -1, converted_string.data(), converted_string.size());
    return {converted_string.data()};
}
#endif

// _wfopen has to be used on Windows for pathnames containing non-ASCII characters.
inline FILE *px_fopen(const std::string &filename, const std::string &mode)
{
#ifdef _WIN32
    return _wfopen(convert_utf8_to_utf16(filename).c_str(), convert_utf8_to_utf16(mode).c_str());
#else
    return fopen(filename.c_str(), mode.c_str());
#endif
}
