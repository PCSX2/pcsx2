/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include "CDVD.h"
#include "AsyncFileReader.h"
#include "CompressedFileReader.h"
#include <memory>
#include <string>

enum isoType
{
	ISOTYPE_ILLEGAL = 0,
	ISOTYPE_CD,
	ISOTYPE_DVD,
	ISOTYPE_AUDIO,
	ISOTYPE_DVDDL
};

static const int CD_FRAMESIZE_RAW = 2448;

// --------------------------------------------------------------------------------------
//  isoFile
// --------------------------------------------------------------------------------------
class InputIsoFile
{
	DeclareNoncopyableObject(InputIsoFile);

	static const uint MaxReadUnit = 128;

protected:
	uint ReadUnit;

protected:
	std::string m_filename;
	AsyncFileReader* m_reader;

	u32 m_current_lsn;

	isoType m_type;
	u32 m_flags;

	s32 m_offset;
	s32 m_blockofs;
	u32 m_blocksize;

	// total number of blocks in the ISO image (including all parts)
	u32 m_blocks;

	bool m_read_inprogress;
	uint m_read_lsn;
	uint m_read_count;
	u8 m_readbuffer[MaxReadUnit * CD_FRAMESIZE_RAW];

public:
	InputIsoFile();
	virtual ~InputIsoFile();

	bool IsOpened() const;

	isoType GetType() const { return m_type; }
	uint GetBlockCount() const { return m_blocks; }
	int GetBlockOffset() const { return m_blockofs; }

	const std::string& GetFilename() const
	{
		return m_filename;
	}

	bool Test(std::string srcfile);
	bool Open(std::string srcfile, bool testOnly = false);
	void Close();
	bool Detect(bool readType = true);

	int ReadSync(u8* dst, uint lsn);

	void BeginRead2(uint lsn);
	int FinishRead3(u8* dest, uint mode);

protected:
	void _init();

	bool tryIsoType(u32 _size, s32 _offset, s32 _blockofs);
	void FindParts();
};

class OutputIsoFile
{
	DeclareNoncopyableObject(OutputIsoFile);

protected:
	std::string m_filename;

	u32 m_version;

	s32 m_offset;
	s32 m_blockofs;
	u32 m_blocksize;

	// total number of blocks in the ISO image (including all parts)
	u32 m_blocks;

	// dtable is used when reading blockdumps
	std::vector<u32> m_dtable;

	std::FILE* m_outstream = nullptr;

public:
	OutputIsoFile();
	virtual ~OutputIsoFile();

	bool IsOpened() const;
	u32 GetBlockSize() const;

	const std::string& GetFilename() const
	{
		return m_filename;
	}

	void Create(std::string filename, int mode);
	void Close();

	void WriteHeader(int blockofs, uint blocksize, uint blocks);

	void WriteSector(const u8* src, uint lsn);

protected:
	void _init();

	void WriteBuffer(const void* src, size_t size);

	template <typename T>
	void WriteValue(const T& data)
	{
		WriteBuffer(&data, sizeof(data));
	}
};
