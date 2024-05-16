// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "CDVD/ThreadedFileReader.h"

#include <cstdio>

class BlockdumpFileReader final : public ThreadedFileReader
{
	DeclareNoncopyableObject(BlockdumpFileReader);

	std::FILE* m_file = nullptr;

	// total number of blocks in the ISO image (including all parts)
	u32 m_dblocksize = 0;
	u32 m_blocks = 0;
	s32 m_blockofs = 0;

	// index table
	std::unique_ptr<u32[]> m_dtable;
	int m_dtablesize = 0;

public:
	BlockdumpFileReader();
	~BlockdumpFileReader() override;

	bool Open2(std::string filename, Error* error) override;

	Chunk ChunkForOffset(u64 offset) override;
	int ReadChunk(void* dst, s64 blockID) override;

	void Close2() override;

	u32 GetBlockCount() const override;

	s32 GetBlockOffset() { return m_blockofs; }
};
