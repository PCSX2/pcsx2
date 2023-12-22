// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "DEV9/Sessions/BaseSession.h"

namespace Sessions
{
	class UDP_BaseSession : public BaseSession
	{
	public:
		UDP_BaseSession(ConnectionKey parKey, PacketReader::IP::IP_Address parAdapterIP)
			: BaseSession(parKey, parAdapterIP)
		{
		}

		virtual bool WillRecive(PacketReader::IP::IP_Address parDestIP) = 0;
	};
} // namespace Sessions
