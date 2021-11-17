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

class GSCodeBuffer
{
	std::vector<void*> m_buffers;
	size_t m_blocksize;
	size_t m_pos, m_reserved;
	u8* m_ptr;

public:
	GSCodeBuffer(size_t blocksize = 4096 * 64); // 256k
	virtual ~GSCodeBuffer();

	void* GetBuffer(size_t size);
	void ReleaseBuffer(size_t size);
};
