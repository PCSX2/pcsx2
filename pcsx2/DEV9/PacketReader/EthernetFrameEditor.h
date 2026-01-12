// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DEV9/net.h"
#include "MAC_Address.h"
#include "Payload.h"

namespace PacketReader
{
	class EthernetFrameEditor
	{
	public:
		int headerLength = 14;
		//Length
	private:
		NetPacket* basePkt;
		std::unique_ptr<PayloadPtrEditor> payload;

	public:
		EthernetFrameEditor(NetPacket* pkt);

		MAC_Address GetDestinationMAC() const;
		void SetDestinationMAC(MAC_Address value);
		MAC_Address GetSourceMAC() const;
		void SetSourceMAC(MAC_Address value);

		u16 GetProtocol() const;

		PayloadPtrEditor* GetPayload() const;
	};
} // namespace PacketReader
