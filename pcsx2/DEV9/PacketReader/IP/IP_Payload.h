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

namespace PacketReader::IP
{
	class IP_Payload
	{
	public: //Nedd GetProtocol
		virtual int GetLength() = 0;
		virtual void WriteBytes(u8* buffer, int* offset) = 0;
		virtual u8 GetProtocol() = 0;
		virtual bool VerifyChecksum(IP_Address srcIP, IP_Address dstIP) { return false; }
		virtual void CalculateChecksum(IP_Address srcIP, IP_Address dstIP) {}
		virtual ~IP_Payload() {}
	};

	//Pointer to bytes not owned by class
	class IP_PayloadPtr : public IP_Payload
	{
	public:
		u8* data;

	private:
		int length;
		u8 protocol;

	public:
		IP_PayloadPtr(u8* ptr, int len, u8 prot)
		{
			data = ptr;
			length = len;
			protocol = prot;
		}
		virtual int GetLength()
		{
			return length;
		}
		virtual void WriteBytes(u8* buffer, int* offset)
		{
			//If buffer & data point to the same location
			//Then no copy is needed
			if (data == buffer)
				return;

			memcpy(buffer, data, length);
			*offset += length;
		}
		virtual u8 GetProtocol()
		{
			return protocol;
		}
	};
} // namespace PacketReader::IP
