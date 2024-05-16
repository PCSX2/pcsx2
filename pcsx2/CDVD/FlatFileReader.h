// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "CDVD/ThreadedFileReader.h"

#include <cstdio>

class FlatFileReader final : public ThreadedFileReader
{
	DeclareNoncopyableObject(FlatFileReader);

	std::FILE* m_file = nullptr;
	u64 m_file_size = 0;

public:
	FlatFileReader();
	~FlatFileReader() override;

	bool Open2(std::string filename, Error* error) override;

	Chunk ChunkForOffset(u64 offset) override;
	int ReadChunk(void* dst, s64 blockID) override;

	void Close2() override;

	u32 GetBlockCount() const override;
};
