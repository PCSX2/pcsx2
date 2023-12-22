// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "DEV9/PacketReader/Payload.h"

namespace PacketReader::ARP
{
	class ARP_PacketEditor
	{
	private:
		PayloadPtr* basePkt;

	public:

		ARP_PacketEditor(PayloadPtr* pkt);

		u16 GetHardwareType();
		u16 GetProtocol();
		u8 GetHardwareAddressLength();
		u8 GetProtocolAddressLength();
		u16 GetOp();

		u8* SenderHardwareAddress();
		u8* SenderProtocolAddress();
		u8* TargetHardwareAddress();
		u8* TargetProtocolAddress();
	};
} // namespace PacketReader::ARP
