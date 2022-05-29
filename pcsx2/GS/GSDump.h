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

#pragma once

#include "SaveState.h"
#include "GSRegs.h"
#include "Renderers/SW/GSVertexSW.h"
#include <lzma.h>
#include <zstd.h>

/*

Dump file format:
- [0xFFFFFFFF] [Header] [state size/4] [state data/size] [PMODE/0x2000] [id/1] [data/?] .. [id/1] [data/?]

Transfer data (id == 0)
- [0/1] [path index/1] [size/4] [data/size]

VSync data (id == 1)
- [1/1] [field/1]

ReadFIFO2 data (id == 2)
- [2/1] [size/?]

Regs data (id == 3)
- [PMODE/0x2000]

*/

#pragma pack(push, 4)
struct GSDumpHeader
{
	u32 state_version; ///< Must always be first in struct to safely prevent old PCSX2 versions from crashing.
	u32 state_size;
	u32 serial_offset;
	u32 serial_size;
	u32 crc;
	u32 screenshot_width;
	u32 screenshot_height;
	u32 screenshot_offset;
	u32 screenshot_size;
};
#pragma pack(pop)

class GSDumpBase
{
	FILE* m_gs;
	std::string m_filename;
	int m_frames;
	int m_extra_frames;

protected:
	void AddHeader(const std::string& serial, u32 crc,
		u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
		const freezeData& fd, const GSPrivRegSet* regs);
	void Write(const void* data, size_t size);

	virtual void AppendRawData(const void* data, size_t size) = 0;
	virtual void AppendRawData(u8 c) = 0;

public:
	GSDumpBase(std::string fn);
	virtual ~GSDumpBase();

	__fi const std::string& GetPath() const { return m_filename; }

	void ReadFIFO(u32 size);
	void Transfer(int index, const u8* mem, size_t size);
	bool VSync(int field, bool last, const GSPrivRegSet* regs);
};

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

class GSDumpXz final : public GSDumpBase
{
	lzma_stream m_strm;

	std::vector<u8> m_in_buff;

	void Flush();
	void Compress(lzma_action action, lzma_ret expected_status);
	void AppendRawData(const void* data, size_t size);
	void AppendRawData(u8 c);

public:
	GSDumpXz(const std::string& fn, const std::string& serial, u32 crc,
		u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
		const freezeData& fd, const GSPrivRegSet* regs);
	virtual ~GSDumpXz();
};

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
