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

#include "DEV9/PacketReader/IP/IP_Packet.h"
#include <functional>
#include <vector>

namespace Sessions
{
	class BaseSession; //Forward declare

	typedef std::function<void(BaseSession*)> ConnectionClosedEventHandler;

	struct ConnectionKey
	{
		PacketReader::IP::IP_Address ip{0};
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
