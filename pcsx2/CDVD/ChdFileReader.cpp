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

#include "CDVD/CompressedFileReaderUtils.h"

#include <wx/dir.h>

bool ChdFileReader::CanHandle(const wxString& fileName)
{
	if (!wxFileName::FileExists(fileName) || !fileName.Lower().EndsWith(L".chd"))
	{
		return false;
	}
	return true;
}

bool ChdFileReader::Open2(const wxString& fileName)
{
	Close2();

	m_filename = fileName;

	chd_file* child = NULL;
	chd_file* parent = NULL;
	chd_header header;
	chd_header parent_header;

	wxString chds[8];
	chds[0] = fileName;
	int chd_depth = 0;
	chd_error error;

	// TODO: Unicode correctness on Windows
	while (CHDERR_REQUIRES_PARENT == (error = chd_open(chds[chd_depth].c_str(), CHD_OPEN_READ, NULL, &child)))
	{
		if (chd_depth >= static_cast<int>(std::size(chds) - 1))
		{
			Console.Error(L"CDVD: chd_open hit recursion limit searching for parents");
			return false;
		}
		if (chd_read_header(chds[chd_depth].c_str(), &header) != CHDERR_NONE)
		{
			Console.Error(L"CDVD: chd_open chd_read_header error: %s: %s", chd_error_string(error), WX_STR(chds[chd_depth]));
			return false;
		}
		bool found_parent = false;
		wxFileName wxfilename(chds[chd_depth]);
		wxString dir_path = wxfilename.GetPath();
		wxDir dir(dir_path);
		if (dir.IsOpened())
		{
			wxString parent_fileName;
			bool cont = dir.GetFirst(&parent_fileName, wxString("*.") + wxfilename.GetExt(), wxDIR_FILES | wxDIR_HIDDEN);
			for (; cont; cont = dir.GetNext(&parent_fileName))
			{
				parent_fileName = wxFileName(dir_path, parent_fileName).GetFullPath();
				if (chd_read_header(parent_fileName.c_str(), &parent_header) == CHDERR_NONE &&
					memcmp(parent_header.sha1, header.parentsha1, sizeof(parent_header.sha1)) == 0)
				{
					found_parent = true;
					chds[++chd_depth] = wxString(parent_fileName);
					break;
				}
			}
		}
		if (!found_parent)
		{
			Console.Error(L"CDVD: chd_open no parent for: %s", WX_STR(chds[chd_depth]));
			break;
		}
	}

	if (error != CHDERR_NONE)
	{
		Console.Error(L"CDVD: chd_open return error: %s", chd_error_string(error));
		return false;
	}

	for (int d = chd_depth - 1; d >= 0; d--)
	{
		parent = child;
		child = NULL;
		error = chd_open(chds[d].c_str(), CHD_OPEN_READ, parent, &child);
		if (error != CHDERR_NONE)
		{
			Console.Error(L"CDVD: chd_open return error: %s", chd_error_string(error));
			if (parent)
				chd_close(parent);
			return false;
		}
	}
	ChdFile = child;
	if (chd_read_header(chds[0].c_str(), &header) != CHDERR_NONE)
	{
		Console.Error(L"CDVD: chd_open chd_read_header error: %s: %s", chd_error_string(error), WX_STR(chds[0]));
		return false;
	}

	file_size = static_cast<u64>(header.unitbytes) * header.unitcount;
	hunk_size = header.hunkbytes;
	// CHD likes to use full 2448 byte blocks, but keeps the +24 offset of source ISOs
	// The rest of PCSX2 likes to use 2448 byte buffers, which can't fit that so trim blocks instead
	m_internalBlockSize = header.unitbytes;

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

int ChdFileReader::ReadChunk(void *dst, s64 chunkID)
{
	if (chunkID < 0)
		return -1;

	chd_error error = chd_read(ChdFile, chunkID, dst);
	if (error != CHDERR_NONE)
	{
		Console.Error(L"CDVD: chd_read returned error: %s", chd_error_string(error));
		return 0;
	}

	return hunk_size;
}

void ChdFileReader::Close2()
{
	if (ChdFile != NULL)
	{
		chd_close(ChdFile);
		ChdFile = NULL;
	}
}

u32 ChdFileReader::GetBlockCount() const
{
	return (file_size - m_dataoffset) / m_internalBlockSize;
}
ChdFileReader::ChdFileReader(void)
{
	m_blocksize = 2048;
	ChdFile = NULL;
};
