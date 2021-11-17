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

#include "DNS_Logger.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"
#include "DEV9/PacketReader/IP/UDP/DNS/DNS_Packet.h"

using PacketReader::PayloadPtr;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;
using namespace PacketReader::IP::UDP::DNS;

namespace InternalServers
{
	void DNS_Logger::InspectRecv(IP_Payload* payload)
	{
		UDP_Packet* udpPacket = static_cast<UDP_Packet*>(payload);
		PayloadPtr* udpPayload = static_cast<PayloadPtr*>(udpPacket->GetPayload());
		DNS_Packet dns(udpPayload->data, udpPayload->GetLength());
		LogPacket(&dns);
	}

	void DNS_Logger::InspectSend(IP_Payload* payload)
	{
		UDP_Packet* udpPacket = static_cast<UDP_Packet*>(payload);
		PayloadPtr* udpPayload = static_cast<PayloadPtr*>(udpPacket->GetPayload());
		DNS_Packet dns(udpPayload->data, udpPayload->GetLength());
		LogPacket(&dns);
	}

	std::string DNS_Logger::VectorToString(const std::vector<u8>& data)
	{
		std::string str;
		if (data.size() != 0)
		{
			str.reserve(data.size() * 4);
			for (size_t i = 0; i < data.size(); i++)
				str += std::to_string(data[i]) + ":";

			str.pop_back();
		} //else leave string empty

		return str;
	}

	const char* DNS_Logger::OpCodeToString(DNS_OPCode opcode)
	{
		switch (opcode)
		{
			case DNS_OPCode::Query:
				return "Query";
			case DNS_OPCode::IQuery:
				return "IQuery";
			case DNS_OPCode::Status:
				return "Status";
			case DNS_OPCode::Reserved:
				return "Reserved";
			case DNS_OPCode::Notify:
				return "Notify";
			case DNS_OPCode::Update:
				return "Update";
			default:
				return "Unknown";
		}
	}

	const char* DNS_Logger::RCodeToString(DNS_RCode rcode)
	{
		switch (rcode)
		{
			case DNS_RCode::NoError:
				return "NoError";
			case DNS_RCode::FormatError:
				return "FormatError";
			case DNS_RCode::ServerFailure:
				return "ServerFailure";
			case DNS_RCode::NameError:
				return "NameError";
			case DNS_RCode::NotImplemented:
				return "NotImplemented";
			case DNS_RCode::Refused:
				return "Refused";
			case DNS_RCode::YXDomain:
				return "YXDomain";
			case DNS_RCode::YXRRSet:
				return "YXRRSet";
			case DNS_RCode::NXRRSet:
				return "NXRRSet";
			case DNS_RCode::NotAuth:
				return "NotAuth";
			case DNS_RCode::NotZone:
				return "NotZone";
			default:
				return "Unknown";
		}
	}

	void DNS_Logger::LogPacket(DNS_Packet* dns)
	{
		Console.WriteLn("DEV9: DNS: ID %i", dns->id);
		Console.WriteLn("DEV9: DNS: Is Response? %s", dns->GetQR() ? "True" : "False");
		Console.WriteLn("DEV9: DNS: OpCode %s (%i)", OpCodeToString((DNS_OPCode)dns->GetOpCode()), dns->GetOpCode());
		Console.WriteLn("DEV9: DNS: Is Authoritative (not cached)? %s", dns->GetAA() ? "True" : "False");
		Console.WriteLn("DEV9: DNS: Is Truncated? %s", dns->GetTC() ? "True" : "False");
		Console.WriteLn("DEV9: DNS: Recursion Desired? %s", dns->GetRD() ? "True" : "False");
		Console.WriteLn("DEV9: DNS: Recursion Available? %s", dns->GetRA() ? "True" : "False");
		Console.WriteLn("DEV9: DNS: Zero %i", dns->GetZ0());
		Console.WriteLn("DEV9: DNS: Authenticated Data? %s", dns->GetAD() ? "True" : "False");
		Console.WriteLn("DEV9: DNS: Checking Disabled? %s", dns->GetCD() ? "True" : "False");
		Console.WriteLn("DEV9: DNS: Result %s (%i)", RCodeToString((DNS_RCode)dns->GetRCode()), dns->GetRCode());

		Console.WriteLn("DEV9: DNS: Question Count %i", dns->questions.size());
		Console.WriteLn("DEV9: DNS: Answer Count %i", dns->answers.size());
		Console.WriteLn("DEV9: DNS: Authority Count %i", dns->authorities.size());
		Console.WriteLn("DEV9: DNS: Additional Count %i", dns->additional.size());

		for (size_t i = 0; i < dns->questions.size(); i++)
		{
			DNS_QuestionEntry entry = dns->questions[i];
			Console.WriteLn("DEV9: DNS: Q%i Name %s", i, entry.name.c_str());
			Console.WriteLn("DEV9: DNS: Q%i Type %i", i, entry.entryType);
			Console.WriteLn("DEV9: DNS: Q%i Class %i", i, entry.entryClass);
		}
		for (size_t i = 0; i < dns->answers.size(); i++)
		{
			DNS_ResponseEntry entry = dns->answers[i];
			Console.WriteLn("DEV9: DNS: Ans%i Name %s", i, entry.name.c_str());
			Console.WriteLn("DEV9: DNS: Ans%i Type %i", i, entry.entryType);
			Console.WriteLn("DEV9: DNS: Ans%i Class %i", i, entry.entryClass);
			Console.WriteLn("DEV9: DNS: Ans%i TTL %i", i, entry.timeToLive);
			Console.WriteLn("DEV9: DNS: Ans%i Data %s", i, VectorToString(entry.data).c_str());
		}
		for (size_t i = 0; i < dns->authorities.size(); i++)
		{
			DNS_ResponseEntry entry = dns->authorities[i];
			Console.WriteLn("DEV9: DNS: Auth%i Name %s", i, entry.name.c_str());
			Console.WriteLn("DEV9: DNS: Auth%i Type %i", i, entry.entryType);
			Console.WriteLn("DEV9: DNS: Auth%i Class %i", i, entry.entryClass);
			Console.WriteLn("DEV9: DNS: Auth%i TTL %i", i, entry.timeToLive);
			Console.WriteLn("DEV9: DNS: Auth%i Data %s", i, VectorToString(entry.data).c_str());
		}
		for (size_t i = 0; i < dns->additional.size(); i++)
		{
			DNS_ResponseEntry entry = dns->additional[i];
			Console.WriteLn("DEV9: DNS: Add%i Name %s", i, entry.name.c_str());
			Console.WriteLn("DEV9: DNS: Add%i Type %i", i, entry.entryType);
			Console.WriteLn("DEV9: DNS: Add%i Class %i", i, entry.entryClass);
			Console.WriteLn("DEV9: DNS: Add%i TTL %i", i, entry.timeToLive);
			Console.WriteLn("DEV9: DNS: Add%i Data %s", i, VectorToString(entry.data).c_str());
		}
	}
} // namespace InternalServers
