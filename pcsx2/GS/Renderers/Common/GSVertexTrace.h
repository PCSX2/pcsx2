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

#include "GS/GS.h"
#include "GS/GSDrawingContext.h"
#include "GSVertex.h"
#include "GS/Renderers/SW/GSVertexSW.h"
#include "GS/Renderers/HW/GSVertexHW.h"
#include "GSFunctionMap.h"

class GSState;

class alignas(32) GSVertexTrace : public GSAlignedClass<32>
{
public:
	struct Vertex
	{
		GSVector4i c;
		GSVector4 p, t;
	};
	struct VertexAlpha
	{
		int min, max;
		bool valid;
	};
	bool m_accurate_stq;

protected:
	const GSState* m_state;

	static const GSVector4 s_minmax;

	typedef void (GSVertexTrace::*FindMinMaxPtr)(const void* vertex, const u32* index, int count);

	FindMinMaxPtr m_fmm[2][2][2][2][4];

	template <GS_PRIM_CLASS primclass, u32 iip, u32 tme, u32 fst, u32 color, bool provoking_vertex_first>
	void FindMinMax(const void* vertex, const u32* index, int count);

public:
	GS_PRIM_CLASS m_primclass;

	Vertex m_min;
	Vertex m_max;
	VertexAlpha m_alpha; // source alpha range after tfx, GSRenderer::GetAlphaMinMax() updates it

	union
	{
		u32 value;
		struct { u32 r:4, g:4, b:4, a:4, x:1, y:1, z:1, f:1, s:1, t:1, q:1, _pad:1; };
		struct { u32 rgba:16, xyzf:4, stq:4; };
	} m_eq;

	union
	{
		struct { u32 mmag:1, mmin:1, linear:1, opt_linear:1; };
	} m_filter;

	GSVector2 m_lod; // x = min, y = max

public:
	GSVertexTrace(const GSState* state, bool provoking_vertex_first);
	virtual ~GSVertexTrace() {}

	void Update(const void* vertex, const u32* index, int v_count, int i_count, GS_PRIM_CLASS primclass);

	bool IsLinear() const { return m_filter.opt_linear; }
	bool IsRealLinear() const { return m_filter.linear; }

	void CorrectDepthTrace(const void* vertex, int count);
};
