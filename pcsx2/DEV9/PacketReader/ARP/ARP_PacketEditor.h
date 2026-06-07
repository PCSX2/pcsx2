// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DEV9/PacketReader/Payload.h"

namespace PacketReader::ARP
{
	class ARP_PacketEditor
	{
	private:
		PayloadPtrEditor* basePkt;

	public:
		ARP_PacketEditor(PayloadPtrEditor* pkt);

		u16 GetHardwareType() const;
		u16 GetProtocol() const;
		u8 GetHardwareAddressLength() const;
		u8 GetProtocolAddressLength() const;
		u16 GetOp() const;

		u8* SenderHardwareAddress() const;
		u8* SenderProtocolAddress() const;
		u8* TargetHardwareAddress() const;
		u8* TargetProtocolAddress() const;

		int GetLength() const;
	};
} // namespace PacketReader::ARP
