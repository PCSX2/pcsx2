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

#include <lzma.h>

class GSDumpFile
{
	FILE* m_repack_fp;

protected:
	FILE* m_fp;

	void Repack(void* ptr, size_t size);

public:
	virtual bool IsEof() = 0;
	virtual bool Read(void* ptr, size_t size) = 0;

	GSDumpFile(char* filename, const char* repack_filename);
	virtual ~GSDumpFile();
};

class GSDumpLzma : public GSDumpFile
{
	lzma_stream m_strm;

	size_t m_buff_size;
	uint8_t* m_area;
	uint8_t* m_inbuf;

	size_t m_avail;
	size_t m_start;

	void Decompress();

public:
	GSDumpLzma(char* filename, const char* repack_filename);
	virtual ~GSDumpLzma();

	bool IsEof() final;
	bool Read(void* ptr, size_t size) final;
};

class GSDumpRaw : public GSDumpFile
{
	size_t m_buff_size;
	uint8_t* m_area;
	uint8_t* m_inbuf;

	size_t m_avail;
	size_t m_start;

public:
	GSDumpRaw(char* filename, const char* repack_filename);
	virtual ~GSDumpRaw() = default;

	bool IsEof() final;
	bool Read(void* ptr, size_t size) final;
};
