// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
		std::unique_ptr<PayloadPtr> payload;

	public:
		EthernetFrameEditor(NetPacket* pkt);

		MAC_Address GetDestinationMAC();
		void SetDestinationMAC(MAC_Address value);
		MAC_Address GetSourceMAC();
		void SetSourceMAC(MAC_Address value);

		u16 GetProtocol();

		PayloadPtr* GetPayload();
	};
} // namespace PacketReader
