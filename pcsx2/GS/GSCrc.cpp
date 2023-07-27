/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "PrecompiledHeader.h"

#include "GS/GSCrc.h"
#include "GS/GSExtra.h"
#include "GS/GS.h"
#include "Host.h"

#include "common/StringUtil.h"

const CRC::Game CRC::m_games[] =
{
	// Note: IDs 0x7ACF7E03, 0x7D4EA48F, 0x37C53760 - shouldn't be added as it's from the multiloaders when packing games.
	{0x00000000, NoTitle /* NoRegion */},
	{0xFC46EA61, Tekken5 /* JP */},
	{0x1F88EE37, Tekken5 /* EU */},
	{0x1F88BECD, Tekken5 /* EU */}, // language selector...
	{0x652050D2, Tekken5 /* US */},
	{0xEA64EF39, Tekken5 /* KO */},
};

const CRC::Game& CRC::Lookup(u32 crc)
{
	for (const Game& game : m_games)
	{
		if (game.crc == crc)
			return game;
	}

	return m_games[0];
}
