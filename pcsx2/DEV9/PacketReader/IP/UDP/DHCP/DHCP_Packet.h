// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <vector>

#include "DHCP_Options.h"
#include "DEV9/PacketReader/IP/IP_Packet.h"

namespace PacketReader::IP::UDP::DHCP
{
	class DHCP_Packet : public Payload
	{
	public:
		u8 op;
		u8 hardwareType;
		u8 hardwareAddressLength;
		u8 hops;
		u32 transactionID; //xid
		u16 seconds;
		u16 flags;
		IP_Address clientIP{};
		IP_Address yourIP{};
		IP_Address serverIP{};
		IP_Address gatewayIP{};
		u8 clientHardwareAddress[16]{}; //always 16 bytes, regardless of HardwareAddressLength
		//192 bytes of padding
		u32 magicCookie;
		//Assumes ownership of ptrs assigned to it
		std::vector<BaseOption*> options;

		//used by GetLength & WriteBytes
		int maxLength = 576;

		DHCP_Packet() {}
		DHCP_Packet(u8* buffer, int bufferSize);
		DHCP_Packet(const DHCP_Packet&);

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);
		virtual DHCP_Packet* Clone() const;

		virtual ~DHCP_Packet();
	};
} // namespace PacketReader::IP::UDP::DHCP
