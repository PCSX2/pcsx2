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

#include "Renderers/SW/GSScanlineEnvironment.h"
#include "Renderers/Common/GSFunctionMap.h"
#include "GSUtil.h"

class GSSetupPrimCodeGenerator : public GSCodeGenerator
{
	void operator=(const GSSetupPrimCodeGenerator&);

	GSScanlineSelector m_sel;
	GSScanlineLocalData& m_local;
	bool m_rip;

	struct
	{
		uint32 z : 1, f : 1, t : 1, c : 1;
	} m_en;

#if _M_SSE < 0x501
	void Generate_SSE();
	void Depth_SSE();
	void Texture_SSE();
	void Color_SSE();

	void Generate_AVX();
	void Depth_AVX();
	void Texture_AVX();
	void Color_AVX();
#else
	void Generate_AVX2();
	void Depth_AVX2();
	void Texture_AVX2();
	void Color_AVX2();
#endif

public:
	GSSetupPrimCodeGenerator(void* param, uint64 key, void* code, size_t maxsize);
};
