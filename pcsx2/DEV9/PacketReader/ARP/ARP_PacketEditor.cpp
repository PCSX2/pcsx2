/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include "ARP_PacketEditor.h"

#ifdef _WIN32
#include "winsock.h"
#else
#include <arpa/inet.h>
#endif

namespace PacketReader::ARP
{
	ARP_PacketEditor::ARP_PacketEditor(PayloadPtr* pkt)
		: basePkt{pkt}
	{
	}

	u16 ARP_PacketEditor::GetHardwareType()
	{
		return ntohs(*(u16*)&basePkt->data[0]);
	}

	u16 ARP_PacketEditor::GetProtocol()
	{
		return ntohs(*(u16*)&basePkt->data[2]);
	}

	u8 ARP_PacketEditor::GetHardwareAddressLength()
	{
		return basePkt->data[4];
	}
	u8 ARP_PacketEditor::GetProtocolAddressLength()
	{
		return basePkt->data[5];
	}

	u16 ARP_PacketEditor::GetOp()
	{
		return ntohs(*(u16*)&basePkt->data[6]);
	}

	u8* ARP_PacketEditor::SenderHardwareAddress()
	{
		return &basePkt->data[8];
	}

	u8* ARP_PacketEditor::SenderProtocolAddress()
	{
		int offset = 8 + GetHardwareAddressLength();
		return &basePkt->data[offset];
	}

	u8* ARP_PacketEditor::TargetHardwareAddress()
	{
		int offset = 8 + GetHardwareAddressLength() + GetProtocolAddressLength();
		return &basePkt->data[offset];
	}

	u8* ARP_PacketEditor::TargetProtocolAddress()
	{
		int offset = 8 + 2 * GetHardwareAddressLength() + GetProtocolAddressLength();
		return &basePkt->data[offset];
	}
} // namespace PacketReader::ARP
