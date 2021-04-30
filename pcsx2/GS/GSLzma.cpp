/*
 *	Copyright (C) 2015-2015 Gregory hainaut
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSLzma.h"

GSDumpFile::GSDumpFile(char* filename, const char* repack_filename)
{
	m_fp = fopen(filename, "rb");
	if (m_fp == nullptr)
	{
		fprintf(stderr, "failed to open %s\n", filename);
		throw "BAD"; // Just exit the program
	}

	m_repack_fp = nullptr;
	if (repack_filename)
	{
		m_repack_fp = fopen(repack_filename, "wb");
		if (m_repack_fp == nullptr)
			fprintf(stderr, "failed to open %s for repack\n", repack_filename);
	}
}

void GSDumpFile::Repack(void* ptr, size_t size)
{
	if (m_repack_fp == nullptr)
		return;

	size_t ret = fwrite(ptr, 1, size, m_repack_fp);
	if (ret != size)
		fprintf(stderr, "Failed to repack\n");
}

GSDumpFile::~GSDumpFile()
{
	if (m_fp)
		fclose(m_fp);
	if (m_repack_fp)
		fclose(m_repack_fp);
}

/******************************************************************/
GSDumpLzma::GSDumpLzma(char* filename, const char* repack_filename)
	: GSDumpFile(filename, repack_filename)
{

	memset(&m_strm, 0, sizeof(lzma_stream));

	lzma_ret ret = lzma_stream_decoder(&m_strm, UINT32_MAX, 0);

	if (ret != LZMA_OK)
	{
		fprintf(stderr, "Error initializing the decoder! (error code %u)\n", ret);
		throw "BAD"; // Just exit the program
	}

	m_buff_size = 1024*1024;
	m_area      = (uint8_t*)_aligned_malloc(m_buff_size, 32);
	m_inbuf     = (uint8_t*)_aligned_malloc(BUFSIZ, 32);
	m_avail     = 0;
	m_start     = 0;

	m_strm.avail_in  = 0;
	m_strm.next_in   = m_inbuf;

	m_strm.avail_out = m_buff_size;
	m_strm.next_out  = m_area;
}

void GSDumpLzma::Decompress()
{
	lzma_action action = LZMA_RUN;

	m_strm.next_out  = m_area;
	m_strm.avail_out = m_buff_size;

	// Nothing left in the input buffer. Read data from the file
	if (m_strm.avail_in == 0 && !feof(m_fp))
	{
		m_strm.next_in   = m_inbuf;
		m_strm.avail_in  = fread(m_inbuf, 1, BUFSIZ, m_fp);

		if (ferror(m_fp))
		{
			fprintf(stderr, "Read error: %s\n", strerror(errno));
			throw "BAD"; // Just exit the program
		}
	}

	lzma_ret ret = lzma_code(&m_strm, action);

	if (ret != LZMA_OK)
	{
		if (ret == LZMA_STREAM_END)
			fprintf(stderr, "LZMA decoder finished without error\n\n");
		else
		{
			fprintf(stderr, "Decoder error: (error code %u)\n", ret);
			throw "BAD"; // Just exit the program
		}
	}

	m_start = 0;
	m_avail = m_buff_size - m_strm.avail_out;
}

bool GSDumpLzma::IsEof()
{
	return feof(m_fp) && m_avail == 0 && m_strm.avail_in == 0;
}

bool GSDumpLzma::Read(void* ptr, size_t size)
{
	size_t off = 0;
	uint8_t* dst = (uint8_t*)ptr;
	size_t full_size = size;
	while (size && !IsEof())
	{
		if (m_avail == 0)
		{
			Decompress();
		}

		size_t l = std::min(size, m_avail);
		memcpy(dst + off, m_area + m_start, l);
		m_avail -= l;
		size    -= l;
		m_start += l;
		off     += l;
	}

	if (size == 0)
	{
		Repack(ptr, full_size);
		return true;
	}

	return false;
}

GSDumpLzma::~GSDumpLzma()
{
	lzma_end(&m_strm);

	if (m_inbuf)
		_aligned_free(m_inbuf);
	if (m_area)
		_aligned_free(m_area);
}

/******************************************************************/

GSDumpRaw::GSDumpRaw(char* filename, const char* repack_filename)
	: GSDumpFile(filename, repack_filename)
{
	m_buff_size = 0;
	m_area      = nullptr;
	m_inbuf     = nullptr;
	m_avail     = 0;
	m_start     = 0;
}

bool GSDumpRaw::IsEof()
{
	return !!feof(m_fp);
}

bool GSDumpRaw::Read(void* ptr, size_t size)
{
	size_t ret = fread(ptr, 1, size, m_fp);
	if (ret != size && ferror(m_fp))
	{
		fprintf(stderr, "GSDumpRaw:: Read error (%zu/%zu)\n", ret, size);
		throw "BAD"; // Just exit the program
	}

	if (ret == size)
	{
		Repack(ptr, size);
		return true;
	}

	return false;
}
