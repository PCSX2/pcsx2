// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "SaveState.h"
#include "GS/GSRegs.h"
#include "GS/Renderers/SW/GSVertexSW.h"

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

	static std::unique_ptr<GSDumpBase> CreateUncompressedDump(
		const std::string& fn, const std::string& serial, u32 crc,
		u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
		const freezeData& fd, const GSPrivRegSet* regs);
	static std::unique_ptr<GSDumpBase> CreateXzDump(
		const std::string& fn, const std::string& serial, u32 crc,
		u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
		const freezeData& fd, const GSPrivRegSet* regs);
	static std::unique_ptr<GSDumpBase> CreateZstDump(
		const std::string& fn, const std::string& serial, u32 crc,
		u32 screenshot_width, u32 screenshot_height, const u32* screenshot_pixels,
		const freezeData& fd, const GSPrivRegSet* regs);
};
