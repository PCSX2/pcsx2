// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
