/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include "common/ByteSwap.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/ZipHelpers.h"

#include "Achievements.h"
#include "Config.h"
#include "GameDatabase.h"
#include "Host.h"
#include "IopMem.h"
#include "Memory.h"
#include "Patch.h"

#include "IconsFontAwesome5.h"
#include "fmt/format.h"
#include "gsl/span"

#include <algorithm>
#include <cstring>
#include <memory>
#include <sstream>
#include <vector>

namespace Patch
{
	enum patch_cpu_type
	{
		NO_CPU,
		CPU_EE,
		CPU_IOP
	};

	enum patch_data_type
	{
		NO_TYPE,
		BYTE_T,
		SHORT_T,
		WORD_T,
		DOUBLE_T,
		EXTENDED_T,
		SHORT_BE_T,
		WORD_BE_T,
		DOUBLE_BE_T
	};

	struct PatchCommand
	{
		patch_data_type type;
		patch_cpu_type cpu;
		int placetopatch;
		u32 addr;
		u64 data;

		bool operator==(const PatchCommand& p) const { return std::memcmp(this, &p, sizeof(*this)) == 0; }
		bool operator!=(const PatchCommand& p) const { return std::memcmp(this, &p, sizeof(*this)) != 0; }
	};
	static_assert(sizeof(PatchCommand) == 24, "IniPatch has no padding");

	struct PatchGroup
	{
		std::string name;
		std::optional<AspectRatioType> override_aspect_ratio;
		std::optional<GSInterlaceMode> override_interlace_mode;
		std::vector<PatchCommand> patches;
	};

	struct PatchTextTable
	{
		int code;
		const char* text;
		void (*func)(PatchGroup* group, const std::string_view& cmd, const std::string_view& param);
	};

	using PatchList = std::vector<PatchGroup>;
	using ActivePatchList = std::vector<const PatchCommand*>;
	using EnablePatchList = std::vector<std::string>;

	namespace PatchFunc
	{
		static void author(PatchGroup* group, const std::string_view& cmd, const std::string_view& param);
		static void comment(PatchGroup* group, const std::string_view& cmd, const std::string_view& param);
		static void patch(PatchGroup* group, const std::string_view& cmd, const std::string_view& param);
		static void gsaspectratio(PatchGroup* group, const std::string_view& cmd, const std::string_view& param);
		static void gsinterlacemode(PatchGroup* group, const std::string_view& cmd, const std::string_view& param);
	} // namespace PatchFunc

	static void TrimPatchLine(std::string& buffer);
	static int PatchTableExecute(PatchGroup* group, const std::string_view& lhs, const std::string_view& rhs,
		const gsl::span<const PatchTextTable>& Table);
	static void LoadPatchLine(PatchGroup* group, const std::string_view& line);
	static u32 LoadPatchesFromString(PatchList* patch_list, const std::string& patch_file);
	static bool OpenPatchesZip();
	static std::string GetPnachTemplate(
		const std::string_view& serial, u32 crc, bool include_serial, bool add_wildcard);
	static std::vector<std::string> FindPatchFilesOnDisk(const std::string_view& serial, u32 crc, bool cheats);

	template <typename F>
	static void EnumeratePnachFiles(const std::string_view& serial, u32 crc, bool cheats, const F& f);

	static void ExtractPatchInfo(PatchInfoList* dst, const std::string& pnach_data, u32* num_unlabelled_patches);
	static void ReloadEnabledLists();
	static u32 EnablePatches(const PatchList& patches, const EnablePatchList& enable_list);

	static void ApplyPatch(const PatchCommand* p);
	static void ApplyDynaPatch(const DynamicPatch& patch, u32 address);
	static void writeCheat();
	static void handle_extended_t(const PatchCommand* p);

	// Name of patches which will be auto-enabled based on global options.
	static constexpr std::string_view WS_PATCH_NAME = "Widescreen 16:9";
	static constexpr std::string_view NI_PATCH_NAME = "No-Interlacing";
	static constexpr std::string_view PATCHES_ZIP_NAME = "patches.zip";

	const char* PATCHES_CONFIG_SECTION = "Patches";
	const char* CHEATS_CONFIG_SECTION = "Cheats";
	const char* PATCH_ENABLE_CONFIG_KEY = "Enable";

	static zip_t* s_patches_zip;
	static PatchList s_gamedb_patches;
	static PatchList s_game_patches;
	static PatchList s_cheat_patches;

	static ActivePatchList s_active_patches;
	static std::vector<DynamicPatch> s_active_dynamic_patches;
	static EnablePatchList s_enabled_cheats;
	static EnablePatchList s_enabled_patches;
	static u32 s_patches_crc;
	static std::string s_patches_serial;
	static std::optional<AspectRatioType> s_override_aspect_ratio;
	static std::optional<GSInterlaceMode> s_override_interlace_mode;

	static const PatchTextTable s_patch_commands[] = {
		{0, "author", &Patch::PatchFunc::author},
		{0, "comment", &Patch::PatchFunc::comment},
		{0, "patch", &Patch::PatchFunc::patch},
		{0, "gsaspectratio", &Patch::PatchFunc::gsaspectratio},
		{0, "gsinterlacemode", &Patch::PatchFunc::gsinterlacemode},
		{0, nullptr, nullptr},
	};

	static const PatchTextTable s_type_commands[] = {
		{BYTE_T, "byte", nullptr},
		{SHORT_T, "short", nullptr},
		{WORD_T, "word", nullptr},
		{DOUBLE_T, "double", nullptr},
		{EXTENDED_T, "extended", nullptr},
		{SHORT_BE_T, "beshort", nullptr},
		{WORD_BE_T, "beword", nullptr},
		{DOUBLE_BE_T, "bedouble", nullptr},
		{NO_TYPE, nullptr, nullptr},
	};

	static const PatchTextTable s_cpu_commands[] = {
		{CPU_EE, "EE", nullptr},
		{CPU_IOP, "IOP", nullptr},
		{NO_CPU, nullptr, nullptr},
	};
} // namespace Patch

void Patch::TrimPatchLine(std::string& buffer)
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

int Patch::PatchTableExecute(PatchGroup* group, const std::string_view& lhs, const std::string_view& rhs,
	const gsl::span<const PatchTextTable>& Table)
{
	int i = 0;

	while (Table[i].text)
	{
		if (lhs.compare(Table[i].text) == 0)
		{
			if (Table[i].func)
				Table[i].func(group, lhs, rhs);
			break;
		}
		i++;
	}

	return Table[i].code;
}

// This routine is for executing the commands of the ini file.
void Patch::LoadPatchLine(PatchGroup* group, const std::string_view& line)
{
	std::string_view key, value;
	StringUtil::ParseAssignmentString(line, &key, &value);

	PatchTableExecute(group, key, value, s_patch_commands);
}

u32 Patch::LoadPatchesFromString(PatchList* patch_list, const std::string& patch_file)
{
	const size_t before = patch_list->size();

	PatchGroup current_patch_group;

	std::istringstream ss(patch_file);
	std::string line;
	while (std::getline(ss, line))
	{
		TrimPatchLine(line);
		if (line.empty())
			continue;

		if (line.front() == '[')
		{
			if (line.length() < 2 || line.back() != ']')
			{
				Console.Error(fmt::format("Malformed patch line: {}", line.c_str()));
				continue;
			}

			if (!current_patch_group.name.empty() || !current_patch_group.patches.empty())
			{
				patch_list->push_back(std::move(current_patch_group));
				current_patch_group = {};
			}

			current_patch_group.name = line.substr(1, line.length() - 2);
			continue;
		}

		LoadPatchLine(&current_patch_group, line);
	}

	if (!current_patch_group.name.empty() || !current_patch_group.patches.empty())
		patch_list->push_back(std::move(current_patch_group));

	return static_cast<u32>(patch_list->size() - before);
}

bool Patch::OpenPatchesZip()
{
	if (s_patches_zip)
		return true;

	const std::string filename = Path::Combine(EmuFolders::Resources, PATCHES_ZIP_NAME);

	zip_error ze = {};
	zip_source_t* zs = zip_source_file_create(filename.c_str(), 0, 0, &ze);
	if (zs && !(s_patches_zip = zip_open_from_source(zs, ZIP_RDONLY, &ze)))
	{
		static bool warning_shown = false;
		if (!warning_shown)
		{
			Host::AddIconOSDMessage("PatchesZipOpenWarning", ICON_FA_MICROCHIP,
				fmt::format("Failed to open {}. Built-in game patches are not available.", PATCHES_ZIP_NAME),
				Host::OSD_ERROR_DURATION);
			warning_shown = true;
		}

		// have to clean up source
		Console.Error("Failed to open %s: %s", filename.c_str(), zip_error_strerror(&ze));
		zip_source_free(zs);
		return false;
	}

	std::atexit([]() { zip_close(s_patches_zip); });
	return true;
}

std::string Patch::GetPnachTemplate(const std::string_view& serial, u32 crc, bool include_serial, bool add_wildcard)
{
	if (include_serial)
		return fmt::format("{}_{:08X}{}.pnach", serial, crc, add_wildcard ? "*" : "");
	else
		return fmt::format("{:08X}{}.pnach", crc, add_wildcard ? "*" : "");
}

std::vector<std::string> Patch::FindPatchFilesOnDisk(const std::string_view& serial, u32 crc, bool cheats)
{
	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(cheats ? EmuFolders::Cheats.c_str() : EmuFolders::Patches.c_str(),
		GetPnachTemplate(serial, crc, true, true).c_str(), FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES,
		&files);

	std::vector<std::string> ret;
	ret.reserve(files.size());

	for (FILESYSTEM_FIND_DATA& fd : files)
		ret.push_back(std::move(fd.FileName));

	// and patches without serials
	FileSystem::FindFiles(cheats ? EmuFolders::Cheats.c_str() : EmuFolders::Patches.c_str(),
		GetPnachTemplate(serial, crc, false, true).c_str(), FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES,
		&files);
	ret.reserve(ret.size() + files.size());
	for (FILESYSTEM_FIND_DATA& fd : files)
		ret.push_back(std::move(fd.FileName));

	return ret;
}

template <typename F>
void Patch::EnumeratePnachFiles(const std::string_view& serial, u32 crc, bool cheats, const F& f)
{
	// Prefer files on disk over the zip.
	std::vector<std::string> disk_patch_files;
	if (cheats || !Achievements::ChallengeModeActive())
		disk_patch_files = FindPatchFilesOnDisk(serial, crc, cheats);

	if (!disk_patch_files.empty())
	{
		for (const std::string& file : disk_patch_files)
		{
			std::optional<std::string> contents = FileSystem::ReadFileToString(file.c_str());
			if (contents.has_value())
				f(std::move(file), std::move(contents.value()));
		}

		return;
	}

	// Otherwise fall back to the zip.
	if (cheats || !OpenPatchesZip())
		return;

	// Prefer filename with serial.
	std::string zip_filename = GetPnachTemplate(serial, crc, true, false);
	std::optional<std::string> pnach_data(ReadFileInZipToString(s_patches_zip, zip_filename.c_str()));
	if (!pnach_data.has_value())
	{
		zip_filename = GetPnachTemplate(serial, crc, false, false);
		pnach_data = ReadFileInZipToString(s_patches_zip, zip_filename.c_str());
	}
	if (pnach_data.has_value())
		f(std::move(zip_filename), std::move(pnach_data.value()));
}

void Patch::ExtractPatchInfo(PatchInfoList* dst, const std::string& pnach_data, u32* num_unlabelled_patches)
{
	std::istringstream ss(pnach_data);
	std::string line;
	PatchInfo current_patch;
	while (std::getline(ss, line))
	{
		TrimPatchLine(line);
		if (line.empty())
			continue;

		const bool has_patch = !current_patch.name.empty();

		if (line.length() > 2 && line.front() == '[' && line.back() == ']')
		{
			if (has_patch)
			{
				dst->push_back(std::move(current_patch));
				current_patch = {};
			}

			current_patch.name = line.substr(1, line.length() - 2);
			continue;
		}

		std::string_view key, value;
		StringUtil::ParseAssignmentString(line, &key, &value);

		// Just ignore other directives, who knows what rubbish people have in here.
		// Use comment for description if it hasn't been otherwise specified.
		if (key == "author")
			current_patch.author = value;
		else if (key == "description")
			current_patch.description = value;
		else if (key == "comment" && current_patch.description.empty())
			current_patch.description = value;
		else if (key == "patch" && !has_patch && num_unlabelled_patches)
			(*num_unlabelled_patches)++;
	}

	// Last one.
	if (!current_patch.name.empty())
		dst->push_back(std::move(current_patch));
}

std::string_view Patch::PatchInfo::GetNamePart() const
{
	const std::string::size_type pos = name.rfind('\\');
	std::string_view ret = name;
	if (pos != std::string::npos)
		ret = ret.substr(pos + 1);
	return ret;
}

std::string_view Patch::PatchInfo::GetNameParentPart() const
{
	const std::string::size_type pos = name.rfind('\\');
	std::string_view ret;
	if (pos != std::string::npos)
		ret = std::string_view(name).substr(0, pos);
	return ret;
}

Patch::PatchInfoList Patch::GetPatchInfo(const std::string& serial, u32 crc, bool cheats, u32* num_unlabelled_patches)
{
	PatchInfoList ret;

	if (num_unlabelled_patches)
		*num_unlabelled_patches = 0;

	EnumeratePnachFiles(serial, crc, cheats,
		[&ret, num_unlabelled_patches](const std::string& filename, const std::string& pnach_data) {
			ExtractPatchInfo(&ret, pnach_data, num_unlabelled_patches);
		});

	return ret;
}

void Patch::ReloadEnabledLists()
{
	if (EmuConfig.EnableCheats && !Achievements::ChallengeModeActive())
		s_enabled_cheats = Host::GetStringListSetting(CHEATS_CONFIG_SECTION, PATCH_ENABLE_CONFIG_KEY);
	else
		s_enabled_cheats = {};

	s_enabled_patches = Host::GetStringListSetting(PATCHES_CONFIG_SECTION, PATCH_ENABLE_CONFIG_KEY);

	// Name based matching for widescreen/NI settings.
	if (EmuConfig.EnableWideScreenPatches)
	{
		if (std::none_of(s_enabled_patches.begin(), s_enabled_patches.end(),
				[](const std::string& it) { return (it == WS_PATCH_NAME); }))
		{
			s_enabled_patches.emplace_back(WS_PATCH_NAME);
		}
	}
	if (EmuConfig.EnableNoInterlacingPatches)
	{
		if (std::none_of(s_enabled_patches.begin(), s_enabled_patches.end(),
				[](const std::string& it) { return (it == NI_PATCH_NAME); }))
		{
			s_enabled_patches.emplace_back(NI_PATCH_NAME);
		}
	}
}

u32 Patch::EnablePatches(const PatchList& patches, const EnablePatchList& enable_list)
{
	u32 count = 0;
	for (const PatchGroup& p : patches)
	{
		// For compatibility, we auto enable anything that's not labelled.
		// Also for gamedb patches.
		if (!p.name.empty() && std::find(enable_list.begin(), enable_list.end(), p.name) == enable_list.end())
			continue;

		if (!p.name.empty())
			Console.WriteLn(Color_Green, fmt::format("Enabled patch: '{}'", p.name));

		for (const PatchCommand& ip : p.patches)
			s_active_patches.push_back(&ip);

		if (p.override_aspect_ratio.has_value())
			s_override_aspect_ratio = p.override_aspect_ratio;
		if (p.override_interlace_mode.has_value())
			s_override_interlace_mode = p.override_interlace_mode;

		count++;
	}

	return count;
}

void Patch::ReloadPatches(std::string serial, u32 crc, bool force_reload_files, bool reload_enabled_list, bool verbose)
{
	const bool serial_changed = (s_patches_serial != serial);
	s_patches_crc = crc;
	s_patches_serial = std::move(serial);

	// Skip reloading gamedb patches if the serial hasn't changed.
	if (serial_changed)
	{
		s_gamedb_patches.clear();

		const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_patches_serial);
		if (game)
		{
			const std::string* patches = game->findPatch(crc);
			if (patches)
			{
				const u32 patch_count = LoadPatchesFromString(&s_gamedb_patches, *patches);
				if (patch_count > 0)
					Console.WriteLn(Color_Green, "(GameDB) Patches Loaded: %d", patch_count);
			}

			LoadDynamicPatches(game->dynaPatches);
		}
	}

	ReloadPatches(serial_changed, reload_enabled_list, verbose);
}

void Patch::ReloadPatches(bool force_reload_files, bool reload_enabled_list, bool verbose)
{
	if (force_reload_files)
	{
		s_game_patches.clear();
		EnumeratePnachFiles(
			s_patches_serial, s_patches_crc, false, [](const std::string& filename, const std::string& pnach_data) {
				const u32 patch_count = LoadPatchesFromString(&s_game_patches, pnach_data);
				if (patch_count > 0)
					Console.WriteLn(Color_Green, fmt::format("Loaded {} game patches from {}.", patch_count, filename));
			});

		s_cheat_patches.clear();
		EnumeratePnachFiles(
			s_patches_serial, s_patches_crc, true, [](const std::string& filename, const std::string& pnach_data) {
				const u32 patch_count = LoadPatchesFromString(&s_cheat_patches, pnach_data);
				if (patch_count > 0)
					Console.WriteLn(Color_Green, fmt::format("Loaded {} cheats from {}.", patch_count, filename));
			});
	}

	UpdateActivePatches(reload_enabled_list, verbose, false);
}

void Patch::UpdateActivePatches(bool reload_enabled_list, bool verbose, bool verbose_if_changed)
{
	if (reload_enabled_list)
		ReloadEnabledLists();

	const size_t prev_count = s_active_patches.size();
	s_active_patches.clear();
	s_override_aspect_ratio.reset();
	s_override_interlace_mode.reset();

	std::string message;
	if (EmuConfig.EnablePatches)
	{
		const u32 gp_count = EnablePatches(s_gamedb_patches, EnablePatchList());
		if (gp_count > 0)
			fmt::format_to(std::back_inserter(message), "{} GameDB patches", gp_count);
	}

	const u32 p_count = EnablePatches(s_game_patches, s_enabled_patches);
	if (p_count > 0)
		fmt::format_to(std::back_inserter(message), "{}{} game patches", message.empty() ? "" : ", ", p_count);

	const u32 c_count = EmuConfig.EnableCheats ? EnablePatches(s_cheat_patches, s_enabled_cheats) : 0;
	if (c_count > 0)
		fmt::format_to(std::back_inserter(message), "{}{} cheat patches", message.empty() ? "" : ", ", c_count);

	// Display message on first boot when we load patches.
	if (verbose || (verbose_if_changed && prev_count != s_active_patches.size()))
	{
		if (!message.empty())
		{
			fmt::format_to(std::back_inserter(message), " are active.");
			Host::AddIconOSDMessage("LoadPatches", ICON_FA_FILE_CODE, std::move(message), Host::OSD_INFO_DURATION);
		}
		else
		{
			Host::AddIconOSDMessage("LoadPatches", ICON_FA_FILE_CODE,
				"No cheats or patches (widescreen, compatibility or others) are found / enabled.",
				Host::OSD_INFO_DURATION);
		}
	}
}

void Patch::ApplyPatchSettingOverrides()
{
	// Switch to 16:9 if widescreen patches are enabled, and AR is auto.
	if (s_override_aspect_ratio.has_value() && EmuConfig.GS.AspectRatio == AspectRatioType::RAuto4_3_3_2)
	{
		// Don't change when reloading settings in the middle of a FMV with switch.
		if (EmuConfig.CurrentAspectRatio == EmuConfig.GS.AspectRatio)
			EmuConfig.CurrentAspectRatio = s_override_aspect_ratio.value();

		Console.WriteLn(Color_Gray,
			fmt::format("Patch: Setting aspect ratio to {} by patch request.",
				Pcsx2Config::GSOptions::AspectRatioNames[static_cast<int>(s_override_aspect_ratio.value())]));
		EmuConfig.GS.AspectRatio = s_override_aspect_ratio.value();
	}

	// Disable interlacing in GS if active.
	if (s_override_interlace_mode.has_value() && EmuConfig.GS.InterlaceMode == GSInterlaceMode::Automatic)
	{
		Console.WriteLn(Color_Gray, fmt::format("Patch: Setting deinterlace mode to {} by patch request.",
										static_cast<int>(s_override_interlace_mode.value())));
		EmuConfig.GS.InterlaceMode = s_override_interlace_mode.value();
	}
}

bool Patch::ReloadPatchAffectingOptions()
{
	const AspectRatioType current_ar = EmuConfig.GS.AspectRatio;
	const GSInterlaceMode current_interlace = EmuConfig.GS.InterlaceMode;

	// This is pretty gross, but we're not using a config layer, so...
	AspectRatioType new_ar = Pcsx2Config::GSOptions::DEFAULT_ASPECT_RATIO;
	const std::string ar_value = Host::GetStringSettingValue("EmuCore/GS", "AspectRatio",
		Pcsx2Config::GSOptions::AspectRatioNames[static_cast<u8>(EmuConfig.GS.AspectRatio)]);
	for (u32 i = 0; i < static_cast<u32>(AspectRatioType::MaxCount); i++)
	{
		if (ar_value == Pcsx2Config::GSOptions::AspectRatioNames[i])
		{
			new_ar = static_cast<AspectRatioType>(i);
			break;
		}
	}
	if (EmuConfig.CurrentAspectRatio == EmuConfig.GS.AspectRatio)
		EmuConfig.CurrentAspectRatio = new_ar;
	EmuConfig.GS.AspectRatio = new_ar;
	EmuConfig.GS.InterlaceMode = static_cast<GSInterlaceMode>(Host::GetIntSettingValue(
		"EmuCore/GS", "deinterlace_mode", static_cast<int>(Pcsx2Config::GSOptions::DEFAULT_INTERLACE_MODE)));

	ApplyPatchSettingOverrides();

	return (current_ar != EmuConfig.GS.AspectRatio || current_interlace != EmuConfig.GS.InterlaceMode);
}

void Patch::UnloadPatches()
{
	s_override_interlace_mode = {};
	s_override_aspect_ratio = {};
	s_patches_crc = 0;
	s_patches_serial = {};
	s_active_patches = {};
	s_active_dynamic_patches = {};
	s_enabled_patches = {};
	s_enabled_cheats = {};
	s_cheat_patches = {};
	s_game_patches = {};
	s_gamedb_patches = {};
}

// PatchFunc Functions.
void Patch::PatchFunc::comment(PatchGroup* group, const std::string_view& cmd, const std::string_view& param)
{
	Console.WriteLn(fmt::format("Patch comment: {}", param));
}

void Patch::PatchFunc::author(PatchGroup* group, const std::string_view& cmd, const std::string_view& param)
{
	Console.WriteLn(fmt::format("Patch author: {}", param));
}

void Patch::PatchFunc::patch(PatchGroup* group, const std::string_view& cmd, const std::string_view& param)
{
	// print the actual patch lines only in verbose mode (even in devel)
	if (DevConWriterEnabled)
		DevCon.WriteLn(fmt::format("{} {}", cmd, param));

#define PATCH_ERROR(fstring, ...) \
	Console.Error(fmt::format("(Patch) Error Parsing: {}={}: " fstring, cmd, param, __VA_ARGS__))

	// [0]=PlaceToPatch,[1]=CpuType,[2]=MemAddr,[3]=OperandSize,[4]=WriteValue
	const std::vector<std::string_view> pieces(StringUtil::SplitString(param, ',', false));
	if (pieces.size() != 5)
	{
		PATCH_ERROR("Expected 5 data parameters; only found {}", pieces.size());
		return;
	}

	std::string_view placetopatch_end;
	const std::optional<u32> placetopatch = StringUtil::FromChars<u32>(pieces[0], 10, &placetopatch_end);

	PatchCommand iPatch;
	iPatch.placetopatch = StringUtil::FromChars<u32>(pieces[0]).value_or(PPT_END_MARKER);

	if (!placetopatch.has_value() || !placetopatch_end.empty() ||
		(iPatch.placetopatch = placetopatch.value()) >= PPT_END_MARKER)
	{
		PATCH_ERROR("Invalid 'place' value '{}' (0 - once on startup, 1: continuously)", pieces[0]);
		return;
	}

	std::string_view addr_end, data_end;
	const std::optional<u32> addr = StringUtil::FromChars<u32>(pieces[2], 16, &addr_end);
	const std::optional<u64> data = StringUtil::FromChars<u64>(pieces[4], 16, &data_end);
	if (!addr.has_value() || !addr_end.empty())
	{
		PATCH_ERROR("Malformed address '{}', a hex number without prefix (e.g. 0123ABCD) is expected", pieces[2]);
		return;
	}
	else if (!data.has_value() || !data_end.empty())
	{
		PATCH_ERROR("Malformed data '{}', a hex number without prefix (e.g. 0123ABCD) is expected", pieces[4]);
		return;
	}

	iPatch.cpu = (patch_cpu_type)PatchTableExecute(group, pieces[1], std::string_view(), s_cpu_commands);
	iPatch.addr = addr.value();
	iPatch.type = (patch_data_type)PatchTableExecute(group, pieces[3], std::string_view(), s_type_commands);
	iPatch.data = data.value();

	if (iPatch.cpu == 0)
	{
		PATCH_ERROR("Unrecognized CPU Target: '%.*s'", pieces[1]);
		return;
	}

	if (iPatch.type == 0)
	{
		PATCH_ERROR("Unrecognized Operand Size: '%.*s'", pieces[3]);
		return;
	}

	group->patches.push_back(iPatch);

#undef PATCH_ERROR
}

void Patch::PatchFunc::gsaspectratio(PatchGroup* group, const std::string_view& cmd, const std::string_view& param)
{
	for (u32 i = 0; i < static_cast<u32>(AspectRatioType::MaxCount); i++)
	{
		if (param == Pcsx2Config::GSOptions::AspectRatioNames[i])
		{
			group->override_aspect_ratio = static_cast<AspectRatioType>(i);
			return;
		}
	}

	Console.Error(fmt::format("Patch error: {} is an unknown aspect ratio.", param));
}

void Patch::PatchFunc::gsinterlacemode(PatchGroup* group, const std::string_view& cmd, const std::string_view& param)
{
	const std::optional<int> interlace_mode = StringUtil::FromChars<int>(param);
	if (!interlace_mode.has_value() || interlace_mode.value() < 0 ||
		interlace_mode.value() >= static_cast<int>(GSInterlaceMode::Count))
	{
		Console.Error(fmt::format("Patch error: {} is an unknown interlace mode.", param));
		return;
	}

	group->override_interlace_mode = static_cast<GSInterlaceMode>(interlace_mode.value());
}

// This is for applying patches directly to memory
void Patch::ApplyLoadedPatches(patch_place_type place)
{
	for (const PatchCommand* i : s_active_patches)
	{
		if (i->placetopatch == place)
			ApplyPatch(i);
	}
}

void Patch::ApplyDynamicPatches(u32 pc)
{
	for (const auto& dynpatch : s_active_dynamic_patches)
		ApplyDynaPatch(dynpatch, pc);
}

void Patch::LoadDynamicPatches(const std::vector<DynamicPatch>& patches)
{
	for (const DynamicPatch& it : patches)
		s_active_dynamic_patches.push_back(it);
}

static u32 SkipCount = 0, IterationCount = 0;
static u32 IterationIncrement = 0, ValueIncrement = 0;
static u32 PrevCheatType = 0, PrevCheatAddr = 0, LastType = 0;

void Patch::writeCheat()
{
	switch (LastType)
	{
		case 0x0:
			memWrite8(PrevCheatAddr, IterationIncrement & 0xFF);
			break;
		case 0x1:
			memWrite16(PrevCheatAddr, IterationIncrement & 0xFFFF);
			break;
		case 0x2:
			memWrite32(PrevCheatAddr, IterationIncrement);
			break;
		default:
			break;
	}
}

void Patch::handle_extended_t(const PatchCommand* p)
{
	if (SkipCount > 0)
	{
		SkipCount--;
	}
	else
		switch (PrevCheatType)
		{
			case 0x3040: // vvvvvvvv 00000000 Inc
			{
				u32 mem = memRead32(PrevCheatAddr);
				memWrite32(PrevCheatAddr, mem + (p->addr));
				PrevCheatType = 0;
				break;
			}

			case 0x3050: // vvvvvvvv 00000000 Dec
			{
				u32 mem = memRead32(PrevCheatAddr);
				memWrite32(PrevCheatAddr, mem - (p->addr));
				PrevCheatType = 0;
				break;
			}

			case 0x4000: // vvvvvvvv iiiiiiii
				for (u32 i = 0; i < IterationCount; i++)
				{
					memWrite32((u32)(PrevCheatAddr + (i * IterationIncrement)), (u32)(p->addr + ((u32)p->data * i)));
				}
				PrevCheatType = 0;
				break;

			case 0x5000: // bbbbbbbb 00000000
				for (u32 i = 0; i < IterationCount; i++)
				{
					u8 mem = memRead8(PrevCheatAddr + i);
					memWrite8((p->addr + i) & 0x0FFFFFFF, mem);
				}
				PrevCheatType = 0;
				break;

			case 0x6000: // 000Xnnnn iiiiiiii
			{
				// Get Number of pointers
				if (((u32)p->addr & 0x0000FFFF) == 0)
					IterationCount = 1;
				else
					IterationCount = (u32)p->addr & 0x0000FFFF;

				// Read first pointer
				LastType = ((u32)p->addr & 0x000F0000) >> 16;
				u32 mem = memRead32(PrevCheatAddr);

				PrevCheatAddr = mem + (u32)p->data;
				IterationCount--;

				// Check if needed to read another pointer
				if (IterationCount == 0)
				{
					PrevCheatType = 0;
					if (((mem & 0x0FFFFFFF) & 0x3FFFFFFC) != 0)
						writeCheat();
				}
				else
				{
					if (((mem & 0x0FFFFFFF) & 0x3FFFFFFC) == 0)
						PrevCheatType = 0;
					else
						PrevCheatType = 0x6001;
				}
			}
			break;

			case 0x6001: // 000Xnnnn iiiiiiii
			{
				// Read first pointer
				u32 mem = memRead32(PrevCheatAddr & 0x0FFFFFFF);

				PrevCheatAddr = mem + (u32)p->addr;
				IterationCount--;

				// Check if needed to read another pointer
				if (IterationCount == 0)
				{
					PrevCheatType = 0;
					if (((mem & 0x0FFFFFFF) & 0x3FFFFFFC) != 0)
						writeCheat();
				}
				else
				{
					mem = memRead32(PrevCheatAddr);

					PrevCheatAddr = mem + (u32)p->data;
					IterationCount--;
					if (IterationCount == 0)
					{
						PrevCheatType = 0;
						if (((mem & 0x0FFFFFFF) & 0x3FFFFFFC) != 0)
							writeCheat();
					}
				}
			}
			break;

			default:
				if ((p->addr & 0xF0000000) == 0x00000000) // 0aaaaaaa 0000000vv
				{
					memWrite8(p->addr & 0x0FFFFFFF, (u8)p->data & 0x000000FF);
					PrevCheatType = 0;
				}
				else if ((p->addr & 0xF0000000) == 0x10000000) // 1aaaaaaa 0000vvvv
				{
					memWrite16(p->addr & 0x0FFFFFFF, (u16)p->data & 0x0000FFFF);
					PrevCheatType = 0;
				}
				else if ((p->addr & 0xF0000000) == 0x20000000) // 2aaaaaaa vvvvvvvv
				{
					memWrite32(p->addr & 0x0FFFFFFF, (u32)p->data);
					PrevCheatType = 0;
				}
				else if ((p->addr & 0xFFFF0000) == 0x30000000) // 300000vv 0aaaaaaa Inc
				{
					u8 mem = memRead8((u32)p->data);
					memWrite8((u32)p->data, mem + (p->addr & 0x000000FF));
					PrevCheatType = 0;
				}
				else if ((p->addr & 0xFFFF0000) == 0x30100000) // 301000vv 0aaaaaaa Dec
				{
					u8 mem = memRead8((u32)p->data);
					memWrite8((u32)p->data, mem - (p->addr & 0x000000FF));
					PrevCheatType = 0;
				}
				else if ((p->addr & 0xFFFF0000) == 0x30200000) // 3020vvvv 0aaaaaaa Inc
				{
					u16 mem = memRead16((u32)p->data);
					memWrite16((u32)p->data, mem + (p->addr & 0x0000FFFF));
					PrevCheatType = 0;
				}
				else if ((p->addr & 0xFFFF0000) == 0x30300000) // 3030vvvv 0aaaaaaa Dec
				{
					u16 mem = memRead16((u32)p->data);
					memWrite16((u32)p->data, mem - (p->addr & 0x0000FFFF));
					PrevCheatType = 0;
				}
				else if ((p->addr & 0xFFFF0000) == 0x30400000) // 30400000 0aaaaaaa Inc + Another line
				{
					PrevCheatType = 0x3040;
					PrevCheatAddr = (u32)p->data;
				}
				else if ((p->addr & 0xFFFF0000) == 0x30500000) // 30500000 0aaaaaaa Inc + Another line
				{
					PrevCheatType = 0x3050;
					PrevCheatAddr = (u32)p->data;
				}
				else if ((p->addr & 0xF0000000) == 0x40000000) // 4aaaaaaa nnnnssss + Another line
				{
					IterationCount = ((u32)p->data & 0xFFFF0000) >> 16;
					IterationIncrement = ((u32)p->data & 0x0000FFFF) * 4;
					PrevCheatAddr = (u32)p->addr & 0x0FFFFFFF;
					PrevCheatType = 0x4000;
				}
				else if ((p->addr & 0xF0000000) == 0x50000000) // 5sssssss nnnnnnnn + Another line
				{
					PrevCheatAddr = (u32)p->addr & 0x0FFFFFFF;
					IterationCount = ((u32)p->data);
					PrevCheatType = 0x5000;
				}
				else if ((p->addr & 0xF0000000) == 0x60000000) // 6aaaaaaa 000000vv + Another line/s
				{
					PrevCheatAddr = (u32)p->addr & 0x0FFFFFFF;
					IterationIncrement = ((u32)p->data);
					IterationCount = 0;
					PrevCheatType = 0x6000;
				}
				else if ((p->addr & 0xF0000000) == 0x70000000)
				{
					if ((p->data & 0x00F00000) == 0x00000000) // 7aaaaaaa 000000vv
					{
						u8 mem = memRead8((u32)p->addr & 0x0FFFFFFF);
						memWrite8((u32)p->addr & 0x0FFFFFFF, (u8)(mem | (p->data & 0x000000FF)));
					}
					else if ((p->data & 0x00F00000) == 0x00100000) // 7aaaaaaa 0010vvvv
					{
						u16 mem = memRead16((u32)p->addr & 0x0FFFFFFF);
						memWrite16((u32)p->addr & 0x0FFFFFFF, (u16)(mem | (p->data & 0x0000FFFF)));
					}
					else if ((p->data & 0x00F00000) == 0x00200000) // 7aaaaaaa 002000vv
					{
						u8 mem = memRead8((u32)p->addr & 0x0FFFFFFF);
						memWrite8((u32)p->addr & 0x0FFFFFFF, (u8)(mem & (p->data & 0x000000FF)));
					}
					else if ((p->data & 0x00F00000) == 0x00300000) // 7aaaaaaa 0030vvvv
					{
						u16 mem = memRead16((u32)p->addr & 0x0FFFFFFF);
						memWrite16((u32)p->addr & 0x0FFFFFFF, (u16)(mem & (p->data & 0x0000FFFF)));
					}
					else if ((p->data & 0x00F00000) == 0x00400000) // 7aaaaaaa 004000vv
					{
						u8 mem = memRead8((u32)p->addr & 0x0FFFFFFF);
						memWrite8((u32)p->addr & 0x0FFFFFFF, (u8)(mem ^ (p->data & 0x000000FF)));
					}
					else if ((p->data & 0x00F00000) == 0x00500000) // 7aaaaaaa 0050vvvv
					{
						u16 mem = memRead16((u32)p->addr & 0x0FFFFFFF);
						memWrite16((u32)p->addr & 0x0FFFFFFF, (u16)(mem ^ (p->data & 0x0000FFFF)));
					}
				}
				else if ((p->addr & 0xF0000000) == 0xD0000000 || (p->addr & 0xF0000000) == 0xE0000000)
				{
					u32 addr = (u32)p->addr;
					u32 data = (u32)p->data;

					// Since D-codes now have the additional functionality present in PS2rd which
					// incorporates E-code-like functionality by making use of the unused bits in
					// D-codes, the E-codes are now just converted to D-codes to reduce bloat.

					if ((addr & 0xF0000000) == 0xE0000000)
					{
						// Ezyyvvvv taaaaaaa  ->  Daaaaaaa yytzvvvv
						addr = 0xD0000000 | ((u32)p->data & 0x0FFFFFFF);
						data = 0x00000000 | ((u32)p->addr & 0x0000FFFF);
						data = data | ((u32)p->addr & 0x00FF0000) << 8;
						data = data | ((u32)p->addr & 0x0F000000) >> 8;
						data = data | ((u32)p->data & 0xF0000000) >> 8;
					}

					const u8 type = (data & 0x000F0000) >> 16;
					const u8 cond = (data & 0x00F00000) >> 20;

					if (cond == 0) // Daaaaaaa yy0zvvvv
					{
						if (type == 0) // Daaaaaaa yy00vvvv
						{
							u16 mem = memRead16(addr & 0x0FFFFFFF);
							if (mem != (data & 0x0000FFFF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
						else if (type == 1) // Daaaaaaa yy0100vv
						{
							u8 mem = memRead8(addr & 0x0FFFFFFF);
							if (mem != (data & 0x000000FF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
					}
					else if (cond == 1) // Daaaaaaa yy1zvvvv
					{
						if (type == 0) // Daaaaaaa yy10vvvv
						{
							u16 mem = memRead16(addr & 0x0FFFFFFF);
							if (mem == (data & 0x0000FFFF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
						else if (type == 1) // Daaaaaaa yy1100vv
						{
							u8 mem = memRead8(addr & 0x0FFFFFFF);
							if (mem == (data & 0x000000FF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
					}
					else if (cond == 2) // Daaaaaaa yy2zvvvv
					{
						if (type == 0) // Daaaaaaa yy20vvvv
						{
							u16 mem = memRead16(addr & 0x0FFFFFFF);
							if (mem >= (data & 0x0000FFFF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
						else if (type == 1) // Daaaaaaa yy2100vv
						{
							u8 mem = memRead8(addr & 0x0FFFFFFF);
							if (mem >= (data & 0x000000FF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
					}
					else if (cond == 3) // Daaaaaaa yy3zvvvv
					{
						if (type == 0) // Daaaaaaa yy30vvvv
						{
							u16 mem = memRead16(addr & 0x0FFFFFFF);
							if (mem <= (data & 0x0000FFFF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
						else if (type == 1) // Daaaaaaa yy3100vv
						{
							u8 mem = memRead8(addr & 0x0FFFFFFF);
							if (mem <= (data & 0x000000FF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
					}
					else if (cond == 4) // Daaaaaaa yy4zvvvv
					{
						if (type == 0) // Daaaaaaa yy40vvvv
						{
							u16 mem = memRead16(addr & 0x0FFFFFFF);
							if (mem & (data & 0x0000FFFF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
						else if (type == 1) // Daaaaaaa yy4100vv
						{
							u8 mem = memRead8(addr & 0x0FFFFFFF);
							if (mem & (data & 0x000000FF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
					}
					else if (cond == 5) // Daaaaaaa yy5zvvvv
					{
						if (type == 0) // Daaaaaaa yy50vvvv
						{
							u16 mem = memRead16(addr & 0x0FFFFFFF);
							if (!(mem & (data & 0x0000FFFF)))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
						else if (type == 1) // Daaaaaaa yy5100vv
						{
							u8 mem = memRead8(addr & 0x0FFFFFFF);
							if (!(mem & (data & 0x000000FF)))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
					}
					else if (cond == 6) // Daaaaaaa yy6zvvvv
					{
						if (type == 0) // Daaaaaaa yy60vvvv
						{
							u16 mem = memRead16(addr & 0x0FFFFFFF);
							if (mem | (data & 0x0000FFFF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
						else if (type == 1) // Daaaaaaa yy6100vv
						{
							u8 mem = memRead8(addr & 0x0FFFFFFF);
							if (mem | (data & 0x000000FF))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
					}
					else if (cond == 7) // Daaaaaaa yy7zvvvv
					{
						if (type == 0) // Daaaaaaa yy70vvvv
						{
							u16 mem = memRead16(addr & 0x0FFFFFFF);
							if (!(mem | (data & 0x0000FFFF)))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
						else if (type == 1) // Daaaaaaa yy7100vv
						{
							u8 mem = memRead8(addr & 0x0FFFFFFF);
							if (!(mem | (data & 0x000000FF)))
							{
								SkipCount = (data & 0xFF000000) >> 24;
								if (!SkipCount)
								{
									SkipCount = 1;
								}
							}
							PrevCheatType = 0;
						}
					}
				}
		}
}

void Patch::ApplyPatch(const PatchCommand* p)
{
	u64 ledata = 0;

	switch (p->cpu)
	{
		case CPU_EE:
			switch (p->type)
			{
				case BYTE_T:
					if (memRead8(p->addr) != (u8)p->data)
						memWrite8(p->addr, (u8)p->data);
					break;

				case SHORT_T:
					if (memRead16(p->addr) != (u16)p->data)
						memWrite16(p->addr, (u16)p->data);
					break;

				case WORD_T:
					if (memRead32(p->addr) != (u32)p->data)
						memWrite32(p->addr, (u32)p->data);
					break;

				case DOUBLE_T:
					if (memRead64(p->addr) != (u64)p->data)
						memWrite64(p->addr, (u64)p->data);
					break;

				case EXTENDED_T:
					handle_extended_t(p);
					break;

				case SHORT_BE_T:
					ledata = ByteSwap(static_cast<u16>(p->data));
					if (memRead16(p->addr) != (u16)ledata)
						memWrite16(p->addr, (u16)ledata);
					break;

				case WORD_BE_T:
					ledata = ByteSwap(static_cast<u32>(p->data));
					if (memRead32(p->addr) != (u32)ledata)
						memWrite32(p->addr, (u32)ledata);
					break;

				case DOUBLE_BE_T:
					ledata = ByteSwap(p->data);
					if (memRead64(p->addr) != (u64)ledata)
						memWrite64(p->addr, (u64)ledata);
					break;

				default:
					break;
			}
			break;

		case CPU_IOP:
			switch (p->type)
			{
				case BYTE_T:
					if (iopMemRead8(p->addr) != (u8)p->data)
						iopMemWrite8(p->addr, (u8)p->data);
					break;
				case SHORT_T:
					if (iopMemRead16(p->addr) != (u16)p->data)
						iopMemWrite16(p->addr, (u16)p->data);
					break;
				case WORD_T:
					if (iopMemRead32(p->addr) != (u32)p->data)
						iopMemWrite32(p->addr, (u32)p->data);
					break;
				default:
					break;
			}
			break;

		default:
			break;
	}
}

void Patch::ApplyDynaPatch(const DynamicPatch& patch, u32 address)
{
	for (const auto& pattern : patch.pattern)
	{
		if (*static_cast<u32*>(PSM(address + pattern.offset)) != pattern.value)
			return;
	}

	Console.WriteLn("Applying Dynamic Patch to address 0x%08X", address);
	// If everything passes, apply the patch.
	for (const auto& replacement : patch.replacement)
	{
		memWrite32(address + replacement.offset, replacement.value);
	}
}
