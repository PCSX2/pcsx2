// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/AlignedMalloc.h"
#include "common/Console.h"
#include "common/HashCombine.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/ScopedGuard.h"
#include "common/TextureDecompress.h"

#include "Config.h"
#include "Host.h"
#include "IconsFontAwesome5.h"
#include "GS/GSLocalMemory.h"
#include "GS/Renderers/HW/GSTextureReplacements.h"
#include "VMManager.h"

#include <cinttypes>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <tuple>
#include <thread>

// this is a #define instead of a variable to avoid warnings from non-literal format strings
#define TEXTURE_FILENAME_FORMAT_STRING "%" PRIx64 "-%08x"
#define TEXTURE_FILENAME_CLUT_FORMAT_STRING "%" PRIx64 "-%" PRIx64 "-%08x"
#define TEXTURE_FILENAME_REGION_FORMAT_STRING "%" PRIx64 "-r%" PRIx64 "-" "-%08x"
#define TEXTURE_FILENAME_REGION_CLUT_FORMAT_STRING "%" PRIx64 "-%" PRIx64 "-r%" PRIx64 "-%08x"
#define TEXTURE_REPLACEMENT_SUBDIRECTORY_NAME "replacements"
#define TEXTURE_DUMP_SUBDIRECTORY_NAME "dumps"

namespace
{
	struct TextureName // 24 bytes
	{
		u64 TEX0Hash;
		u64 CLUTHash;
		GSTextureCache::SourceRegion region;

		union
		{
			struct
			{
				u32 TEX0_PSM : 6;
				u32 TEX0_TW : 4;
				u32 TEX0_TH : 4;
				u32 TEX0_TCC : 1;
				u32 TEXA_TA0 : 8;
				u32 TEXA_AEM : 1;
				u32 TEXA_TA1 : 8;
			};
			u32 bits;
		};
		u32 miplevel;

		__fi u32 Width() const { return (region.HasX() ? region.GetWidth() : (1u << TEX0_TW)); }
		__fi u32 Height() const { return (region.HasY() ? region.GetWidth() : (1u << TEX0_TH)); }
		__fi bool HasPalette() const { return (GSLocalMemory::m_psm[TEX0_PSM].pal > 0); }
		__fi bool HasRegion() const { return region.HasEither(); }

		__fi bool operator==(const TextureName& rhs) const
		{
			return std::tie(TEX0Hash, CLUTHash, region.bits, bits) ==
				   std::tie(rhs.TEX0Hash, rhs.CLUTHash, region.bits, rhs.bits);
		}
		__fi bool operator!=(const TextureName& rhs) const
		{
			return std::tie(TEX0Hash, CLUTHash, region.bits, bits) !=
				   std::tie(rhs.TEX0Hash, rhs.CLUTHash, region.bits, rhs.bits);
		}
		__fi bool operator<(const TextureName& rhs) const
		{
			return std::tie(TEX0Hash, CLUTHash, region.bits, bits) <
				   std::tie(rhs.TEX0Hash, rhs.CLUTHash, region.bits, rhs.bits);
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
			HashCombine(h, val.TEX0Hash, val.CLUTHash, val.region.bits, val.bits, val.miplevel);
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

	static void StartWorkerThread();
	static void StopWorkerThread();
	static void QueueWorkerThreadItem(std::function<void()> fn);
	static void WorkerThreadEntryPoint();
	static void SyncWorkerThread();
	static void CancelPendingLoadsAndDumps();

	static std::string s_current_serial;

	/// Textures that have been dumped, to save stat() calls.
	static std::unordered_set<TextureName> s_dumped_textures;

	/// Lookup map of texture names to replacements, if they exist.
	static std::unordered_map<TextureName, std::string> s_replacement_texture_filenames;

	/// Lookup map of texture names without CLUT hash, to know when we need to disable paltex.
	static std::unordered_set<TextureName> s_replacement_textures_without_clut_hash;

	/// Lookup map of texture names to replacement data which has been cached.
	static std::unordered_map<TextureName, ReplacementTexture> s_replacement_texture_cache;
	static std::mutex s_replacement_texture_cache_mutex;

	/// List of textures that are pending asynchronous load. Second element is whether we're only precaching.
	static std::unordered_map<TextureName, bool> s_pending_async_load_textures;

	/// List of textures that we have asynchronously loaded and can now be injected back into the TC.
	/// Second element is whether the texture should be created with mipmaps.
	static std::vector<std::pair<TextureName, bool>> s_async_loaded_textures;

	/// Loader/dumper thread.
	static std::thread s_worker_thread;
	static std::mutex s_worker_thread_mutex;
	static std::condition_variable s_worker_thread_cv;
	static std::queue<std::function<void()>> s_worker_thread_queue;
	static bool s_worker_thread_running = false;
}; // namespace GSTextureReplacements

TextureName GSTextureReplacements::CreateTextureName(const GSTextureCache::HashCacheKey& hash, u32 miplevel)
{
	TextureName name;
	name.bits = 0;
	name.TEX0_PSM = hash.TEX0.PSM;
	name.TEX0_TW = hash.TEX0.TW;
	name.TEX0_TH = hash.TEX0.TH;
	name.TEX0_TCC = hash.TEX0.TCC;
	name.TEXA_TA0 = hash.TEXA.TA0;
	name.TEXA_AEM = hash.TEXA.AEM;
	name.TEXA_TA1 = hash.TEXA.TA1;
	name.TEX0Hash = hash.TEX0Hash;
	name.CLUTHash = name.HasPalette() ? hash.CLUTHash : 0;
	name.miplevel = miplevel;
	name.region = hash.region;
	return name;
}

GSTextureCache::HashCacheKey GSTextureReplacements::HashCacheKeyFromTextureName(const TextureName& tn)
{
	GSTextureCache::HashCacheKey key = {};
	key.TEX0.PSM = tn.TEX0_PSM;
	key.TEX0.TW = tn.TEX0_TW;
	key.TEX0.TH = tn.TEX0_TH;
	key.TEX0.TCC = tn.TEX0_TCC;
	key.TEXA.TA0 = tn.TEXA_TA0;
	key.TEXA.AEM = tn.TEXA_AEM;
	key.TEXA.TA1 = tn.TEXA_TA1;
	key.TEX0Hash = tn.TEX0Hash;
	key.CLUTHash = tn.HasPalette() ? tn.CLUTHash : 0;
	key.region = tn.region;
	return key;
}

std::optional<TextureName> GSTextureReplacements::ParseReplacementName(const std::string& filename)
{
	TextureName ret;
	ret.miplevel = 0;

	char extension_dot;
	if (std::sscanf(filename.c_str(), TEXTURE_FILENAME_REGION_CLUT_FORMAT_STRING "%c", &ret.TEX0Hash, &ret.CLUTHash,
			&ret.region.bits, &ret.bits, &extension_dot) == 5 &&
		extension_dot == '.')
	{
		return ret;
	}

	if (std::sscanf(filename.c_str(), TEXTURE_FILENAME_REGION_FORMAT_STRING "%c", &ret.TEX0Hash, &ret.region.bits,
			&ret.bits, &extension_dot) == 4 &&
		extension_dot == '.')
	{
		ret.CLUTHash = 0;
		return ret;
	}

	ret.region.bits = 0;

	if (std::sscanf(filename.c_str(), TEXTURE_FILENAME_CLUT_FORMAT_STRING "%c", &ret.TEX0Hash, &ret.CLUTHash, &ret.bits,
			&extension_dot) == 4 &&
		extension_dot == '.')
	{
		return ret;
	}

	if (std::sscanf(filename.c_str(), TEXTURE_FILENAME_FORMAT_STRING "%c", &ret.TEX0Hash, &ret.bits, &extension_dot) ==
			3 &&
		extension_dot == '.')
	{
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
	if (!FileSystem::DirectoryExists(game_dir.c_str()))
	{
		// create both dumps and replacements
		if (!FileSystem::CreateDirectoryPath(game_dir.c_str(), false) ||
			!FileSystem::EnsureDirectoryExists(Path::Combine(game_dir, "dumps").c_str(), false) ||
			!FileSystem::EnsureDirectoryExists(Path::Combine(game_dir, "replacements").c_str(), false))
		{
			// if it fails to create, we're not going to be able to use it anyway
			return ret;
		}
	}

	const std::string game_subdir(Path::Combine(game_dir, TEXTURE_DUMP_SUBDIRECTORY_NAME));

	std::string filename;
	if (name.HasRegion())
	{
		if (name.HasPalette())
		{
			filename = (level > 0) ?
						   StringUtil::StdStringFromFormat(TEXTURE_FILENAME_REGION_CLUT_FORMAT_STRING "-mip%u.png",
							   name.TEX0Hash, name.CLUTHash, name.region.bits, name.bits, level) :
						   StringUtil::StdStringFromFormat(TEXTURE_FILENAME_REGION_CLUT_FORMAT_STRING ".png",
							   name.TEX0Hash, name.CLUTHash, name.region.bits, name.bits);
		}
		else
		{
			filename = (level > 0) ? StringUtil::StdStringFromFormat(
										 TEXTURE_FILENAME_FORMAT_STRING "-mip%u.png", name.TEX0Hash, name.bits, level) :
									 StringUtil::StdStringFromFormat(
										 TEXTURE_FILENAME_FORMAT_STRING ".png", name.TEX0Hash, name.bits);
		}
	}
	else
	{
		if (name.HasPalette())
		{
			filename = (level > 0) ? StringUtil::StdStringFromFormat(TEXTURE_FILENAME_CLUT_FORMAT_STRING "-mip%u.png",
										 name.TEX0Hash, name.CLUTHash, name.bits, level) :
									 StringUtil::StdStringFromFormat(TEXTURE_FILENAME_CLUT_FORMAT_STRING ".png",
										 name.TEX0Hash, name.CLUTHash, name.bits);
		}
		else
		{
			filename = (level > 0) ? StringUtil::StdStringFromFormat(
										 TEXTURE_FILENAME_FORMAT_STRING "-mip%u.png", name.TEX0Hash, name.bits, level) :
									 StringUtil::StdStringFromFormat(
										 TEXTURE_FILENAME_FORMAT_STRING ".png", name.TEX0Hash, name.bits);
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

void GSTextureReplacements::ReloadReplacementMap()
{
	SyncWorkerThread();

	// clear out the caches
	{
		s_replacement_texture_filenames.clear();
		s_replacement_textures_without_clut_hash.clear();

		std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);
		s_replacement_texture_cache.clear();
		s_pending_async_load_textures.clear();
		s_async_loaded_textures.clear();
	}

	// can't replace bios textures.
	if (s_current_serial.empty() || !GSConfig.LoadTextureReplacements)
		return;

	const std::string replacement_dir(Path::Combine(GetGameTextureDirectory(), TEXTURE_REPLACEMENT_SUBDIRECTORY_NAME));

	FileSystem::FindResultsArray files;
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

		// insert into cache
		std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);
		const ReplacementTexture& rtex = s_replacement_texture_cache.emplace(name, std::move(replacement.value())).first->second;

		// and upload to gpu
		*alpha_minmax = rtex.alpha_minmax;
		return CreateReplacementTexture(rtex, mipmap);
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
		it->second &= cache_only;
		return;
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
		if (replacement.has_value())
		{
			s_replacement_texture_cache.emplace(name, std::move(replacement.value()));
			s_async_loaded_textures.emplace_back(name, mipmap);
		}
		else
		{
			// loading failed, so clear it from the pending list
			s_pending_async_load_textures.erase(name);
		}
	});
}

void GSTextureReplacements::PrecacheReplacementTextures()
{
	std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);

	// predict whether the requests will come with mipmaps
	// TODO: This will be wrong for hw mipmap games like Jak.
	const bool mipmap = GSConfig.HWMipmap >= HWMipmapLevel::Basic ||
						GSConfig.TriFilter == TriFiltering::Forced;

	// pretty simple, just go through the filenames and if any aren't cached, cache them
	for (const auto& it : s_replacement_texture_filenames)
	{
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
	s_replacement_texture_cache.clear();
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
			Host::AddIconOSDMessage("DisablingReplacementAutoGeneratedMipmap", ICON_FA_EXCLAMATION_CIRCLE,
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
	// this holds the lock while doing the upload, but it should be reasonably quick
	std::unique_lock<std::mutex> lock(s_replacement_texture_cache_mutex);
	for (const auto& [name, mipmap] : s_async_loaded_textures)
	{
		// no longer pending!
		const auto pit = s_pending_async_load_textures.find(name);
		if (pit != s_pending_async_load_textures.end())
		{
			const bool cache_only = pit->second;
			s_pending_async_load_textures.erase(pit);

			// if we were precaching, don't inject into the TC if we didn't actually get requested
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
	}
	s_async_loaded_textures.clear();
}

void GSTextureReplacements::DumpTexture(const GSTextureCache::HashCacheKey& hash, const GIFRegTEX0& TEX0,
	const GIFRegTEXA& TEXA, GSTextureCache::SourceRegion region, GSLocalMemory& mem, u32 level)
{
	// check if it's been dumped or replaced already
	const TextureName name(CreateTextureName(hash, level));
	if (s_dumped_textures.find(name) != s_dumped_textures.end() || s_replacement_texture_filenames.find(name) != s_replacement_texture_filenames.end())
		return;

	s_dumped_textures.insert(name);

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
	});
}

void GSTextureReplacements::ClearDumpedTextureList()
{
	s_dumped_textures.clear();
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

void GSTextureReplacements::QueueWorkerThreadItem(std::function<void()> fn)
{
	pxAssert(s_worker_thread.joinable());

	std::unique_lock<std::mutex> lock(s_worker_thread_mutex);
	s_worker_thread_queue.push(std::move(fn));
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

		std::function<void()> fn = std::move(s_worker_thread_queue.front());
		s_worker_thread_queue.pop();
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
		s_worker_thread_queue.pop();
	s_async_loaded_textures.clear();
	s_pending_async_load_textures.clear();
}
