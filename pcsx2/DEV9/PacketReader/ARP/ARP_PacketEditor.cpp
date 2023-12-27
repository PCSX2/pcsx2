// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
