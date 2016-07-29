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

#include "stdafx.h"
#include "GSSetupPrimCodeGenerator.h"

#if _M_SSE >= 0x501

GSVector8 GSSetupPrimCodeGenerator::m_shift[9];

#else

GSVector4 GSSetupPrimCodeGenerator::m_shift[5];

#endif

GSSetupPrimCodeGenerator::GSSetupPrimCodeGenerator(void* param, uint64 key, void* code, size_t maxsize)
	: GSCodeGenerator(code, maxsize)
	, m_local(*(GSScanlineLocalData*)param)
{
	m_sel.key = key;

	m_en.z = m_sel.zb ? 1 : 0;
	m_en.f = m_sel.fb && m_sel.fge ? 1 : 0;
	m_en.t = m_sel.fb && m_sel.tfx != TFX_NONE ? 1 : 0;
	m_en.c = m_sel.fb && !(m_sel.tfx == TFX_DECAL && m_sel.tcc) ? 1 : 0;

	Generate();
}

void GSSetupPrimCodeGenerator::Init()
{
	#if _M_SSE >= 0x501

	m_shift[0] = GSVector8(8.0f, 8.0f, 8.0f, 8.0f, 8.0f, 8.0f, 8.0f, 8.0f);
	m_shift[1] = GSVector8(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
	m_shift[2] = GSVector8(-1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f);
	m_shift[3] = GSVector8(-2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f);
	m_shift[4] = GSVector8(-3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f);
	m_shift[5] = GSVector8(-4.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f);
	m_shift[6] = GSVector8(-5.0f, -4.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f);
	m_shift[7] = GSVector8(-6.0f, -5.0f, -4.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f);
	m_shift[8] = GSVector8(-7.0f, -6.0f, -5.0f, -4.0f, -3.0f, -2.0f, -1.0f, 0.0f);

	#else

	m_shift[0] = GSVector4(4.0f, 4.0f, 4.0f, 4.0f);
	m_shift[1] = GSVector4(0.0f, 1.0f, 2.0f, 3.0f);
	m_shift[2] = GSVector4(-1.0f, 0.0f, 1.0f, 2.0f);
	m_shift[3] = GSVector4(-2.0f, -1.0f, 0.0f, 1.0f);
	m_shift[4] = GSVector4(-3.0f, -2.0f, -1.0f, 0.0f);

	#endif
}
