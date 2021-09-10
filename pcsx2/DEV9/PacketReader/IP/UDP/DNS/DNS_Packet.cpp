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

#include "DNS_Packet.h"
#include "DEV9/PacketReader/NetLib.h"

namespace PacketReader::IP::UDP::DNS
{
	bool DNS_Packet::GetQR()
	{
		return (flags1 & (1 << 7)) != 0;
	}
	void DNS_Packet::SetQR(bool value)
	{
		flags1 = (flags1 & ~(0x1 << 7)) | ((value & 0x1) << 7);
	}

	u8 DNS_Packet::GetOpCode()
	{
		return (flags1 >> 3) & 0xF;
	}
	void DNS_Packet::SetOpCode(u8 value)
	{
		flags1 = (flags1 & ~(0xF << 3)) | ((value & 0xF) << 3);
	}

	bool DNS_Packet::GetAA()
	{
		return (flags1 & (1 << 2)) != 0;
	}
	void DNS_Packet::SetAA(bool value)
	{
		flags1 = (flags1 & ~(0x1 << 2)) | ((value & 0x1) << 2);
	}

	bool DNS_Packet::GetTC()
	{
		return (flags1 & (1 << 1)) != 0;
	}
	void DNS_Packet::SetTC(bool value)
	{
		flags1 = (flags1 & ~(0x1 << 1)) | ((value & 0x1) << 1);
	}

	bool DNS_Packet::GetRD()
	{
		return (flags1 & 1) != 0;
	}
	void DNS_Packet::SetRD(bool value)
	{
		flags1 = (flags1 & ~0x1) | (value & 0x1);
	}

	bool DNS_Packet::GetRA()
	{
		return (flags2 & (1 << 7)) != 0;
	}
	void DNS_Packet::SetRA(bool value)
	{
		flags2 = (flags2 & ~(0x1 << 7)) | ((value & 0x1) << 7);
	}

	u8 DNS_Packet::GetZ0()
	{
		return (flags2 & (1 << 6)) != 0;
	}
	void DNS_Packet::SetZ0(u8 value)
	{
		flags1 = (flags2 & ~(0x1 << 6)) | ((value & 0x1) << 6);
	}

	bool DNS_Packet::GetAD()
	{
		return (flags2 & (1 << 5)) != 0;
	}
	void DNS_Packet::SetAD(bool value)
	{
		flags2 = (flags2 & ~(0x1 << 5)) | ((value & 0x1) << 5);
	}

	bool DNS_Packet::GetCD()
	{
		return (flags2 & (1 << 4)) != 0;
	}
	void DNS_Packet::SetCD(bool value)
	{
		flags2 = (flags2 & ~(0x1 << 4)) | ((value & 0x1) << 4);
	}

	u8 DNS_Packet::GetRCode()
	{
		return flags2 & 0xF;
	}
	void DNS_Packet::SetRCode(u8 value)
	{
		flags2 = (flags2 & ~(0xF << 3)) | ((value & 0xF) << 3);
	}

	DNS_Packet::DNS_Packet(u8* buffer, int bufferSize)
	{
		int offset = 0;
		//Bits 0-31 //Bytes 0-3
		NetLib::ReadUInt16(buffer, &offset, &id);
		NetLib::ReadByte08(buffer, &offset, &flags1);
		NetLib::ReadByte08(buffer, &offset, &flags2);
		//Bits 32-63 //Bytes 4-7
		u16 qCount;
		u16 aCount;
		u16 auCount;
		u16 adCount;
		NetLib::ReadUInt16(buffer, &offset, &qCount);
		NetLib::ReadUInt16(buffer, &offset, &aCount);
		//Bits 64-95 //Bytes 8-11
		NetLib::ReadUInt16(buffer, &offset, &auCount);
		NetLib::ReadUInt16(buffer, &offset, &adCount);
		//Bits 96+   //Bytes 8+
		for (int i = 0; i < qCount; i++)
		{
			DNS_QuestionEntry entry(buffer, &offset);
			questions.push_back(entry);
		}
		for (int i = 0; i < aCount; i++)
		{
			DNS_ResponseEntry entry(buffer, &offset);
			answers.push_back(entry);
		}
		for (int i = 0; i < auCount; i++)
		{
			DNS_ResponseEntry entry(buffer, &offset);
			authorities.push_back(entry);
		}
		for (int i = 0; i < adCount; i++)
		{
			DNS_ResponseEntry entry(buffer, &offset);
			additional.push_back(entry);
		}
	}

	int DNS_Packet::GetLength()
	{
		int length = 2 * 2 + 4 * 2;

		for (size_t i = 0; i < questions.size(); i++)
			length += questions[i].GetLength();

		for (size_t i = 0; i < answers.size(); i++)
			length += answers[i].GetLength();

		for (size_t i = 0; i < authorities.size(); i++)
			length += authorities[i].GetLength();

		for (size_t i = 0; i < additional.size(); i++)
			length += additional[i].GetLength();

		return length;
	}

	void DNS_Packet::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteUInt16(buffer, offset, id);
		NetLib::WriteByte08(buffer, offset, flags1);
		NetLib::WriteByte08(buffer, offset, flags2);
		NetLib::WriteUInt16(buffer, offset, questions.size());
		NetLib::WriteUInt16(buffer, offset, answers.size());
		NetLib::WriteUInt16(buffer, offset, authorities.size());
		NetLib::WriteUInt16(buffer, offset, additional.size());

		for (size_t i = 0; i < questions.size(); i++)
			questions[i].WriteBytes(buffer, offset);

		for (size_t i = 0; i < answers.size(); i++)
			answers[i].WriteBytes(buffer, offset);

		for (size_t i = 0; i < authorities.size(); i++)
			authorities[i].WriteBytes(buffer, offset);

		for (size_t i = 0; i < additional.size(); i++)
			additional[i].WriteBytes(buffer, offset);
	}

	DNS_Packet* DNS_Packet::Clone() const
	{
		return new DNS_Packet(*this);
	}
} // namespace PacketReader::IP::UDP::DNS
