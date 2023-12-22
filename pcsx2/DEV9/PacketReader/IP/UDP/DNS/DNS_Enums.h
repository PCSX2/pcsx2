// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

namespace PacketReader::IP::UDP::DNS
{
	enum struct DNS_OPCode : u8
	{
		Query = 0,
		IQuery = 1,
		Status = 2,
		Reserved = 3,
		Notify = 4,
		Update = 5
	};
	enum struct DNS_RCode : u8
	{
		NoError = 0,
		FormatError = 1,
		ServerFailure = 2,
		NameError = 3,
		NotImplemented = 4,
		Refused = 5,
		YXDomain = 6,
		YXRRSet = 7,
		NXRRSet = 8,
		NotAuth = 9,
		NotZone = 10,
	};
} // namespace PacketReader::IP::UDP
