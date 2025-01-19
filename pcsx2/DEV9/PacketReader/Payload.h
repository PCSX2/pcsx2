// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <cstring>
#include <memory>

#include "common/Assertions.h"
#include "common/Pcsx2Defs.h"

namespace PacketReader
{
	class Payload
	{
	public:
		virtual int GetLength() = 0;
		virtual void WriteBytes(u8* buffer, int* offset) = 0;
		virtual Payload* Clone() const = 0;
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
			: length{len}
		{
			if (len != 0)
				data = std::make_unique<u8[]>(len);
		}
		PayloadData(const PayloadData& original)
			: length{original.length}
		{
			if (length != 0)
			{
				data = std::make_unique<u8[]>(length);
				memcpy(data.get(), original.data.get(), length);
			}
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
		virtual PayloadData* Clone() const
		{
			return new PayloadData(*this);
		}
	};

	//Pointer to bytes not owned by class
	class PayloadPtr : public Payload
	{
	public:
		const u8* data;

	private:
		const int length;

	public:
		PayloadPtr(const u8* ptr, int len)
			: data{ptr}
			, length{len}
		{
		}
		PayloadPtr(const PayloadPtr&) = delete;
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
		virtual Payload* Clone() const
		{
			PayloadData* ret = new PayloadData(length);
			memcpy(ret->data.get(), data, length);
			return ret;
		}
	};

	//Pointer to bytes not owned by class, used by *Editor classes only
	class PayloadPtrEditor : public Payload
	{
	public:
		u8* data;

	private:
		const int length;

	public:
		PayloadPtrEditor(u8* ptr, int len)
			: data{ptr}
			, length{len}
		{
		}
		PayloadPtrEditor(const PayloadPtrEditor&) = delete;
		virtual int GetLength()
		{
			return length;
		}
		virtual void WriteBytes(u8* buffer, int* offset)
		{
			pxAssert(false);
		}
		virtual Payload* Clone() const
		{
			PayloadData* ret = new PayloadData(length);
			memcpy(ret->data.get(), data, length);
			return ret;
		}
	};
} // namespace PacketReader
