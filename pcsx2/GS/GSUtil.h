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

#include "GS.h"
#include "GSRegs.h"

class GSUtil
{
public:
	static void Init();

	static GS_PRIM_CLASS GetPrimClass(u32 prim);
	static int GetVertexCount(u32 prim);
	static int GetClassVertexCount(u32 primclass);

	static const u32* HasSharedBitsPtr(u32 dpsm);
	static bool HasSharedBits(u32 spsm, const u32* ptr);
	static bool HasSharedBits(u32 spsm, u32 dpsm);
	static bool HasSharedBits(u32 sbp, u32 spsm, u32 dbp, u32 dpsm);
	static bool HasCompatibleBits(u32 spsm, u32 dpsm);
	static bool HasSameSwizzleBits(u32 spsm, u32 dpsm);
	static u32 GetChannelMask(u32 spsm);
	static u32 GetChannelMask(u32 spsm, u32 fbmsk);

	static GSRendererType GetPreferredRenderer();
};

const char* psm_str(int psm);
