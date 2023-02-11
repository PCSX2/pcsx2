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
#include "GSCrc.h"
#include "GSExtra.h"
#include "GS.h"
#include "HostSettings.h"
#include "common/StringUtil.h"

const CRC::Game CRC::m_games[] =
{
	// Note: IDs 0x7ACF7E03, 0x7D4EA48F, 0x37C53760 - shouldn't be added as it's from the multiloaders when packing games.
	{0x00000000, NoTitle /* NoRegion */},
	{0x08C1ED4D, HauntingGround /* EU */},
	{0x2CD5794C, HauntingGround /* EU */},
	{0x867BB945, HauntingGround /* JP */},
	{0xE263BC4B, HauntingGround /* JP */},
	{0x901AAC09, HauntingGround /* US */},
	{0x6F8545DB, ICO /* US */},
	{0x48CDF317, ICO /* US */}, // Demo
	{0xB01A4C95, ICO /* JP */},
	{0x2DF2C1EA, ICO /* KO */},
	{0x5C991F4E, ICO /* EU */},
	{0x788D8B4F, ICO /* EU */},
	{0x29C28734, ICO /* CH */},
	{0x60013EBD, PolyphonyDigitalGames /* EU */}, // Gran Turismo Concept
	{0x6810C3BC, PolyphonyDigitalGames /* CH */}, // Gran Turismo Concept 2002 Tokyo-Geneva
	{0x0EEF32A3, PolyphonyDigitalGames /* KO */}, // Gran Turismo Concept 2002 Tokyo-Seoul
	{0x3E9D448A, PolyphonyDigitalGames /* CH */}, // GT3
	{0xAD66643C, PolyphonyDigitalGames /* CH */}, // GT3
	{0x85AE91B3, PolyphonyDigitalGames /* US */}, // GT3
	{0x8AA991B0, PolyphonyDigitalGames /* US */}, // GT3
	{0xC220951A, PolyphonyDigitalGames /* JP */}, // GT3
	{0x9DE5CF65, PolyphonyDigitalGames /* JP */}, // GT3
	{0x706DFF80, PolyphonyDigitalGames /* JP */}, // GT3 Store Disc Vol. 2
	{0x55CE5111, PolyphonyDigitalGames /* JP */}, // Gran Turismo 2000 Body Omen
	{0xE9A7E08D, PolyphonyDigitalGames /* JP */}, // Gran Turismo 2000 Body Omen
	{0xB590CE04, PolyphonyDigitalGames /* EU */}, // GT3
	{0xC02C653E, PolyphonyDigitalGames /* CH */}, // GT4
	{0x7ABDBB5E, PolyphonyDigitalGames /* CH */}, // GT4
	{0xAEAD1CA3, PolyphonyDigitalGames /* JP */}, // GT4
	{0xA3AF15A0, PolyphonyDigitalGames /* JP */}, // GT4 PS2 Racing Pack
	{0xE906EA37, PolyphonyDigitalGames /* JP */}, // GT4 First Preview
	{0xCA6243B9, PolyphonyDigitalGames /* JP */}, // GT4 Prologue
	{0xDD764BBE, PolyphonyDigitalGames /* JP */}, // GT4 Prologue
	{0xE1258846, PolyphonyDigitalGames /* JP */}, // GT4 Prologue
	{0x27B8F05F, PolyphonyDigitalGames /* JP */}, // GT4 Prius Trial Version
	{0x30E41D93, PolyphonyDigitalGames /* KO */}, // GT4
	{0x715CF2EC, PolyphonyDigitalGames /* EU */}, // GT4
	{0x44A61C8F, PolyphonyDigitalGames /* EU */}, // GT4
	{0x0086E35B, PolyphonyDigitalGames /* EU */}, // GT4
	{0x3FB69323, PolyphonyDigitalGames /* EU */}, // GT4 Prologue
	{0x77E61C8A, PolyphonyDigitalGames /* US */}, // GT4
	{0x33C6E35E, PolyphonyDigitalGames /* US */}, // GT4
	{0x70538747, PolyphonyDigitalGames /* US */}, // GT4 Toyota Prius Trial
	{0x32A1C752, PolyphonyDigitalGames /* US */}, // GT4 Online Beta
	{0x2A84A1E2, PolyphonyDigitalGames /* US */}, // GT4 Mazda MX-5 Edition
	{0x0087EEC4, PolyphonyDigitalGames /* NoRegion */}, // GT4 Online Beta, JP and US versions have the same CRC
	{0x5AC7E79C, PolyphonyDigitalGames /* CH */}, // TouristTrophy
	{0xFF9C0E93, PolyphonyDigitalGames /* US */}, // TouristTrophy
	{0xCA9AA903, PolyphonyDigitalGames /* EU */}, // TouristTrophy
	{0xAC3C1147, SVCChaos /* EU */}, // SVC Chaos: SNK vs. Capcom
	{0xB00FF2ED, SVCChaos /* JP */},
	{0x94834BD3, SVCChaos /* JP */},
	{0xCF1D71EE, KOF2002 /* EU */}, // The King of Fighters 2002
	{0xABD16263, KOF2002 /* JP */},
	{0x424A8601, KOF2002 /* JP */},
	{0x7F74D8D0, KOF2002 /* US */},
	{0xFC46EA61, Tekken5 /* JP */},
	{0x1F88EE37, Tekken5 /* EU */},
	{0x1F88BECD, Tekken5 /* EU */}, // language selector...
	{0x652050D2, Tekken5 /* US */},
	{0xEA64EF39, Tekken5 /* KO */},
	{0xE8FCF8EC, SMTNocturne /* US */},
	{0xF0A31EE3, SMTNocturne /* EU */}, // SMTNocturne (Lucifers Call in EU)
	{0xAE0DE7B7, SMTNocturne /* EU */}, // SMTNocturne (Lucifers Call in EU)
	{0xD60DA6D4, SMTNocturne /* JP */}, // SMTNocturne
	{0x0E762E8D, SMTNocturne /* JP */}, // SMTNocturne Maniacs
	{0x47BA9034, SMTNocturne /* JP */}, // SMTNocturne Maniacs Chronicle
	{0xD3FFC263, SMTNocturne /* KO */},
	{0x84D1A8DA, SMTNocturne /* KO */},
	{0xE21404E2, GetawayGames /* US */}, // Getaway
	{0xE8249852, GetawayGames /* JP */}, // Getaway
	{0x458485EF, GetawayGames /* EU */}, // Getaway
	{0x5DFBE144, GetawayGames /* EU */}, // Getaway
	{0xE78971DF, GetawayGames /* US */}, // GetawayBlackMonday
	{0x342D97FA, GetawayGames /* US */}, // GetawayBlackMonday Demo
	{0xE8C0AD1A, GetawayGames /* JP */}, // GetawayBlackMonday
	{0x09C3DF79, GetawayGames /* EU */}, // GetawayBlackMonday
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
