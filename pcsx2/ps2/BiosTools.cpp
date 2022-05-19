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

#include <cstdio>
#include <cstring>

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include "Common.h"
#include "BiosTools.h"
#include "Config.h"

static constexpr u32 MIN_BIOS_SIZE = 4 * _1mb;
static constexpr u32 MAX_BIOS_SIZE = 8 * _1mb;
static constexpr u32 DIRENTRY_SIZE = 16;

// --------------------------------------------------------------------------------------
// romdir structure (packing required!)
// --------------------------------------------------------------------------------------
//
#pragma pack(push, 1)

struct romdir
{
	char fileName[10];
	u16 extInfoSize;
	u32 fileSize;
};

#pragma pack(pop)

static_assert(sizeof(romdir) == DIRENTRY_SIZE, "romdir struct not packed to 16 bytes");

u32 BiosVersion;
u32 BiosChecksum;
u32 BiosRegion;
bool NoOSD;
bool AllowParams1;
bool AllowParams2;
std::string BiosDescription;
std::string BiosZone;
std::string BiosPath;
BiosDebugInformation CurrentBiosInformation;
s64 BiosRegionOffset = 0;

static bool LoadBiosVersion(std::FILE* fp, u32& version, std::string& description, u32& region, std::string& zone)
{
	romdir rd;
	for (u32 i = 0; i < 512 * 1024; i++)
	{
		if (std::fread(&rd, sizeof(rd), 1, fp) != 1)
			return false;

		if (std::strncmp(rd.fileName, "RESET", sizeof(rd.fileName)) == 0)
			break; /* found romdir */
	}

	s64 fileOffset = 0;
	s64 fileSize = FileSystem::FSize64(fp);
	bool foundRomVer = false;

	// ensure it's a null-terminated and not zero-length string
	while (rd.fileName[0] != '\0' && strnlen(rd.fileName, sizeof(rd.fileName)) != sizeof(rd.fileName))
	{
		if (std::strncmp(rd.fileName, "ROMVER", sizeof(rd.fileName)) == 0)
		{
			char romver[14 + 1] = {}; // ascii version loaded from disk.

			s64 pos = FileSystem::FTell64(fp);
			if (FileSystem::FSeek64(fp, fileOffset, SEEK_SET) != 0 ||
				std::fread(romver, 14, 1, fp) != 1 || FileSystem::FSeek64(fp, pos, SEEK_SET) != 0)
			{
				break;
			}

			switch (romver[4])
			{
				// clang-format off
				case 'T': region = 0; break;
				case 'X': region = 1; break;
				case 'J': region = 2; break;
				case 'A': region = 3; break;
				case 'E': region = 4; break;
				case 'H': region = 5; break;
				case 'P': region = 6; break;
				case 'C': region = 7; break;
				// clang-format on
			}

			if (region <= 7)
			{
				zone = BiosZoneStrings[region];
			}
			else
			{
				zone.clear();
				zone += romver[4];
			}

			char vermaj[3] = {romver[0], romver[1], 0};
			char vermin[3] = {romver[2], romver[3], 0};

			description = StringUtil::StdStringFromFormat("%-7s v%s.%s(%c%c/%c%c/%c%c%c%c)  %s",
				zone.c_str(),
				vermaj, vermin,
				romver[12], romver[13], // day
				romver[10], romver[11], // month
				romver[6], romver[7], romver[8], romver[9], // year!
				(romver[5] == 'C') ? "Console" : (romver[5] == 'D') ? "Devel" : "");

			version = strtol(vermaj, (char**)NULL, 0) << 8;
			version |= strtol(vermin, (char**)NULL, 0);
			foundRomVer = true;
			BiosRegionOffset = fileOffset;

			Console.WriteLn("Bios Found: %s", description.c_str());
		}

		if ((rd.fileSize % 0x10) == 0)
			fileOffset += rd.fileSize;
		else
			fileOffset += (rd.fileSize + 0x10) & 0xfffffff0;

		if (std::fread(&rd, sizeof(rd), 1, fp) != 1)
			break;
	}

	fileOffset -= ((rd.fileSize + 0x10) & 0xfffffff0) - rd.fileSize;

	if (!foundRomVer)
		return false;

	if (fileSize < (int)fileOffset)
	{
		description += StringUtil::StdStringFromFormat(" %d%%", ((fileSize * 100) / (int)fileOffset));
		// we force users to have correct bioses,
		// not that lame scph10000 of 513KB ;-)
	}

	return true;
}

template <size_t _size>
void ChecksumIt(u32& result, const u8 (&srcdata)[_size])
{
	pxAssume((_size & 3) == 0);
	for (size_t i = 0; i < _size / 4; ++i)
		result ^= ((u32*)srcdata)[i];
}

// Attempts to load a BIOS rom sub-component, by trying multiple combinations of base
// filename and extension.  The bios specified in the user's configuration is used as
// the base.
//
// Parameters:
//   ext - extension of the sub-component to load.  Valid options are rom1, rom2, AND erom.
//
template <size_t _size>
static void LoadExtraRom(const char* ext, u8 (&dest)[_size])
{
	// Try first a basic extension concatenation (normally results in something like name.bin.rom1)
	std::string Bios1(StringUtil::StdStringFromFormat("%s.%s", BiosPath.c_str(), ext));

	s64 filesize;
	if ((filesize = FileSystem::GetPathFileSize(Bios1.c_str())) <= 0)
	{
		// Try the name properly extensioned next (name.rom1)
		Bios1 = Path::ReplaceExtension(BiosPath, ext);
		if ((filesize = FileSystem::GetPathFileSize(Bios1.c_str())) <= 0)
		{
			Console.WriteLn(Color_Gray, "BIOS %s module not found, skipping...", ext);
			return;
		}
	}

	auto fp = FileSystem::OpenManagedCFile(Bios1.c_str(), "rb");
	if (!fp || std::fread(dest, static_cast<size_t>(std::min<s64>(_size, filesize)), 1, fp.get()) != 1)
	{
		Console.Warning("BIOS Warning: %s could not be read (permission denied?)", ext);
		return;
	}
	// Checksum for ROM1, ROM2, EROM?  Rama says no, Gigaherz says yes.  I'm not sure either way.  --air
	//ChecksumIt( BiosChecksum, dest );
}

static void LoadIrx(const std::string& filename, u8* dest, size_t maxSize)
{
	auto fp = FileSystem::OpenManagedCFile(filename.c_str(), "rb");
	if (fp)
	{
		const s64 filesize = FileSystem::FSize64(fp.get());
		const s64 readSize = std::min(filesize, static_cast<s64>(maxSize));
		if (std::fread(dest, readSize, 1, fp.get()) == 1)
			return;
	}

	Console.Warning("IRX Warning: %s could not be read", filename.c_str());
	return;
}

static std::string FindBiosImage()
{
	Console.WriteLn("Searching for a BIOS image in '%s'...", EmuFolders::Bios.c_str());

	FileSystem::FindResultsArray results;
	if (!FileSystem::FindFiles(EmuFolders::Bios.c_str(), "*", FILESYSTEM_FIND_FILES, &results))
		return std::string();

	u32 version, region;
	std::string description, zone;
	for (const FILESYSTEM_FIND_DATA& fd : results)
	{
		if (fd.Size < MIN_BIOS_SIZE || fd.Size > MAX_BIOS_SIZE)
			continue;

		if (IsBIOS(fd.FileName.c_str(), version, description, region, zone))
		{
			Console.WriteLn("Using BIOS '%s' (%s %s)", fd.FileName.c_str(), description.c_str(), zone.c_str());
			return std::move(fd.FileName);
		}
	}

	Console.Error("Unable to auto locate a BIOS image");
	return std::string();
}

// Loads the configured bios rom file into PS2 memory.  PS2 memory must be allocated prior to
// this method being called.
//
// Remarks:
//   This function does not fail if rom1, rom2, or erom files are missing, since none are
//   explicitly required for most emulation tasks.
//
// Exceptions:
//   BadStream - Thrown if the primary bios file (usually .bin) is not found, corrupted, etc.
//
bool LoadBIOS()
{
	pxAssertDev(eeMem->ROM != NULL, "PS2 system memory has not been initialized yet.");

	std::string path = EmuConfig.FullpathToBios();
	if (path.empty() || !FileSystem::FileExists(path.c_str()))
	{
		if (!path.empty())
		{
			Console.Warning("Configured BIOS '%s' does not exist, trying to find an alternative.",
				EmuConfig.BaseFilenames.Bios.c_str());
		}

		path = FindBiosImage();
		if (path.empty())
			return false;
	}

	auto fp = FileSystem::OpenManagedCFile(path.c_str(), "rb");
	if (!fp)
		return false;

	const s64 filesize = FileSystem::FSize64(fp.get());
	if (filesize <= 0)
		return false;

	LoadBiosVersion(fp.get(), BiosVersion, BiosDescription, BiosRegion, BiosZone);

	if (FileSystem::FSeek64(fp.get(), 0, SEEK_SET) ||
		std::fread(eeMem->ROM, static_cast<size_t>(std::min<s64>(Ps2MemSize::Rom, filesize)), 1, fp.get()) != 1)
	{
		return false;
	}

	// If file is less than 2mb it doesn't have an OSD (Devel consoles)
	// So skip HLEing OSDSys Param stuff
	if (filesize < 2465792)
		NoOSD = true;
	else
		NoOSD = false;

	BiosChecksum = 0;
	ChecksumIt(BiosChecksum, eeMem->ROM);
	BiosPath = std::move(path);

	// Patch the region
	if (EmuConfig.PatchBios)
	{
		eeMem->ROM[BiosRegionOffset + 4] = EmuConfig.PatchRegion[0];
		Console.WriteLn("Patching ROM with region code %c", EmuConfig.PatchRegion[0]);
	}

#ifndef PCSX2_CORE
	Console.SetTitle(StringUtil::StdStringFromFormat("Running BIOS (%s v%u.%u)",
		BiosZone.c_str(), BiosVersion >> 8, BiosVersion & 0xff).c_str());
#endif

	//injectIRX("host.irx");	//not fully tested; still buggy

	LoadExtraRom("rom1", eeMem->ROM1);
	LoadExtraRom("rom2", eeMem->ROM2);
	LoadExtraRom("erom", eeMem->EROM);

	if (EmuConfig.CurrentIRX.length() > 3)
		LoadIrx(EmuConfig.CurrentIRX, &eeMem->ROM[0x3C0000], sizeof(eeMem->ROM) - 0x3C0000);

	CurrentBiosInformation.threadListAddr = 0;
	return true;
}

bool IsBIOS(const char* filename, u32& version, std::string& description, u32& region, std::string& zone)
{
	const std::string bios_path(Path::Combine(EmuFolders::Bios, filename));
	const auto fp = FileSystem::OpenManagedCFile(filename, "rb");
	if (!fp)
		return false;

	// FPS2BIOS is smaller and of variable size
	//if (inway.Length() < 512*1024) return false;
	return LoadBiosVersion(fp.get(), version, description, region, zone);
}

bool IsBIOSAvailable(const std::string& full_path)
{
	// We can't use EmuConfig here since it may not be loaded yet.
	if (!full_path.empty() && FileSystem::FileExists(full_path.c_str()))
		return true;

	// No bios configured or the configured name is missing, check for one in the BIOS directory.
	const std::string auto_path(FindBiosImage());
	return !auto_path.empty() && FileSystem::FileExists(auto_path.c_str());
}
