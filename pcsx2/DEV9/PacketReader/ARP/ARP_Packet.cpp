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

#include "ARP_Packet.h"
#include "DEV9/PacketReader/NetLib.h"

namespace PacketReader::ARP
{
	ARP_Packet::ARP_Packet(u8 hwAddrLen, u8 procAddrLen)
		: hardwareAddressLength{hwAddrLen}
		, protocolAddressLength{procAddrLen}
		, senderHardwareAddress{std::make_unique<u8[]>(hwAddrLen)}
		, senderProtocolAddress{std::make_unique<u8[]>(procAddrLen)}
		, targetHardwareAddress{std::make_unique<u8[]>(hwAddrLen)}
		, targetProtocolAddress{std::make_unique<u8[]>(procAddrLen)}
	{
	}

	ARP_Packet::ARP_Packet(u8* buffer, int bufferSize)
	{
		int offset = 0;

		NetLib::ReadUInt16(buffer, &offset, &hardwareType);
		NetLib::ReadUInt16(buffer, &offset, &protocol);
		//
		NetLib::ReadByte08(buffer, &offset, &hardwareAddressLength);
		NetLib::ReadByte08(buffer, &offset, &protocolAddressLength);

		NetLib::ReadUInt16(buffer, &offset, &op);

		//Allocate arrays
		senderHardwareAddress = std::make_unique<u8[]>(hardwareAddressLength);
		senderProtocolAddress = std::make_unique<u8[]>(protocolAddressLength);
		targetHardwareAddress = std::make_unique<u8[]>(hardwareAddressLength);
		targetProtocolAddress = std::make_unique<u8[]>(protocolAddressLength);

		//Assume normal MAC/IP address lengths for logging

		NetLib::ReadByteArray(buffer, &offset, hardwareAddressLength, senderHardwareAddress.get());
		NetLib::ReadByteArray(buffer, &offset, protocolAddressLength, senderProtocolAddress.get());

		NetLib::ReadByteArray(buffer, &offset, hardwareAddressLength, targetHardwareAddress.get());
		NetLib::ReadByteArray(buffer, &offset, protocolAddressLength, targetProtocolAddress.get());
	}
	ARP_Packet::ARP_Packet(const ARP_Packet& original)
		: hardwareType{original.hardwareType}
		, protocol{original.protocol}
		, hardwareAddressLength{original.hardwareAddressLength}
		, protocolAddressLength{original.protocolAddressLength}
		, op{original.protocol}
		, senderHardwareAddress{std::make_unique<u8[]>(original.hardwareAddressLength)}
		, senderProtocolAddress{std::make_unique<u8[]>(original.protocolAddressLength)}
		, targetHardwareAddress{std::make_unique<u8[]>(original.hardwareAddressLength)}
		, targetProtocolAddress{std::make_unique<u8[]>(original.protocolAddressLength)}
	{	
		memcpy(senderHardwareAddress.get(), original.senderHardwareAddress.get(), hardwareAddressLength);
		memcpy(senderProtocolAddress.get(), original.senderProtocolAddress.get(), protocolAddressLength);
		memcpy(targetHardwareAddress.get(), original.targetHardwareAddress.get(), hardwareAddressLength);
		memcpy(targetProtocolAddress.get(), original.targetProtocolAddress.get(), protocolAddressLength);
	}

	int ARP_Packet::GetLength()
	{
		return 8 + 2 * hardwareAddressLength + 2 * protocolAddressLength;
	}

	void ARP_Packet::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteUInt16(buffer, offset, hardwareType);
		NetLib::WriteUInt16(buffer, offset, protocol);
		NetLib::WriteByte08(buffer, offset, hardwareAddressLength);
		NetLib::WriteByte08(buffer, offset, protocolAddressLength);
		NetLib::WriteUInt16(buffer, offset, op);
		NetLib::WriteByteArray(buffer, offset, hardwareAddressLength, senderHardwareAddress.get());
		NetLib::WriteByteArray(buffer, offset, protocolAddressLength, senderProtocolAddress.get());
		NetLib::WriteByteArray(buffer, offset, hardwareAddressLength, targetHardwareAddress.get());
		NetLib::WriteByteArray(buffer, offset, protocolAddressLength, targetProtocolAddress.get());
	}

	ARP_Packet* ARP_Packet::Clone() const
	{
		return new ARP_Packet(*this);
	}
} // namespace PacketReader::ARP
