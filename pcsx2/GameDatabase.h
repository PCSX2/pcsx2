// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"
#include "Patch.h"

#include "common/FPControl.h"

#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

enum GamefixId;

namespace GameDatabaseSchema
{
	enum class Compatibility
	{
		Unknown = 0,
		Nothing,
		Intro,
		Menu,
		InGame,
		Playable,
		Perfect
	};

	enum class ClampMode
	{
		Undefined = -1,
		Disabled = 0,
		Normal,
		Extra,
		Full,
		Count
	};

	enum class GSHWFixId : u32
	{
		// boolean settings
		AutoFlush,
		CPUFramebufferConversion,
		FlushTCOnClose,
		DisableDepthSupport,
		PreloadFrameData,
		DisablePartialInvalidation,
		TextureInsideRT,
		AlignSprite,
		MergeSprite,
		Mipmap,
		ForceEvenSpritePosition,
		BilinearUpscale,
		NativePaletteDraw,
		EstimateTextureRegion,
		PCRTCOffsets,
		PCRTCOverscan,

		// integer settings
		TrilinearFiltering,
		SkipDrawStart,
		SkipDrawEnd,
		HalfBottomOverride,
		HalfPixelOffset,
		RoundSprite,
		NativeScaling,
		TexturePreloading,
		Deinterlace,
		CPUSpriteRenderBW,
		CPUSpriteRenderLevel,
		CPUCLUTRender,
		GPUTargetCLUT,
		GPUPaletteConversion,
		MinimumBlendingLevel,
		MaximumBlendingLevel,
		RecommendedBlendingLevel,
		GetSkipCount,
		BeforeDraw,
		MoveHandler,

		Count
	};

	struct GameEntry
	{
		std::string name;
		std::string name_sort;
		std::string name_en;
		std::string region;
		Compatibility compat = Compatibility::Unknown;
		FPRoundMode eeRoundMode = FPRoundMode::MaxCount;
		FPRoundMode eeDivRoundMode = FPRoundMode::MaxCount;
		FPRoundMode vu0RoundMode = FPRoundMode::MaxCount;
		FPRoundMode vu1RoundMode = FPRoundMode::MaxCount;
		ClampMode eeClampMode = ClampMode::Undefined;
		ClampMode vu0ClampMode = ClampMode::Undefined;
		ClampMode vu1ClampMode = ClampMode::Undefined;
		std::vector<GamefixId> gameFixes;
		std::vector<std::pair<SpeedHack, int>> speedHacks;
		std::vector<std::pair<GSHWFixId, s32>> gsHWFixes;
		std::vector<std::string> memcardFilters;
		std::unordered_map<u32, std::string> patches;
		std::vector<Patch::DynamicPatch> dynaPatches;

		// Returns the list of memory card serials as a `/` delimited string
		std::string memcardFiltersAsString() const;
		const std::string* findPatch(u32 crc) const;
		const char* compatAsString() const;

		/// Applies Core game fixes to an existing config.
		void applyGameFixes(Pcsx2Config& config, bool applyAuto) const;

		/// Applies GS hardware fixes to an existing config.
		void applyGSHardwareFixes(Pcsx2Config::GSOptions& config) const;

		/// Returns true if the current config value for the specified hw fix id matches the value.
		static bool configMatchesHWFix(const Pcsx2Config::GSOptions& config, GSHWFixId id, int value);
	};
}; // namespace GameDatabaseSchema

namespace GameDatabase
{
	void ensureLoaded();
	const GameDatabaseSchema::GameEntry* findGame(const std::string_view serial);

	struct TrackHash
	{
		static constexpr u32 SIZE = 16;

		bool parseHash(const std::string_view str);
		std::string toString() const;

#define MAKE_OPERATOR(op) \
	bool operator op(const TrackHash& hash) const { return (std::memcmp(data, hash.data, sizeof(data)) op 0); }
		MAKE_OPERATOR(==);
		MAKE_OPERATOR(!=);
		MAKE_OPERATOR(<);
		MAKE_OPERATOR(<=);
		MAKE_OPERATOR(>);
		MAKE_OPERATOR(>=);
#undef MAKE_OPERATOR

		u8 data[SIZE];
		u64 size;
	};

	struct HashDatabaseEntry
	{
		std::string serial;
		std::string name;
		std::string version;
		std::vector<TrackHash> tracks;
	};

	bool loadHashDatabase();
	void unloadHashDatabase();
	const HashDatabaseEntry* lookupHash(const TrackHash* tracks, size_t num_tracks, bool* tracks_matched, std::string* match_error);
}; // namespace GameDatabase
