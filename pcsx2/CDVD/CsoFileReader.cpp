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

#include "PrecompiledHeader.h"
#include "AsyncFileReader.h"
#include "CompressedFileReaderUtils.h"
#include "CsoFileReader.h"
#include "Pcsx2Types.h"
#ifdef __POSIX__
#include <zlib.h>
#else
#include <zlib/zlib.h>
#endif

// Implementation of CSO compressed ISO reading, based on:
// https://github.com/unknownbrackets/maxcso/blob/master/README_CSO.md
struct CsoHeader
{
	u8 magic[4];
	u32 header_size;
	u64 total_bytes;
	u32 frame_size;
	u8 ver;
	u8 align;
	u8 reserved[2];
};

static const u32 CSO_READ_BUFFER_SIZE = 256 * 1024;

bool CsoFileReader::CanHandle(const wxString& fileName)
{
	bool supported = false;
	if (wxFileName::FileExists(fileName) && fileName.Lower().EndsWith(L".cso"))
	{
		FILE* fp = PX_fopen_rb(fileName);
		CsoHeader hdr;
		if (fp)
		{
			if (fread(&hdr, 1, sizeof(hdr), fp) == sizeof(hdr))
			{
				supported = ValidateHeader(hdr);
			}
			fclose(fp);
		}
	}
	return supported;
}

bool CsoFileReader::ValidateHeader(const CsoHeader& hdr)
{
	if (hdr.magic[0] != 'C' || hdr.magic[1] != 'I' || hdr.magic[2] != 'S' || hdr.magic[3] != 'O')
	{
		// Invalid magic, definitely a bad file.
		return false;
	}
	if (hdr.ver > 1)
	{
		Console.Error(L"Only CSOv1 files are supported.");
		return false;
	}
	if ((hdr.frame_size & (hdr.frame_size - 1)) != 0)
	{
		Console.Error(L"CSO frame size must be a power of two.");
		return false;
	}
	if (hdr.frame_size < 2048)
	{
		Console.Error(L"CSO frame size must be at least one sector.");
		return false;
	}

	// All checks passed, this is a good CSO header.
	return true;
}

bool CsoFileReader::Open(const wxString& fileName)
{
	Close();
	m_filename = fileName;
	m_src = PX_fopen_rb(m_filename);

	bool success = false;
	if (m_src && ReadFileHeader() && InitializeBuffers())
	{
		success = true;
	}

	if (!success)
	{
		Close();
		return false;
	}
	return true;
}

bool CsoFileReader::ReadFileHeader()
{
	CsoHeader hdr = {};

	PX_fseeko(m_src, m_dataoffset, SEEK_SET);
	if (fread(&hdr, 1, sizeof(hdr), m_src) != sizeof(hdr))
	{
		Console.Error(L"Failed to read CSO file header.");
		return false;
	}

	if (!ValidateHeader(hdr))
	{
		Console.Error(L"CSO has invalid header.");
		return false;
	}

	m_frameSize = hdr.frame_size;
	// Determine the translation from bytes to frame.
	m_frameShift = 0;
	for (u32 i = m_frameSize; i > 1; i >>= 1)
	{
		++m_frameShift;
	}

	// This is the index alignment (index values need shifting by this amount.)
	m_indexShift = hdr.align;
	m_totalSize = hdr.total_bytes;

	return true;
}

bool CsoFileReader::InitializeBuffers()
{
	// Round up, since part of a frame requires a full frame.
	u32 numFrames = (u32)((m_totalSize + m_frameSize - 1) / m_frameSize);

	// We might read a bit of alignment too, so be prepared.
	if (m_frameSize + (1 << m_indexShift) < CSO_READ_BUFFER_SIZE)
	{
		m_readBuffer = new u8[CSO_READ_BUFFER_SIZE];
	}
	else
	{
		m_readBuffer = new u8[m_frameSize + (1 << m_indexShift)];
	}

	// This is a buffer for the most recently decompressed frame.
	m_zlibBuffer = new u8[m_frameSize + (1 << m_indexShift)];
	m_zlibBufferFrame = numFrames;

	const u32 indexSize = numFrames + 1;
	m_index = new u32[indexSize];
	if (fread(m_index, sizeof(u32), indexSize, m_src) != indexSize)
	{
		Console.Error(L"Unable to read index data from CSO.");
		return false;
	}

	m_z_stream = new z_stream;
	m_z_stream->zalloc = Z_NULL;
	m_z_stream->zfree = Z_NULL;
	m_z_stream->opaque = Z_NULL;
	if (inflateInit2(m_z_stream, -15) != Z_OK)
	{
		Console.Error("Unable to initialize zlib for CSO decompression.");
		return false;
	}

	return true;
}

void CsoFileReader::Close()
{
	m_filename.Empty();
#if CSO_USE_CHUNKSCACHE
	m_cache.Clear();
#endif

	if (m_src)
	{
		fclose(m_src);
		m_src = NULL;
	}
	if (m_z_stream)
	{
		inflateEnd(m_z_stream);
		m_z_stream = NULL;
	}

	if (m_readBuffer)
	{
		delete[] m_readBuffer;
		m_readBuffer = NULL;
	}
	if (m_zlibBuffer)
	{
		delete[] m_zlibBuffer;
		m_zlibBuffer = NULL;
	}
	if (m_index)
	{
		delete[] m_index;
		m_index = NULL;
	}
}

int CsoFileReader::ReadSync(void* pBuffer, uint sector, uint count)
{
	if (!m_src)
	{
		return 0;
	}

	// Note that, in practice, count will always be 1.  It seems one sector is read
	// per interrupt, even if multiple are requested by the application.

	u8* dest = (u8*)pBuffer;
	// We do it this way in case m_blocksize is not well aligned to our frame size.
	u64 pos = (u64)sector * (u64)m_blocksize;
	int remaining = count * m_blocksize;
	int bytes = 0;

	while (remaining > 0)
	{
		int readBytes;

#if CSO_USE_CHUNKSCACHE
		// Try first to read from the cache.
		readBytes = m_cache.Read(dest + bytes, pos + bytes, remaining);
#else
		readBytes = -1;
#endif
		if (readBytes < 0)
		{
			readBytes = ReadFromFrame(dest + bytes, pos + bytes, remaining);
			if (readBytes == 0)
			{
				// We hit EOF.
				break;
			}

#if CSO_USE_CHUNKSCACHE
			// Add the bytes into the cache.  We need to allocate a buffer for it.
			void* cached = malloc(readBytes);
			memcpy(cached, dest + bytes, readBytes);
			m_cache.Take(cached, pos + bytes, readBytes, readBytes);
#endif
		}

		bytes += readBytes;
		remaining -= readBytes;
	}

	return bytes;
}

int CsoFileReader::ReadFromFrame(u8* dest, u64 pos, int maxBytes)
{
	if (pos >= m_totalSize)
	{
		// Can't read anything passed the end.
		return 0;
	}

	const u32 frame = (u32)(pos >> m_frameShift);
	const u32 offset = (u32)(pos - (frame << m_frameShift));
	// This is how many bytes we will actually be reading from this frame.
	const u32 bytes = (u32)(std::min(m_blocksize, static_cast<uint>(m_frameSize - offset)));

	// Grab the index data for the frame we're about to read.
	const bool compressed = (m_index[frame + 0] & 0x80000000) == 0;
	const u32 index0 = m_index[frame + 0] & 0x7FFFFFFF;
	const u32 index1 = m_index[frame + 1] & 0x7FFFFFFF;

	// Calculate where the compressed payload is (if compressed.)
	const u64 frameRawPos = (u64)index0 << m_indexShift;
	const u64 frameRawSize = (u64)(index1 - index0) << m_indexShift;

	if (!compressed)
	{
		// Just read directly, easy.
		if (PX_fseeko(m_src, m_dataoffset + frameRawPos + offset, SEEK_SET) != 0)
		{
			Console.Error("Unable to seek to uncompressed CSO data.");
			return 0;
		}
		return fread(dest, 1, bytes, m_src);
	}
	else
	{
		// We don't need to decompress if we already did this same frame last time.
		if (m_zlibBufferFrame != frame)
		{
			if (PX_fseeko(m_src, m_dataoffset + frameRawPos, SEEK_SET) != 0)
			{
				Console.Error("Unable to seek to compressed CSO data.");
				return 0;
			}
			// This might be less bytes than frameRawSize in case of padding on the last frame.
			// This is because the index positions must be aligned.
			const u32 readRawBytes = fread(m_readBuffer, 1, frameRawSize, m_src);
			if (!DecompressFrame(frame, readRawBytes))
			{
				return 0;
			}
		}

		// Now we just copy the offset data from the cache.
		memcpy(dest, m_zlibBuffer + offset, bytes);
	}

	return bytes;
}

bool CsoFileReader::DecompressFrame(u32 frame, u32 readBufferSize)
{
	m_z_stream->next_in = m_readBuffer;
	m_z_stream->avail_in = readBufferSize;
	m_z_stream->next_out = m_zlibBuffer;
	m_z_stream->avail_out = m_frameSize;

	int status = inflate(m_z_stream, Z_FINISH);
	bool success = status == Z_STREAM_END && m_z_stream->total_out == m_frameSize;
	if (success)
	{
		// Our buffer now contains this frame.
		m_zlibBufferFrame = frame;
	}
	else
	{
		Console.Error("Unable to decompress CSO frame using zlib.");
		m_zlibBufferFrame = (u32)-1;
	}

	inflateReset(m_z_stream);
	return success;
}

void CsoFileReader::BeginRead(void* pBuffer, uint sector, uint count)
{
	// TODO: No async support yet, implement as sync.
	m_bytesRead = ReadSync(pBuffer, sector, count);
}

int CsoFileReader::FinishRead()
{
	int res = m_bytesRead;
	m_bytesRead = -1;
	return res;
}

void CsoFileReader::CancelRead()
{
	// TODO: No async read support yet.
}
