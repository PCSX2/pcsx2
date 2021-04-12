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
#include "GSDrawScanlineCodeGenerator.h"

#if _M_SSE >= 0x501
#else
void GSDrawScanlineCodeGenerator::Generate()
{
	if (m_cpu.has(util::Cpu::tAVX))
		Generate_AVX();
	else
		Generate_SSE();
}
#endif

GSDrawScanlineCodeGenerator::GSDrawScanlineCodeGenerator(void* param, uint64 key, void* code, size_t maxsize)
	: GSCodeGenerator(code, maxsize)
	, m_local(*(GSScanlineLocalData*)param)
	, m_rip(false)
{
	m_sel.key = key;

	if (m_sel.breakpoint)
		db(0xCC);

	try
	{
		Generate();
	}
	catch (std::exception& e)
	{
		fprintf(stderr, "ERR:GSDrawScanlineCodeGenerator %s\n", e.what());
	}
}

void GSDrawScanlineCodeGenerator::modulate16(const Xmm& a, const Operand& f, uint8 shift)
{
	if (m_cpu.has(util::Cpu::tAVX))
	{
		if (shift == 0)
		{
			vpmulhrsw(a, f);
		}
		else
		{
			vpsllw(a, shift + 1);
			vpmulhw(a, f);
		}
	}
	else
	{
		if (shift == 0 && m_cpu.has(util::Cpu::tSSSE3))
		{
			pmulhrsw(a, f);
		}
		else
		{
			psllw(a, shift + 1);
			pmulhw(a, f);
		}
	}
}

void GSDrawScanlineCodeGenerator::lerp16(const Xmm& a, const Xmm& b, const Xmm& f, uint8 shift)
{
	if (m_cpu.has(util::Cpu::tAVX))
	{
		vpsubw(a, b);
		modulate16(a, f, shift);
		vpaddw(a, b);
	}
	else
	{
		psubw(a, b);
		modulate16(a, f, shift);
		paddw(a, b);
	}
}

void GSDrawScanlineCodeGenerator::lerp16_4(const Xmm& a, const Xmm& b, const Xmm& f)
{
	if (m_cpu.has(util::Cpu::tAVX))
	{
		vpsubw(a, b);
		vpmullw(a, f);
		vpsraw(a, 4);
		vpaddw(a, b);
	}
	else
	{
		psubw(a, b);
		pmullw(a, f);
		psraw(a, 4);
		paddw(a, b);
	}
}

void GSDrawScanlineCodeGenerator::mix16(const Xmm& a, const Xmm& b, const Xmm& temp)
{
	if (m_cpu.has(util::Cpu::tAVX))
	{
		vpblendw(a, b, 0xaa);
	}
	else
	{
		pblendw(a, b, 0xaa);
	}
}

void GSDrawScanlineCodeGenerator::clamp16(const Xmm& a, const Xmm& temp)
{
	if (m_cpu.has(util::Cpu::tAVX))
	{
		vpackuswb(a, a);

#if _M_SSE >= 0x501
		// Greg: why ?
		if (m_cpu.has(util::Cpu::tAVX2))
		{
			ASSERT(a.isYMM());
			vpermq(Ymm(a.getIdx()), Ymm(a.getIdx()), _MM_SHUFFLE(3, 1, 2, 0)); // this sucks
		}
#endif

		vpmovzxbw(a, a);
	}
	else
	{
		packuswb(a, a);
		pmovzxbw(a, a);
	}
}

void GSDrawScanlineCodeGenerator::alltrue(const Xmm& test)
{
	uint32 mask = test.isYMM() ? 0xffffffff : 0xffff;

	if (m_cpu.has(util::Cpu::tAVX))
	{
		vpmovmskb(eax, test);
		cmp(eax, mask);
		je("step", T_NEAR);
	}
	else
	{
		pmovmskb(eax, test);
		cmp(eax, mask);
		je("step", T_NEAR);
	}
}

void GSDrawScanlineCodeGenerator::blend(const Xmm& a, const Xmm& b, const Xmm& mask)
{
	if (m_cpu.has(util::Cpu::tAVX))
	{
		vpand(b, mask);
		vpandn(mask, a);
		vpor(a, b, mask);
	}
	else
	{
		pand(b, mask);
		pandn(mask, a);
		por(b, mask);
		movdqa(a, b);
	}
}

void GSDrawScanlineCodeGenerator::blendr(const Xmm& b, const Xmm& a, const Xmm& mask)
{
	if (m_cpu.has(util::Cpu::tAVX))
	{
		vpand(b, mask);
		vpandn(mask, a);
		vpor(b, mask);
	}
	else
	{
		pand(b, mask);
		pandn(mask, a);
		por(b, mask);
	}
}

void GSDrawScanlineCodeGenerator::blend8(const Xmm& a, const Xmm& b)
{
	if (m_cpu.has(util::Cpu::tAVX))
		vpblendvb(a, a, b, xmm0);
	else
		pblendvb(a, b);
}

void GSDrawScanlineCodeGenerator::blend8r(const Xmm& b, const Xmm& a)
{
	if (m_cpu.has(util::Cpu::tAVX))
	{
		vpblendvb(b, a, b, xmm0);
	}
	else
	{
		pblendvb(a, b);
		movdqa(b, a);
	}
}

void GSDrawScanlineCodeGenerator::split16_2x8(const Xmm& l, const Xmm& h, const Xmm& src)
{
	// l = src & 0xFF; (1 left shift + 1 right shift)
	// h = (src >> 8) & 0xFF; (1 right shift)

	if (m_cpu.has(util::Cpu::tAVX))
	{
		if (src == h)
		{
			vpsllw(l, src, 8);
			vpsrlw(h, 8);
		}
		else if (src == l)
		{
			vpsrlw(h, src, 8);
			vpsllw(l, 8);
		}
		else
		{
			vpsllw(l, src, 8);
			vpsrlw(h, src, 8);
		}
		vpsrlw(l, 8);
	}
	else
	{
		if (src == h)
		{
			movdqa(l, src);
		}
		else if (src == l)
		{
			movdqa(h, src);
		}
		else
		{
			movdqa(l, src);
			movdqa(h, src);
		}
		psllw(l, 8);
		psrlw(l, 8);
		psrlw(h, 8);
	}
}
