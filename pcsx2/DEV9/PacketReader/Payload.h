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

namespace PacketReader
{
	class Payload
	{
	public:
		virtual int GetLength() = 0;
		virtual void WriteBytes(u8* buffer, int* offset) = 0;
		virtual ~Payload() {}
	};

	//Data owned by class
	class PayloadData : public Payload
	{
	public:
		std::unique_ptr<u8[]> data;

	private:
		int length;

	public:
		PayloadData(int len)
		{
			length = len;

			if (len != 0)
				data = std::make_unique<u8[]>(len);
		}
		virtual int GetLength()
		{
			return length;
		}
		virtual void WriteBytes(u8* buffer, int* offset)
		{
			if (length == 0)
				return;

			memcpy(&buffer[*offset], data.get(), length);
			*offset += length;
		}
	};

	//Pointer to bytes not owned by class
	class PayloadPtr : public Payload
	{
	public:
		u8* data;

	private:
		int length;

	public:
		PayloadPtr(u8* ptr, int len)
		{
			data = ptr;
			length = len;
		}
		virtual int GetLength()
		{
			return length;
		}
		virtual void WriteBytes(u8* buffer, int* offset)
		{
			//If buffer & data point to the same location
			//Then no copy is needed
			if (data == buffer)
				return;

			memcpy(&buffer[*offset], data, length);
			*offset += length;
		}
	};
} // namespace PacketReader
