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

#include "GS/GSState.h"
#include "GS/Renderers/SW/GSSetupPrimCodeGenerator.h"
#include "GS/Renderers/SW/GSDrawScanlineCodeGenerator.h"

struct GSScanlineLocalData;

MULTI_ISA_UNSHARED_START

class GSRasterizerData;

class GSSetupPrimCodeGenerator;
class GSDrawScanlineCodeGenerator;

class GSDrawScanline : public GSVirtualAlignedClass<32>
{
	friend GSSetupPrimCodeGenerator;
	friend GSDrawScanlineCodeGenerator;

public:
	GSDrawScanline();
	~GSDrawScanline() override;

	/// Function pointer types which we call back into.
	using SetupPrimPtr = void(*)(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan, GSScanlineLocalData& local);
	using DrawScanlinePtr = void(*)(int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local);

	/// Flushes the code cache, forcing everything to be recompiled.
	void ResetCodeCache();

	/// Populates function pointers. If this returns false, we ran out of code space.
	bool SetupDraw(GSRasterizerData& data);

	/// Draw pre-calculations, computed per-thread.
	static void BeginDraw(const GSRasterizerData& data, GSScanlineLocalData& local);

	/// Not currently jitted.
	static void DrawRect(const GSVector4i& r, const GSVertexSW& v, GSScanlineLocalData& local);

	void UpdateDrawStats(u64 frame, u64 ticks, int actual, int total, int prims);
	void PrintStats();

private:
	GSCodeGeneratorFunctionMap<GSSetupPrimCodeGenerator, u64, SetupPrimPtr> m_sp_map;
	GSCodeGeneratorFunctionMap<GSDrawScanlineCodeGenerator, u64, DrawScanlinePtr> m_ds_map;

	static void CSetupPrim(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan, GSScanlineLocalData& local);
	static void CDrawScanline(int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local);
	static void CDrawEdge(int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local);
};

MULTI_ISA_UNSHARED_END
