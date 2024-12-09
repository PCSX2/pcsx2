// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

namespace PacketReader::IP
{
	struct IP_Address
	{
		union
		{
			u8 bytes[4];
			u32 integer;
		};

		bool operator==(const IP_Address& other) const { return this->integer == other.integer; }
		bool operator!=(const IP_Address& other) const { return this->integer != other.integer; }

		IP_Address operator~() const
		{
			IP_Address ret;
			ret.integer = ~this->integer;
			return ret;
		}
		IP_Address operator&(const IP_Address& other) const 
		{
			IP_Address ret;
			ret.integer = this->integer & other.integer;
			return ret; 
		}
		IP_Address operator|(const IP_Address& other) const
		{
			IP_Address ret;
			ret.integer = this->integer | other.integer;
			return ret;
		}
		IP_Address operator^(const IP_Address& other) const
		{
			IP_Address ret;
			ret.integer = this->integer ^ other.integer;
			return ret;
		}
	};
} // namespace PacketReader::IP
