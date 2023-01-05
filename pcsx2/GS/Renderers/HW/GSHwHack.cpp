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
#include "GS/Renderers/HW/GSRendererHW.h"
#include "GS/Renderers/HW/GSHwHack.h"
#include "GS/GSGL.h"

static bool s_nativeres;
static CRCHackLevel s_crc_hack_level = CRCHackLevel::Full;

#define CRC_Partial (s_crc_hack_level >= CRCHackLevel::Partial)
#define CRC_Full (s_crc_hack_level >= CRCHackLevel::Full)
#define CRC_Aggressive (s_crc_hack_level >= CRCHackLevel::Aggressive)

#define RPRIM r.PRIM
#define RCONTEXT r.m_context

////////////////////////////////////////////////////////////////////////////////
// Partial level, broken on all renderers.
////////////////////////////////////////////////////////////////////////////////

bool GSHwHack::GSC_BigMuthaTruckers(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && (fi.TBP0 == 0x01400 || fi.TBP0 == 0x012c0) && fi.FPSM == fi.TPSM && fi.TPSM == PSM_PSMCT16)
		{
			// Mid-texture pointer is a cache miss,
			// luckily we replace a half-screen TS effect with a full-screen one in
			// EmulateTextureShuffleAndFbmask (see #2934).
			// While this works for the time being, it's not ideal.
			// Skip the unneeded extra TS draw.
			skip = 1;
		}
	}

	return true;
}

bool GSHwHack::GSC_DeathByDegreesTekkenNinaWilliams(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	// Note: Game also has issues with texture shuffle not supported on strange clamp mode.
	// See https://forums.pcsx2.net/Thread-GSDX-Texture-Cache-Bug-Report-Death-By-Degrees-SLUS-20934-NTSC
	if (skip == 0)
	{
		if (!s_nativeres && fi.TME && fi.FBP == 0 && fi.TBP0 == 0x34a0 && fi.TPSM == PSM_PSMCT32)
		{
			// Don't enable hack on native res if crc is below aggressive.
			// Upscaling issue similar to Tekken 5.
			skip = 1; // Animation pane
		}
		else if (CRC_Aggressive && fi.FBP == 0x3500 && fi.TPSM == PSM_PSMT8 && fi.FBMSK == 0xFFFF00FF)
		{
			// Needs to be further tested so put it on Aggressive for now, likely channel shuffle.
			skip = 4; // Underwater white fog
		}
	}
	else
	{
		if (!s_nativeres && fi.TME && (fi.FBP | fi.TBP0 | fi.FPSM | fi.TPSM) && fi.FBMSK == 0x00FFFFFF)
		{
			// Needs to be further tested so assume it's related with the upscaling hack.
			skip = 1; // Animation speed
		}
	}

	return true;
}

bool GSHwHack::GSC_GiTS(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x03000 && fi.FPSM == PSM_PSMCT32 && fi.TPSM == PSM_PSMT8)
		{
			// Channel effect not properly supported yet
			skip = 9;
		}
	}

	return true;
}

// Channel effect not properly supported yet
bool GSHwHack::GSC_Manhunt2(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	/*
	 * The game readback RT as 8 bits index texture to apply a non-linear brightness/gamma correction on all channel
	 * It could be written in HLE shader as:
	 * out = blue_lut[in.blue] + green_lut[in.green] + blue_lut[in.blue]
	 *
	 * Unlike others games (which do all pages of a channel), man hunt apply the 3 channel corrections by page.
	 * (in short it is loop index/loop page instead of loop page/loop index)
	 *
	 * It is very annoying to detect.So in order to fix the effect the best
	 * solution will be to implement an alternate draw call and then skip the
	 * useless gs draw call.
	 *
	 * Blue  Palette correction is located @ 0x3C08 (TEX0.CBP of the first draw call that will fire the effect)
	 * Green Palette correction is located @ 0x3C04
	 * Blue  Palette correction is located @ 0x3C00
	 * Either we upload the data as a new texture or we could hardcode them in a shader
	 *
	 */
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x03c20 && fi.FPSM == PSM_PSMCT32 && fi.TBP0 == 0x01400 && fi.TPSM == PSM_PSMT8)
		{
			skip = 640;
		}
	}

	return true;
}

bool GSHwHack::GSC_CrashBandicootWoC(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	// Channel effect not properly supported - Removes fog to fix the fog wall issue on Direct3D at any resolution, and while upscaling on every Hardware renderer.
	if (skip == 0)
	{
		if (fi.TME && (fi.FBP == 0x00000 || fi.FBP == 0x008c0 || fi.FBP == 0x00a00) && (fi.TBP0 == 0x00000 || fi.TBP0 == 0x008c0 || fi.TBP0 == 0x00a00) && fi.FBP == fi.TBP0 && fi.FPSM == PSM_PSMCT32 && fi.FPSM == fi.TPSM)
		{
			return false; // allowed
		}

		if (fi.TME && (fi.FBP == 0x01e40 || fi.FBP == 0x02200) && fi.FPSM == PSM_PSMZ24 && (fi.TBP0 == 0x01180 || fi.TBP0 == 0x01400) && fi.TPSM == PSM_PSMZ24)
		{
			skip = 42;
		}
	}
	else
	{
		if (fi.TME && (fi.FBP == 0x00000 || fi.FBP == 0x008c0 || fi.FBP == 0x00a00) && fi.FPSM == PSM_PSMCT32 && fi.TBP0 == 0x03c00 && fi.TPSM == PSM_PSMCT32)
		{
			skip = 0;
		}
		else if (!fi.TME && (fi.FBP == 0x00000 || fi.FBP == 0x008c0 || fi.FBP == 0x00a00))
		{
			skip = 0;
		}
	}

	return true;
}

bool GSHwHack::GSC_SacredBlaze(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	//Fix Sacred Blaze rendering glitches
	if (skip == 0)
	{
		if (fi.TME && (fi.FBP == 0x0000 || fi.FBP == 0x0e00) && (fi.TBP0 == 0x2880 || fi.TBP0 == 0x2a80) && fi.FPSM == fi.TPSM && fi.TPSM == PSM_PSMCT32 && fi.FBMSK == 0x0)
		{
			skip = 1;
		}
	}

	return true;
}

bool GSHwHack::GSC_Spartan(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME)
		{
			// depth textures (bully, mgs3s1 intro, Front Mission 5)
			if ((fi.TPSM == PSM_PSMZ32 || fi.TPSM == PSM_PSMZ24 || fi.TPSM == PSM_PSMZ16 || fi.TPSM == PSM_PSMZ16S) ||
				// General, often problematic post processing
				(GSUtil::HasSharedBits(fi.FBP, fi.FPSM, fi.TBP0, fi.TPSM)))
			{
				skip = 2;
			}
		}
	}

	return true;
}

bool GSHwHack::GSC_Oneechanbara2Special(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (!s_nativeres && fi.TPSM == PSM_PSMCT24 && fi.TME && fi.FBP == 0x01180)
		{
			// Don't enable hack on native res if crc is below aggressive.
			// Ghosting upscaling issue, bottom and right red lines also by upscaling.
			skip = 1;
		}
	}

	return true;
}

bool GSHwHack::GSC_SakuraTaisen(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (!fi.TME && (fi.FBP == 0x0 || fi.FBP == 0x1180) && (fi.TBP0 != 0x3fc0 && fi.TBP0 != 0x3c9a && fi.TBP0 != 0x3dec /*fi.TBP0 ==0x38d0 || fi.TBP0==0x3912 ||fi.TBP0==0x3bdc ||fi.TBP0==0x3ab3 ||fi.TBP0<=0x3a92*/) && fi.FPSM == PSM_PSMCT32 && (fi.TPSM == PSM_PSMT8 || fi.TPSM == PSM_PSMT4) && (fi.FBMSK == 0x00FFFFFF || !fi.FBMSK))
		{
			skip = 0; //3dec 3fc0 3c9a
		}
		if (!fi.TME && (fi.FBP | fi.TBP0) != 0 && (fi.FBP | fi.TBP0) != 0x1180 && (fi.FBP | fi.TBP0) != 0x3be0 && (fi.FBP | fi.TBP0) != 0x3c80 && fi.TBP0 != 0x3c9a && (fi.FBP | fi.TBP0) != 0x3d80 && fi.TBP0 != 0x3dec && fi.FPSM == PSM_PSMCT32 && (fi.FBMSK == 0))
		{
			skip = 0; //3dec 3fc0 3c9a
		}
		if (!fi.TME && (fi.FBP | fi.TBP0) != 0 && (fi.FBP | fi.TBP0) != 0x1180 && (fi.FBP | fi.TBP0) != 0x3be0 && (fi.FBP | fi.TBP0) != 0x3c80 && (fi.FBP | fi.TBP0) != 0x3d80 && fi.TBP0 != 0x3c9a && fi.TBP0 != 0x3de && fi.FPSM == PSM_PSMCT32 && (fi.FBMSK == 0))
		{
			skip = 1; //3dec 3fc0 3c9a
		}
		else if (fi.TME && (fi.FBP == 0 || fi.FBP == 0x1180) && fi.TBP0 == 0x35B8 && fi.TPSM == PSM_PSMT4)
		{
			skip = 1;
		}
		else
		{
			if (!fi.TME && (fi.FBP | fi.TBP0) == 0x38d0 && fi.FPSM == PSM_PSMCT32)
			{
				skip = 1; //3dec 3fc0 3c9a
			}
		}
	}

	return true;
}

bool GSHwHack::GSC_SFEX3(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x00500 && fi.FPSM == PSM_PSMCT16 && fi.TBP0 == 0x00f00 && fi.TPSM == PSM_PSMCT16)
		{
			// Not an upscaling issue.
			// Elements on the screen show double/distorted.
			skip = 2;
		}
	}

	return true;
}

bool GSHwHack::GSC_Tekken5(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (!s_nativeres && fi.TME && (fi.FBP == 0x02d60 || fi.FBP == 0x02d80 || fi.FBP == 0x02ea0 || fi.FBP == 0x03620 || fi.FBP == 0x03640) && fi.FPSM == fi.TPSM && fi.TBP0 == 0x00000 && fi.TPSM == PSM_PSMCT32)
		{
			// Don't enable hack on native res if crc is below aggressive.
			// Fixes/removes ghosting/blur effect and white lines appearing in stages: Moonfit Wilderness, Acid Rain - caused by upscaling.
			// Downside is it also removes the channel effect which is fixed.
			// Let's enable this hack for Aggressive only since it's an upscaling issue for both renders.
			skip = 95;
		}
		else if (fi.ZTST == 1 && fi.TME && (fi.FBP == 0x02bc0 || fi.FBP == 0x02be0 || fi.FBP == 0x02d00 || fi.FBP == 0x03480 || fi.FBP == 0x034a0) && fi.FPSM == fi.TPSM && fi.TBP0 == 0x00000 && fi.TPSM == PSM_PSMCT32)
		{
			// The moving display effect(flames) is not emulated properly in the entire screen so let's remove the effect in the stage: Burning Temple. Related to half screen bottom issue.
			// Fixes black lines in the stage: Burning Temple - caused by upscaling. Note the black lines can also be fixed with Merge Sprite hack.
			skip = 2;
		}
	}

	return true;
}

bool GSHwHack::GSC_TombRaiderAnniversary(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x01000 && fi.FPSM == fi.TPSM && fi.TPSM == PSM_PSMCT32)
		{
			skip = 1; // Garbage TC
		}
	}

	return true;
}

bool GSHwHack::GSC_TombRaiderLegend(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		// ||fi.TBP0 ==0x2F00
		if (fi.TME && fi.FBP == 0x01000 && fi.FPSM == fi.TPSM && fi.TPSM == PSM_PSMCT32 && (fi.TBP0 == 0x2b60 || fi.TBP0 == 0x2b80 || fi.TBP0 == 0x2E60 || fi.TBP0 == 0x3020 || fi.TBP0 == 0x3200 || fi.TBP0 == 0x3320))
		{
			skip = 1; // Garbage TC
		}
		else if (fi.TPSM == PSM_PSMCT32 && (fi.TPSM | fi.FBP) == 0x2fa0 && (fi.TBP0 == 0x2bc0) && fi.FBMSK == 0)
		{
			skip = 2; // Underwater black screen
		}
	}

	return true;
}

bool GSHwHack::GSC_TombRaiderUnderWorld(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x01000 && fi.FPSM == fi.TPSM && fi.TPSM == PSM_PSMCT32 && (fi.TBP0 == 0x2B60 /*|| fi.TBP0 == 0x2EFF || fi.TBP0 ==0x2F00 || fi.TBP0 == 0x3020*/ || (fi.TBP0 >= 0x2C01 && fi.TBP0 != 0x3029 && fi.TBP0 != 0x302d)))
		{
			skip = 1; // Garbage TC
		}
		else if (fi.TPSM == PSM_PSMCT32 && (fi.TPSM | fi.FBP) == 0x2c00 && (fi.TBP0 == 0x0ee0) && fi.FBMSK == 0)
		{
			skip = 2; // Underwater black screen
		}
	}

	return true;
}

bool GSHwHack::GSC_BurnoutGames(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && (fi.FBP == 0x01dc0 || fi.FBP == 0x01c00 || fi.FBP == 0x01f00 || fi.FBP == 0x01d40 || fi.FBP == 0x02200 || fi.FBP == 0x02000) && fi.FPSM == fi.TPSM && (fi.TBP0 == 0x01dc0 || fi.TBP0 == 0x01c00 || fi.TBP0 == 0x01f00 || fi.TBP0 == 0x01d40 || fi.TBP0 == 0x02200 || fi.TBP0 == 0x02000) && fi.TPSM == PSM_PSMCT32)
		{
			// 0x01dc0 01c00(MP) ntsc, 0x01f00 0x01d40(MP) ntsc progressive, 0x02200(MP) pal.
			// Yellow stripes.
			// Multiplayer tested only on Takedown.
			skip = GSConfig.UserHacks_AutoFlush ? 2 : 4;
		}
	}

	return true;
}

bool GSHwHack::GSC_MidnightClub3(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && (fi.FBP > 0x01d00 && fi.FBP <= 0x02a00) && fi.FPSM == PSM_PSMCT32 && (fi.FBP >= 0x01600 && fi.FBP < 0x03260) && fi.TPSM == PSM_PSMT8H)
		{
			// Vram usage.
			// Tested: tokyo default cruise.
			// Move around a bit, stop car, wait as vram goes down, start moving again, vram spike.
			skip = 1;
		}
	}

	return true;
}

bool GSHwHack::GSC_TalesOfLegendia(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && (fi.FBP == 0x3f80 || fi.FBP == 0x03fa0) && fi.FPSM == PSM_PSMCT32 && fi.TPSM == PSM_PSMT8)
		{
			skip = 3; // 3, 9
		}
		if (fi.TME && fi.FBP == 0x3800 && fi.FPSM == PSM_PSMCT32 && fi.TPSM == PSM_PSMZ32)
		{
			skip = 2;
		}
		if (fi.TME && fi.FBP && fi.FPSM == PSM_PSMCT32 && fi.TBP0 == 0x3d80)
		{
			skip = 1; // Missing block 2a00 in the upper left
		}
		if (fi.TME && fi.FBP == 0x1c00 && (fi.TBP0 == 0x2e80 || fi.TBP0 == 0x2d80) && fi.TPSM == 0 && fi.FBMSK == 0xff000000)
		{
			skip = 1; // Ghosting
		}
		if (!fi.TME && fi.FBP == 0x2a00 && (fi.TBP0 == 0x1C00) && fi.TPSM == 0 && fi.FBMSK == 0x00FFFFFF)
		{
			skip = 1; // Poisoned layer dislocation
		}
	}

	return true;
}

bool GSHwHack::GSC_Kunoichi(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (!fi.TME && (fi.FBP == 0x0 || fi.FBP == 0x00700 || fi.FBP == 0x00800) && fi.FPSM == PSM_PSMCT32 && fi.FBMSK == 0x00FFFFFF)
		{
			// Removes depth effects(shadows) not rendered correctly on all renders.
			skip = 3;
		}
		if (fi.TME && (fi.FBP == 0x0700 || fi.FBP == 0) && fi.TBP0 == 0x0e00 && fi.TPSM == 0 && fi.FBMSK == 0)
		{
			skip = 1; // Removes black screen (not needed anymore maybe)?
		}
	}
	else
	{
		if (fi.TME && (fi.FBP == 0x0e00) && fi.FPSM == PSM_PSMCT32 && fi.FBMSK == 0xFF000000)
		{
			skip = 0;
		}
	}

	return true;
}

bool GSHwHack::GSC_ZettaiZetsumeiToshi2(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.TPSM == PSM_PSMCT16S && (fi.FBMSK >= 0x6FFFFFFF || fi.FBMSK == 0))
		{
			skip = 1000;
		}
		else if (fi.TME && fi.TPSM == PSM_PSMCT32 && fi.FBMSK == 0xFF000000)
		{
			skip = 2; // Fog
		}
		else if ((fi.FBP | fi.TBP0) && fi.FPSM == fi.TPSM && fi.TPSM == PSM_PSMCT16 && fi.FBMSK == 0x3FFF)
		{
			// Note start of the effect (texture shuffle) is fixed but maybe not the extra draw call
			skip = 1000;
		}
	}
	else
	{
		if (!fi.TME && fi.TPSM == PSM_PSMCT32 && fi.FBP == 0x1180 && fi.TBP0 == 0x1180 && (fi.FBMSK == 0))
		{
			skip = 0;
		}
		if (fi.TME && fi.TPSM == PSM_PSMT4 && fi.FBP && (fi.TBP0 != 0x3753))
		{
			skip = 0;
		}
		if (fi.TME && fi.TPSM == PSM_PSMT8H && fi.FBP == 0x22e0 && fi.TBP0 == 0x36e0)
		{
			skip = 0;
		}
		if (!fi.TME && fi.TPSM == PSM_PSMT8H && fi.FBP == 0x22e0)
		{
			skip = 0;
		}
		if (fi.TME && fi.TPSM == PSM_PSMT8 && (fi.FBP == 0x1180 || fi.FBP == 0) && (fi.TBP0 != 0x3764 && fi.TBP0 != 0x370f))
		{
			skip = 0;
		}
		if (fi.TME && fi.TPSM == PSM_PSMCT16S && (fi.FBP == 0x1180))
		{
			skip = 2;
		}
	}

	return true;
}

bool GSHwHack::GSC_SakuraWarsSoLongMyLove(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME == 0 && fi.FBP != fi.TBP0 && fi.TBP0 && fi.FBMSK == 0x00FFFFFF)
		{
			skip = 3; // Remove darkness
		}
		else if (fi.TME == 0 && fi.FBP == fi.TBP0 && (fi.TBP0 == 0x1200 || fi.TBP0 == 0x1180 || fi.TBP0 == 0) && fi.FBMSK == 0x00FFFFFF)
		{
			skip = 3; // Remove darkness
		}
		else if (fi.TME && (fi.FBP == 0 || fi.FBP == 0x1180) && fi.FPSM == PSM_PSMCT32 && fi.TBP0 == 0x3F3F && fi.TPSM == PSM_PSMT8)
		{
			skip = 1; // Floodlight
		}
	}

	return true;
}

bool GSHwHack::GSC_FightingBeautyWulong(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (!s_nativeres && fi.TME && (fi.TBP0 == 0x0700 || fi.TBP0 == 0x0a80) && (fi.TPSM == PSM_PSMCT32 || fi.TPSM == PSM_PSMCT24))
		{
			// Don't enable hack on native res if crc is below aggressive.
			// removes glow/blur which cause ghosting and other sprite issues similar to Tekken 5
			skip = 1;
		}
	}

	return true;
}

bool GSHwHack::GSC_GodHand(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && (fi.FBP == 0x0) && (fi.TBP0 == 0x2800) && fi.FPSM == fi.TPSM && fi.TPSM == PSM_PSMCT32)
		{
			skip = 1; // Blur
		}
	}

	return true;
}

bool GSHwHack::GSC_KnightsOfTheTemple2(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TPSM == PSM_PSMT8H && fi.FBMSK == 0)
		{
			skip = 1; // Ghosting
		}
		else if (fi.TPSM == 0x00000 && PSM_PSMCT24 && fi.TME && (fi.FBP == 0x3400 || fi.FBP == 0x3a00))
		{
			skip = 1; // Light source
		}
	}

	return true;
}

bool GSHwHack::GSC_UltramanFightingEvolution(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (!s_nativeres && fi.TME && fi.FBP == 0x2a00 && fi.FPSM == PSM_PSMZ24 && fi.TBP0 == 0x1c00 && fi.TPSM == PSM_PSMZ24)
		{
			// Don't enable hack on native res if crc is below aggressive.
			skip = 5; // blur
		}
	}

	return true;
}

bool GSHwHack::GSC_TalesofSymphonia(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FPSM == PSM_PSMCT32 && (fi.TBP0 == 0x2bc0 || fi.TBP0 <= 0x0200) && (fi.FBMSK == 0xFF000000 || fi.FBMSK == 0x00FFFFFF))
		{
			skip = 1; //fi.FBMSK==0 Causing an animated black screen to speed up the battle
		}
		if (fi.TME && (fi.TBP0 == 0x1180 || fi.TBP0 == 0x1a40 || fi.TBP0 == 0x2300) && fi.FBMSK >= 0xFF000000)
		{
			skip = 1; // Afterimage
		}
	}

	return true;
}

bool GSHwHack::GSC_Simple2000Vol114(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (!s_nativeres && fi.TME == 0 && (fi.FBP == 0x1500) && (fi.TBP0 == 0x2c97 || fi.TBP0 == 0x2ace || fi.TBP0 == 0x03d0 || fi.TBP0 == 0x2448) && (fi.FBMSK == 0x0000))
		{
			// Don't enable hack on native res if crc is below aggressive.
			// Upscaling issues, removes glow/blur effect which fixes ghosting.
			skip = 1;
		}
		if (fi.TME && (fi.FBP == 0x0e00) && (fi.TBP0 == 0x1000) && (fi.FBMSK == 0x0000))
		{
			// Depth shadows.
			skip = 1;
		}
	}

	return true;
}

bool GSHwHack::GSC_UrbanReign(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x0000 && fi.TBP0 == 0x3980 && fi.FPSM == fi.TPSM && fi.TPSM == PSM_PSMCT32 && fi.FBMSK == 0x0)
		{
			skip = 1; // Black shadow
		}
	}

	return true;
}

bool GSHwHack::GSC_SteambotChronicles(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		// Author: miseru99 on forums.pcsx2.net
		if (fi.TME && fi.TPSM == PSM_PSMCT16S)
		{
			if (fi.FBP == 0x1180)
			{
				skip = 1; // 1 deletes some of the glitched effects
			}
			else if (fi.FBP == 0)
			{
				skip = 100; // deletes most others(too high deletes the buggy sea completely;c, too low causes glitches to be visible)
			}
			else if (CRC_Aggressive && fi.FBP != 0)
			{
				skip = 19; // "speedhack", makes the game very light, vaporized water can disappear when not looked at directly, possibly some interface still, other value to try: 6 breaks menu background, possibly nothing(?) during gameplay, but it's slower, hence not much of a speedhack anymore
			}
		}
	}

	return true;
}

bool GSHwHack::GSC_YakuzaGames(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (!s_nativeres && !fi.TME && (fi.FBP == 0x1c20 || fi.FBP == 0x1e20 || fi.FBP == 0x1620) && (fi.TBP0 == 0xe00 || fi.TBP0 == 0x1000 || fi.TBP0 == 0x800) && fi.TPSM == PSM_PSMZ24 && fi.FPSM == PSM_PSMCT32
			/*&& fi.FBMSK == 0xffffff && fi.TZTST && !GSUtil::HasSharedBits(fi.FBP, fi.FPSM, fi.TBP0, fi.TPSM)*/)
		{
			// Don't enable hack on native res if crc is below aggressive.
			// Upscaling issues, removes glow/blur effect which fixes ghosting.
			// Skip 3 removes most of the post effect which doesn't upscale well, but causes a depth effect to completely mess up.
			// Skip 9 removes both the depth and blur effect, which seems to work okay.
			skip = 9;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Full level, correctly emulated on OpenGL/Vulkan but can be used as potential speed hack
////////////////////////////////////////////////////////////////////////////////

bool GSHwHack::GSC_GetawayGames(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if ((fi.FBP == 0 || fi.FBP == 0x1180 || fi.FBP == 0x1400) && fi.TPSM == PSM_PSMT8H && fi.FBMSK == 0)
		{
			skip = 1; // Removes fog wall.
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Aggressive only hack
////////////////////////////////////////////////////////////////////////////////

bool GSHwHack::GSC_AceCombat4(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	// Removes clouds for a good speed boost, removes both 3D clouds(invisible with Hardware renderers, but cause slowdown) and 2D background clouds.
	// Removes blur from player airplane.
	// This hack also removes rockets, shows explosions(invisible without CRC hack) as garbage data,
	// causes flickering issues with the HUD, and in some (night) missions removes the HUD altogether.

	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x02a00 && fi.FPSM == PSM_PSMZ24 && fi.TBP0 == 0x01600 && fi.TPSM == PSM_PSMZ24)
		{
			skip = 71; // clouds (z, 16-bit)
		}
	}

	return true;
}

bool GSHwHack::GSC_FFXGames(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME)
		{
			// depth textures (bully, mgs3s1 intro, Front Mission 5)
			if ((fi.TPSM == PSM_PSMZ32 || fi.TPSM == PSM_PSMZ24 || fi.TPSM == PSM_PSMZ16 || fi.TPSM == PSM_PSMZ16S) ||
				// General, often problematic post processing
				(GSUtil::HasSharedBits(fi.FBP, fi.FPSM, fi.TBP0, fi.TPSM)))
			{
				skip = 1;
			}
		}
	}

	return true;
}

bool GSHwHack::GSC_Okami(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x00e00 && fi.FPSM == PSM_PSMCT32 && fi.TBP0 == 0x00000 && fi.TPSM == PSM_PSMCT32)
		{
			skip = 1000;
		}
	}
	else
	{
		if (fi.TME && fi.FBP == 0x00e00 && fi.FPSM == PSM_PSMCT32 && fi.TBP0 == 0x03800 && fi.TPSM == PSM_PSMT4)
		{
			skip = 0;
		}
	}

	return true;
}

bool GSHwHack::GSC_RedDeadRevolver(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.FBP == 0x03700 && fi.FPSM == PSM_PSMCT32 && fi.TPSM == PSM_PSMCT24)
		{
			skip = 2; // Blur
		}
	}

	return true;
}

bool GSHwHack::GSC_ShinOnimusha(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x001000 && (fi.TBP0 == 0 || fi.TBP0 == 0x0800) && fi.TPSM == PSM_PSMT8H && fi.FBMSK == 0x00FFFFFF)
		{
			skip = 0; // Water ripple not needed ?
		}
		else if (fi.TPSM == PSM_PSMCT24 && fi.TME && fi.FBP == 0x01000) // || fi.FBP == 0x00000
		{
			skip = 28; //28 30 56 64
		}
		else if (fi.FBP && fi.TPSM == PSM_PSMT8H && fi.FBMSK == 0xFFFFFF)
		{
			skip = 0; //24 33 40 9
		}
		else if (fi.TPSM == PSM_PSMT8H && fi.FBMSK == 0xFF000000)
		{
			skip = 1; // White fog when picking up things
		}
		else if (fi.TME && (fi.TBP0 == 0x1400 || fi.TBP0 == 0x1000 || fi.TBP0 == 0x1200) && (fi.TPSM == PSM_PSMCT32 || fi.TPSM == PSM_PSMCT24))
		{
			skip = 1; // Eliminate excessive flooding, water and other light and shadow
		}
	}

	return true;
}

bool GSHwHack::GSC_XenosagaE3(GSRendererHW& r, const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TPSM == PSM_PSMT8H && fi.FBMSK >= 0xEFFFFFFF)
		{
			skip = 73; // Animation
		}
		else if (fi.TME && fi.FBP == 0x03800 && fi.TBP0 && fi.TPSM == 0 && fi.FBMSK == 0)
		{
			skip = 1; // Ghosting
		}
		else
		{
			if (fi.TME)
			{
				// depth textures (bully, mgs3s1 intro, Front Mission 5)
				if ((fi.TPSM == PSM_PSMZ32 || fi.TPSM == PSM_PSMZ24 || fi.TPSM == PSM_PSMZ16 || fi.TPSM == PSM_PSMZ16S) ||
					// General, often problematic post processing
					(GSUtil::HasSharedBits(fi.FBP, fi.FPSM, fi.TBP0, fi.TPSM)))
				{
					skip = 1;
				}
			}
		}
	}

	return true;
}

bool GSHwHack::OI_BigMuthaTruckers(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// Rendering pattern:
	// CRTC frontbuffer at 0x0 is interlaced (half vertical resolution),
	// game needs to do a depth effect (so green channel to alpha),
	// but there is a vram limitation so green is pushed into the alpha channel of the CRCT buffer,
	// vertical resolution is half so only half is processed at once
	// We, however, don't have this limitation so we'll replace the draw with a full-screen TS.

	const GIFRegTEX0& Texture = RCONTEXT->TEX0;

	GIFRegTEX0 Frame = {};
	Frame.TBW = RCONTEXT->FRAME.FBW;
	Frame.TBP0 = RCONTEXT->FRAME.Block();

	if (RPRIM->TME && Frame.TBW == 10 && Texture.TBW == 10 && Frame.TBP0 == 0x00a00 && Texture.PSM == PSM_PSMT8H && (r.m_r.y == 256 || r.m_r.y == 224))
	{
		// 224 ntsc, 256 pal.
		GL_INS("OI_BigMuthaTruckers half bottom offset");

		const size_t count = r.m_vertex.next;
		GSVertex* v = &r.m_vertex.buff[0];
		const u16 offset = (u16)r.m_r.y * 16;

		for (size_t i = 0; i < count; i++)
			v[i].V += offset;
	}

	return true;
}

bool GSHwHack::OI_DBZBTGames(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if (t && t->m_from_target) // Avoid slow framebuffer readback
		return true;

	if (!((r.m_r == GSVector4i(0, 0, 16, 16)).alltrue() || (r.m_r == GSVector4i(0, 0, 64, 64)).alltrue()))
		return true; // Only 16x16 or 64x64 draws.

	// Sprite rendering
	if (!r.CanUseSwSpriteRender())
		return true;

	r.SwSpriteRender();

	return false; // Skip current draw
}

bool GSHwHack::OI_FFXII(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	static u32* video = nullptr;
	static size_t lines = 0;

	if (lines == 0)
	{
		if (r.m_vt.m_primclass == GS_LINE_CLASS && (r.m_vertex.next == 448 * 2 || r.m_vertex.next == 512 * 2))
		{
			lines = r.m_vertex.next / 2;
		}
	}
	else
	{
		if (r.m_vt.m_primclass == GS_POINT_CLASS)
		{
			if (r.m_vertex.next >= 16 * 512)
			{
				// incoming pixels are stored in columns, one column is 16x512, total res 448x512 or 448x454

				if (!video)
					video = new u32[512 * 512];

				const GSVertex* RESTRICT v = r.m_vertex.buff;
				const int ox = RCONTEXT->XYOFFSET.OFX - 8;
				const int oy = RCONTEXT->XYOFFSET.OFY - 8;

				for (int i = (int)r.m_vertex.next; i > 0; i--, v++)
				{
					const int x = (v->XYZ.X - ox) >> 4;
					const int y = (v->XYZ.Y - oy) >> 4;

					if (x < 0 || x >= 448 || y < 0 || y >= (int)lines)
						return false; // le sigh

					video[(y << 8) + (y << 7) + (y << 6) + x] = v->RGBAQ.U32[0];
				}

				return false;
			}
			else
			{
				lines = 0;
			}
		}
		else if (r.m_vt.m_primclass == GS_LINE_CLASS)
		{
			if (r.m_vertex.next == lines * 2)
			{
				// normally, this step would copy the video onto screen with 512 texture mapped horizontal lines,
				// but we use the stored video data to create a new texture, and replace the lines with two triangles

				g_gs_device->Recycle(t->m_texture);

				t->m_texture = g_gs_device->CreateTexture(512, 512, 1, GSTexture::Format::Color);

				t->m_texture->Update(GSVector4i(0, 0, 448, lines), video, 448 * 4);

				r.m_vertex.buff[2] = r.m_vertex.buff[r.m_vertex.next - 2];
				r.m_vertex.buff[3] = r.m_vertex.buff[r.m_vertex.next - 1];

				r.m_index.buff[0] = 0;
				r.m_index.buff[1] = 1;
				r.m_index.buff[2] = 2;
				r.m_index.buff[3] = 1;
				r.m_index.buff[4] = 2;
				r.m_index.buff[5] = 3;

				r.m_vertex.head = r.m_vertex.tail = r.m_vertex.next = 4;
				r.m_index.tail = 6;

				r.m_vt.Update(r.m_vertex.buff, r.m_index.buff, r.m_vertex.tail, r.m_index.tail, GS_TRIANGLE_CLASS);
			}
			else
			{
				lines = 0;
			}
		}
	}

	return true;
}

bool GSHwHack::OI_FFX(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	const u32 FBP = RCONTEXT->FRAME.Block();
	const u32 ZBP = RCONTEXT->ZBUF.Block();
	const u32 TBP = RCONTEXT->TEX0.TBP0;

	if ((FBP == 0x00d00 || FBP == 0x00000) && ZBP == 0x02100 && RPRIM->TME && TBP == 0x01a00 && RCONTEXT->TEX0.PSM == PSM_PSMCT16S)
	{
		// random battle transition (z buffer written directly, clear it now)
		GL_INS("OI_FFX ZB clear");
		g_gs_device->ClearDepth(ds);
	}

	return true;
}

bool GSHwHack::OI_MetalSlug6(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// missing red channel fix (looks alright in pcsx2 r5000+)

	GSVertex* RESTRICT v = r.m_vertex.buff;

	for (size_t i = r.m_vertex.next; i > 0; i--, v++)
	{
		const u32 c = v->RGBAQ.U32[0];

		const u32 r = (c >> 0) & 0xff;
		const u32 g = (c >> 8) & 0xff;
		const u32 b = (c >> 16) & 0xff;

		if (r == 0 && g != 0 && b != 0)
		{
			v->RGBAQ.U32[0] = (c & 0xffffff00) | ((g + b + 1) >> 1);
		}
	}

	r.m_vt.Update(r.m_vertex.buff, r.m_index.buff, r.m_vertex.tail, r.m_index.tail, r.m_vt.m_primclass);

	return true;
}

bool GSHwHack::OI_RozenMaidenGebetGarden(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if (!RPRIM->TME)
	{
		const u32 FBP = RCONTEXT->FRAME.Block();
		const u32 ZBP = RCONTEXT->ZBUF.Block();

		if (FBP == 0x008c0 && ZBP == 0x01a40)
		{
			//  frame buffer clear, atst = fail, afail = write z only, z buffer points to frame buffer

			GIFRegTEX0 TEX0 = {};

			TEX0.TBP0 = ZBP;
			TEX0.TBW = RCONTEXT->FRAME.FBW;
			TEX0.PSM = RCONTEXT->FRAME.PSM;

			if (GSTextureCache::Target* tmp_rt = r.m_tc->LookupTarget(TEX0, r.GetTargetSize(), GSTextureCache::RenderTarget, true))
			{
				GL_INS("OI_RozenMaidenGebetGarden FB clear");
				g_gs_device->ClearRenderTarget(tmp_rt->m_texture, 0);
			}

			return false;
		}
		else if (FBP == 0x00000 && RCONTEXT->ZBUF.Block() == 0x01180)
		{
			// z buffer clear, frame buffer now points to the z buffer (how can they be so clever?)

			GIFRegTEX0 TEX0 = {};

			TEX0.TBP0 = FBP;
			TEX0.TBW = RCONTEXT->FRAME.FBW;
			TEX0.PSM = RCONTEXT->ZBUF.PSM;

			if (GSTextureCache::Target* tmp_ds = r.m_tc->LookupTarget(TEX0, r.GetTargetSize(), GSTextureCache::DepthStencil, true))
			{
				GL_INS("OI_RozenMaidenGebetGarden ZB clear");
				g_gs_device->ClearDepth(tmp_ds->m_texture);
			}

			return false;
		}
	}

	return true;
}

bool GSHwHack::OI_SonicUnleashed(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// Rendering pattern is:
	// Save RG channel with a kind of a TS (replaced by a copy in this hack),
	// compute shadow in RG,
	// save result in alpha with a TS,
	// Restore RG channel that we previously copied to render shadows.

	const GIFRegTEX0& Texture = RCONTEXT->TEX0;

	GIFRegTEX0 Frame = {};
	Frame.TBW = RCONTEXT->FRAME.FBW;
	Frame.TBP0 = RCONTEXT->FRAME.Block();
	Frame.PSM = RCONTEXT->FRAME.PSM;

	if ((!RPRIM->TME) || (GSLocalMemory::m_psm[Texture.PSM].bpp != 16) || (GSLocalMemory::m_psm[Frame.PSM].bpp != 16) || (Texture.TBP0 == Frame.TBP0) || (Frame.TBW != 16 && Texture.TBW != 16))
		return true;

	GL_INS("OI_SonicUnleashed replace draw by a copy");

	GSTextureCache::Target* src = r.m_tc->LookupTarget(Texture, GSVector2i(1, 1), GSTextureCache::RenderTarget, true);

	const GSVector2i rt_size(rt->GetSize());
	const GSVector2i src_size(src->m_texture->GetSize());
	const GSVector2i copy_size(std::min(rt_size.x, src_size.x), std::min(rt_size.y, src_size.y));

	const GSVector4 sRect(0.0f, 0.0f, static_cast<float>(copy_size.x) / static_cast<float>(src_size.x), static_cast<float>(copy_size.y) / static_cast<float>(src_size.y));
	const GSVector4 dRect(0, 0, copy_size.x, copy_size.y);

	g_gs_device->StretchRect(src->m_texture, sRect, rt, dRect, true, true, true, false);

	return false;
}


bool GSHwHack::OI_ArTonelico2(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// world map clipping
	//
	// The bad draw call is a sprite rendering to clear the z buffer

	/*
	   Depth buffer description
	   * width is 10 pages
	   * texture/scissor size is 640x448
	   * depth is 16 bits so it writes 70 (10w * 7h) pages of data.

	   following draw calls will use the buffer as 6 pages width with a scissor
	   test of 384x672. So the above texture can be seen as a

	   * texture width: 6 pages * 64 pixels/page = 384
	   * texture height: 70/6 pages * 64 pixels/page =746

	   So as you can see the GS issue a write of 640x448 but actually it
	   expects to clean a 384x746 area. Ideally the fix will transform the
	   buffer to adapt the page width properly.
	 */

	const GSVertex* v = &r.m_vertex.buff[0];

	if (r.m_vertex.next == 2 && !RPRIM->TME && RCONTEXT->FRAME.FBW == 10 && v->XYZ.Z == 0 && RCONTEXT->TEST.ZTST == ZTST_ALWAYS)
	{
		GL_INS("OI_ArTonelico2");
		g_gs_device->ClearDepth(ds);
	}

	return true;
}

bool GSHwHack::OI_JakGames(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if (!(r.m_r == GSVector4i(0, 0, 16, 16)).alltrue())
		return true; // Only 16x16 draws.

	if (!r.CanUseSwSpriteRender())
		return true;

	// Render 16x16 palette via CPU.
	r.SwSpriteRender();

	return false; // Skip current draw.
}

bool GSHwHack::OI_BurnoutGames(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if (!r.OI_PointListPalette(r, rt, ds, t))
		return false; // Render point list palette.

	if (t && t->m_from_target) // Avoid slow framebuffer readback
		return true;

	if (!r.CanUseSwSpriteRender())
		return true;

	// Render palette via CPU.
	r.SwSpriteRender();

	return false;
}

// OO (others output?) hacks: invalidate extra local memory after the draw call

void GSHwHack::OO_BurnoutGames(GSRendererHW& r)
{
	const GIFRegTEX0& TEX0 = RCONTEXT->TEX0;
	const GIFRegALPHA& ALPHA = RCONTEXT->ALPHA;
	const GIFRegFRAME& FRAME = RCONTEXT->FRAME;
	if (RPRIM->PRIM == GS_SPRITE && !RPRIM->IIP && RPRIM->TME && !RPRIM->FGE && RPRIM->ABE && !RPRIM->AA1 && !RPRIM->FST && !RPRIM->FIX && TEX0.TBW == 16 && TEX0.TW == 10 && TEX0.TCC && !TEX0.TFX && TEX0.PSM == PSM_PSMT8 && TEX0.CPSM == PSM_PSMCT32 && !TEX0.CSM && TEX0.TH == 8 && ALPHA.A == ALPHA.B && ALPHA.D == 0 && FRAME.FBW == 16 && FRAME.PSM == PSM_PSMCT32)
	{
		// Readback clouds being rendered during level loading.
		// Later the alpha channel from the 32 bit frame buffer is used as an 8 bit indexed texture to draw
		// the clouds on top of the sky at each frame.
		// Burnout 3 PAL 50Hz: 0x3ba0 => 0x1e80.
		GL_INS("OO_BurnoutGames - Readback clouds renderered from TEX0.TBP0 = 0x%04x (TEX0.CBP = 0x%04x) to FBP = 0x%04x", TEX0.TBP0, TEX0.CBP, FRAME.Block());
		r.m_tc->InvalidateLocalMem(RCONTEXT->offset.fb, r.m_r);
	}
}

#undef RCONTEXT
#undef RPRIM

#undef CRC_Partial
#undef CRC_Full
#undef CRC_Aggressive

////////////////////////////////////////////////////////////////////////////////

const GSHwHack::Entry<GSRendererHW::GSC_Ptr> GSHwHack::s_gsc_functions[] = {
	{CRC::GodHand, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_GodHand},
	{CRC::KnightsOfTheTemple2, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_KnightsOfTheTemple2},
	{CRC::Kunoichi, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_Kunoichi},
	{CRC::Manhunt2, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_Manhunt2},
	{CRC::MidnightClub3, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_MidnightClub3},
	{CRC::SacredBlaze, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_SacredBlaze},
	{CRC::SakuraTaisen, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_SakuraTaisen},
	{CRC::SakuraWarsSoLongMyLove, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_SakuraWarsSoLongMyLove},
	{CRC::Simple2000Vol114, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_Simple2000Vol114},
	{CRC::SFEX3, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_SFEX3},
	{CRC::TalesOfLegendia, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_TalesOfLegendia},
	{CRC::TalesofSymphonia, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_TalesofSymphonia},
	{CRC::TombRaiderAnniversary, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_TombRaiderAnniversary},
	{CRC::TombRaiderLegend, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_TombRaiderLegend},
	{CRC::TombRaiderUnderworld, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_TombRaiderUnderWorld},
	{CRC::UrbanReign, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_UrbanReign},
	{CRC::ZettaiZetsumeiToshi2, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_ZettaiZetsumeiToshi2},

	// Channel Effect
	{CRC::CrashBandicootWoC, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_CrashBandicootWoC},
	{CRC::GiTS, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_GiTS},
	{CRC::Spartan, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_Spartan},
	{CRC::SteambotChronicles, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_SteambotChronicles},

	// Depth Issue
	{CRC::BurnoutGames, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_BurnoutGames},

	// Half Screen bottom issue
	{CRC::Tekken5, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_Tekken5},

	// Texture shuffle
	{CRC::BigMuthaTruckers, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_BigMuthaTruckers},
	{CRC::DeathByDegreesTekkenNinaWilliams, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_DeathByDegreesTekkenNinaWilliams}, // + Upscaling issues

	// Upscaling hacks
	{CRC::FightingBeautyWulong, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_FightingBeautyWulong},
	{CRC::Oneechanbara2Special, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_Oneechanbara2Special},
	{CRC::UltramanFightingEvolution, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_UltramanFightingEvolution},
	{CRC::YakuzaGames, CRC::RegionCount, CRCHackLevel::Partial, &GSHwHack::GSC_YakuzaGames},

	// Accurate Blending
	{CRC::GetawayGames, CRC::RegionCount, CRCHackLevel::Full, &GSHwHack::GSC_GetawayGames}, // Blending High

	{CRC::AceCombat4, CRC::RegionCount, CRCHackLevel::Aggressive, &GSHwHack::GSC_AceCombat4},
	{CRC::FFX2, CRC::RegionCount, CRCHackLevel::Aggressive, &GSHwHack::GSC_FFXGames},
	{CRC::FFX, CRC::RegionCount, CRCHackLevel::Aggressive, &GSHwHack::GSC_FFXGames},
	{CRC::FFXII, CRC::RegionCount, CRCHackLevel::Aggressive, &GSHwHack::GSC_FFXGames},
	{CRC::RedDeadRevolver, CRC::RegionCount, CRCHackLevel::Aggressive, &GSHwHack::GSC_RedDeadRevolver},
	{CRC::ShinOnimusha, CRC::RegionCount, CRCHackLevel::Aggressive, &GSHwHack::GSC_ShinOnimusha},
	{CRC::XenosagaE3, CRC::RegionCount, CRCHackLevel::Aggressive, &GSHwHack::GSC_XenosagaE3},

	// Upscaling issues
	{CRC::Okami, CRC::RegionCount, CRCHackLevel::Aggressive, &GSHwHack::GSC_Okami},
};

const GSHwHack::Entry<GSRendererHW::OI_Ptr> GSHwHack::s_oi_functions[] = {
	{CRC::BigMuthaTruckers, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_BigMuthaTruckers},
	{CRC::DBZBT2, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_DBZBTGames},
	{CRC::DBZBT3, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_DBZBTGames},
	{CRC::FFXII, CRC::EU, CRCHackLevel::Minimum, &GSHwHack::OI_FFXII},
	{CRC::FFX, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_FFX},
	{CRC::MetalSlug6, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_MetalSlug6},
	{CRC::RozenMaidenGebetGarden, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_RozenMaidenGebetGarden},
	{CRC::SonicUnleashed, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_SonicUnleashed},
	{CRC::ArTonelico2, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_ArTonelico2},
	{CRC::Jak2, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_JakGames},
	{CRC::Jak3, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_JakGames},
	{CRC::JakX, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_JakGames},
	{CRC::BurnoutGames, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_BurnoutGames},
	{CRC::Black, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OI_BurnoutGames}};

const GSHwHack::Entry<GSRendererHW::OO_Ptr> GSHwHack::s_oo_functions[] = {
	{CRC::BurnoutGames, CRC::RegionCount, CRCHackLevel::Minimum, &GSHwHack::OO_BurnoutGames},
};

void GSRendererHW::SetupCrcHack(CRCHackLevel level)
{
	s_nativeres = m_nativeres;
	s_crc_hack_level = level;

	m_gsc = nullptr;
	if (level != CRCHackLevel::Off)
	{
		for (const auto& entry : GSHwHack::s_gsc_functions)
		{
			if (entry.Test(m_game.title, m_game.region, level))
			{
				m_gsc = entry.ptr;
				break;
			}
		}
	}

	m_oi = nullptr;
	if (level != CRCHackLevel::Off)
	{
		for (const auto& entry : GSHwHack::s_oi_functions)
		{
			if (entry.Test(m_game.title, m_game.region, level))
			{
				m_oi = entry.ptr;
				break;
			}
		}
	}
	if (GSConfig.PointListPalette)
	{
		if (m_oi)
			Console.Warning("Overriding m_oi with PointListPalette");

		m_oi = &GSRendererHW::OI_PointListPalette;
	}

	m_oo = nullptr;
	if (level != CRCHackLevel::Off)
	{
		for (const auto& entry : GSHwHack::s_oo_functions)
		{
			if (entry.Test(m_game.title, m_game.region, level))
			{
				m_oo = entry.ptr;
				break;
			}
		}
	}
}

bool GSRendererHW::IsBadFrame()
{
	if (m_gsc)
	{
		const GSFrameInfo fi = {
			m_context->FRAME.Block(),
			m_context->FRAME.PSM,
			m_context->FRAME.FBMSK,
			m_context->ZBUF.Block(),
			m_context->ZBUF.ZMSK,
			m_context->TEST.ZTST,
			PRIM->TME,
			m_context->TEX0.TBP0,
			m_context->TEX0.PSM,
		};

		if (!m_gsc(*this, fi, m_skip))
			return false;
	}

	if (m_skip == 0 && GSConfig.SkipDrawEnd > 0)
	{
		if (PRIM->TME)
		{
			// depth textures (bully, mgs3s1 intro, Front Mission 5)
			// General, often problematic post processing
			if (GSLocalMemory::m_psm[m_context->TEX0.PSM].depth ||
				GSUtil::HasSharedBits(m_context->FRAME.Block(), m_context->FRAME.PSM, m_context->TEX0.TBP0, m_context->TEX0.PSM))
			{
				m_skip_offset = GSConfig.SkipDrawStart;
				m_skip = GSConfig.SkipDrawEnd;
			}
		}
	}

	if (m_skip > 0)
	{
		m_skip--;

		if (m_skip_offset > 1)
			m_skip_offset--;
		else
			return true;
	}

	return false;
}
