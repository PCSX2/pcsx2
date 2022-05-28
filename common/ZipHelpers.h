/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

static inline std::optional<std::string> ReadFileInZipToString(zip_t* zip, const char* name)
{
	return ReadFileInZipToContainer<std::string>(zip, name);
}

static inline std::optional<std::vector<u8>> ReadBinaryFileInZip(zip_t* zip, const char* name)
{
	return ReadFileInZipToContainer<std::vector<u8>>(zip, name);
}
