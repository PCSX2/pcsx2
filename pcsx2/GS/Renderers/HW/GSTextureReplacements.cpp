// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/AlignedMalloc.h"
#include "common/Console.h"
#include "common/HashCombine.h"
#include "common/FileSystem.h"
#include "common/HostSys.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/ScopedGuard.h"
#include "common/TextureDecompress.h"

#include "Config.h"
#include "Host.h"
#include "IconsFontAwesome.h"
#include "GS/GSExtra.h"
#include "GS/GSLocalMemory.h"
#include "GS/Renderers/HW/GSTextureReplacements.h"
#include "VMManager.h"

#include <cinttypes>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <list>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <thread>

// this is a #define instead of a variable to avoid warnings from non-literal format strings
#define TEXTURE_FILENAME_FORMAT_STRING "%" PRIx64 "-%08x"
#define TEXTURE_FILENAME_CLUT_FORMAT_STRING "%" PRIx64 "-%" PRIx64 "-%08x"
#define TEXTURE_FILENAME_REGION_FORMAT_STRING "%" PRIx64 "-r%ux%u-%08x"
#define TEXTURE_FILENAME_REGION_CLUT_FORMAT_STRING "%" PRIx64 "-%" PRIx64 "-r%ux%u-%08x"
#define TEXTURE_FILENAME_OLD_REGION_FORMAT_STRING "%" PRIx64 "-r%" PRIx64 "-%08x"
#define TEXTURE_FILENAME_OLD_REGION_CLUT_FORMAT_STRING "%" PRIx64 "-%" PRIx64 "-r%" PRIx64 "-%08x"
#define TEXTURE_REPLACEMENT_SUBDIRECTORY_NAME "replacements"
#define TEXTURE_DUMP_SUBDIRECTORY_NAME "dumps"

namespace
{
	struct TextureName // 32 bytes
	{
		u64 TEX0Hash;
		u64 CLUTHash;
		u32 region_width;
		u32 region_height;

		union
		{
			struct
			{
				u32 TEX0_PSM : 6;
				u32 TEX0_TW : 4;
				u32 TEX0_TH : 4;
				u32 unused0 : 1; // was TCC
				u32 TEXA_TA0 : 8;
				u32 TEXA_AEM : 1;
				u32 TEXA_TA1 : 8;
			};
			u32 bits;
		};
		u32 miplevel;

		__fi u32 Width() const { return (region_width ? region_width : (1u << TEX0_TW)); }
		__fi u32 Height() const { return (region_height ? region_height : (1u << TEX0_TH)); }
		__fi bool HasPalette() const { return (GSLocalMemory::m_psm[TEX0_PSM].pal > 0); }
		__fi bool HasRegion() const { return (region_width != 0 || region_height != 0); }

		__fi bool operator==(const TextureName& rhs) const { return BitEqual(*this, rhs); }
		__fi bool operator!=(const TextureName& rhs) const { return !BitEqual(*this, rhs); }
		__fi bool operator<(const TextureName& rhs) const { return (std::memcmp(this, &rhs, sizeof(*this)) < 0); }

		__fi void RemoveUnusedBits()
		{
			// Remove bits which were previously present, but no longer used.
			unused0 = 0;
		}
	};
	static_assert(sizeof(TextureName) == 32, "ReplacementTextureName is expected size");
} // namespace

namespace std
{
	template <>
	struct hash<TextureName>
	{
		std::size_t operator()(const TextureName& val) const
		{
			std::size_t h = 0;
			HashCombine(h, val.TEX0Hash, val.CLUTHash,
				static_cast<u64>(val.region_width) | (static_cast<u64>(val.region_height) << 32),
				static_cast<u64>(val.bits) | (static_cast<u64>(val.miplevel) << 32));
			return h;
		}
	};
} // namespace std

namespace GSTextureReplacements
{
	static TextureName CreateTextureName(const GSTextureCache::HashCacheKey& hash, u32 miplevel);
	static GSTextureCache::HashCacheKey HashCacheKeyFromTextureName(const TextureName& tn);
	static std::optional<TextureName> ParseReplacementName(const std::string& filename);
	static std::string GetGameTextureDirectory();
	static std::string GetDumpFilename(const TextureName& name, u32 level);
	template <GSTexture::Format format>
	std::pair<u8, u8> GetBCAlphaMinMax(ReplacementTexture& rtex);
	static void SetReplacementTextureAlphaMinMax(ReplacementTexture& rtex);
	static std::optional<ReplacementTexture> LoadReplacementTexture(const TextureName& name, const std::string& filename, bool only_base_image);
	static void QueueAsyncReplacementTextureLoad(const TextureName& name, const std::string& filename, bool mipmap, bool cache_only);
	static void PrecacheReplacementTextures();
	static void ClearReplacementTextures();

	static size_t ReplacementTextureBytes(const ReplacementTexture& tex);
	static size_t GetReplacementCacheBudget();
	static void TouchReplacementCacheLocked(const TextureName& name);
	static const ReplacementTexture* InsertReplacementCacheLocked(const TextureName& name, ReplacementTexture& tex);
	static void ResetReplacementCacheLocked();

	static void StartWorkerThread();
	static void StopWorkerThread();
	static void QueueWorkerThreadItem(std::function<void()> fn, bool high_priority);
	static void WorkerThreadEntryPoint();
	static void SyncWorkerThread();
	static void CancelPendingLoadsAndDumps();

	static std::string s_current_serial;

	/// Textures that have been dumped, to save stat() calls.
	static std::unordered_set<TextureName> s_dumped_textures;
	static std::mutex s_dumped_textures_mutex;

	/// Lookup map of texture names to replacements, if they exist.
	static std::unordered_map<TextureName, std::string> s_replacement_texture_filenames;

	/// Lookup map of texture names without CLUT hash, to know when we need to disable paltex.
	static std::unordered_set<TextureName> s_replacement_textures_without_clut_hash;

	/// Lookup map of texture names to replacement data which has been cached.
	static std::unordered_map<TextureName, ReplacementTexture> s_replacement_texture_cache;
	static std::mutex s_replacement_texture_cache_mutex;

	/// Byte accounting + LRU ordering for the cache above.
	///
	/// Replacement packs can be enormous — a 5 GB uncompressed-DDS Persona 3 FES pack was
	/// OOM-killing Android mid-load — and this cache previously had NO size cap and NO
	/// eviction: it was only ever cleared wholesale on shutdown/game change, so every texture
	/// the game touched stayed resident until the process died. Turning Precache off did not
	/// help, it only changed how quickly memory filled. Now we track bytes and evict the
	/// least-recently-used entries once past a budget derived from physical RAM, so an
	/// oversized pack degrades to "some textures aren't replaced" instead of a hard crash.
	static size_t s_replacement_texture_cache_bytes = 0;
	static size_t s_replacement_texture_cache_budget = 0; // lazily computed on first use
	static std::list<TextureName> s_replacement_texture_lru; // front = least recently used
	static std::unordered_map<TextureName, std::list<TextureName>::iterator> s_replacement_texture_lru_map;
	static bool s_replacement_cache_budget_hit = false;

	/// List of textures that are pending asynchronous load. Second element is whether we're only precaching.
	static std::unordered_map<TextureName, bool> s_pending_async_load_textures;

	/// List of textures that we have asynchronously loaded and can now be injected back into the TC.
	/// Second element is whether the texture should be created with mipmaps.
	static std::vector<std::pair<TextureName, bool>> s_async_loaded_textures;

	/// Loader/dumper thread.
	static std::thread s_worker_thread;
	static std::mutex s_worker_thread_mutex;
	static std::condition_variable s_worker_thread_cv;
	static std::deque<std::pair<std::function<void()>, bool>> s_worker_thread_queue;
	static bool s_worker_thread_running = false;
}; // namespace GSTextureReplacements

size_t GSTextureReplacements::ReplacementTextureBytes(const ReplacementTexture& tex)
{
	size_t bytes = tex.data.size();
	for (const ReplacementTexture::MipData& mip : tex.mips)
		bytes += mip.data.size();
	return bytes;
}

size_t GSTextureReplacements::GetReplacementCacheBudget()
{
	if (s_replacement_texture_cache_budget != 0)
		return s_replacement_texture_cache_budget;

	// Derived from physical RAM rather than hardcoded: the same core runs on 3 GB phones and
	// 32 GB desktops. A quarter of RAM leaves headroom for the EE/GS allocations and, on
	// Android, keeps us clear of the low-memory killer.
	//
	// The ceiling is deliberately generous. The cache only ever grows to what a pack actually
	// loads, so a high cap costs nothing on small packs — it only decides when we START
	// EVICTING, and evicting a pack that would otherwise have fit turns a one-off load into
	// repeated reload churn (felt as stutter when walking into a new area). Real packs
	// measured: God of War 1 HD = 2.97 GB, Persona 3 FES HD = 5.0 GB, both UNCOMPRESSED DDS.
	// A 2 GB ceiling made the GoW1 pack evict even on a 12 GB tablet with room to spare, so
	// it is 3 GB: enough to hold that pack whole, while RAM/4 remains the real limiter on the
	// phones that actually need protecting. NOTE 3 GB is also the largest round value that
	// cannot overflow a 32-bit size_t (4 GB would wrap to 0 and evict everything).
	constexpr size_t MIN_BUDGET = static_cast<size_t>(192) * 1024 * 1024;
	constexpr size_t MAX_BUDGET = static_cast<size_t>(3072) * 1024 * 1024;
	const u64 physical = GetPhysicalMemory();
	size_t budget = (physical != 0) ? static_cast<size_t>(physical / 4) : MIN_BUDGET;
	if (budget < MIN_BUDGET)
		budget = MIN_BUDGET;
	if (budget > MAX_BUDGET)
		budget = MAX_BUDGET;

	s_replacement_texture_cache_budget = budget;
	Console.WriteLnFmt("Texture replacements: cache budget {} MB (physical memory {} MB).",
		budget / 1048576, physical / 1048576);
	return s_replacement_texture_cache_budget;
}

void GSTextureReplacements::TouchReplacementCacheLocked(const TextureName& name)
{
	const auto it = s_replacement_texture_lru_map.find(name);
	if (it == s_replacement_texture_lru_map.end())
		return;

	// Back = most recently used; the front is what gets evicted first.
	s_replacement_texture_lru.splice(s_replacement_texture_lru.end(), s_replacement_texture_lru, it->second);
}

const GSTextureReplacements::ReplacementTexture* GSTextureReplacements::InsertReplacementCacheLocked(
	const TextureName& name, ReplacementTexture& tex)
{
	const size_t incoming = ReplacementTextureBytes(tex);
	const size_t budget = GetReplacementCacheBudget();

	// A single texture larger than the entire budget can never be held. Leave [tex] untouched
	// so the caller can still upload it this once, rather than evicting everything for it.
	if (incoming > budget)
		return nullptr;

	while ((s_replacement_texture_cache_bytes + incoming) > budget && !s_replacement_texture_lru.empty())
	{
		const TextureName victim = s_replacement_texture_lru.front();
		const auto vit = s_replacement_texture_cache.find(victim);
		if (vit != s_replacement_texture_cache.end())
		{
			s_replacement_texture_cache_bytes -= ReplacementTextureBytes(vit->second);
			s_replacement_texture_cache.erase(vit);
		}
		s_replacement_texture_lru_map.erase(victim);
		s_replacement_texture_lru.pop_front();

		if (!s_replacement_cache_budget_hit)
		{
			s_replacement_cache_budget_hit = true;
			Console.WarningFmt("Texture replacements: cache budget of {} MB reached; evicting. An oversized "
							   "pack (typically uncompressed DDS) will only be partially applied.",
				budget / 1048576);
			Host::AddIconOSDMessage("ReplacementCacheBudget", ICON_FA_CIRCLE_EXCLAMATION,
				fmt::format(TRANSLATE_FS("TextureReplacement",
								"Texture pack is larger than the {} MB cache budget, so only part of it will be "
								"applied. Use a block-compressed (BC/DXT) pack for full coverage."),
					budget / 1048576),
				Host::OSD_WARNING_DURATION);
		}
	}

	s_replacement_texture_cache_bytes += incoming;
	s_replacement_texture_lru.push_back(name);
	s_replacement_texture_lru_map.emplace(name, std::prev(s_replacement_texture_lru.end()));
	return &s_replacement_texture_cache.emplace(name, std::move(tex)).first->second;
}

void GSTextureReplacements::ResetReplacementCacheLocked()
{
	s_replacement_texture_cache.clear();
	s_replacement_texture_lru.clear();
	s_replacement_texture_lru_map.clear();
	s_replacement_texture_cache_bytes = 0;
	s_replacement_cache_budget_hit = false;
}

TextureName GSTextureReplacements::CreateTextureName(const GSTextureCache::HashCacheKey& hash, u32 miplevel)
{
	TextureName name;
	name.bits = 0;
	name.TEX0_PSM = hash.TEX0.PSM;
	name.TEX0_TW = hash.TEX0.TW;
	name.TEX0_TH = hash.TEX0.TH;
	name.TEXA_TA0 = hash.TEXA.TA0;
	name.TEXA_AEM = hash.TEXA.AEM;
	name.TEXA_TA1 = hash.TEXA.TA1;
	name.TEX0Hash = hash.TEX0Hash;
	name.CLUTHash = name.HasPalette() ? hash.CLUTHash : 0;
	name.miplevel = miplevel;
	name.region_width = hash.region_width;
	name.region_height = hash.region_height;
	return name;
}

GSTextureCache::HashCacheKey GSTextureReplacements::HashCacheKeyFromTextureName(const TextureName& tn)
{
	const GSLocalMemory::psm_t& psm_s = GSLocalMemory::m_psm[tn.TEX0_PSM];
	GSTextureCache::HashCacheKey key = {};
	key.TEX0.PSM = tn.TEX0_PSM;
	key.TEX0.TW = tn.TEX0_TW;
	key.TEX0.TH = tn.TEX0_TH;
	if (psm_s.pal == 0 && psm_s.fmt > 0)
	{
		key.TEXA.TA0 = tn.TEXA_TA0;
		key.TEXA.AEM = tn.TEXA_AEM;
		key.TEXA.TA1 = tn.TEXA_TA1;
	}
	key.TEX0Hash = tn.TEX0Hash;
	key.CLUTHash = tn.HasPalette() ? tn.CLUTHash : 0;
	key.region_width = tn.region_width;
	key.region_height = tn.region_height;
	return key;
}

std::optional<TextureName> GSTextureReplacements::ParseReplacementName(const std::string& filename)
{
	TextureName ret;
	ret.miplevel = 0;

	GSTextureCache::SourceRegion full_region;

	char extension_dot;
	if (std::sscanf(filename.c_str(), TEXTURE_FILENAME_REGION_CLUT_FORMAT_STRING "%c", &ret.TEX0Hash, &ret.CLUTHash,
			&ret.region_width, &ret.region_height, &ret.bits, &extension_dot) == 6 &&
		extension_dot == '.')
	{
		ret.RemoveUnusedBits();
		return ret;
	}

	if (std::sscanf(filename.c_str(), TEXTURE_FILENAME_REGION_FORMAT_STRING "%c", &ret.TEX0Hash,
			&ret.region_width, &ret.region_height, &ret.bits, &extension_dot) == 5 &&
		extension_dot == '.')
	{
		ret.RemoveUnusedBits();
		ret.CLUTHash = 0;
		return ret;
	}

	// Allow loading of dumped textures from older versions that included the full region bits.
	if (std::sscanf(filename.c_str(), TEXTURE_FILENAME_OLD_REGION_CLUT_FORMAT_STRING "%c", &ret.TEX0Hash, &ret.CLUTHash,
			&full_region.bits, &ret.bits, &extension_dot) == 5 &&
		extension_dot == '.')
	{
		ret.RemoveUnusedBits();
		ret.region_width = static_cast<u32>(full_region.GetWidth());
		ret.region_height = static_cast<u32>(full_region.GetHeight());
		return ret;
	}

	if (std::sscanf(filename.c_str(), TEXTURE_FILENAME_OLD_REGION_FORMAT_STRING "%c", &ret.TEX0Hash, &full_region.bits,
			&ret.bits, &extension_dot) == 4 &&
		extension_dot == '.')
	{
		ret.RemoveUnusedBits();
		ret.CLUTHash = 0;
		ret.region_width = static_cast<u32>(full_region.GetWidth());
		ret.region_height = static_cast<u32>(full_region.GetHeight());
		return ret;
	}

	ret.region_width = 0;
	ret.region_height = 0;

	if (std::sscanf(filename.c_str(), TEXTURE_FILENAME_CLUT_FORMAT_STRING "%c", &ret.TEX0Hash, &ret.CLUTHash, &ret.bits,
			&extension_dot) == 4 &&
		extension_dot == '.')
	{
		ret.RemoveUnusedBits();
		return ret;
	}

	if (std::sscanf(filename.c_str(), TEXTURE_FILENAME_FORMAT_STRING "%c", &ret.TEX0Hash, &ret.bits, &extension_dot) ==
			3 &&
		extension_dot == '.')
	{
		ret.RemoveUnusedBits();
		ret.CLUTHash = 0;
		return ret;
	}

	return std::nullopt;
}

std::string GSTextureReplacements::GetGameTextureDirectory()
{
	return Path::Combine(EmuFolders::Textures, s_current_serial);
}

std::string GSTextureReplacements::GetDumpFilename(const TextureName& name, u32 level)
{
	std::string ret;
	if (s_current_serial.empty())
		return ret;

	const std::string game_dir(GetGameTextureDirectory());
	const std::string game_subdir(Path::Combine(game_dir, TEXTURE_DUMP_SUBDIRECTORY_NAME));

	if (!FileSystem::DirectoryExists(game_subdir.c_str()))
	{
		// create both dumps and replacements
		if (!FileSystem::CreateDirectoryPath(game_dir.c_str(), false) ||
			!FileSystem::EnsureDirectoryExists(game_subdir.c_str(), false) ||
			!FileSystem::EnsureDirectoryExists(Path::Combine(game_dir, TEXTURE_REPLACEMENT_SUBDIRECTORY_NAME).c_str(), false))
		{
			// if it fails to create, we're not going to be able to use it anyway
			return ret;
		}
	}

	std::string filename;
	if (name.HasRegion())
	{
		if (name.HasPalette())
		{
			filename = (level > 0)
				? StringUtil::StdStringFromFormat(TEXTURE_FILENAME_REGION_CLUT_FORMAT_STRING "-mip%u.png",
					name.TEX0Hash, name.CLUTHash, name.region_width, name.region_height, name.bits, level)
				: StringUtil::StdStringFromFormat(TEXTURE_FILENAME_REGION_CLUT_FORMAT_STRING ".png",
					name.TEX0Hash, name.CLUTHash, name.region_width, name.region_height, name.bits);
		}
		else
		{
			filename = (level > 0)
				? StringUtil::StdStringFromFormat(TEXTURE_FILENAME_REGION_FORMAT_STRING "-mip%u.png",
					name.TEX0Hash, name.region_width, name.region_height, name.bits, level)
				: StringUtil::StdStringFromFormat(TEXTURE_FILENAME_REGION_FORMAT_STRING ".png",
					name.TEX0Hash, name.region_width, name.region_height, name.bits);
		}
	}
	else
	{
		if (name.HasPalette())
		{
			filename = (level > 0)
				? StringUtil::StdStringFromFormat(TEXTURE_FILENAME_CLUT_FORMAT_STRING "-mip%u.png",
				                                  name.TEX0Hash, name.CLUTHash, name.bits, level)
				: StringUtil::StdStringFromFormat(TEXTURE_FILENAME_CLUT_FORMAT_STRING ".png",
				                                  name.TEX0Hash, name.CLUTHash, name.bits);
		}
		else
		{
			filename = (level > 0)
				? StringUtil::StdStringFromFormat(TEXTURE_FILENAME_FORMAT_STRING "-mip%u.png",
				                                  name.TEX0Hash, name.bits, level)
				: StringUtil::StdStringFromFormat(TEXTURE_FILENAME_FORMAT_STRING ".png",
				                                  name.TEX0Hash, name.bits);
		}
	}

	ret = Path::Combine(game_subdir, filename);

	return ret;
}

void GSTextureReplacements::Initialize()
{
	s_current_serial = VMManager::GetDiscSerial();

	if (GSConfig.DumpReplaceableTextures || GSConfig.LoadTextureReplacements)
		StartWorkerThread();

	ReloadReplacementMap();
}

void GSTextureReplacements::GameChanged()
{
	std::string new_serial = VMManager::GetDiscSerial();
	if (s_current_serial == new_serial)
		return;

	s_current_serial = std::move(new_serial);
	ReloadReplacementMap();
	ClearDumpedTextureList();
}

/// If the given file exists in the given directory, but with a different case than the original file, write its path to `*output` and return true.
static bool GetWrongCasePath(std::string* output, const char* dir, std::string_view file, FileSystem::FindResultsArray* reuseme)
{
	if (FileSystem::FindFiles(dir, "*", FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_HIDDEN_FILES, reuseme))
	{
		for (const FILESYSTEM_FIND_DATA& fd : *reuseme)
		{
			std::string_view name = Path::GetFileName(fd.FileName);
			if (name.size() != file.size())
				continue;
			if (0 == strncmp(name.data(), file.data(), name.size()))
				continue;
			if (0 == StringUtil::Strncasecmp(name.data(), file.data(), name.size()))
			{
				*output = fd.FileName;
				return true;
			}
		}
	}
	return false;
}

void GSTextureReplacements::ReloadReplacementMap()
{
	SyncWorkerThread();

	// clear out the caches
	{
		s_replacement_texture_filenames.clear();
		s_replacement_textures_without_clut_hash.clear();

		std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);
		ResetReplacementCacheLocked();
		s_pending_async_load_textures.clear();
		s_async_loaded_textures.clear();
	}

	// can't replace bios textures.
	if (s_current_serial.empty() || !GSConfig.LoadTextureReplacements)
	{
		// Say why, rather than returning silently — "off" and "no serial yet" are the two
		// most common reasons a pack appears to do nothing (see the summary log below).
		if (!s_current_serial.empty() && !GSConfig.LoadTextureReplacements)
			Console.WriteLnFmt("Texture replacements: disabled (LoadTextureReplacements off) for {}", s_current_serial);
		return;
	}

	const std::string texture_dir = GetGameTextureDirectory();
	const std::string replacement_dir(Path::Combine(texture_dir, TEXTURE_REPLACEMENT_SUBDIRECTORY_NAME));

	FileSystem::FindResultsArray files;

	// For some reason texture pack authors think it's a good idea to rename the replacements directory to something with the wrong case...
	std::string wrong_case_path;
	const std::string* right_case_path = nullptr;
	if (GetWrongCasePath(&wrong_case_path, EmuFolders::Textures.c_str(), s_current_serial, &files))
		right_case_path = &texture_dir;
	else if (GetWrongCasePath(&wrong_case_path, texture_dir.c_str(), TEXTURE_REPLACEMENT_SUBDIRECTORY_NAME, &files))
		right_case_path = &replacement_dir;
	if (right_case_path)
	{
		Host::AddKeyedOSDMessage("TextureReplacementDirCaseMismatch",
			fmt::format(TRANSLATE_FS("TextureReplacement", "Texture replacement directory {} will not work on case sensitive filesystems.\n"
			                                               "Rename it to {} to remove this warning."),
			            wrong_case_path, *right_case_path),
			Host::OSD_WARNING_DURATION);
	}

	if (!FileSystem::FindFiles(replacement_dir.c_str(), "*", FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES | FILESYSTEM_FIND_RECURSIVE, &files))
		return;

	std::string filename;
	for (FILESYSTEM_FIND_DATA& fd : files)
	{
		// file format we can handle?
		filename = Path::GetFileName(fd.FileName);
		if (!GetLoader(filename))
			continue;

		// parse the name if it's valid
		std::optional<TextureName> name = ParseReplacementName(filename);
		if (!name.has_value())
			continue;

		DbgCon.WriteLn("Found %ux%u replacement '%.*s'", name->Width(), name->Height(), static_cast<int>(filename.size()), filename.data());
		s_replacement_texture_filenames.emplace(name.value(), std::move(fd.FileName));

		// zero out the CLUT hash, because we need this for checking if there's any replacements with this hash when using paltex
		name->CLUTHash = 0;
		s_replacement_textures_without_clut_hash.insert(name.value());
	}

	// "indexed", not "loaded": this count only proves filename discovery + name parsing.
	// It says nothing about whether any texture was looked up, decoded, or uploaded — those
	// are separate stages that fail independently and silently. Every quiet path out of this
	// function looks identical from outside (feature off, wrong serial, empty folder,
	// unparseable names), so print the count AND the exact directory scanned: a zero here
	// with a path that doesn't match the user's pack folder is the whole diagnosis.
	Console.WriteLnFmt("Texture replacements: {} indexed for '{}' (scanned {})",
		s_replacement_texture_filenames.size(), s_current_serial, replacement_dir);

	if (!s_replacement_texture_filenames.empty())
	{
		if (GSConfig.PrecacheTextureReplacements)
			PrecacheReplacementTextures();

		// log a warning when paltex is on and preloading is off, since we'll be disabling paltex
		if (GSConfig.GPUPaletteConversion && GSConfig.TexturePreloading != TexturePreloadingLevel::Full)
		{
			Console.Warning("Replacement textures were found, and GPU palette conversion is enabled without full preloading.");
			Console.Warning("Palette textures will be disabled. Please enable full preloading or disable GPU palette conversion.");
		}
	}
}

void GSTextureReplacements::UpdateConfig(Pcsx2Config::GSOptions& old_config)
{
	// get rid of worker thread if it's no longer needed
	if (s_worker_thread_running && !GSConfig.DumpReplaceableTextures && !GSConfig.LoadTextureReplacements)
		StopWorkerThread();
	if (!s_worker_thread_running && (GSConfig.DumpReplaceableTextures || GSConfig.LoadTextureReplacements))
		StartWorkerThread();

	if ((!GSConfig.DumpReplaceableTextures && old_config.DumpReplaceableTextures) ||
		(!GSConfig.LoadTextureReplacements && old_config.LoadTextureReplacements))
	{
		CancelPendingLoadsAndDumps();
	}

	if (GSConfig.LoadTextureReplacements && !old_config.LoadTextureReplacements)
		ReloadReplacementMap();
	else if (!GSConfig.LoadTextureReplacements && old_config.LoadTextureReplacements)
		ClearReplacementTextures();

	if (!GSConfig.DumpReplaceableTextures && old_config.DumpReplaceableTextures)
		ClearDumpedTextureList();

	if (GSConfig.LoadTextureReplacements && GSConfig.PrecacheTextureReplacements && !old_config.PrecacheTextureReplacements)
		PrecacheReplacementTextures();
}

void GSTextureReplacements::Shutdown()
{
	StopWorkerThread();

	std::string().swap(s_current_serial);
	ClearReplacementTextures();
	ClearDumpedTextureList();
}

u32 GSTextureReplacements::CalcMipmapLevelsForReplacement(u32 width, u32 height)
{
	return static_cast<u32>(std::log2(std::max(width, height))) + 1u;
}

bool GSTextureReplacements::HasAnyReplacementTextures()
{
	return !s_replacement_texture_filenames.empty();
}

bool GSTextureReplacements::HasReplacementTextureWithOtherPalette(const GSTextureCache::HashCacheKey& hash)
{
	const TextureName name(CreateTextureName(hash.WithRemovedCLUTHash(), 0));
	return s_replacement_textures_without_clut_hash.find(name) != s_replacement_textures_without_clut_hash.end();
}

GSTexture* GSTextureReplacements::LookupReplacementTexture(const GSTextureCache::HashCacheKey& hash, bool mipmap,
	bool* pending, std::pair<u8, u8>* alpha_minmax)
{
	const TextureName name(CreateTextureName(hash, 0));
	*pending = false;

	// replacement for this name exists?
	auto fnit = s_replacement_texture_filenames.find(name);
	if (fnit == s_replacement_texture_filenames.end())
		return nullptr;

	// try the full cache first, to avoid reloading from disk
	{
		std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);
		auto it = s_replacement_texture_cache.find(name);
		if (it != s_replacement_texture_cache.end())
		{
			// replacement is cached, can immediately upload to host GPU
			TouchReplacementCacheLocked(name);
			*alpha_minmax = it->second.alpha_minmax;
			return CreateReplacementTexture(it->second, mipmap);
		}
	}

	// load asynchronously?
	if (GSConfig.LoadTextureReplacementsAsync)
	{
		// replacement will be injected into the TC later on
		std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);
		QueueAsyncReplacementTextureLoad(name, fnit->second, mipmap, false);

		*pending = true;
		return nullptr;
	}
	else
	{
		// synchronous load
		std::optional<ReplacementTexture> replacement(LoadReplacementTexture(name, fnit->second, !mipmap));
		if (!replacement.has_value())
			return nullptr;

		// Insert into cache. This can decline when a single texture is bigger than the entire
		// budget, in which case [local] is left intact and we upload it just this once.
		std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);
		ReplacementTexture& local = replacement.value();
		const ReplacementTexture* rtex = InsertReplacementCacheLocked(name, local);
		if (!rtex)
			rtex = &local;

		// and upload to gpu
		*alpha_minmax = rtex->alpha_minmax;
		return CreateReplacementTexture(*rtex, mipmap);
	}
}

template <GSTexture::Format format>
std::pair<u8, u8> GSTextureReplacements::GetBCAlphaMinMax(ReplacementTexture& rtex)
{
	constexpr u32 BC_BLOCK_SIZE = 4;
	constexpr u32 BC_BLOCK_BYTES = (format == GSTexture::Format::BC1) ? 8 : 16;

	const u32 blocks_wide = (rtex.width + (BC_BLOCK_SIZE - 1)) / BC_BLOCK_SIZE;
	const u32 blocks_high = (rtex.height + (BC_BLOCK_SIZE - 1)) / BC_BLOCK_SIZE;

	GSVector4i minc = GSVector4i::xffffffff();
	GSVector4i maxc = GSVector4i::zero();

	for (u32 y = 0; y < blocks_high; y++)
	{
		const u8* block_in = rtex.data.data() + y * rtex.pitch;
		alignas(16) u8 block_pixels_out[BC_BLOCK_SIZE * BC_BLOCK_SIZE * sizeof(u32)];

		for (u32 x = 0; x < blocks_wide; x++, block_in += BC_BLOCK_BYTES)
		{
			switch (format)
			{
				case GSTexture::Format::BC1:
					DecompressBlockBC1(0, 0, sizeof(u32) * BC_BLOCK_SIZE, block_in, block_pixels_out);
					break;
				case GSTexture::Format::BC2:
					DecompressBlockBC2(0, 0, sizeof(u32) * BC_BLOCK_SIZE, block_in, block_pixels_out);
					break;
				case GSTexture::Format::BC3:
					DecompressBlockBC3(0, 0, sizeof(u32) * BC_BLOCK_SIZE, block_in, block_pixels_out);
					break;

				case GSTexture::Format::BC7:
					bc7decomp::unpack_bc7(block_in, reinterpret_cast<bc7decomp::color_rgba*>(block_pixels_out));
					break;
			}

			const u8* out_ptr = block_pixels_out;
			for (u32 i = 0; i < ((BC_BLOCK_SIZE * BC_BLOCK_SIZE * sizeof(u32)) / sizeof(GSVector4i)); i++)
			{
				const GSVector4i v = GSVector4i::load<true>(out_ptr);
				out_ptr += sizeof(GSVector4i);
				minc = minc.min_u32(v);
				maxc = maxc.max_u32(v);
			}
		}
	}

	return std::make_pair<u8, u8>(static_cast<u8>(minc.minv_u32() >> 24), static_cast<u8>(maxc.maxv_u32() >> 24));
}

void GSTextureReplacements::SetReplacementTextureAlphaMinMax(ReplacementTexture& rtex)
{
	switch (rtex.format)
	{
		case GSTexture::Format::BC1:
			rtex.alpha_minmax = GetBCAlphaMinMax<GSTexture::Format::BC1>(rtex);
			break;

		case GSTexture::Format::BC2:
			rtex.alpha_minmax = GetBCAlphaMinMax<GSTexture::Format::BC2>(rtex);
			break;

		case GSTexture::Format::BC3:
			rtex.alpha_minmax = GetBCAlphaMinMax<GSTexture::Format::BC3>(rtex);
			break;

		case GSTexture::Format::BC7:
			rtex.alpha_minmax = GetBCAlphaMinMax<GSTexture::Format::BC7>(rtex);
			break;

		default:
			pxAssert(rtex.format == GSTexture::Format::Color);
			rtex.alpha_minmax = GSGetRGBA8AlphaMinMax(rtex.data.data(), rtex.width, rtex.height, rtex.pitch);
			break;
	}
}

std::optional<GSTextureReplacements::ReplacementTexture> GSTextureReplacements::LoadReplacementTexture(const TextureName& name, const std::string& filename, bool only_base_image)
{
	ReplacementTextureLoader loader = GetLoader(filename);
	if (!loader)
		return std::nullopt;

	ReplacementTexture rtex;
	if (!loader(filename.c_str(), &rtex, only_base_image))
	{
		Console.Warning("Failed to load replacement texture %s", filename.c_str());
		return std::nullopt;
	}

	SetReplacementTextureAlphaMinMax(rtex);

	return rtex;
}

void GSTextureReplacements::QueueAsyncReplacementTextureLoad(const TextureName& name, const std::string& filename, bool mipmap, bool cache_only)
{
	// check the pending list, so we don't queue it up multiple times
	auto it = s_pending_async_load_textures.find(name);
	if (it != s_pending_async_load_textures.end())
	{
		// remove from queue if it's cache-only, so we bump it to the front of the work items
		if (!cache_only && it->second)
		{
			s_pending_async_load_textures.erase(it);
		}
		else
		{
			it->second &= cache_only;
			return;
		}
	}

	s_pending_async_load_textures.emplace(name, cache_only);
	QueueWorkerThreadItem([name, filename, mipmap]() {
		// actually load the file, this is what will take the time
		std::optional<ReplacementTexture> replacement(LoadReplacementTexture(name, filename, !mipmap));

		// check the pending set, there's a race here if we disable replacements while loading otherwise
		// also check the full replacement list, if async loading is off, it might already be in there
		std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);
		auto it = s_pending_async_load_textures.find(name);
		if (it == s_pending_async_load_textures.end() ||
			s_replacement_texture_cache.find(name) != s_replacement_texture_cache.end())
		{
			if (it != s_pending_async_load_textures.end())
				s_pending_async_load_textures.erase(it);

			return;
		}

		// insert into the cache and queue for later injection
		if (replacement.has_value() && InsertReplacementCacheLocked(name, replacement.value()))
		{
			s_async_loaded_textures.emplace_back(name, mipmap);
		}
		else
		{
			// Load failed, or the texture is too large to ever cache. Either way it can't be
			// injected later (injection reads back out of the cache), so drop the pending mark.
			s_pending_async_load_textures.erase(name);
		}
	}, !cache_only);
}

void GSTextureReplacements::PrecacheReplacementTextures()
{
	std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);

	// predict whether the requests will come with mipmaps
	// TODO: This will be wrong for hw mipmap games like Jak.
	const bool mipmap = GSConfig.HWMipmap || GSConfig.TriFilter == TriFiltering::Forced;

	// pretty simple, just go through the filenames and if any aren't cached, cache them
	for (const auto& it : s_replacement_texture_filenames)
	{
		// Stop once the cache is full. Queueing the remainder of an oversized pack would only
		// thrash — each load immediately evicting the previous one — and hammer storage for
		// nothing. The textures still load on demand later if they're actually used.
		if (s_replacement_texture_cache_bytes >= GetReplacementCacheBudget())
			break;

		if (s_replacement_texture_cache.find(it.first) != s_replacement_texture_cache.end())
			continue;

		// precaching always goes async.. for now
		QueueAsyncReplacementTextureLoad(it.first, it.second, mipmap, true);
	}
}

void GSTextureReplacements::ClearReplacementTextures()
{
	s_replacement_texture_filenames.clear();
	s_replacement_textures_without_clut_hash.clear();

	std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);
	ResetReplacementCacheLocked();
	s_pending_async_load_textures.clear();
	s_async_loaded_textures.clear();
}

GSTexture* GSTextureReplacements::CreateReplacementTexture(const ReplacementTexture& rtex, bool mipmap)
{
	// can't use generated mipmaps with compressed formats, because they can't be rendered to
	// in the future I guess we could decompress the dds and generate them... but there's no reason that modders can't generate mips in dds
	if (mipmap && GSTexture::IsCompressedFormat(rtex.format) && rtex.mips.empty())
	{
		static bool log_once = false;
		if (!log_once)
		{
			Console.Warning("Disabling autogenerated mipmaps on one or more compressed replacement textures.");
			Host::AddIconOSDMessage("DisablingReplacementAutoGeneratedMipmap", ICON_FA_CIRCLE_EXCLAMATION,
				TRANSLATE_SV("GS", "Disabling autogenerated mipmaps on one or more compressed replacement textures. "
								   "Please generate mipmaps when compressing your textures."),
				Host::OSD_WARNING_DURATION);
			log_once = true;
		}

		mipmap = false;
	}

	GSTexture* tex = g_gs_device->CreateTexture(rtex.width, rtex.height, static_cast<int>(rtex.mips.size()) + 1, rtex.format);
	if (!tex)
		return nullptr;

	// upload base level
	tex->Update(GSVector4i(0, 0, rtex.width, rtex.height), rtex.data.data(), rtex.pitch);

	// and the mips if they're present in the replacement texture
	if (!rtex.mips.empty())
	{
		for (u32 i = 0; i < static_cast<u32>(rtex.mips.size()); i++)
		{
			const ReplacementTexture::MipData& mip = rtex.mips[i];
			tex->Update(GSVector4i(0, 0, static_cast<int>(mip.width), static_cast<int>(mip.height)), mip.data.data(), mip.pitch, i + 1);
		}
	}

	return tex;
}

void GSTextureReplacements::ProcessAsyncLoadedTextures()
{
	// Per-frame GPU upload budget.
	//
	// Every pending texture used to be uploaded in ONE call ("this should be reasonably
	// quick" — true for a handful of BC-compressed textures, false otherwise). Walking into
	// a new area streams a whole batch in at once, and real HD packs are entirely
	// UNCOMPRESSED DDS (God of War 1 HD = 2.97 GB over 2235 files, Persona 3 FES = 5.0 GB),
	// where one 2048x2048 texture is 16 MB. Ten arriving together meant ~160 MB of GPU
	// upload inside a single frame, while holding the cache lock — reported as "FPS drops to
	// 50 and stutters in certain areas" with a pack that runs fine on other emulators.
	//
	// Spreading the uploads costs a frame or two of pop-in, which is imperceptible next to
	// dropping the frame outright. 16 MB bounds the worst case to roughly one uncompressed
	// 2048x2048 texture per frame — the natural granularity here.
	constexpr size_t MAX_UPLOAD_BYTES_PER_FRAME = static_cast<size_t>(16) * 1024 * 1024;

	std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);
	size_t uploaded_bytes = 0;
	size_t idx = 0;
	for (; idx < s_async_loaded_textures.size(); idx++)
	{
		// Checked at the top with a zero start, so a texture larger than the whole budget
		// still goes through on its own frame and can never wedge the queue.
		if (uploaded_bytes >= MAX_UPLOAD_BYTES_PER_FRAME)
			break;

		const auto& [name, mipmap] = s_async_loaded_textures[idx];

		// no longer pending!
		const auto pit = s_pending_async_load_textures.find(name);
		if (pit != s_pending_async_load_textures.end())
		{
			const bool cache_only = pit->second;
			s_pending_async_load_textures.erase(pit);

			// if we were precaching, don't inject into the TC if we didn't actually get requested
			// (costs no upload, so it doesn't draw down the budget)
			if (cache_only)
				continue;
		}

		// we should be in the cache now, lock and loaded
		auto it = s_replacement_texture_cache.find(name);
		if (it == s_replacement_texture_cache.end())
			continue;

		// upload and inject into TC
		GSTexture* tex = CreateReplacementTexture(it->second, mipmap);
		if (tex)
			g_texture_cache->InjectHashCacheTexture(HashCacheKeyFromTextureName(name), tex, it->second.alpha_minmax);

		uploaded_bytes += ReplacementTextureBytes(it->second);
	}

	// Carry whatever we didn't reach into the next frame rather than dropping it.
	if (idx >= s_async_loaded_textures.size())
		s_async_loaded_textures.clear();
	else
		s_async_loaded_textures.erase(s_async_loaded_textures.begin(), s_async_loaded_textures.begin() + idx);
}

void GSTextureReplacements::DumpTexture(const GSTextureCache::HashCacheKey& hash, const GIFRegTEX0& TEX0,
	const GIFRegTEXA& TEXA, GSTextureCache::SourceRegion region, GSLocalMemory& mem, u32 level)
{
	// check if it's been dumped or replaced already
	const TextureName name(CreateTextureName(hash, level));
	{
		std::unique_lock<std::mutex> lock(s_dumped_textures_mutex);
		if (s_dumped_textures.find(name) != s_dumped_textures.end() || s_replacement_texture_filenames.find(name) != s_replacement_texture_filenames.end())
			return;

		s_dumped_textures.insert(name);
	}

	// already exists on disk?
	std::string filename(GetDumpFilename(name, level));
	if (filename.empty() || FileSystem::FileExists(filename.c_str()))
		return;

	const std::string_view title(Path::GetFileTitle(filename));
	DevCon.WriteLn("Dumping %ux%u texture '%.*s'.", name.Width(), name.Height(), static_cast<int>(title.size()), title.data());

	// compute width/height
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[TEX0.PSM];
	const GSVector2i& bs = psm.bs;
	const int tw = region.HasX() ? region.GetWidth() : (1 << TEX0.TW);
	const int th = region.HasY() ? region.GetHeight() : (1 << TEX0.TH);
	const GSVector4i rect(region.GetRect(tw, th));
	const GSVector4i block_rect(rect.ralign<Align_Outside>(bs));
	const int read_width = block_rect.width();
	const int read_height = block_rect.height();
	const u32 pitch = static_cast<u32>(read_width) * sizeof(u32);

	// use per-texture buffer so we can compress the texture asynchronously and not block the GS thread
	// must be 32 byte aligned for ReadTexture().
	u8* buffer = static_cast<u8*>(_aligned_malloc(pitch * static_cast<u32>(read_height), 32));
	psm.rtx(mem, mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM), block_rect, buffer, pitch, TEXA);

	// okay, now we can actually dump it
	const u32 buffer_offset = ((rect.top - block_rect.top) * pitch) + ((rect.left - block_rect.left) * sizeof(u32));
	QueueWorkerThreadItem([filename = std::move(filename), tw, th, pitch, buffer, buffer_offset]() {
		if (!SavePNGImage(filename.c_str(), tw, th, buffer + buffer_offset, pitch))
			Console.Error(fmt::format("Failed to dump texture to '{}'.", filename));
		_aligned_free(buffer);
	}, false);
}

void GSTextureReplacements::ClearDumpedTextureList()
{
	std::unique_lock<std::mutex> lock(s_dumped_textures_mutex);
	s_dumped_textures.clear();
}

u32 GSTextureReplacements::GetDumpedTextureCount()
{
	std::unique_lock<std::mutex> lock(s_dumped_textures_mutex);
	return static_cast<u32>(s_dumped_textures.size());
}

u32 GSTextureReplacements::GetLoadedTextureCount()
{
	std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);
	return static_cast<u32>(s_replacement_texture_cache.size());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Worker Thread
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GSTextureReplacements::StartWorkerThread()
{
	std::unique_lock<std::mutex> lock(s_worker_thread_mutex);

	if (s_worker_thread.joinable())
		return;

	s_worker_thread_running = true;
	s_worker_thread = std::thread(WorkerThreadEntryPoint);
}

void GSTextureReplacements::StopWorkerThread()
{
	{
		std::unique_lock<std::mutex> lock(s_worker_thread_mutex);
		if (!s_worker_thread.joinable())
			return;

		s_worker_thread_running = false;
		s_worker_thread_cv.notify_one();
	}

	s_worker_thread.join();

	// clear out workery-things too
	CancelPendingLoadsAndDumps();
}

void GSTextureReplacements::QueueWorkerThreadItem(std::function<void()> fn, bool high_priority)
{
	pxAssert(s_worker_thread.joinable());

	std::unique_lock<std::mutex> lock(s_worker_thread_mutex);
	if (!high_priority)
	{
		// Low priority => throw on end.
		s_worker_thread_queue.emplace_back(std::move(fn), false);
	}
	else
	{
		auto iter = s_worker_thread_queue.rbegin();
		for (; iter != s_worker_thread_queue.rend(); ++iter)
		{
			// Found our first high priority item?
			if (iter->second)
			{
				// Insert after here!
				break;
			}
		}

		if (iter != s_worker_thread_queue.rend())
		{
			// Insert after the last high priority item. Remember base() points to the next element.
			s_worker_thread_queue.insert(iter.base(), std::make_pair(std::move(fn), true));
		}
		else
		{
			// All low-priority => insert at beginning.
			s_worker_thread_queue.emplace_front(std::move(fn), true);
		}
	}

	s_worker_thread_cv.notify_one();
}

void GSTextureReplacements::WorkerThreadEntryPoint()
{
	std::unique_lock<std::mutex> lock(s_worker_thread_mutex);
	while (s_worker_thread_running)
	{
		if (s_worker_thread_queue.empty())
		{
			s_worker_thread_cv.wait(lock);
			continue;
		}

		std::function<void()> fn = std::move(s_worker_thread_queue.front().first);
		s_worker_thread_queue.pop_front();
		lock.unlock();
		fn();
		lock.lock();
	}
}

void GSTextureReplacements::SyncWorkerThread()
{
	std::unique_lock<std::mutex> lock(s_worker_thread_mutex);
	if (!s_worker_thread.joinable())
		return;

	// not the most efficient by far, but it only gets called on config changes, so whatever
	for (;;)
	{
		if (s_worker_thread_queue.empty())
			break;

		lock.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		lock.lock();
	}
}

void GSTextureReplacements::CancelPendingLoadsAndDumps()
{
	std::unique_lock<std::mutex> lock(s_worker_thread_mutex);
	while (!s_worker_thread_queue.empty())
		s_worker_thread_queue.pop_back();
	s_async_loaded_textures.clear();
	s_pending_async_load_textures.clear();
}
