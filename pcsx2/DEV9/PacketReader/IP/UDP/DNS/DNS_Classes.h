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
		void WriteDNS_String(u8* buffer, int* offset, std::string value);
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