// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/GSDump.h"
#include "GS/GSExtra.h"
#include "GS/GSLzma.h"
#include "GS/GSState.h"

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/HeapArray.h"
#include "common/ScopedGuard.h"

#include <7zCrc.h>
#include <XzCrc64.h>
#include <XzEnc.h>
#include <zstd.h>

GSDumpBase::GSDumpBase(std::string fn)
	: m_filename(std::move(fn))
	, m_frames(0)
	, m_extra_frames(2)
{
	m_gs = FileSystem::OpenCFile(m_filename.c_str(), "wb");
	if (!m_gs)
		Console.ErrorFmt("GSDump: Error failed to open {}", m_filename);
}

GSDumpBase::~GSDumpBase()
{
	if (m_gs)
		std::fclose(m_gs);
}

void GSDumpBase::AddHeader(const std::string& serial, u32 crc,
	u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
	const freezeData& fd, const GSPrivRegSet* regs)
{
	// New header: CRC of FFFFFFFF, secondary header, full header follows.
	const u32 fake_crc = 0xFFFFFFFFu;
	AppendRawData(&fake_crc, 4);

	// Compute full header size (with serial).
	// This acts as the state size for loading older dumps.
	const u32 screenshot_size = screenshot_width * screenshot_height * sizeof(screenshot_pixels[0]);
	const u32 header_size = sizeof(GSDumpHeader) + static_cast<u32>(serial.size()) + screenshot_size;
	AppendRawData(&header_size, 4);

	// Write hader.
	GSDumpHeader header = {};
	header.state_version = GSState::STATE_VERSION;
	header.state_size = fd.size;
	header.crc = crc;
	header.serial_offset = sizeof(header);
	header.serial_size = static_cast<u32>(serial.size());
	header.screenshot_width = screenshot_width;
	header.screenshot_height = screenshot_height;
	header.screenshot_offset = header.serial_offset + header.serial_size;
	header.screenshot_size = screenshot_size;
	AppendRawData(&header, sizeof(header));
	if (!serial.empty())
		AppendRawData(serial.data(), serial.size());
	if (screenshot_pixels)
		AppendRawData(screenshot_pixels, screenshot_size);

	// Then the real state data.
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
		Console.Error("GSDump: Error failed to write data");
}

//////////////////////////////////////////////////////////////////////
// GSDump implementation
//////////////////////////////////////////////////////////////////////

namespace
{
	class GSDumpUncompressed final : public GSDumpBase
	{
		void AppendRawData(const void* data, size_t size) final;
		void AppendRawData(u8 c) final;

	public:
		GSDumpUncompressed(const std::string& fn, const std::string& serial, u32 crc,
			u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
			const freezeData& fd, const GSPrivRegSet* regs);
		virtual ~GSDumpUncompressed() = default;
	};

	GSDumpUncompressed::GSDumpUncompressed(const std::string& fn, const std::string& serial, u32 crc,
		u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
		const freezeData& fd, const GSPrivRegSet* regs)
		: GSDumpBase(fn + ".gs")
	{
		AddHeader(serial, crc, screenshot_width, screenshot_height, screenshot_pixels, fd, regs);
	}

	void GSDumpUncompressed::AppendRawData(const void* data, size_t size)
	{
		Write(data, size);
	}

	void GSDumpUncompressed::AppendRawData(u8 c)
	{
		Write(&c, 1);
	}
} // namespace

std::unique_ptr<GSDumpBase> GSDumpBase::CreateUncompressedDump(
	const std::string& fn, const std::string& serial, u32 crc,
	u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
	const freezeData& fd, const GSPrivRegSet* regs)
{
	return std::make_unique<GSDumpUncompressed>(fn, serial, crc,
		screenshot_width, screenshot_height, screenshot_pixels,
		fd, regs);
}

namespace
{
	class GsDumpBuffered : public GSDumpBase
	{
	protected:
		void AppendRawData(const void* data, size_t size) override final;
		void AppendRawData(u8 c) override final;

		void EnsureSpace(size_t size);

		DynamicHeapArray<u8, 64> m_buffer;
		size_t m_buffer_size = 0;

	public:
		GsDumpBuffered(std::string fn);
		virtual ~GsDumpBuffered() override = default;
	};

	GsDumpBuffered::GsDumpBuffered(std::string fn)
		: GSDumpBase(std::move(fn))
	{
		m_buffer.resize(_1mb);
	}

	void GsDumpBuffered::AppendRawData(const void* data, size_t size)
	{
		if (size == 0) [[unlikely]]
			return;

		EnsureSpace(size);
		std::memcpy(&m_buffer[m_buffer_size], data, size);
		m_buffer_size += size;
	}

	void GsDumpBuffered::AppendRawData(u8 c)
	{
		EnsureSpace(1);
		m_buffer[m_buffer_size++] = c;
	}

	void GsDumpBuffered::EnsureSpace(size_t size)
	{
		const size_t new_size = m_buffer_size + size;
		if (new_size <= m_buffer.size())
			return;

		const size_t alloc_size = std::max(m_buffer.size() * 2, new_size);
		m_buffer.resize(alloc_size);
	}
} // namespace

//////////////////////////////////////////////////////////////////////
// GSDumpXz implementation
//////////////////////////////////////////////////////////////////////
namespace
{
	class GSDumpXz final : public GsDumpBuffered
	{
		void Compress();

	public:
		GSDumpXz(const std::string& fn, const std::string& serial, u32 crc,
			u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
			const freezeData& fd, const GSPrivRegSet* regs);
		~GSDumpXz() override;
	};

	GSDumpXz::GSDumpXz(const std::string& fn, const std::string& serial, u32 crc,
		u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
		const freezeData& fd, const GSPrivRegSet* regs)
		: GsDumpBuffered(fn + ".gs.xz")
	{
		AddHeader(serial, crc, screenshot_width, screenshot_height, screenshot_pixels, fd, regs);
	}

	GSDumpXz::~GSDumpXz()
	{
		Compress();
	}

	void GSDumpXz::Compress()
	{
		struct MemoryInStream
		{
			ISeqInStream vt;
			const u8* buffer;
			size_t buffer_size;
			size_t read_pos;
		};
		MemoryInStream mis = {
			{.Read = [](const ISeqInStream* p, void* buf, size_t* size) -> SRes {
				MemoryInStream* mis = Z7_CONTAINER_FROM_VTBL(p, MemoryInStream, vt);
				const size_t avail = mis->buffer_size - mis->read_pos;
				const size_t copy = std::min(avail, *size);

				std::memcpy(buf, &mis->buffer[mis->read_pos], copy);
				mis->read_pos += copy;
				*size = copy;
				return SZ_OK;
			}},
			m_buffer.data(),
			m_buffer_size,
			0};

		struct DumpOutStream
		{
			ISeqOutStream vt;
			GSDumpXz* real;
		};
		DumpOutStream dos = {
			{.Write = [](const ISeqOutStream* p, const void* buf, size_t size) -> size_t {
				DumpOutStream* dos = Z7_CONTAINER_FROM_VTBL(p, DumpOutStream, vt);
				dos->real->Write(buf, size);
				return size;
			}},
			this};

		pxAssert(m_buffer_size > 0);

		GSInit7ZCRCTables();

		CXzProps props;
		XzProps_Init(&props);
		const SRes res = Xz_Encode(&dos.vt, &mis.vt, &props, nullptr);
		if (res != SZ_OK)
		{
			Console.ErrorFmt("Xz_Encode() failed: {}", static_cast<int>(res));
			return;
		}
	}
} // namespace

std::unique_ptr<GSDumpBase> GSDumpBase::CreateXzDump(
	const std::string& fn, const std::string& serial, u32 crc,
	u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
	const freezeData& fd, const GSPrivRegSet* regs)
{
	return std::make_unique<GSDumpXz>(fn, serial, crc,
		screenshot_width, screenshot_height, screenshot_pixels,
		fd, regs);
}

//////////////////////////////////////////////////////////////////////
// GSDumpZstd implementation
//////////////////////////////////////////////////////////////////////

namespace
{
	class GSDumpZst final : public GSDumpBase
	{
		ZSTD_CStream* m_strm;

		std::vector<u8> m_in_buff;
		std::vector<u8> m_out_buff;

		void MayFlush();
		void Compress(ZSTD_EndDirective action);
		void AppendRawData(const void* data, size_t size);
		void AppendRawData(u8 c);

	public:
		GSDumpZst(const std::string& fn, const std::string& serial, u32 crc,
			u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
			const freezeData& fd, const GSPrivRegSet* regs);
		virtual ~GSDumpZst();
	};

	GSDumpZst::GSDumpZst(const std::string& fn, const std::string& serial, u32 crc,
		u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
		const freezeData& fd, const GSPrivRegSet* regs)
		: GSDumpBase(fn + ".gs.zst")
	{
		m_strm = ZSTD_createCStream();

		// Compression level 6 provides a good balance between speed and ratio.
		ZSTD_CCtx_setParameter(m_strm, ZSTD_c_compressionLevel, 6);

		m_in_buff.reserve(_1mb);
		m_out_buff.resize(_1mb);

		AddHeader(serial, crc, screenshot_width, screenshot_height, screenshot_pixels, fd, regs);
	}

	GSDumpZst::~GSDumpZst()
	{
		// Finish the stream
		Compress(ZSTD_e_end);

		ZSTD_freeCStream(m_strm);
	}

	void GSDumpZst::AppendRawData(const void* data, size_t size)
	{
		size_t old_size = m_in_buff.size();
		m_in_buff.resize(old_size + size);
		memcpy(&m_in_buff[old_size], data, size);
		MayFlush();
	}

	void GSDumpZst::AppendRawData(u8 c)
	{
		m_in_buff.push_back(c);
		MayFlush();
	}

	void GSDumpZst::MayFlush()
	{
		if (m_in_buff.size() >= _1mb)
			Compress(ZSTD_e_continue);
	}

	void GSDumpZst::Compress(ZSTD_EndDirective action)
	{
		if (m_in_buff.empty())
			return;

		ZSTD_inBuffer inbuf = {m_in_buff.data(), m_in_buff.size(), 0};

		for (;;)
		{
			ZSTD_outBuffer outbuf = {m_out_buff.data(), m_out_buff.size(), 0};

			const size_t remaining = ZSTD_compressStream2(m_strm, &outbuf, &inbuf, action);
			if (ZSTD_isError(remaining))
			{
				Console.ErrorFmt("GSDumpZstd: Error {}", ZSTD_getErrorName(remaining));
				return;
			}

			if (outbuf.pos > 0)
			{
				Write(m_out_buff.data(), outbuf.pos);
				outbuf.pos = 0;
			}

			if (action == ZSTD_e_end)
			{
				// break when compression output has finished
				if (remaining == 0)
					break;
			}
			else
			{
				// break when all input data is consumed
				if (inbuf.pos == inbuf.size)
					break;
			}
		}

		m_in_buff.clear();
	}
} // namespace

std::unique_ptr<GSDumpBase> GSDumpBase::CreateZstDump(
	const std::string& fn, const std::string& serial, u32 crc,
	u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
	const freezeData& fd, const GSPrivRegSet* regs)
{
	return std::make_unique<GSDumpZst>(fn, serial, crc,
		screenshot_width, screenshot_height, screenshot_pixels,
		fd, regs);
}
