// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "CDVD.h"
#include "AsyncFileReader.h"
#include "CompressedFileReader.h"
#include <memory>
#include <string>
#include <vector>

class Error;

enum isoType
{
	ISOTYPE_ILLEGAL = 0,
	ISOTYPE_CD,
	ISOTYPE_DVD,
	ISOTYPE_AUDIO,
	ISOTYPE_DVDDL
};

static constexpr int CD_FRAMESIZE_RAW = 2448;

// --------------------------------------------------------------------------------------
//  isoFile
// --------------------------------------------------------------------------------------
class InputIsoFile final
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
	~InputIsoFile();

	bool IsOpened() const;

	isoType GetType() const noexcept { return m_type; }
	uint GetBlockCount() const noexcept { return m_blocks; }
	int GetBlockOffset() const  noexcept { return m_blockofs; }

	const std::string& GetFilename() const
	{
		return m_filename;
	}

	bool Test(std::string srcfile);
	bool Open(std::string srcfile, Error* error, bool testOnly);
	void Close();
	bool Detect(bool readType = true);

	int ReadSync(u8* dst, uint lsn);

	void BeginRead2(uint lsn);
	int FinishRead3(u8* dest, uint mode);

protected:
	void _init();

	bool tryIsoType(u32 size, u32 offset, u32 blockofs);
	void FindParts();
};

class OutputIsoFile final
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
	~OutputIsoFile();

	bool IsOpened() const;
	u32 GetBlockSize() const;

	const std::string& GetFilename() const noexcept
	{
		return m_filename;
	}

	bool Create(std::string filename, int mode);
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
