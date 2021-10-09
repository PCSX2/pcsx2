/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2014  PCSX2 Dev Team
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

typedef struct zstate Zstate;

#include "AsyncFileReader.h"
#include "ChunksCache.h"
#include "zlib_indexed.h"

#define GZFILE_SPAN_DEFAULT (1048576L * 4)  /* distance between direct access points when creating a new index */
#define GZFILE_READ_CHUNK_SIZE (256 * 1024) /* zlib extraction chunks size (at 0-based boundaries) */
#define GZFILE_CACHE_SIZE_MB 200            /* cache size for extracted data. must be at least GZFILE_READ_CHUNK_SIZE (in MB)*/

class GzippedFileReader : public AsyncFileReader
{
	DeclareNoncopyableObject(GzippedFileReader);

public:
	GzippedFileReader(void);

	virtual ~GzippedFileReader(void) { Close(); };

	static bool CanHandle(const std::string& fileName, const std::string& displayName);
	virtual bool Open(std::string fileName);

	virtual int ReadSync(void* pBuffer, uint sector, uint count);

	virtual void BeginRead(void* pBuffer, uint sector, uint count);
	virtual int FinishRead(void);
	virtual void CancelRead(void){};

	virtual void Close(void);

	virtual uint GetBlockCount(void) const
	{
		// type and formula copied from FlatFileReader
		// FIXME? : Shouldn't it be uint and (size - m_dataoffset) / m_blocksize ?
		return (int)((m_pIndex ? m_pIndex->uncompressed_size : 0) / m_blocksize);
	};

	virtual void SetBlockSize(uint bytes) { m_blocksize = bytes; }
	virtual void SetDataOffset(int bytes) { m_dataoffset = bytes; }

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

	bool OkIndex(); // Verifies that we have an index, or try to create one
	s64 GetOptimalExtractionStart(s64 offset);
	int _ReadSync(void* pBuffer, s64 offset, uint bytesToRead);
	void InitZstates();

	int mBytesRead;   // Temp sync read result when simulating async read
	Access* m_pIndex; // Quick access index
	Czstate* m_zstates;
	FILE* m_src;

	ChunksCache m_cache;

#ifdef _WIN32
	// Used by async prefetch
	HANDLE hOverlappedFile;
	OVERLAPPED asyncOperationContext;
	bool asyncInProgress;
	char mDummyAsyncPrefetchTarget[GZFILE_READ_CHUNK_SIZE];
#endif

	void AsyncPrefetchReset();
	void AsyncPrefetchOpen();
	void AsyncPrefetchClose();
	void AsyncPrefetchChunk(s64 dummy);
	void AsyncPrefetchCancel();
};
