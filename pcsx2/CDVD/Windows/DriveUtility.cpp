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
#include "../CDVDdiscReader.h"

std::vector<std::wstring> GetOpticalDriveList()
{
	DWORD size = GetLogicalDriveStrings(0, nullptr);
	std::vector<wchar_t> drive_strings(size);
	if (GetLogicalDriveStrings(size, drive_strings.data()) != size - 1)
		return {};

	std::vector<std::wstring> drives;
	for (auto p = drive_strings.data(); *p; ++p)
	{
		if (GetDriveType(p) == DRIVE_CDROM)
			drives.push_back(p);
		while (*p)
			++p;
	}
	return drives;
}

void GetValidDrive(std::wstring& drive)
{
	if (drive.empty() || GetDriveType(drive.c_str()) != DRIVE_CDROM)
	{
		auto drives = GetOpticalDriveList();
		if (drives.empty())
		{
			drive = {};
		}
		else
		{
			drive = drives.front();
		}
	}
	else
	{
		int size = WideCharToMultiByte(CP_UTF8, 0, drive.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::vector<char> converted_string(size);
		WideCharToMultiByte(CP_UTF8, 0, drive.c_str(), -1, converted_string.data(), converted_string.size(), nullptr, nullptr);
		printf(" * CDVD: Opening drive '%s'...\n", converted_string.data());

		// The drive string has the form "X:\", but to open the drive, the string
		// has to be in the form "\\.\X:"
		drive.pop_back();
		drive.insert(0, L"\\\\.\\");
	}
}