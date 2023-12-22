// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "AsyncFileReader.h"
#include "CsoFileReader.h"

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Error.h"
#include "common/StringUtil.h"

#include "fmt/format.h"
#include "lz4.h"

#include <zlib.h>

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

CsoFileReader::CsoFileReader()
{
	m_blocksize = 2048;
}

CsoFileReader::~CsoFileReader()
{
	Close();
}

bool CsoFileReader::ValidateHeader(const CsoHeader& hdr, Error* error)
{
	if ((hdr.magic[0] != 'C' && hdr.magic[0] != 'Z') || hdr.magic[1] != 'I' || hdr.magic[2] != 'S' || hdr.magic[3] != 'O')
	{
		// Invalid magic, definitely a bad file.
		Error::SetString(error, "File is not a CSO or ZSO.");
		return false;
	}
	if (hdr.ver > 1)
	{
		Error::SetString(error, "Only CSOv1 files are supported.");
		return false;
	}
	if ((hdr.frame_size & (hdr.frame_size - 1)) != 0)
	{
		Error::SetString(error, "CSO frame size must be a power of two.");
		return false;
	}
	if (hdr.frame_size < 2048)
	{
		Error::SetString(error, "CSO frame size must be at least one sector.");
		return false;
	}

	// All checks passed, this is a good CSO header.
	return true;
}

bool CsoFileReader::Open2(std::string filename, Error* error)
{
	Close2();
	m_filename = std::move(filename);
	m_src = FileSystem::OpenCFile(m_filename.c_str(), "rb", error);

	bool success = false;
	if (m_src && ReadFileHeader(error) && InitializeBuffers(error))
	{
		success = true;
	}

	if (!success)
	{
		Close2();
		return false;
	}
	return true;
}

bool CsoFileReader::ReadFileHeader(Error* error)
{
	CsoHeader hdr;

	if (FileSystem::FSeek64(m_src, m_dataoffset, SEEK_SET) != 0 || std::fread(&hdr, 1, sizeof(hdr), m_src) != sizeof(hdr))
	{
		Error::SetString(error, "Failed to read CSO file header.");
		return false;
	}

	if (!ValidateHeader(hdr, error))
		return false;

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

	// Check compression method (ZSO=lz4)
	m_uselz4 = hdr.magic[0] == 'Z';

	return true;
}

bool CsoFileReader::InitializeBuffers(Error* error)
{
	// Round up, since part of a frame requires a full frame.
	u32 numFrames = (u32)((m_totalSize + m_frameSize - 1) / m_frameSize);

	// We might read a bit of alignment too, so be prepared.
	if (m_frameSize + (1 << m_indexShift) < CSO_READ_BUFFER_SIZE)
	{
		m_readBuffer = std::make_unique<u8[]>(CSO_READ_BUFFER_SIZE);
	}
	else
	{
		m_readBuffer = std::make_unique<u8[]>(m_frameSize + (1 << m_indexShift));
	}

	const u32 indexSize = numFrames + 1;
	m_index = std::make_unique<u32[]>(indexSize);
	if (fread(m_index.get(), sizeof(u32), indexSize, m_src) != indexSize)
	{
		Error::SetString(error, "Unable to read index data from CSO.");
		return false;
	}

	// initialize zlib if not a ZSO
	if (!m_uselz4)
	{
		m_z_stream = std::make_unique<z_stream>();
		m_z_stream->zalloc = Z_NULL;
		m_z_stream->zfree = Z_NULL;
		m_z_stream->opaque = Z_NULL;
		if (inflateInit2(m_z_stream.get(), -15) != Z_OK)
		{
			Error::SetString(error, "Unable to initialize zlib for CSO decompression.");
			return false;
		}
	}

	return true;
}

void CsoFileReader::Close2()
{
	m_filename.clear();

	if (m_src)
	{
		fclose(m_src);
		m_src = nullptr;
	}
	if (m_z_stream)
	{
		inflateEnd(m_z_stream.get());
		m_z_stream.reset();
	}

	m_readBuffer.reset();
	m_index.reset();
}

u32 CsoFileReader::GetBlockCount() const
{
	return static_cast<u32>((m_totalSize - m_dataoffset) / m_blocksize);
}

ThreadedFileReader::Chunk CsoFileReader::ChunkForOffset(u64 offset)
{
	Chunk chunk = {0};
	if (offset >= m_totalSize)
	{
		chunk.chunkID = -1;
	}
	else
	{
		chunk.chunkID = offset >> m_frameShift;
		chunk.length = m_frameSize;
		chunk.offset = chunk.chunkID << m_frameShift;
	}
	return chunk;
}

int CsoFileReader::ReadChunk(void* dst, s64 chunkID)
{
	if (chunkID < 0)
		return -1;

	const u32 frame = chunkID;

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
		if (FileSystem::FSeek64(m_src, frameRawPos, SEEK_SET) != 0)
		{
			Console.Error("Unable to seek to uncompressed CSO data.");
			return 0;
		}
		return fread(dst, 1, m_frameSize, m_src);
	}
	else
	{
		if (FileSystem::FSeek64(m_src, frameRawPos, SEEK_SET) != 0)
		{
			Console.Error("Unable to seek to compressed CSO data.");
			return 0;
		}
		// This might be less bytes than frameRawSize in case of padding on the last frame.
		// This is because the index positions must be aligned.
		const u32 readRawBytes = fread(m_readBuffer.get(), 1, frameRawSize, m_src);
		bool success = false;

		if (m_uselz4)
		{
			const int src_size = static_cast<int>(readRawBytes);
			const int dst_size = static_cast<int>(m_frameSize);
			const char* src_buf = reinterpret_cast<const char*>(m_readBuffer.get());
			char* dst_buf = static_cast<char*>(dst);
			
			const int res = LZ4_decompress_safe_partial(src_buf, dst_buf, src_size, dst_size, dst_size);
			success = (res > 0);
		}
		else
		{
			m_z_stream->next_in = m_readBuffer.get();
			m_z_stream->avail_in = readRawBytes;
			m_z_stream->next_out = static_cast<Bytef*>(dst);
			m_z_stream->avail_out = m_frameSize;

			const int status = inflate(m_z_stream.get(), Z_FINISH);
			success = (status == Z_STREAM_END && m_z_stream->total_out == m_frameSize);
		}

		if (!success)
			Console.Error(fmt::format("Unable to decompress CSO frame using {}", (m_uselz4)? "lz4":"zlib"));
		
		if (!m_uselz4)
			inflateReset(m_z_stream.get());

		return success ? m_frameSize : 0;
	}
}
