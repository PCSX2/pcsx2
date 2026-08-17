// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "zip.h"

#include "common/Path.h"
#include "common/StringUtil.h"
#include "Console.h"

static inline std::unique_ptr<zip_t, void (*)(zip_t*)> zip_open_managed(const char* filename, int flags, zip_error_t* ze);
static inline std::unique_ptr<zip_file_t, int (*)(zip_file_t*)> zip_fopen_managed(zip_t* zip, const char* filename, zip_flags_t flags);
static inline std::unique_ptr<zip_file_t, int (*)(zip_file_t*)> zip_fopen_index_managed(zip_t* zip, zip_uint64_t index, zip_flags_t flags);
static inline std::optional<std::vector<u8>> ReadZipEntryBytes(const std::string& archive_path, const std::string& entry_name);
static inline std::optional<std::vector<u8>> ReadBinaryFileInZip(zip_t* zip, const char* name);
static inline std::optional<std::vector<u8>> ReadBinaryFileInZip(zip_file_t* file, u32 chunk_size = 4096);

static inline bool ArchiveEntryMatches(const std::string_view archive_entry_name, const std::string_view wanted_name)
{
	const std::string archive_name(Path::GetFileName(archive_entry_name));
	const std::string normalized_wanted_name(Path::GetFileName(wanted_name));

	if (archive_name.size() == normalized_wanted_name.size() &&
		StringUtil::Strncasecmp(archive_name.c_str(), normalized_wanted_name.c_str(), archive_name.size()) == 0)
	{
		return true;
	}

	return (StringUtil::Strncasecmp(std::string(archive_entry_name).c_str(), std::string(wanted_name).c_str(), std::min(archive_entry_name.size(), wanted_name.size())) == 0) &&
	       (archive_entry_name.size() == wanted_name.size());
}

static inline std::vector<std::string> ListArchiveEntryNames(const std::string& archive_path)
{
	std::vector<std::string> entries;

	zip_error_t zip_error = {};
	auto archive = zip_open_managed(archive_path.c_str(), ZIP_RDONLY, &zip_error);
	if (!archive)
		return entries;

	const zip_uint64_t total_entries = static_cast<zip_uint64_t>(zip_get_num_entries(archive.get(), 0));
	for (zip_uint64_t i = 0; i < total_entries; ++i)
	{
		const char* name = zip_get_name(archive.get(), i, 0);
		if (name)
			entries.emplace_back(name);
	}
	return entries;
}

static inline std::optional<std::vector<u8>> ReadArchiveEntryBytes(const std::string& archive_path, const std::string& entry_name)
{
	return ReadZipEntryBytes(archive_path, entry_name);
}

static inline std::optional<std::vector<u8>> ReadZipEntryBytes(const std::string& archive_path, const std::string& entry_name)
{
	zip_error_t zip_error = {};
	auto archive = zip_open_managed(archive_path.c_str(), ZIP_RDONLY, &zip_error);
	if (!archive)
		return std::nullopt;

	zip_int64_t index = zip_name_locate(archive.get(), entry_name.c_str(), ZIP_FL_NOCASE);
	if (index < 0)
	{
		const std::string_view wanted_name = Path::GetFileName(entry_name);
		const zip_uint64_t total_entries = static_cast<zip_uint64_t>(zip_get_num_entries(archive.get(), 0));
		for (zip_uint64_t i = 0; i < total_entries; ++i)
		{
			const char* name = zip_get_name(archive.get(), i, 0);
			if (!name)
				continue;

			const std::string_view archive_name = Path::GetFileName(name);
			if (archive_name.size() == wanted_name.size() &&
				StringUtil::Strncasecmp(archive_name.data(), wanted_name.data(), archive_name.size()) == 0)
			{
				index = static_cast<zip_int64_t>(i);
				break;
			}
		}
	}

	if (index < 0)
		return std::nullopt;

	zip_stat_t stat = {};
	if (zip_stat_index(archive.get(), index, ZIP_FL_NOCASE, &stat) != 0)
		return std::nullopt;

	auto file = zip_fopen_index_managed(archive.get(), static_cast<zip_uint64_t>(index), ZIP_FL_NOCASE);
	if (!file)
		return std::nullopt;

	auto data = ReadBinaryFileInZip(file.get());
	if (!data.has_value())
		return std::nullopt;

	return data;
}

static inline std::unique_ptr<zip_t, void (*)(zip_t*)> zip_open_managed(const char* filename, int flags, zip_error_t* ze)
{
	zip_source_t* zs = zip_source_file_create(filename, 0, 0, ze);
	zip_t* zip = nullptr;
	if (zs && !(zip = zip_open_from_source(zs, flags, ze)))
	{
		// have to clean up source
		zip_source_free(zs);
	}

	return std::unique_ptr<zip_t, void (*)(zip_t*)>(zip, [](zip_t* zf) {
		if (!zf)
			return;

		int err = zip_close(zf);
		if (err != 0)
		{
			Console.Error("Failed to close zip file: %d", err);
			zip_discard(zf);
		}
	});
}

static inline std::unique_ptr<zip_file_t, int (*)(zip_file_t*)> zip_fopen_managed(zip_t* zip, const char* filename, zip_flags_t flags)
{
	return std::unique_ptr<zip_file_t, int (*)(zip_file_t*)>(zip_fopen(zip, filename, flags), zip_fclose);
}

static inline std::unique_ptr<zip_file_t, int (*)(zip_file_t*)> zip_fopen_index_managed(zip_t* zip, zip_uint64_t index, zip_flags_t flags)
{
	return std::unique_ptr<zip_file_t, int (*)(zip_file_t*)>(zip_fopen_index(zip, index, flags), zip_fclose);
}

template<typename T>
static inline std::optional<T> ReadFileInZipToContainer(zip_t* zip, const char* name)
{
	std::optional<T> ret;
	const zip_int64_t file_index = zip_name_locate(zip, name, ZIP_FL_NOCASE);
	if (file_index >= 0)
	{
		zip_stat_t zst;
		if (zip_stat_index(zip, file_index, ZIP_FL_NOCASE, &zst) == 0)
		{
			zip_file_t* zf = zip_fopen_index(zip, file_index, ZIP_FL_NOCASE);
			if (zf)
			{
				ret = T();
				ret->resize(static_cast<size_t>(zst.size));
				if (zip_fread(zf, ret->data(), ret->size()) != static_cast<zip_int64_t>(ret->size()))
				{
					ret.reset();
				}
			}
		}
	}

	return ret;
}


template <typename T>
static inline std::optional<T> ReadFileInZipToContainer(zip_file_t* file, u32 chunk_size = 4096)
{
	std::optional<T> ret = T();
	for (;;)
	{
		const size_t pos = ret->size();
		ret->resize(pos + chunk_size);
		const s64 read = zip_fread(file, ret->data() + pos, chunk_size);
		if (read < 0)
		{
			// read error
			ret.reset();
			break;
		}

		// if less than chunk size, we're EOF
		if (read != static_cast<s64>(chunk_size))
		{
			ret->resize(pos + static_cast<size_t>(read));
			break;
		}
	}

	return ret;
}


static inline std::optional<std::string> ReadFileInZipToString(zip_t* zip, const char* name)
{
	return ReadFileInZipToContainer<std::string>(zip, name);
}

static inline std::optional<std::string> ReadFileInZipToString(zip_file_t* file, u32 chunk_size = 4096)
{
	return ReadFileInZipToContainer<std::string>(file, chunk_size);
}

static inline std::optional<std::vector<u8>> ReadBinaryFileInZip(zip_t* zip, const char* name)
{
	return ReadFileInZipToContainer<std::vector<u8>>(zip, name);
}

static inline std::optional<std::vector<u8>> ReadBinaryFileInZip(zip_file_t* file, u32 chunk_size)
{
	return ReadFileInZipToContainer<std::vector<u8>>(file, chunk_size);
}