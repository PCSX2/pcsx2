/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "GS/Renderers/HW/GSRendererHW.h"

class GSHwHack
{
public:
	static bool GSC_DeathByDegreesTekkenNinaWilliams(GSRendererHW& r, int& skip);
	static bool GSC_GiTS(GSRendererHW& r, int& skip);
	static bool GSC_Manhunt2(GSRendererHW& r, int& skip);
	static bool GSC_SacredBlaze(GSRendererHW& r, int& skip);
	static bool GSC_SakuraTaisen(GSRendererHW& r, int& skip);
	static bool GSC_SFEX3(GSRendererHW& r, int& skip);
	static bool GSC_Tekken5(GSRendererHW& r, int& skip);
	static bool GSC_BurnoutGames(GSRendererHW& r, int& skip);
	static bool GSC_BlackAndBurnoutSky(GSRendererHW& r, int& skip);
	static bool GSC_MidnightClub3(GSRendererHW& r, int& skip);
	static bool GSC_TalesOfLegendia(GSRendererHW& r, int& skip);
	static bool GSC_Kunoichi(GSRendererHW& r, int& skip);
	static bool GSC_ZettaiZetsumeiToshi2(GSRendererHW& r, int& skip);
	static bool GSC_SakuraWarsSoLongMyLove(GSRendererHW& r, int& skip);
	static bool GSC_KnightsOfTheTemple2(GSRendererHW& r, int& skip);
	static bool GSC_UltramanFightingEvolution(GSRendererHW& r, int& skip);
	static bool GSC_TalesofSymphonia(GSRendererHW& r, int& skip);
	static bool GSC_Simple2000Vol114(GSRendererHW& r, int& skip);
	static bool GSC_UrbanReign(GSRendererHW& r, int& skip);
	static bool GSC_SteambotChronicles(GSRendererHW& r, int& skip);
	static bool GSC_GetawayGames(GSRendererHW& r, int& skip);
	static bool GSC_BlueTongueGames(GSRendererHW& r, int& skip);
	static bool GSC_Battlefield2(GSRendererHW& r, int& skip);
	static bool GSC_NFSUndercover(GSRendererHW& r, int& skip);
	static bool GSC_PolyphonyDigitalGames(GSRendererHW& r, int& skip);
	static bool GSC_MetalGearSolid3(GSRendererHW& r, int& skip);
	static bool GSC_BigMuthaTruckers(GSRendererHW& r, int& skip);

	static bool OI_PointListPalette(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	static bool OI_DBZBTGames(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	static bool OI_FFX(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	static bool OI_RozenMaidenGebetGarden(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	static bool OI_SonicUnleashed(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	static bool OI_ArTonelico2(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	static bool OI_BurnoutGames(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	static bool OI_Battlefield2(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	static bool OI_HauntingGround(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);

	static bool MV_Growlanser(GSRendererHW& r);
	static bool MV_Ico(GSRendererHW& r);

	template <typename F>
	struct Entry
	{
		const char* name;
		F ptr;
	};

	static const Entry<GSRendererHW::GSC_Ptr> s_get_skip_count_functions[];
	static const Entry<GSRendererHW::OI_Ptr> s_before_draw_functions[];
	static const Entry<GSRendererHW::MV_Ptr> s_move_handler_functions[];
};
