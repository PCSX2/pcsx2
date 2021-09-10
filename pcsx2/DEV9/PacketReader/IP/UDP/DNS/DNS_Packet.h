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
#include <vector>

#include "DNS_Classes.h"
#include "DNS_Enums.h"
#include "DEV9/PacketReader/Payload.h"

namespace PacketReader::IP::UDP::DNS
{
	class DNS_Packet : public Payload
	{
	public:
		u16 id = 0;

	private:
		u8 flags1 = 0;
		u8 flags2 = 0;

		//QuestionCount
		//AnswerCount
		//Authorities
		//Additional

	public:
		std::vector<DNS_QuestionEntry> questions;
		std::vector<DNS_ResponseEntry> answers;
		std::vector<DNS_ResponseEntry> authorities;
		std::vector<DNS_ResponseEntry> additional;

		bool GetQR();
		void SetQR(bool value);

		u8 GetOpCode();
		void SetOpCode(u8 value);

		bool GetAA();
		void SetAA(bool value);

		bool GetTC();
		void SetTC(bool value);

		bool GetRD();
		void SetRD(bool value);

		bool GetRA();
		void SetRA(bool value);

		u8 GetZ0();
		void SetZ0(u8 value);

		bool GetAD();
		void SetAD(bool value);

		bool GetCD();
		void SetCD(bool value);

		u8 GetRCode();
		void SetRCode(u8 value);

		DNS_Packet() {}
		DNS_Packet(u8* buffer, int bufferSize);

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);
		virtual DNS_Packet* Clone() const;
	};
} // namespace PacketReader::IP::UDP::DNS
