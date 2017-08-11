/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2016  PCSX2 Dev Team
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

#include "AsyncFileReader.h"
#include <lzma.h>

#ifdef ISO_LZMA_READER

class LzmaBlock
{
	lzma_block m_header;
	lzma_filter m_filters[LZMA_FILTERS_MAX + 1];

	u64 m_compressed_offset;
	u64 m_compressed_size;
	u64 m_uncompressed_offset;
	u64 m_uncompressed_size;
	u64 m_block_number;

	std::shared_ptr<wxFile> m_fd;
	bool m_error;

public:
	LzmaBlock(const std::shared_ptr<wxFile>& file, const lzma_index_iter& iter);

	u64 Index() const { return m_block_number; }
	bool IsGood() const { return !m_error; }
	u8* Uncompress();
	void Uncompress(u8* dest);
};

class LzmaFileCache
{
	enum class STATE {
		INVALID,
		VALID,
	};

	wxString m_filename;
	std::unique_ptr<wxFile> m_cache;

	std::vector<STATE> m_state;

	std::vector<u8> m_data;
	u64 m_old_index;

public:
	LzmaFileCache() = delete;
	LzmaFileCache(const wxString& file, int block_nb, int block_size);
	~LzmaFileCache();

	u8* MapBlock(LzmaBlock& block);
};

class LzmaFileReader : public AsyncFileReader
{
	DeclareNoncopyableObject(LzmaFileReader);

	int m_bytes_read;
	lzma_stream m_strm;

	std::unique_ptr<LzmaFileCache> m_cache;
	std::vector<LzmaBlock> m_blocks;
	int m_lzma_blocksize;

	u64 m_uncompressed_file_size;

	lzma_index* GetIndex(const std::shared_ptr<wxFile>& file);

public:
	LzmaFileReader();
	virtual ~LzmaFileReader(void) { Close(); };

	static  bool CanHandle(const wxString& fileName);
	virtual bool Open(const wxString& fileName);

	virtual int ReadSync(void* pBuffer, uint sector, uint count);

	virtual void BeginRead(void* pBuffer, uint sector, uint count);
	virtual int FinishRead(void);
	virtual void CancelRead(void) {}

	virtual void Close(void);

	virtual uint GetBlockCount(void) const {
		return (m_uncompressed_file_size - m_dataoffset) / m_blocksize;
	};

	virtual void SetBlockSize(uint bytes) { m_blocksize = bytes; }
	virtual void SetDataOffset(int bytes) { m_dataoffset = bytes; }
};

#endif
