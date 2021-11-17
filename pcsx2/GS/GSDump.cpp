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

#include "PrecompiledHeader.h"
#include "GSDump.h"
#include "GSExtra.h"

GSDumpBase::GSDumpBase(const std::string& fn)
	: m_frames(0)
	, m_extra_frames(2)
{
	m_gs = px_fopen(fn, "wb");
	if (!m_gs)
		fprintf(stderr, "GSDump: Error failed to open %s\n", fn.c_str());
}

GSDumpBase::~GSDumpBase()
{
	if (m_gs)
		fclose(m_gs);
}

void GSDumpBase::AddHeader(u32 crc, const freezeData& fd, const GSPrivRegSet* regs)
{
	AppendRawData(&crc, 4);
	AppendRawData(&fd.size, 4);
	AppendRawData(fd.data, fd.size);
	AppendRawData(regs, sizeof(*regs));
}

void GSDumpBase::Transfer(int index, const u8* mem, size_t size)
{
	if (size == 0)
		return;

	AppendRawData(0);
	AppendRawData(static_cast<u8>(index));
	AppendRawData(&size, 4);
	AppendRawData(mem, size);
}

void GSDumpBase::ReadFIFO(u32 size)
{
	if (size == 0)
		return;

	AppendRawData(2);
	AppendRawData(&size, 4);
}

bool GSDumpBase::VSync(int field, bool last, const GSPrivRegSet* regs)
{
	// dump file is bad, return done to delete the object
	if (!m_gs)
		return true;

	AppendRawData(3);
	AppendRawData(regs, sizeof(*regs));

	AppendRawData(1);
	AppendRawData(static_cast<u8>(field));

	if (last)
		m_extra_frames--;

	return (++m_frames & 1) == 0 && last && (m_extra_frames < 0);
}

void GSDumpBase::Write(const void* data, size_t size)
{
	if (!m_gs || size == 0)
		return;

	size_t written = fwrite(data, 1, size, m_gs);
	if (written != size)
		fprintf(stderr, "GSDump: Error failed to write data\n");
}

//////////////////////////////////////////////////////////////////////
// GSDump implementation
//////////////////////////////////////////////////////////////////////

GSDump::GSDump(const std::string& fn, u32 crc, const freezeData& fd, const GSPrivRegSet* regs)
	: GSDumpBase(fn + ".gs")
{
	AddHeader(crc, fd, regs);
}

void GSDump::AppendRawData(const void* data, size_t size)
{
	Write(data, size);
}

void GSDump::AppendRawData(u8 c)
{
	Write(&c, 1);
}

//////////////////////////////////////////////////////////////////////
// GSDumpXz implementation
//////////////////////////////////////////////////////////////////////

GSDumpXz::GSDumpXz(const std::string& fn, u32 crc, const freezeData& fd, const GSPrivRegSet* regs)
	: GSDumpBase(fn + ".gs.xz")
{
	m_strm = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_easy_encoder(&m_strm, 6 /*level*/, LZMA_CHECK_CRC64);
	if (ret != LZMA_OK)
	{
		fprintf(stderr, "GSDumpXz: Error initializing LZMA encoder ! (error code %u)\n", ret);
		return;
	}

	AddHeader(crc, fd, regs);
}

GSDumpXz::~GSDumpXz()
{
	Flush();

	// Finish the stream
	m_strm.avail_in = 0;
	Compress(LZMA_FINISH, LZMA_STREAM_END);

	lzma_end(&m_strm);
}

void GSDumpXz::AppendRawData(const void* data, size_t size)
{
	size_t old_size = m_in_buff.size();
	m_in_buff.resize(old_size + size);
	memcpy(&m_in_buff[old_size], data, size);

	// Enough data was accumulated, time to write/compress it.  If compression
	// is enabled, it will freeze PCSX2. 1GB should be enough for long dump.
	//
	// Note: long dumps are currently not supported so this path won't be executed
	if (m_in_buff.size() > 1024 * 1024 * 1024)
		Flush();
}

void GSDumpXz::AppendRawData(u8 c)
{
	m_in_buff.push_back(c);
}

void GSDumpXz::Flush()
{
	if (m_in_buff.empty())
		return;

	m_strm.next_in = m_in_buff.data();
	m_strm.avail_in = m_in_buff.size();

	Compress(LZMA_RUN, LZMA_OK);

	m_in_buff.clear();
}

void GSDumpXz::Compress(lzma_action action, lzma_ret expected_status)
{
	std::vector<u8> out_buff(1024 * 1024);
	do
	{
		m_strm.next_out = out_buff.data();
		m_strm.avail_out = out_buff.size();

		lzma_ret ret = lzma_code(&m_strm, action);

		if (ret != expected_status)
		{
			fprintf(stderr, "GSDumpXz: Error %d\n", (int)ret);
			return;
		}

		size_t write_size = out_buff.size() - m_strm.avail_out;
		Write(out_buff.data(), write_size);

	} while (m_strm.avail_out == 0);
}
