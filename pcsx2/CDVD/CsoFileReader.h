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

#include "AsyncFileReader.h"

struct CsoHeader;
typedef struct z_stream_s z_stream;

class CsoFileReader : public AsyncFileReader
{
	DeclareNoncopyableObject(CsoFileReader);
public:
	CsoFileReader(void) :
		m_readBuffer(0),
		m_zlibBuffer(0),
		m_index(0),
		m_totalSize(0),
		m_src(0),
		m_z_stream(0),
		m_bytesRead(0) {
		m_blocksize = 2048;
	};

	virtual ~CsoFileReader(void) { Close(); };

	static  bool CanHandle(const wxString& fileName);
	virtual bool Open(const wxString& fileName);

	virtual int ReadSync(void* pBuffer, uint sector, uint count);

	virtual void BeginRead(void* pBuffer, uint sector, uint count);
	virtual int FinishRead(void);
	virtual void CancelRead(void);

	virtual void Close(void);

	virtual uint GetBlockCount(void) const {
		return (m_totalSize - m_dataoffset) / m_blocksize;
	};

	virtual void SetBlockSize(uint bytes) { m_blocksize = bytes; }
	virtual void SetDataOffset(int bytes) { m_dataoffset = bytes; }

private:
	static bool ValidateHeader(const CsoHeader& hdr);
	bool ReadFileHeader();
	bool InitializeBuffers();
	int ReadFromFrame(u8 *dest, u64 pos, u64 maxBytes);
	bool DecompressFrame(u32 frame, u32 readBufferSize);

	u32 m_frameSize;
	u8 m_frameShift;
	u8 m_indexShift;
	u8* m_readBuffer;
	u8 *m_zlibBuffer;
	u32 m_zlibBufferFrame;
	u32 *m_index;
	u64 m_totalSize;
	// The actual source cso file handle.
	FILE* m_src;
	z_stream* m_z_stream;
	// The result of a read is stored here between BeginRead() and FinishRead().
	int m_bytesRead;
};
