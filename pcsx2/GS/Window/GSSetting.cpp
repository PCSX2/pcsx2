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
#include "GSSetting.h"

#ifdef _WIN32
#define cvtString(s) L##s
const wchar_t* dialog_message(int ID, bool* updateText)
{
#else
#define cvtString(s) s
const char* dialog_message(int ID, bool* updateText)
{
#endif
	if (updateText)
		*updateText = true;
	switch (ID)
	{
		case IDC_FILTER:
			return cvtString("Control the texture filtering of the emulation.\n\n"
				"Nearest:\nAlways disable interpolation, rendering will be blocky.\n\n"
				"Bilinear Forced (excluding sprite):\nAlways enable interpolation except for sprites (FMV/Text/2D elements)."
				" Rendering is smoother but it could generate a few glitches. If upscaling is enabled, this setting is recommended over 'Bilinear Forced'\n\n"
				"Bilinear Forced:\nAlways enable interpolation. Rendering is smoother but it could generate some glitches.\n\n"
				"Bilinear PS2:\nUse same mode as the PS2. It is the more accurate option.");
		case IDC_HALF_SCREEN_TS:
			return cvtString("Control the half-screen fix detection on texture shuffling.\n\n"
				"Automatic:\nUses an algorithm to automatically enable or disable the detection.\n\n"
				"Force-Disabled:\nDisables the detection. Will cause visual bugs in many games. It helps Xenosaga games.\n\n"
				"Force-Enabled:\nAlways enables the detection. Use it when a game has half-screen issues.");
		case IDC_TRI_FILTER:
			return cvtString("Control the texture tri-filtering of the emulation.\n\n"
				"None:\nNo extra trilinear filtering.\n\n"
				"Trilinear:\nUse OpenGL/Vulkan trilinear interpolation when PS2 uses mipmaps.\n\n"
				"Trilinear Forced:\nAlways enable full trilinear interpolation. Warning Slow!\n\n");
		case IDC_CRC_LEVEL:
			return cvtString("Control the number of Auto-CRC fixes and hacks applied to games.\n\n"
				"Automatic:\nAutomatically sets the recommended CRC level based on the selected renderer.\n"
				"This is the recommended setting.\n\n"
				"None:\nRemove all CRC rendering fixes and hacks.\n\n"
				"Minimum:\nEnables CRC lookup for special post processing effects.\n\n"
				"Aggressive:\nUse more aggressive CRC hacks.\n"
				"Removes effects in some games which make the image appear sharper/clearer.\n"
				"Affected games: AC4, BleachBB, Bully, DBZBT 2 & 3, DeathByDegrees, Evangelion, FF games, FightingBeautyWulong, GOW 1 & 2, Kunoichi, IkkiTousen, Okami, Oneechanbara2, OnimushaDoD, RDRevolver, Simple2000Vol114, SoTC, SteambotChronicles, Tekken5, Ultraman, XenosagaE3, Yakuza 1 & 2.\n");
		case IDC_SKIPDRAWEND:
		case IDC_SKIPDRAWHACKEDIT:
		case IDC_SKIPDRAWSTART:
		case IDC_SKIPDRAWOFFSETEDIT:
			return cvtString("Completely skips drawing surfaces from the surface in the left box up to the surface specified in the box on the right.\n\n"
				"Use it, for example, to try and get rid of bad post processing effects.\n"
				"Step 1: Increase the value in the left box and keep the value in the right box set to the same value as the left box to find and remove a bad effect.\n"
				"Step 2: If a bad effect found with Step 1 is not completely removed yet, then without changing the value in the left box, try increasing the value in the box to right until the effect is completely gone.\n\n"
				"Note: Increase the value in the right box and keep the value in the left box set to \"1\" to reproduce the old skipdraw behaviour.");
		case IDC_OFFSETHACK:
			return cvtString("Might fix some misaligned fog, bloom, or blend effect.\n"
				"The preferred option is Normal (Vertex) as it is most likely to resolve misalignment issues.\n"
				"The special cases are only useful in a couple of games like Captain Tsubasa.");
		case IDC_WILDHACK:
			return cvtString("Lowers the GS precision to avoid gaps between pixels when upscaling.\n"
				"Fixes the text on Wild Arms games.");
		case IDC_ALIGN_SPRITE:
			return cvtString("Fixes issues with upscaling(vertical lines) in Namco games like Ace Combat, Tekken, Soul Calibur, etc.");
		case IDC_ROUND_SPRITE:
			return cvtString("Corrects the sampling of 2D sprite textures when upscaling.\n\n"
				"Fixes lines in sprites of games like Ar tonelico when upscaling.\n\n"
				"Half option is for flat sprites, Full is for all sprites.");
		case IDC_TCOFFSETX:
		case IDC_TCOFFSETX2:
		case IDC_TCOFFSETY:
		case IDC_TCOFFSETY2:
			return cvtString("Offset for the ST/UV texture coordinates. Fixes some odd texture issues and might fix some post processing alignment too.\n\n"
				"  0500 0500, fixes Persona 3 minimap, helps Haunting Ground.");
		case IDC_OSD_LOG:
			return cvtString("Prints log messages from the Function keys onscreen.");
		case IDC_PALTEX:
			return cvtString("Enabled: GPU converts colormap-textures.\n"
				"Disabled: CPU converts colormap-textures.\n\n"
				"It is a trade-off between GPU and CPU.");
		case IDC_PCRTC_OFFSETS:
			return cvtString("Enable: Takes in to account offsets in the analogue circuits.\n"
				"This will use the intended aspect ratios and screen offsets, may cause odd black borders.\n"
				"Used for screen positioning and screen shake in Wipeout Fusion.");
		case IDC_DISABLE_INTERLACE_OFFSETS:
			return cvtString("Enable: Removes the offset for interlacing when upscaling.\n"
				"Can reduce blurring in some games, where the opposite is true most of the time.\n"
				"Used for ICO to reduce blur.");
		case IDC_ACCURATE_DATE:
			return cvtString("Implement a more accurate algorithm to compute GS destination alpha testing.\n"
				"It improves shadow and transparency rendering.\n\n"
				"Note: Direct3D 11 is less accurate.");
		case IDC_ACCURATE_BLEND_UNIT:
			return cvtString("Control the accuracy level of the GS blending unit emulation.\n\n"
				"Minimum:\nFast but introduces various rendering issues.\n"
				"It is intended for slow computers.\n\n"
				"Basic:\nEmulate correctly most of the effects with a limited speed penalty.\n"
				"This is the recommended setting.\n\n"
				"Medium:\nExtend it to all sprites. Performance impact remains reasonable in 3D game.\n\n"
				"High:\nExtend it to destination alpha blending and color wrapping (helps shadow and fog effects).\n"
				"A good CPU is required.\n\n"
				"Full:\nExcept few cases, the blending unit will be fully emulated by the shader. It is slow!\n\n"
				"Maximum:\nThe blending unit will be completely emulated by the shader. It is very slow!\n\n"
				"Note: Direct3D11's blending is capped at High and is reduced in capability compared to OpenGL/Vulkan");
		case IDC_TC_DEPTH:
			return cvtString("Disable the support of Depth buffer in the texture cache.\n"
				"It can help to increase speed but it will likely create various glitches.");
		case IDC_CPU_FB_CONVERSION:
			return cvtString("Convert 4-bit and 8-bit frame buffer on the CPU instead of the GPU.\n\n"
				"The hack can fix glitches in some games.\n"
				"Harry Potter games and Stuntman for example.\n\n"
				"Note: This hack has an impact on performance.\n");
		case IDC_AFCOMBO:
			return cvtString("Reduces texture aliasing at extreme viewing angles.");
		case IDC_AA1:
			return cvtString("Internal GS feature. Reduces edge aliasing of lines and triangles when the game requests it.");
		case IDC_SWTHREADS:
		case IDC_SWTHREADS_EDIT:
			return cvtString("Number of rendering threads: 0 for single thread, 2 or more for multithread (1 is for debugging)\n"
				"If you have 4 threads on your CPU pick 2 or 3.\n"
				"You can calculate how to get the best performance (amount of CPU threads - 2)\n"
				"Note: 7+ threads will not give much more performance and could perhaps even lower it.");
		case IDC_MIPMAP_SW:
			return cvtString("Enables mipmapping, which some games require to render correctly.");
		case IDC_SHADEBOOST:
			return cvtString("Allows brightness, contrast and saturation to be manually adjusted.");
		case IDC_SHADER_FX:
			return cvtString("Enables external shader for additional post-processing effects.");
		case IDC_FXAA:
			return cvtString("Enables fast approximate anti-aliasing. Small performance impact.");
		case IDC_AUTO_FLUSH_HW:
			return cvtString("Force a primitive flush when a framebuffer is also an input texture.\n"
				"Fixes some processing effects such as the shadows in the Jak series and radiosity in GTA:SA.\n"
				"Warning: It's very costly on the performance.\n\n"
				"Note: OpenGL/Vulkan HW renderer is able to handle Jak shadows at full speed without this option.");
		case IDC_AUTO_FLUSH_SW:
			return cvtString("Force a primitive flush when a framebuffer is also an input texture.\n"
				"Fixes some processing effects such as the shadows in the Jak series and radiosity in GTA:SA.");
		case IDC_SAFE_FEATURES:
			return cvtString("This option disables multiple safe features.\n\n"
				"Disables accurate Unscale Point and Line rendering.\n"
				"It can help Xenosaga games.\n\n"
				"Disables accurate GS Memory Clearing to be done on the CPU, and let only the GPU handle it.\n"
				"It can help Kingdom Hearts games.");
		case IDC_MEMORY_WRAPPING:
			return cvtString("Emulates GS memory wrapping accurately. This fixes issues where part of the image is cut-off by block shaped sections such as the FMVs in Wallace & Gromit: The Curse of the Were-Rabbit and Thrillville.\n\n"
				"Note: This hack can have a small impact on performance.");
		case IDC_MERGE_PP_SPRITE:
			return cvtString("Replaces post-processing multiple paving sprites by a single fat sprite.\n"
				"It reduces various upscaling lines.\n\n"
				"Note: This hack is a work in progress.");
		case IDC_GEOMETRY_SHADER_OVERRIDE:
			return cvtString("Allows the GPU instead of just the CPU to transform lines into sprites. This reduces CPU load and bandwidth requirement, but it is heavier on the GPU.\n"
				"Automatic detection is recommended.");
		case IDC_IMAGE_LOAD_STORE:
			return cvtString("Allows advanced atomic operations to speed up Accurate DATE.\n"
				"Only disable this if using Accurate DATE causes (GPU driver) issues.\n\n"
				"Note: This option is only supported by GPUs which support at least Direct3D 11.");
		case IDC_SPARSE_TEXTURE:
			return cvtString("Allows to reduce VRAM usage on the GPU.\n\n"
				"Note: Feature is currently experimental and works only on Nvidia GPUs.");
		case IDC_LINEAR_PRESENT:
			return cvtString("Use bilinear filtering when Upscaling/Downscaling the image to the screen. Disable it if you want a sharper/pixelated output.");
		// Exclusive for Hardware Renderer
		case IDC_PRELOAD_GS:
			return cvtString("Uploads GS data when rendering a new frame to reproduce some effects accurately.\n"
				"Fixes black screen issues in games like Armored Core: Last Raven.");
		case IDC_MIPMAP_HW:
			return	cvtString("Control the accuracy level of the mipmapping emulation.\n\n"
				"Automatic:\nAutomatically sets the mipmapping level based on the game.\n"
				"This is the recommended setting.\n\n"
				"Off:\nMipmapping emulation is disabled.\n\n"
				"Basic (Fast):\nPartially emulates mipmapping, performance impact is negligible in most cases.\n\n"
				"Full (Slow):\nCompletely emulates the mipmapping function of the GS, might significantly impact performance.");
		case IDC_DISABLE_PARTIAL_TC_INV:
			return cvtString("By default, the texture cache handles partial invalidations. Unfortunately it is very costly to compute CPU wise."
				   "\n\nThis hack replaces the partial invalidation with a complete deletion of the texture to reduce the CPU load.\n\nIt helps snowblind engine games.");
		case IDC_CONSERVATIVE_FB:
			return cvtString("Disabled: Reserves a larger framebuffer to prevent FMV flickers.\n"
				   "Increases GPU/memory requirements.\n"
				   "Disabling this can amplify stuttering due to low RAM/VRAM.\n\n"
				   "Note: It should be enabled for Armored Core, Destroy All Humans, Gran Turismo and possibly others.\n"
				   "This option does not improve the graphics or the FPS.");
		case IDC_DITHERING:
			return cvtString("In the PS2's case, it reduces banding between colors and improves the perceived color depth.\n"
				   "In the PS1's case, it was used more aggressively due to 16-bit colour.\n"
				   "Sit far enough and don't examine it too closely for the best effect.\n\n"
				   "Off:\nDisables any dithering.\n\n"
				   "Unscaled:\nNative Dithering / Lowest dithering effect does not increase size of squares when upscaling.\n\n"
				   "Scaled:\nUpscaling-aware / Highest dithering effect.");
		case IDC_PRELOAD_TEXTURES:
			return cvtString("Uploads entire textures at once instead of small pieces, avoiding redundant uploads when possible.\n"
				   "Improves performance in most games, but can make a small selection slower.");
		case IDC_TEX_IN_RT:
			return cvtString("Allows the texture cache to reuse as an input texture the inner portion of a previous framebuffer.\n"
				"In some selected games this is enabled by default regardless of this setting.");
		default:
			if (updateText)
				*updateText = false;
			return cvtString("");
	}
}
#undef cvtString
