// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/AlignedMalloc.h"
#include "common/Console.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "common/BitUtils.h"
#include "common/Error.h"
#include "common/HeapArray.h"

#include "GS/GSDump.h"
#include "GS/GSLzma.h"
#include "GS/GSExtra.h"

#include "Host.h"

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
	u32 ss;
	if (Read(&m_crc, sizeof(m_crc)) != sizeof(m_crc) || Read(&ss, sizeof(ss)) != sizeof(ss))
	{
		Error::SetString(error, TRANSLATE_STR("GSDumpFile", "Failed to read header."));
		return false;
	}

	m_state_data.resize(ss);
	if (Read(m_state_data.data(), ss) != ss)
	{
		Error::SetString(error, TRANSLATE_STR("GSDumpFile", "Failed to read state data."));
		return false;
	}

	// Pull serial out of new header, if present.
	if (m_crc == 0xFFFFFFFFu)
	{
		GSDumpHeader header;
		if (m_state_data.size() < sizeof(header))
		{
			Error::SetString(error, TRANSLATE_STR("GSDumpFile", "GSDump header is corrupted."));
			return false;
		}

		std::memcpy(&header, m_state_data.data(), sizeof(header));

		m_crc = header.crc;

		if (header.serial_size > 0)
		{
			if (header.serial_offset > ss || (static_cast<u64>(header.serial_offset) + header.serial_size) > ss)
			{
				Error::SetString(error, TRANSLATE_STR("GSDumpFile", "GSDump header is corrupted."));
				return false;
			}

			if (header.serial_size > 0)
				m_serial.assign(reinterpret_cast<const char*>(m_state_data.data()) + header.serial_offset, header.serial_size);
		}

		// Read the real state data
		m_state_data.resize(header.state_size);
		if (Read(m_state_data.data(), header.state_size) != header.state_size)
		{
			Error::SetString(error, TRANSLATE_STR("GSDumpFile", "Failed to read real state data"));
			return false;
		}
	}

	m_regs_data.resize(8192);
	if (Read(m_regs_data.data(), m_regs_data.size()) != m_regs_data.size())
	{
		Error::SetString(error, TRANSLATE_STR("GSDumpFile", "Failed to read regs data."));
		return false;
	}

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
			{
				Error::SetString(error, TRANSLATE_STR("GSDumpFile", "Failed to read packet."));
				return false;
			}

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
		{ \
			Error::SetString(error, TRANSLATE_STR("GSDumpFile", "Failed to read byte.")); \
			return false; \
		} \
		std::memcpy(dst, data, sizeof(u8)); \
		data++; \
		remaining--; \
	} while (0)
#define GET_WORD(dst) \
	do \
	{ \
		if (remaining < sizeof(u32)) \
		{ \
			Error::SetString(error, TRANSLATE_STR("GSDumpFile", "Failed to read word.")); \
			return false; \
		} \
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
				Error::SetStringFmt(error,
					TRANSLATE_FS("GSDumpFile", "Unknown packet type {}"), static_cast<u32>(packet.id));
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
	class GSDumpLzma final : public GSDumpFile
	{
	public:
		GSDumpLzma();
		~GSDumpLzma() override;

	protected:
		bool Open(FileSystem::ManagedCFilePtr fp, Error* error) override;
		bool IsEof() override;
		size_t Read(void* ptr, size_t size) override;

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
			Error::SetString(error, TRANSLATE_STR("GSDumpLzma", "Failed to allocate lookahead buffer."));
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
			Error::SetStringFmt(error, TRANSLATE_FS("GSDumpLzma", "Xzs_ReadBackward() failed: {}."), res);
			return false;
		}

		const size_t num_blocks = Xzs_GetNumBlocks(&xzs);
		if (num_blocks == 0)
		{
			Error::SetString(error, TRANSLATE_STR("GSDumpLzma", "Stream has no blocks."));
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

	/******************************************************************/

	class GSDumpRaw final : public GSDumpFile
	{
	public:
		GSDumpRaw();
		~GSDumpRaw() override;

		bool Open(FileSystem::ManagedCFilePtr fp, Error* error) override;
		bool IsEof() override;
		size_t Read(void* ptr, size_t size) override;
	};

	GSDumpRaw::GSDumpRaw() = default;

	GSDumpRaw::~GSDumpRaw() = default;

	bool GSDumpRaw::Open(FileSystem::ManagedCFilePtr fp, Error* error)
	{
		m_fp = std::move(fp);
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
} // namespace

/******************************************************************/

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
