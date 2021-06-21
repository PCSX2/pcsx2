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

#include <xbyak/xbyak_util.h>

class GSUtil
{
public:
	static void Init();

	static GS_PRIM_CLASS GetPrimClass(uint32 prim);
	static int GetVertexCount(uint32 prim);
	static int GetClassVertexCount(uint32 primclass);

	static const uint32* HasSharedBitsPtr(uint32 dpsm);
	static bool HasSharedBits(uint32 spsm, const uint32* ptr);
	static bool HasSharedBits(uint32 spsm, uint32 dpsm);
	static bool HasSharedBits(uint32 sbp, uint32 spsm, uint32 dbp, uint32 dpsm);
	static bool HasCompatibleBits(uint32 spsm, uint32 dpsm);

	static bool CheckSSE();
	static CRCHackLevel GetRecommendedCRCHackLevel(GSRendererType type);

#ifdef _WIN32
	static bool CheckDXGI();
	static bool CheckD3D11();
	static GSRendererType GetBestRenderer();
	static D3D_FEATURE_LEVEL CheckDirect3D11Level(IDXGIAdapter* adapter = NULL, D3D_DRIVER_TYPE type = D3D_DRIVER_TYPE_HARDWARE);
#endif
};

#ifdef _WIN32
void GSmkdir(const wchar_t* dir);
#else
void GSmkdir(const char* dir);
#endif
std::string GStempdir();

const char* psm_str(int psm);

extern Xbyak::util::Cpu g_cpu;
