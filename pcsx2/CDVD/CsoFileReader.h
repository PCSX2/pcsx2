/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

// Based on testing, the overhead of using this cache is high.
//
// The test was done with CSO files using a block size of 16KB.
// Cache hit rates were observed in the range of 25%.
// Cache overhead added 35% to the overall read time.
//
// For this reason, it's currently disabled.
#define CSO_USE_CHUNKSCACHE 0

#include "ThreadedFileReader.h"
#include "ChunksCache.h"
#include <zlib.h>

struct CsoHeader;
typedef struct z_stream_s z_stream;

class CsoFileReader final : public ThreadedFileReader
{
	DeclareNoncopyableObject(CsoFileReader);

public:
	CsoFileReader();
	~CsoFileReader() override;

	static bool CanHandle(const std::string& fileName, const std::string& displayName);
	bool Open2(std::string filename, Error* error) override;

	Chunk ChunkForOffset(u64 offset) override;
	int ReadChunk(void* dst, s64 chunkID) override;

	void Close2() override;

	u32 GetBlockCount() const override;

private:
	static bool ValidateHeader(const CsoHeader& hdr, Error* error);
	bool ReadFileHeader(Error* error);
	bool InitializeBuffers(Error* error);
	int ReadFromFrame(u8* dest, u64 pos, int maxBytes);
	bool DecompressFrame(Bytef* dst, u32 frame, u32 readBufferSize);
	bool DecompressFrame(u32 frame, u32 readBufferSize);

	u32 m_frameSize = 0;
	u8 m_frameShift = 0;
	u8 m_indexShift = 0;
	bool m_uselz4 = false; // flag to enable LZ4 decompression (ZSO files)
	std::unique_ptr<u8[]> m_readBuffer;

	std::unique_ptr<u32[]> m_index;
	u64 m_totalSize = 0;
	// The actual source cso file handle.
	std::FILE* m_src = nullptr;
	std::unique_ptr<z_stream> m_z_stream;
};
