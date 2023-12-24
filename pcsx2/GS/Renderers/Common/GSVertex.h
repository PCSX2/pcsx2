// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/GSRegs.h"
#include "GS/GSVector.h"
#include "GS/Renderers/HW/GSVertexHW.h"
#include "GS/Renderers/SW/GSVertexSW.h"

#pragma pack(push, 1)

struct alignas(32) GSVertex
{
	union
	{
		struct
		{
			GIFRegST ST;       // S:0, T:4
			GIFRegRGBAQ RGBAQ; // RGBA:8, Q:12
			GIFRegXYZ XYZ;     // XY:16, Z:20
			union { u32 UV; struct { u16 U, V; }; }; // UV:24
			u32 FOG;        // FOG:28
		};

#if _M_SSE >= 0x500
		__m256i mx;
#endif
		__m128i m[2];
	};

	GSVertex() = default; // Warning object is potentially used in hot path

#if _M_SSE >= 0x500
	GSVertex(const GSVertex& v)
	{
		mx = v.mx;
	}
	void operator=(const GSVertex& v) { mx = v.mx; }
#else
	GSVertex(const GSVertex& v)
	{
		m[0] = v.m[0];
		m[1] = v.m[1];
	}
	void operator=(const GSVertex& v)
	{
		m[0] = v.m[0];
		m[1] = v.m[1];
	}
#endif
};

struct GSVertexP
{
	GSVector4 p;
};

struct alignas(32) GSVertexPT1
{
	GSVector4 p;
	GSVector2 t;
	char pad[4];
	union { u32 c; struct { u8 r, g, b, a; }; };
};

struct GSVertexPT2
{
	GSVector4 p;
	GSVector2 t[2];
};

#pragma pack(pop)
