// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/FileSystem.h"
#include "common/Pcsx2Defs.h"

#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

typedef struct _chd_file chd_file;

class ChdHddImage
{
public:
	ChdHddImage();
	~ChdHddImage();

	bool Open(const std::string& path);
	void Close();

	u64 GetSize() const { return m_logical_size; }
	const std::string& GetOverlayPath() const { return m_overlay_path; }

	bool ReadSectors(u64 sector, u32 count, void* dst);
	bool WriteSectors(u64 sector, u32 count, const void* src);

	static std::optional<u64> GetChdLogicalSize(const std::string& path);
	static bool IsChdFileName(const std::string& path);
	static std::optional<u64> GetHddImageLogicalSize(const std::string& path);

private:
	static constexpr u32 SECTOR_SIZE = 512;
	static constexpr u32 OVERLAY_BLOCK_SIZE = 64 * 1024;
	static constexpr u32 HUNK_CACHE_SIZE = 4;

	struct HunkCacheEntry
	{
		u32 hunk = UINT32_MAX;
		u64 last_used = 0;
		std::vector<u8> data;
	};

	struct OverlayMapHeader
	{
		char magic[8];
		u32 version;
		u32 block_size;
		u64 logical_size;
		u64 block_count;
	};

	bool OpenChd(const std::string& path);
	bool OpenOverlay(const std::string& path);
	bool CreateOverlayFiles();
	bool LoadOverlayMap();
	bool InitializeOverlayFile();
	bool ReadBytes(u64 offset, u32 size, u8* dst);
	bool ReadBaseBytes(u64 offset, u32 size, u8* dst);
	bool ReadOverlayBytes(u64 offset, u32 size, u8* dst);
	bool WriteOverlayBytes(u64 offset, u32 size, const u8* src);
	bool ReadOverlayBlock(u64 block_index, u8* dst, u32 block_size);
	bool WriteOverlayBlock(u64 block_index, const u8* src, u32 block_size);
	bool GetCachedHunk(u32 hunk, const u8** data);
	bool IsOverlayBlockPresent(u64 block_index) const;
	bool SetOverlayBlockPresent(u64 block_index);
	u32 GetOverlayBlockSize(u64 block_index) const;
	std::string GetDefaultOverlayBasePath(const std::string& path) const;

	FileSystem::ManagedCFilePtr m_chd_file_handle;
	chd_file* m_chd = nullptr;
	u64 m_logical_size = 0;
	u32 m_hunk_size = 0;
	u32 m_hunk_count = 0;

	std::array<HunkCacheEntry, HUNK_CACHE_SIZE> m_hunk_cache;
	u64 m_hunk_cache_counter = 0;

	std::string m_overlay_path;
	std::string m_overlay_map_path;
	FileSystem::ManagedCFilePtr m_overlay_file;
	FileSystem::ManagedCFilePtr m_overlay_map_file;
	u64 m_overlay_block_count = 0;
	std::vector<u8> m_overlay_map;
	std::vector<u8> m_overlay_block_buffer;
};
