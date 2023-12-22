// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/GSVector.h"

#pragma pack(push, 1)

struct alignas(32) GSVertexHW9
{
	GSVector4 t;
	GSVector4 p;

	// t.z = union {struct {u8 r, g, b, a;}; u32 c0;};
	// t.w = union {struct {u8 ta0, ta1, res, f;}; u32 c1;}

	GSVertexHW9& operator=(GSVertexHW9& v)
	{
		t = v.t;
		p = v.p;
		return *this;
	}
};

#pragma pack(pop)
