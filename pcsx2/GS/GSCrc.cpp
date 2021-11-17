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

const CRC::Game CRC::m_games[] =
{
	// Note: IDs 0x7ACF7E03, 0x7D4EA48F, 0x37C53760 - shouldn't be added as it's from the multiloaders when packing games.
	{0x00000000, NoTitle, NoRegion, 0},
	{0xF95F37EE, ArTonelico2, US, 0},
	{0x68CE6801, ArTonelico2, JP, 0},
	{0xCE2C1DBF, ArTonelico2, EU, 0},
	{0x2113EA2E, MetalSlug6, JP, 0},
	{0x42E05BAF, TomoyoAfter, JP, PointListPalette},
	{0x7800DC84, Clannad, JP, PointListPalette},
	{0xA6167B59, Lamune, JP, PointListPalette},
	{0xDDB59F46, KyuuketsuKitanMoonties, JP, PointListPalette},
	{0xC8EE2562, PiaCarroteYoukosoGPGakuenPrincess, JP, PointListPalette},
	{0x6CF94A43, KazokuKeikakuKokoroNoKizuna, JP, PointListPalette},
	{0xEDAF602D, DuelSaviorDestiny, JP, PointListPalette},
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
	{0xC19A374E, SoTC, US, 0},
	{0x7D8F539A, SoTC, EU, 0},
	{0x0F0C4A9C, SoTC, EU, 0},
	{0x877F3436, SoTC, JP, 0},
	{0xA17D6AAA, SoTC, KO, 0},
	{0x877B3D35, SoTC, CH, 0},
	{0x6F8545DB, ICO, US, 0},
	{0x48CDF317, ICO, US, 0}, // Demo
	{0xB01A4C95, ICO, JP, 0},
	{0x2DF2C1EA, ICO, KO, 0},
	{0x5C991F4E, ICO, EU, 0},
	{0x788D8B4F, ICO, EU, 0},
	{0x29C28734, ICO, CH, 0},
	{0x60013EBD, GTConcept, EU, 0},
	{0x6810C3BC, GTConcept, CH, 0}, // Gran Turismo Concept 2002 Tokyo-Geneva
	{0x0EEF32A3, GTConcept, KO, 0}, // Gran Turismo Concept 2002 Tokyo-Seoul
	{0x3E9D448A, GT3, CH, 0}, // cutie comment
	{0xAD66643C, GT3, CH, 0}, // cutie comment
	{0x85AE91B3, GT3, US, 0},
	{0x8AA991B0, GT3, US, 0},
	{0xC220951A, GT3, JP, 0},
	{0x9DE5CF65, GT3, JP, 0},
	{0x706DFF80, GT3, JP, 0}, // GT3 Store Disc Vol. 2
	{0x55CE5111, GT3, JP, 0}, // Gran Turismo 2000 Body Omen
	{0xE9A7E08D, GT3, JP, 0}, // Gran Turismo 2000 Body Omen
	{0xB590CE04, GT3, EU, 0},
	{0xC02C653E, GT4, CH, 0},
	{0x7ABDBB5E, GT4, CH, 0}, // cutie comment
	{0xAEAD1CA3, GT4, JP, 0},
	{0xE906EA37, GT4, JP, 0}, // GT4 First Preview
	{0xCA6243B9, GT4, JP, 0}, // GT4 Prologue
	{0xDD764BBE, GT4, JP, 0}, // GT4 Prologue
	{0xE1258846, GT4, JP, 0}, // GT4 Prologue
	{0x27B8F05F, GT4, JP, 0}, // GT4 Prius Trial Version
	{0x30E41D93, GT4, KO, 0},
	{0x715CF2EC, GT4, EU, 0},
	{0x44A61C8F, GT4, EU, 0},
	{0x0086E35B, GT4, EU, 0},
	{0x3FB69323, GT4, EU, 0}, // GT4 Prologue
	{0x77E61C8A, GT4, US, 0},
	{0x33C6E35E, GT4, US, 0},
	{0x70538747, GT4, US, 0}, // Toyota Prius Trial
	{0x32A1C752, GT4, US, 0}, // GT4 Online Beta
	{0x2A84A1E2, GT4, US, 0}, // Mazda MX-5 Edition
	{0x0087EEC4, GT4, NoRegion, 0}, // JP and US versions have the same CRC - GT4 Online Beta
	{0xC1640D2C, WildArms5, US, 0},
	{0x0FCF8FE4, WildArms5, EU, 0},
	{0x2294D322, WildArms5, JP, 0},
	{0x565B6170, WildArms4, JP, 0}, // Wild Arms: The 4th Detonator
	{0xBBC3EFFA, WildArms4, US, 0},
	{0xBBC396EC, WildArms4, US, 0}, // hmm such a small diff in the CRC..
	{0x7B2DE9CC, WildArms4, EU, 0},
	{0x8B029334, Manhunt2, EU, 0},
	{0x3B0ADBEF, Manhunt2, US, 0},
	{0x09F49E37, CrashBandicootWoC, NoRegion, 0},
	{0x103B5706, CrashBandicootWoC, US, 0}, // American Greatest Hits release
	{0x75182BE5, CrashBandicootWoC, US, 0},
	{0x5188ABCA, CrashBandicootWoC, US, 0},
	{0x34E2EEC7, CrashBandicootWoC, RU, 0},
	{0x3A03D62F, CrashBandicootWoC, EU, 0},
	{0x35D70452, CrashBandicootWoC, EU, 0},
	{0x72E1E60E, Spartan, EU, 0},
	{0x26689C87, Spartan, JP, 0},
	{0x08277A9E, Spartan, US, 0},
	{0xAC3C1147, SVCChaos, EU, 0}, // SVC Chaos: SNK vs. Capcom
	{0xB00FF2ED, SVCChaos, JP, 0},
	{0xA32F7CD0, AceCombat4, US, 0}, // Also needed for automatic mipmapping
	{0x5ED8FB53, AceCombat4, JP, 0},
	{0x1B9B7563, AceCombat4, EU, 0},
	{0x39B574F0, AceCombat5, US, 0}, // The Unsung War
	{0x86089F31, AceCombat5, JP, 0},
	{0x1D54FEA9, AceCombat5, EU, 0}, // Squadron Leader
	{0xFC46EA61, Tekken5, JP, 0},
	{0x1F88EE37, Tekken5, EU, 0},
	{0x1F88BECD, Tekken5, EU, 0}, // language selector...
	{0x652050D2, Tekken5, US, 0},
	{0xEA64EF39, Tekken5, KO, 0},
	{0x9E98B8AE, IkkiTousen, JP, 0},
	{0xD6385328, GodOfWar, US, 0},
	{0xF2A8D307, GodOfWar, US, 0},
	{0xA61A4C6D, GodOfWar, US, 0},
	{0xDF3A5A5C, GodOfWar, US, 0}, // Demo
	{0xFB0E6D72, GodOfWar, EU, 0},
	{0xEB001875, GodOfWar, EU, 0},
	{0xCF148C74, GodOfWar, EU, 0},
	{0xDF1AF973, GodOfWar, EU, 0},
	{0xCA052D22, GodOfWar, JP, 0},
	{0xBFCC1795, GodOfWar, KO, 0},
	{0x9567B7D6, GodOfWar, KO, 0},
	{0x9B5C97BA, GodOfWar, KO, 0},
	{0xE23D532B, GodOfWar, NoRegion, 0},
	{0x1A85E924, GodOfWar, NoRegion, 0}, // cutie comment
	{0x608ACBD3, GodOfWar, CH, 0}, // cutie comment
	// {0x1A85E924, GodOfWar, NoRegion, 0}, // same CRC as {0x1A85E924, DevilMayCry3, CH, 0}
	{0x2F123FD8, GodOfWar2, US, 0}, // same CRC as RU
	{0x44A8A22A, GodOfWar2, EU, 0},
	{0x60BC362B, GodOfWar2, EU, 0},
	{0x4340C7C6, GodOfWar2, KO, 0},
	{0xE96E55BD, GodOfWar2, JP, 0},
	{0xF8CD3DF6, GodOfWar2, NoRegion, 0},
	{0x0B82BFF7, GodOfWar2, NoRegion, 0},
	{0x5990866F, GodOfWar2, NoRegion, 0},
	{0xC4C4FD5F, GodOfWar2, CH, 0},
	{0xDCD9A9F7, GodOfWar2, EU, 0},
	{0xFA0DF523, GodOfWar2, CH, 0}, // cutie comment
	{0x9FEE3466, GodOfWar2, CH, 0}, // cutie comment
	{0x5D482F18, JackieChanAdv, EU, 0},
	{0xAC4DFD5A, JackieChanAdv, EU, 0},
	{0x95CC86EF, GiTS, US, 0}, // same CRC also reported as EU
	{0x2C5BF134, GiTS, US, 0}, // Demo
	{0xA5768F53, GiTS, JP, 0},
	{0xA3643EB1, GiTS, KO, 0},
	{0x28557423, GiTS, RU, 0},
	{0xBF6F101F, GiTS, EU, 0}, // same CRC as another US disc
	{0xF442260C, MajokkoALaMode2, JP, 0},
	{0xA616A6C2, TalesOfAbyss, US, 0},
	{0x14FE77F7, TalesOfAbyss, US, 0},
	{0xAA5EC3A3, TalesOfAbyss, JP, 0},
	{0xFB236A46, SonicUnleashed, US, 0},
	{0x8C913264, SonicUnleashed, EU, 0},
	{0xE04EA200, StarOcean3, EU, 0},
	{0x23A97857, StarOcean3, US, 0},
	{0xBEC32D49, StarOcean3, JP, 0},
	{0x8192A241, StarOcean3, JP, 0}, // NTSC JP special directors cut limited extra sugar on top edition (the special one :p)
	{0xCC96CE93, ValkyrieProfile2, US, 0},
	{0x774DE8E2, ValkyrieProfile2, JP, 0},
	{0x04CCB600, ValkyrieProfile2, EU, 0},
	{0xB65E141B, ValkyrieProfile2, DE, 0}, // PAL German
	{0x8510854E, ValkyrieProfile2, FR, 0},
	{0xC70FC973, ValkyrieProfile2, IT, 0},
	{0x47B9B2FD, RadiataStories, US, 0},
	{0xAC73005E, RadiataStories, JP, 0},
	{0xE8FCF8EC, SMTNocturne, US, 0},
	{0xF0A31EE3, SMTNocturne, EU, 0}, // SMTNocturne (Lucifers Call in EU)
	{0xAE0DE7B7, SMTNocturne, EU, 0}, // SMTNocturne (Lucifers Call in EU)
	{0xD60DA6D4, SMTNocturne, JP, 0}, // SMTNocturne
	{0x0E762E8D, SMTNocturne, JP, 0}, // SMTNocturne Maniacs
	{0x47BA9034, SMTNocturne, JP, 0}, // SMTNocturne Maniacs Chronicle
	{0xD3FFC263, SMTNocturne, KO, 0},
	{0x84D1A8DA, SMTNocturne, KO, 0},
	{0x0B8AB37B, RozenMaidenGebetGarden, JP, 0},
	{0xA33AF77A, TenchuFS, US, 0},
	{0x64C58FB4, TenchuFS, US, 0},
	{0xE7CCCB1E, TenchuFS, EU, 0},
	{0x89E63B40, TenchuFS, RU, 0}, // Beta
	{0x1969B19A, TenchuFS, ES, 0}, // PAL Spanish
	{0xBF0DC4CE, TenchuFS, DE, 0},
	{0x696BBEC3, TenchuFS, KO, 0},
	{0x525C1994, TenchuFS, ASIA, 0},
	{0x0D73BBCD, TenchuFS, KO, 0},
	{0x735A10C2, TenchuFS, JP, 0}, // Tenchu Kurenai
	{0xAFBFB287, TenchuWoH, KO, 0},
	{0xAFBEC8B7, TenchuWoH, KO, 0},
	{0x767E383D, TenchuWoH, US, 0},
	{0x83261085, TenchuWoH, DE, 0}, // PAL German
	{0x7FA1510D, TenchuWoH, EU, 0}, // PAL ES, IT
	{0xC8DADF58, TenchuWoH, EU, 0},
	{0x13DD9957, TenchuWoH, JP, 0},
	{0x506644B3, BigMuthaTruckers, EU, 0},
	{0x90F0D852, BigMuthaTruckers, US, 0},
	{0x92624842, BigMuthaTruckers, US, 0},
	{0xDD93DA88, BigMuthaTruckers, JP, 0}, // Bakusou Convoy Densetsu - Otoko Hanamichi America Roman
	{0xEB198738, LordOfTheRingsThirdAge, US, 0},
	{0x614F4CF4, LordOfTheRingsThirdAge, EU, 0},
	{0x37CD4279, LordOfTheRingsThirdAge, KO, 0},
	{0xE169BAF8, RedDeadRevolver, US, 0},
	{0xE2E67E23, RedDeadRevolver, EU, 0},
	{0xCBB87BF9, EvangelionJo, JP, 0}, // cutie comment
	{0xC5B75C7C, Oneechanbara2Special, JP, 0}, // cutie comment
	{0xC725CC6C, Oneechanbara2Special, JP, 0},
	{0x07608CA2, Oneechanbara2Special, EU, 0}, // Zombie Hunters 2
	{0xE0347841, XenosagaE3, JP, 0}, // cutie comment
	{0xA707236E, XenosagaE3, JP, 0}, // Demo
	{0xA4E88698, XenosagaE3, CH, 0},
	{0x2088950A, XenosagaE3, US, 0},
	{0xB1995E29, ShadowofRome, EU, 0}, // cutie comment
	{0x958DCA28, ShadowofRome, EU, 0},
	{0x57818AF6, ShadowofRome, US, 0},
	{0x1E210E60, ShadowofRome, US, 0}, // Demo
	{0x36393CD3, ShadowofRome, JP, 0},
	{0x694A998E, TombRaiderUnderworld, JP, 0}, // cutie comment
	{0x8E214549, TombRaiderUnderworld, EU, 0},
	{0x618769D6, TombRaiderUnderworld, US, 0},
	{0xB639EB17, TombRaiderAnniversary, US, 0}, // Also needed for automatic mipmapping
	{0xB05805B6, TombRaiderAnniversary, JP, 0}, // cutie comment
	{0xA629A376, TombRaiderAnniversary, EU, 0},
	{0xBC8B3F50, TombRaiderLegend, US, 0}, // cutie comment
	{0x365172A0, TombRaiderLegend, JP, 0},
	{0x05177ECE, TombRaiderLegend, EU, 0},
	{0xBEBF8793, BurnoutTakedown, US, 0},
	{0xBB2E845F, BurnoutTakedown, JP, 0},
	{0x5F060991, BurnoutTakedown, KO, 0},
	{0x75BECC18, BurnoutTakedown, EU, 0},
	{0xCE49B0DE, BurnoutTakedown, EU, 0},
	{0x381EE9EF, BurnoutTakedown, EU, 0}, // E3 Demo
	{0xD224D348, BurnoutRevenge, US, 0},
	{0x878E7A1D, BurnoutRevenge, JP, 0},
	{0xEEA60511, BurnoutRevenge, KO, 0},
	{0x7E83CC5B, BurnoutRevenge, EU, 0},
	{0x2CAC3DBC, BurnoutRevenge, EU, 0},
	{0x8C9576A1, BurnoutDominator, US, 0},
	{0xDDF76A98, BurnoutDominator, JP, 0},
	{0x8C9576B4, BurnoutDominator, EU, 0},
	{0x8C9C76B4, BurnoutDominator, EU, 0},
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
	{0XD3F182A3, Yakuza, EU, 0},
	{0x6F9F99F8, Yakuza, EU, 0},
	{0x388F687B, Yakuza, US, 0},
	{0xC1B91FC5, Yakuza, US, 0}, // Demo
	{0xB7B3800A, Yakuza, JP, 0},
	{0xA60C2E65, Yakuza2, EU, 0},
	{0x800E3E5A, Yakuza2, EU, 0},
	{0x97E9C87E, Yakuza2, US, 0},
	{0xB1EBD841, Yakuza2, US, 0},
	{0xC6B95C48, Yakuza2, JP, 0},
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
	{0xE21404E2, GetaWay, US, 0},
	{0xE8249852, GetaWay, JP, 0},
	{0x458485EF, GetaWay, EU, 0},
	{0x5DFBE144, GetaWay, EU, 0},
	{0xE78971DF, GetaWayBlackMonday, US, 0},
	{0x342D97FA, GetaWayBlackMonday, US, 0}, // Demo
	{0xE8C0AD1A, GetaWayBlackMonday, JP, 0},
	{0x09C3DF79, GetaWayBlackMonday, EU, 0},
	{0x1130BF23, SakuraTaisen, CH, 0}, // cutie comment
	{0x4FAE8B83, SakuraTaisen, KO, 0},
	{0xEF06DBD6, SakuraWarsSoLongMyLove, JP, 0}, // cutie comment
	{0xDD41054D, SakuraWarsSoLongMyLove, US, 0}, // cutie comment
	{0xC2E3A7A4, SakuraWarsSoLongMyLove, KO, 0},
	{0x4A4B623A, FightingBeautyWulong, JP, 0}, // cutie comment
	{0x5AC7E79C, TouristTrophy, CH, 0}, // cutie comment
	{0xFF9C0E93, TouristTrophy, US, 0},
	{0xCA9AA903, TouristTrophy, EU, 0},
	{0xAEDAEE99, GodHand, JP, 0},
	{0x6FB69282, GodHand, US, 0},
	{0x924C4AA6, GodHand, KO, 0},
	{0xDE9722A5, GodHand, EU, 0},
	{0x9637D496, KnightsOfTheTemple2, NoRegion, 0}, // // EU and JP versions have the same CRC
	{0x4E811100, UltramanFightingEvolution, JP, 0}, // cutie comment
	{0xF7F181C3, DeathByDegreesTekkenNinaWilliams, CH, 0}, // cutie comment
	{0xF088FA5B, DeathByDegreesTekkenNinaWilliams, KO, 0},
	{0xE1D6F85E, DeathByDegreesTekkenNinaWilliams, US, 0},
	{0x59683BB0, DeathByDegreesTekkenNinaWilliams, EU, 0},
	{0x830B6FB1, TalesofSymphonia, JP, 0},
	{0xFC0F8A5B, Simple2000Vol114, JP, 0},
	{0xBDD9BAAD, UrbanReign, US, 0}, // cutie comment
	{0x0418486E, UrbanReign, RU, 0},
	{0xAE4BEBD3, UrbanReign, EU, 0},
	{0x48AC09BC, SteambotChronicles, EU, 0},
	{0x9F391882, SteambotChronicles, US, 0},
	{0xFEFCF9DE, SteambotChronicles, JP, 0}, // Ponkotsu Roman Daikatsugeki: Bumpy Trot
	{0XE1BF5DCA, SuperManReturns, US, 0},
	{0XE8F7BAB6, SuperManReturns, EU, 0},
	{0x06A7506A, SacredBlaze, JP, 0},
	{0x9C712FF0, Jak1, EU, TextureInsideRt}, // Jak and Daxter: The Precursor Legacy
	{0x1B3976AB, Jak1, US, TextureInsideRt},
	{0x472E7699, Jak1, US, TextureInsideRt},
	{0x96A608C5, Jak1, US, TextureInsideRt}, // Cingular Wireless Demo, PS Underground Demo
	{0xEDE4FE64, Jak1, JP, TextureInsideRt}, // Jak x Daxter: Kyuusekai no Isan
	{0x2A7FD3B4, Jak1, JP, TextureInsideRt}, // Demo, Taikenba
	{0x2479F4A9, Jak2, EU, TextureInsideRt},
	{0xF41C1B29, Jak2, EU, TextureInsideRt}, // Demo
	{0x9184AAF1, Jak2, US, TextureInsideRt},
	{0xA2034C69, Jak2, US, TextureInsideRt}, // Demo
	{0x25FE4D23, Jak2, KO, TextureInsideRt},
	{0xB4976DAF, Jak2, JP, TextureInsideRt},
	{0x1ED2EF9E, Jak2, NoRegion, TextureInsideRt}, // EU Preview, EU Review
	{0x12804727, Jak3, EU, TextureInsideRt},
	{0xE59E10BF, Jak3, EU, TextureInsideRt},
	{0xCA68E4D5, Jak3, EU, TextureInsideRt}, // Demo
	{0x644CFD03, Jak3, US, TextureInsideRt},
	{0xD401BC20, Jak3, US, TextureInsideRt}, // Demo
	{0xD1368EAE, Jak3, KO, TextureInsideRt},
	{0x23F8D35B, Jak3, NoRegion, TextureInsideRt}, // EU Preview, EU Review, US Internal test build
	{0xDF659E77, JakX, EU, TextureInsideRt}, // Jak X: Combat Racing
	{0xC20596DB, JakX, EU, TextureInsideRt}, // Beta Trial Disc, v0.01
	{0x3091E6FB, JakX, US, TextureInsideRt},
	{0xC417D919, JakX, US, TextureInsideRt}, // Demo
	{0xDA366A53, JakX, US, TextureInsideRt}, // Public Beta v.1
	{0x7B564230, JakX, US, TextureInsideRt}, // Jak and Daxter Complete Trilogy Demo
	{0xDBA28C59, JakX, US, TextureInsideRt}, // Greatest Hits
	{0x4653CA3E, HarleyDavidson, US, 0},
	// Games list for Automatic Mipmapping
	// Basic mipmapping
	{0x194C9F38, AceCombatZero, EU, 0}, // Ace Combat: The Belkan War
	{0x65729657, AceCombatZero, US, 0},
	{0xA04B52DB, AceCombatZero, JP, 0},
	{0x2799A4E5, AceCombatZero, KO, 0},
	{0x09B3AD4D, ApeEscape2, EU, 0},
	{0xBDD9F5E1, ApeEscape2, US, 0},
	{0xFE0A6AB6, ApeEscape2, JP, 0}, // Saru! Get You! 2
	{0x64A9982B, ApeEscape2, CH, 0},
	{0xEC8EF2DE, Barnyard, US, 0}, // Nickelodeon: Barnyard
	{0x0B2F3DEE, Barnyard, KO, 0},
	{0x5267A845, Barnyard, EU, 0},
	{0x0940508D, BrianLaraInternationalCricket, EU, 0},
	{0x0BAA8DD8, DarkCloud, EU, 0},
	{0x1DF41F33, DarkCloud, US, 0},
	{0xA5C05C78, DarkCloud, US, 0},
	{0x60AA5049, DarkCloud, KO, 0},
	{0xECD8E386, DarkCloud, JP, 0},
	{0x67A29886, DestroyAllHumans, US, 0},
	{0xE3E8E893, DestroyAllHumans, EU, 0},
	{0x42DF8C8C, DestroyAllHumans2, US, 0},
	{0x743E10C2, DestroyAllHumans2, EU, 0},
	{0x67C38BAA, FIFA03, US, 0},
	{0x722BBD62, FIFA03, EU, 0},
	{0x2BCCF704, FIFA03, EU, 0},
	{0xCC6AA742, FIFA04, KO, 0},
	{0x2C6A4E2E, FIFA04, US, 0},
	{0x684ADFC6, FIFA04, EU, 0},
	{0x972611BB, FIFA05, US, 0},
	{0x972719A3, FIFA05, EU, 0},
	{0xC5473413, HarryPotterATCOS, NoRegion, 0}, // EU and US versions have the same CRC - Chamber Of Secrets
	{0xE1963055, HarryPotterATCOS, JP, 0}, // Harry Potter to Himitsu no Heya
	{0xE90BE9F8, HarryPotterATCOS, JP, 0}, // Coca Cola original Version
	{0xB38CC628, HarryPotterATGOF, US, 0},
	{0xCDE017A7, HarryPotterATGOF, KO, 0},
	{0xB18DC525, HarryPotterATGOF, EU, 0},
	{0x9C3A84F4, HarryPotterATHBP, US, 0}, // Half-Blood Prince
	{0xCB598BC2, HarryPotterATHBP, EU, 0},
	{0x51E019BC, HarryPotterATPOA, NoRegion, 0}, // EU and US versions have the same CRC - Prisoner of Azkaban
	{0x99A8B4FF, HarryPotterATPOA, KO, 0},
	{0xA8901AD6, HarryPotterATPOA, JP, 0}, // Harry Potter to Azkaban no Shuujin
	{0x51E417AA, HarryPotterATPOA, EU, 0},
	{0x4C01B1B0, HarryPotterOOTP, US, 0}, // Order Of The Phoenix
	{0x01A9BF0E, HarryPotterOOTP, EU, 0},
	{0x230CB71D, SoulReaver2, US, 0},
	{0x1771BFE4, SoulReaver2, US, 0},
	{0x6F991F52, SoulReaver2, JP, 0},
	{0x1B7FF35A, SoulReaver2, KO, 0},
	{0x6D8B4CD1, SoulReaver2, EU, 0},
	{0x728AB07C, LegacyOfKainDefiance, US, 0},
	{0xBCAD1E8A, LegacyOfKainDefiance, EU, 0},
	{0x28D09BF9, NicktoonsUnite, US, 0},
	{0xF25266C4, NicktoonsUnite, EU, 0}, // Nickelodeon SpongeBob SquarePants And Friends Unite
	{0x7AE1C04B, Persona3, US, 0}, // Regular Version
	{0x05C3D28F, Persona3, JP, 0},
	{0xBCD68B1E, Persona3, KO, 0},
	{0x8A557EE5, Persona3, EU, 0},
	{0x94A82AAA, Persona3, US, 0}, // FES
	{0x232C7D72, Persona3, JP, 0},
	{0x8897C208, Persona3, KO, 0},
	{0xF64A6AE5, Persona3, EU, 0},
	{0x2BDA8ADB, ProjectSnowblind, US, 0},
	{0xF00CA82B, ProjectSnowblind, EU, 0},
	{0xF1583665, ProjectSnowblind, EU, 0},
	{0xA56A0525, Quake3Revolution, US, 0},
	{0x2064ACE6, Quake3Revolution, EU, 0},
	{0xCE4933D0, RatchetAndClank, US, 0},
	{0x6F191506, RatchetAndClank, US, 0}, // E3 Demo
	{0x81CBFEA2, RatchetAndClank, US, 0}, // EB Games Demo
	{0x56A35F77, RatchetAndClank, JP, 0},
	{0x76F724A3, RatchetAndClank, EU, 0},
	{0x6A8F18B9, RatchetAndClank, EU, 0},
	{0xB3A71D10, RatchetAndClank2, US, 0}, // Going Commando
	{0x38996035, RatchetAndClank2, US, 0},
	{0xDF6F94A1, RatchetAndClank2, US, 0}, // Demo - Going Commando & Jak II
	{0x8CAA5F16, RatchetAndClank2, JP, 0}, // Gagaga! Ginga no Commando-ssu
	{0x2F486E6F, RatchetAndClank2, EU, 0},
	{0x45FE0CC4, RatchetAndClank3, US, 0}, // Up Your Arsenal
	{0x2A12175A, RatchetAndClank3, US, 0}, // Regular Demo
	{0xD8EB2C29, RatchetAndClank3, US, 0}, // 1108 Beta
	{0x64DC6000, RatchetAndClank3, JP, 0}, // Totsugeki! Galactic Rangers
	{0x17125698, RatchetAndClank3, EU, 0},
	{0x9BFBCD42, RatchetAndClank4, US, 0}, // Deadlocked
	{0x2EC9DA96, RatchetAndClank4, JP, 0}, // GiriGiri Ginga no Giga Battle
	{0xD697D204, RatchetAndClank4, EU, 0}, // Ratchet Gladiator
	{0x8661F7BA, RatchetAndClank5, US, 0}, // Size Matters
	{0xFCB981D5, RatchetAndClank5, EU, 0}, // Size Matters
	{0x8634861F, RickyPontingInternationalCricket, EU, 0},
	{0xDDAC3815, Shox, US, 0},
	{0xF84FE9DE, Shox, KO, 0},
	{0x09F4038B, Shox, EU, 0},
	{0x78FFA39F, Shox, EU, 0},
	{0x3DF10389, Shox, EU, 0},
	{0x43CC009B, SlamTennis, EU, 0},
	{0xF17AF8BD, TheIncredibleHulkUD, US, 0},
	{0xEA8D4BDF, TheIncredibleHulkUD, US, 0},
	{0x6B3D50A5, TheIncredibleHulkUD, EU, 0},
	{0x2B58234D, TribesAerialAssault, US, 0},
	{0x4D22DB95, Whiplash, US, 0},
	{0xE8A97250, Whiplash, EU, 0},
	{0xB1BE3E51, Whiplash, EU, 0},
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
	std::string target = format("0x%08x", crc);
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
