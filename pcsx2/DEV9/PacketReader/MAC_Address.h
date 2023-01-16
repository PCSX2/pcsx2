/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

namespace PacketReader
{
#pragma pack(push, 1)
	struct MAC_Address
	{
		union
		{
			u8 bytes[6];
			struct
			{
				u32 integer03;
				u16 short45;
			} u;
		};

		bool operator==(const MAC_Address& other) const { return (this->u.integer03 == other.u.integer03) && (this->u.short45 == other.u.short45); }
		bool operator!=(const MAC_Address& other) const { return (this->u.integer03 != other.u.integer03) || (this->u.short45 != other.u.short45); }
	};
#pragma pack(pop)
} // namespace PacketReader
