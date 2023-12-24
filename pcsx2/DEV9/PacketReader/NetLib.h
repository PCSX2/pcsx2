// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "DEV9/PacketReader/MAC_Address.h"
#include "DEV9/PacketReader/IP/IP_Address.h"

#include <cstring>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace PacketReader::NetLib
{
	// Write.
	inline void WriteByte08(u8* data, int* index, u8 value)
	{
		data[*index] = value;
		*index += sizeof(u8);
	}
	inline void WriteUInt16(u8* data, int* index, u16 value)
	{
		*(u16*)&data[*index] = htons(value);
		*index += sizeof(u16);
	}
	inline void WriteUInt32(u8* data, int* index, u32 value)
	{
		*(u32*)&data[*index] = htonl(value);
		*index += sizeof(u32);
	}

	// Special write.
	inline void WriteMACAddress(u8* data, int* index, PacketReader::MAC_Address value)
	{
		*(PacketReader::MAC_Address*)&data[*index] = value;
		*index += sizeof(PacketReader::MAC_Address);
	}
	inline void WriteIPAddress(u8* data, int* index, PacketReader::IP::IP_Address value)
	{
		*(PacketReader::IP::IP_Address*)&data[*index] = value;
		*index += sizeof(PacketReader::IP::IP_Address);
	}
	inline void WriteByteArray(u8* data, int* index, int length, u8* value)
	{
		memcpy(&data[*index], value, length);
		*index += length;
	}

	// Read.
	inline void ReadByte08(u8* data, int* index, u8* value)
	{
		*value = data[*index];
		*index += sizeof(u8);
	}
	inline void ReadUInt16(u8* data, int* index, u16* value)
	{
		*value = ntohs(*(u16*)&data[*index]);
		*index += sizeof(u16);
	}
	inline void ReadUInt32(u8* data, int* index, u32* value)
	{
		*value = ntohl(*(u32*)&data[*index]);
		*index += sizeof(u32);
	}

	// Special read.
	inline void ReadMACAddress(u8* data, int* index, PacketReader::MAC_Address* value)
	{
		*value = *(PacketReader::MAC_Address*)&data[*index];
		*index += sizeof(PacketReader::MAC_Address);
	}
	inline void ReadIPAddress(u8* data, int* index, PacketReader::IP::IP_Address* value)
	{
		*value = *(PacketReader::IP::IP_Address*)&data[*index];
		*index += sizeof(PacketReader::IP::IP_Address);
	}
	inline void ReadByteArray(u8* data, int* index, int length, u8* value)
	{
		memcpy(value, &data[*index], length);
		*index += length;
	}
} // namespace PacketReader::NetLib
