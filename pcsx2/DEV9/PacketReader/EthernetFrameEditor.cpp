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

#include "EthernetFrameEditor.h"

#ifdef _WIN32
#include "winsock.h"
#else
#include <arpa/inet.h>
#endif

namespace PacketReader
{
	EthernetFrameEditor::EthernetFrameEditor(NetPacket* pkt)
		: basePkt{pkt}
	{
		headerLength = 14; //(6+6+2)

		//Note: we don't have to worry about the Ethernet Frame CRC as it is not included in the packet
		//Note: We don't support tagged frames

		payload = std::make_unique<PayloadPtr>((u8*)&basePkt->buffer[14], pkt->size - headerLength);
	}

	MAC_Address EthernetFrameEditor::GetDestinationMAC()
	{
		return *(MAC_Address*)&basePkt->buffer[0];
	}
	void EthernetFrameEditor::SetDestinationMAC(MAC_Address value)
	{
		*(MAC_Address*)&basePkt->buffer[0] = value;
	}

	MAC_Address EthernetFrameEditor::GetSourceMAC()
	{
		return *(MAC_Address*)&basePkt->buffer[6];
	}
	void EthernetFrameEditor::SetSourceMAC(MAC_Address value)
	{
		*(MAC_Address*)&basePkt->buffer[6] = value;
	}

	u16 EthernetFrameEditor::GetProtocol()
	{
		return ntohs(*(u16*)&basePkt->buffer[12]);
	}

	PayloadPtr* EthernetFrameEditor::GetPayload()
	{
		return payload.get();
	}
} // namespace PacketReader
