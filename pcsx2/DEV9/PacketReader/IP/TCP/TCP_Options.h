// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DEV9/PacketReader/IP/IP_Options.h"

namespace PacketReader::IP::TCP
{
	class TCPopNOP : public BaseOption
	{
		virtual u8 GetLength() const { return 1; }
		virtual u8 GetCode() const { return 1; }

		virtual void WriteBytes(u8* buffer, int* offset) const
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
		TCPopMSS(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 4; }
		virtual u8 GetCode() const { return 2; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

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
		TCPopWS(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 3; }
		virtual u8 GetCode() const { return 3; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

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
		TCPopTS(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 10; }
		virtual u8 GetCode() const { return 8; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual TCPopTS* Clone() const
		{
			return new TCPopTS(*this);
		}
	};
} // namespace PacketReader::IP::TCP
