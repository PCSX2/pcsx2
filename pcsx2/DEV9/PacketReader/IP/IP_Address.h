// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
	};
} // namespace PacketReader::IP
