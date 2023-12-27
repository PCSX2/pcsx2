// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
