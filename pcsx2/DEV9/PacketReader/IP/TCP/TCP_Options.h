// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "DEV9/PacketReader/IP/IP_Options.h"

namespace PacketReader::IP::TCP
{
	class TCPopNOP : public BaseOption
	{
		virtual u8 GetLength() { return 1; }
		virtual u8 GetCode() { return 1; }

		virtual void WriteBytes(u8* buffer, int* offset)
		{
			buffer[*offset] = GetCode();
			(*offset)++;
		}

		virtual TCPopNOP* Clone() const
		{
			return new TCPopNOP(*this);
		}
	};

	class TCPopMSS : public BaseOption
	{
	public:
		u16 maxSegmentSize;

		TCPopMSS(u16 mss);
		TCPopMSS(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 4; }
		virtual u8 GetCode() { return 2; }

		virtual void WriteBytes(u8* buffer, int* offset);

		virtual TCPopMSS* Clone() const
		{
			return new TCPopMSS(*this);
		}
	};

	class TCPopWS : public BaseOption
	{
	public:
		u8 windowScale;

		TCPopWS(u8 ws);
		TCPopWS(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 3; }
		virtual u8 GetCode() { return 3; }

		virtual void WriteBytes(u8* buffer, int* offset);

		virtual TCPopWS* Clone() const
		{
			return new TCPopWS(*this);
		}
	};

	class TCPopTS : public BaseOption
	{
	public:
		u32 senderTimeStamp;
		u32 echoTimeStamp;

		TCPopTS(u32 senderTS, u32 echoTS);
		TCPopTS(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 10; }
		virtual u8 GetCode() { return 8; }

		virtual void WriteBytes(u8* buffer, int* offset);

		virtual TCPopTS* Clone() const
		{
			return new TCPopTS(*this);
		}
	};
} // namespace PacketReader::IP::TCP
