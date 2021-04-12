/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "GS.h"
#include "GSVector.h"
#include "Renderers/HW/GSVertexHW.h"
#include "Renderers/SW/GSVertexSW.h"

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
			union { uint32 UV; struct { uint16 U, V; }; }; // UV:24
			uint32 FOG;        // FOG:28
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
	union { uint32 c; struct { uint8 r, g, b, a; }; };
};

struct GSVertexPT2
{
	GSVector4 p;
	GSVector2 t[2];
};

#pragma pack(pop)
