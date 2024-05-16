// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "BlockdumpFileReader.h"
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

BlockdumpFileReader::BlockdumpFileReader() = default;

BlockdumpFileReader::~BlockdumpFileReader()
{
	pxAssert(!m_file);
}

bool BlockdumpFileReader::Open2(std::string filename, Error* error)
{
	char signature[4];

	m_filename = std::move(filename);
	if (!(m_file = FileSystem::OpenCFile(m_filename.c_str(), "rb", error)))
		return false;

	if (std::fread(signature, sizeof(signature), 1, m_file) != 1 || std::memcmp(signature, "BDV2", sizeof(signature)) != 0)
	{
		Error::SetStringView(error, "Block dump signature is invalid.");
		return false;
	}

	//m_flags = ISOFLAGS_BLOCKDUMP_V2;
	if (std::fread(&m_dblocksize, sizeof(m_dblocksize), 1, m_file) != 1 ||
		std::fread(&m_blocks, sizeof(m_blocks), 1, m_file) != 1 ||
		std::fread(&m_blockofs, sizeof(m_blockofs), 1, m_file) != 1)
	{
		Error::SetStringView(error, "Failed to read block dump information.");
		return false;
	}

	m_blocksize = m_dblocksize;

	const s64 flen = FileSystem::FSize64(m_file);
	const s64 datalen = flen - BlockDumpHeaderSize;

	pxAssert((datalen % (m_dblocksize + 4)) == 0);

	m_dtablesize = datalen / (m_dblocksize + 4);
	m_dtable = std::make_unique_for_overwrite<u32[]>(m_dtablesize);

	if (FileSystem::FSeek64(m_file, BlockDumpHeaderSize, SEEK_SET) != 0)
	{
		Error::SetStringView(error, "Failed to seek to block dump data.");
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
			off += m_dblocksize;
		}

		off -= has;

	} while (has == bs);

	return true;
}

ThreadedFileReader::Chunk BlockdumpFileReader::ChunkForOffset(u64 offset)
{
	Chunk chunk = {};
	chunk.chunkID = offset / m_dblocksize;
	chunk.length = m_dblocksize;
	chunk.offset = chunk.chunkID * m_dblocksize;
	return chunk;
}

int BlockdumpFileReader::ReadChunk(void* dst, s64 blockID)
{
	pxAssert(blockID >= 0 && blockID < static_cast<s64>(m_blocks));
	const u32 lsn = static_cast<u32>(blockID);
	//	Console.WriteLn("_isoReadBlockD %u, blocksize=%u, blockofs=%u\n", static_cast<u32>(blockID), iso->blocksize, iso->blockofs);

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
			return 0;
#endif

		if (std::fread(dst, m_blocksize, 1, m_file) != 1)
			return 0;
		else
			return m_blocksize;
	}

	// Either we hit a sector that's not in the dump, and needed, or the threaded reader is just reading ahead.
	return -1;
}

void BlockdumpFileReader::Close2()
{
	if (!m_file)
		return;

	std::fclose(m_file);
	m_file = nullptr;
	m_dtable.reset();
	m_dtablesize = 0;
	m_dblocksize = 0;
	m_blocks = 0;
}

u32 BlockdumpFileReader::GetBlockCount() const
{
	return m_blocks;
}
