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
