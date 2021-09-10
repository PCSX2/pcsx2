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
