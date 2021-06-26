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

#include "GSRegs.h"
#include "GSTables.h"
#include "GSVector.h"

class GSBlock
{
	static const GSVector4i m_r16mask;
	static const GSVector4i m_r8mask;
	static const GSVector4i m_r4mask;
	static const GSVector4i m_w4mask;
	static const GSVector4i m_r4hmask;
	static const GSVector4i m_r4hmask_avx2;
	static const GSVector4i m_palvec_mask;

	static const GSVector4i m_avx2_r8mask1;
	static const GSVector4i m_avx2_r8mask2;
	static const GSVector4i m_avx2_w8mask1;
	static const GSVector4i m_avx2_w8mask2;

	static const GSVector4i m_uw8hmask0;
	static const GSVector4i m_uw8hmask1;
	static const GSVector4i m_uw8hmask2;
	static const GSVector4i m_uw8hmask3;

#if _M_SSE >= 0x501
	// Equvialent of `a = *s0; b = *s1; sw128(a, b);`
	// Loads in two halves instead to reduce shuffle instructions
	// Especially good for Zen/Zen+, as it's replacing a very expensive vperm2i128
	template <bool Aligned = true>
	static void LoadSW128(GSVector8i& a, GSVector8i& b, const void* s0, const void* s1)
	{
		const GSVector4i* src0 = static_cast<const GSVector4i*>(s0);
		const GSVector4i* src1 = static_cast<const GSVector4i*>(s1);
		a = GSVector8i(GSVector4i::load<Aligned>(src0), GSVector4i::load<Aligned>(src1));
		b = GSVector8i(GSVector4i::load<Aligned>(src0 + 1), GSVector4i::load<Aligned>(src1 + 1));
	}
#endif

public:
	template <int i, int alignment, u32 mask>
	__forceinline static void WriteColumn32(u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		const u8* RESTRICT s0 = &src[srcpitch * 0];
		const u8* RESTRICT s1 = &src[srcpitch * 1];

#if _M_SSE >= 0x501

		GSVector8i v0 = GSVector8i::load<false>(s0).acbd();
		GSVector8i v1 = GSVector8i::load<false>(s1).acbd();

		GSVector8i::sw64(v0, v1);

		GSVector8i* d = reinterpret_cast<GSVector8i*>(dst);

		d[i * 2 + 0] = d[i * 2 + 0].smartblend<mask>(v0);
		d[i * 2 + 1] = d[i * 2 + 1].smartblend<mask>(v1);

#else

		GSVector4i v0, v1, v2, v3;

#if FAST_UNALIGNED

		v0 = GSVector4i::load<false>(&s0[0]);
		v1 = GSVector4i::load<false>(&s0[16]);
		v2 = GSVector4i::load<false>(&s1[0]);
		v3 = GSVector4i::load<false>(&s1[16]);

		GSVector4i::sw64(v0, v2, v1, v3);

#else

		if (alignment != 0)
		{
			v0 = GSVector4i::load<true>(&s0[0]);
			v1 = GSVector4i::load<true>(&s0[16]);
			v2 = GSVector4i::load<true>(&s1[0]);
			v3 = GSVector4i::load<true>(&s1[16]);

			GSVector4i::sw64(v0, v2, v1, v3);
		}
		else
		{
			v0 = GSVector4i::load(&s0[0], &s1[0]);
			v1 = GSVector4i::load(&s0[8], &s1[8]);
			v2 = GSVector4i::load(&s0[16], &s1[16]);
			v3 = GSVector4i::load(&s0[24], &s1[24]);
		}

#endif

		GSVector4i* d = reinterpret_cast<GSVector4i*>(dst);

		d[i * 4 + 0] = d[i * 4 + 0].smartblend<mask>(v0);
		d[i * 4 + 1] = d[i * 4 + 1].smartblend<mask>(v1);
		d[i * 4 + 2] = d[i * 4 + 2].smartblend<mask>(v2);
		d[i * 4 + 3] = d[i * 4 + 3].smartblend<mask>(v3);

#endif
	}

	template <int i, int alignment>
	__forceinline static void WriteColumn16(u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		const u8* RESTRICT s0 = &src[srcpitch * 0];
		const u8* RESTRICT s1 = &src[srcpitch * 1];

		// for(int j = 0; j < 16; j++) {((u16*)s0)[j] = columnTable16[0][j]; ((u16*)s1)[j] = columnTable16[1][j];}

#if _M_SSE >= 0x501

		GSVector8i v0, v1;

		LoadSW128<false>(v0, v1, s0, s1);
		GSVector8i::sw16(v0, v1);

		v0 = v0.acbd();
		v1 = v1.acbd();

		((GSVector8i*)dst)[i * 2 + 0] = v0;
		((GSVector8i*)dst)[i * 2 + 1] = v1;

#else

		GSVector4i v0, v1, v2, v3;

#if FAST_UNALIGNED

		v0 = GSVector4i::load<false>(&s0[0]);
		v1 = GSVector4i::load<false>(&s0[16]);
		v2 = GSVector4i::load<false>(&s1[0]);
		v3 = GSVector4i::load<false>(&s1[16]);

		GSVector4i::sw16(v0, v1, v2, v3);
		GSVector4i::sw64(v0, v1, v2, v3);

#else

		if (alignment != 0)
		{
			v0 = GSVector4i::load<true>(&s0[0]);
			v1 = GSVector4i::load<true>(&s0[16]);
			v2 = GSVector4i::load<true>(&s1[0]);
			v3 = GSVector4i::load<true>(&s1[16]);

			GSVector4i::sw16(v0, v1, v2, v3);
			GSVector4i::sw64(v0, v1, v2, v3);
		}
		else
		{
			v0 = GSVector4i::loadl(&s0[0]).upl16(GSVector4i::loadl(&s0[16]));
			v2 = GSVector4i::loadl(&s0[8]).upl16(GSVector4i::loadl(&s0[24]));
			v1 = GSVector4i::loadl(&s1[0]).upl16(GSVector4i::loadl(&s1[16]));
			v3 = GSVector4i::loadl(&s1[8]).upl16(GSVector4i::loadl(&s1[24]));

			GSVector4i::sw64(v0, v1, v2, v3);
		}

#endif

		((GSVector4i*)dst)[i * 4 + 0] = v0;
		((GSVector4i*)dst)[i * 4 + 1] = v2;
		((GSVector4i*)dst)[i * 4 + 2] = v1;
		((GSVector4i*)dst)[i * 4 + 3] = v3;

#endif
	}

	template <int i, int alignment>
	__forceinline static void WriteColumn8(u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		// TODO: read unaligned as WriteColumn32 does and try saving a few shuffles

#if _M_SSE >= 0x501

		GSVector4i v4 = GSVector4i::load<false>(&src[srcpitch * 0]);
		GSVector4i v5 = GSVector4i::load<false>(&src[srcpitch * 1]);
		GSVector4i v6 = GSVector4i::load<false>(&src[srcpitch * 2]);
		GSVector4i v7 = GSVector4i::load<false>(&src[srcpitch * 3]);

		GSVector8i v0(v4, v5);
		GSVector8i v1(v6, v7);

		GSVector8i v2 = v0.blend32<0xaa>(v1).shuffle8(GSVector8i::broadcast128(m_avx2_w8mask1)).acbd();
		GSVector8i v3 = v1.blend32<0xaa>(v0).shuffle8(GSVector8i::broadcast128(m_avx2_w8mask2)).acbd();

		if ((i & 1) == 0)
		{
			((GSVector8i*)dst)[i * 2 + 0] = v2;
			((GSVector8i*)dst)[i * 2 + 1] = v3;
		}
		else
		{
			((GSVector8i*)dst)[i * 2 + 0] = v3;
			((GSVector8i*)dst)[i * 2 + 1] = v2;
		}

#else

		GSVector4i v0 = GSVector4i::load<alignment != 0>(&src[srcpitch * 0]);
		GSVector4i v1 = GSVector4i::load<alignment != 0>(&src[srcpitch * 1]);
		GSVector4i v2 = GSVector4i::load<alignment != 0>(&src[srcpitch * 2]);
		GSVector4i v3 = GSVector4i::load<alignment != 0>(&src[srcpitch * 3]);

		if ((i & 1) == 0)
		{
			v2 = v2.yxwz();
			v3 = v3.yxwz();
		}
		else
		{
			v0 = v0.yxwz();
			v1 = v1.yxwz();
		}

		GSVector4i::sw8(v0, v2, v1, v3);
		GSVector4i::sw16(v0, v1, v2, v3);
		GSVector4i::sw64(v0, v1, v2, v3);

		((GSVector4i*)dst)[i * 4 + 0] = v0;
		((GSVector4i*)dst)[i * 4 + 1] = v2;
		((GSVector4i*)dst)[i * 4 + 2] = v1;
		((GSVector4i*)dst)[i * 4 + 3] = v3;

#endif
	}

	template <int i, int alignment>
	__forceinline static void WriteColumn4(u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		//printf("WriteColumn4\n");

		// TODO: read unaligned as WriteColumn32 does and try saving a few shuffles

		// TODO: pshufb

#if _M_SSE >= 0x501

		GSVector8i v0 = GSVector8i(GSVector4i::load<false>(&src[srcpitch * 0]), GSVector4i::load<false>(&src[srcpitch * 1]));
		GSVector8i v1 = GSVector8i(GSVector4i::load<false>(&src[srcpitch * 2]), GSVector4i::load<false>(&src[srcpitch * 3]));

		v0 = v0.shuffle8(GSVector8i::broadcast128(m_w4mask)).acbd();
		v1 = v1.shuffle8(GSVector8i::broadcast128(m_w4mask)).acbd();

		if ((i & 1) == 0)
		{
			v0 = v0.xzyw();
			v1 = v1.ywxz();
		}
		else
		{
			v0 = v0.ywxz();
			v1 = v1.xzyw();
		}

		GSVector8i::mix4(v0, v1);
		GSVector8i::sw32(v0, v1);

		((GSVector8i*)dst)[i * 2 + 0] = v0;
		((GSVector8i*)dst)[i * 2 + 1] = v1;

#else

		GSVector4i v0 = GSVector4i::load<alignment != 0>(&src[srcpitch * 0]);
		GSVector4i v1 = GSVector4i::load<alignment != 0>(&src[srcpitch * 1]);
		GSVector4i v2 = GSVector4i::load<alignment != 0>(&src[srcpitch * 2]);
		GSVector4i v3 = GSVector4i::load<alignment != 0>(&src[srcpitch * 3]);

		if ((i & 1) == 0)
		{
			v2 = v2.yxwzlh();
			v3 = v3.yxwzlh();
		}
		else
		{
			v0 = v0.yxwzlh();
			v1 = v1.yxwzlh();
		}

		GSVector4i::sw4(v0, v2, v1, v3);
		GSVector4i::sw8(v0, v1, v2, v3);
		GSVector4i::sw8(v0, v2, v1, v3);
		GSVector4i::sw64(v0, v2, v1, v3);

		((GSVector4i*)dst)[i * 4 + 0] = v0;
		((GSVector4i*)dst)[i * 4 + 1] = v1;
		((GSVector4i*)dst)[i * 4 + 2] = v2;
		((GSVector4i*)dst)[i * 4 + 3] = v3;

#endif
	}

	template <int alignment, u32 mask>
	static void WriteColumn32(int y, u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		switch ((y >> 1) & 3)
		{
			case 0: WriteColumn32<0, alignment, mask>(dst, src, srcpitch); break;
			case 1: WriteColumn32<1, alignment, mask>(dst, src, srcpitch); break;
			case 2: WriteColumn32<2, alignment, mask>(dst, src, srcpitch); break;
			case 3: WriteColumn32<3, alignment, mask>(dst, src, srcpitch); break;
			default: __assume(0);
		}
	}

	template <int alignment>
	static void WriteColumn16(int y, u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		switch ((y >> 1) & 3)
		{
			case 0: WriteColumn16<0, alignment>(dst, src, srcpitch); break;
			case 1: WriteColumn16<1, alignment>(dst, src, srcpitch); break;
			case 2: WriteColumn16<2, alignment>(dst, src, srcpitch); break;
			case 3: WriteColumn16<3, alignment>(dst, src, srcpitch); break;
			default: __assume(0);
		}
	}

	template <int alignment>
	static void WriteColumn8(int y, u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		switch ((y >> 2) & 3)
		{
			case 0: WriteColumn8<0, alignment>(dst, src, srcpitch); break;
			case 1: WriteColumn8<1, alignment>(dst, src, srcpitch); break;
			case 2: WriteColumn8<2, alignment>(dst, src, srcpitch); break;
			case 3: WriteColumn8<3, alignment>(dst, src, srcpitch); break;
			default: __assume(0);
		}
	}

	template <int alignment>
	static void WriteColumn4(int y, u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		switch ((y >> 2) & 3)
		{
			case 0: WriteColumn4<0, alignment>(dst, src, srcpitch); break;
			case 1: WriteColumn4<1, alignment>(dst, src, srcpitch); break;
			case 2: WriteColumn4<2, alignment>(dst, src, srcpitch); break;
			case 3: WriteColumn4<3, alignment>(dst, src, srcpitch); break;
			default: __assume(0);
		}
	}

	template <int alignment, u32 mask>
	static void WriteBlock32(u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		WriteColumn32<0, alignment, mask>(dst, src, srcpitch);
		src += srcpitch * 2;
		WriteColumn32<1, alignment, mask>(dst, src, srcpitch);
		src += srcpitch * 2;
		WriteColumn32<2, alignment, mask>(dst, src, srcpitch);
		src += srcpitch * 2;
		WriteColumn32<3, alignment, mask>(dst, src, srcpitch);
	}

	template <int alignment>
	static void WriteBlock16(u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		WriteColumn16<0, alignment>(dst, src, srcpitch);
		src += srcpitch * 2;
		WriteColumn16<1, alignment>(dst, src, srcpitch);
		src += srcpitch * 2;
		WriteColumn16<2, alignment>(dst, src, srcpitch);
		src += srcpitch * 2;
		WriteColumn16<3, alignment>(dst, src, srcpitch);
	}

	template <int alignment>
	static void WriteBlock8(u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		WriteColumn8<0, alignment>(dst, src, srcpitch);
		src += srcpitch * 4;
		WriteColumn8<1, alignment>(dst, src, srcpitch);
		src += srcpitch * 4;
		WriteColumn8<2, alignment>(dst, src, srcpitch);
		src += srcpitch * 4;
		WriteColumn8<3, alignment>(dst, src, srcpitch);
	}

	template <int alignment>
	static void WriteBlock4(u8* RESTRICT dst, const u8* RESTRICT src, int srcpitch)
	{
		WriteColumn4<0, alignment>(dst, src, srcpitch);
		src += srcpitch * 4;
		WriteColumn4<1, alignment>(dst, src, srcpitch);
		src += srcpitch * 4;
		WriteColumn4<2, alignment>(dst, src, srcpitch);
		src += srcpitch * 4;
		WriteColumn4<3, alignment>(dst, src, srcpitch);
	}

	template <int i>
	__forceinline static void ReadColumn32(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i v0, v1;

		LoadSW128(v0, v1, &s[i * 2 + 0], &s[i * 2 + 1]);
		GSVector8i::sw64(v0, v1);

		GSVector8i::store<true>(&dst[dstpitch * 0], v0);
		GSVector8i::store<true>(&dst[dstpitch * 1], v1);

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i v0 = s[i * 4 + 0];
		GSVector4i v1 = s[i * 4 + 1];
		GSVector4i v2 = s[i * 4 + 2];
		GSVector4i v3 = s[i * 4 + 3];

		GSVector4i::sw64(v0, v1, v2, v3);

		GSVector4i* d0 = (GSVector4i*)&dst[dstpitch * 0];
		GSVector4i* d1 = (GSVector4i*)&dst[dstpitch * 1];

		GSVector4i::store<true>(&d0[0], v0);
		GSVector4i::store<true>(&d0[1], v1);
		GSVector4i::store<true>(&d1[0], v2);
		GSVector4i::store<true>(&d1[1], v3);

#endif
	}

	template <int i>
	__forceinline static void ReadColumn16(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i mask = GSVector8i::broadcast128(m_r16mask);

		GSVector8 v0 = GSVector8::cast(s[i * 2 + 0].shuffle8(mask).acbd());
		GSVector8 v1 = GSVector8::cast(s[i * 2 + 1].shuffle8(mask).acbd());

		GSVector8::store<true>(&dst[dstpitch * 0], v0.xzxz(v1));
		GSVector8::store<true>(&dst[dstpitch * 1], v0.ywyw(v1));

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i v0 = s[i * 4 + 0];
		GSVector4i v1 = s[i * 4 + 1];
		GSVector4i v2 = s[i * 4 + 2];
		GSVector4i v3 = s[i * 4 + 3];

		GSVector4i::sw16(v0, v1, v2, v3);
		GSVector4i::sw32(v0, v1, v2, v3);
		GSVector4i::sw16(v0, v2, v1, v3);

		GSVector4i* d0 = (GSVector4i*)&dst[dstpitch * 0];
		GSVector4i* d1 = (GSVector4i*)&dst[dstpitch * 1];

		GSVector4i::store<true>(&d0[0], v0);
		GSVector4i::store<true>(&d0[1], v1);
		GSVector4i::store<true>(&d1[0], v2);
		GSVector4i::store<true>(&d1[1], v3);

#endif
	}

	template <int i>
	__forceinline static void ReadColumn8(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{

		//for(int j = 0; j < 64; j++) ((u8*)src)[j] = (u8)j;

#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i v0, v1;

		if ((i & 1) == 0)
		{
			v0 = s[i * 2 + 0];
			v1 = s[i * 2 + 1];
		}
		else
		{
			v1 = s[i * 2 + 0];
			v0 = s[i * 2 + 1];
		}

		GSVector8i v2 = v0.acbd().shuffle8(GSVector8i::broadcast128(m_avx2_r8mask1));
		GSVector8i v3 = v1.acbd().shuffle8(GSVector8i::broadcast128(m_avx2_r8mask2));

		v0 = v2.blend32<0xaa>(v3);
		v1 = v3.blend32<0xaa>(v2);

		GSVector8i::storel(&dst[dstpitch * 0], v0);
		GSVector8i::storeh(&dst[dstpitch * 1], v0);
		GSVector8i::storel(&dst[dstpitch * 2], v1);
		GSVector8i::storeh(&dst[dstpitch * 3], v1);

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i v0, v1, v2, v3;

		if ((i & 1) == 0)
		{
			v0 = s[i * 4 + 0];
			v1 = s[i * 4 + 1];
			v2 = s[i * 4 + 2];
			v3 = s[i * 4 + 3];
		}
		else
		{
			v2 = s[i * 4 + 0];
			v3 = s[i * 4 + 1];
			v0 = s[i * 4 + 2];
			v1 = s[i * 4 + 3];
		}

		v0 = v0.shuffle8(m_r8mask);
		v1 = v1.shuffle8(m_r8mask);
		v2 = v2.shuffle8(m_r8mask);
		v3 = v3.shuffle8(m_r8mask);

		GSVector4i::sw16(v0, v1, v2, v3);
		GSVector4i::sw32(v0, v1, v3, v2);

		GSVector4i::store<true>(&dst[dstpitch * 0], v0);
		GSVector4i::store<true>(&dst[dstpitch * 1], v3);
		GSVector4i::store<true>(&dst[dstpitch * 2], v1);
		GSVector4i::store<true>(&dst[dstpitch * 3], v2);

#endif
	}

	template <int i>
	__forceinline static void ReadColumn4(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		//printf("ReadColumn4\n");

#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i v0 = s[i * 2 + 0];
		GSVector8i v1 = s[i * 2 + 1];

		GSVector8i::sw32_inv(v0, v1);
		GSVector8i::mix4(v0, v1);

		if ((i & 1) == 0)
		{
			v0 = v0.xzyw();
			v1 = v1.zxwy();
		}
		else
		{
			v0 = v0.zxwy();
			v1 = v1.xzyw();
		}

		v0 = v0.acbd().shuffle8(GSVector8i::broadcast128(m_r4mask));
		v1 = v1.acbd().shuffle8(GSVector8i::broadcast128(m_r4mask));

		GSVector8i::storel(&dst[dstpitch * 0], v0);
		GSVector8i::storeh(&dst[dstpitch * 1], v0);
		GSVector8i::storel(&dst[dstpitch * 2], v1);
		GSVector8i::storeh(&dst[dstpitch * 3], v1);

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i v0 = s[i * 4 + 0];
		GSVector4i v1 = s[i * 4 + 1];
		GSVector4i v2 = s[i * 4 + 2];
		GSVector4i v3 = s[i * 4 + 3];

		GSVector4i::sw32_inv(v0, v1, v2, v3);
		GSVector4i::mix4(v0, v1);
		GSVector4i::mix4(v2, v3);

		GSVector4 v0f = GSVector4::cast(v0);
		GSVector4 v1f = GSVector4::cast(v1);
		GSVector4 v2f = GSVector4::cast(v2);
		GSVector4 v3f = GSVector4::cast(v3);

		if ((i & 1) == 0)
		{
			v0 = GSVector4i::cast(v0f.xzxz(v2f)).shuffle8(m_r4mask);
			v1 = GSVector4i::cast(v0f.ywyw(v2f)).shuffle8(m_r4mask);
			v2 = GSVector4i::cast(v1f.zxzx(v3f)).shuffle8(m_r4mask);
			v3 = GSVector4i::cast(v1f.wywy(v3f)).shuffle8(m_r4mask);
		}
		else
		{
			v0 = GSVector4i::cast(v0f.zxzx(v2f)).shuffle8(m_r4mask);
			v1 = GSVector4i::cast(v0f.wywy(v2f)).shuffle8(m_r4mask);
			v2 = GSVector4i::cast(v1f.xzxz(v3f)).shuffle8(m_r4mask);
			v3 = GSVector4i::cast(v1f.ywyw(v3f)).shuffle8(m_r4mask);
		}

		GSVector4i::store<true>(&dst[dstpitch * 0], v0);
		GSVector4i::store<true>(&dst[dstpitch * 1], v1);
		GSVector4i::store<true>(&dst[dstpitch * 2], v2);
		GSVector4i::store<true>(&dst[dstpitch * 3], v3);

#endif
	}

	static void ReadColumn32(int y, const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		switch ((y >> 1) & 3)
		{
			case 0: ReadColumn32<0>(src, dst, dstpitch); break;
			case 1: ReadColumn32<1>(src, dst, dstpitch); break;
			case 2: ReadColumn32<2>(src, dst, dstpitch); break;
			case 3: ReadColumn32<3>(src, dst, dstpitch); break;
			default: __assume(0);
		}
	}

	static void ReadColumn16(int y, const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		switch ((y >> 1) & 3)
		{
			case 0: ReadColumn16<0>(src, dst, dstpitch); break;
			case 1: ReadColumn16<1>(src, dst, dstpitch); break;
			case 2: ReadColumn16<2>(src, dst, dstpitch); break;
			case 3: ReadColumn16<3>(src, dst, dstpitch); break;
			default: __assume(0);
		}
	}

	static void ReadColumn8(int y, const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		switch ((y >> 2) & 3)
		{
			case 0: ReadColumn8<0>(src, dst, dstpitch); break;
			case 1: ReadColumn8<1>(src, dst, dstpitch); break;
			case 2: ReadColumn8<2>(src, dst, dstpitch); break;
			case 3: ReadColumn8<3>(src, dst, dstpitch); break;
			default: __assume(0);
		}
	}

	static void ReadColumn4(int y, const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		switch ((y >> 2) & 3)
		{
			case 0: ReadColumn4<0>(src, dst, dstpitch); break;
			case 1: ReadColumn4<1>(src, dst, dstpitch); break;
			case 2: ReadColumn4<2>(src, dst, dstpitch); break;
			case 3: ReadColumn4<3>(src, dst, dstpitch); break;
			default: __assume(0);
		}
	}

	static void ReadBlock32(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		ReadColumn32<0>(src, dst, dstpitch);
		dst += dstpitch * 2;
		ReadColumn32<1>(src, dst, dstpitch);
		dst += dstpitch * 2;
		ReadColumn32<2>(src, dst, dstpitch);
		dst += dstpitch * 2;
		ReadColumn32<3>(src, dst, dstpitch);
	}

	static void ReadBlock16(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		ReadColumn16<0>(src, dst, dstpitch);
		dst += dstpitch * 2;
		ReadColumn16<1>(src, dst, dstpitch);
		dst += dstpitch * 2;
		ReadColumn16<2>(src, dst, dstpitch);
		dst += dstpitch * 2;
		ReadColumn16<3>(src, dst, dstpitch);
	}

	static void ReadBlock8(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		ReadColumn8<0>(src, dst, dstpitch);
		dst += dstpitch * 4;
		ReadColumn8<1>(src, dst, dstpitch);
		dst += dstpitch * 4;
		ReadColumn8<2>(src, dst, dstpitch);
		dst += dstpitch * 4;
		ReadColumn8<3>(src, dst, dstpitch);
	}

	static void ReadBlock4(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		ReadColumn4<0>(src, dst, dstpitch);
		dst += dstpitch * 4;
		ReadColumn4<1>(src, dst, dstpitch);
		dst += dstpitch * 4;
		ReadColumn4<2>(src, dst, dstpitch);
		dst += dstpitch * 4;
		ReadColumn4<3>(src, dst, dstpitch);
	}

	__forceinline static void ReadBlock4P(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		//printf("ReadBlock4P\n");

#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i v0, v1;

		GSVector8i shuf = GSVector8i::broadcast128(m_palvec_mask);
		GSVector8i mask(0x0f0f0f0f);

		for (int i = 0; i < 2; i++)
		{
			// col 0, 2

			v0 = s[i * 4 + 0];
			v1 = s[i * 4 + 1];

			GSVector8i::sw8(v0, v1);
			v0 = v0.xzyw().acbd().shuffle8(shuf);
			v1 = v1.xzyw().acbd().shuffle8(shuf);

			GSVector8i::store<true>(dst + dstpitch * 0, v0 & mask);
			GSVector8i::store<true>(dst + dstpitch * 1, v1 & mask);
			GSVector8i::store<true>(dst + dstpitch * 2, (v0.yxwz() >> 4) & mask);
			GSVector8i::store<true>(dst + dstpitch * 3, (v1.yxwz() >> 4) & mask);

			dst += dstpitch * 4;

			// col 1, 3

			v0 = s[i * 4 + 2];
			v1 = s[i * 4 + 3];

			GSVector8i::sw8(v0, v1);
			v0 = v0.xzyw().acbd().shuffle8(shuf);
			v1 = v1.xzyw().acbd().shuffle8(shuf);

			GSVector8i::store<true>(dst + dstpitch * 0, v0.yxwz() & mask);
			GSVector8i::store<true>(dst + dstpitch * 1, v1.yxwz() & mask);
			GSVector8i::store<true>(dst + dstpitch * 2, (v0 >> 4) & mask);
			GSVector8i::store<true>(dst + dstpitch * 3, (v1 >> 4) & mask);

			dst += dstpitch * 4;
		}

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i v0, v1, v2, v3;

		GSVector4i mask(0x0f0f0f0f);

		for (int i = 0; i < 2; i++)
		{
			// col 0, 2

			v0 = s[i * 8 + 0];
			v1 = s[i * 8 + 1];
			v2 = s[i * 8 + 2];
			v3 = s[i * 8 + 3];

			GSVector4i::sw8(v0, v1, v2, v3);
			GSVector4i::sw16(v0, v1, v2, v3);
			GSVector4i::sw8(v0, v2, v1, v3);

			GSVector4i::store<true>(&dst[dstpitch * 0 +  0], (v0 & mask));
			GSVector4i::store<true>(&dst[dstpitch * 0 + 16], (v1 & mask));
			GSVector4i::store<true>(&dst[dstpitch * 1 +  0], (v2 & mask));
			GSVector4i::store<true>(&dst[dstpitch * 1 + 16], (v3 & mask));

			dst += dstpitch * 2;

			GSVector4i::store<true>(&dst[dstpitch * 0 +  0], (v0.andnot(mask)).yxwz() >> 4);
			GSVector4i::store<true>(&dst[dstpitch * 0 + 16], (v1.andnot(mask)).yxwz() >> 4);
			GSVector4i::store<true>(&dst[dstpitch * 1 +  0], (v2.andnot(mask)).yxwz() >> 4);
			GSVector4i::store<true>(&dst[dstpitch * 1 + 16], (v3.andnot(mask)).yxwz() >> 4);

			dst += dstpitch * 2;

			// col 1, 3

			v0 = s[i * 8 + 4];
			v1 = s[i * 8 + 5];
			v2 = s[i * 8 + 6];
			v3 = s[i * 8 + 7];

			GSVector4i::sw8(v0, v1, v2, v3);
			GSVector4i::sw16(v0, v1, v2, v3);
			GSVector4i::sw8(v0, v2, v1, v3);

			GSVector4i::store<true>(&dst[dstpitch * 0 +  0], (v0 & mask).yxwz());
			GSVector4i::store<true>(&dst[dstpitch * 0 + 16], (v1 & mask).yxwz());
			GSVector4i::store<true>(&dst[dstpitch * 1 +  0], (v2 & mask).yxwz());
			GSVector4i::store<true>(&dst[dstpitch * 1 + 16], (v3 & mask).yxwz());

			dst += dstpitch * 2;

			GSVector4i::store<true>(&dst[dstpitch * 0 +  0], (v0.andnot(mask)) >> 4);
			GSVector4i::store<true>(&dst[dstpitch * 0 + 16], (v1.andnot(mask)) >> 4);
			GSVector4i::store<true>(&dst[dstpitch * 1 +  0], (v2.andnot(mask)) >> 4);
			GSVector4i::store<true>(&dst[dstpitch * 1 + 16], (v3.andnot(mask)) >> 4);

			dst += dstpitch * 2;
		}

#endif
	}

	template <u32 shift, u32 mask>
	__forceinline static void ReadBlockHP(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i v0, v1, v2, v3;
		GSVector4i v4, v5;

		GSVector8i maskvec = GSVector8i(mask);

		for (int i = 0; i < 2; i++)
		{
			v0 = s[i * 4 + 0].acbd();
			v1 = s[i * 4 + 1].acbd();
			v2 = s[i * 4 + 2].acbd();
			v3 = s[i * 4 + 3].acbd();

			v0 = (v0 >> shift).ps32(v1 >> shift).pu16((v2 >> shift).ps32(v3 >> shift));

			if (mask != 0xffffffff)
				v0 = v0 & maskvec;

			v4 = v0.extract<0>();
			v5 = v0.extract<1>();

			GSVector4i::storel(&dst[dstpitch * 0], v4);
			GSVector4i::storel(&dst[dstpitch * 1], v5);
			GSVector4i::storeh(&dst[dstpitch * 2], v4);
			GSVector4i::storeh(&dst[dstpitch * 3], v5);

			dst += dstpitch * 4;
		}

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i v0, v1, v2, v3;

		GSVector4i maskvec(mask);

		for (int i = 0; i < 4; i++)
		{
			v0 = s[i * 4 + 0];
			v1 = s[i * 4 + 1];
			v2 = s[i * 4 + 2];
			v3 = s[i * 4 + 3];

			GSVector4i::sw64(v0, v1, v2, v3);

			v0 = ((v0 >> shift).ps32(v1 >> shift)).pu16((v2 >> shift).ps32(v3 >> shift));

			if (mask != 0xffffffff)
				v0 = v0 & maskvec;

			GSVector4i::storel(dst, v0);

			dst += dstpitch;

			GSVector4i::storeh(dst, v0);

			dst += dstpitch;
		}

#endif
	}

	__forceinline static void ReadBlock8HP(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		ReadBlockHP<24, 0xffffffff>(src, dst, dstpitch);
	}

	__forceinline static void ReadBlock4HLP(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		ReadBlockHP<24, 0x0f0f0f0f>(src, dst, dstpitch);
	}

	__forceinline static void ReadBlock4HHP(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch)
	{
		ReadBlockHP<28, 0xffffffff>(src, dst, dstpitch);
	}

	template <bool AEM, class V>
	__forceinline static V Expand24to32(const V& c, const V& TA0)
	{
		return c | (AEM ? TA0.andnot(c == V::zero()) : TA0); // TA0 & (c != GSVector4i::zero())
	}

	/// Expands the 16bpp pixel duplicated across both halves of each dword to a 32bpp pixel
	template <bool AEM, class V>
	__forceinline static V Expand16to32(const V& c, const V& TA0, const V& TA1)
	{
		V rmask = V(0x000000f8);
		V gmask = V(0x0000f800);
		V bmask = V(0x00f80000);
		return ((c << 3) & rmask) | ((c << 6) & gmask) | ((c << 9) & bmask) | (AEM ? TA0.blend8(TA1, c).andnot(c == V::zero()) : TA0.blend8(TA1, c));
	}

	/// Expands the 16bpp pixel in the low half of each dword to a 32bpp pixel
	template <bool AEM, class V>
	__forceinline static V Expand16Lto32(const V& c, const V& TA0, const V& TA1)
	{
		V rmask = V(0x000000f8);
		V gmask = V(0x0000f800);
		V bmask = V(0x00f80000);
		V o = ((c << 3) & rmask) | ((c << 6) & gmask) | ((c << 9) & bmask);
		V ta0 = AEM ? TA0.andnot(o == V::zero()) : TA0;
		return o | ta0.blend8(TA1, c << 16);
	}

	/// Expands the 16bpp pixel in the high half of each dword to a 32bpp pixel
	template <bool AEM, class V>
	__forceinline static V Expand16Hto32(const V& c, const V& TA0, const V& TA1)
	{
		V rmask = V(0x000000f8);
		V gmask = V(0x0000f800);
		V bmask = V(0x00f80000);
		V o = ((c >> 13) & rmask) | ((c >> 10) & gmask) | ((c >> 7) & bmask);
		V ta0 = AEM ? TA0.andnot(o == V::zero()) : TA0;
		return o | ta0.blend8(TA1, c);
	}

	template <bool AEM>
	static void ExpandBlock24(const u32* RESTRICT src, u8* RESTRICT dst, int dstpitch, const GIFRegTEXA& TEXA)
	{
#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i TA0(TEXA.TA0 << 24);
		GSVector8i mask = GSVector8i::x00ffffff();

		for (int i = 0; i < 4; i++, dst += dstpitch * 2)
		{
			GSVector8i v0 = s[i * 2 + 0] & mask;
			GSVector8i v1 = s[i * 2 + 1] & mask;

			GSVector8i* d0 = (GSVector8i*)&dst[dstpitch * 0];
			GSVector8i* d1 = (GSVector8i*)&dst[dstpitch * 1];

			d0[0] = Expand24to32<AEM>(v0, TA0);
			d1[0] = Expand24to32<AEM>(v1, TA0);
		}

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i TA0(TEXA.TA0 << 24);
		GSVector4i mask = GSVector4i::x00ffffff();

		for (int i = 0; i < 4; i++, dst += dstpitch * 2)
		{
			GSVector4i v0 = s[i * 4 + 0] & mask;
			GSVector4i v1 = s[i * 4 + 1] & mask;
			GSVector4i v2 = s[i * 4 + 2] & mask;
			GSVector4i v3 = s[i * 4 + 3] & mask;

			GSVector4i* d0 = (GSVector4i*)&dst[dstpitch * 0];
			GSVector4i* d1 = (GSVector4i*)&dst[dstpitch * 1];

			d0[0] = Expand24to32<AEM>(v0, TA0);
			d0[1] = Expand24to32<AEM>(v1, TA0);
			d1[0] = Expand24to32<AEM>(v2, TA0);
			d1[1] = Expand24to32<AEM>(v3, TA0);
		}

#endif
	}

	template <bool AEM>
	static void ExpandBlock16(const u16* RESTRICT src, u8* RESTRICT dst, int dstpitch, const GIFRegTEXA& TEXA) // do not inline, uses too many xmm regs
	{
#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i TA0(TEXA.TA0 << 24);
		GSVector8i TA1(TEXA.TA1 << 24);

		for (int i = 0; i < 8; i++, dst += dstpitch)
		{
			GSVector8i v = s[i].acbd();

			((GSVector8i*)dst)[0] = Expand16to32<AEM>(v.upl16(v), TA0, TA1);
			((GSVector8i*)dst)[1] = Expand16to32<AEM>(v.uph16(v), TA0, TA1);
		}

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i TA0(TEXA.TA0 << 24);
		GSVector4i TA1(TEXA.TA1 << 24);

		for (int i = 0; i < 8; i++, dst += dstpitch)
		{
			GSVector4i v0 = s[i * 2 + 0];

			((GSVector4i*)dst)[0] = Expand16to32<AEM>(v0.upl16(v0), TA0, TA1);
			((GSVector4i*)dst)[1] = Expand16to32<AEM>(v0.uph16(v0), TA0, TA1);

			GSVector4i v1 = s[i * 2 + 1];

			((GSVector4i*)dst)[2] = Expand16to32<AEM>(v1.upl16(v1), TA0, TA1);
			((GSVector4i*)dst)[3] = Expand16to32<AEM>(v1.uph16(v1), TA0, TA1);
		}

#endif
	}

	__forceinline static void ExpandBlock8_32(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		for (int j = 0; j < 16; j++, dst += dstpitch)
		{
			for (int k = 0; k < 4; k++)
			{
				const u8* s = src + j * 16 + k * 4;
				GSVector4i v = GSVector4i(pal[s[0]], pal[s[1]], pal[s[2]], pal[s[3]]);
				reinterpret_cast<GSVector4i*>(dst)[k] = v;
			}
		}
	}

	__forceinline static void ExpandBlock8_16(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		for (int j = 0; j < 16; j++, dst += dstpitch)
		{
			((const GSVector4i*)src)[j].gather16_8(pal, (GSVector4i*)dst);
		}
	}

	__forceinline static void ExpandBlock4_32(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u64* RESTRICT pal)
	{
		for (int j = 0; j < 16; j++, dst += dstpitch)
		{
			((const GSVector4i*)src)[j].gather64_8(pal, (GSVector4i*)dst);
		}
	}

	__forceinline static void ExpandBlock4_16(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u64* RESTRICT pal)
	{
		for (int j = 0; j < 16; j++, dst += dstpitch)
		{
			((const GSVector4i*)src)[j].gather32_8(pal, (GSVector4i*)dst);
		}
	}

	template <u32 shift, u8 mask>
	__forceinline static void ExpandBlockH_32(u32* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		for (int j = 0; j < 8; j++, dst += dstpitch)
		{
			const GSVector4i* s = (const GSVector4i*)src;

			GSVector4i v0 = s[j * 2 + 0] >> shift;
			GSVector4i v1 = s[j * 2 + 1] >> shift;
			if (mask != 0xff)
			{
				v0 = v0 & mask;
				v1 = v1 & mask;
			}
			((GSVector4i*)dst)[0] = v0.gather32_32<>(pal);
			((GSVector4i*)dst)[1] = v1.gather32_32<>(pal);
		}
	}

	template <u32 shift, u8 mask>
	__forceinline static void ExpandBlockH_16(u32* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		for (int j = 0; j < 8; j++, dst += dstpitch)
		{
			const GSVector4i* s = (const GSVector4i*)src;

			GSVector4i v0 = s[j * 2 + 0] >> shift;
			GSVector4i v1 = s[j * 2 + 1] >> shift;
			if (mask != 0xff)
			{
				v0 = v0 & mask;
				v1 = v1 & mask;
			}
			v0 = v0.gather32_32<>(pal);
			v1 = v1.gather32_32<>(pal);

			((GSVector4i*)dst)[0] = v0.pu32(v1);
		}
	}

	__forceinline static void ExpandBlock8H_32(u32* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		ExpandBlockH_32<24, 0xff>(src, dst, dstpitch, pal);
	}

	__forceinline static void ExpandBlock8H_16(u32* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		ExpandBlockH_16<24, 0xff>(src, dst, dstpitch, pal);
	}

	__forceinline static void ExpandBlock4HL_32(u32* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		ExpandBlockH_32<24, 0x0f>(src, dst, dstpitch, pal);
	}

	__forceinline static void ExpandBlock4HL_16(u32* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		ExpandBlockH_16<24, 0x0f>(src, dst, dstpitch, pal);
	}

	__forceinline static void ExpandBlock4HH_32(u32* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		ExpandBlockH_32<28, 0xff>(src, dst, dstpitch, pal);
	}

	__forceinline static void ExpandBlock4HH_16(u32* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		ExpandBlockH_16<28, 0xff>(src, dst, dstpitch, pal);
	}

	__forceinline static void UnpackAndWriteBlock24(const u8* RESTRICT src, int srcpitch, u8* RESTRICT dst)
	{
#if _M_SSE >= 0x501

		const u8* RESTRICT s0 = &src[srcpitch * 0];
		const u8* RESTRICT s1 = &src[srcpitch * 1];
		const u8* RESTRICT s2 = &src[srcpitch * 2];
		const u8* RESTRICT s3 = &src[srcpitch * 3];

		GSVector8i v0, v1, v2, v3, v4, v5, v6;
		GSVector8i mask = GSVector8i::x00ffffff();

		v4 = GSVector8i::load(s0, s0 + 8, s2, s2 + 8);
		v5 = GSVector8i::load(s0 + 16, s1, s2 + 16, s3);
		v6 = GSVector8i::load(s1 + 8, s1 + 16, s3 + 8, s3 + 16);

		v0 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>())).acbd();
		v4 = v4.srl<12>(v5);
		v1 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>())).acbd();
		v4 = v5.srl<8>(v6);
		v2 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>())).acbd();
		v4 = v6.srl<4>();
		v3 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>())).acbd();

		GSVector8i::sw64(v0, v2, v1, v3);

		((GSVector8i*)dst)[0] = ((GSVector8i*)dst)[0].blend8(v0, mask);
		((GSVector8i*)dst)[1] = ((GSVector8i*)dst)[1].blend8(v2, mask);
		((GSVector8i*)dst)[2] = ((GSVector8i*)dst)[2].blend8(v1, mask);
		((GSVector8i*)dst)[3] = ((GSVector8i*)dst)[3].blend8(v3, mask);

		src += srcpitch * 4;

		s0 = &src[srcpitch * 0];
		s1 = &src[srcpitch * 1];
		s2 = &src[srcpitch * 2];
		s3 = &src[srcpitch * 3];

		v4 = GSVector8i::load(s0, s0 + 8, s2, s2 + 8);
		v5 = GSVector8i::load(s0 + 16, s1, s2 + 16, s3);
		v6 = GSVector8i::load(s1 + 8, s1 + 16, s3 + 8, s3 + 16);

		v0 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>())).acbd();
		v4 = v4.srl<12>(v5);
		v1 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>())).acbd();
		v4 = v5.srl<8>(v6);
		v2 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>())).acbd();
		v4 = v6.srl<4>();
		v3 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>())).acbd();

		GSVector8i::sw64(v0, v2, v1, v3);

		((GSVector8i*)dst)[4] = ((GSVector8i*)dst)[4].blend8(v0, mask);
		((GSVector8i*)dst)[5] = ((GSVector8i*)dst)[5].blend8(v2, mask);
		((GSVector8i*)dst)[6] = ((GSVector8i*)dst)[6].blend8(v1, mask);
		((GSVector8i*)dst)[7] = ((GSVector8i*)dst)[7].blend8(v3, mask);

#else

		GSVector4i v0, v1, v2, v3, v4, v5, v6;
		GSVector4i mask = GSVector4i::x00ffffff();

		for (int i = 0; i < 4; i++, src += srcpitch * 2)
		{
			v4 = GSVector4i::load<false>(src);
			v5 = GSVector4i::load(src + 16, src + srcpitch);
			v6 = GSVector4i::load<false>(src + srcpitch + 8);

			v0 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>()));
			v4 = v4.srl<12>(v5);
			v1 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>()));
			v4 = v5.srl<8>(v6);
			v2 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>()));
			v4 = v6.srl<4>();
			v3 = v4.upl32(v4.srl<3>()).upl64(v4.srl<6>().upl32(v4.srl<9>()));

			GSVector4i::sw64(v0, v2, v1, v3);

			((GSVector4i*)dst)[i * 4 + 0] = ((GSVector4i*)dst)[i * 4 + 0].blend8(v0, mask);
			((GSVector4i*)dst)[i * 4 + 1] = ((GSVector4i*)dst)[i * 4 + 1].blend8(v1, mask);
			((GSVector4i*)dst)[i * 4 + 2] = ((GSVector4i*)dst)[i * 4 + 2].blend8(v2, mask);
			((GSVector4i*)dst)[i * 4 + 3] = ((GSVector4i*)dst)[i * 4 + 3].blend8(v3, mask);
		}

#endif
	}

	template <u32 mask>
	__forceinline static void UnpackAndWriteBlockH(const u8* RESTRICT src, int srcpitch, u8* RESTRICT dst)
	{
		GSVector4i v4, v5, v6, v7;

#if _M_SSE >= 0x501

		GSVector8i* d = reinterpret_cast<GSVector8i*>(dst);

		for (int i = 0; i < 2; i++, src += srcpitch * 4, d += 4)
		{
			if (mask == 0xff000000)
			{
				v4 = GSVector4i::loadl(&src[srcpitch * 0]);
				v5 = GSVector4i::loadl(&src[srcpitch * 1]);
				v6 = GSVector4i::loadl(&src[srcpitch * 2]);
				v7 = GSVector4i::loadl(&src[srcpitch * 3]);

				v4 = v4.upl16(v5);
				v5 = v6.upl16(v7);
			}
			else
			{
				v4 = GSVector4i::load(*(u32*)&src[srcpitch * 0]).upl32(GSVector4i::load(*(u32*)&src[srcpitch * 2]));
				v5 = GSVector4i::load(*(u32*)&src[srcpitch * 1]).upl32(GSVector4i::load(*(u32*)&src[srcpitch * 3]));

				if (mask == 0x0f000000)
				{
					v6 = v4.upl8(v4 >> 4);
					v7 = v5.upl8(v5 >> 4);
				}
				else if (mask == 0xf0000000)
				{
					v6 = (v4 << 4).upl8(v4);
					v7 = (v5 << 4).upl8(v5);
				}
				else
				{
					ASSERT(0);
				}

				v4 = v6.upl16(v7);
				v5 = v6.uph16(v7);
			}

			GSVector8i v0 = GSVector8i::u8to32(v4) << 24;
			GSVector8i v1 = GSVector8i::u8to32(v4.zwzw()) << 24;
			GSVector8i v2 = GSVector8i::u8to32(v5) << 24;
			GSVector8i v3 = GSVector8i::u8to32(v5.zwzw()) << 24;

			d[0] = d[0].smartblend<mask>(v0);
			d[1] = d[1].smartblend<mask>(v1);
			d[2] = d[2].smartblend<mask>(v2);
			d[3] = d[3].smartblend<mask>(v3);
		}

#else

		GSVector4i v0, v1, v2, v3;

		GSVector4i mask0 = m_uw8hmask0;
		GSVector4i mask1 = m_uw8hmask1;
		GSVector4i mask2 = m_uw8hmask2;
		GSVector4i mask3 = m_uw8hmask3;

		GSVector4i* d = reinterpret_cast<GSVector4i*>(dst);

		for (int i = 0; i < 2; i++, src += srcpitch * 4, d += 8)
		{
			if (mask == 0xff000000)
			{
				v4 = GSVector4i::load(src + srcpitch * 0, src + srcpitch * 1);
				v5 = GSVector4i::load(src + srcpitch * 2, src + srcpitch * 3);
			}
			else
			{
				v6 = GSVector4i::load(*(u32*)&src[srcpitch * 0]);
				v7 = GSVector4i::load(*(u32*)&src[srcpitch * 1]);
				v4 = v6.upl32(v7);
				v6 = GSVector4i::load(*(u32*)&src[srcpitch * 2]);
				v7 = GSVector4i::load(*(u32*)&src[srcpitch * 3]);
				v5 = v6.upl32(v7);

				if (mask == 0x0f000000)
				{
					v4 = v4.upl8(v4 >> 4);
					v5 = v5.upl8(v5 >> 4);
				}
				else if (mask == 0xf0000000)
				{
					v4 = (v4 << 4).upl8(v4);
					v5 = (v5 << 4).upl8(v5);
				}
				else
				{
					ASSERT(0);
				}
			}

			v0 = v4.shuffle8(mask0);
			v1 = v4.shuffle8(mask1);
			v2 = v4.shuffle8(mask2);
			v3 = v4.shuffle8(mask3);

			d[0] = d[0].smartblend<mask>(v0);
			d[1] = d[1].smartblend<mask>(v1);
			d[2] = d[2].smartblend<mask>(v2);
			d[3] = d[3].smartblend<mask>(v3);

			v0 = v5.shuffle8(mask0);
			v1 = v5.shuffle8(mask1);
			v2 = v5.shuffle8(mask2);
			v3 = v5.shuffle8(mask3);

			d[4] = d[4].smartblend<mask>(v0);
			d[5] = d[5].smartblend<mask>(v1);
			d[6] = d[6].smartblend<mask>(v2);
			d[7] = d[7].smartblend<mask>(v3);
		}

#endif
	}

	__forceinline static void UnpackAndWriteBlock8H(const u8* RESTRICT src, int srcpitch, u8* RESTRICT dst)
	{
		UnpackAndWriteBlockH<0xff000000>(src, srcpitch, dst);
	}

	__forceinline static void UnpackAndWriteBlock4HL(const u8* RESTRICT src, int srcpitch, u8* RESTRICT dst)
	{
		//printf("4HL\n");

		if (0)
		{
			u8* s = (u8*)src;
			for (int j = 0; j < 8; j++, s += srcpitch)
				for (int i = 0; i < 4; i++)
					s[i] = (columnTable32[j][i * 2] & 0x0f) | (columnTable32[j][i * 2 + 1] << 4);
		}

		UnpackAndWriteBlockH<0x0f000000>(src, srcpitch, dst);
	}

	__forceinline static void UnpackAndWriteBlock4HH(const u8* RESTRICT src, int srcpitch, u8* RESTRICT dst)
	{
		UnpackAndWriteBlockH<0xf0000000>(src, srcpitch, dst);
	}

	template <bool AEM>
	__forceinline static void ReadAndExpandBlock24(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const GIFRegTEXA& TEXA)
	{
#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i TA0(TEXA.TA0 << 24);
		GSVector8i mask = GSVector8i::x00ffffff();

		GSVector8i v0, v1, v2, v3;

		v0 = s[0] & mask;
		v1 = s[1] & mask;
		v2 = s[2] & mask;
		v3 = s[3] & mask;

		GSVector8i::sw128(v0, v1);
		GSVector8i::sw64(v0, v1);
		GSVector8i::sw128(v2, v3);
		GSVector8i::sw64(v2, v3);

		*(GSVector8i*)&dst[dstpitch * 0] = Expand24to32<AEM>(v0, TA0);
		*(GSVector8i*)&dst[dstpitch * 1] = Expand24to32<AEM>(v1, TA0);
		*(GSVector8i*)&dst[dstpitch * 2] = Expand24to32<AEM>(v2, TA0);
		*(GSVector8i*)&dst[dstpitch * 3] = Expand24to32<AEM>(v3, TA0);

		v0 = s[4] & mask;
		v1 = s[5] & mask;
		v2 = s[6] & mask;
		v3 = s[7] & mask;

		GSVector8i::sw128(v0, v1);
		GSVector8i::sw64(v0, v1);
		GSVector8i::sw128(v2, v3);
		GSVector8i::sw64(v2, v3);

		dst += dstpitch * 4;

		*(GSVector8i*)&dst[dstpitch * 0] = Expand24to32<AEM>(v0, TA0);
		*(GSVector8i*)&dst[dstpitch * 1] = Expand24to32<AEM>(v1, TA0);
		*(GSVector8i*)&dst[dstpitch * 2] = Expand24to32<AEM>(v2, TA0);
		*(GSVector8i*)&dst[dstpitch * 3] = Expand24to32<AEM>(v3, TA0);

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i TA0(TEXA.TA0 << 24);
		GSVector4i mask = GSVector4i::x00ffffff();

		for (int i = 0; i < 4; i++, dst += dstpitch * 2)
		{
			GSVector4i v0 = s[i * 4 + 0];
			GSVector4i v1 = s[i * 4 + 1];
			GSVector4i v2 = s[i * 4 + 2];
			GSVector4i v3 = s[i * 4 + 3];

			GSVector4i::sw64(v0, v1, v2, v3);

			v0 &= mask;
			v1 &= mask;
			v2 &= mask;
			v3 &= mask;

			GSVector4i* d0 = (GSVector4i*)&dst[dstpitch * 0];
			GSVector4i* d1 = (GSVector4i*)&dst[dstpitch * 1];

			d0[0] = Expand24to32<AEM>(v0, TA0);
			d0[1] = Expand24to32<AEM>(v1, TA0);
			d1[0] = Expand24to32<AEM>(v2, TA0);
			d1[1] = Expand24to32<AEM>(v3, TA0);
		}

#endif
	}

	template <bool AEM>
	__forceinline static void ReadAndExpandBlock16(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const GIFRegTEXA& TEXA)
	{
#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i TA0(TEXA.TA0 << 24);
		GSVector8i TA1(TEXA.TA1 << 24);

		for (int i = 0; i < 4; i++, dst += dstpitch * 2)
		{
			GSVector8i v0, v1;

			LoadSW128(v0, v1, &s[i * 2 + 0], &s[i * 2 + 1]);
			GSVector8i::sw64(v0, v1);

			GSVector8i* d0 = (GSVector8i*)&dst[dstpitch * 0];
			GSVector8i* d1 = (GSVector8i*)&dst[dstpitch * 1];

			d0[0] = Expand16Lto32<AEM>(v0, TA0, TA1);
			d0[1] = Expand16Hto32<AEM>(v0, TA0, TA1);
			d1[0] = Expand16Lto32<AEM>(v1, TA0, TA1);
			d1[1] = Expand16Hto32<AEM>(v1, TA0, TA1);
		}

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i TA0(TEXA.TA0 << 24);
		GSVector4i TA1(TEXA.TA1 << 24);

		for (int i = 0; i < 4; i++, dst += dstpitch * 2)
		{
			GSVector4i v0 = s[i * 4 + 0];
			GSVector4i v1 = s[i * 4 + 1];
			GSVector4i v2 = s[i * 4 + 2];
			GSVector4i v3 = s[i * 4 + 3];

			GSVector4i::sw64(v0, v1, v2, v3);

			GSVector4i* d0 = (GSVector4i*)&dst[dstpitch * 0];

			d0[0] = Expand16Lto32<AEM>(v0, TA0, TA1);
			d0[1] = Expand16Lto32<AEM>(v1, TA0, TA1);
			d0[2] = Expand16Hto32<AEM>(v0, TA0, TA1);
			d0[3] = Expand16Hto32<AEM>(v1, TA0, TA1);

			GSVector4i* d1 = (GSVector4i*)&dst[dstpitch * 1];

			d1[0] = Expand16Lto32<AEM>(v2, TA0, TA1);
			d1[1] = Expand16Lto32<AEM>(v3, TA0, TA1);
			d1[2] = Expand16Hto32<AEM>(v2, TA0, TA1);
			d1[3] = Expand16Hto32<AEM>(v3, TA0, TA1);
		}

#endif
	}

	/// ReadAndExpandBlock8 for AVX2 platforms with slow VPGATHERDD (Haswell, Zen, Zen2, Zen3)
	/// This is faster than the one in ReadAndExpandBlock8_32 on HSW+ due to a port 5 traffic jam, should be about the same on Zen
	__forceinline static void ReadAndExpandBlock8_32HSW(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		alignas(32) u8 block[16 * 16];
		ReadBlock8(src, (u8*)block, sizeof(block) / 16);
		ExpandBlock8_32(block, dst, dstpitch, pal);
	}

	__forceinline static void ReadAndExpandBlock8_32(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		//printf("ReadAndExpandBlock8_32\n");

#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i v0, v1;
		GSVector8i mask = GSVector8i::x000000ff();

		for (int i = 0; i < 2; i++)
		{
			GSVector8i* d0 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 0);
			GSVector8i* d1 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 1);
			GSVector8i* d2 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 2);
			GSVector8i* d3 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 3);

			LoadSW128(v0, v1, &s[i * 4 + 0], &s[i * 4 + 1]);
			GSVector8i::sw64(v0, v1);

			d0[0] = ((v0      ) & mask).gather32_32(pal);
			d0[1] = ((v0 >> 16) & mask).gather32_32(pal);
			d1[0] = ((v1      ) & mask).gather32_32(pal);
			d1[1] = ((v1 >> 16) & mask).gather32_32(pal);
			v0 = v0.cdab();
			v1 = v1.cdab();
			d2[0] = ((v0 >>  8) & mask).gather32_32(pal);
			d2[1] = ((v0 >> 24)       ).gather32_32(pal);
			d3[0] = ((v1 >>  8) & mask).gather32_32(pal);
			d3[1] = ((v1 >> 24)       ).gather32_32(pal);

			dst += dstpitch * 4;

			d0 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 0);
			d1 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 1);
			d2 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 2);
			d3 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 3);

			LoadSW128(v0, v1, &s[i * 4 + 3], &s[i * 4 + 2]);
			GSVector8i::sw64(v0, v1);

			d0[0] = ((v0      ) & mask).gather32_32(pal);
			d0[1] = ((v0 >> 16) & mask).gather32_32(pal);
			d1[0] = ((v1      ) & mask).gather32_32(pal);
			d1[1] = ((v1 >> 16) & mask).gather32_32(pal);
			v0 = v0.cdab();
			v1 = v1.cdab();
			d2[0] = ((v0 >>  8) & mask).gather32_32(pal);
			d2[1] = ((v0 >> 24)       ).gather32_32(pal);
			d3[0] = ((v1 >>  8) & mask).gather32_32(pal);
			d3[1] = ((v1 >> 24)       ).gather32_32(pal);

			dst += dstpitch * 4;
		}

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i v0, v1, v2, v3;
		GSVector4i mask = m_r8mask;

		for (int i = 0; i < 2; i++)
		{
			v0 = s[i * 8 + 0].shuffle8(mask);
			v1 = s[i * 8 + 1].shuffle8(mask);
			v2 = s[i * 8 + 2].shuffle8(mask);
			v3 = s[i * 8 + 3].shuffle8(mask);

			GSVector4i::sw16(v0, v1, v2, v3);
			GSVector4i::sw32(v0, v1, v3, v2);

			v0.gather32_8<>(pal, (GSVector4i*)dst);
			dst += dstpitch;
			v3.gather32_8<>(pal, (GSVector4i*)dst);
			dst += dstpitch;
			v1.gather32_8<>(pal, (GSVector4i*)dst);
			dst += dstpitch;
			v2.gather32_8<>(pal, (GSVector4i*)dst);
			dst += dstpitch;

			v2 = s[i * 8 + 4].shuffle8(mask);
			v3 = s[i * 8 + 5].shuffle8(mask);
			v0 = s[i * 8 + 6].shuffle8(mask);
			v1 = s[i * 8 + 7].shuffle8(mask);

			GSVector4i::sw16(v0, v1, v2, v3);
			GSVector4i::sw32(v0, v1, v3, v2);

			v0.gather32_8<>(pal, (GSVector4i*)dst);
			dst += dstpitch;
			v3.gather32_8<>(pal, (GSVector4i*)dst);
			dst += dstpitch;
			v1.gather32_8<>(pal, (GSVector4i*)dst);
			dst += dstpitch;
			v2.gather32_8<>(pal, (GSVector4i*)dst);
			dst += dstpitch;
		}

#endif
	}

	// TODO: ReadAndExpandBlock8_16

	/// Load 16-element palette into four vectors, with each u32 split across the four vectors
	template <typename V>
	__forceinline static void LoadPalVecs(const u32* RESTRICT pal, V& p0, V& p1, V& p2, V& p3)
	{
		const GSVector4i* p = (const GSVector4i*)pal;
		p0 = V::broadcast128(p[0]).shuffle8(V::broadcast128(m_palvec_mask));
		p1 = V::broadcast128(p[1]).shuffle8(V::broadcast128(m_palvec_mask));
		p2 = V::broadcast128(p[2]).shuffle8(V::broadcast128(m_palvec_mask));
		p3 = V::broadcast128(p[3]).shuffle8(V::broadcast128(m_palvec_mask));
		V::sw32(p0, p1, p2, p3);
		V::sw64(p0, p1, p2, p3);
		std::swap(p1, p2);
	}

	template <typename V>
	__forceinline static void ReadClut4(const V& p0, const V& p1, const V& p2, const V& p3, const V& src, V& d0, V& d1, V& d2, V& d3)
	{
		V r0 = p0.shuffle8(src);
		V r1 = p1.shuffle8(src);
		V r2 = p2.shuffle8(src);
		V r3 = p3.shuffle8(src);

		V::sw8(r0, r1, r2, r3);
		V::sw16(r0, r1, r2, r3);

		d0 = r0;
		d1 = r2;
		d2 = r1;
		d3 = r3;
	}

	template <typename V>
	__forceinline static void ReadClut4AndWrite(const V& p0, const V& p1, const V& p2, const V& p3, const V& src, V* dst, int dstride)
	{
		ReadClut4(p0, p1, p2, p3, src, dst[dstride * 0], dst[dstride * 1], dst[dstride * 2], dst[dstride * 3]);
	}

	__forceinline static void ReadAndExpandBlock4_32(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		//printf("ReadAndExpandBlock4_32\n");

#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i p0, p1, p2, p3;
		LoadPalVecs(pal, p0, p1, p2, p3);
		GSVector8i shuf = GSVector8i::broadcast128(m_palvec_mask);
		GSVector8i mask(0x0f0f0f0f);

		GSVector8i v0, v1;

		for (int i = 0; i < 2; i++)
		{
			GSVector8i* d0 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 0);
			GSVector8i* d1 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 1);
			GSVector8i* d2 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 2);
			GSVector8i* d3 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 3);

			LoadSW128(v0, v1, &s[i * 4 + 0], &s[i * 4 + 1]);
			GSVector8i::sw64(v0, v1);

			v0 = v0.shuffle8(shuf);
			v1 = v1.shuffle8(shuf);

			ReadClut4AndWrite(p0, p1, p2, p3, v0 & mask, d0, 1);
			ReadClut4AndWrite(p0, p1, p2, p3, v1 & mask, d1, 1);
			v0 = v0.cdab() >> 4;
			v1 = v1.cdab() >> 4;
			ReadClut4AndWrite(p0, p1, p2, p3, v0 & mask, d2, 1);
			ReadClut4AndWrite(p0, p1, p2, p3, v1 & mask, d3, 1);

			dst += dstpitch * 4;

			d0 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 0);
			d1 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 1);
			d2 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 2);
			d3 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 3);

			LoadSW128(v0, v1, &s[i * 4 + 3], &s[i * 4 + 2]);
			GSVector8i::sw64(v0, v1);

			v0 = v0.shuffle8(shuf);
			v1 = v1.shuffle8(shuf);

			ReadClut4AndWrite(p0, p1, p2, p3, v0 & mask, d0, 1);
			ReadClut4AndWrite(p0, p1, p2, p3, v1 & mask, d1, 1);
			v0 = v0.cdab() >> 4;
			v1 = v1.cdab() >> 4;
			ReadClut4AndWrite(p0, p1, p2, p3, v0 & mask, d2, 1);
			ReadClut4AndWrite(p0, p1, p2, p3, v1 & mask, d3, 1);

			dst += dstpitch * 4;
		}

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i p0, p1, p2, p3;
		LoadPalVecs(pal, p0, p1, p2, p3);
		GSVector4i mask(0x0f0f0f0f);

		GSVector4i v0, v1, v2, v3;

		for (int i = 0; i < 2; i++)
		{
			GSVector4i* d0 = reinterpret_cast<GSVector4i*>(dst + dstpitch * 0);
			GSVector4i* d1 = reinterpret_cast<GSVector4i*>(dst + dstpitch * 1);
			GSVector4i* d2 = reinterpret_cast<GSVector4i*>(dst + dstpitch * 2);
			GSVector4i* d3 = reinterpret_cast<GSVector4i*>(dst + dstpitch * 3);

			v0 = s[i * 8 + 0];
			v1 = s[i * 8 + 1];
			v2 = s[i * 8 + 2];
			v3 = s[i * 8 + 3];

			GSVector4i::sw64(v0, v1, v2, v3);

			v0 = v0.shuffle8(m_palvec_mask);
			v1 = v1.shuffle8(m_palvec_mask);
			v2 = v2.shuffle8(m_palvec_mask);
			v3 = v3.shuffle8(m_palvec_mask);

			ReadClut4AndWrite(p0, p1, p2, p3,  v0       & mask, d0 + 0, 2);
			ReadClut4AndWrite(p0, p1, p2, p3, (v0 >> 4) & mask, d2 + 1, 2);
			ReadClut4AndWrite(p0, p1, p2, p3,  v1       & mask, d0 + 1, 2);
			ReadClut4AndWrite(p0, p1, p2, p3, (v1 >> 4) & mask, d2 + 0, 2);
			ReadClut4AndWrite(p0, p1, p2, p3,  v2       & mask, d1 + 0, 2);
			ReadClut4AndWrite(p0, p1, p2, p3, (v2 >> 4) & mask, d3 + 1, 2);
			ReadClut4AndWrite(p0, p1, p2, p3,  v3       & mask, d1 + 1, 2);
			ReadClut4AndWrite(p0, p1, p2, p3, (v3 >> 4) & mask, d3 + 0, 2);

			dst += dstpitch * 4;

			d0 = reinterpret_cast<GSVector4i*>(dst + dstpitch * 0);
			d1 = reinterpret_cast<GSVector4i*>(dst + dstpitch * 1);
			d2 = reinterpret_cast<GSVector4i*>(dst + dstpitch * 2);
			d3 = reinterpret_cast<GSVector4i*>(dst + dstpitch * 3);

			v0 = s[i * 8 + 4];
			v1 = s[i * 8 + 5];
			v2 = s[i * 8 + 6];
			v3 = s[i * 8 + 7];

			GSVector4i::sw64(v0, v1, v2, v3);

			v0 = v0.shuffle8(m_palvec_mask);
			v1 = v1.shuffle8(m_palvec_mask);
			v2 = v2.shuffle8(m_palvec_mask);
			v3 = v3.shuffle8(m_palvec_mask);

			ReadClut4AndWrite(p0, p1, p2, p3,  v0       & mask, d0 + 1, 2);
			ReadClut4AndWrite(p0, p1, p2, p3, (v0 >> 4) & mask, d2 + 0, 2);
			ReadClut4AndWrite(p0, p1, p2, p3,  v1       & mask, d0 + 0, 2);
			ReadClut4AndWrite(p0, p1, p2, p3, (v1 >> 4) & mask, d2 + 1, 2);
			ReadClut4AndWrite(p0, p1, p2, p3,  v2       & mask, d1 + 1, 2);
			ReadClut4AndWrite(p0, p1, p2, p3, (v2 >> 4) & mask, d3 + 0, 2);
			ReadClut4AndWrite(p0, p1, p2, p3,  v3       & mask, d1 + 0, 2);
			ReadClut4AndWrite(p0, p1, p2, p3, (v3 >> 4) & mask, d3 + 1, 2);

			dst += dstpitch * 4;
		}

#endif
	}

	// TODO: ReadAndExpandBlock4_16

	// ReadAndExpandBlock8H for AVX2 platforms with slow VPGATHERDD (Haswell, Zen, Zen2, Zen3)
	// Also serves as the implementation for AVX / SSE
	__forceinline static void ReadAndExpandBlock8H_32HSW(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		for (int i = 0; i < 4; i++)
		{
			const u8* s = src + i * 64;
			GSVector4i* d0 = reinterpret_cast<GSVector4i*>(dst + dstpitch * 0);
			GSVector4i* d1 = reinterpret_cast<GSVector4i*>(dst + dstpitch * 1);

			d0[0] = GSVector4i(pal[s[ 3]], pal[s[ 7]], pal[s[19]], pal[s[23]]);
			d0[1] = GSVector4i(pal[s[35]], pal[s[39]], pal[s[51]], pal[s[55]]);
			d1[0] = GSVector4i(pal[s[11]], pal[s[15]], pal[s[27]], pal[s[31]]);
			d1[1] = GSVector4i(pal[s[43]], pal[s[47]], pal[s[59]], pal[s[63]]);

			dst += dstpitch * 2;
		}
	}

	__forceinline static void ReadAndExpandBlock8H_32(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		//printf("ReadAndExpandBlock8H_32\n");

#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;
		for (int i = 0; i < 4; i++)
		{
			GSVector8i v0, v1;

			LoadSW128(v0, v1, &s[i * 2 + 0], &s[i * 2 + 1]);
			GSVector8i::sw64(v0, v1);

			*reinterpret_cast<GSVector8i*>(dst) = (v0 >> 24).gather32_32(pal);
			dst += dstpitch;

			*reinterpret_cast<GSVector8i*>(dst) = (v1 >> 24).gather32_32(pal);
			dst += dstpitch;
		}

#else

		ReadAndExpandBlock8H_32HSW(src, dst, dstpitch, pal);

#endif
	}

	// TODO: ReadAndExpandBlock8H_16

	template <u32 shift, u32 mask>
	__forceinline static void ReadAndExpandBlock4H_32(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
#if _M_SSE >= 0x501

		const GSVector8i* s = (const GSVector8i*)src;

		GSVector8i p0, p1, p2, p3;
		LoadPalVecs(pal, p0, p1, p2, p3);
		GSVector8i maskvec(mask);
		GSVector8i shufvec = GSVector8i::broadcast128(m_r4hmask_avx2);

		GSVector8i v0, v1, v2, v3;

		for(int i = 0; i < 2; i++)
		{
			GSVector8i* d0 = reinterpret_cast<GSVector8i*>(dst);
			GSVector8i* d1 = reinterpret_cast<GSVector8i*>(dst + dstpitch);
			GSVector8i* d2 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 2);
			GSVector8i* d3 = reinterpret_cast<GSVector8i*>(dst + dstpitch * 3);

			v0 = s[i * 4 + 0] >> shift;
			v1 = s[i * 4 + 1] >> shift;
			v2 = s[i * 4 + 2] >> shift;
			v3 = s[i * 4 + 3] >> shift;

			GSVector8i all = v0.ps32(v1).pu16(v2.ps32(v3)).xzyw().acbd().shuffle8(shufvec);
			if (mask != 0xffffffff)
				all = all & mask;

			ReadClut4(p0, p1, p2, p3, all, *d0, *d1, *d2, *d3);

			dst += dstpitch * 4;
		}

#else

		const GSVector4i* s = (const GSVector4i*)src;

		GSVector4i p0, p1, p2, p3;
		LoadPalVecs(pal, p0, p1, p2, p3);
		GSVector4i maskvec(mask);

		GSVector4i v0, v1, v2, v3;

		for (int i = 0; i < 4; i++)
		{
			GSVector4i* d0 = reinterpret_cast<GSVector4i*>(dst);
			GSVector4i* d1 = reinterpret_cast<GSVector4i*>(dst + dstpitch);

			v0 = s[i * 4 + 0] >> shift;
			v1 = s[i * 4 + 1] >> shift;
			v2 = s[i * 4 + 2] >> shift;
			v3 = s[i * 4 + 3] >> shift;

			GSVector4i all = v0.ps32(v1).pu16(v2.ps32(v3)).shuffle8(m_r4hmask);
			if (mask != 0xffffffff)
				all = all & mask;

			ReadClut4(p0, p1, p2, p3, all, d0[0], d0[1], d1[0], d1[1]);

			dst += dstpitch * 2;
		}

#endif
	}

	__forceinline static void ReadAndExpandBlock4HL_32(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		//printf("ReadAndExpandBlock4HL_32\n");
		ReadAndExpandBlock4H_32<24, 0x0f0f0f0f>(src, dst, dstpitch, pal);
	}

	// TODO: ReadAndExpandBlock4HL_16

	__forceinline static void ReadAndExpandBlock4HH_32(const u8* RESTRICT src, u8* RESTRICT dst, int dstpitch, const u32* RESTRICT pal)
	{
		//printf("ReadAndExpandBlock4HH_32\n");
		ReadAndExpandBlock4H_32<28, 0xffffffff>(src, dst, dstpitch, pal);
	}

	// TODO: ReadAndExpandBlock4HH_16
};
