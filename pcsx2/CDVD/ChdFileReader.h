/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "ThreadedFileReader.h"
#include "libchdr/chd.h"

class ChdFileReader : public ThreadedFileReader
{
	DeclareNoncopyableObject(ChdFileReader);

public:
	virtual ~ChdFileReader(void) { Close(); };

	static bool CanHandle(const wxString& fileName);
	bool Open2(const wxString& fileName) override;

	Chunk ChunkForOffset(u64 offset) override;
	int ReadChunk(void *dst, s64 blockID) override;

	void Close2(void) override;
	uint GetBlockCount(void) const override;
	ChdFileReader(void);

private:
	chd_file* ChdFile;
	u64 file_size;
	u32 hunk_size;
};
