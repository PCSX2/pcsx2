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

#pragma once

#include <map>

class CRC
{
public:
	enum Title
	{
		NoTitle,
		AceCombat4,
		ArTonelico2,
		BigMuthaTruckers,
		BurnoutGames,
		CrashBandicootWoC,
		DBZBT2,
		DBZBT3,
		DeathByDegreesTekkenNinaWilliams,
		FFX,
		FFX2,
		FFXII,
		FightingBeautyWulong,
		GetawayGames,
		GiTS,
		GodHand,
		HauntingGround,
		ICO,
		IkkiTousen,
		Jak2,
		Jak3,
		JakX,
		KnightsOfTheTemple2,
		KOF2002,
		Kunoichi,
		Lamune,
		Manhunt2,
		MetalSlug6,
		MidnightClub3,
		Okami,
		Oneechanbara2Special,
		PolyphonyDigitalGames,
		RedDeadRevolver,
		RozenMaidenGebetGarden,
		SacredBlaze,
		SakuraTaisen,
		SakuraWarsSoLongMyLove,
		SFEX3,
		ShadowofRome,
		ShinOnimusha,
		Simple2000Vol114,
		SkyGunner,
		SMTNocturne,
		SonicUnleashed,
		Spartan,
		SteambotChronicles,
		SuperManReturns,
		SVCChaos,
		TalesOfAbyss,
		TalesOfLegendia,
		TalesofSymphonia,
		Tekken5,
		TombRaiderAnniversary,
		TombRaiderLegend,
		TombRaiderUnderworld,
		TriAceGames,
		UltramanFightingEvolution,
		UrbanReign,
		XenosagaE3,
		YakuzaGames,
		ZettaiZetsumeiToshi2,
		TitleCount,
	};

	enum Region
	{
		NoRegion,
		US,
		EU,
		JP,
		RU,
		FR,
		DE,
		IT,
		ES,
		CH,
		ASIA,
		KO,
		RegionCount,
	};

	struct Game
	{
		u32 crc;
		Title title;
		Region region;
		u32 flags;
	};

private:
	static const Game m_games[];
	static std::map<u32, const Game*> m_map;

public:
	static const Game& Lookup(u32 crc);
};
