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
