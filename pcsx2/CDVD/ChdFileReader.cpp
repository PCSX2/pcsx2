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
#include "ChdFileReader.h"

#include "common/Assertions.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
#include "libchdr/chd.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

ChdFileReader::ChdFileReader()
{
	m_blocksize = 2048;
	ChdFile = nullptr;
}

ChdFileReader::~ChdFileReader()
{
	Close();

	for (std::FILE* fp : m_files)
		std::fclose(fp);
}

bool ChdFileReader::CanHandle(const std::string& fileName, const std::string& displayName)
{
	if (!StringUtil::EndsWith(displayName, ".chd"))
		return false;

	return true;
}

static chd_error chd_open_wrapper(const char* filename, std::FILE** fp, int mode, chd_file* parent, chd_file** chd)
{
	*fp = FileSystem::OpenCFile(filename, "rb");
	if (!*fp)
		return CHDERR_FILE_NOT_FOUND;

	const chd_error err = chd_open_file(*fp, mode, parent, chd);
	if (err == CHDERR_NONE)
		return err;

	std::fclose(*fp);
	*fp = nullptr;
	return err;
}

bool ChdFileReader::Open2(std::string fileName)
{
	Close2();

	m_filename = std::move(fileName);

	chd_file* child = nullptr;
	chd_file* parent = nullptr;
	std::FILE* fp = nullptr;
	chd_header header;
	chd_header parent_header;

	std::string chds[8];
	chds[0] = m_filename;
	int chd_depth = 0;
	chd_error error;

	std::string dirname;
	FileSystem::FindResultsArray results;

	while (CHDERR_REQUIRES_PARENT == (error = chd_open_wrapper(chds[chd_depth].c_str(), &fp, CHD_OPEN_READ, nullptr, &child)))
	{
		if (chd_depth >= static_cast<int>(std::size(chds) - 1))
		{
			Console.Error("CDVD: chd_open hit recursion limit searching for parents");
			return false;
		}

		// TODO: This is still broken on Windows. Needs to be fixed in libchdr.
		if (chd_read_header(chds[chd_depth].c_str(), &header) != CHDERR_NONE)
		{
			Console.Error("CDVD: chd_open chd_read_header error: %s: %s", chd_error_string(error), chds[chd_depth].c_str());
			return false;
		}

		bool found_parent = false;
		dirname = Path::GetDirectory(chds[chd_depth]);
		if (FileSystem::FindFiles(dirname.c_str(), "*.*", FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES, &results))
		{
			for (const FILESYSTEM_FIND_DATA& fd : results)
			{
				const std::string_view extension(Path::GetExtension(fd.FileName));
				if (extension.empty() || StringUtil::Strncasecmp(extension.data(), "chd", 3) != 0)
					continue;

				if (chd_read_header(fd.FileName.c_str(), &parent_header) == CHDERR_NONE &&
					memcmp(parent_header.sha1, header.parentsha1, sizeof(parent_header.sha1)) == 0)
				{
					found_parent = true;
					chds[++chd_depth] = std::move(fd.FileName);
					break;
				}
			}
		}

		if (!found_parent)
		{
			Console.Error("CDVD: chd_open no parent for: %s", chds[chd_depth].c_str());
			break;
		}
	}

	if (error != CHDERR_NONE)
	{
		Console.Error("CDVD: chd_open return error: %s", chd_error_string(error));
		return false;
	}

	if (child)
	{
		pxAssert(fp != nullptr);
		m_files.push_back(fp);
	}

	for (int d = chd_depth - 1; d >= 0; d--)
	{
		parent = child;
		child = nullptr;
		error = chd_open_wrapper(chds[d].c_str(), &fp, CHD_OPEN_READ, parent, &child);
		if (error != CHDERR_NONE)
		{
			Console.Error("CDVD: chd_open return error: %s", chd_error_string(error));
			if (parent)
				chd_close(parent);
			return false;
		}

		m_files.push_back(fp);
	}
	ChdFile = child;

	const chd_header* chd_header = chd_get_header(ChdFile);
	hunk_size = chd_header->hunkbytes;
	// CHD likes to use full 2448 byte blocks, but keeps the +24 offset of source ISOs
	// The rest of PCSX2 likes to use 2448 byte buffers, which can't fit that so trim blocks instead
	m_internalBlockSize = chd_header->unitbytes;

	// The file size in the header is incorrect, each track gets padded to a multiple of 4 frames.
	// (see chdman.cpp from MAME). Instead, we pull the real frame count from the TOC.
	u64 total_frames;
	if (ParseTOC(&total_frames))
	{
		file_size = total_frames * static_cast<u64>(chd_header->unitbytes);
	}
	else
	{
		Console.Warning("Failed to parse CHD TOC, file size may be incorrect.");
		file_size = static_cast<u64>(chd_header->unitbytes) * chd_header->unitcount;
	}

	return true;
}

ThreadedFileReader::Chunk ChdFileReader::ChunkForOffset(u64 offset)
{
	Chunk chunk = {0};
	if (offset >= file_size)
	{
		chunk.chunkID = -1;
	}
	else
	{
		chunk.chunkID = offset / hunk_size;
		chunk.length = hunk_size;
		chunk.offset = chunk.chunkID * hunk_size;
	}
	return chunk;
}

int ChdFileReader::ReadChunk(void* dst, s64 chunkID)
{
	if (chunkID < 0)
		return -1;

	chd_error error = chd_read(ChdFile, chunkID, dst);
	if (error != CHDERR_NONE)
	{
		Console.Error("CDVD: chd_read returned error: %s", chd_error_string(error));
		return 0;
	}

	return hunk_size;
}

void ChdFileReader::Close2()
{
	if (ChdFile)
	{
		chd_close(ChdFile);
		ChdFile = nullptr;
	}
}

u32 ChdFileReader::GetBlockCount() const
{
	return (file_size - m_dataoffset) / m_internalBlockSize;
}

bool ChdFileReader::ParseTOC(u64* out_frame_count)
{
	u64 total_frames = 0;
	int max_found_track = -1;

	for (int search_index = 0;; search_index++)
	{
		char metadata_str[256];
		char type_str[256];
		char subtype_str[256];
		char pgtype_str[256];
		char pgsub_str[256];
		u32 metadata_length;

		int track_num = 0, frames = 0, pregap_frames = 0, postgap_frames = 0;
		chd_error err = chd_get_metadata(ChdFile, CDROM_TRACK_METADATA2_TAG, search_index, metadata_str, sizeof(metadata_str),
			&metadata_length, nullptr, nullptr);
		if (err == CHDERR_NONE)
		{
			if (std::sscanf(metadata_str, CDROM_TRACK_METADATA2_FORMAT, &track_num, type_str, subtype_str, &frames,
				&pregap_frames, pgtype_str, pgsub_str, &postgap_frames) != 8)
			{
				Console.Error(fmt::format("Invalid track v2 metadata: '{}'", metadata_str));
				return false;
			}
		}
		else
		{
			// try old version
			err = chd_get_metadata(ChdFile, CDROM_TRACK_METADATA_TAG, search_index, metadata_str, sizeof(metadata_str),
				&metadata_length, nullptr, nullptr);
			if (err != CHDERR_NONE)
			{
				// not found, so no more tracks
				break;
			}

			if (std::sscanf(metadata_str, CDROM_TRACK_METADATA_FORMAT, &track_num, type_str, subtype_str, &frames) != 4)
			{
				Console.Error(fmt::format("Invalid track metadata: '{}'", metadata_str));
				return false;
			}
		}

		DevCon.WriteLn(fmt::format("CHD Track {}: frames:{} pregap:{} postgap:{} type:{} sub:{} pgtype:{} pgsub:{}",
			track_num, frames, pregap_frames, postgap_frames, type_str, subtype_str, pgtype_str, pgsub_str));

		// PCSX2 doesn't currently support multiple tracks for CDs.
		if (track_num != 1)
		{
			Console.Warning(fmt::format("  Ignoring track {} in CHD.", track_num, frames));
			continue;
		}

		total_frames += static_cast<u64>(pregap_frames) + static_cast<u64>(frames) + static_cast<u64>(postgap_frames);
		max_found_track = std::max(max_found_track, track_num);
	}

	// No tracks in TOC?
	if (max_found_track < 0)
		return false;

	// Compute total data size.
	*out_frame_count = total_frames;
	return true;
}
