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

#include "TCP_Options.h"
#include "DEV9/PacketReader/IP/IP_Packet.h"

namespace PacketReader::IP::TCP
{
	class TCP_Packet : public IP_Payload
	{
	public:
		u16 sourcePort;
		u16 destinationPort;
		u32 sequenceNumber;
		u32 acknowledgementNumber;

	private:
		u8 dataOffsetAndNS_Flag = 0;
		int headerLength; //Can have varying Header Len
		u8 flags = 0;

	public:
		u16 windowSize;

	private:
		u16 checksum;
		u16 urgentPointer = 0;

	public:
		std::vector<BaseOption*> options;

	private:
		const static IP_Type protocol = IP_Type::TCP;

		std::unique_ptr<Payload> payload;

	public:
		//Flags
		bool GetNS();
		void SetNS(bool value);

		bool GetCWR();
		void SetCWR(bool value);

		bool GetECE();
		void SetECE(bool value);

		bool GetURG();
		void SetURG(bool value);

		bool GetACK();
		void SetACK(bool value);

		bool GetPSH();
		void SetPSH(bool value);

		bool GetRST();
		void SetRST(bool value);

		bool GetSYN();
		void SetSYN(bool value);

		bool GetFIN();
		void SetFIN(bool value);

		//Takes ownership of payload
		TCP_Packet(Payload* data);
		TCP_Packet(u8* buffer, int bufferSize);
		TCP_Packet(const TCP_Packet&);

		Payload* GetPayload();

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);
		virtual TCP_Packet* Clone() const;

		virtual u8 GetProtocol();

		virtual bool VerifyChecksum(IP_Address srcIP, IP_Address dstIP);
		virtual void CalculateChecksum(IP_Address srcIP, IP_Address dstIP);

	private:
		void ReComputeHeaderLen();
	};
} // namespace PacketReader::IP::TCP
