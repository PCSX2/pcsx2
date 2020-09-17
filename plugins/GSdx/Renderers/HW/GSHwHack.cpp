/*
 *	Copyright (C) 2007-2016 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSState.h"
#include "GSdx.h"

bool s_nativeres;
static CRCHackLevel s_crc_hack_level = CRCHackLevel::Full;

// hacks
#define Dx_and_OGL (s_crc_hack_level >= CRCHackLevel::Partial)
#define Dx_only (s_crc_hack_level >= CRCHackLevel::Full)
#define Aggressive (s_crc_hack_level >= CRCHackLevel::Aggressive)

CRC::Region g_crc_region = CRC::NoRegion;

////////////////////////////////////////////////////////////////////////////////
// Broken on both DirectX and OpenGL
// (note: could potentially work with latest OpenGL)
////////////////////////////////////////////////////////////////////////////////

bool GSC_BigMuthaTruckers(const GSFrameInfo& fi, int& skip)
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

bool GSC_Bully(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if ((Aggressive || !s_nativeres) && !fi.TME && (fi.FBP == 0x02300 || fi.FBP == 0x02800) && fi.FPSM == PSM_PSMCT24)
		{
			// ntsc 0x02300, pal 0x02800
			// Don't enable hack on native res if crc is below aggressive.
			// Previous value 6, ntsc didn't like it.
			skip = 8; // Upscaling blur/ghosting
		}
	}

	return true;
}

bool GSC_DBZBT3(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if ((Aggressive || !s_nativeres) && fi.TME && (fi.FBP == 0x03400 || fi.FBP == 0x02e00) && fi.FPSM == fi.TPSM && fi.TBP0 == 0x03f00 && fi.TPSM == PSM_PSMCT32)
		{
			// Ghosting/Blur effect. Upscaling issue.
			// Can be fixed with TC X,Y offsets.
			// Don't enable hack on native res if crc is below aggressive.
			skip = 3;
		}
	}

	return true;
}

bool GSC_DeathByDegreesTekkenNinaWilliams(const GSFrameInfo& fi, int& skip)
{
	// Note: Game also has issues with texture shuffle not supported on strange clamp mode.
	// See https://forums.pcsx2.net/Thread-GSDX-Texture-Cache-Bug-Report-Death-By-Degrees-SLUS-20934-NTSC
	if (skip == 0)
	{
		if ((Aggressive || !s_nativeres) && fi.TME && fi.FBP == 0 && fi.TBP0 == 0x34a0 && fi.TPSM == PSM_PSMCT32)
		{
			// Don't enable hack on native res if crc is below aggressive.
			// Upscaling issue similar to Tekken 5.
			skip = 1; // Animation pane
		}
		else if (Aggressive && fi.FBP == 0x3500 && fi.TPSM == PSM_PSMT8 && fi.FBMSK == 0xFFFF00FF)
		{
			// Needs to be further tested so put it on Aggressive for now, likely channel shuffle.
			skip = 4; // Underwater white fog
		}
	}
	else
	{
		if ((Aggressive || !s_nativeres) && fi.TME && (fi.FBP | fi.TBP0 | fi.FPSM | fi.TPSM) && fi.FBMSK == 0x00FFFFFF)
		{
			// Needs to be further tested so assume it's related with the upscaling hack.
			skip = 1; // Animation speed
		}
	}

	return true;
}

bool GSC_GiTS(const GSFrameInfo& fi, int& skip)
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

bool GSC_GodOfWar2(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (Aggressive && fi.TME && fi.FPSM == PSM_PSMCT16 && (fi.FBP == 0x00100 || fi.FBP == 0x02100) && (fi.TBP0 == 0x00100 || fi.TBP0 == 0x02100) && fi.TPSM == PSM_PSMCT16)
		{
			// Can be used as a speed hack.
			// Removes shadows.
			skip = 1000;
		}
		else if (Aggressive && fi.TME && fi.TPSM == PSM_PSMCT24 && fi.FBP == 0x1300 && (fi.TBP0 == 0x0F00 || fi.TBP0 == 0x1300 || fi.TBP0 == 0x2b00)) // || fi.FBP == 0x0100
		{
			// Upscaling hack maybe ? Needs to be verified, move it to Aggressive state just in case.
			skip = 1; // global haze/halo
		}
		else if ((Aggressive || !s_nativeres) && fi.TME && fi.TPSM == PSM_PSMCT24 && (fi.FBP == 0x0100 || fi.FBP == 0x2100) && (fi.TBP0 == 0x2b00 || fi.TBP0 == 0x2e80 || fi.TBP0 == 0x3100)) // 480P 2e80, interlaced 3100
		{
			// Upscaling issue.
			// Don't enable hack on native res if crc is below aggressive.
			skip = 1; // water effect and water vertical lines
		}
	}
	else
	{
		if (Aggressive && fi.TME && (fi.FBP == 0x00100 || fi.FBP == 0x02100) && fi.FPSM == PSM_PSMCT16)
		{
			skip = 3;
		}
	}

	return true;
}

bool GSC_WildArmsGames(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x03100 && fi.FPSM == PSM_PSMZ32 && fi.TBP0 == 0x01c00 && fi.TPSM == PSM_PSMZ32)
		{
			skip = 100;
		}
	}
	else
	{
		if (fi.TME && fi.FBP == 0x00e00 && fi.FPSM == PSM_PSMCT32 && fi.TBP0 == 0x02a00 && fi.TPSM == PSM_PSMCT32)
		{
			skip = 1;
		}
	}

	return true;
}

// Channel effect not properly supported yet
bool GSC_Manhunt2(const GSFrameInfo& fi, int& skip)
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

bool GSC_CrashBandicootWoC(const GSFrameInfo& fi, int& skip)
{
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

bool GSC_SacredBlaze(const GSFrameInfo& fi, int& skip)
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

bool GSC_Spartan(const GSFrameInfo& fi, int& skip)
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

bool GSC_IkkiTousen(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if ((Aggressive || !s_nativeres) && fi.TME && fi.FBP == 0x00700 && fi.FPSM == PSM_PSMZ24 && fi.TBP0 == 0x01180 && fi.TPSM == PSM_PSMZ24)
		{
			// Might not be needed if any of the upscaling hacks fix the issues, needs to be further tested.
			// Don't enable hack on native res if crc is below aggressive.
			skip = 11; // Upscaling blur/ghosting
		}
	}

	return true;
}

bool GSC_EvangelionJo(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if ((Aggressive || !s_nativeres) && fi.TME && fi.TBP0 == 0x2BC0 || (fi.FBP == 0 || fi.FBP == 0x1180) && (fi.FPSM | fi.TPSM) == 0)
		{
			// Don't enable hack on native res if crc is below aggressive.
			// Removes blur/glow. Fixes ghosting when resolution is upscaled.
			skip = 1;
		}
	}

	return true;
}

bool GSC_Oneechanbara2Special(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if ((Aggressive || !s_nativeres) && fi.TPSM == PSM_PSMCT24 && fi.TME && fi.FBP == 0x01180)
		{
			// Don't enable hack on native res if crc is below aggressive.
			// Ghosting upscaling issue, bottom and right red lines also by upscaling.
			skip = 1;
		}
	}

	return true;
}

bool GSC_SakuraTaisen(const GSFrameInfo& fi, int& skip)
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

bool GSC_ShadowofRome(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.FBP && fi.TPSM == PSM_PSMT8H && (fi.FBMSK == 0x00FFFFFF))
		{
			// Depth issues on all renders, white wall and white duplicate characters.
			skip = 1;
		}
		else if (fi.TME == 0x0001 && (fi.TBP0 == 0x1300 || fi.TBP0 == 0x0f00) && fi.FBMSK >= 0xFFFFFF)
		{
			// Cause a grey transparent wall (D3D) and a transparent vertical grey line (all renders) on the left side of the screen.
			// Blur effect maybe ?
			skip = 1;
		}
		else if (fi.TME && fi.FPSM == PSM_PSMCT32 && (fi.TBP0 == 0x0160 || fi.TBP0 == 0x01e0 || fi.TBP0 <= 0x0800) && fi.TPSM == PSM_PSMT8)
		{
			skip = 1; // Speedhack ?
		}
	}

	return true;
}

bool GSC_SFEX3(const GSFrameInfo& fi, int& skip)
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

bool GSC_LordOfTheRingsThirdAge(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (!fi.TME && fi.FBP == 0x03000 && fi.FPSM == PSM_PSMCT32 && fi.TPSM == PSM_PSMT4 && fi.FBMSK == 0xFF000000)
		{
			skip = 1000; //shadows
		}
	}
	else
	{
		if (fi.TME && (fi.FBP == 0x0 || fi.FBP == 0x00e00 || fi.FBP == 0x01000) && fi.FPSM == PSM_PSMCT32 && fi.TBP0 == 0x03000 && fi.TPSM == PSM_PSMCT24)
		{
			skip = 1;
		}
	}

	return true;
}

bool GSC_Tekken5(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if ((Aggressive || !s_nativeres) && fi.TME && (fi.FBP == 0x02d60 || fi.FBP == 0x02d80 || fi.FBP == 0x02ea0 || fi.FBP == 0x03620 || fi.FBP == 0x03640) && fi.FPSM == fi.TPSM && fi.TBP0 == 0x00000 && fi.TPSM == PSM_PSMCT32)
		{
			// Don't enable hack on native res if crc is below aggressive.
			// Fixes/removes ghosting/blur effect and white lines appearing in stages: Moonfit Wilderness, Acid Rain - caused by upscaling.
			// Downside is it also removes the channel effect which is fixed on OpenGL.
			// Let's enable this hack for Aggressive only since it's an upscaling issue for both renders.
			skip = 95;
		}
		else if (fi.TME && (fi.FBP == 0x02bc0 || fi.FBP == 0x02be0 || fi.FBP == 0x02d00 || fi.FBP == 0x03480 || fi.FBP == 0x034a0) && fi.FPSM == fi.TPSM && fi.TBP0 == 0x00000 && fi.TPSM == PSM_PSMCT32)
		{
			// The moving display effect(flames) is not emulated properly in the entire screen so let's remove the effect in the stage: Burning Temple. Related to half screen bottom issue.
			// Fixes black lines in the stage: Burning Temple - caused by upscaling. Note the black lines can also be fixed with Merge Sprite hack.
			skip = 2;
		}
	}

	return true;
}

bool GSC_TombRaiderAnniversary(const GSFrameInfo& fi, int& skip)
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

bool GSC_TombRaiderLegend(const GSFrameInfo& fi, int& skip)
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

bool GSC_TombRaiderUnderWorld(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x01000 && fi.FPSM == fi.TPSM && fi.TPSM == PSM_PSMCT32 && (fi.TBP0 == 0x2B60 /*|| fi.TBP0 == 0x2EFF || fi.TBP0 ==0x2F00 || fi.TBP0 == 0x3020*/ || fi.TBP0 >= 0x2C01 && fi.TBP0 != 0x3029 && fi.TBP0 != 0x302d))
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

bool GSC_BurnoutGames(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && (fi.FBP == 0x01dc0 || fi.FBP == 0x01c00 || fi.FBP == 0x01f00 || fi.FBP == 0x01d40 || fi.FBP == 0x02200 || fi.FBP == 0x02000) && fi.FPSM == fi.TPSM && (fi.TBP0 == 0x01dc0 || fi.TBP0 == 0x01c00 || fi.TBP0 == 0x01f00 || fi.TBP0 == 0x01d40 || fi.TBP0 == 0x02200 || fi.TBP0 == 0x02000) && fi.TPSM == PSM_PSMCT32)
		{
			// 0x01dc0 01c00(MP) ntsc, 0x01f00 0x01d40(MP) ntsc progressive, 0x02200(MP) pal.
			// Yellow stripes.
			// Multiplayer tested only on Takedown.
			skip = 4;
		}
	}

	return true;
}

bool GSC_MidnightClub3(const GSFrameInfo& fi, int& skip)
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

bool GSC_TalesOfLegendia(const GSFrameInfo& fi, int& skip)
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

bool GSC_Kunoichi(const GSFrameInfo& fi, int& skip)
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
		if (Aggressive && fi.TME)
		{
			// depth textures (bully, mgs3s1 intro, Front Mission 5)
			if ((fi.TPSM == PSM_PSMZ32 || fi.TPSM == PSM_PSMZ24 || fi.TPSM == PSM_PSMZ16 || fi.TPSM == PSM_PSMZ16S) ||
				// General, often problematic post processing
				(GSUtil::HasSharedBits(fi.FBP, fi.FPSM, fi.TBP0, fi.TPSM)))
			{
				// Removes burning air effect, the effect causes major slowdowns.
				skip = 1;
			}
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

bool GSC_ZettaiZetsumeiToshi2(const GSFrameInfo& fi, int& skip)
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
			// Note start of the effect (texture shuffle) is fixed in openGL but maybe not the extra draw
			// call....
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

bool GSC_SakuraWarsSoLongMyLove(const GSFrameInfo& fi, int& skip)
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

bool GSC_FightingBeautyWulong(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if ((Aggressive || !s_nativeres) && fi.TME && (fi.TBP0 == 0x0700 || fi.TBP0 == 0x0a80) && (fi.TPSM == PSM_PSMCT32 || fi.TPSM == PSM_PSMCT24))
		{
			// Don't enable hack on native res if crc is below aggressive.
			// removes glow/blur which cause ghosting and other sprite issues similar to Tekken 5
			skip = 1;
		}
	}

	return true;
}

bool GSC_GodHand(const GSFrameInfo& fi, int& skip)
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

bool GSC_KnightsOfTheTemple2(const GSFrameInfo& fi, int& skip)
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

bool GSC_UltramanFightingEvolution(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if ((Aggressive || !s_nativeres) && fi.TME && fi.FBP == 0x2a00 && fi.FPSM == PSM_PSMZ24 && fi.TBP0 == 0x1c00 && fi.TPSM == PSM_PSMZ24)
		{
			// Don't enable hack on native res if crc is below aggressive.
			skip = 5; // blur
		}
	}

	return true;
}

bool GSC_TalesofSymphonia(const GSFrameInfo& fi, int& skip)
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

bool GSC_Simple2000Vol114(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if ((Aggressive || !s_nativeres) && fi.TME == 0 && (fi.FBP == 0x1500) && (fi.TBP0 == 0x2c97 || fi.TBP0 == 0x2ace || fi.TBP0 == 0x03d0 || fi.TBP0 == 0x2448) && (fi.FBMSK == 0x0000))
		{
			// Don't enable hack on native res if crc is below aggressive.
			// Upscaling issues, removes glow/blur effect which fixes ghosting.
			skip = 1;
		}
		if (fi.TME && (fi.FBP == 0x0e00) && (fi.TBP0 == 0x1000) && (fi.FBMSK == 0x0000))
		{
			// Depth shadows, they don't work properly on OpenGL as well.
			skip = 1;
		}
	}

	return true;
}

bool GSC_UrbanReign(const GSFrameInfo& fi, int& skip)
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

bool GSC_SkyGunner(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (!fi.TME && !(fi.FBP == 0x0 || fi.FBP == 0x00800 || fi.FBP == 0x008c0 || fi.FBP == 0x03e00) && fi.FPSM == PSM_PSMCT32 && (fi.TBP0 == 0x0 || fi.TBP0 == 0x01800) && fi.TPSM == PSM_PSMCT32)
		{
			skip = 1; // Huge Vram usage
		}
	}

	return true;
}

bool GSC_SteambotChronicles(const GSFrameInfo& fi, int& skip)
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
			else if (Aggressive && fi.FBP != 0) // Aggressive CRC
			{
				skip = 19; // "speedhack", makes the game very light, vaporized water can disappear when not looked at directly, possibly some interface still, other value to try: 6 breaks menu background, possibly nothing(?) during gameplay, but it's slower, hence not much of a speedhack anymore
			}
		}
	}

	return true;
}

bool GSC_YakuzaGames(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if ((Aggressive || !s_nativeres) && !fi.TME && (fi.FBP == 0x1c20 || fi.FBP == 0x1e20 || fi.FBP == 0x1620) && (fi.TBP0 == 0xe00 || fi.TBP0 == 0x1000 || fi.TBP0 == 0x800) && fi.TPSM == PSM_PSMZ24 && fi.FPSM == PSM_PSMCT32
			/*&& fi.FBMSK == 0xffffff && fi.TZTST && !GSUtil::HasSharedBits(fi.FBP, fi.FPSM, fi.TBP0, fi.TPSM)*/)
		{
			// Don't enable hack on native res if crc is below aggressive.
			// Upscaling issues, removes glow/blur effect which fixes ghosting.
			skip = 3;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Correctly emulated on OpenGL but can be used as potential speed hack
////////////////////////////////////////////////////////////////////////////////

bool GSC_GetaWayGames(const GSFrameInfo& fi, int& skip)
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

bool GSC_StarOcean3(const GSFrameInfo& fi, int& skip)
{
	// The game emulate a stencil buffer with the alpha channel of the RT
	// The operation of the stencil is selected with the palette
	// For example -1 wrap will be [240, 16, 32, 48 ....]
	// i.e. p[A>>4] = (A - 16) % 256
	//
	// The fastest and accurate solution will be to replace this pseudo stencil
	// by a dedicated GPU draw call
	// 1/ Use future GPU capabilities to do a "kind" of SW blending
	// 2/ Use a real stencil/atomic image, and then compute the RT alpha value
	//
	// Both of those solutions will increase code complexity (and only avoid upscaling
	// glitches)

	if (skip == 0)
	{
		if (fi.TME && fi.FBP == fi.TBP0 && fi.FPSM == PSM_PSMCT32 && fi.TPSM == PSM_PSMT4HH)
		{
			skip = 1000; //
		}
	}
	else
	{
		if (!(fi.TME && fi.FBP == fi.TBP0 && fi.FPSM == PSM_PSMCT32 && fi.TPSM == PSM_PSMT4HH))
		{
			skip = 0;
		}
	}

	return true;
}

bool GSC_ValkyrieProfile2(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == fi.TBP0 && fi.FPSM == PSM_PSMCT32 && fi.TPSM == PSM_PSMT4HH)
		{
			// GH: Hack is quite similar to GSC_StarOcean3. It is potentially the same issue.
			skip = 1000; //
		}
	}
	else
	{
		if (!(fi.TME && fi.FBP == fi.TBP0 && fi.FPSM == PSM_PSMCT32 && fi.TPSM == PSM_PSMT4HH))
		{
			skip = 0;
		}
	}

	return true;
}

bool GSC_RadiataStories(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == fi.TBP0 && fi.FPSM == PSM_PSMCT32 && fi.TPSM == PSM_PSMT4HH)
		{
			// GH: Hack is quite similar to GSC_StarOcean3. It is potentially the same issue.
			skip = 1000;
		}
	}
	else
	{
		if (!(fi.TME && fi.FBP == fi.TBP0 && fi.FPSM == PSM_PSMCT32 && fi.TPSM == PSM_PSMT4HH))
		{
			skip = 0;
		}
	}

	return true;
}

bool GSC_TenchuGames(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.TPSM == PSM_PSMZ16 && fi.FPSM == PSM_PSMCT16 && fi.FBMSK == 0x03FFF)
		{
			// Depth is fine, blending issues remain, crc hack can be adjusted to skip blend wall/fog only.
			skip = 3;
		}
	}

	return true;
}

bool GSC_SlyGames(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FPSM == fi.TPSM && (fi.FBP == 0x00000 || fi.FBP == 0x00700 || fi.FBP == 0x00800 || fi.FBP == 0x008c0 || fi.FBP == 0x00a80 || fi.FBP == 0x00e00) && fi.TPSM == PSM_PSMCT16 && fi.FBMSK == 0x03FFF)
		// 0x00a80, 0x00e00 from Sly 3
		{
			// Upscaling issue with texture shuffle on dx and gl. Also removes shadows on gl.
			skip = 1000;
		}
	}
	else
	{
		if (fi.TME && fi.FPSM == fi.TPSM && fi.TPSM == PSM_PSMCT16 && fi.FBMSK == 0x03FFF)
		{
			skip = 3;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Aggressive only hack
////////////////////////////////////////////////////////////////////////////////

bool GSC_AceCombat4(const GSFrameInfo& fi, int& skip)
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

bool GSC_BleachBladeBattlers(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x01180 && fi.FPSM == fi.TPSM && fi.TBP0 == 0x03fc0 && fi.TPSM == PSM_PSMCT32)
		{
			// Removes body shading. Not needed but offers a very decent speed boost.
			skip = 1;
		}
	}

	return true;
}

bool GSC_GodOfWar(const GSFrameInfo& fi, int& skip)
{
	if (skip == 0)
	{
		if (fi.TME && fi.FBP == 0x00000 && fi.FPSM == PSM_PSMCT16 && fi.TBP0 == 0x00000 && fi.TPSM == PSM_PSMCT16 && fi.FBMSK == 0x03FFF)
		{
			// Can be used as a speed hack.
			// Removes shadows.
			skip = 1000;
		}
		else if (fi.TME && fi.FBP == 0x00000 && fi.FPSM == PSM_PSMCT32 && fi.TBP0 == 0x00000 && fi.TPSM == PSM_PSMCT32 && fi.FBMSK == 0xff000000)
		{
			// Upscaling hack maybe ? Needs to be verified, move it to Aggressive state just in case.
			skip = 1; // blur
		}
	}
	else
	{
		if (fi.TME && fi.FBP == 0x00000 && fi.FPSM == PSM_PSMCT16)
		{
			skip = 3;
		}
	}

	return true;
}

bool GSC_SoTC(const GSFrameInfo& fi, int& skip)
{
	// Not needed anymore? What did it fix anyway? (rama)
	if (skip == 0)
	{
		if (fi.TME /*&& fi.FBP == 0x03d80*/ && fi.FPSM == 0 && fi.TBP0 == 0x03fc0 && fi.TPSM == 1)
		{
			skip = 48; // Removes sky bloom
		}
	}

	return true;
}

bool GSC_FFXGames(const GSFrameInfo& fi, int& skip)
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

bool GSC_Okami(const GSFrameInfo& fi, int& skip)
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

bool GSC_RedDeadRevolver(const GSFrameInfo& fi, int& skip)
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

bool GSC_ShinOnimusha(const GSFrameInfo& fi, int& skip)
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

bool GSC_XenosagaE3(const GSFrameInfo& fi, int& skip)
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

////////////////////////////////////////////////////////////////////////////////

void GSState::SetupCrcHack()
{
	GetSkipCount lut[CRC::TitleCount];

	s_nativeres = m_nativeres;
	s_crc_hack_level = m_crc_hack_level;

	memset(lut, 0, sizeof(lut));

	if (Dx_and_OGL)
	{
		lut[CRC::CrashBandicootWoC] = GSC_CrashBandicootWoC;
		lut[CRC::GodHand] = GSC_GodHand;
		lut[CRC::KnightsOfTheTemple2] = GSC_KnightsOfTheTemple2;
		lut[CRC::Kunoichi] = GSC_Kunoichi;
		lut[CRC::Manhunt2] = GSC_Manhunt2;
		lut[CRC::MidnightClub3] = GSC_MidnightClub3;
		lut[CRC::SacredBlaze] = GSC_SacredBlaze;
		lut[CRC::SakuraTaisen] = GSC_SakuraTaisen;
		lut[CRC::SakuraWarsSoLongMyLove] = GSC_SakuraWarsSoLongMyLove;
		lut[CRC::ShadowofRome] = GSC_ShadowofRome;
		lut[CRC::Simple2000Vol114] = GSC_Simple2000Vol114;
		lut[CRC::Spartan] = GSC_Spartan;
		lut[CRC::SFEX3] = GSC_SFEX3;
		lut[CRC::TalesOfLegendia] = GSC_TalesOfLegendia;
		lut[CRC::TalesofSymphonia] = GSC_TalesofSymphonia;
		lut[CRC::TombRaiderAnniversary] = GSC_TombRaiderAnniversary;
		lut[CRC::TombRaiderLegend] = GSC_TombRaiderLegend;
		lut[CRC::TombRaiderUnderworld] = GSC_TombRaiderUnderWorld;
		lut[CRC::UrbanReign] = GSC_UrbanReign;
		lut[CRC::WildArms4] = GSC_WildArmsGames;
		lut[CRC::WildArms5] = GSC_WildArmsGames;
		lut[CRC::ZettaiZetsumeiToshi2] = GSC_ZettaiZetsumeiToshi2;

		// Channel Effect
		lut[CRC::GiTS] = GSC_GiTS;
		lut[CRC::SkyGunner] = GSC_SkyGunner; // Maybe not a channel effect
		lut[CRC::SteambotChronicles] = GSC_SteambotChronicles;

		// Colclip not supported
		lut[CRC::LordOfTheRingsThirdAge] = GSC_LordOfTheRingsThirdAge;

		// Depth Issue
		lut[CRC::BurnoutDominator] = GSC_BurnoutGames;
		lut[CRC::BurnoutRevenge] = GSC_BurnoutGames;
		lut[CRC::BurnoutTakedown] = GSC_BurnoutGames;

		// Half Screen bottom issue
		lut[CRC::Tekken5] = GSC_Tekken5;

		// Texture shuffle
		lut[CRC::BigMuthaTruckers] = GSC_BigMuthaTruckers;
		lut[CRC::DeathByDegreesTekkenNinaWilliams] = GSC_DeathByDegreesTekkenNinaWilliams; // + Upscaling issues

		// Upscaling hacks
		lut[CRC::Bully] = GSC_Bully;
		lut[CRC::DBZBT3] = GSC_DBZBT3;
		lut[CRC::EvangelionJo] = GSC_EvangelionJo;
		lut[CRC::FightingBeautyWulong] = GSC_FightingBeautyWulong;
		lut[CRC::GodOfWar2] = GSC_GodOfWar2;
		lut[CRC::IkkiTousen] = GSC_IkkiTousen;
		lut[CRC::Oneechanbara2Special] = GSC_Oneechanbara2Special;
		lut[CRC::UltramanFightingEvolution] = GSC_UltramanFightingEvolution;
		lut[CRC::Yakuza] = GSC_YakuzaGames;
		lut[CRC::Yakuza2] = GSC_YakuzaGames;
	}

	// Hacks that were fixed on OpenGL
	if (Dx_only)
	{
		// Accurate Blending
		lut[CRC::GetaWay] = GSC_GetaWayGames;            // Blending High
		lut[CRC::GetaWayBlackMonday] = GSC_GetaWayGames; // Blending High
		lut[CRC::TenchuFS] = GSC_TenchuGames;
		lut[CRC::TenchuWoH] = GSC_TenchuGames;
		lut[CRC::Sly2] = GSC_SlyGames; // SW blending on fbmask + Upscaling issue
		lut[CRC::Sly3] = GSC_SlyGames; // SW blending on fbmask + Upscaling issue

		// These games emulate a stencil buffer with the alpha channel of the RT (too slow to move to Aggressive)
		// Needs at least Basic Blending,
		// see https://github.com/PCSX2/pcsx2/pull/2921
		lut[CRC::RadiataStories] = GSC_RadiataStories;
		lut[CRC::StarOcean3] = GSC_StarOcean3;
		lut[CRC::ValkyrieProfile2] = GSC_ValkyrieProfile2;
	}

	if (Aggressive)
	{
		lut[CRC::AceCombat4] = GSC_AceCombat4;
		lut[CRC::BleachBladeBattlers] = GSC_BleachBladeBattlers;
		lut[CRC::FFX2] = GSC_FFXGames;
		lut[CRC::FFX] = GSC_FFXGames;
		lut[CRC::FFXII] = GSC_FFXGames;
		lut[CRC::RedDeadRevolver] = GSC_RedDeadRevolver;
		lut[CRC::ShinOnimusha] = GSC_ShinOnimusha;
		lut[CRC::SoTC] = GSC_SoTC;
		lut[CRC::XenosagaE3] = GSC_XenosagaE3;

		// Upscaling issues
		lut[CRC::GodOfWar] = GSC_GodOfWar;
		lut[CRC::Okami] = GSC_Okami;
	}

	m_gsc = lut[m_game.title];
	g_crc_region = m_game.region;
}

#undef Dx_and_OGL
#undef Dx_only
#undef Aggressive

bool GSState::IsBadFrame()
{
	GSFrameInfo fi;

	fi.FBP = m_context->FRAME.Block();
	fi.FPSM = m_context->FRAME.PSM;
	fi.FBMSK = m_context->FRAME.FBMSK;
	fi.TME = PRIM->TME;
	fi.TBP0 = m_context->TEX0.TBP0;
	fi.TPSM = m_context->TEX0.PSM;
	fi.TZTST = m_context->TEST.ZTST;

	if (m_gsc && !m_gsc(fi, m_skip))
	{
		return false;
	}

	if (m_skip == 0 && (m_userhacks_skipdraw > 0))
	{
		if (fi.TME)
		{
			// depth textures (bully, mgs3s1 intro, Front Mission 5)
			// General, often problematic post processing
			if (GSLocalMemory::m_psm[fi.TPSM].depth || GSUtil::HasSharedBits(fi.FBP, fi.FPSM, fi.TBP0, fi.TPSM))
			{
				m_skip_offset = m_userhacks_skipdraw_offset;
				m_skip = std::max(m_userhacks_skipdraw, m_skip_offset);
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
