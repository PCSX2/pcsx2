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
#include "GS/Renderers/SW/GSRasterizer.h"
#include "GS/Renderers/SW/GSScanlineEnvironment.h"
#include "GS/Renderers/SW/GSSetupPrimCodeGenerator.h"
#include "GS/Renderers/SW/GSDrawScanlineCodeGenerator.h"
#include "GS/config.h"

MULTI_ISA_UNSHARED_START

class GSDrawScanline : public GSAlignedClass<32>
{
public:
	class SharedData : public GSRasterizerData
	{
	public:
		GSScanlineGlobalData global;
	};

	typedef void (*SetupPrimPtr)(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan);
	typedef void (*DrawScanlinePtr)(int pixels, int left, int top, const GSVertexSW& scan);

protected:
	GSScanlineGlobalData m_global = {};
	GSScanlineLocalData m_local = {};

	SetupPrimPtr m_sp = nullptr;
	DrawScanlinePtr m_ds = nullptr;
	DrawScanlinePtr m_de = nullptr;

	GSCodeGeneratorFunctionMap<GSSetupPrimCodeGenerator, u64, SetupPrimPtr> m_sp_map;
	GSCodeGeneratorFunctionMap<GSDrawScanlineCodeGenerator, u64, DrawScanlinePtr> m_ds_map;

	template <class T, bool masked>
	void DrawRectT(const GSOffset& off, const GSVector4i& r, u32 c, u32 m);

	template <class T, bool masked>
	__forceinline void FillRect(const GSOffset& off, const GSVector4i& r, u32 c, u32 m);

#if _M_SSE >= 0x501

	template <class T, bool masked>
	__forceinline void FillBlock(const GSOffset& off, const GSVector4i& r, const GSVector8i& c, const GSVector8i& m);

#else

	template <class T, bool masked>
	__forceinline void FillBlock(const GSOffset& off, const GSVector4i& r, const GSVector4i& c, const GSVector4i& m);

#endif

public:
	GSDrawScanline();
	virtual ~GSDrawScanline() = default;

	__forceinline bool HasEdge() const { return m_de != nullptr; }
	__forceinline bool IsSolidRect() const { return m_global.sel.IsSolidRect(); }

	// IDrawScanline

	void BeginDraw(const GSRasterizerData* data);
	void EndDraw(u64 frame, u64 ticks, int actual, int total, int prims);

	static void CSetupPrim(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan, GSScanlineLocalData& local, const GSScanlineGlobalData& global);
	static void CDrawScanline(int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local, const GSScanlineGlobalData& global);

	template<class T> static bool TestAlpha(T& test, T& fm, T& zm, const T& ga, const GSScanlineGlobalData& global);
	template<class T> static void WritePixel(const T& src, int addr, int i, u32 psm, const GSScanlineGlobalData& global);

#ifdef ENABLE_JIT_RASTERIZER

	__forceinline void SetupPrim(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan) { m_sp(vertex, index, dscan); }
	__forceinline void DrawScanline(int pixels, int left, int top, const GSVertexSW& scan) { m_ds(pixels, left, top, scan); }
	__forceinline void DrawEdge(int pixels, int left, int top, const GSVertexSW& scan) { m_de(pixels, left, top, scan); }

#else

	void SetupPrim(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan);
	void DrawScanline(int pixels, int left, int top, const GSVertexSW& scan);
	void DrawEdge(int pixels, int left, int top, const GSVertexSW& scan);

#endif

	// Not currently jitted.
	void DrawRect(const GSVector4i& r, const GSVertexSW& v);

	void PrintStats()
	{
		m_ds_map.PrintStats();
	}
};

MULTI_ISA_UNSHARED_END
