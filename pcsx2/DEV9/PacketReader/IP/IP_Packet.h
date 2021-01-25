/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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
#include <vector>

#include "../Payload.h"
#include "IP_Address.h"
#include "IP_Options.h"
#include "IP_Payload.h"

namespace PacketReader::IP
{
	enum struct IP_Type : u8
	{
		ICMP = 0x01,
		IGMP = 0x02,
		TCP = 0x06,
		UDP = 0x11
	};

	class IP_Packet : public Payload
	{
	private:
		const u8 _verHi = 4 << 4;

	public:
		int headerLength;

	private:
		u8 typeOfService;
		//Flags

		//u16 length;

		u16 id; //used during reassembly fragmented packets
	private:
		u8 fragmentFlags1;
		u8 fragmentFlags2;
		//Fragment Flags
	public:
		u8 timeToLive;
		u8 protocol;

	private:
		u16 checksum;

	public:
		IP_Address sourceIP{0};
		IP_Address destinationIP{0};
		std::vector<IPOption> options;

	private:
		IP_Payload* payload;
		bool ownsPayload = true;

	public:
		//Deal with flags here

		IP_Packet(u8* buffer, int bufferSize, bool fromICMP = false);

		IP_Payload* GetPayload();
		void SetPayload(IP_Payload* value, bool takeOwnership);

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);

		bool VerifyChecksum();
		static u16 InternetChecksum(u8* buffer, int length);

		~IP_Packet();

	private:
		void ReComputeHeaderLen();
		void CalculateChecksum();
	};
} // namespace PacketReader::IP
