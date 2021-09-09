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
#include "GSRasterizer.h"
#include "GSScanlineEnvironment.h"
#include "GSSetupPrimCodeGenerator.h"
#include "GSDrawScanlineCodeGenerator.h"

class GSDrawScanline : public IDrawScanline
{
public:
	class SharedData : public GSRasterizerData
	{
	public:
		GSScanlineGlobalData global;
	};

protected:
	GSScanlineGlobalData m_global;
	GSScanlineLocalData m_local;

	GSCodeGeneratorFunctionMap<GSSetupPrimCodeGenerator, uint64, SetupPrimPtr> m_sp_map;
	GSCodeGeneratorFunctionMap<GSDrawScanlineCodeGenerator, uint64, DrawScanlinePtr> m_ds_map;

	template <class T, bool masked>
	void DrawRectT(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, uint32 c, uint32 m);

	template <class T, bool masked>
	__forceinline void FillRect(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, uint32 c, uint32 m);

#if _M_SSE >= 0x501

	template <class T, bool masked>
	__forceinline void FillBlock(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, const GSVector8i& c, const GSVector8i& m);

#else

	template <class T, bool masked>
	__forceinline void FillBlock(const int* RESTRICT row, const int* RESTRICT col, const GSVector4i& r, const GSVector4i& c, const GSVector4i& m);

#endif

public:
	GSDrawScanline();
	virtual ~GSDrawScanline() = default;

	// IDrawScanline

	void BeginDraw(const GSRasterizerData* data);
	void EndDraw(uint64 frame, uint64 ticks, int actual, int total, int prims);

	void DrawRect(const GSVector4i& r, const GSVertexSW& v);

#ifndef ENABLE_JIT_RASTERIZER

	void SetupPrim(const GSVertexSW* vertex, const uint32* index, const GSVertexSW& dscan);
	void DrawScanline(int pixels, int left, int top, const GSVertexSW& scan);
	void DrawEdge(int pixels, int left, int top, const GSVertexSW& scan);

	bool IsEdge() const { return m_global.sel.aa1; }
	bool IsRect() const { return m_global.sel.IsSolidRect(); }

	template<class T> bool TestAlpha(T& test, T& fm, T& zm, const T& ga);
	template<class T> void WritePixel(const T& src, int addr, int i, uint32 psm);

#endif

	void PrintStats()
	{
		m_ds_map.PrintStats();
	}
};
