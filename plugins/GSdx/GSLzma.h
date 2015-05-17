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

#ifdef __linux__

#ifdef LZMA_SUPPORTED
#include <lzma.h>
#endif

class GSDumpFile {
	protected:
	FILE*		m_fp;

	GSDumpFile(char* filename);
	virtual ~GSDumpFile();

	public:
	virtual bool IsEof() = 0;
	virtual void Read(void* ptr, size_t size) = 0;
};

#ifdef LZMA_SUPPORTED
class GSDumpLzma : public GSDumpFile {

	lzma_stream m_strm;

	size_t		m_buff_size;
	uint8_t*	m_area;
	uint8_t*	m_inbuf;

	size_t		m_avail;
	size_t		m_start;
	bool  		m_eof;

	void Decompress();

	public:

	GSDumpLzma(char* filename);
	virtual ~GSDumpLzma();

	bool IsEof();
	void Read(void* ptr, size_t size);
};
#endif

class GSDumpRaw : public GSDumpFile {

	lzma_stream m_strm;

	size_t		m_buff_size;
	uint8_t*	m_area;
	uint8_t*	m_inbuf;

	size_t		m_avail;
	size_t		m_start;
	bool  		m_eof;

	void Decompress();

	public:

	GSDumpRaw(char* filename);
	virtual ~GSDumpRaw();

	bool IsEof();
	void Read(void* ptr, size_t size);
};

#endif
