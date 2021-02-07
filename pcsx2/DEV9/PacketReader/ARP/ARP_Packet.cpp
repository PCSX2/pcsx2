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

#include "PrecompiledHeader.h"

#include "ARP_Packet.h"
#include "../NetLib.h"

namespace PacketReader::ARP
{
	ARP_Packet::ARP_Packet(u8* buffer, int bufferSize)
	{
		Console.Error("Reading ARP packet");
		int offset = 0;

		NetLib::ReadUInt16(buffer, &offset, &hardwareType);
		Console.Error("HwType       %d", hardwareType);

		NetLib::ReadUInt16(buffer, &offset, &protocol);
		Console.Error("Protocol     %04X", protocol);
		//
		NetLib::ReadByte08(buffer, &offset, &hardwareAddressLength);
		Console.Error("HwAddrLen    %d", hardwareAddressLength);
		NetLib::ReadByte08(buffer, &offset, &protocolAddressLength);
		Console.Error("PtAddrLen    %d", protocolAddressLength);

		NetLib::ReadUInt16(buffer, &offset, &op);
		Console.Error("OP           %d", op);

		//Allocate arrays
		senderHardwareAddress = new u8[hardwareAddressLength];
		senderProtocolAddress = new u8[protocolAddressLength];
		targetHardwareAddress = new u8[hardwareAddressLength];
		targetProtocolAddress = new u8[protocolAddressLength];

		//Assume normal MAC/IP address lengths for logging

		NetLib::ReadByteArray(buffer, &offset, hardwareAddressLength, senderHardwareAddress);
		Console.Error("SenderMac    %02X:%02X:%02X:%02X:%02X:%02X",
					  senderHardwareAddress[0], senderHardwareAddress[1], senderHardwareAddress[2], senderHardwareAddress[3], senderHardwareAddress[4], senderHardwareAddress[5]);

		NetLib::ReadByteArray(buffer, &offset, protocolAddressLength, senderProtocolAddress);
		Console.Error("SenderIP     %d.%d.%d.%d", senderProtocolAddress[0], senderProtocolAddress[1], senderProtocolAddress[2], senderProtocolAddress[3]);

		NetLib::ReadByteArray(buffer, &offset, hardwareAddressLength, targetHardwareAddress);
		Console.Error("TargetMac    %02X:%02X:%02X:%02X:%02X:%02X",
					  targetHardwareAddress[0], targetHardwareAddress[1], targetHardwareAddress[2], targetHardwareAddress[3], targetHardwareAddress[4], targetHardwareAddress[5]);

		NetLib::ReadByteArray(buffer, &offset, protocolAddressLength, targetProtocolAddress);
		Console.Error("TargetIP     %d.%d.%d.%d", targetProtocolAddress[0], targetProtocolAddress[1], targetProtocolAddress[2], targetProtocolAddress[3]);
	}

	int ARP_Packet::GetLength()
	{
		return 8 + 2 * hardwareAddressLength + 2 * protocolAddressLength;
	}

	ARP_Packet::~ARP_Packet()
	{
		delete[] senderHardwareAddress;
		delete[] senderProtocolAddress;
		delete[] targetHardwareAddress;
		delete[] targetProtocolAddress;
	}
} // namespace PacketReader::ARP
