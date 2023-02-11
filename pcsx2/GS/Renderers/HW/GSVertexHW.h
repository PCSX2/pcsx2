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

#include <cstdint>
#include <cstring>
#include <algorithm>

#include "GSVector.h"

#pragma pack(push, 1)

struct GSVertexHW9
{
    GSVector4 t;
    GSVector4 p;

    GSVertexHW9() = default;

    GSVertexHW9(const GSVector4& t_, const GSVector4& p_)
        : t(t_)
        , p(p_)
    {}

    GSVertexHW9(const GSVertexHW9& other)
    {
        memcpy(this, &other, sizeof(GSVertexHW9));
    }

    GSVertexHW9& operator=(const GSVertexHW9& other)
    {
        memcpy(this, &other, sizeof(GSVertexHW9));
        return *this;
    }
};

#pragma pack(pop)

static_assert(sizeof(GSVertexHW9) == 32, "GSVertexHW9 should have a size of 32 bytes");

