// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "zip.h"

#include "Console.h"

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

static inline std::unique_ptr<zip_t, void (*)(zip_t*)> zip_open_buffer_managed(const void* buffer, size_t size, int flags, int freep, zip_error_t* ze)
{
	zip_source_t* zs = zip_source_buffer_create(buffer, size, freep, ze);
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

static inline std::optional<std::vector<u8>> ReadBinaryFileInZip(zip_file_t* file, u32 chunk_size = 4096)
{
	return ReadFileInZipToContainer<std::vector<u8>>(file, chunk_size);
}