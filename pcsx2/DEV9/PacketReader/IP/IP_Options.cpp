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

#include "PrecompiledHeader.h"

#include "IP_Options.h"
#include "DEV9/PacketReader/NetLib.h"

namespace PacketReader::IP
{
	bool IPOption::IsCopyOnFragment()
	{
		return ((GetCode() & (1 << 0x7)) != 0);
	}
	u8 IPOption::GetClass()
	{
		return (GetCode() >> 5) & 0x3;
	}
	u8 IPOption::GetNumber()
	{
		return GetCode() & 0x1F;
	}

	IPopUnk::IPopUnk(u8* data, int offset)
	{
		NetLib::ReadByte08(data, &offset, &code);
		NetLib::ReadByte08(data, &offset, &length);

		value.resize(length - 2);
		NetLib::ReadByteArray(data, &offset, length - 2, &value[0]);
	}
	void IPopUnk::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, code);
		NetLib::WriteByte08(buffer, offset, length);
		NetLib::WriteByteArray(buffer, offset, length - 2, &value[0]);
	}

	IPopRouterAlert::IPopRouterAlert(u16 parValue)
		: value{parValue}
	{
	}
	IPopRouterAlert::IPopRouterAlert(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadUInt16(data, &offset, &value);
	}
	void IPopRouterAlert::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength());
		NetLib::WriteUInt16(buffer, offset, value);
	}
} // namespace PacketReader::IP
