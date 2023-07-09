/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include "IsoFS.h"
#include "IsoFile.h"

#include "common/Assertions.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include <memory>

//////////////////////////////////////////////////////////////////////////
// IsoDirectory
//////////////////////////////////////////////////////////////////////////

//u8		filesystemType;	// 0x01 = ISO9660, 0x02 = Joliet, 0xFF = NULL
//u8		volID[5];		// "CD001"


std::string IsoDirectory::FStype_ToString() const
{
	switch (m_fstype)
	{
		case FStype_ISO9660:
			return "ISO9660";
		case FStype_Joliet:
			return "Joliet";
	}

	return StringUtil::StdStringFromFormat("Unrecognized Code (0x%x)", m_fstype);
}

// Used to load the Root directory from an image
IsoDirectory::IsoDirectory(SectorSource& r)
	: internalReader(r)
{
}

IsoDirectory::~IsoDirectory() = default;

bool IsoDirectory::OpenRootDirectory(Error* error /* = nullptr */)
{
	IsoFileDescriptor rootDirEntry;
	bool isValid = false;
	bool done = false;
	uint i = 16;

	m_fstype = FStype_ISO9660;

	while (!done)
	{
		u8 sector[2048];
		// If this fails, we're not reading an iso, or it's bad.
		if (!internalReader.readSector(sector, i))
			break;

		if (memcmp(&sector[1], "CD001", 5) == 0)
		{
			switch (sector[0])
			{
				case 0:
					DevCon.WriteLn(Color_Green, "(IsoFS) Block 0x%x: Boot partition info.", i);
					break;

				case 1:
					DevCon.WriteLn("(IsoFS) Block 0x%x: Primary partition info.", i);
					rootDirEntry.Load(sector + 156, 38);
					isValid = true;
					break;

				case 2:
					// Probably means Joliet (long filenames support), which PCSX2 doesn't care about.
					DevCon.WriteLn(Color_Green, "(IsoFS) Block 0x%x: Extended partition info.", i);
					m_fstype = FStype_Joliet;
					break;

				case 0xff:
					// Null terminator.  End of partition information.
					done = true;
					break;

				default:
					Console.Error("(IsoFS) Unknown partition type ID=%d, encountered at block 0x%x", sector[0], i);
					break;
			}
		}
		else
		{
			sector[9] = 0;
			Console.Error("(IsoFS) Invalid partition descriptor encountered at block 0x%x: '%s'", i, &sector[1]);
			break; // if no valid root partition was found, an exception will be thrown below.
		}

		++i;
	}

	if (!isValid)
	{
		Error::SetString(error, "IsoFS could not find the root directory on the ISO image.");
		return false;
	}

	DevCon.WriteLn("(IsoFS) Filesystem is %s", FStype_ToString().c_str());
	return Open(rootDirEntry);
}

// Used to load a specific directory from a file descriptor
bool IsoDirectory::Open(const IsoFileDescriptor& directoryEntry, Error* error /* = nullptr */)
{
	// parse directory sector
	IsoFile dataStream(internalReader, directoryEntry);

	files.clear();

	u32 remainingSize = directoryEntry.size;

	u8 b[257];

	while (remainingSize >= 4) // hm hack :P
	{
		b[0] = dataStream.read<u8>();

		if (b[0] == 0)
		{
			break; // or continue?
		}

		remainingSize -= b[0];

		if (dataStream.read(b + 1, static_cast<s32>(b[0] - 1)) != static_cast<s32>(b[0] - 1))
			break;

		files.emplace_back(b, b[0]);
	}

	return true;
}

const IsoFileDescriptor& IsoDirectory::GetEntry(size_t index) const
{
	return files[index];
}

int IsoDirectory::GetIndexOf(const std::string_view& fileName) const
{
	for (unsigned int i = 0; i < files.size(); i++)
	{
		if (files[i].name == fileName)
			return i;
	}

	return -1;
}

std::optional<IsoFileDescriptor> IsoDirectory::FindFile(const std::string_view& filePath) const
{
	if (filePath.empty())
		return std::nullopt;

	// DOS-style parser should work fine for ISO 9660 path names.  Only practical difference
	// is case sensitivity, and that won't matter for path splitting.
	std::vector<std::string_view> parts(Path::SplitWindowsPath(filePath));
	const IsoDirectory* dir = this;
	IsoDirectory subdir(internalReader);

	for (size_t index = 0; index < (parts.size() - 1); index++)
	{
		const int subdir_index = dir->GetIndexOf(parts[index]);
		if (subdir_index < 0)
			return std::nullopt;

		const IsoFileDescriptor& subdir_entry = GetEntry(static_cast<size_t>(subdir_index));
		if (subdir_entry.IsFile() || !subdir.Open(subdir_entry, nullptr))
			return std::nullopt;

		dir = &subdir;
	}

	const int file_index = dir->GetIndexOf(parts.back());
	if (file_index < 0)
		return std::nullopt;

	return dir->GetEntry(static_cast<size_t>(file_index));
}

bool IsoDirectory::Exists(const std::string_view& filePath) const
{
	if (filePath.empty())
		return false;

	const std::optional<IsoFileDescriptor> fd(FindFile(filePath));
	return fd.has_value();
}

bool IsoDirectory::IsFile(const std::string_view& filePath) const
{
	if (filePath.empty())
		return false;

	const std::optional<IsoFileDescriptor> fd(FindFile(filePath));
	if (fd.has_value())
		return false;

	return ((fd->flags & 2) != 2);
}

bool IsoDirectory::IsDir(const std::string_view& filePath) const
{
	if (filePath.empty())
		return false;

	const std::optional<IsoFileDescriptor> fd(FindFile(filePath));
	if (fd.has_value())
		return false;

	return ((fd->flags & 2) == 2);
}

u32 IsoDirectory::GetFileSize(const std::string_view& filePath) const
{
	if (filePath.empty())
		return 0;

	const std::optional<IsoFileDescriptor> fd(FindFile(filePath));
	if (fd.has_value())
		return 0;

	return fd->size;
}

IsoFileDescriptor::IsoFileDescriptor()
	: lba(0)
	, size(0)
	, flags(0)
{
	memset(&date, 0, sizeof(date));
}

IsoFileDescriptor::IsoFileDescriptor(const u8* data, int length)
{
	Load(data, length);
}

void IsoFileDescriptor::Load(const u8* data, int length)
{
	lba = (u32&)data[2];
	size = (u32&)data[10];

	date.year = data[18] + 1900;
	date.month = data[19];
	date.day = data[20];
	date.hour = data[21];
	date.minute = data[22];
	date.second = data[23];
	date.gmtOffset = data[24];

	flags = data[25];

	int file_name_length = data[32];

	if (file_name_length == 1)
	{
		u8 c = data[33];

		switch (c)
		{
			case 0:
				name = ".";
				break;
			case 1:
				name = "..";
				break;
			default:
				name = static_cast<char>(c);
				break;
		}
	}
	else
	{
		const u8* fnsrc = data + 33;

		// Strip any version information like the PS2 BIOS does.
		int length_without_version = 0;
		for (; length_without_version < file_name_length; length_without_version++)
		{
			if (fnsrc[length_without_version] == ';' || fnsrc[length_without_version] == '\0')
				break;
		}

		name.assign(reinterpret_cast<const char*>(fnsrc), length_without_version);
	}
}
