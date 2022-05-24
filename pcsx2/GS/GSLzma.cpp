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

#include "common/AlignedMalloc.h"
#include "common/FileSystem.h"
#include "common/StringUtil.h"

#include "GSDump.h"
#include "GSLzma.h"

using namespace GSDumpTypes;

GSDumpFile::GSDumpFile(FILE* file, FILE* repack_file)
	: m_fp(file)
	, m_repack_fp(repack_file)
{
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

std::unique_ptr<GSDumpFile> GSDumpFile::OpenGSDump(const char* filename, const char* repack_filename /*= nullptr*/)
{
	std::FILE* fp = FileSystem::OpenCFile(filename, "rb");
	if (!fp)
		return nullptr;

	std::FILE* repack_fp = nullptr;
	if (repack_filename && std::strlen(repack_filename) > 0)
	{
		repack_fp = FileSystem::OpenCFile(repack_filename, "wb");
		if (!repack_fp)
		{
			std::fclose(fp);
			return nullptr;
		}
	}

	if (StringUtil::EndsWithNoCase(filename, ".xz"))
		return std::make_unique<GSDumpLzma>(fp, nullptr);
	else if (StringUtil::EndsWithNoCase(filename, ".zst"))
		return std::make_unique<GSDumpDecompressZst>(fp, nullptr);
	else
		return std::make_unique<GSDumpRaw>(fp, nullptr);
}

bool GSDumpFile::GetPreviewImageFromDump(const char* filename, u32* width, u32* height, std::vector<u32>* pixels)
{
	std::unique_ptr<GSDumpFile> dump = OpenGSDump(filename);
	if (!dump)
		return false;

	u32 crc;
	if (!dump->Read(&crc, sizeof(crc)) || crc != 0xFFFFFFFFu)
	{
		// not new header dump, so no preview
		return false;
	}

	u32 header_size;
	if (!dump->Read(&header_size, sizeof(header_size)) || header_size < sizeof(GSDumpHeader))
	{
		// doesn't have the screenshot fields
		return false;
	}

	std::unique_ptr<u8[]> header_bits = std::make_unique<u8[]>(header_size);
	if (!dump->Read(header_bits.get(), header_size))
		return false;

	GSDumpHeader header;
	std::memcpy(&header, header_bits.get(), sizeof(header));
	if (header.screenshot_size == 0 ||
		header.screenshot_size < (header.screenshot_width * header.screenshot_height * sizeof(u32)) ||
		(static_cast<u64>(header.screenshot_offset) + header.screenshot_size) > header_size)
	{
		// doesn't have a screenshot
		return false;
	}

	*width = header.screenshot_width;
	*height = header.screenshot_height;
	pixels->resize(header.screenshot_width * header.screenshot_height);
	std::memcpy(pixels->data(), header_bits.get() + header.screenshot_offset, header.screenshot_size);
	return true;
}

bool GSDumpFile::ReadFile()
{
	u32 ss;
	if (Read(&m_crc, sizeof(m_crc)) != sizeof(m_crc) || Read(&ss, sizeof(ss)) != sizeof(ss))
		return false;

	m_state_data.resize(ss);
	if (Read(m_state_data.data(), ss) != ss)
		return false;

	// Pull serial out of new header, if present.
	if (m_crc == 0xFFFFFFFFu)
	{
		GSDumpHeader header;
		if (m_state_data.size() < sizeof(header))
		{
			Console.Error("GSDump header is corrupted.");
			return false;
		}

		std::memcpy(&header, m_state_data.data(), sizeof(header));

		m_crc = header.crc;

		if (header.serial_size > 0)
		{
			if (header.serial_offset > ss || (static_cast<u64>(header.serial_offset) + header.serial_size) > ss)
			{
				Console.Error("GSDump header is corrupted.");
				return false;
			}

			if (header.serial_size > 0)
				m_serial.assign(reinterpret_cast<const char*>(m_state_data.data()) + header.serial_offset, header.serial_size);
		}

		// Read the real state data
		m_state_data.resize(header.state_size);
		if (Read(m_state_data.data(), header.state_size) != header.state_size)
			return false;
	}

	m_regs_data.resize(8192);
	if (Read(m_regs_data.data(), m_regs_data.size()) != m_regs_data.size())
		return false;

	// read all the packet data in
	// TODO: make this suck less by getting the full/extracted size and preallocating
	for (;;)
	{
		const size_t packet_data_size = m_packet_data.size();
		m_packet_data.resize(std::max<size_t>(packet_data_size * 2, 8 * _1mb));

		const size_t read_size = m_packet_data.size() - packet_data_size;
		const size_t read = Read(m_packet_data.data() + packet_data_size, read_size);
		if (read != read_size)
		{
			if (!IsEof())
				return false;

			m_packet_data.resize(packet_data_size + read);
			m_packet_data.shrink_to_fit();
			break;
		}
	}

	u8* data = m_packet_data.data();
	size_t remaining = m_packet_data.size();

#define GET_BYTE(dst) \
	do \
	{ \
		if (remaining < sizeof(u8)) \
			return false; \
		std::memcpy(dst, data, sizeof(u8)); \
		data++; \
		remaining--; \
	} while (0)
#define GET_WORD(dst) \
	do \
	{ \
		if (remaining < sizeof(u32)) \
			return false; \
		std::memcpy(dst, data, sizeof(u32)); \
		data += sizeof(u32); \
		remaining -= sizeof(u32); \
	} while (0)

	while (remaining > 0)
	{
		GSData packet = {};
		packet.path = GSTransferPath::Dummy;
		GET_BYTE(&packet.id);

		switch (packet.id)
		{
			case GSType::Transfer:
				GET_BYTE(&packet.path);
				GET_WORD(&packet.length);
				break;
			case GSType::VSync:
				packet.length = 1;
				break;
			case GSType::ReadFIFO2:
				packet.length = 4;
				break;
			case GSType::Registers:
				packet.length = 8192;
				break;
			default:
				return false;
		}

		if (packet.length > 0)
		{
			if (remaining < packet.length)
			{
				// There's apparently some "bad" dumps out there that are missing bytes on the end..
				// The "safest" option here is to discard the last packet, since that has less risk
				// of leaving the GS in the middle of a command.
				Console.Error("(GSDump) Dropping last packet of %u bytes (we only have %u bytes)",
					static_cast<u32>(packet.length), static_cast<u32>(remaining));
				break;
			}

			packet.data = data;
			data += packet.length;
			remaining -= packet.length;
		}

		m_dump_packets.push_back(std::move(packet));
	}

#undef GET_WORD
#undef GET_BYTE

	return true;
}

/******************************************************************/
GSDumpLzma::GSDumpLzma(FILE* file, FILE* repack_file)
	: GSDumpFile(file, repack_file)
{
	Initialize();
}

void GSDumpLzma::Initialize()
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

size_t GSDumpLzma::Read(void* ptr, size_t size)
{
	size_t off = 0;
	uint8_t* dst = (uint8_t*)ptr;
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

	if (off > 0)
		Repack(ptr, off);

	return off;
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
GSDumpDecompressZst::GSDumpDecompressZst(FILE* file, FILE* repack_file)
	: GSDumpFile(file, repack_file)
{
	Initialize();
}

void GSDumpDecompressZst::Initialize()
{
	m_strm = ZSTD_createDStream();

	m_area      = (uint8_t*)_aligned_malloc(OUTPUT_BUFFER_SIZE, 32);
	m_inbuf.src = (uint8_t*)_aligned_malloc(INPUT_BUFFER_SIZE, 32);
	m_inbuf.pos = 0;
	m_inbuf.size = 0;
	m_avail     = 0;
	m_start     = 0;
}

void GSDumpDecompressZst::Decompress()
{
	ZSTD_outBuffer outbuf = { m_area, OUTPUT_BUFFER_SIZE, 0 };
	while (outbuf.pos == 0)
	{
		// Nothing left in the input buffer. Read data from the file
		if (m_inbuf.pos == m_inbuf.size && !feof(m_fp))
		{
			m_inbuf.size = fread((void*)m_inbuf.src, 1, INPUT_BUFFER_SIZE, m_fp);
			m_inbuf.pos = 0;

			if (ferror(m_fp))
			{
				fprintf(stderr, "Read error: %s\n", strerror(errno));
				throw "BAD"; // Just exit the program
			}
		}

		size_t ret = ZSTD_decompressStream(m_strm, &outbuf, &m_inbuf);
		if (ZSTD_isError(ret))
		{
			fprintf(stderr, "Decoder error: (error code %s)\n", ZSTD_getErrorName(ret));
			throw "BAD"; // Just exit the program
		}
	}

	m_start = 0;
	m_avail = outbuf.pos;
}

bool GSDumpDecompressZst::IsEof()
{
	return feof(m_fp) && m_avail == 0 && m_inbuf.pos == m_inbuf.size;
}

size_t GSDumpDecompressZst::Read(void* ptr, size_t size)
{
	size_t off = 0;
	uint8_t* dst = (uint8_t*)ptr;
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

	if (off > 0)
		Repack(ptr, off);

	return off;
}

GSDumpDecompressZst::~GSDumpDecompressZst()
{
	ZSTD_freeDStream(m_strm);

	if (m_inbuf.src)
		_aligned_free((void*)m_inbuf.src);
	if (m_area)
		_aligned_free(m_area);
}

/******************************************************************/

GSDumpRaw::GSDumpRaw(FILE* file, FILE* repack_file)
	: GSDumpFile(file, repack_file)
{
}

bool GSDumpRaw::IsEof()
{
	return !!feof(m_fp);
}

size_t GSDumpRaw::Read(void* ptr, size_t size)
{
	size_t ret = fread(ptr, 1, size, m_fp);
	if (ret != size && ferror(m_fp))
	{
		fprintf(stderr, "GSDumpRaw:: Read error (%zu/%zu)\n", ret, size);
		return ret;
	}

	if (ret > 0)
		Repack(ptr, ret);

	return ret;
}
