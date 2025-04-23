// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#define _PC_ // disables MIPS opcode macros.

#include "common/Assertions.h"
#include "common/ByteSwap.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SmallString.h"
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

#include <algorithm>
#include <cstring>
#include <memory>
#include <span>
#include <sstream>
#include <vector>

namespace Patch
{
	enum patch_cpu_type : u8
	{
		CPU_EE,
		CPU_IOP
	};

	enum patch_data_type : u8
	{
		BYTE_T,
		SHORT_T,
		WORD_T,
		DOUBLE_T,
		EXTENDED_T,
		SHORT_BE_T,
		WORD_BE_T,
		DOUBLE_BE_T,
		BYTES_T
	};

	static constexpr std::array<const char*, 3> s_place_to_string = {{"0", "1", "2"}};
	static constexpr std::array<const char*, 2> s_cpu_to_string = {{"EE", "IOP"}};
	static constexpr std::array<const char*, 9> s_type_to_string = {
		{"byte", "short", "word", "double", "extended", "beshort", "beword", "bedouble", "bytes"}};

	template <typename EnumType, class ArrayType>
	static inline std::optional<EnumType> LookupEnumName(const std::string_view val, const ArrayType& arr)
	{
		for (size_t i = 0; i < arr.size(); i++)
		{
			if (val == arr[i])
				return static_cast<EnumType>(i);
		}
		return std::nullopt;
	}

	struct PatchCommand
	{
		patch_place_type placetopatch;
		patch_cpu_type cpu;
		patch_data_type type;
		u32 addr;
		u64 data;
		u8* data_ptr;

		// needed because of the pointer
		PatchCommand() { std::memset(this, 0, sizeof(*this)); }
		PatchCommand(const PatchCommand& p) = delete;
		PatchCommand(PatchCommand&& p)
		{
			std::memcpy(this, &p, sizeof(*this));
			p.data_ptr = nullptr;
		}
		~PatchCommand()
		{
			if (data_ptr)
				std::free(data_ptr);
		}

		PatchCommand& operator=(const PatchCommand& p) = delete;
		PatchCommand& operator=(PatchCommand&& p)
		{
			std::memcpy(this, &p, sizeof(*this));
			p.data_ptr = nullptr;
			return *this;
		}

		bool operator==(const PatchCommand& p) const { return std::memcmp(this, &p, sizeof(*this)) == 0; }
		bool operator!=(const PatchCommand& p) const { return std::memcmp(this, &p, sizeof(*this)) != 0; }

		SmallString ToString() const
		{
			return SmallString::from_format("{},{},{},{:08x},{:x}", s_place_to_string[static_cast<u8>(placetopatch)],
				s_cpu_to_string[static_cast<u8>(cpu)], s_type_to_string[static_cast<u8>(type)], addr, data);
		}
	};
	static_assert(sizeof(PatchCommand) == 24, "IniPatch has no padding");

	struct PatchGroup
	{
		std::string name;
		std::optional<float> override_aspect_ratio;
		std::optional<GSInterlaceMode> override_interlace_mode;
		std::vector<PatchCommand> patches;
		std::vector<DynamicPatch> dpatches;
	};

	struct PatchTextTable
	{
		int code;
		const char* text;
		void (*func)(PatchGroup* group, const std::string_view cmd, const std::string_view param);
	};

	using PatchList = std::vector<PatchGroup>;
	using ActivePatchList = std::vector<const PatchCommand*>;
	using EnablePatchList = std::vector<std::string>;

	namespace PatchFunc
	{
		static void patch(PatchGroup* group, const std::string_view cmd, const std::string_view param);
		static void gsaspectratio(PatchGroup* group, const std::string_view cmd, const std::string_view param);
		static void gsinterlacemode(PatchGroup* group, const std::string_view cmd, const std::string_view param);
		static void dpatch(PatchGroup* group, const std::string_view cmd, const std::string_view param);
	} // namespace PatchFunc

	static void TrimPatchLine(std::string& buffer);
	static int PatchTableExecute(PatchGroup* group, const std::string_view lhs, const std::string_view rhs,
		const std::span<const PatchTextTable>& Table);
	static void LoadPatchLine(PatchGroup* group, const std::string_view line);
	static u32 LoadPatchesFromString(PatchList* patch_list, const std::string& patch_file);
	static bool OpenPatchesZip();
	static std::string GetPnachTemplate(
		const std::string_view serial, u32 crc, bool include_serial, bool add_wildcard, bool all_crcs);
	static std::vector<std::string> FindPatchFilesOnDisk(
		const std::string_view serial, u32 crc, bool cheats, bool all_crcs);

	static bool ContainsPatchName(const PatchInfoList& patches, const std::string_view patchName);
	static bool ContainsPatchName(const PatchList& patches, const std::string_view patchName);

	template <typename F>
	static void EnumeratePnachFiles(const std::string_view serial, u32 crc, bool cheats, bool for_ui, const F& f);

	static bool PatchStringHasUnlabelledPatch(const std::string& pnach_data);
	static void ExtractPatchInfo(PatchInfoList* dst, const std::string& pnach_data, u32* num_unlabelled_patches);
	static void ReloadEnabledLists();
	static u32 EnablePatches(const PatchList& patches, const EnablePatchList& enable_list, const EnablePatchList& enable_immediately_list);

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
	const char* PATCH_DISABLE_CONFIG_KEY = "Disable";

	static zip_t* s_patches_zip;
	static PatchList s_gamedb_patches;
	static PatchList s_game_patches;
	static PatchList s_cheat_patches;

	static ActivePatchList s_active_patches;
	static std::vector<DynamicPatch> s_active_gamedb_dynamic_patches;
	static std::vector<DynamicPatch> s_active_pnach_dynamic_patches;
	static EnablePatchList s_enabled_cheats;
	static EnablePatchList s_enabled_patches;
	static EnablePatchList s_just_enabled_cheats;
	static EnablePatchList s_just_enabled_patches;
	static u32 s_patches_crc;
	static std::optional<float> s_override_aspect_ratio;
	static std::optional<GSInterlaceMode> s_override_interlace_mode;

	static const PatchTextTable s_patch_commands[] = {
		{0, "patch", &Patch::PatchFunc::patch},
		{0, "gsaspectratio", &Patch::PatchFunc::gsaspectratio},
		{0, "gsinterlacemode", &Patch::PatchFunc::gsinterlacemode},
		{0, "dpatch", &Patch::PatchFunc::dpatch},
		{0, nullptr, nullptr},
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

bool Patch::ContainsPatchName(const PatchList& patch_list, const std::string_view patch_name)
{
	return std::find_if(patch_list.begin(), patch_list.end(), [&patch_name](const PatchGroup& patch) {
		return patch.name == patch_name;
	}) != patch_list.end();
}

int Patch::PatchTableExecute(PatchGroup* group, const std::string_view lhs, const std::string_view rhs,
	const std::span<const PatchTextTable>& Table)
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
void Patch::LoadPatchLine(PatchGroup* group, const std::string_view line)
{
	std::string_view key, value;
	StringUtil::ParseAssignmentString(line, &key, &value);

	PatchTableExecute(group, key, value, s_patch_commands);
}

u32 Patch::LoadPatchesFromString(PatchList* patch_list, const std::string& patch_file)
{
	const size_t before = patch_list->size();

	PatchGroup current_patch_group;
	const auto add_current_patch = [patch_list, &current_patch_group]() {
		if (!current_patch_group.patches.empty())
		{
			// Ungrouped/legacy patches should merge with other ungrouped patches.
			if (current_patch_group.name.empty())
			{
				const PatchList::iterator ungrouped_patch = std::find_if(patch_list->begin(), patch_list->end(),
					[](const PatchGroup& pg) { return pg.name.empty(); });
				if (ungrouped_patch != patch_list->end())
				{
					Console.WriteLn(Color_Gray, fmt::format(
						"Patch: Merging {} new patch commands into ungrouped list.", current_patch_group.patches.size()));

					ungrouped_patch->patches.reserve(ungrouped_patch->patches.size() + current_patch_group.patches.size());
					for (PatchCommand& cmd : current_patch_group.patches)
						ungrouped_patch->patches.push_back(std::move(cmd));
				}
				else
				{
					// Always add ungrouped patches, no sense to compare empty names.
					patch_list->push_back(std::move(current_patch_group));
				}

				return;
			}
		}

		if (current_patch_group.patches.empty() && current_patch_group.dpatches.empty())
			return;

		// Don't show patches with duplicate names, prefer the first loaded.
		if (!ContainsPatchName(*patch_list, current_patch_group.name))
		{
			patch_list->push_back(std::move(current_patch_group));
		}
		else
		{
			Console.WriteLn(Color_Gray, fmt::format(
				"Patch: Skipped loading patch '{}' since a patch with a duplicate name was already loaded.",
				current_patch_group.name));
		}
	};

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

			if (!current_patch_group.name.empty() || !current_patch_group.patches.empty() || !current_patch_group.dpatches.empty())
			{
				add_current_patch();
				current_patch_group = {};
			}

			current_patch_group.name = line.substr(1, line.length() - 2);
			if (current_patch_group.name.empty())
				Console.Error(fmt::format("Malformed patch name: {}", line));

			continue;
		}

		LoadPatchLine(&current_patch_group, line);
	}

	if (!current_patch_group.name.empty() || !current_patch_group.patches.empty() || !current_patch_group.dpatches.empty())
		add_current_patch();

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
			Host::AddIconOSDMessage("PatchesZipOpenWarning", ICON_FA_BAND_AID,
				fmt::format(TRANSLATE_FS("Patch", "Failed to open {}. Built-in game patches are not available."),
					PATCHES_ZIP_NAME),
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

std::string Patch::GetPnachTemplate(const std::string_view serial, u32 crc, bool include_serial, bool add_wildcard, bool all_crcs)
{
	pxAssert(!all_crcs || (include_serial && add_wildcard));
	if (!serial.empty())
	{
		if (all_crcs)
			return fmt::format("{}_*.pnach", serial);	
		else if (include_serial)
			return fmt::format("{}_{:08X}{}.pnach", serial, crc, add_wildcard ? "*" : "");
	}
	return fmt::format("{:08X}{}.pnach", crc, add_wildcard ? "*" : "");
}

std::vector<std::string> Patch::FindPatchFilesOnDisk(const std::string_view serial, u32 crc, bool cheats, bool all_crcs)
{
	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(cheats ? EmuFolders::Cheats.c_str() : EmuFolders::Patches.c_str(),
		GetPnachTemplate(serial, crc, true, true, all_crcs).c_str(),
		FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES, &files);

	std::vector<std::string> ret;
	ret.reserve(files.size());

	for (FILESYSTEM_FIND_DATA& fd : files)
		ret.push_back(std::move(fd.FileName));

	// and patches without serials
	FileSystem::FindFiles(cheats ? EmuFolders::Cheats.c_str() : EmuFolders::Patches.c_str(),
		GetPnachTemplate(serial, crc, false, true, false).c_str(), FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES,
		&files);
	ret.reserve(ret.size() + files.size());
	for (FILESYSTEM_FIND_DATA& fd : files)
		ret.push_back(std::move(fd.FileName));

	return ret;
}

bool Patch::ContainsPatchName(const PatchInfoList& patches, const std::string_view patchName)
{
	return std::find_if(patches.begin(), patches.end(), [&patchName](const PatchInfo& patch) {
		return patch.name == patchName;
	}) != patches.end();
}

template <typename F>
void Patch::EnumeratePnachFiles(const std::string_view serial, u32 crc, bool cheats, bool for_ui, const F& f)
{
	// Prefer files on disk over the zip.
	std::vector<std::string> disk_patch_files;
	if (for_ui || !Achievements::IsHardcoreModeActive())
		disk_patch_files = FindPatchFilesOnDisk(serial, crc, cheats, for_ui);

	bool unlabeled_patch_found = false;
	if (!disk_patch_files.empty())
	{
		for (const std::string& file : disk_patch_files)
		{
			std::optional<std::string> contents = FileSystem::ReadFileToString(file.c_str());
			if (contents.has_value())
			{
				// Catch if unlabeled patches are being loaded so we can disable ZIP patches to prevent conflicts.
				if (PatchStringHasUnlabelledPatch(contents.value()))
				{
					unlabeled_patch_found = true;
					Console.WriteLn(fmt::format("Patch: Disabling any bundled '{}' patches due to unlabeled patch being loaded. (To avoid conflicts)", PATCHES_ZIP_NAME));
				}

				f(std::move(file), std::move(contents.value()));
			}
		}
	}

	// Otherwise fall back to the zip.
	if (cheats || unlabeled_patch_found || !OpenPatchesZip())
		return;

	// Prefer filename with serial.
	std::string zip_filename = GetPnachTemplate(serial, crc, true, false, false);
	std::optional<std::string> pnach_data(ReadFileInZipToString(s_patches_zip, zip_filename.c_str()));
	if (!pnach_data.has_value())
	{
		zip_filename = GetPnachTemplate(serial, crc, false, false, false);
		pnach_data = ReadFileInZipToString(s_patches_zip, zip_filename.c_str());
	}
	if (pnach_data.has_value())
		f(std::move(zip_filename), std::move(pnach_data.value()));
}

bool Patch::PatchStringHasUnlabelledPatch(const std::string& pnach_data)
{
	std::istringstream ss(pnach_data);
	std::string line;
	bool foundPatch = false, foundLabel = false;

	while (std::getline(ss, line))
	{
		TrimPatchLine(line);
		if (line.empty())
			continue;

		if (line.length() > 2 && line.front() == '[' && line.back() == ']')
		{
			if (!foundPatch)
				return false;
			foundLabel = true;
			continue;
		}

		std::string_view key, value;
		StringUtil::ParseAssignmentString(line, &key, &value);
		if (key == "patch")
		{
			if (!foundLabel)
				return true;

			foundPatch = true;
		}
	}
	return false;
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
				if (std::none_of(dst->begin(), dst->end(),
						[&current_patch](const PatchInfo& pi) { return (pi.name == current_patch.name); }))
				{
					// Don't show patches with duplicate names, prefer the first loaded.
					if (!ContainsPatchName(*dst, current_patch.name))
					{
						dst->push_back(std::move(current_patch));
					}
					else
					{
						Console.WriteLn(Color_Gray, fmt::format("Patch: Skipped reading patch '{}' since a patch with a duplicate name was already loaded.", current_patch.name));
					}
				}
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
	if (!current_patch.name.empty() && std::none_of(dst->begin(), dst->end(), [&current_patch](const PatchInfo& pi) {
			return (pi.name == current_patch.name);
		}))
	{
		dst->push_back(std::move(current_patch));
	}
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

Patch::PatchInfoList Patch::GetPatchInfo(const std::string_view serial, u32 crc, bool cheats, bool showAllCRCS, u32* num_unlabelled_patches)
{
	PatchInfoList ret;

	if (num_unlabelled_patches)
		*num_unlabelled_patches = 0;

	EnumeratePnachFiles(serial, crc, cheats, showAllCRCS,
		[&ret, num_unlabelled_patches](const std::string& filename, const std::string& pnach_data) {
			ExtractPatchInfo(&ret, pnach_data, num_unlabelled_patches);
		});

	return ret;
}

std::string Patch::GetPnachFilename(const std::string_view serial, u32 crc, bool cheats)
{
	return Path::Combine(cheats ? EmuFolders::Cheats : EmuFolders::Patches, GetPnachTemplate(serial, crc, true, false, false));
}

void Patch::ReloadEnabledLists()
{
	const EnablePatchList prev_enabled_cheats = std::move(s_enabled_cheats);
	if (EmuConfig.EnableCheats && !Achievements::IsHardcoreModeActive())
		s_enabled_cheats = Host::GetStringListSetting(CHEATS_CONFIG_SECTION, PATCH_ENABLE_CONFIG_KEY);
	else
		s_enabled_cheats = {};

	const EnablePatchList prev_enabled_patches = std::exchange(s_enabled_patches, Host::GetStringListSetting(PATCHES_CONFIG_SECTION, PATCH_ENABLE_CONFIG_KEY));
	const EnablePatchList disabled_patches = Host::GetStringListSetting(PATCHES_CONFIG_SECTION, PATCH_DISABLE_CONFIG_KEY);

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

	for (auto it = s_enabled_patches.begin(); it != s_enabled_patches.end();)
	{
		if (std::find(disabled_patches.begin(), disabled_patches.end(), *it) != disabled_patches.end())
		{
			it = s_enabled_patches.erase(it);
		}
		else
		{
			++it;
		}
	}

	s_just_enabled_cheats.clear();
	s_just_enabled_patches.clear();
	for (const auto& p : s_enabled_cheats)
	{
		if (std::find(prev_enabled_cheats.begin(), prev_enabled_cheats.end(), p) == prev_enabled_cheats.end())
		{
			s_just_enabled_cheats.emplace_back(p);
		}
	}
	for (const auto& p : s_enabled_patches)
	{
		if (std::find(prev_enabled_patches.begin(), prev_enabled_patches.end(), p) == prev_enabled_patches.end())
		{
			s_just_enabled_patches.emplace_back(p);
		}
	}
}

u32 Patch::EnablePatches(const PatchList& patches, const EnablePatchList& enable_list, const EnablePatchList& enable_immediately_list)
{
	ActivePatchList patches_to_apply_immediately;

	u32 count = 0;
	for (const PatchGroup& p : patches)
	{
		// For compatibility, we auto enable anything that's not labelled.
		// Also for gamedb patches.
		if (!p.name.empty() && std::find(enable_list.begin(), enable_list.end(), p.name) == enable_list.end())
			continue;

		Console.WriteLn(Color_Green, fmt::format("Enabled patch: {}",
										 p.name.empty() ? std::string_view("<unknown>") : std::string_view(p.name)));

		const bool apply_immediately = std::find(enable_immediately_list.begin(), enable_immediately_list.end(), p.name) != enable_immediately_list.end();
		for (const PatchCommand& ip : p.patches)
		{
			// print the actual patch lines only in verbose mode (even in devel)
			if (Log::GetMaxLevel() >= LOGLEVEL_DEV)
				DevCon.WriteLnFmt("  {}", ip.ToString());

			s_active_patches.push_back(&ip);
			if (apply_immediately && ip.placetopatch == PPT_ONCE_ON_LOAD)
				patches_to_apply_immediately.push_back(&ip);
		}

		for (const DynamicPatch& dp : p.dpatches)
		{
			s_active_pnach_dynamic_patches.push_back(dp);
		}

		if (p.override_aspect_ratio.has_value())
			s_override_aspect_ratio = p.override_aspect_ratio;
		if (p.override_interlace_mode.has_value())
			s_override_interlace_mode = p.override_interlace_mode;

		// Count unlabelled patches once per command, or one patch per group.
		count += p.name.empty() ? (static_cast<u32>(p.patches.size()) + static_cast<u32>(p.dpatches.size())) : 1;
	}

	if (!patches_to_apply_immediately.empty())
	{
		Host::RunOnCPUThread([patches = std::move(patches_to_apply_immediately)]() {
			for (const PatchCommand* i : patches)
			{
				ApplyPatch(i);
			}
		});
	}

	return count;
}

void Patch::ReloadPatches(const std::string& serial, u32 crc, bool reload_files, bool reload_enabled_list, bool verbose,
	bool verbose_if_changed)
{
	reload_files |= (s_patches_crc != crc);
	s_patches_crc = crc;

	if (reload_files)
	{
		s_gamedb_patches.clear();

		const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(serial);
		if (game)
		{
			const std::string* patches = game->findPatch(crc);
			if (patches)
			{
				const u32 patch_count = LoadPatchesFromString(&s_gamedb_patches, *patches);
				if (patch_count > 0)
					Console.WriteLn(Color_Green, fmt::format("Found {} game patches in GameDB.", patch_count));
			}

			LoadDynamicPatches(game->dynaPatches);
		}

		s_game_patches.clear();
		EnumeratePnachFiles(
			serial, s_patches_crc, false, false, [](const std::string& filename, const std::string& pnach_data) {
				const u32 patch_count = LoadPatchesFromString(&s_game_patches, pnach_data);
				if (patch_count > 0)
					Console.WriteLn(Color_Green, fmt::format("Found {} game patches in {}.", patch_count, filename));
			});

		s_cheat_patches.clear();
		EnumeratePnachFiles(
			serial, s_patches_crc, true, false, [](const std::string& filename, const std::string& pnach_data) {
				const u32 patch_count = LoadPatchesFromString(&s_cheat_patches, pnach_data);
				if (patch_count > 0)
					Console.WriteLn(Color_Green, fmt::format("Found {} cheats in {}.", patch_count, filename));
			});
	}

	UpdateActivePatches(reload_enabled_list, verbose, verbose_if_changed, false);
}

void Patch::UpdateActivePatches(bool reload_enabled_list, bool verbose, bool verbose_if_changed, bool apply_new_patches)
{
	if (reload_enabled_list)
		ReloadEnabledLists();

	const size_t prev_count = s_active_patches.size();
	s_active_patches.clear();
	s_override_aspect_ratio.reset();
	s_override_interlace_mode.reset();
	s_active_pnach_dynamic_patches.clear();

	SmallString message;
	u32 gp_count = 0;
	if (EmuConfig.EnablePatches)
	{
		gp_count = EnablePatches(s_gamedb_patches, EnablePatchList(), EnablePatchList());
		if (gp_count > 0)
			message.append(TRANSLATE_PLURAL_STR("Patch", "%n GameDB patches are active.", "OSD Message", gp_count));
	}

	const u32 p_count = EnablePatches(s_game_patches, s_enabled_patches, apply_new_patches ? s_just_enabled_patches : EnablePatchList());
	if (p_count > 0)
	{
		message.append_format("{}{}", message.empty() ? "" : "\n",
			TRANSLATE_PLURAL_STR("Patch", "%n game patches are active.", "OSD Message", p_count));
	}

	const u32 c_count = EmuConfig.EnableCheats ? EnablePatches(s_cheat_patches, s_enabled_cheats, apply_new_patches ? s_just_enabled_cheats : EnablePatchList()) : 0;
	if (c_count > 0)
	{
		message.append_format("{}{}", message.empty() ? "" : "\n",
			TRANSLATE_PLURAL_STR("Patch", "%n cheat patches are active.", "OSD Message", c_count));
	}

	// Display message on first boot when we load patches.
	// Except when it's just GameDB.
	const bool just_gamedb = (p_count == 0 && c_count == 0 && gp_count > 0);
	if (verbose || (verbose_if_changed && prev_count != s_active_patches.size() && !just_gamedb))
	{
		if (!message.empty())
		{
			Host::AddIconOSDMessage("LoadPatches", ICON_FA_BAND_AID, message, Host::OSD_INFO_DURATION);
		}
		else
		{
			Host::AddIconOSDMessage("LoadPatches", ICON_FA_BAND_AID,
				TRANSLATE_SV(
					"Patch", "No cheats or patches (widescreen, compatibility or others) are found / enabled."),
				Host::OSD_INFO_DURATION);
		}
	}
}

void Patch::ApplyPatchSettingOverrides()
{
	// Switch to 16:9 (or any custom aspect ratio) if widescreen patches are enabled, and AR is auto.
	if (s_override_aspect_ratio.has_value() && EmuConfig.GS.AspectRatio == AspectRatioType::RAuto4_3_3_2)
	{
		EmuConfig.CurrentCustomAspectRatio = s_override_aspect_ratio.value();

		Console.WriteLn(Color_Gray,
			fmt::format("Patch: Setting aspect ratio to {} by patch request.", s_override_aspect_ratio.value()));
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
	// Restore the aspect ratio + interlacing setting the user had set before reloading the patch,
	// as the custom patch settings only apply if the "Auto" settings are selected.

	const AspectRatioType current_ar = EmuConfig.GS.AspectRatio;
	const GSInterlaceMode current_interlace = EmuConfig.GS.InterlaceMode;
	const float custom_aspect_ratio = EmuConfig.CurrentCustomAspectRatio;
	
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
	EmuConfig.GS.AspectRatio = new_ar;
	EmuConfig.GS.InterlaceMode = static_cast<GSInterlaceMode>(Host::GetIntSettingValue(
		"EmuCore/GS", "deinterlace_mode", static_cast<int>(Pcsx2Config::GSOptions::DEFAULT_INTERLACE_MODE)));

	ApplyPatchSettingOverrides();

	// Return true if any config setting changed
	return current_ar != EmuConfig.GS.AspectRatio || custom_aspect_ratio != EmuConfig.CurrentCustomAspectRatio || current_interlace != EmuConfig.GS.InterlaceMode;
}

void Patch::UnloadPatches()
{
	s_override_interlace_mode = {};
	s_override_aspect_ratio = {};
	s_patches_crc = 0;
	s_active_patches = {};
	s_active_pnach_dynamic_patches = {};
	s_active_gamedb_dynamic_patches = {};
	s_enabled_patches = {};
	s_enabled_cheats = {};
	decltype(s_cheat_patches)().swap(s_cheat_patches);
	decltype(s_game_patches)().swap(s_game_patches);
	decltype(s_gamedb_patches)().swap(s_gamedb_patches);
}

// PatchFunc Functions.
void Patch::PatchFunc::patch(PatchGroup* group, const std::string_view cmd, const std::string_view param)
{
#define PATCH_ERROR(fstring, ...) \
	Console.Error(fmt::format("(Patch) Error Parsing: {}={}: " fstring, cmd, param, __VA_ARGS__))

	// [0]=PlaceToPatch,[1]=CpuType,[2]=MemAddr,[3]=OperandSize,[4]=WriteValue
	const std::vector<std::string_view> pieces(StringUtil::SplitString(param, ',', false));
	if (pieces.size() != 5)
	{
		PATCH_ERROR("Expected 5 data parameters; only found {}", pieces.size());
		return;
	}

	std::string_view addr_end, data_end;
	const std::optional<patch_place_type> placetopatch = LookupEnumName<patch_place_type>(pieces[0], s_place_to_string);
	const std::optional<patch_cpu_type> cpu = LookupEnumName<patch_cpu_type>(pieces[1], s_cpu_to_string);
	const std::optional<u32> addr = StringUtil::FromChars<u32>(pieces[2], 16, &addr_end);
	const std::optional<patch_data_type> type = LookupEnumName<patch_data_type>(pieces[3], s_type_to_string);
	std::optional<u64> data = StringUtil::FromChars<u64>(pieces[4], 16, &data_end);
	u8* data_ptr = nullptr;

	if (!placetopatch.has_value())
	{
		PATCH_ERROR("Invalid 'place' value '{}' (0 - once on startup, 1: continuously)", pieces[0]);
		return;
	}
	if (!addr.has_value() || !addr_end.empty())
	{
		PATCH_ERROR("Malformed address '{}', a hex number without prefix (e.g. 0123ABCD) is expected", pieces[2]);
		return;
	}
	if (!cpu.has_value())
	{
		PATCH_ERROR("Unrecognized CPU Target: '%.*s'", pieces[1]);
		return;
	}
	if (!type.has_value())
	{
		PATCH_ERROR("Unrecognized Operand Size: '%.*s'", pieces[3]);
		return;
	}
	if (type.value() != BYTES_T)
	{
		if (!data.has_value() || !data_end.empty())
		{
			PATCH_ERROR("Malformed data '{}', a hex number without prefix (e.g. 0123ABCD) is expected", pieces[4]);
			return;
		}
	}
	else
	{
		// bit crappy to copy it, but eh, saves writing a new routine
		std::optional<std::vector<u8>> bytes = StringUtil::DecodeHex(pieces[4]);
		if (!bytes.has_value() || bytes->empty())
		{
			PATCH_ERROR("Malformed data '{}', a hex string without prefix (e.g. 0123ABCD) is expected", pieces[4]);
			return;
		}

		data = bytes->size();
		data_ptr = static_cast<u8*>(std::malloc(bytes->size()));
		std::memcpy(data_ptr, bytes->data(), bytes->size());
	}

	PatchCommand iPatch;
	iPatch.placetopatch = placetopatch.value();
	iPatch.cpu = cpu.value();
	iPatch.addr = addr.value();
	iPatch.type = type.value();
	iPatch.data = data.value();
	iPatch.data_ptr = data_ptr;
	group->patches.push_back(std::move(iPatch));

#undef PATCH_ERROR
}

void Patch::PatchFunc::gsaspectratio(PatchGroup* group, const std::string_view cmd, const std::string_view param)
{
	std::string str(param);
	std::istringstream ss(str);
	uint dividend, divisor;
	char delimiter;
	float aspect_ratio = 0.f;

	ss >> dividend >> delimiter >> divisor;
	if (!ss.fail() && delimiter == ':' && divisor != 0)
	{
		aspect_ratio = static_cast<float>(dividend) / static_cast<float>(divisor);
	}

	if (aspect_ratio > 0.f)
	{
		group->override_aspect_ratio = aspect_ratio;
		return;
	}

	Console.Error(fmt::format("Patch error: {} is an unknown aspect ratio.", param));
}

void Patch::PatchFunc::gsinterlacemode(PatchGroup* group, const std::string_view cmd, const std::string_view param)
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

void Patch::PatchFunc::dpatch(PatchGroup* group, const std::string_view cmd, const std::string_view param)
{
#define PATCH_ERROR(fstring, ...) \
	Console.Error(fmt::format("(dPatch) Error Parsing: {}={}: " fstring, cmd, param, __VA_ARGS__))

	// [0]=version/type,[1]=number of patterns,[2]=number of replacements
	// Each pattern or replacement is [3]=offset,[4]=hex

	const std::vector<std::string_view> pieces(StringUtil::SplitString(param, ',', false));
	if (pieces.size() < 3)
	{
		PATCH_ERROR("Expected at least 3 data parameters; only found {}", pieces.size());
		return;
	}


	std::string_view patterns_end, replacements_end;

	// Implemented for possible future use so we don't have to break backcompat
	std::optional<u32> dpatch_type = StringUtil::FromChars<u32>(pieces[0]);

	std::optional<u32> num_patterns = StringUtil::FromChars<u32>(pieces[1], 16, &patterns_end);
	std::optional<u32> num_replacements = StringUtil::FromChars<u32>(pieces[2], 16, &replacements_end);

	if (!dpatch_type.has_value())
	{
		PATCH_ERROR("Malformed version/type '{}', a decimal number(e.g. 0,1,2) is expected", pieces[0]);
		return;
	}

	if (dpatch_type.value() != 0)
	{
		PATCH_ERROR("Unsupported version/type '{}', only 0 is currently supported", pieces[0]);
		return;
	}

	if (!num_patterns.has_value())
	{
		PATCH_ERROR("Malformed number of patterns '{}', a decimal number is expected", pieces[1]);
		return;
	}

	if (!num_replacements.has_value())
	{
		PATCH_ERROR("Malformed number of replacements '{}', a decimal number is expected", pieces[2]);
		return;
	}

	if (pieces.size() != ((num_patterns.value() * 2) + (num_replacements.value() * 2) + 3))
	{
		PATCH_ERROR("Expected 2 fields for each {} patterns and {} replacements; found {}", num_patterns.value(), num_replacements.value(), pieces.size() - 2);
		return;
	}

	DynamicPatch dpatch;
	for (u32 i = 0; i < num_patterns.value(); i++)
	{
		std::optional<u32> offset = StringUtil::FromChars<u32>(pieces[3 + (i * 2)], 16);
		std::optional<u32> value = StringUtil::FromChars<u32>(pieces[4 + (i * 2)], 16);
		if (!offset.has_value())
		{
			PATCH_ERROR("Malformed offset '{}', a hex number without prefix (e.g. 0123ABCD) is expected", pieces[3 + (i * 2)]);
			return;
		}
		if (!value.has_value())
		{
			PATCH_ERROR("Malformed value '{}', a hex number without prefix (e.g. 0123ABCD) is expected", pieces[4 + (i * 2)]);
			return;
		}

		DynamicPatchEntry pattern;
		pattern.offset = offset.value();
		pattern.value = value.value();

		dpatch.pattern.push_back(pattern);
	}

	for (u32 i = 0; i < num_replacements.value(); i++)
	{
		std::optional<u32> offset = StringUtil::FromChars<u32>(pieces[3 + (num_patterns.value() * 2) + (i * 2)], 16);
		std::optional<u32> value = StringUtil::FromChars<u32>(pieces[4 + (num_patterns.value() * 2) + (i * 2)], 16);
		if (!offset.has_value())
		{
			PATCH_ERROR("Malformed offset '{}', a hex number without prefix (e.g. 0123ABCD) is expected", pieces[3 + (num_patterns.value() * 2) + (i * 2)]);
			return;
		}
		if (!value.has_value())
		{
			PATCH_ERROR("Malformed value '{}', a hex number without prefix (e.g. 0123ABCD) is expected", pieces[4 + (num_patterns.value() * 2) + (i * 2)]);
			return;
		}

		DynamicPatchEntry replacement;
		replacement.offset = offset.value();
		replacement.value = value.value();

		dpatch.replacement.push_back(replacement);
	}

	group->dpatches.push_back(dpatch);
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

bool Patch::IsGloballyToggleablePatch(const PatchInfo& patch_info)
{
	return patch_info.name == WS_PATCH_NAME || patch_info.name == NI_PATCH_NAME;
}

void Patch::ApplyDynamicPatches(u32 pc)
{
	for (const auto& dynpatch : s_active_pnach_dynamic_patches)
		ApplyDynaPatch(dynpatch, pc);
	for (const auto& dynpatch : s_active_gamedb_dynamic_patches)
		ApplyDynaPatch(dynpatch, pc);
}

void Patch::LoadDynamicPatches(const std::vector<DynamicPatch>& patches)
{
	for (const DynamicPatch& it : patches)
		s_active_gamedb_dynamic_patches.push_back(it);
}

static u32 SkipCount = 0, IterationCount = 0;
static u32 IterationIncrement = 0;
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

				case BYTES_T:
				{
					// We compare before writing so the rec doesn't get upset and invalidate when there's no change.
					if (vtlb_memSafeCmpBytes(p->addr, p->data_ptr, static_cast<u32>(p->data)) != 0)
						vtlb_memSafeWriteBytes(p->addr, p->data_ptr, static_cast<u32>(p->data));
				}
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
				case BYTES_T:
				{
					if (iopMemSafeCmpBytes(p->addr, p->data_ptr, static_cast<u32>(p->data)) != 0)
						iopMemSafeWriteBytes(p->addr, p->data_ptr, static_cast<u32>(p->data));
				}
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
