// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "AsyncFileReader.h"
#include "IsoFileFormats.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Error.h"

#include <cerrno>
#include <cstring>

enum isoFlags
{
	ISOFLAGS_BLOCKDUMP_V2 = 0x0004,
	ISOFLAGS_BLOCKDUMP_V3 = 0x0020
};

static constexpr u32 BlockDumpHeaderSize = 16;

bool BlockdumpFileReader::DetectBlockdump(AsyncFileReader* reader)
{
	u32 oldbs = reader->GetBlockSize();

	reader->SetBlockSize(1);

	char buf[4] = {0};
	bool isbd = (reader->ReadSync(buf, 0, sizeof(buf)) == 4
	          && std::memcmp(buf, "BDV2", sizeof(buf)) == 0);

	if (!isbd)
		reader->SetBlockSize(oldbs);

	return isbd;
}

BlockdumpFileReader::BlockdumpFileReader()
	: m_file(NULL)
	, m_blocks(0)
	, m_blockofs(0)
	, m_dtablesize(0)
	, m_lresult(0)
{
}

BlockdumpFileReader::~BlockdumpFileReader()
{
	Close();
}

bool BlockdumpFileReader::Open(std::string filename, Error* error)
{
	char signature[4];

	m_filename = std::move(filename);
	if (!(m_file = FileSystem::OpenCFile(m_filename.c_str(), "rb", error)))
		return false;

	if (std::fread(signature, sizeof(signature), 1, m_file) != 1 || std::memcmp(signature, "BDV2", sizeof(signature)) != 0)
	{
		Error::SetString(error, "Block dump signature is invalid.");
		return false;
	}

	//m_flags = ISOFLAGS_BLOCKDUMP_V2;
	if (std::fread(&m_blocksize, sizeof(m_blocksize), 1, m_file) != 1
	 || std::fread(&m_blocks,    sizeof(m_blocks),    1, m_file) != 1
	 || std::fread(&m_blockofs,  sizeof(m_blockofs),  1, m_file) != 1)
	{
		Error::SetString(error, "Failed to read block dump information.");
		return false;
	}

	const s64 flen = FileSystem::FSize64(m_file);
	const s64 datalen = flen - BlockDumpHeaderSize;

	pxAssert((datalen % (m_blocksize + 4)) == 0);

	m_dtablesize = datalen / (m_blocksize + 4);
	m_dtable = std::make_unique<u32[]>(m_dtablesize);

	if (FileSystem::FSeek64(m_file, BlockDumpHeaderSize, SEEK_SET) != 0)
	{
		Error::SetString(error, "Failed to seek to block dump data.");
		return false;
	}

	u32 bs = 1024 * 1024;
	u32 off = 0;
	u32 has = 0;
	int i = 0;

	std::unique_ptr<u8[]> buffer = std::make_unique<u8[]>(bs);
	do
	{
		has = static_cast<u32>(std::fread(buffer.get(), 1, bs, m_file));
		while (i < m_dtablesize && off < has)
		{
			m_dtable[i++] = *reinterpret_cast<u32*>(buffer.get() + off);
			off += 4;
			off += m_blocksize;
		}

		off -= has;

	} while (has == bs);

	return true;
}

int BlockdumpFileReader::ReadSync(void* pBuffer, u32 lsn, u32 count)
{
	u8* dst = (u8*)pBuffer;
	//	Console.WriteLn("_isoReadBlockD %u, blocksize=%u, blockofs=%u\n", lsn, iso->blocksize, iso->blockofs);

	while (count > 0)
	{
		bool ok = false;
		for (int i = 0; i < m_dtablesize; ++i)
		{
			if (m_dtable[i] != lsn)
				continue;

				// We store the LSN (u32) along with each block inside of blockdumps, so the
				// seek position ends up being based on (m_blocksize + 4) instead of just m_blocksize.

#ifdef PCSX2_DEBUG
			u32 check_lsn = 0;
			FileSystem::FSeek64(m_file, BlockDumpHeaderSize + (i * (m_blocksize + 4)), SEEK_SET);
			std::fread(&check_lsn, sizeof(check_lsn), 1, m_file);
			pxAssert(check_lsn == lsn);
#else
			if (FileSystem::FSeek64(m_file, BlockDumpHeaderSize + (i * (m_blocksize + 4)) + 4, SEEK_SET) != 0)
				break;
#endif

			if (std::fread(dst, m_blocksize, 1, m_file) != 1)
				break;

			ok = true;
			break;
		}

		if (!ok)
		{
			Console.WriteLn("Block %u not found in dump", lsn);
			return -1;
		}

		count--;
		lsn++;
		dst += m_blocksize;
	}

	return 0;
}

void BlockdumpFileReader::BeginRead(void* pBuffer, u32 sector, u32 count)
{
	m_lresult = ReadSync(pBuffer, sector, count);
}

int BlockdumpFileReader::FinishRead()
{
	return m_lresult;
}

void BlockdumpFileReader::CancelRead()
{
}

void BlockdumpFileReader::Close(void)
{
	if (m_file)
	{
		std::fclose(m_file);
		m_file = nullptr;
	}
}

u32 BlockdumpFileReader::GetBlockCount() const
{
	return m_blocks;
}
