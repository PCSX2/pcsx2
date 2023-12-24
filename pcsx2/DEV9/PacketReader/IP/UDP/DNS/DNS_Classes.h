// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <string>
#include <vector>

namespace PacketReader::IP::UDP::DNS
{
	class DNS_QuestionEntry
	{
	public:
		std::string name;
		u16 entryType;
		u16 entryClass;

		DNS_QuestionEntry(const std::string& qName, u16 qType, u16 qClass);
		DNS_QuestionEntry(u8* buffer, int* offset);

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);

		virtual ~DNS_QuestionEntry(){};

	private:
		void ReadDNS_String(u8* buffer, int* offset, std::string* value);
		void WriteDNS_String(u8* buffer, int* offset, const std::string& value);
	};

	class DNS_ResponseEntry : public DNS_QuestionEntry
	{
	public:
		u32 timeToLive;
		std::vector<u8> data;

		DNS_ResponseEntry(const std::string& rName, u16 rType, u16 rClass, const std::vector<u8>& rData, u32 rTTL);
		DNS_ResponseEntry(u8* buffer, int* offset);

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);

		virtual ~DNS_ResponseEntry(){};

	private:
		void ReadDNSString(u8* buffer, int* offset, std::string* value);
		void WriteDNSString(u8* buffer, int* offset, std::string value);
	};
} // namespace PacketReader::IP::UDP::DNS