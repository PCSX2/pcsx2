// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/AlignedMalloc.h"
#include "common/Console.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "common/BitUtils.h"
#include "common/Error.h"
#include "common/HeapArray.h"
#include "common/Timer.h"
#include "common/Path.h"

#include "GS/GSDump.h"
#include "GS/GSLzma.h"
#include "GS/GSExtra.h"

#include <Alloc.h>
#include <7zCrc.h>
#include <Xz.h>
#include <XzCrc64.h>
#include <zstd.h>

#include <mutex>

using namespace GSDumpTypes;

GSDumpFile::GSDumpFile() = default;

GSDumpFile::~GSDumpFile() = default;

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

	std::unique_ptr<u8[]> header_bits = std::make_unique_for_overwrite<u8[]>(header_size);
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

bool GSDumpFile::ReadFile(Error* error)
{
	if (!ReadHeaderStateRegs(error))
		return false;
	if (!ReadPackets(error))
		return false;
	return true;
}

bool GSDumpFile::ReadHeaderStateRegs(Error* error)
{
	u32 ss;
	if (Read(&m_crc, sizeof(m_crc)) != sizeof(m_crc) || Read(&ss, sizeof(ss)) != sizeof(ss))
	{
		Error::SetString(error, "Failed to read header");
		return false;
	}

	m_state_data.resize(ss);
	if (Read(m_state_data.data(), ss) != ss)
	{
		Error::SetString(error, "Failed to read state data");
		return false;
	}

	// Pull serial out of new header, if present.
	if (m_crc == 0xFFFFFFFFu)
	{
		GSDumpHeader header;
		if (m_state_data.size() < sizeof(header))
		{
			Error::SetString(error, "GSDump header is corrupted.");
			return false;
		}

		std::memcpy(&header, m_state_data.data(), sizeof(header));

		m_crc = header.crc;

		if (header.serial_size > 0)
		{
			if (header.serial_offset > ss || (static_cast<u64>(header.serial_offset) + header.serial_size) > ss)
			{
				Error::SetString(error, "GSDump header is corrupted.");
				return false;
			}

			if (header.serial_size > 0)
				m_serial.assign(reinterpret_cast<const char*>(m_state_data.data()) + header.serial_offset, header.serial_size);
		}

		// Read the real state data
		m_state_data.resize(header.state_size);
		if (Read(m_state_data.data(), header.state_size) != header.state_size)
		{
			Error::SetString(error, "Failed to read real state data");
			return false;
		}
	}

	m_regs_data.resize(8192);
	if (Read(m_regs_data.data(), m_regs_data.size()) != m_regs_data.size())
	{
		Error::SetString(error, "Failed to read regs data");
		return false;
	}

	return true;
}

bool GSDumpFile::ReadPackets(Error* error)
{
	if (!ReadPacketData(error))
		return false;

	u8* data = m_packet_data.data();
	u8* data_end = m_packet_data.data() + m_packet_data.size();

	while (data < data_end)
	{
		GSData packet = {};
		packet.path = GSTransferPath::Dummy;
		s64 n = ReadOnePacket(data, data_end, packet, error);
		if (n == PACKET_FAILURE)
			return false;
		if (n == PACKET_OUT_OF_DATA)
			break;
		pxAssert(n > 0);
		m_dump_packets.push_back(std::move(packet));
		data += n;
	}

	return true;
}

bool GSDumpFile::ReadPacketData(Error* error)
{
	if (m_size >= 0)
	{
		// Know the full size beforehand.

		const s64 try_read = m_size - static_cast<s64>(m_state_data.size()) - static_cast<s64>(m_regs_data.size());

		pxAssert(try_read > 0);

		m_packet_data.resize(try_read);
		const size_t read = Read(m_packet_data.data(), try_read);

		if (!IsEof())
		{
			Error::SetString(error, "Failed to read packet");
			return false;
		}

		m_packet_data.resize(read);
	}
	else
	{
		// read all the packet data in
		// TODO: make this suck less by getting the full/extracted size and preallocating

		const size_t min_packet_data_size = m_size_compressed >= 0 ? 4 * m_size_compressed : 8 * _1mb;
		for (;;)
		{

			const size_t packet_data_size = m_packet_data.size();
			m_packet_data.resize(std::max<size_t>(packet_data_size * 2, min_packet_data_size));

			const size_t read_size = m_packet_data.size() - packet_data_size;
			const size_t read = Read(m_packet_data.data() + packet_data_size, read_size);
			if (read != read_size)
			{
				if (!IsEof())
				{
					Error::SetString(error, "Failed to read packet");
					return false;
				}

				m_packet_data.resize(packet_data_size + read);
				break;
			}
		}
	}

	return true;
}

s64 GSDumpFile::ReadOnePacket(u8* data_start, u8* data_end, GSData& packet, Error* error)
{
	u8* data = data_start;

	const auto GetBytes = [&]<size_t bytes>(void* dst) {
		if (data + bytes > data_end)
		{
			Error::SetStringFmt(error, "Failed to read {} bytes", bytes);
			return false;
		}
		std::memcpy(dst, data, bytes);
		data += bytes;
		return true;
	};

	packet = {};
	packet.path = GSTransferPath::Dummy;
	if (!GetBytes.operator()<1>(&packet.id))
		return PACKET_OUT_OF_DATA;

	switch (packet.id)
	{
		case GSType::Transfer:
			if (!GetBytes.operator()<1>(&packet.path))
				return PACKET_OUT_OF_DATA;
			if (!GetBytes.operator()<4>(&packet.length))
				return PACKET_OUT_OF_DATA;
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
			Error::SetString(error, fmt::format("Unknown packet type {}", static_cast<u32>(packet.id)));
			return PACKET_FAILURE;
	}

	if (packet.length > 0)
	{
		if (data + packet.length > data_end)
		{
			// There's apparently some "bad" dumps out there that are missing bytes on the end..
			// The "safest" option here is to discard the last packet, since that has less risk
			// of leaving the GS in the middle of a command.
			Console.Error("(GSDump) Dropping last packet of %u bytes (we only have %u bytes)",
				static_cast<u32>(packet.length), static_cast<u32>(data_end - data));
			return PACKET_OUT_OF_DATA;
		}

		packet.data = data;
		data += packet.length;
	}

	return data - data_start;
}

bool GSDumpFile::ReadFile(std::vector<u8>& dst, size_t max_size, Error* error)
{
	if (m_size >= 0)
	{
		if (static_cast<size_t>(m_size) > max_size)
		{
			Error::SetStringFmt(error, "Buffer out of memory (got {}; expected {})", m_size, max_size);
			return false;
		}

		dst.resize(m_size);

		size_t read_size = Read(dst.data(), m_size);

		if (read_size != static_cast<size_t>(m_size) || !IsEof())
		{
			Error::SetStringFmt(error, "Did not read full contents (read {} bytes; expected {} bytes)", read_size, m_size);
			return false;
		}

		return true;
	}
	else
	{
		const size_t min_size = m_size_compressed >= 0 ? 4 * m_size_compressed : 8 * _1mb;

		dst.clear();

		while (true)
		{
			const size_t old_size = dst.size();
			dst.resize(std::max<size_t>(std::min(old_size * 2, max_size), min_size));

			const size_t read_size = dst.size() - old_size;
			const size_t read = Read(dst.data() + old_size, read_size);
			if (read != read_size)
			{
				std::size_t final_size = old_size + read;

				if (!IsEof())
				{
					Error::SetStringFmt(error, "Failed to read all data (read {} bytes)", final_size);
					return false;
				}

				dst.resize(final_size);
				return true;
			}

			if (dst.size() >= max_size)
				break;
		}

		if (dst.size() >= max_size)
		{
			Error::SetStringFmt(error, "Dump too large (read {} bytes; expected {} bytes)", dst.size(), max_size - 1);
			return false;
		}

		return true;
	}
}

void GSDumpFile::Clear()
{
	m_serial.clear();
	m_crc = 0;
	m_regs_data.clear();
	m_state_data.clear();
	m_packet_data.clear();
	m_dump_packets.clear();
}

s64 GSDumpFile::GetPacket(size_t i, GSData& data, Error* error)
{
	if (i < m_dump_packets.size())
	{
		data = m_dump_packets[i];
		return static_cast<s64>(GetPacketSize(data));
	}
	else
	{
		return PACKET_EOF;
	}
}

/******************************************************************/

static std::once_flag s_lzma_crc_table_init;

void GSInit7ZCRCTables()
{
	std::call_once(s_lzma_crc_table_init, []() {
		CrcGenerateTable();
		Crc64GenerateTable();
	});
}

namespace
{
	// Unbuffered Lzma stream
	class GSStreamLzma final : public GSStream
	{
	public:
		~GSStreamLzma();
		bool Open(FileSystem::ManagedCFilePtr fp, Error* error) override;
		s64 Read(void* ptr, size_t size, Error* error) override;
		bool IsEof() override;
		Type GetType() override;

	private:
		s64 ReadBlock(void* ptr, size_t size, Error* error);

		static constexpr size_t kInputBufSize = static_cast<size_t>(1) << 18;

		struct Block
		{
			bool loading = false;
			size_t file_offset = 0;
			size_t stream_offset = 0;
			size_t compressed_size = 0;
			size_t uncompressed_size = 0;
			CXzStreamFlags stream_flags = 0;
			size_t read_offset = 0;
			size_t write_offset = 0;
		};

		std::vector<Block> m_blocks;
		size_t m_stream_size = 0;
		size_t m_block_index = 0;

		DynamicHeapArray<u8, 64> m_block_read_buffer;
		alignas(__cachelinesize) CXzUnpacker m_unpacker = {};
		bool unpacker_constructed = false;
	};

	GSStreamLzma::~GSStreamLzma()
	{
		if (unpacker_constructed)
			XzUnpacker_Free(&m_unpacker);
	}

	GSStream::Type GSStreamLzma::GetType()
	{
		return LZMA;
	}

	bool GSStreamLzma::Open(FileSystem::ManagedCFilePtr fp, Error* error)
	{
		m_fp = std::move(fp);
		m_size_compressed = FileSystem::FSize64(m_fp.get());

		GSInit7ZCRCTables();

		// Reset state for multiple opens
		m_blocks.clear();
		m_stream_size = 0;
		m_block_index = 0;

		struct MyFileInStream
		{
			ISeekInStream vt;
			std::FILE* fp;
		};

		MyFileInStream fis = {
			{.Read = [](const ISeekInStream* p, void* buf, size_t* size) -> SRes {
				 MyFileInStream* fis = Z7_CONTAINER_FROM_VTBL(p, MyFileInStream, vt);
				 const size_t size_to_read = *size;
				 const auto bytes_read = std::fread(buf, 1, size_to_read, fis->fp);
				 *size = (bytes_read >= 0) ? bytes_read : 0;
				 return (bytes_read == size_to_read) ? SZ_OK : SZ_ERROR_READ;
			 },
				.Seek = [](const ISeekInStream* p, Int64* pos, ESzSeek origin) -> SRes {
					MyFileInStream* fis = Z7_CONTAINER_FROM_VTBL(p, MyFileInStream, vt);
					static_assert(SZ_SEEK_CUR == SEEK_CUR && SZ_SEEK_SET == SEEK_SET && SZ_SEEK_END == SEEK_END);
					if (FileSystem::FSeek64(fis->fp, *pos, static_cast<int>(origin)) != 0)
						return SZ_ERROR_READ;

					const s64 new_pos = FileSystem::FTell64(fis->fp);
					if (new_pos < 0)
						return SZ_ERROR_READ;

					*pos = new_pos;
					return SZ_OK;
				}},
			m_fp.get()};

		CLookToRead2 look_stream = {};
		LookToRead2_INIT(&look_stream);
		LookToRead2_CreateVTable(&look_stream, False);
		look_stream.realStream = &fis.vt;
		look_stream.bufSize = kInputBufSize;
		look_stream.buf = static_cast<Byte*>(ISzAlloc_Alloc(&g_Alloc, kInputBufSize));
		if (!look_stream.buf)
		{
			Error::SetString(error, "Failed to allocate lookahead buffer");
			return false;
		}
		ScopedGuard guard = [&look_stream]() {
			if (look_stream.buf)
				ISzAlloc_Free(&g_Alloc, look_stream.buf);
		};

		// Read blocks
		CXzs xzs;
		Xzs_Construct(&xzs);
		const ScopedGuard xzs_guard([&xzs]() {
			Xzs_Free(&xzs, &g_Alloc);
		});

		const s64 file_size = FileSystem::FSize64(m_fp.get());
		Int64 start_pos = file_size;
		SRes res = Xzs_ReadBackward(&xzs, &look_stream.vt, &start_pos, nullptr, &g_Alloc);
		if (res != SZ_OK)
		{
			Error::SetString(error, fmt::format("Xzs_ReadBackward() failed: {}", res));
			return false;
		}

		const size_t num_blocks = Xzs_GetNumBlocks(&xzs);
		if (num_blocks == 0)
		{
			Error::SetString(error, "Stream has no blocks.");
			return false;
		}

		m_blocks.reserve(num_blocks);
		for (int sn = xzs.num - 1; sn >= 0; sn--)
		{
			const CXzStream& stream = xzs.streams[sn];
			size_t src_offset = stream.startOffset + XZ_STREAM_HEADER_SIZE;
			for (size_t bn = 0; bn < stream.numBlocks; bn++)
			{
				const CXzBlockSizes& block = stream.blocks[bn];

				Block out_block;
				out_block.file_offset = src_offset;
				out_block.stream_offset = m_stream_size;
				out_block.compressed_size = std::min<size_t>(Common::AlignUpPow2(block.totalSize, 4),
					static_cast<size_t>(file_size - static_cast<s64>(src_offset))); // LZMA blocks are 4 byte aligned?
				out_block.uncompressed_size = block.unpackSize;
				out_block.stream_flags = stream.flags;

				m_stream_size += out_block.uncompressed_size;
				src_offset += out_block.compressed_size;
				m_blocks.push_back(std::move(out_block));
			}
		}

		DevCon.WriteLnFmt("XZ stream is {} bytes across {} blocks", m_stream_size, m_blocks.size());

		if (unpacker_constructed)
			XzUnpacker_Free(&m_unpacker);
		XzUnpacker_Construct(&m_unpacker, &g_Alloc);
		unpacker_constructed = true;
		return true;
	}

	s64 GSStreamLzma::ReadBlock(void* ptr, size_t size, Error* error)
	{
		if (IsEof())
			return READ_EOF;

		Block& block = m_blocks[m_block_index];

		if (!block.loading)
		{
			if (block.compressed_size > m_block_read_buffer.size())
				m_block_read_buffer.resize(Common::AlignUpPow2(block.compressed_size, _128kb));

			if (FileSystem::FSeek64(m_fp.get(), static_cast<s64>(block.file_offset), SEEK_SET) != 0 ||
				std::fread(m_block_read_buffer.data(), block.compressed_size, 1, m_fp.get()) != 1)
			{
				Error::SetStringFmt(error, "Failed to read {} bytes from offset {}", block.file_offset, block.compressed_size);
				return READ_ERROR;
			}

			XzUnpacker_Init(&m_unpacker);
			m_unpacker.streamFlags = block.stream_flags;
			XzUnpacker_PrepareToRandomBlockDecoding(&m_unpacker);
			
			block.loading = true;
		}

		pxAssert(block.read_offset < block.compressed_size);

		SizeT uncompressed_available = size;
		SizeT compressed_available = block.compressed_size - block.read_offset;

		ECoderStatus status;
		const SRes res = XzUnpacker_Code(&m_unpacker, static_cast<Byte*>(ptr), &uncompressed_available,
			m_block_read_buffer.data() + block.read_offset, &compressed_available, true, CODER_FINISH_ANY, &status);
		if (res != SZ_OK) [[unlikely]]
		{
			Error::SetStringFmt(error, "XzUnpacker_Code() failed: {} (status {})", res, static_cast<unsigned>(status));
			return READ_ERROR;
		}

		block.read_offset += compressed_available;
		block.write_offset += uncompressed_available;

		if (status == CODER_STATUS_FINISHED_WITH_MARK)
		{
			if (block.read_offset != block.compressed_size || block.write_offset != block.uncompressed_size)
			{
				Console.WarningFmt("Decompress size mismatch: {}/{} vs {}/{}", block.read_offset, block.write_offset,
					block.compressed_size, block.uncompressed_size);
			}
			block.loading = false;
			block.read_offset = 0;
			block.write_offset = 0;
			m_block_index++;
		}
		return static_cast<s64>(uncompressed_available);
	}

	s64 GSStreamLzma::Read(void* ptr, size_t size, Error* error)
	{
		if (IsEof())
		{
			Error::SetString(error, "(GSStreamLzma) EOF");
			return READ_EOF;
		}
		Byte* p = static_cast<Byte*>(ptr);
		while (size > 0 && !IsEof())
		{
			s64 ret = ReadBlock(p, size, error);
			if (ret == READ_ERROR)
				return READ_ERROR;
			if (ret == READ_EOF)
				break;
			p += ret;
			size -= ret;
		}
		return p - static_cast<Byte*>(ptr);
	}

	bool GSStreamLzma::IsEof()
	{
		return m_block_index >= m_blocks.size();
	}

	class GSDumpLzma final : public GSDumpFile
	{
	public:
		GSDumpLzma();
		~GSDumpLzma() override;

	protected:
		bool Open(FileSystem::ManagedCFilePtr fp, Error* error) override;
		bool IsEof() override;
		size_t Read(void* ptr, size_t size) override;
		s64 GetFileSize() override;

	private:
		static constexpr size_t kInputBufSize = static_cast<size_t>(1) << 18;

		struct Block
		{
			size_t file_offset;
			size_t stream_offset;
			size_t compressed_size;
			size_t uncompressed_size;
			CXzStreamFlags stream_flags;
		};

		bool DecompressNextBlock();


		std::vector<Block> m_blocks;
		size_t m_stream_size = 0;

		DynamicHeapArray<u8, 64> m_block_buffer;
		size_t m_block_index = 0;
		size_t m_block_size = 0;
		size_t m_block_pos = 0;

		DynamicHeapArray<u8, 64> m_block_read_buffer;
		alignas(__cachelinesize) CXzUnpacker m_unpacker = {};
	};

	GSDumpLzma::GSDumpLzma() = default;

	GSDumpLzma::~GSDumpLzma()
	{
		XzUnpacker_Free(&m_unpacker);
	}

	bool GSDumpLzma::Open(FileSystem::ManagedCFilePtr fp, Error* error)
	{
		m_fp = std::move(fp);
		m_size_compressed = FileSystem::FSize64(m_fp.get());

		GSInit7ZCRCTables();

		struct MyFileInStream
		{
			ISeekInStream vt;
			std::FILE* fp;
		};

		MyFileInStream fis = {
			{.Read = [](const ISeekInStream* p, void* buf, size_t* size) -> SRes {
				 MyFileInStream* fis = Z7_CONTAINER_FROM_VTBL(p, MyFileInStream, vt);
				 const size_t size_to_read = *size;
				 const auto bytes_read = std::fread(buf, 1, size_to_read, fis->fp);
				 *size = (bytes_read >= 0) ? bytes_read : 0;
				 return (bytes_read == size_to_read) ? SZ_OK : SZ_ERROR_READ;
			 },
				.Seek = [](const ISeekInStream* p, Int64* pos, ESzSeek origin) -> SRes {
					MyFileInStream* fis = Z7_CONTAINER_FROM_VTBL(p, MyFileInStream, vt);
					static_assert(SZ_SEEK_CUR == SEEK_CUR && SZ_SEEK_SET == SEEK_SET && SZ_SEEK_END == SEEK_END);
					if (FileSystem::FSeek64(fis->fp, *pos, static_cast<int>(origin)) != 0)
						return SZ_ERROR_READ;

					const s64 new_pos = FileSystem::FTell64(fis->fp);
					if (new_pos < 0)
						return SZ_ERROR_READ;

					*pos = new_pos;
					return SZ_OK;
				}},
			m_fp.get()};

		CLookToRead2 look_stream = {};
		LookToRead2_INIT(&look_stream);
		LookToRead2_CreateVTable(&look_stream, False);
		look_stream.realStream = &fis.vt;
		look_stream.bufSize = kInputBufSize;
		look_stream.buf = static_cast<Byte*>(ISzAlloc_Alloc(&g_Alloc, kInputBufSize));
		if (!look_stream.buf)
		{
			Error::SetString(error, "Failed to allocate lookahead buffer");
			return false;
		}
		ScopedGuard guard = [&look_stream]() {
			if (look_stream.buf)
				ISzAlloc_Free(&g_Alloc, look_stream.buf);
		};

		// Read blocks
		CXzs xzs;
		Xzs_Construct(&xzs);
		const ScopedGuard xzs_guard([&xzs]() {
			Xzs_Free(&xzs, &g_Alloc);
		});

		const s64 file_size = FileSystem::FSize64(m_fp.get());
		Int64 start_pos = file_size;
		SRes res = Xzs_ReadBackward(&xzs, &look_stream.vt, &start_pos, nullptr, &g_Alloc);
		if (res != SZ_OK)
		{
			Error::SetString(error, fmt::format("Xzs_ReadBackward() failed: {}", res));
			return false;
		}

		const size_t num_blocks = Xzs_GetNumBlocks(&xzs);
		if (num_blocks == 0)
		{
			Error::SetString(error, "Stream has no blocks.");
			return false;
		}

		m_blocks.reserve(num_blocks);
		for (int sn = xzs.num - 1; sn >= 0; sn--)
		{
			const CXzStream& stream = xzs.streams[sn];
			size_t src_offset = stream.startOffset + XZ_STREAM_HEADER_SIZE;
			for (size_t bn = 0; bn < stream.numBlocks; bn++)
			{
				const CXzBlockSizes& block = stream.blocks[bn];

				Block out_block;
				out_block.file_offset = src_offset;
				out_block.stream_offset = m_stream_size;
				out_block.compressed_size = std::min<size_t>(Common::AlignUpPow2(block.totalSize, 4),
					static_cast<size_t>(file_size - static_cast<s64>(src_offset))); // LZMA blocks are 4 byte aligned?
				out_block.uncompressed_size = block.unpackSize;
				out_block.stream_flags = stream.flags;
				m_stream_size += out_block.uncompressed_size;
				src_offset += out_block.compressed_size;
				m_blocks.push_back(std::move(out_block));
			}
		}

		DevCon.WriteLnFmt("XZ stream is {} bytes across {} blocks", m_stream_size, m_blocks.size());
		XzUnpacker_Construct(&m_unpacker, &g_Alloc);
		return true;
	}

	bool GSDumpLzma::DecompressNextBlock()
	{
		if (m_block_index == m_blocks.size())
			return false;

		const Block& block = m_blocks[m_block_index];

		if (block.compressed_size > m_block_read_buffer.size())
			m_block_read_buffer.resize(Common::AlignUpPow2(block.compressed_size, _128kb));

		if (FileSystem::FSeek64(m_fp.get(), static_cast<s64>(block.file_offset), SEEK_SET) != 0 ||
			std::fread(m_block_read_buffer.data(), block.compressed_size, 1, m_fp.get()) != 1)
		{
			Console.ErrorFmt("Failed to read {} bytes from offset {}", block.file_offset, block.compressed_size);
			return false;
		}

		if (block.uncompressed_size > m_block_buffer.size())
			m_block_buffer.resize(Common::AlignUpPow2(block.uncompressed_size, _128kb));

		XzUnpacker_Init(&m_unpacker);
		m_unpacker.streamFlags = block.stream_flags;
		XzUnpacker_PrepareToRandomBlockDecoding(&m_unpacker);
		XzUnpacker_SetOutBuf(&m_unpacker, m_block_buffer.data(), block.uncompressed_size);
		SizeT out_uncompressed_size = block.uncompressed_size;
		SizeT out_compressed_size = block.compressed_size;

		ECoderStatus status;
		const SRes res = XzUnpacker_Code(&m_unpacker, nullptr, &out_uncompressed_size,
			m_block_read_buffer.data(), &out_compressed_size, true, CODER_FINISH_END, &status);
		if (res != SZ_OK || status != CODER_STATUS_FINISHED_WITH_MARK) [[unlikely]]
		{
			Console.ErrorFmt("XzUnpacker_Code() failed: {} (status {})", res, static_cast<unsigned>(status));
			return false;
		}

		if (out_compressed_size != block.compressed_size || out_uncompressed_size != block.uncompressed_size)
		{
			Console.WarningFmt("Decompress size mismatch: {}/{} vs {}/{}", out_compressed_size, out_uncompressed_size,
				block.compressed_size, block.uncompressed_size);
		}

		m_block_size = out_uncompressed_size;
		m_block_pos = 0;
		m_block_index++;
		return true;
	}

	bool GSDumpLzma::IsEof()
	{
		return (m_block_pos == m_block_size && m_block_index == m_blocks.size());
	}

	size_t GSDumpLzma::Read(void* ptr, size_t size)
	{
		u8* dst = static_cast<u8*>(ptr);
		size_t remain = size;
		while (remain > 0)
		{
			if (m_block_size == m_block_pos && !DecompressNextBlock()) [[unlikely]]
				break;

			const size_t avail = (m_block_size - m_block_pos);
			const size_t read = std::min(avail, remain);
			pxAssert(avail > 0 && read > 0);
			std::memcpy(dst, &m_block_buffer[m_block_pos], read);
			dst += read;
			remain -= read;
			m_block_pos += read;
		}

		return size - remain;
	}

	s64 GSDumpLzma::GetFileSize()
	{
		return m_size_compressed;
	}

	/******************************************************************/

	class GSDumpDecompressZst final : public GSDumpFile
	{
		static constexpr u32 INPUT_BUFFER_SIZE = 512 * _1kb;
		static constexpr u32 OUTPUT_BUFFER_SIZE = 2 * _1mb;

		ZSTD_DStream* m_strm = nullptr;
		ZSTD_inBuffer m_inbuf = {};

		uint8_t* m_area = nullptr;

		size_t m_avail = 0;
		size_t m_start = 0;

		bool Decompress();

	public:
		GSDumpDecompressZst();
		~GSDumpDecompressZst() override;

		bool Open(FileSystem::ManagedCFilePtr fp, Error* error) override;
		bool IsEof() override;
		size_t Read(void* ptr, size_t size) override;
		s64 GetFileSize() override;
	};

	GSDumpDecompressZst::GSDumpDecompressZst() = default;

	GSDumpDecompressZst::~GSDumpDecompressZst()
	{
		if (m_strm)
			ZSTD_freeDStream(m_strm);

		if (m_inbuf.src)
			_aligned_free(const_cast<void*>(m_inbuf.src));
		if (m_area)
			_aligned_free(m_area);
	}

	bool GSDumpDecompressZst::Open(FileSystem::ManagedCFilePtr fp, Error* error)
	{
		m_fp = std::move(fp);
		m_strm = ZSTD_createDStream();
		m_size_compressed = FileSystem::FSize64(m_fp.get());

		m_area = static_cast<uint8_t*>(_aligned_malloc(OUTPUT_BUFFER_SIZE, 32));
		m_inbuf.src = static_cast<uint8_t*>(_aligned_malloc(INPUT_BUFFER_SIZE, 32));
		m_inbuf.pos = 0;
		m_inbuf.size = 0;
		m_avail = 0;
		m_start = 0;
		return true;
	}

	bool GSDumpDecompressZst::Decompress()
	{
		ZSTD_outBuffer outbuf = {m_area, OUTPUT_BUFFER_SIZE, 0};
		while (outbuf.pos == 0)
		{
			// Nothing left in the input buffer. Read data from the file
			if (m_inbuf.pos == m_inbuf.size && !std::feof(m_fp.get()))
			{
				m_inbuf.size = fread(const_cast<void*>(m_inbuf.src), 1, INPUT_BUFFER_SIZE, m_fp.get());
				m_inbuf.pos = 0;

				if (ferror(m_fp.get()))
				{
					Console.Error("Zst read error: %s", strerror(errno));
					return false;
				}
			}

			const size_t ret = ZSTD_decompressStream(m_strm, &outbuf, &m_inbuf);
			if (ZSTD_isError(ret))
			{
				Console.Error("Decoder error: (error code %s)", ZSTD_getErrorName(ret));
				return false;
			}
		}

		m_start = 0;
		m_avail = outbuf.pos;
		return true;
	}

	bool GSDumpDecompressZst::IsEof()
	{
		return feof(m_fp.get()) && m_avail == 0 && m_inbuf.pos == m_inbuf.size;
	}

	size_t GSDumpDecompressZst::Read(void* ptr, size_t size)
	{
		size_t off = 0;
		uint8_t* dst = static_cast<uint8_t*>(ptr);
		while (size && !IsEof())
		{
			if (m_avail == 0)
			{
				if (!Decompress()) [[unlikely]]
					break;
			}

			const size_t l = std::min(size, m_avail);
			std::memcpy(dst + off, m_area + m_start, l);
			m_avail -= l;
			size -= l;
			m_start += l;
			off += l;
		}

		return off;
	}

	s64 GSDumpDecompressZst::GetFileSize()
	{
		return m_size_compressed;
	}

	/******************************************************************/

	class GSDumpRaw final : public GSDumpFile
	{
	public:
		GSDumpRaw();
		~GSDumpRaw() override;

		bool Open(FileSystem::ManagedCFilePtr fp, Error* error) override;
		bool IsEof() override;
		size_t Read(void* ptr, size_t size) override;
		s64 GetFileSize() override;
	};

	GSDumpRaw::GSDumpRaw() = default;

	GSDumpRaw::~GSDumpRaw() = default;

	bool GSDumpRaw::Open(FileSystem::ManagedCFilePtr fp, Error* error)
	{
		m_fp = std::move(fp);
		m_size = FileSystem::FSize64(m_fp.get());
		return true;
	}

	bool GSDumpRaw::IsEof()
	{
		return !!feof(m_fp.get());
	}

	size_t GSDumpRaw::Read(void* ptr, size_t size)
	{
		size_t ret = fread(ptr, 1, size, m_fp.get());
		if (ret != size && ferror(m_fp.get()))
		{
			fprintf(stderr, "GSDumpRaw:: Read error (%zu/%zu)\n", ret, size);
		}

		return ret;
	}

	s64 GSDumpRaw::GetFileSize()
	{
		return m_size;
	}

	class GSDumpMemory final : public GSDumpFile
	{
		friend class GSDumpFile;
	private:
		const u8* data;
		size_t curr = 0;

		void Init(const u8* data, size_t curr);
	public:
		GSDumpMemory(const u8* data, size_t size);
		~GSDumpMemory() override;

		bool Open(FileSystem::ManagedCFilePtr fp, Error* error) override;
		bool IsEof() override;
		size_t Read(void* ptr, size_t size) override;
		s64 GetFileSize() override;
	};

	void GSDumpMemory::Init(const u8* data, size_t size)
	{
		this->data = data;
		m_size = static_cast<s64>(size);
		curr = 0;
	}

	GSDumpMemory::GSDumpMemory(const u8* data, size_t size)
	{
		Init(data, size);
	}

	GSDumpMemory::~GSDumpMemory() = default;

	bool GSDumpMemory::Open(FileSystem::ManagedCFilePtr fp, Error* error)
	{
		pxFail("Not implemented.");
		return false;
	}

	bool GSDumpMemory::IsEof()
	{
		return curr >= static_cast<size_t>(m_size);
	}

	size_t GSDumpMemory::Read(void* ptr, size_t size)
	{
		if (IsEof())
			size = 0;
		else
			size = std::min(size, static_cast<size_t>(m_size) - curr);
		memcpy(ptr, &data[curr], size);
		curr += size;
		return size;
	}

	s64 GSDumpMemory::GetFileSize()
	{
		return m_size;
	}
} // namespace

GSDumpLazy::GSDumpLazy(size_t buffer_size) : buffer_size(buffer_size)
{
}

void GSDumpLazy::_ResetState()
{
	read_buffer = 0;
	parse_buffer = 0;
	write_buffer = 0;
	read_packet = 0;
	write_packet = 0;
	reading_packet = false;
	eof_stream = false;
	parse_error = false;
}

void GSDumpLazy::Init()
{
	buffer.resize(buffer_size + pad_size);
	packets.resize(buffer_size / 128);

	_ResetState();

	this->thread = std::thread(&GSDumpLazy::_LoaderFunc, this);
}

bool GSDumpLazy::OpenNext(const std::string& filename, Error* error)
{
	std::unique_lock lock(mut);

	return _OpenNext(filename, error, lock);
}

bool GSDumpLazy::_OpenNext(const std::string& filename, Error* error, std::unique_lock<std::mutex>& lock)
{
	cond_read.wait(lock, [&]() { return _Stopped() || !_HasNext(); });

	if (_Stopped())
	{
		Error::SetString(error, "(GSDumpLazy) Cannot open next when stopped.");
		return false;
	}

	filename_next = filename;
	cond_write.notify_one();
	return true;
}

GSDumpLazy::~GSDumpLazy()
{
	Stop();
}

void GSDumpLazy::_DebugCheck() const
{
	pxAssert(
		read_buffer <= write_buffer && write_buffer <= read_buffer + buffer_size &&
		read_buffer <= parse_buffer && parse_buffer <= write_buffer &&
		read_packet <= write_packet && write_packet <= read_packet + packets.size());
}

bool GSDumpLazy::_NotLoading() const
{
	return !stream && !_HasNext();
}

bool GSDumpLazy::_Stopped() const
{
	return stop || !thread.joinable();
}

bool GSDumpLazy::_HasNext() const
{
	return !filename_next.empty();
}

bool GSDumpLazy::_EofStream() const
{
	return !stream || eof_stream;
}

bool GSDumpLazy::_Eof() const
{
	_DebugCheck();
	return _EmptyBuffer() && _EmptyPackets() && _EofStream();
}

bool GSDumpLazy::_FullBuffer() const
{
	_DebugCheck();
	return (write_buffer - read_buffer) >= buffer_size;
}

bool GSDumpLazy::_FullPackets() const
{
	_DebugCheck();
	return (write_packet - read_packet) >= packets.size();
}

bool GSDumpLazy::_EmptyBuffer() const
{
	_DebugCheck();
	return write_buffer == read_buffer;
}

bool GSDumpLazy::_EmptyPackets() const
{
	_DebugCheck();
	return write_packet == read_packet;
}

bool GSDumpLazy::_LoadCond() const
{
	_DebugCheck();
	// Want more data when the buffer is less than half full or there packet buffer is empty.
	// The latter condition makes it more likely that EOFs are detected correctly in case there
	// is an incomplete packet at the end.
	bool load_data = ((write_buffer - read_buffer) <= (buffer_size / 2)) && !_EofStream();
	bool parse_packets = _EmptyPackets() && !_EmptyBuffer() && !_FullPackets();
	return load_data || parse_packets;
}

void GSDumpLazy::Stop()
{
	{
		std::unique_lock lock(mut);

		stop = true;
	}

	cond_write.notify_all();
	cond_read.notify_all();

	if (thread.joinable())
		thread.join();
}

bool GSDumpLazy::DonePackets(size_t i) const
{
	std::unique_lock lock(mut);

	if (reading_packet)
	{
		if (i != read_packet + 1)
			Console.ErrorFmt("GSDumpLazy: Incorrect index in DonePackets (got {}; expected {})", i, read_packet + 1);
		return write_packet == read_packet + 1;
	}
	else
	{
		if (i != read_packet)
			Console.ErrorFmt("GSDumpLazy: Incorrect index in DonePackets (got {}; expected {})", i, read_packet);
		return write_packet == read_packet;
	}
}

bool GSDumpLazy::_FillBytesRaw(std::unique_ptr<GSStream>& stream, std::vector<u8>& buffer, size_t buffer_size,
	size_t& _write_buffer, size_t& _read_buffer, Error* error)
{
	// First iteration: fill to buffer end.
	// Second iteration: fill to read index.
	for (int i = 0; i < 2 && !stream->IsEof(); i++)
	{
		size_t mod_write = _write_buffer % buffer_size;
		size_t mod_read = _read_buffer % buffer_size;
		size_t try_read_size;
		if (i == 0 && mod_write >= mod_read)
		{
			// Fill up to the buffer end.
			try_read_size = buffer_size - mod_write;
		}
		else if (i == 1 && mod_write < mod_read)
		{
			// Fill up to the read index.
			try_read_size = mod_read - mod_write;
		}
		else
		{
			continue;
		}
		s64 read_size = stream->Read(buffer.data() + mod_write, try_read_size, error);

		if (read_size == GSStream::READ_ERROR)
			return false;

		_write_buffer += read_size;
	}
	return true;
}

size_t GSDumpLazy::_CopyPadBytes(std::vector<u8>& buffer, size_t buffer_size, size_t _write_buffer, size_t _read_buffer)
{
	// Copy pad bytes if the range wraps.
	size_t mod_write = _write_buffer % buffer_size;
	size_t mod_read = _read_buffer % buffer_size;
	if (_read_buffer < _write_buffer && mod_write <= mod_read)
	{
		size_t use_pad_size = std::min(mod_write, pad_size);
		std::memcpy(buffer.data() + buffer_size, buffer.data(), use_pad_size);
		return use_pad_size;
	}
	else
	{
		return 0;
	}
}

bool GSDumpLazy::_ParsePackets(std::vector<u8>& buffer, size_t buffer_size, std::vector<PacketInfo>& packets,
	size_t& _write_buffer, size_t& _read_buffer, size_t& _parse_buffer, size_t& _write_packet,
	size_t& _read_packet, size_t use_pad_size, bool eof_stream, Error* _error)
{
	while (_write_packet - _read_packet < packets.size() && _parse_buffer < _write_buffer)
	{
		size_t mod_parse = _parse_buffer % buffer_size;
		size_t bytes_unparsed = _write_buffer - _parse_buffer;
		size_t bytes_padding = buffer_size + use_pad_size - mod_parse;
		size_t bytes_available = std::min(bytes_padding, bytes_unparsed);
		size_t mod_packet = _write_packet % packets.size();
		s64 packet_size = GSDumpFile::ReadOnePacket(
			buffer.data() + mod_parse,
			buffer.data() + mod_parse + bytes_available,
			packets[mod_packet].data,
			_error);
		if (packet_size == PACKET_FAILURE)
			return false;

		if (packet_size == PACKET_OUT_OF_DATA)
		{
			if (bytes_padding <= bytes_unparsed && use_pad_size >= pad_size)
			{
				Error::SetStringFmt(_error, "GSDumpLazy: Packet exceeded the pad region ({} bytes).", pad_size);
				return false;
			}
			if (eof_stream) // Skip any incomplete last packets.
				_parse_buffer = _write_buffer;
			return true;
		}
		pxAssert(packet_size > 0);

		packets[mod_packet].packet_num = _write_packet;

		packets[mod_packet].buffer_start = _parse_buffer;
		_parse_buffer += static_cast<size_t>(packet_size);
		packets[mod_packet].buffer_end = _parse_buffer;

		_write_packet++;
	}
	return true;
}

void GSDumpLazy::_LoaderFunc(GSDumpLazy* dump)
{
	std::mutex& mut = dump->mut;
	std::unique_ptr<GSStream>& stream = dump->stream;
	std::string& filename = dump->filename;
	std::string& filename_next = dump->filename_next;
	const size_t buffer_size = dump->buffer_size;
	std::vector<u8>& buffer = dump->buffer;
	std::vector<PacketInfo>& packets = dump->packets;
	std::condition_variable& cond_write = dump->cond_write;
	std::condition_variable& cond_read = dump->cond_read;
	bool& stop = dump->stop;
	std::vector<u8>& m_regs_data = dump->m_regs_data;
	std::vector<u8>& m_state_data = dump->m_state_data;
	const auto _LoadCond = std::bind(&GSDumpLazy::_LoadCond, dump);
	const auto _ResetState = std::bind(&GSDumpLazy::_ResetState, dump);
	const auto _Stopped = std::bind(&GSDumpLazy::_Stopped, dump);
	const auto _HasNext = std::bind(&GSDumpLazy::_HasNext, dump);

	while (true)
	{
		size_t _read_buffer;
		size_t _parse_buffer;
		size_t _write_buffer;
		size_t _read_packet;
		size_t _write_packet;
		Error _error;
		bool _parse_error = false;
		bool _eof_actual = false;

		// Acquire the range.
		{
			std::unique_lock lock(mut);

			cond_write.wait(lock, [&] {
				if (_Stopped())
					return true;
				if (dump->parse_error)
					return false; // Block until consumer adds a new dump.
				return _LoadCond() || _HasNext();
			});

			if (_Stopped())
				return;

			if (_HasNext())
			{
				_ResetState();
				filename = filename_next;
				filename_next.clear();
				dump->parse_error = false;
				if (!GSStream::OpenStream(stream, filename.c_str(), &dump->error))
					dump->parse_error = true;
				if (!dump->parse_error &&
					!_FillBytesRaw(stream, buffer, buffer_size, dump->write_buffer, dump->read_buffer, &dump->error))
					_parse_error = true;
				if (!dump->parse_error && !dump->ReadHeaderStateRegs(&_error))
					dump->parse_error = true;
				cond_read.notify_one();
				continue;
			}

			// Otherwise load condition is met so acquire range.
			_read_buffer = dump->read_buffer;
			_parse_buffer = dump->parse_buffer;
			_write_buffer = dump->write_buffer;
			_read_packet = dump->read_packet;
			_write_packet = dump->write_packet;
		}

		if (!_FillBytesRaw(stream, buffer, buffer_size, _write_buffer, _read_buffer, &_error))
			_parse_error = true;

		size_t use_pad_size = 0;
		if (!_parse_error)
			use_pad_size = _CopyPadBytes(buffer, buffer_size, _write_buffer, _read_buffer);

		// Parse as many packets as possible.
		if (!_parse_error &&
			!_ParsePackets(buffer, buffer_size, packets, _write_buffer, _read_buffer, _parse_buffer,
				_write_packet, _read_packet, use_pad_size, stream->IsEof(), &_error))
			_parse_error = true;

		// Should not really happen.
		if (_write_packet - _read_packet >= packets.size())
			Console.WarningFmt("GSDumpLazy: Packet buffer is full when loading new data.");

		// Release the range.
		{
			std::unique_lock lock(mut);

			dump->parse_buffer = _parse_buffer;
			dump->write_buffer = _write_buffer;
			dump->write_packet = _write_packet;
			dump->eof_stream = stream->IsEof();
			if (_parse_error)
			{
				dump->parse_error = true;
				dump->error = _error;
			}

			cond_read.notify_one();
		}
	}
}

s64 GSDumpLazy::GetPacket(size_t i, GSData& data, Error* error)
{
	std::unique_lock lock(mut);

	if (parse_error)
	{
		if (error)
			*error = this->error;
		parse_error = false;
		return PACKET_FAILURE;
	}
	if (_Stopped())
	{
		Error::SetString(error, "(GSDumpLazy) Attempting to get packets when stopped.");
		return PACKET_STOP;
	}
	if (_NotLoading())
	{
		Error::SetString(error, "(GSDumpLazy) Attempting to get packet when no dumps are loading.");
		return PACKET_FAILURE;
	}

	// Release the currently reading packet if any.
	if (reading_packet)
	{
		const PacketInfo& info = packets[read_packet % packets.size()];
		pxAssert(read_buffer == info.buffer_start);
		read_buffer = info.buffer_end;
		read_packet++;
		reading_packet = false;

		cond_write.notify_one();
	}

	// Check if we must loop back to the beginning.
	bool loop = (i == 0 && read_packet > 0);
	bool reopen = false;
	if (loop)
	{
		if (_EofStream() && packets[0].packet_num == 0 && write_buffer <= buffer_size)
		{
			// Safe to rewind.
			read_packet = 0;
			read_buffer = packets[0].buffer_start;
		}
		else
		{
			// Not safe to rewind so reopen the file.
			if (!_OpenNext(filename, error, lock))
				return PACKET_FAILURE;
			reopen = true;
		}

		cond_write.notify_one();
	}
	
	// Let producer do it's thing.
	cond_read.wait(lock, [&]() {
		if (_Stopped() || _NotLoading() || parse_error)
			return true;
		if (reopen)
			return !_HasNext() && !_EmptyPackets(); // Next dump should be loaded before we proceed.
		else if (loop)
			return !_EmptyPackets(); // Don't check EOF because it may be for the previous loop.
		else
			return !_EmptyPackets() || _Eof();
	});

	if (parse_error)
	{
		if (error)
			*error = this->error;
		return PACKET_FAILURE;
	}
	if (_Eof() && !loop)
	{
		Error::SetString(error, "(GSDumpLazy) Attempting to get packets when EOF.");
		return PACKET_EOF;
	}
	if (_Stopped())
	{
		Error::SetString(error, "(GSDumpLazy) Attempting to get packets when stopped.");
		return PACKET_STOP;
	}
	if (_NotLoading())
	{
		Error::SetString(error, "(GSDumpLazy) Attempting to get packet when no dumps are loading.");
		return PACKET_FAILURE;
	}

	// We only support reading packets sequentially (other than looping).
	if (i != read_packet)
	{
		Error::SetStringFmt(error, "GSDumpLazy: Incorrect GetPacket() index (got {}; expected {})", i, read_packet);
		return PACKET_FAILURE;
	}

	// Get the next packet in the buffer.
	const PacketInfo& info = packets[read_packet % packets.size()];
	pxAssert(read_buffer == info.buffer_start);
	data = info.data;
	size_t size = info.buffer_end - info.buffer_start;
	reading_packet = true;

	if (_LoadCond())
		cond_write.notify_one();

	return size;
}

bool GSDumpLazy::Open(FileSystem::ManagedCFilePtr fp, Error* error)
{
	if (error)
		Error::SetString(error, "GSDumpLazy::Open() not implemented");
	pxFail("GSDumpLazy::Open() not implemented");
	return false;
}

bool GSDumpLazy::IsEof()
{
	Console.Error("GSDumpLazy::IsEof() not implemented");
	pxFail("GSDumpLazy::IsEof() not implemented");
	return false;
}

size_t GSDumpLazy::Read(void* ptr, size_t size)
{
	// Only call by the producer with the lock held!
	if (parse_error || _Eof() || !_EmptyPackets() || _NotLoading() || _Stopped())
		return 0;

	if (read_buffer + size > write_buffer)
		return 0;

	size_t read_size = 0;
	for (int i = 0; i < 2 && read_size < size; i++)
	{
		size_t mod_read = read_buffer % buffer_size;
		size_t mod_write = write_buffer % buffer_size;
		size_t copy_size;
		if (mod_read < mod_write)
			copy_size = mod_write - mod_read;
		else
			copy_size = buffer_size - mod_read;
		copy_size = std::min(size, copy_size);
		std::memcpy(ptr, buffer.data() + mod_read, copy_size);
		read_size += copy_size;
		read_buffer += copy_size;
		parse_buffer += copy_size;
	}
	pxAssert(read_size == size);
	return read_size;
}

s64 GSDumpLazy::GetFileSize()
{
	std::unique_lock lock(mut);

	cond_read.wait(lock, [&]() { return _Stopped() || _NotLoading() || stream; });

	if (_Stopped())
	{
		Console.Error("GSDumpLazy::GetFileSize() loading stopped.");
		return 0;
	}

	if (_NotLoading())
	{
		Console.Error("GSDumpLazy::GetFileSize() no dumps being loaded.");
		return 0;
	}

	return stream->GetSizeCompressed();
}

const GSDumpFile::ByteArray& GSDumpLazy::GetRegsData() const
{
	std::unique_lock lock(mut);

	cond_read.wait(lock, [&]() { return _Stopped() || _NotLoading() || !_HasNext(); });

	if (_Stopped())
	{
		Console.Error("GSDumpLazy::GetRegsData() loading stopped.");
		pxFail("GSDumpLazy::GetRegsData() loading stopped.");
	}

	if (_NotLoading())
	{
		Console.Error("GSDumpLazy::GetRegsData() no dumps being loaded.");
		pxFail("GSDumpLazy::GetRegsData() no dumps being loaded.");
	}

	return m_regs_data;
}

const GSDumpFile::ByteArray& GSDumpLazy::GetStateData() const
{
	std::unique_lock lock(mut);

	cond_read.wait(lock, [&]() { return _Stopped() || _NotLoading() || !_HasNext(); });

	if (_Stopped())
	{
		Console.Error("GSDumpLazy::GetStateData() loading stopped.");
		pxFail("GSDumpLazy::GetStateData() loading stopped.");
	}

	if (_NotLoading())
	{
		Console.Error("GSDumpLazy::GetStateData() no dumps being loaded.");
		pxFail("GSDumpLazy::GetStateData() no dumps being loaded.");
	}

	return m_state_data;
}

const GSDumpFile::GSDataArray& GSDumpLazy::GetPackets() const
{
	Console.Error("GSDumpLazy::GetPackets() not implemented.");
	pxFail("GSDumpLazy::GetPackets() not implemented.");
	return m_dump_packets;
}

/******************************************************************/

size_t GSDumpFile::GetPacketSize(const GSData& data)
{
	switch (data.id)
	{
		case GSType::Transfer:
			return 6 + data.length;
		case GSType::VSync:
		case GSType::ReadFIFO2:
		case GSType::Registers:
			return 1 + data.length;
		default:
			Console.ErrorFmt("GSDumpLazy: Unknown packet type {}", static_cast<int>(data.id)); // Impossible.
			pxFail("GSDumpLazy: Unknown packet type");
			return 0;
	}
}

bool GSStream::OpenStream(std::unique_ptr<GSStream>& stream, const char* filename, Error* error)
{
	FileSystem::ManagedCFilePtr fp = FileSystem::OpenManagedCFile(filename, "rb", error);
	if (!fp)
		return false;

	Type file_type;
	if (StringUtil::EndsWithNoCase(filename, ".xz"))
		file_type = LZMA;
	else if (StringUtil::EndsWithNoCase(filename, ".zst"))
		file_type = ZSTD;
	else
		file_type = RAW;

	if (!stream || stream->GetType() != file_type)
	{
		if (file_type == LZMA)
			stream = std::make_unique<GSStreamLzma>();
		else if (file_type == ZSTD)
			return false;
		else if (file_type == RAW)
			return false;
	}

	return stream->Open(std::move(fp), error);
}

std::unique_ptr<GSDumpFile> GSDumpFile::OpenGSDump(const char* filename, Error* error)
{
	FileSystem::ManagedCFilePtr fp = FileSystem::OpenManagedCFile(filename, "rb", error);
	if (!fp)
		return nullptr;

	std::unique_ptr<GSDumpFile> file;
	if (StringUtil::EndsWithNoCase(filename, ".xz"))
		file = std::make_unique<GSDumpLzma>();
	else if (StringUtil::EndsWithNoCase(filename, ".zst"))
		file = std::make_unique<GSDumpDecompressZst>();
	else
		file = std::make_unique<GSDumpRaw>();

	if (!file->Open(std::move(fp), error))
		file = {};

	return file;
}

void GSDumpFile::OpenGSDumpMemory(std::unique_ptr<GSDumpFile>& dump_, const void* ptr, const size_t size)
{
	if (!dump_)
	{
		dump_ = std::make_unique<GSDumpMemory>(static_cast<const u8*>(ptr), size);
	}
	else
	{
		GSDumpMemory* dump = reinterpret_cast<GSDumpMemory*>(dump_.get());
		dump->Clear();
		dump->Init(static_cast<const u8*>(ptr), size);
	}
}

bool GSDumpFile::OpenGSDumpLazy(std::unique_ptr<GSDumpLazy>& dump, size_t buffer_size, const char* filename, Error* error)
{
	if (!dump)
	{
		dump = std::make_unique<GSDumpLazy>(buffer_size);
		dump->Init();
	}
	return dump->OpenNext(filename, error);
}

GSDumpFileLoader::GSDumpFileLoader(size_t num_threads, size_t num_dumps_buffered, size_t max_file_size)
	: num_threads(num_threads)
	, num_dumps_buffered(num_dumps_buffered)
	, max_file_size(max_file_size)
	, dumps_avail_list(num_dumps_buffered)
{
	pxAssert(1 <= num_threads && num_threads <= num_dumps_buffered && num_dumps_buffered <= 8);
}

GSDumpFileLoader::~GSDumpFileLoader()
{
	Stop();
}

void GSDumpFileLoader::Start(const std::vector<std::string>& files, const std::string& from)
{
	std::vector <std::string> filenames = files;
	
	if (!from.empty())
	{
		std::erase_if(filenames, [&](const std::string& x) { return Path::GetFileName(x) < from; });
	}

	dump_list.resize(filenames.size());
	for (std::size_t i = 0; i < filenames.size(); i++)
		dump_list[i].filename = filenames[i];

	// Start threads
	for (size_t i = 0; i < num_threads; i++)
		threads.emplace_back(LoaderFunc, this);

	started = true;
}

bool GSDumpFileLoader::Started()
{
	return started;
}

bool GSDumpFileLoader::Finished()
{
	std::unique_lock lock(mut);

	return _DoneRead();
}

size_t GSDumpFileLoader::DumpsRemaining()
{
	std::unique_lock lock(mut);

	return dump_list.size() - read;
}

void GSDumpFileLoader::AddFile(const std::string& file)
{
	std::unique_lock lock(mut);

	dump_list.push_back(DumpInfo());
	dump_list.back().filename = file;

	cond_write.notify_one();
}

void GSDumpFileLoader::SetMaxFileSize(size_t size)
{
	std::unique_lock lock(mut);

	max_file_size = size;
}

void GSDumpFileLoader::_DebugCheck()
{
	// Must have mutex locked!
	pxAssertRel(
		read <= dump_list.size() &&
		read <= write &&
		write <= dump_list.size() &&
		write <= read + num_dumps_buffered,
		"Dump loader is in an inconsistent state.");
}

bool GSDumpFileLoader::_Full()
{
	// Must have mutex locked!

	_DebugCheck();

	return write >= read + num_dumps_buffered;
}

bool GSDumpFileLoader::_Empty()
{
	// Must have mutex locked!

	_DebugCheck();

	return read == write || (read < write && dump_list[read].state == WRITEABLE);
}

bool GSDumpFileLoader::_DoneRead()
{
	// Must have mutex locked!

	_DebugCheck();

	return read >= dump_list.size();
}

bool GSDumpFileLoader::_DoneWrite()
{
	// Must have mutex locked!

	_DebugCheck();

	return write >= dump_list.size();
}

bool GSDumpFileLoader::_Stopped()
{
	return stopped;
}

template <typename T>
	requires GSDumpFileLoader_IsDstType<T>
GSDumpFileLoader::ReturnValue GSDumpFileLoader::Get(T& dst, DumpInfo* info_out, bool block)
{
	size_t i; // Copy of read index.
	DumpInfo dump; // Copy of dump info.
	
	// Acquire the slot.
	Common::Timer block_timer;
	{
		std::unique_lock<std::mutex> lock(mut);

		cond_read.wait(lock, [&]() { return !block || !_Empty() || _DoneRead() || _Stopped(); });

		if (_DoneRead() || _Stopped())
		{
			cond_write.notify_all();

			return FINISHED;
		}

		if (_Empty())
		{
			return EMPTY;
		}

		i = read;
		dump = dump_list[i];

		pxAssert(dump.state == READABLE);
	}
	dump.block_time_read = block_timer.GetTimeSeconds();
	
	ReturnValue ret;

	// Error checking and writing to destination buffer.
	if (dump.error.empty())
	{
		if constexpr (std::same_as<T, std::unique_ptr<GSDumpFile>>)
		{
			GSDumpFile::OpenGSDumpMemory(dst, dump.data->data(), dump.data->size());

			Error error2;
			if (!dst->ReadFile(&error2))
			{
				dump.error = error2.GetDescription();
				ret = ERROR_;
			}
			else
			{
				ret = SUCCESS;
			}
		}
		else if constexpr (std::same_as<T, std::vector<u8>>)
		{
			dst.resize(dump.data->size());
			std::memcpy(dst.data(), dump.data->data(), dump.data->size());
			ret = SUCCESS;
		}
		else
		{
			static_assert(!std::is_same_v<T, T>, "Wrong type for Get()"); // Impossible.
		}
	}
	else
	{
		ret = ERROR_;
	}

	// Return info if needed.
	if (info_out)
	{
		*info_out = dump;
	}

	// Cleanup the allocated data.
	dump.data->clear();
	dump.data = nullptr;

	// Release the slot.
	{
		std::unique_lock<std::mutex> lock(mut);

		dump.state = DONE;
		dump_list[i] = std::move(dump);

		read = i + 1;
	}

	cond_write.notify_one();

	return ret;
}

// Instantiate templates.
template
GSDumpFileLoader::ReturnValue GSDumpFileLoader::Get<std::unique_ptr<GSDumpFile>>(
	std::unique_ptr<GSDumpFile>& dst,
	DumpInfo* info_out,
	bool block);

template
GSDumpFileLoader::ReturnValue GSDumpFileLoader::Get<std::vector<u8>>(
	std::vector<u8>& dst,
	DumpInfo* info_out,
	bool block);

void GSDumpFileLoader::LoaderFunc(GSDumpFileLoader* parent)
{
	while (true)
	{
		size_t i; // Copy of write index.
		DumpInfo dump; // Copy of dump info.

		// Acquire the slot.
		Common::Timer block_timer;
		{
			std::unique_lock<std::mutex> lock(parent->mut);

			parent->cond_write.wait(lock, [&]() { return (!parent->_Full() && !parent->_DoneWrite()) || parent->_Stopped(); });

			if (parent->_Stopped())
				return;

			i = parent->write;
			dump = parent->dump_list[i];

			pxAssert(dump.state == WRITEABLE);

			dump.data = &parent->dumps_avail_list[i % parent->num_dumps_buffered];

			pxAssert(dump.data->empty()); // Or something went wrong...
		}
		dump.block_time_write = block_timer.GetTimeSeconds();

		Common::Timer load_timer;
		Error error;
		std::unique_ptr<GSDumpFile> dump_file = GSDumpFile::OpenGSDump(dump.filename.c_str(), &error);

		if (!dump_file)
		{
			dump.error = fmt::format("Unable to open GS dump '{}' (error: {})",
				dump.filename, error.GetDescription());

			parent->num_errored.fetch_add(1, std::memory_order_seq_cst);
		}
		else if (!dump_file->ReadFile(*dump.data, parent->max_file_size, &error))
		{
			dump.error = fmt::format("Unable to read GS dump '{}' (error: {})", dump.filename, error.GetDescription());

			parent->num_errored.fetch_add(1, std::memory_order_seq_cst);
		}
		else
		{
			dump.load_time = load_timer.GetTimeSeconds();

			parent->num_loaded.fetch_add(1, std::memory_order_seq_cst);
		}

		// Release the slot.
		{
			std::unique_lock<std::mutex> lock(parent->mut);

			dump.state = READABLE;
			parent->dump_list[i] = dump;

			parent->write = i + 1;
		}
			
		parent->cond_read.notify_one();
	}
}

void GSDumpFileLoader::Stop()
{
	{
		std::unique_lock<std::mutex> lock(mut);

		stopped = true;
	}

	cond_read.notify_all();
	cond_write.notify_all();

	for (std::thread& t : threads)
	{
		if (t.joinable())
			t.join();
	}
}

void GSDumpFileLoader::DebugPrint()
{
	Console.WarningFmt("GSDumpFileLoader debug");
	Console.WarningFmt("   Total dumps    = {}", dump_list.size());
	Console.WarningFmt("   Threads        = {}", num_threads);
	Console.WarningFmt("   Dumps buffered = {}", num_dumps_buffered);
	Console.WarningFmt("   Max file size  = {}", max_file_size);
	Console.WarningFmt("   Started        = {}", started);
	Console.WarningFmt("   Stopped        = {}", stopped);
	Console.WarningFmt("   Read           = {}", read);
	Console.WarningFmt("   Write          = {}", write);
	Console.WarningFmt("   Loaded         = {}", num_loaded.load());
	Console.WarningFmt("   Errored        = {}", num_errored.load());
	Console.WarningFmt("");
	for (size_t i = 0; i < dump_list.size(); i++)
	{
		if (!dump_list[i].error.empty())
		{
			Console.WarningFmt("   Error: {}: '{}': '{}'",
				i, Path::GetFileName(dump_list[i].filename), dump_list[i].error);
		}
	}
}

void GSDumpFileLoaderLazy::Start(size_t num_dumps, size_t buffer_size)
{
	dumps.resize(num_dumps);
	filenames.resize(num_dumps);
	this->buffer_size = buffer_size;
}

bool GSDumpFileLoaderLazy::Started()
{
	return dumps.size() > 0;
}

bool GSDumpFileLoaderLazy::_HasNext()
{
	return !filename_next.empty();
}

bool GSDumpFileLoaderLazy::Full()
{
	size_t real_read = reading_dump ? read + 1 : read;
	size_t real_write = _HasNext() ? write + 1 : write;
	if (real_write - real_read > dumps.size())
		Console.ErrorFmt("(GSDumpFileLoaderLazy) Write/read pointers overlapped incorrectly.");
	return real_write - real_read == dumps.size();
}

bool GSDumpFileLoaderLazy::Empty()
{
	size_t real_read = reading_dump ? read + 1 : read;
	size_t real_write = _HasNext() ? write + 1 : write;
	if (real_write < real_read)
		Console.ErrorFmt("(GSDumpFileLoaderLazy) Write/read pointers incorrect order.");
	return real_write - real_read == 0;
}

GSDumpFileLoaderLazy::RetVal GSDumpFileLoaderLazy::AddFile(const std::string& filename, Error* error)
{
	if (!Started())
	{
		if (error)
			Error::SetString(error, "(GSDumpFileLoaderLazy) Not started.");
		return FAILURE;
	}
	if (Full())
		return FULL;
	if (reading_dump && (write - read == dumps.size()))
	{
		// Place filename in temporary slot.
		filename_next = filename;
		return SUCCESS;
	}
	if (!GSDumpFile::OpenGSDumpLazy(dumps[write % dumps.size()], buffer_size, filename.c_str(), error))
		return FAILURE;
	filenames[write % dumps.size()] = filename;
	write++;
	return SUCCESS;
}

GSDumpFileLoaderLazy::RetVal GSDumpFileLoaderLazy::GetFile(
	std::unique_ptr<GSDumpFile>& dump, std::string& filename, Error* error)
{
	if (!Started())
	{
		Error::SetString(error, "(GSDumpFileLoaderLazy) Not started.");
		return FAILURE;
	}

	if (reading_dump)
	{
		if (reading_dump != dump.get())
		{
			Console.ErrorFmt("(GSDumpFileLoaderLazy) Incorrect dump being recycled.");
			return FAILURE;
		}
		pxAssert(dumps[read % dumps.size()] == nullptr);

		// Reacquire ownership of the previous lazy dump.
		dumps[read % dumps.size()].reset(reinterpret_cast<GSDumpLazy*>(dump.release()));
		reading_dump = nullptr;
		read++;

		// Add any file in the temporary slot.
		if (_HasNext())
		{
			std::string filename_next_tmp = filename_next;
			filename_next.clear();
			RetVal ret = AddFile(filename_next_tmp, error);
			if (ret != SUCCESS)
				return ret;
		}
	}

	if (Empty())
		return EMPTY;

	if (dump != nullptr)
	{
		Console.ErrorFmt("(GSDumpFileLoaderLazy) Attempting to acquire into a non-empty pointer.");
		return FAILURE;
	}

	// Release the next dump.
	dump.reset(dumps[read % dumps.size()].release());
	reading_dump = reinterpret_cast<GSDumpLazy*>(dump.get());

	filename = filenames[read % filenames.size()];

	return SUCCESS;
}