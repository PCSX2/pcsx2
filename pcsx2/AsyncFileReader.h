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

#ifdef _WIN32
#	include <Windows.h>
#	undef Yield
#elif defined(__linux__)
#	include <libaio.h>
#elif defined(__POSIX__)
#	include <aio.h>
#endif
#include <memory>

class AsyncFileReader
{
protected:
	AsyncFileReader() : m_dataoffset(0), m_blocksize(0) {}

	wxString m_filename;

	int m_dataoffset;
	uint m_blocksize;

public:
	virtual ~AsyncFileReader(void) {};

	virtual bool Open(const wxString& fileName)=0;

	virtual int ReadSync(void* pBuffer, uint sector, uint count)=0;

	virtual void BeginRead(void* pBuffer, uint sector, uint count)=0;
	virtual int FinishRead(void)=0;
	virtual void CancelRead(void)=0;

	virtual void Close(void)=0;

	virtual uint GetBlockCount(void) const=0;

	virtual void SetBlockSize(uint bytes) {}
	virtual void SetDataOffset(int bytes) {}

	uint GetBlockSize() const { return m_blocksize; }

	const wxString& GetFilename() const
	{
		return m_filename;
	}
};

class FlatFileReader : public AsyncFileReader
{
	DeclareNoncopyableObject( FlatFileReader );

#ifdef _WIN32
	HANDLE hOverlappedFile;

	OVERLAPPED asyncOperationContext;

	HANDLE hEvent;

	bool asyncInProgress;
#elif defined(__linux__)
	int m_fd; // FIXME don't know if overlap as an equivalent on linux
	io_context_t m_aio_context;
#elif defined(__POSIX__)
	int m_fd; // TODO OSX don't know if overlap as an equivalent on OSX
	struct aiocb m_aiocb;
	bool m_async_read_in_progress;
#endif

	bool shareWrite;

public:
	FlatFileReader(bool shareWrite = false);
	virtual ~FlatFileReader(void);

	virtual bool Open(const wxString& fileName);

	virtual int ReadSync(void* pBuffer, uint sector, uint count);

	virtual void BeginRead(void* pBuffer, uint sector, uint count);
	virtual int FinishRead(void);
	virtual void CancelRead(void);

	virtual void Close(void);

	virtual uint GetBlockCount(void) const;

	virtual void SetBlockSize(uint bytes) { m_blocksize = bytes; }
	virtual void SetDataOffset(int bytes) { m_dataoffset = bytes; }
};

class MultipartFileReader : public AsyncFileReader
{
	DeclareNoncopyableObject( MultipartFileReader );

	static const int MaxParts = 8;

	struct Part {
		uint start;
		uint end; // exclusive
		bool isReading;
		AsyncFileReader* reader;
	} m_parts[MaxParts];
	uint m_numparts;

	uint GetFirstPart(uint lsn);
	void FindParts();

public:
	MultipartFileReader(AsyncFileReader* firstPart);
	virtual ~MultipartFileReader(void);

	virtual bool Open(const wxString& fileName);

	virtual int ReadSync(void* pBuffer, uint sector, uint count);

	virtual void BeginRead(void* pBuffer, uint sector, uint count);
	virtual int FinishRead(void);
	virtual void CancelRead(void);

	virtual void Close(void);

	virtual uint GetBlockCount(void) const;

	virtual void SetBlockSize(uint bytes);

	static AsyncFileReader* DetectMultipart(AsyncFileReader* reader);
};

class BlockdumpFileReader : public AsyncFileReader
{
	DeclareNoncopyableObject( BlockdumpFileReader );

	wxFileInputStream* m_file;

	// total number of blocks in the ISO image (including all parts)
	u32 m_blocks;
	s32 m_blockofs;

	// index table
	std::unique_ptr<u32[]> m_dtable;
	int m_dtablesize;

	int m_lresult;

public:
	BlockdumpFileReader(void);
	virtual ~BlockdumpFileReader(void);

	virtual bool Open(const wxString& fileName);

	virtual int ReadSync(void* pBuffer, uint sector, uint count);

	virtual void BeginRead(void* pBuffer, uint sector, uint count);
	virtual int FinishRead(void);
	virtual void CancelRead(void);

	virtual void Close(void);

	virtual uint GetBlockCount(void) const;

	static bool DetectBlockdump(AsyncFileReader* reader);

	int GetBlockOffset() { return m_blockofs; }
};
