// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ARP_PacketEditor.h"

#ifdef _WIN32
#include "winsock.h"
#else
#include <arpa/inet.h>
#endif

namespace PacketReader::ARP
{
	ARP_PacketEditor::ARP_PacketEditor(PayloadPtrEditor* pkt)
		: basePkt{pkt}
	{
	}

	u16 ARP_PacketEditor::GetHardwareType() const
	{
		return ntohs(*(u16*)&basePkt->data[0]);
	}

	u16 ARP_PacketEditor::GetProtocol() const
	{
		return ntohs(*(u16*)&basePkt->data[2]);
	}

	u8 ARP_PacketEditor::GetHardwareAddressLength() const
	{
		return basePkt->data[4];
	}
	u8 ARP_PacketEditor::GetProtocolAddressLength() const
	{
		return basePkt->data[5];
	}

	u16 ARP_PacketEditor::GetOp() const
	{
		return ntohs(*(u16*)&basePkt->data[6]);
	}

	u8* ARP_PacketEditor::SenderHardwareAddress() const
	{
		return &basePkt->data[8];
	}

	u8* ARP_PacketEditor::SenderProtocolAddress() const
	{
		const int offset = 8 + GetHardwareAddressLength();
		return &basePkt->data[offset];
	}

	u8* ARP_PacketEditor::TargetHardwareAddress() const
	{
		const int offset = 8 + GetHardwareAddressLength() + GetProtocolAddressLength();
		return &basePkt->data[offset];
	}

	u8* ARP_PacketEditor::TargetProtocolAddress() const
	{
		const int offset = 8 + 2 * GetHardwareAddressLength() + GetProtocolAddressLength();
		return &basePkt->data[offset];
	}

	int ARP_PacketEditor::GetLength() const
	{
		return 8 + 2 * GetHardwareAddressLength() + 2 * GetProtocolAddressLength();
	}
} // namespace PacketReader::ARP
