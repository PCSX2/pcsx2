/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#endif

#include <fcntl.h>
#include <unistd.h>

#ifdef __APPLE__

std::vector<std::string> GetDriveListFromClasses(CFMutableDictionaryRef classes)
{
	io_iterator_t iterator = IO_OBJECT_NULL;
	kern_return_t result;
	std::vector<std::string> drives;

	CFDictionarySetValue(classes, CFSTR(kIOMediaEjectableKey), kCFBooleanTrue);
	result = IOServiceGetMatchingServices(kIOMasterPortDefault, classes, &iterator);
	if (result != KERN_SUCCESS)
		return drives;
	while (io_object_t media = IOIteratorNext(iterator))
	{
		CFTypeRef path_cfstr = IORegistryEntryCreateCFProperty(media, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
		if (path_cfstr)
		{
			char path[PATH_MAX] = {0};
			strlcpy(path, "/dev/r", PATH_MAX);
			size_t path_prefix_len = strnlen(path, PATH_MAX);
			result = CFStringGetCString((CFStringRef)path_cfstr, path + path_prefix_len, PATH_MAX - path_prefix_len, kCFStringEncodingUTF8);
			if (result)
			{
				drives.emplace_back(path);
			}
			CFRelease(path_cfstr);
		}
		IOObjectRelease(media);
	}
	IOObjectRelease(iterator);
	return drives;
}

#endif

std::vector<std::string> GetOpticalDriveList()
{
#ifdef __APPLE__
	std::vector<std::string> drives;

	if (CFMutableDictionaryRef cd_classes = IOServiceMatching(kIOCDMediaClass))
	{
		std::vector<std::string> cd = GetDriveListFromClasses(cd_classes);
		drives.insert(drives.end(), cd.begin(), cd.end());
	}

	if (CFMutableDictionaryRef dvd_classes = IOServiceMatching(kIODVDMediaClass))
	{
		std::vector<std::string> dvd = GetDriveListFromClasses(dvd_classes);
		drives.insert(drives.end(), dvd.begin(), dvd.end());
	}
	return drives;
#else
	return {};
#endif
}

void GetValidDrive(std::string& drive)
{
	if (!drive.empty())
	{
#ifdef __APPLE__
		int fd = open(drive.c_str(), O_RDONLY | O_NONBLOCK);
		if (fd != -1)
		{
			close(fd);
		}
		else
		{
			drive.clear();
		}
#else
		drive.clear();
#endif
	}
	if (drive.empty())
	{
		auto drives = GetOpticalDriveList();
		if (!drives.empty())
			drive = drives.front();
	}
	if (!drive.empty())
		DevCon.WriteLn("CDVD: Opening drive '%s'...", drive.c_str());
}
