// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
