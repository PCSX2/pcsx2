/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2016  PCSX2 Dev Team
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

#include "Settings.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <vector>
#endif
#include <fstream>
#include <locale>
#include <string>
#include <PluginCompatibility.h>

Settings::Settings()
{
}

Settings::Settings(std::map<std::string, std::string> data)
    : m_data(data)
{
}

void Settings::TrimWhitespace(std::string &str) const
{
    // Leading whitespace
    str.erase(0, str.find_first_not_of(" \r\t"));
    // Trailing whitespace
    auto pos = str.find_last_not_of(" \r\t");
    if (pos != std::string::npos && pos != str.size() - 1)
        str.erase(pos + 1);
}

void Settings::Load(const std::string &filename)
{
#ifdef _WIN32
    std::ifstream file(convert_utf8_to_utf16(filename));
#else
    std::ifstream file(filename);
#endif
    if (!file.is_open())
        return;

    while (!file.eof()) {
        std::string line;
        std::getline(file, line);

        auto separator = line.find('=');
        if (separator == std::string::npos)
            continue;

        std::string key = line.substr(0, separator);
        // Trim leading and trailing whitespace
        TrimWhitespace(key);
        if (key.empty())
            continue;

        std::string value = line.substr(separator + 1);
        TrimWhitespace(value);

        Set(key, value);
    }
}

void Settings::Save(const std::string &filename) const
{
#ifdef _WIN32
    std::ofstream file(convert_utf8_to_utf16(filename), std::ios::trunc);
#else
    std::ofstream file(filename, std::ios::trunc);
#endif
    if (!file.is_open())
        return;

    for (const auto &pair : m_data)
        file << pair.first << '=' << pair.second << '\n';
}

void Settings::Set(std::string key, std::string value)
{
    m_data[key] = value;
}

bool Settings::Get(const std::string &key, std::string &data) const
{
    auto it = m_data.find(key);
    if (it == m_data.end())
        return false;

    data = it->second;
    return true;
}

#if defined(_WIN32)
void Settings::Set(std::string key, std::wstring value)
{
    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::vector<char> converted_string(size);
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, converted_string.data(), converted_string.size(), nullptr, nullptr);
    m_data[key] = converted_string.data();
}

bool Settings::Get(const std::string &key, std::wstring &data) const
{
    auto it = m_data.find(key);
    if (it == m_data.end())
        return false;

    int size = MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> converted_string(size);
    MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(), -1, converted_string.data(), converted_string.size());
    data = converted_string.data();
    return true;
}
#endif
