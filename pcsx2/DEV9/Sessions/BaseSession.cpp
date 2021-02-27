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

#include "PrecompiledHeader.h"

#include "BaseSession.h"

namespace Sessions
{
	bool ConnectionKey::operator==(const ConnectionKey& other) const
	{
		return (*(u32*)&ip == *(u32*)&other.ip) &&
			   (protocol == other.protocol) &&
			   (ps2Port == other.ps2Port) &&
			   (srvPort == other.srvPort);
	}
	bool ConnectionKey::operator!=(const ConnectionKey& other) const
	{
		return !(*this == other);
	}

	BaseSession::BaseSession(ConnectionKey parKey, PacketReader::IP::IP_Address parAdapterIP)
		: key{parKey}
		, adapterIP{parAdapterIP}
	{
	}

	void BaseSession::AddConnectionClosedHandler(ConnectionClosedEventHandler handler)
	{
		connectionClosedHandlers.push_back(handler);
	}

	void BaseSession::RaiseEventConnectionClosed()
	{
		std::vector<ConnectionClosedEventHandler> Handlers = connectionClosedHandlers;
		connectionClosedHandlers.clear();

		for (size_t i = 0; i < Handlers.size(); i++)
			Handlers[i](this);
	}
} // namespace Sessions
