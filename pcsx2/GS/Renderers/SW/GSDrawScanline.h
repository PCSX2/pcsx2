// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/GSState.h"
#include "GS/Renderers/SW/GSSetupPrimCodeGenerator.all.h"
#include "GS/Renderers/SW/GSDrawScanlineCodeGenerator.all.h"

struct GSScanlineLocalData;

MULTI_ISA_UNSHARED_START

class GSRasterizerData;

class GSDrawScanline : public GSVirtualAlignedClass<32>
{
	friend GSSetupPrimCodeGenerator;
	friend GSDrawScanlineCodeGenerator;

public:
	GSDrawScanline();
	~GSDrawScanline() override;

	/// Debug override for disabling scanline JIT on a key basis.
	static bool ShouldUseCDrawScanline(u64 key);

	/// Function pointer types which we call back into.
	using SetupPrimPtr = void(*)(const GSVertexSW* vertex, const u16* index, const GSVertexSW& dscan, GSScanlineLocalData& local);
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

	static void CSetupPrim(const GSVertexSW* vertex, const u16* index, const GSVertexSW& dscan, GSScanlineLocalData& local);
	static void CDrawScanline(int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local);
	static void CDrawEdge(int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local);
	__ri static void CDrawScanline(int pixels, int left, int top, const GSVertexSW& scan, GSScanlineLocalData& local, GSScanlineSelector sel);
};

MULTI_ISA_UNSHARED_END
