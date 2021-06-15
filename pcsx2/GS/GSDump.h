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

#include "GS.h"
#include "Renderers/SW/GSVertexSW.h"
#include <lzma.h>

/*

Dump file format:
- [crc/4] [state size/4] [state data/size] [PMODE/0x2000] [id/1] [data/?] .. [id/1] [data/?]

Transfer data (id == 0)
- [0/1] [path index/1] [size/4] [data/size]

VSync data (id == 1)
- [1/1] [field/1]

ReadFIFO2 data (id == 2)
- [2/1] [size/?]

Regs data (id == 3)
- [PMODE/0x2000]

*/

class GSDumpBase
{
	int m_frames;
	int m_extra_frames;
	FILE* m_gs;

protected:
	void AddHeader(uint32 crc, const freezeData& fd, const GSPrivRegSet* regs);
	void Write(const void* data, size_t size);

	virtual void AppendRawData(const void* data, size_t size) = 0;
	virtual void AppendRawData(uint8 c) = 0;

public:
	GSDumpBase(const std::string& fn);
	virtual ~GSDumpBase();

	void ReadFIFO(uint32 size);
	void Transfer(int index, const uint8* mem, size_t size);
	bool VSync(int field, bool last, const GSPrivRegSet* regs);
};

class GSDump final : public GSDumpBase
{
	void AppendRawData(const void* data, size_t size) final;
	void AppendRawData(uint8 c) final;

public:
	GSDump(const std::string& fn, uint32 crc, const freezeData& fd, const GSPrivRegSet* regs);
	virtual ~GSDump() = default;
};

class GSDumpXz final : public GSDumpBase
{
	lzma_stream m_strm;

	std::vector<uint8> m_in_buff;

	void Flush();
	void Compress(lzma_action action, lzma_ret expected_status);
	void AppendRawData(const void* data, size_t size);
	void AppendRawData(uint8 c);

public:
	GSDumpXz(const std::string& fn, uint32 crc, const freezeData& fd, const GSPrivRegSet* regs);
	virtual ~GSDumpXz();
};
