// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/GS.h"
#include "GS/GSDrawingContext.h"
#include "GS/MultiISA.h"
#include "GSVertex.h"
#include "GS/Renderers/SW/GSVertexSW.h"
#include "GS/Renderers/HW/GSVertexHW.h"
#include "GSFunctionMap.h"

class GSState;
class GSVertexTrace;

MULTI_ISA_DEF(class GSVertexTraceFMM;)
MULTI_ISA_DEF(void GSVertexTracePopulateFunctions(GSVertexTrace& vt, bool provoking_vertex_first);)

class alignas(32) GSVertexTrace final : public GSAlignedClass<32>
{
	MULTI_ISA_FRIEND(GSVertexTraceFMM)

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
	bool m_accurate_stq = false;

protected:
	const GSState* m_state;

	typedef void (*FindMinMaxPtr)(GSVertexTrace& vt, const void* vertex, const u16* index, int count);

	FindMinMaxPtr m_fmm[2][2][2][2][4];

public:
	GS_PRIM_CLASS m_primclass = GS_INVALID_CLASS;

	Vertex m_min = {};
	Vertex m_max = {};
	VertexAlpha m_alpha = {}; // source alpha range after tfx, GSRenderer::GetAlphaMinMax() updates it

	union
	{
		u32 value;
		struct { u32 r:4, g:4, b:4, a:4, x:1, y:1, z:1, f:1, s:1, t:1, q:1, _pad:1; };
		struct { u32 rgba:16, xyzf:4, stq:4; };
	} m_eq = {};

	union
	{
		struct { u32 mmag:1, mmin:1, linear:1, opt_linear:1; };
	} m_filter = {};

	GSVector2 m_lod = {}; // x = min, y = max

public:
	GSVertexTrace(const GSState* state, bool provoking_vertex_first);

	void Update(const void* vertex, const u16* index, int v_count, int i_count, GS_PRIM_CLASS primclass);

	bool IsLinear() const { return m_filter.opt_linear; }
	bool IsRealLinear() const { return m_filter.linear; }

	void CorrectDepthTrace(const void* vertex, int count);
};
