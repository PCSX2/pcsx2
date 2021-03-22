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
#include "AsyncFileReader.h"
#include "libchdr/chd.h"

class ChdFileReader : public AsyncFileReader
{
	DeclareNoncopyableObject(ChdFileReader);

public:
	virtual ~ChdFileReader(void) { Close(); };

	static bool CanHandle(const wxString& fileName);
	bool Open(const wxString& fileName) override;

	int ReadSync(void* pBuffer, uint sector, uint count) override;

	void BeginRead(void* pBuffer, uint sector, uint count) override;
	int FinishRead(void) override;
	void CancelRead(void) override{};

	void Close(void) override;
	void SetBlockSize(uint blocksize);
	uint GetBlockSize() const;
	uint GetBlockCount(void) const override;
	ChdFileReader(void);

private:
	chd_file* ChdFile;
	u8* hunk_buffer;
	u32 sector_size;
	u32 sector_count;
	u32 sectors_per_hunk;
	u32 current_hunk;
	u32 async_read;
};
