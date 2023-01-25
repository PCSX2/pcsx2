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

#include <cstring>

MULTI_ISA_UNSHARED_START

class GSDrawScanline : public GSVirtualAlignedClass<32>
{
public:
	using SetupPrimPtr = void(*)(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan, GSScanlineLocalData& local);
	using DrawScanlinePtr = void(*)(int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local);

	class SharedData : public GSRasterizerData
	{
	public:
		GSScanlineGlobalData global;

#ifdef ENABLE_JIT_RASTERIZER
		SetupPrimPtr sp;
		DrawScanlinePtr ds;
		DrawScanlinePtr de;
#endif
	};

protected:
	GSCodeGeneratorFunctionMap<GSSetupPrimCodeGenerator, u64, SetupPrimPtr> m_sp_map;
	GSCodeGeneratorFunctionMap<GSDrawScanlineCodeGenerator, u64, DrawScanlinePtr> m_ds_map;

	template <class T, bool masked>
	static void DrawRectT(const GSOffset& off, const GSVector4i& r, u32 c, u32 m, GSScanlineLocalData& local);

	template <class T, bool masked>
	static __forceinline void FillRect(const GSOffset& off, const GSVector4i& r, u32 c, u32 m, GSScanlineLocalData& local);

#if _M_SSE >= 0x501

	template <class T, bool masked>
	static __forceinline void FillBlock(const GSOffset& off, const GSVector4i& r, const GSVector8i& c, const GSVector8i& m, GSScanlineLocalData& local);

#else

	template <class T, bool masked>
	static __forceinline void FillBlock(const GSOffset& off, const GSVector4i& r, const GSVector4i& c, const GSVector4i& m, GSScanlineLocalData& local);

#endif

public:
	GSDrawScanline();
	~GSDrawScanline() override;

	void SetupDraw(GSRasterizerData& data);
	void UpdateDrawStats(u64 frame, u64 ticks, int actual, int total, int prims);

	static void BeginDraw(const GSRasterizerData& data, GSScanlineLocalData& local);

	static void CSetupPrim(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan, GSScanlineLocalData& local);
	static void CDrawScanline(int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local);

	template<class T> static bool TestAlpha(T& test, T& fm, T& zm, const T& ga, const GSScanlineGlobalData& global);
	template<class T> static void WritePixel(const T& src, int addr, int i, u32 psm, const GSScanlineGlobalData& global);

#ifdef ENABLE_JIT_RASTERIZER

	__forceinline static void SetupPrim(const GSRasterizerData& data, const GSVertexSW* vertex, const u32* index,
		const GSVertexSW& dscan, GSScanlineLocalData& local)
	{
		static_cast<const SharedData&>(data).sp(vertex, index, dscan, local);
	}
	__forceinline static void DrawScanline(
		const GSRasterizerData& data, int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local)
	{
		static_cast<const SharedData&>(data).ds(pixels, left, top, scan, local);
	}
	__forceinline static void DrawEdge(
		const GSRasterizerData& data, int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local)
	{
		static_cast<const SharedData&>(data).de(pixels, left, top, scan, local);
	}

	__forceinline static bool HasEdge(const GSRasterizerData& data)
	{
		return static_cast<const SharedData&>(data).de != nullptr;
	}

#else

	__forceinline static void SetupPrim(const GSRasterizerData& data, const GSVertexSW* vertex, const u32* index,
		const GSVertexSW& dscan, GSScanlineLocalData& local)
	{
		CSetupPrim(vertex, index, dscan, local);
	}
	__forceinline static void DrawScanline(
		const GSRasterizerData& data, int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local)
	{
		CDrawScanline(pixels, left, top, scan, local);
	}
	__forceinline static void DrawEdge(
		const GSRasterizerData& data, int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local)
	{
		// This sucks. But so does not jitting!
		const GSScanlineGlobalData* old_gd = local.gd;
		GSScanlineGlobalData gd;
		std::memcpy(&gd, &local.gd, sizeof(gd));
		gd.sel.zwrite = 0;
		gd.sel.edge = 1;
		local.gd = &gd;

		CDrawScanline(pixels, left, top, scan, local);

		local.gd = old_gd;
	}

	__forceinline static bool HasEdge(const SharedData& data)
	{
		return static_cast<const SharedData&>(data).global.sel.aa1;
	}

#endif

	// Not currently jitted.
	void DrawRect(const GSVector4i& r, const GSVertexSW& v, GSScanlineLocalData& local);

	void PrintStats()
	{
		m_ds_map.PrintStats();
	}
};

MULTI_ISA_UNSHARED_END
