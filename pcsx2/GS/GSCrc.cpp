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
#include "common/StringUtil.h"

const CRC::Game CRC::m_games[] =
{
	// Note: IDs 0x7ACF7E03, 0x7D4EA48F, 0x37C53760 - shouldn't be added as it's from the multiloaders when packing games.
	{0x00000000, NoTitle, NoRegion, 0},
	{0xF95F37EE, ArTonelico2, US, 0},
	{0x68CE6801, ArTonelico2, JP, 0},
	{0xCE2C1DBF, ArTonelico2, EU, 0},
	{0x2113EA2E, MetalSlug6, JP, 0},
	{0xA6167B59, Lamune, JP, 0},
	{0xA39517AB, FFX, EU, 0},
	{0x78D83FD5, FFX, EU, 0}, // Demo
	{0xA39517AE, FFX, FR, 0},
	{0x941BB7D9, FFX, DE, 0},
	{0xA39517A9, FFX, IT, 0},
	{0x941BB7DE, FFX, ES, 0},
	{0xA80F497C, FFX, ES, 0},
	{0xB4414EA1, FFX, RU, 0},
	{0xEE97DB5B, FFX, RU, 0},
	{0xAEC495CC, FFX, RU, 0},
	{0xBB3D833A, FFX, US, 0},
	{0x6A4EFE60, FFX, JP, 0},
	{0x3866CA7E, FFX, ASIA, 0}, // int.
	{0x658597E2, FFX, JP, 0}, // int.
	{0x9AAC5309, FFX2, EU, 0},
	{0x9AAC530C, FFX2, FR, 0},
	{0x9AAC530A, FFX2, ES, 0},
	{0x9AAC530D, FFX2, DE, 0},
	{0x9AAC530B, FFX2, IT, 0},
	{0x48FE0C71, FFX2, US, 0},
	{0x8A6D7F14, FFX2, JP, 0},
	{0xE1FD9A2D, FFX2, JP, 0}, // int.
	{0x11624CD6, FFX2, KO, 0},
	{0x78DA0252, FFXII, EU, 0},
	{0xC1274668, FFXII, EU, 0},
	{0xDC2A467E, FFXII, EU, 0},
	{0xCA284668, FFXII, EU, 0},
	{0xC52B466E, FFXII, EU, 0}, // ES
	{0xE5E71BF9, FFXII, FR, 0},
	{0x0779FBDB, FFXII, US, 0},
	{0x280AD120, FFXII, JP, 0},
	{0x08C1ED4D, HauntingGround, EU, 0},
	{0x2CD5794C, HauntingGround, EU, 0},
	{0x867BB945, HauntingGround, JP, 0},
	{0xE263BC4B, HauntingGround, JP, 0},
	{0x901AAC09, HauntingGround, US, 0},
	{0x21068223, Okami, US, 0},
	{0x891F223F, Okami, EU, 0}, // PAL DE, ES & FR.
	{0xC5DEFEA0, Okami, JP, 0},
	{0xCCC8F3A4, Okami, KO, 0},
	{0x278722BF, DBZBT2, EU, 0},
	{0xFE961D28, DBZBT2, US, 0},
	{0x0393B6BE, DBZBT2, EU, 0},
	{0xE2F289ED, DBZBT2, JP, 0}, // Sparking Neo!
	{0xE29C09A3, DBZBT2, KO, 0}, // DragonBall Z Sparking Neo
	{0x0BAA4387, DBZBT2, JP, 0},
	{0x35AA84D1, DBZBT2, NoRegion, 0},
	{0xBE6A9CFB, DBZBT2, NoRegion, 0},
	{0x428113C2, DBZBT3, US, 0},
	{0xA422BB13, DBZBT3, EU, 0},
	{0xCE93CB30, DBZBT3, JP, 0},
	{0xF28D21F1, DBZBT3, JP, 0},
	{0x983C53D2, DBZBT3, NoRegion, 0},
	{0x983C53D3, DBZBT3, EU, 0},
	{0x9B0E119F, DBZBT3, KO, 0}, // DragonBall Z Sparking Meteo
	{0x5E13E6D6, SFEX3, EU, 0},
	{0x72B3802A, SFEX3, US, 0},
	{0x71521863, SFEX3, US, 0},
	{0x63642E9F, SFEX3, JP, 0},
	{0xCA1F6E53, SFEX3, JP, 0}, // Taikenban Disc (Demo/Trial)
	{0x6F8545DB, ICO, US, 0},
	{0x48CDF317, ICO, US, 0}, // Demo
	{0xB01A4C95, ICO, JP, 0},
	{0x2DF2C1EA, ICO, KO, 0},
	{0x5C991F4E, ICO, EU, 0},
	{0x788D8B4F, ICO, EU, 0},
	{0x29C28734, ICO, CH, 0},
	{0x60013EBD, PolyphonyDigitalGames, EU, 0}, // Gran Turismo Concept
	{0x6810C3BC, PolyphonyDigitalGames, CH, 0}, // Gran Turismo Concept 2002 Tokyo-Geneva
	{0x0EEF32A3, PolyphonyDigitalGames, KO, 0}, // Gran Turismo Concept 2002 Tokyo-Seoul
	{0x3E9D448A, PolyphonyDigitalGames, CH, 0}, // GT3
	{0xAD66643C, PolyphonyDigitalGames, CH, 0}, // GT3
	{0x85AE91B3, PolyphonyDigitalGames, US, 0}, // GT3
	{0x8AA991B0, PolyphonyDigitalGames, US, 0}, // GT3
	{0xC220951A, PolyphonyDigitalGames, JP, 0}, // GT3
	{0x9DE5CF65, PolyphonyDigitalGames, JP, 0}, // GT3
	{0x706DFF80, PolyphonyDigitalGames, JP, 0}, // GT3 Store Disc Vol. 2
	{0x55CE5111, PolyphonyDigitalGames, JP, 0}, // Gran Turismo 2000 Body Omen
	{0xE9A7E08D, PolyphonyDigitalGames, JP, 0}, // Gran Turismo 2000 Body Omen
	{0xB590CE04, PolyphonyDigitalGames, EU, 0}, // GT3
	{0xC02C653E, PolyphonyDigitalGames, CH, 0}, // GT4
	{0x7ABDBB5E, PolyphonyDigitalGames, CH, 0}, // GT4
	{0xAEAD1CA3, PolyphonyDigitalGames, JP, 0}, // GT4
	{0xA3AF15A0, PolyphonyDigitalGames, JP, 0}, // GT4 PS2 Racing Pack
	{0xE906EA37, PolyphonyDigitalGames, JP, 0}, // GT4 First Preview
	{0xCA6243B9, PolyphonyDigitalGames, JP, 0}, // GT4 Prologue
	{0xDD764BBE, PolyphonyDigitalGames, JP, 0}, // GT4 Prologue
	{0xE1258846, PolyphonyDigitalGames, JP, 0}, // GT4 Prologue
	{0x27B8F05F, PolyphonyDigitalGames, JP, 0}, // GT4 Prius Trial Version
	{0x30E41D93, PolyphonyDigitalGames, KO, 0}, // GT4
	{0x715CF2EC, PolyphonyDigitalGames, EU, 0}, // GT4
	{0x44A61C8F, PolyphonyDigitalGames, EU, 0}, // GT4
	{0x0086E35B, PolyphonyDigitalGames, EU, 0}, // GT4
	{0x3FB69323, PolyphonyDigitalGames, EU, 0}, // GT4 Prologue
	{0x77E61C8A, PolyphonyDigitalGames, US, 0}, // GT4
	{0x33C6E35E, PolyphonyDigitalGames, US, 0}, // GT4
	{0x70538747, PolyphonyDigitalGames, US, 0}, // GT4 Toyota Prius Trial
	{0x32A1C752, PolyphonyDigitalGames, US, 0}, // GT4 Online Beta
	{0x2A84A1E2, PolyphonyDigitalGames, US, 0}, // GT4 Mazda MX-5 Edition
	{0x0087EEC4, PolyphonyDigitalGames, NoRegion, 0}, // GT4 Online Beta, JP and US versions have the same CRC
	{0x5AC7E79C, PolyphonyDigitalGames, CH, 0}, // TouristTrophy
	{0xFF9C0E93, PolyphonyDigitalGames, US, 0}, // TouristTrophy
	{0xCA9AA903, PolyphonyDigitalGames, EU, 0}, // TouristTrophy
	{0x8B029334, Manhunt2, EU, 0},
	{0x3B0ADBEF, Manhunt2, US, 0},
	{0x09F49E37, CrashBandicootWoC, NoRegion, 0},
	{0x103B5706, CrashBandicootWoC, US, 0}, // American Greatest Hits release
	{0x75182BE5, CrashBandicootWoC, US, 0},
	{0x5188ABCA, CrashBandicootWoC, US, 0},
	{0xEB1EB7FE, CrashBandicootWoC, US, 0}, // Regular Demo
	{0x34E2EEC7, CrashBandicootWoC, RU, 0},
	{0x00A074A7, CrashBandicootWoC, KO, 0},
	{0xF8643F9B, CrashBandicootWoC, JP, 0},
	{0x3A03D62F, CrashBandicootWoC, EU, 0},
	{0x35D70452, CrashBandicootWoC, EU, 0},
	{0x1E935600, CrashBandicootWoC, EU, 0},
	{0x72E1E60E, Spartan, EU, 0},
	{0x26689C87, Spartan, JP, 0},
	{0x08277A9E, Spartan, US, 0},
	{0xAC3C1147, SVCChaos, EU, 0}, // SVC Chaos: SNK vs. Capcom
	{0xB00FF2ED, SVCChaos, JP, 0},
	{0x94834BD3, SVCChaos, JP, 0},
	{0xCF1D71EE, KOF2002, EU, 0}, // The King of Fighters 2002
	{0xABD16263, KOF2002, JP, 0},
	{0x424A8601, KOF2002, JP, 0},
	{0x7F74D8D0, KOF2002, US, 0},
	{0xA32F7CD0, AceCombat4, US, 0}, // Also needed for automatic mipmapping
	{0x5ED8FB53, AceCombat4, JP, 0},
	{0x1B9B7563, AceCombat4, EU, 0},
	{0xFC46EA61, Tekken5, JP, 0},
	{0x1F88EE37, Tekken5, EU, 0},
	{0x1F88BECD, Tekken5, EU, 0}, // language selector...
	{0x652050D2, Tekken5, US, 0},
	{0xEA64EF39, Tekken5, KO, 0},
	{0x9E98B8AE, IkkiTousen, JP, 0},
	{0x95CC86EF, GiTS, US, 0}, // same CRC also reported as EU
	{0x2C5BF134, GiTS, US, 0}, // Demo
	{0xA5768F53, GiTS, JP, 0},
	{0xA3643EB1, GiTS, KO, 0},
	{0x28557423, GiTS, RU, 0},
	{0xBF6F101F, GiTS, EU, 0}, // same CRC as another US disc
	{0xA616A6C2, TalesOfAbyss, US, 0},
	{0x14FE77F7, TalesOfAbyss, US, 0},
	{0xAA5EC3A3, TalesOfAbyss, JP, 0},
	{0xFB236A46, SonicUnleashed, US, 0},
	{0x8C913264, SonicUnleashed, EU, 0},
	{0xE04EA200, TriAceGames, EU, 0}, // StarOcean3
	{0x23A97857, TriAceGames, US, 0}, // StarOcean3
	{0xBEC32D49, TriAceGames, JP, 0}, // StarOcean3
	{0x8192A241, TriAceGames, JP, 0}, // StarOcean3 directors cut
	{0xCC96CE93, TriAceGames, US, 0}, // ValkyrieProfile2
	{0x774DE8E2, TriAceGames, JP, 0}, // ValkyrieProfile2
	{0x04CCB600, TriAceGames, EU, 0}, // ValkyrieProfile2
	{0xB65E141B, TriAceGames, DE, 0}, // ValkyrieProfile2
	{0x8510854E, TriAceGames, FR, 0}, // ValkyrieProfile2
	{0xC70FC973, TriAceGames, IT, 0}, // ValkyrieProfile2
	{0x47B9B2FD, TriAceGames, US, 0}, // RadiataStories
	{0xAC73005E, TriAceGames, JP, 0}, // RadiataStories
	{0xE8FCF8EC, SMTNocturne, US, 0},
	{0xF0A31EE3, SMTNocturne, EU, 0}, // SMTNocturne (Lucifers Call in EU)
	{0xAE0DE7B7, SMTNocturne, EU, 0}, // SMTNocturne (Lucifers Call in EU)
	{0xD60DA6D4, SMTNocturne, JP, 0}, // SMTNocturne
	{0x0E762E8D, SMTNocturne, JP, 0}, // SMTNocturne Maniacs
	{0x47BA9034, SMTNocturne, JP, 0}, // SMTNocturne Maniacs Chronicle
	{0xD3FFC263, SMTNocturne, KO, 0},
	{0x84D1A8DA, SMTNocturne, KO, 0},
	{0x0B8AB37B, RozenMaidenGebetGarden, JP, 0},
	{0x506644B3, BigMuthaTruckers, EU, 0},
	{0x90F0D852, BigMuthaTruckers, US, 0},
	{0x92624842, BigMuthaTruckers, US, 0},
	{0xDD93DA88, BigMuthaTruckers, JP, 0}, // Bakusou Convoy Densetsu - Otoko Hanamichi America Roman
	{0xE169BAF8, RedDeadRevolver, US, 0},
	{0xE2E67E23, RedDeadRevolver, EU, 0},
	{0xC5B75C7C, Oneechanbara2Special, JP, 0},
	{0xC725CC6C, Oneechanbara2Special, JP, 0},
	{0x07608CA2, Oneechanbara2Special, EU, 0}, // Zombie Hunters 2
	{0xE0347841, XenosagaE3, JP, 0},
	{0xA707236E, XenosagaE3, JP, 0}, // Demo
	{0xA4E88698, XenosagaE3, CH, 0},
	{0x2088950A, XenosagaE3, US, 0},
	{0xB1995E29, ShadowofRome, EU, 0},
	{0x958DCA28, ShadowofRome, EU, 0},
	{0x57818AF6, ShadowofRome, US, 0},
	{0x1E210E60, ShadowofRome, US, 0}, // Demo
	{0x36393CD3, ShadowofRome, JP, 0},
	{0x694A998E, TombRaiderUnderworld, JP, 0},
	{0x8E214549, TombRaiderUnderworld, EU, 0},
	{0x618769D6, TombRaiderUnderworld, US, 0},
	{0xB639EB17, TombRaiderAnniversary, US, 0}, // Also needed for automatic mipmapping
	{0xB05805B6, TombRaiderAnniversary, JP, 0},
	{0xA629A376, TombRaiderAnniversary, EU, 0},
	{0xBC8B3F50, TombRaiderLegend, US, 0},
	{0x365172A0, TombRaiderLegend, JP, 0},
	{0x05177ECE, TombRaiderLegend, EU, 0},
	{0xBEBF8793, BurnoutGames, US, 0}, // BurnoutTakedown
	{0xBB2E845F, BurnoutGames, JP, 0}, // BurnoutTakedown
	{0x5F060991, BurnoutGames, KO, 0}, // BurnoutTakedown
	{0x75BECC18, BurnoutGames, EU, 0}, // BurnoutTakedown
	{0xCE49B0DE, BurnoutGames, EU, 0}, // BurnoutTakedown
	{0x381EE9EF, BurnoutGames, EU, 0}, // BurnoutTakedown E3 Demo
	{0xD224D348, BurnoutGames, US, 0}, // BurnoutRevenge
	{0x878E7A1D, BurnoutGames, JP, 0}, // BurnoutRevenge
	{0xEEA60511, BurnoutGames, KO, 0}, // BurnoutRevenge
	{0x7E83CC5B, BurnoutGames, EU, 0}, // BurnoutRevenge
	{0x2CAC3DBC, BurnoutGames, EU, 0}, // BurnoutRevenge
	{0x8C9576A1, BurnoutGames, US, 0}, // BurnoutDominator
	{0xDDF76A98, BurnoutGames, JP, 0}, // BurnoutDominator
	{0x8C9576B4, BurnoutGames, EU, 0}, // BurnoutDominator
	{0x8C9C76B4, BurnoutGames, EU, 0}, // BurnoutDominator
	{0x4A0E5B3A, MidnightClub3, US, 0}, // dub
	{0xEBE1972D, MidnightClub3, EU, 0}, // dub
	{0x60A42FF5, MidnightClub3, US, 0}, // remix
	{0x43AB7214, TalesOfLegendia, US, 0},
	{0x1F8640E0, TalesOfLegendia, JP, 0},
	{0xE4F5DA2B, TalesOfLegendia, KO, 0},
	{0x519E816B, Kunoichi, US, 0}, // Nightshade
	{0x3FB419FD, Kunoichi, JP, 0},
	{0x086D198E, Kunoichi, CH, 0},
	{0x3B470BBD, Kunoichi, EU, 0},
	{0x6BA65DD8, Kunoichi, KO, 0},
	{0XD3F182A3, YakuzaGames, EU, 0}, // Yakuza
	{0x6F9F99F8, YakuzaGames, EU, 0}, // Yakuza
	{0x388F687B, YakuzaGames, US, 0}, // Yakuza
	{0xC1B91FC5, YakuzaGames, US, 0}, // Yakuza Demo
	{0xB7B3800A, YakuzaGames, JP, 0}, // Yakuza2
	{0xA60C2E65, YakuzaGames, EU, 0}, // Yakuza2
	{0x800E3E5A, YakuzaGames, EU, 0}, // Yakuza2
	{0x97E9C87E, YakuzaGames, US, 0}, // Yakuza2
	{0xB1EBD841, YakuzaGames, US, 0}, // Yakuza2
	{0xC6B95C48, YakuzaGames, JP, 0}, // Yakuza2
	{0x9000252A, SkyGunner, JP, 0},
	{0x93092623, SkyGunner, JP, 0},
	{0xA9461CB2, SkyGunner, US, 0},
	{0xC71DE999, SkyGunner, US, 0}, // Regular Demo
	{0xAADF3287, SkyGunner, US, 0}, // Trade Demo
	{0xB799A60C, SkyGunner, NoRegion, 0},
	{0x2905C5C6, ZettaiZetsumeiToshi2, US, 0}, // Raw Danger!
	{0xC988ECBB, ZettaiZetsumeiToshi2, JP, 0},
	{0x90F4B057, ZettaiZetsumeiToshi2, CH, 0},
	{0xA98B5B22, ZettaiZetsumeiToshi2, EU, 0},
	{0xBD17248E, ShinOnimusha, JP, 0},
	{0xBE17248E, ShinOnimusha, JP, 0},
	{0xB817248E, ShinOnimusha, JP, 0},
	{0xC1C77637, ShinOnimusha, JP, 0}, // PlayStation 2 The Best, Disc 1
	{0x5C1E5BEF, ShinOnimusha, JP, 0}, // PlayStation 2 The Best, Disc 2
	{0x812C5A96, ShinOnimusha, EU, 0},
	{0xFE44479E, ShinOnimusha, US, 0},
	{0xFFDE85E9, ShinOnimusha, US, 0},
	{0xE21404E2, GetawayGames, US, 0}, // Getaway
	{0xE8249852, GetawayGames, JP, 0}, // Getaway
	{0x458485EF, GetawayGames, EU, 0}, // Getaway
	{0x5DFBE144, GetawayGames, EU, 0}, // Getaway
	{0xE78971DF, GetawayGames, US, 0}, // GetawayBlackMonday
	{0x342D97FA, GetawayGames, US, 0}, // GetawayBlackMonday Demo
	{0xE8C0AD1A, GetawayGames, JP, 0}, // GetawayBlackMonday
	{0x09C3DF79, GetawayGames, EU, 0}, // GetawayBlackMonday
	{0x1130BF23, SakuraTaisen, CH, 0},
	{0x4FAE8B83, SakuraTaisen, KO, 0},
	{0xEF06DBD6, SakuraWarsSoLongMyLove, JP, 0},
	{0xDD41054D, SakuraWarsSoLongMyLove, US, 0},
	{0xC2E3A7A4, SakuraWarsSoLongMyLove, KO, 0},
	{0x4A4B623A, FightingBeautyWulong, JP, 0},
	{0xAEDAEE99, GodHand, JP, 0},
	{0x6FB69282, GodHand, US, 0},
	{0x924C4AA6, GodHand, KO, 0},
	{0xDE9722A5, GodHand, EU, 0},
	{0x9637D496, KnightsOfTheTemple2, NoRegion, 0}, // // EU and JP versions have the same CRC
	{0x4E811100, UltramanFightingEvolution, JP, 0},
	{0xF7F181C3, DeathByDegreesTekkenNinaWilliams, CH, 0},
	{0xF088FA5B, DeathByDegreesTekkenNinaWilliams, KO, 0},
	{0xE1D6F85E, DeathByDegreesTekkenNinaWilliams, US, 0},
	{0x59683BB0, DeathByDegreesTekkenNinaWilliams, EU, 0},
	{0x830B6FB1, TalesofSymphonia, JP, 0},
	{0xFC0F8A5B, Simple2000Vol114, JP, 0},
	{0xBDD9BAAD, UrbanReign, US, 0},
	{0x0418486E, UrbanReign, RU, 0},
	{0xAE4BEBD3, UrbanReign, EU, 0},
	{0x48AC09BC, SteambotChronicles, EU, 0},
	{0x9F391882, SteambotChronicles, US, 0},
	{0xFEFCF9DE, SteambotChronicles, JP, 0}, // Ponkotsu Roman Daikatsugeki: Bumpy Trot
	{0XE1BF5DCA, SuperManReturns, US, 0},
	{0XE8F7BAB6, SuperManReturns, EU, 0},
	{0x06A7506A, SacredBlaze, JP, 0},
	{0x2479F4A9, Jak2, EU, 0},
	{0xF41C1B29, Jak2, EU, 0}, // Demo
	{0x9184AAF1, Jak2, US, 0},
	{0xA2034C69, Jak2, US, 0}, // Demo
	{0x25FE4D23, Jak2, KO, 0},
	{0xB4976DAF, Jak2, JP, 0}, // Jak II: Jak x Daxter 2
	{0x43D4FF3E, Jak2, JP, 0}, // Demo
	{0x12804727, Jak3, EU, 0},
	{0xE59E10BF, Jak3, EU, 0},
	{0xCA68E4D5, Jak3, EU, 0}, // Demo
	{0x644CFD03, Jak3, US, 0},
	{0xD401BC20, Jak3, US, 0}, // Demo
	{0xD1368EAE, Jak3, KO, 0},
	{0xDF659E77, JakX, EU, 0}, // Jak X: Combat Racing
	{0xC20596DB, JakX, EU, 0}, // Beta Trial Disc, v0.01
	{0x3091E6FB, JakX, US, 0},
	{0xC417D919, JakX, US, 0}, // Demo
	{0xDA366A53, JakX, US, 0}, // Public Beta v.1
	{0x7B564230, JakX, US, 0}, // Jak and Daxter Complete Trilogy Demo
	{0xDBA28C59, JakX, US, 0}, // Greatest Hits
};

std::map<u32, const CRC::Game*> CRC::m_map;

std::string ToLower(std::string str)
{
	transform(str.begin(), str.end(), str.begin(), ::tolower);
	return str;
}

// The exclusions list is a comma separated list of: the word "all" and/or CRCs in standard hex notation (0x and 8 digits with leading 0's if required).
// The list is case insensitive and order insensitive.
// E.g. Disable all CRC hacks:          CrcHacksExclusions=all
// E.g. Disable hacks for these CRCs:   CrcHacksExclusions=0x0F0C4A9C, 0x0EE5646B, 0x7ACF7E03
bool IsCrcExcluded(std::string exclusionList, u32 crc)
{
	std::string target = StringUtil::StdStringFromFormat("0x%08x", crc);
	exclusionList = ToLower(exclusionList);
	return exclusionList.find(target) != std::string::npos || exclusionList.find("all") != std::string::npos;
}

const CRC::Game& CRC::Lookup(u32 crc)
{
	printf("GS Lookup CRC:%08X\n", crc);
	if (m_map.empty())
	{
		std::string exclusions = theApp.GetConfigS("CrcHacksExclusions");
		if (exclusions.length() != 0)
			printf("GS: CrcHacksExclusions: %s\n", exclusions.c_str());
		int crcDups = 0;
		for (const Game& game : m_games)
		{
			if (!IsCrcExcluded(exclusions, game.crc))
			{
				if (m_map[game.crc])
				{
					printf("[FIXME] GS: Duplicate CRC: 0x%08X: (game-id/region-id) %d/%d overrides %d/%d\n", game.crc, game.title, game.region, m_map[game.crc]->title, m_map[game.crc]->region);
					crcDups++;
				}

				m_map[game.crc] = &game;
			}
			//else
			//	printf( "GS: excluding CRC hack for 0x%08x\n", game.crc );
		}
		if (crcDups)
			printf("[FIXME] GS: Duplicate CRC: Overall: %d\n", crcDups);
	}

	auto i = m_map.find(crc);

	if (i != m_map.end())
	{
		return *i->second;
	}

	return m_games[0];
}
