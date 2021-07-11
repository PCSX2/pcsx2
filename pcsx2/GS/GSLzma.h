/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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
