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

#pragma once

#include "UDP_BaseSession.h"

namespace Sessions
{
	class UDP_ServerSession : public UDP_BaseSession
	{
	public:
		UDP_ServerSession(ConnectionKey parKey, PacketReader::IP::IP_Address parAdapterIP)
			: UDP_BaseSession(parKey, parAdapterIP)
		{
		}

		virtual std::optional<ReceivedPayload> Recv() { return std::nullopt; }
		virtual bool Send(PacketReader::IP::IP_Payload* payload)
		{
			pxAssert(false);
			return false;
		}
		virtual void Reset(){};

		virtual bool WillRecive(PacketReader::IP::IP_Address parDestIP) { return true; };
	};
} // namespace Sessions
