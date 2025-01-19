// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <vector>

namespace PacketReader::IP
{
	class BaseOption
	{
	public:
		virtual u8 GetLength() const = 0;
		virtual u8 GetCode() const = 0;
		virtual void WriteBytes(u8* buffer, int* offset) const = 0;
		virtual BaseOption* Clone() const = 0;
		virtual ~BaseOption() {}
	};

	class IPOption : public BaseOption
	{
	public:
		bool IsCopyOnFragment() const;
		u8 GetClass() const; //0 = control, 2 = debugging and measurement
		u8 GetNumber() const;
		virtual IPOption* Clone() const = 0;
	};

	class IPopUnk : public IPOption
	{
		u8 length;
		u8 code;
		std::vector<u8> value;

	public:
		IPopUnk(const u8* data, int offset);

		virtual u8 GetLength() const { return length; }
		virtual u8 GetCode() const { return code; }

		void WriteBytes(u8* buffer, int* offset) const;

		virtual IPopUnk* Clone() const
		{
			return new IPopUnk(*this);
		}
	};

	class IPopNOP : public IPOption
	{
	public:
		virtual u8 GetLength() const { return 1; }
		virtual u8 GetCode() const { return 1; }

		virtual void WriteBytes(u8* buffer, int* offset) const
		{
			buffer[*offset] = GetCode();
			(*offset)++;
		}

		virtual IPopNOP* Clone() const
		{
			return new IPopNOP(*this);
		}
	};

	class IPopRouterAlert : public IPOption
	{
		//Should the router intercept packet?
	public:
		u16 value;

		IPopRouterAlert(u16 parValue);
		IPopRouterAlert(const u8* data, int offset);

		virtual u8 GetLength() const { return 4; }
		virtual u8 GetCode() const { return 148; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual IPopRouterAlert* Clone() const
		{
			return new IPopRouterAlert(*this);
		}
	};
} // namespace PacketReader::IP
