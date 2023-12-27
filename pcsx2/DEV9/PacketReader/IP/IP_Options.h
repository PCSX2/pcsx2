// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <vector>

namespace PacketReader::IP
{
	class BaseOption
	{
	public:
		virtual u8 GetLength() = 0;
		virtual u8 GetCode() = 0;
		virtual void WriteBytes(u8* buffer, int* offset) = 0;
		virtual BaseOption* Clone() const = 0;
		virtual ~BaseOption() {}
	};

	class IPOption : public BaseOption
	{
	public:
		bool IsCopyOnFragment();
		u8 GetClass(); //0 = control, 2 = debugging and measurement
		u8 GetNumber();
		virtual IPOption* Clone() const = 0;
	};

	class IPopUnk : public IPOption
	{
		u8 length;
		u8 code;
		std::vector<u8> value;

	public:
		IPopUnk(u8* data, int offset);

		virtual u8 GetLength() { return length; }
		virtual u8 GetCode() { return code; }

		void WriteBytes(u8* buffer, int* offset);

		virtual IPopUnk* Clone() const
		{
			return new IPopUnk(*this);
		}
	};

	class IPopNOP : public IPOption
	{
	public:
		virtual u8 GetLength() { return 1; }
		virtual u8 GetCode() { return 1; }

		virtual void WriteBytes(u8* buffer, int* offset)
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
		IPopRouterAlert(u8* data, int offset);

		virtual u8 GetLength() { return 4; }
		virtual u8 GetCode() { return 148; }

		virtual void WriteBytes(u8* buffer, int* offset);

		virtual IPopRouterAlert* Clone() const
		{
			return new IPopRouterAlert(*this);
		}
	};
} // namespace PacketReader::IP
