// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "DEV9/PacketReader/Payload.h"
#include <vector>

namespace PacketReader::ARP
{
	class ARP_Packet : public Payload
	{
	public:
		u16 hardwareType;
		u16 protocol;
		u8 hardwareAddressLength = 6;
		u8 protocolAddressLength = 4;
		u16 op;
		std::unique_ptr<u8[]> senderHardwareAddress;
		std::unique_ptr<u8[]> senderProtocolAddress;
		std::unique_ptr<u8[]> targetHardwareAddress;
		std::unique_ptr<u8[]> targetProtocolAddress;

		ARP_Packet(u8 hwAddrLen, u8 procAddrLen);
		ARP_Packet(u8* buffer, int bufferSize);
		ARP_Packet(const ARP_Packet&);

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);
		virtual ARP_Packet* Clone() const;
	};
} // namespace PacketReader::ARP
