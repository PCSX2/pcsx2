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
#include "CDVD/CDVDdiscReader.h"

std::vector<std::string> GetOpticalDriveList()
{
	DWORD size = GetLogicalDriveStringsA(0, nullptr);
	std::vector<char> drive_strings(size);
	if (GetLogicalDriveStringsA(size, drive_strings.data()) != size - 1)
		return {};

	std::vector<std::string> drives;
	for (auto p = drive_strings.data(); *p; ++p)
	{
		if (GetDriveTypeA(p) == DRIVE_CDROM)
			drives.push_back(p);
		while (*p)
			++p;
	}
	return drives;
}

void GetValidDrive(std::string& drive)
{
	if (drive.empty() || GetDriveTypeA(drive.c_str()) != DRIVE_CDROM)
	{
		auto drives = GetOpticalDriveList();
		if (drives.empty())
		{
			drive.clear();
			return;
		}
		drive = drives.front();
	}

	printf(" * CDVD: Opening drive '%s'...\n", drive.data());

	// The drive string has the form "X:\", but to open the drive, the string
	// has to be in the form "\\.\X:"
	drive.pop_back();
	drive.insert(0, "\\\\.\\");
}
