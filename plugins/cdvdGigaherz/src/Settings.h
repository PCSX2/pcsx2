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

#pragma once

#include <map>
#include <string>

class Settings
{
private:
    std::map<std::string, std::string> m_data;

    void TrimWhitespace(std::string &str) const;

public:
    Settings();
    Settings(std::map<std::string, std::string> data);

    void Load(const std::string &filename);
    void Save(const std::string &filename) const;

    void Set(std::string key, std::string value);
    bool Get(const std::string &key, std::string &data) const;
#if defined(_WIN32)
    void Set(std::string key, std::wstring value);
    bool Get(const std::string &key, std::wstring &data) const;
#endif
};
