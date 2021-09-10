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
#include <vector>

#include "DEV9/PacketReader/Payload.h"
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
		int headerLength = 20;

		u8 dscp = 0;
		//Flags

		//u16 length;

		u16 id = 0; //used during reassembly fragmented packets
	private:
		u8 fragmentFlags1 = 0;
		u8 fragmentFlags2 = 0;
		//Fragment Flags
	public:
		u8 timeToLive = 0;
		u8 protocol;

	private:
		u16 checksum;

	public:
		IP_Address sourceIP{0};
		IP_Address destinationIP{0};
		std::vector<IPOption*> options;

	private:
		std::unique_ptr<IP_Payload> payload;

	public:
		int GetHeaderLength();

		//DSCP/TOS Flags

		/* Upper 3 Bits
		 * DSCP Class is equal to TOS precedence
		 * DSCP vs TOS
         * Default (xxx000), Low Effort (xxx010)
         * 0 = Routine				Class 0
         * Assured Forwarding (xxx000, xxx010, xxx100, xxx110)
         * 1 = Priority				Class 1
         * 2 = Immediate			Class 2
         * 3 = Flash				Class 3
         * 4 = Flash Override		Class 4
         * Expedited Forwarding (xxx110, xxx100)
         * 5 = Critical				Class 5
         * Not Defined (xxx000)
         * 6 = Internetwork Control	Class 6
         * 7 = Network Control		Class 7
		 * 
		 * Lower 3 Bits
		 * In TOS, defined as follows
		 * bit 0: Reliability
		 * bit 1: Throughput
		 * bit 2: Low Delay
		 * In DSCP, defined as following (bits 1-2)
		 * Class 0,   Low Effort, 1
		 * Class 1-4, Assured Forwarding drop probability, Low = 1, Mid = 2, High = 3
		 * Class 5,   Expedited Forwarding, 3
		 * bit0: Set to zero
		 */
		u8 GetDscpValue();
		void GetDscpValue(u8 value);

		/* 2 bits
		 * In TOS, defined as follows
		 * Bit 0: Unused
		 * Bit 1: Low Cost
		 * In DSCP, defined as follows
		 * 0 = ECN not supported
		 * 1,2 ECN Supported
		 * 3 = Congestion Encountered
		 */
		u8 GetDscpECN();
		void SetDscpECN(u8 value);

		//Fragment Flags
		//bit 0, reserverd

		bool GetDoNotFragment();
		void SetDoNotFragment(bool value);

		bool GetMoreFragments();
		void SetMoreFragments(bool value);

		//Untested
		u16 GetFragmentOffset();

		//Takes ownership of payload
		IP_Packet(IP_Payload* data);
		IP_Packet(u8* buffer, int bufferSize, bool fromICMP = false);
		IP_Packet(const IP_Packet&);

		IP_Payload* GetPayload();

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);
		virtual IP_Packet* Clone() const;

		bool VerifyChecksum();
		static u16 InternetChecksum(u8* buffer, int length);

		~IP_Packet();

	private:
		void ReComputeHeaderLen();
		void CalculateChecksum();
	};
} // namespace PacketReader::IP
