// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "DEV9/PacketReader/IP/IP_Packet.h"
#include <functional>
#include <vector>

namespace Sessions
{
	class BaseSession; //Forward declare

	typedef std::function<void(BaseSession*)> ConnectionClosedEventHandler;

	struct ConnectionKey
	{
		PacketReader::IP::IP_Address ip{};
		u8 protocol = 0;
		u16 ps2Port = 0;
		u16 srvPort = 0;

		bool operator==(const ConnectionKey& other) const;
		bool operator!=(const ConnectionKey& other) const;
	};

	class BaseSession
	{
	public:
		ConnectionKey key;
		PacketReader::IP::IP_Address sourceIP;
		PacketReader::IP::IP_Address destIP;

	protected:
		PacketReader::IP::IP_Address adapterIP;

	private:
		std::vector<ConnectionClosedEventHandler> connectionClosedHandlers;

	public:
		BaseSession(ConnectionKey parKey, PacketReader::IP::IP_Address parAdapterIP);

		void AddConnectionClosedHandler(ConnectionClosedEventHandler handler);

		virtual PacketReader::IP::IP_Payload* Recv() = 0;
		virtual bool Send(PacketReader::IP::IP_Payload* payload) = 0;
		virtual void Reset() = 0;

		virtual ~BaseSession() {}

	protected:
		void RaiseEventConnectionClosed();
	};
} // namespace Sessions

//ConnectionKey Hash function
template <>
struct std::hash<Sessions::ConnectionKey>
{
	size_t operator()(Sessions::ConnectionKey const& s) const noexcept
	{
		size_t hash = 17;
		hash = hash * 23 + std::hash<u8>{}(s.ip.bytes[0]);
		hash = hash * 23 + std::hash<u8>{}(s.ip.bytes[1]);
		hash = hash * 23 + std::hash<u8>{}(s.ip.bytes[2]);
		hash = hash * 23 + std::hash<u8>{}(s.ip.bytes[3]);
		hash = hash * 23 + std::hash<u8>{}(s.protocol);
		hash = hash * 23 + std::hash<u16>{}(s.ps2Port);
		hash = hash * 23 + std::hash<u16>{}(s.srvPort);
		return hash;
	}
};
