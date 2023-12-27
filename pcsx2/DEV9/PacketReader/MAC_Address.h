// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

namespace PacketReader
{
#pragma pack(push, 1)
	struct MAC_Address
	{
		union
		{
			u8 bytes[6];
			struct
			{
				u32 integer03;
				u16 short45;
			} u;
		};

		bool operator==(const MAC_Address& other) const { return (this->u.integer03 == other.u.integer03) && (this->u.short45 == other.u.short45); }
		bool operator!=(const MAC_Address& other) const { return (this->u.integer03 != other.u.integer03) || (this->u.short45 != other.u.short45); }
	};
#pragma pack(pop)
} // namespace PacketReader
