/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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
#include "ChunksCache.h"
#include "zlib_indexed.h"

static constexpr int GZFILE_SPAN_DEFAULT = (1048576 * 4); /* distance between direct access points when creating a new index */
static constexpr int GZFILE_READ_CHUNK_SIZE = (256 * 1024); /* zlib extraction chunks size (at 0-based boundaries) */
static constexpr int GZFILE_CACHE_SIZE_MB = 200; /* cache size for extracted data. must be at least GZFILE_READ_CHUNK_SIZE (in MB)*/

typedef struct zstate Zstate;

class GzippedFileReader final : public AsyncFileReader
{
	DeclareNoncopyableObject(GzippedFileReader);

public:
	GzippedFileReader();
	~GzippedFileReader();;

	static bool CanHandle(const std::string& fileName, const std::string& displayName);
	bool Open(std::string filename, Error* error) override;

	int ReadSync(void* pBuffer, u32 sector, u32 count) override;

	void BeginRead(void* pBuffer, u32 sector, u32 count) override;
	int FinishRead() override;
	void CancelRead() override;

	void Close() override;

	u32 GetBlockCount() const override;

	void SetBlockSize(u32 bytes) override;
	void SetDataOffset(u32 bytes) override;

private:
	class Czstate
	{
	public:
		Czstate() { state.isValid = 0; };
		~Czstate() { Kill(); };
		void Kill()
		{
			if (state.isValid)
				inflateEnd(&state.strm);
			state.isValid = 0;
		}
		Zstate state;
	};

	bool OkIndex(Error* error); // Verifies that we have an index, or try to create one
	s64 GetOptimalExtractionStart(s64 offset);
	int _ReadSync(void* pBuffer, s64 offset, uint bytesToRead);
	void InitZstates();

	int mBytesRead = 0; // Temp sync read result when simulating async read
	Access* m_pIndex = nullptr; // Quick access index
	Czstate* m_zstates = nullptr;
	FILE* m_src = nullptr;

	ChunksCache m_cache;

#ifdef _WIN32
	// Used by async prefetch
	HANDLE hOverlappedFile = INVALID_HANDLE_VALUE;
	OVERLAPPED asyncOperationContext = {};
	bool asyncInProgress = false;
	char mDummyAsyncPrefetchTarget[GZFILE_READ_CHUNK_SIZE];
#endif

	void AsyncPrefetchReset();
	void AsyncPrefetchOpen();
	void AsyncPrefetchClose();
	void AsyncPrefetchChunk(s64 dummy);
	void AsyncPrefetchCancel();
};
