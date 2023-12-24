// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "ThreadedFileReader.h"
#include <vector>

typedef struct _chd_file chd_file;

class ChdFileReader final : public ThreadedFileReader
{
	DeclareNoncopyableObject(ChdFileReader);

public:
	ChdFileReader();
	~ChdFileReader() override;

	bool Open2(std::string filename, Error* error) override;

	Chunk ChunkForOffset(u64 offset) override;
	int ReadChunk(void* dst, s64 blockID) override;

	void Close2(void) override;
	uint GetBlockCount(void) const override;

private:
	bool ParseTOC(u64* out_frame_count);

	chd_file* ChdFile;
	u64 file_size;
	u32 hunk_size;
};
