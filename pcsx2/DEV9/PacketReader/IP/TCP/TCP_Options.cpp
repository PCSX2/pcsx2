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

#include "TCP_Options.h"
#include "DEV9/PacketReader/NetLib.h"

namespace PacketReader::IP::TCP
{
	TCPopMSS::TCPopMSS(u16 mss)
		: maxSegmentSize{mss}
	{
	}
	TCPopMSS::TCPopMSS(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadUInt16(data, &offset, &maxSegmentSize);
	}
	void TCPopMSS::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength());

		NetLib::WriteUInt16(buffer, offset, maxSegmentSize);
	}

	TCPopWS::TCPopWS(u8 ws)
		: windowScale{ws}
	{
	}
	TCPopWS::TCPopWS(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadByte08(data, &offset, &windowScale);
	}
	void TCPopWS::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength());

		NetLib::WriteByte08(buffer, offset, windowScale);
	}

	TCPopTS::TCPopTS(u32 senderTS, u32 echoTS)
		: senderTimeStamp{senderTS}
		, echoTimeStamp{echoTS}
	{
	}
	TCPopTS::TCPopTS(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadUInt32(data, &offset, &senderTimeStamp);
		NetLib::ReadUInt32(data, &offset, &echoTimeStamp);
	}
	void TCPopTS::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength());

		NetLib::WriteUInt32(buffer, offset, senderTimeStamp);
		NetLib::WriteUInt32(buffer, offset, echoTimeStamp);
	}
} // namespace PacketReader::IP::TCP
