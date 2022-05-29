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

#define _PC_ // disables MIPS opcode macros.

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/ZipHelpers.h"

#include "Config.h"
#include "Patch.h"

#include <memory>
#include <sstream>
#include <vector>

// This is a declaration for PatchMemory.cpp::_ApplyPatch where we're (patch.cpp)
// the only consumer, so it's not made public via Patch.h
// Applies a single patch line to emulation memory regardless of its "place" value.
extern void _ApplyPatch(IniPatch* p);

static std::vector<IniPatch> Patch;

struct PatchTextTable
{
	int code;
	const char* text;
	PATCHTABLEFUNC* func;
};

static const PatchTextTable commands_patch[] =
	{
		{1, "author", PatchFunc::author},
		{2, "comment", PatchFunc::comment},
		{3, "patch", PatchFunc::patch},
		{0, nullptr, nullptr} // Array Terminator
};

static const PatchTextTable dataType[] =
	{
		{1, "byte", nullptr},
		{2, "short", nullptr},
		{3, "word", nullptr},
		{4, "double", nullptr},
		{5, "extended", nullptr},
		{6, "leshort", nullptr},
		{7, "leword", nullptr},
		{8, "ledouble", nullptr},
		{0, nullptr, nullptr} // Array Terminator
};

static const PatchTextTable cpuCore[] =
	{
		{1, "EE", nullptr},
		{2, "IOP", nullptr},
		{0, nullptr, nullptr} // Array Terminator
};

// IniFile Functions.

static void inifile_trim(std::string& buffer)
{
	StringUtil::StripWhitespace(&buffer);
	if (std::strncmp(buffer.c_str(), "//", 2) == 0)
	{
		// comment
		buffer.clear();
	}

	// check for comments at the end of a line
	const std::string::size_type pos = buffer.find("//");
	if (pos != std::string::npos)
		buffer.erase(pos);
}

static int PatchTableExecute(const std::string_view& lhs, const std::string_view& rhs, const PatchTextTable* Table)
{
	int i = 0;

	while (Table[i].text)
	{
		if (lhs.compare(Table[i].text) == 0)
		{
			if (Table[i].func)
				Table[i].func(lhs, rhs);
			break;
		}
		i++;
	}

	return Table[i].code;
}

// This routine is for executing the commands of the ini file.
static void inifile_command(const std::string& cmd)
{
	std::string_view key, value;
	StringUtil::ParseAssignmentString(cmd, &key, &value);

	// Is this really what we want to be doing here? Seems like just leaving it empty/blank
	// would make more sense... --air
	if (value.empty())
		value = key;

	/*int code = */ PatchTableExecute(key, value, commands_patch);
}

int LoadPatchesFromString(const std::string& patches)
{
	const size_t before = Patch.size();

	std::istringstream ss(patches);
	std::string line;
	while (std::getline(ss, line))
	{
		inifile_trim(line);
		if (!line.empty())
			inifile_command(line);
	}

	return static_cast<int>(Patch.size() - before);
}

void ForgetLoadedPatches()
{
	Patch.clear();
}

// This routine loads patches from a zip file
// Returns number of patches loaded
// Note: does not reset previously loaded patches (use ForgetLoadedPatches() for that)
// Note: only load patches from the root folder of the zip
int LoadPatchesFromZip(const std::string& crc, const u8* zip_data, size_t zip_data_size)
{
	zip_error ze = {};
	auto zf = zip_open_buffer_managed(zip_data, zip_data_size, ZIP_RDONLY, 0, &ze);
	if (!zf)
		return 0;

	const std::string pnach_filename(crc + ".pnach");
	std::optional<std::string> pnach_data(ReadFileInZipToString(zf.get(), pnach_filename.c_str()));
	if (!pnach_data.has_value())
		return 0;

	PatchesCon->WriteLn(Color_Green, "Loading patch '%s' from archive.", pnach_filename.c_str());
	return LoadPatchesFromString(pnach_data.value());
}


// This routine loads patches from *.pnach files
// Returns number of patches loaded
// Note: does not reset previously loaded patches (use ForgetLoadedPatches() for that)
int LoadPatchesFromDir(const std::string& crc, const std::string& folder, const char* friendly_name, bool show_error_when_missing)
{
	if (!FileSystem::DirectoryExists(folder.c_str()))
	{
		Console.WriteLn(Color_Red, "The %s folder ('%s') is inaccessible. Skipping...", friendly_name, folder.c_str());
		return 0;
	}

	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(folder.c_str(), StringUtil::StdStringFromFormat("*.pnach", crc.c_str()).c_str(),
		FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES, &files);

	if (show_error_when_missing && files.empty())
	{
		PatchesCon->WriteLn(Color_Gray, "Not found %s file: %s" FS_OSPATH_SEPARATOR_STR "%s.pnach",
			friendly_name, folder.c_str(), crc.c_str());
	}

	int total_loaded = 0;

	for (const FILESYSTEM_FIND_DATA& fd : files)
	{
		const std::string_view name(Path::GetFileName(fd.FileName));
		if (name.length() < crc.length() || StringUtil::Strncasecmp(name.data(), crc.c_str(), crc.size()) != 0)
			continue;

		PatchesCon->WriteLn(Color_Green, "Found %s file: '%.*s'", friendly_name, static_cast<int>(name.size()), name.data());

		const std::optional<std::string> pnach_data(FileSystem::ReadFileToString(fd.FileName.c_str()));
		if (!pnach_data.has_value())
			continue;

		const int loaded = LoadPatchesFromString(pnach_data.value());
		total_loaded += loaded;

		PatchesCon->WriteLn((loaded ? Color_Green : Color_Gray), "Loaded %d %s from '%.*s'.",
			loaded, friendly_name, static_cast<int>(name.size()), name.data());
	}

	PatchesCon->WriteLn((total_loaded ? Color_Green : Color_Gray), "Overall %d %s loaded", total_loaded, friendly_name);
	return total_loaded;
}

// PatchFunc Functions.
namespace PatchFunc
{
	void comment(const std::string_view& text1, const std::string_view& text2)
	{
		PatchesCon->WriteLn("comment: %.*s", static_cast<int>(text2.length()), text2.data());
	}

	void author(const std::string_view& text1, const std::string_view& text2)
	{
		PatchesCon->WriteLn("Author: %.*s", static_cast<int>(text2.length()), text2.data());
	}

	void patch(const std::string_view& cmd, const std::string_view& param)
	{
		// print the actual patch lines only in verbose mode (even in devel)
		if (DevConWriterEnabled)
		{
			DevCon.WriteLn("%.*s %.*s", static_cast<int>(cmd.size()), cmd.data(),
				static_cast<int>(param.size()), param.data());
		}

#define PATCH_ERROR(fmt, ...) Console.Error("(Patch) Error Parsing: %.*s=%.*s: " fmt, \
	static_cast<int>(cmd.size()), cmd.data(), static_cast<int>(param.size()), param.data(), \
	__VA_ARGS__)

		// [0]=PlaceToPatch,[1]=CpuType,[2]=MemAddr,[3]=OperandSize,[4]=WriteValue
		const std::vector<std::string_view> pieces(StringUtil::SplitString(param, ',', false));
		if (pieces.size() != 5)
		{
			PATCH_ERROR("Expected 5 data parameters; only found %zu", pieces.size());
			return;
		}

		IniPatch iPatch = {0};
		iPatch.enabled = 0;
		iPatch.placetopatch = StringUtil::FromChars<u32>(pieces[0]).value_or(_PPT_END_MARKER);

		if (iPatch.placetopatch >= _PPT_END_MARKER)
		{
			PATCH_ERROR("Invalid 'place' value '%.*s' (0 - once on startup, 1: continuously)",
				static_cast<int>(pieces[0].size()), pieces[0].data());
			return;
		}

		iPatch.cpu = (patch_cpu_type)PatchTableExecute(pieces[1], std::string_view(), cpuCore);
		iPatch.addr = StringUtil::FromChars<u32>(pieces[2], 16).value_or(0);
		iPatch.type = (patch_data_type)PatchTableExecute(pieces[3], std::string_view(), dataType);
		iPatch.data = StringUtil::FromChars<u64>(pieces[4], 16).value_or(0);

		if (iPatch.cpu == 0)
		{
			PATCH_ERROR("Unrecognized CPU Target: '%.*s'", static_cast<int>(pieces[1].size()), pieces[1].data());
			return;
		}

		if (iPatch.type == 0)
		{
			PATCH_ERROR("Unrecognized Operand Size: '%.*s'", static_cast<int>(pieces[3].size()), pieces[3].data());
			return;
		}

		iPatch.enabled = 1;
		Patch.push_back(iPatch);

#undef PATCH_ERROR
	}
} // namespace PatchFunc

// This is for applying patches directly to memory
void ApplyLoadedPatches(patch_place_type place)
{
	for (auto& i : Patch)
	{
		if (i.placetopatch == place)
			_ApplyPatch(&i);
	}
}
