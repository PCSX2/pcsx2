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

#ifdef __linux__

GSDumpFile::GSDumpFile(char* filename) {
	m_fp = fopen(filename, "rb");
	if (m_fp == NULL) {
		fprintf(stderr, "failed to open %s\n", filename);
		throw "BAD"; // Just exit the program
	}
}

GSDumpFile::~GSDumpFile() {
	if (m_fp)
		fclose(m_fp);
}

/******************************************************************/
#ifdef LZMA_SUPPORTED

GSDumpLzma::GSDumpLzma(char* filename) : GSDumpFile(filename) {

	memset(&m_strm, 0, sizeof(lzma_stream));

	lzma_ret ret = lzma_stream_decoder(&m_strm, UINT32_MAX, 0);

	if (ret != LZMA_OK) {
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

void GSDumpLzma::Decompress() {
	lzma_action action = LZMA_RUN;

	m_strm.next_out  = m_area;
	m_strm.avail_out = m_buff_size;

	// Nothing left in the input buffer. Read data from the file
	if (m_strm.avail_in == 0 && !feof(m_fp)) {
		m_strm.next_in   = m_inbuf;
		m_strm.avail_in  = fread(m_inbuf, 1, BUFSIZ, m_fp);

		if (ferror(m_fp)) {
			fprintf(stderr, "Read error: %s\n", strerror(errno));
			throw "BAD"; // Just exit the program
		}
	}

	lzma_ret ret = lzma_code(&m_strm, action);

	if (ret != LZMA_OK) {
		if (ret == LZMA_STREAM_END)
			fprintf(stderr, "LZMA decoder finished without error\n\n");
		else {
			fprintf(stderr, "Decoder error: (error code %u)\n", ret);
			throw "BAD"; // Just exit the program
		}
	}

	m_start = 0;
	m_avail = m_buff_size - m_strm.avail_out;
}

bool GSDumpLzma::IsEof() {
	return feof(m_fp) && (m_avail == 0);
}

void GSDumpLzma::Read(void* ptr, size_t size) {
	size_t off = 0;
	uint8_t* dst = (uint8_t*)ptr;
	while (size) {
		if (m_avail == 0) {
			Decompress();
		}

		size_t l = min(size, m_avail);
		memcpy(dst + off, m_area+m_start, l);
		m_avail -= l;
		size    -= l;
		m_start += l;
		off     += l;
	}
}

GSDumpLzma::~GSDumpLzma() {
	lzma_end(&m_strm);

	if (m_inbuf)
		_aligned_free(m_inbuf);
	if (m_area)
		_aligned_free(m_area);
}

#endif

/******************************************************************/

GSDumpRaw::GSDumpRaw(char* filename) : GSDumpFile(filename) {
}

GSDumpRaw::~GSDumpRaw() {
}

bool GSDumpRaw::IsEof() {
	return feof(m_fp);
}

void GSDumpRaw::Read(void* ptr, size_t size) {
	if (size == 1) {
		// I don't know why but read of size 1 is not happy
		int v = fgetc(m_fp);
		memcpy(ptr, &v, 1);
	} else {
		size_t ret = fread(ptr, 1, size, m_fp);
		if (ret != size) {
			fprintf(stderr, "GSDumpRaw:: Read error\n");
			throw "BAD"; // Just exit the program
		}
	}
}

#endif
