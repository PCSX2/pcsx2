// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

namespace PacketReader::IP
{
	class IP_Payload
	{
	public: //Nedd GetProtocol
		virtual int GetLength() = 0;
		virtual void WriteBytes(u8* buffer, int* offset) = 0;
		virtual u8 GetProtocol() = 0;
		virtual bool VerifyChecksum(IP_Address srcIP, IP_Address dstIP) { return false; }
		virtual void CalculateChecksum(IP_Address srcIP, IP_Address dstIP) {}
		virtual IP_Payload* Clone() const = 0;
		virtual ~IP_Payload() {}
	};

	class IP_PayloadData : public IP_Payload
	{
	public:
		std::unique_ptr<u8[]> data;

	private:
		int length;
		u8 protocol;

	public:
		IP_PayloadData(int len, u8 prot)
		{
			protocol = prot;
			length = len;

			if (len != 0)
				data = std::make_unique<u8[]>(len);
		}
		IP_PayloadData(const IP_PayloadData& original)
		{
			protocol = original.protocol;
			length = original.length;

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
		virtual u8 GetProtocol()
		{
			return protocol;
		}
		virtual IP_PayloadData* Clone() const
		{
			return new IP_PayloadData(*this);
		}
	};

	//Pointer to bytes not owned by class
	class IP_PayloadPtr : public IP_Payload
	{
	public:
		u8* data;

	private:
		int length;
		u8 protocol;

	public:
		IP_PayloadPtr(u8* ptr, int len, u8 prot)
		{
			data = ptr;
			length = len;
			protocol = prot;
		}
		IP_PayloadPtr(const IP_PayloadPtr&) = delete;
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

			memcpy(buffer, data, length);
			*offset += length;
		}
		virtual IP_Payload* Clone() const
		{
			IP_PayloadData* ret = new IP_PayloadData(length, protocol);
			memcpy(ret->data.get(), data, length);
			return ret;
		}
		virtual u8 GetProtocol()
		{
			return protocol;
		}
	};
} // namespace PacketReader::IP
