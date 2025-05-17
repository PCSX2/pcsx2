// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#include "GS/Renderers/SW/GSSetupPrimCodeGenerator.arm64.h"
#include "GS/Renderers/SW/GSVertexSW.h"

#include "common/StringUtil.h"
#include "common/Perf.h"

#include <cstdint>

MULTI_ISA_UNSHARED_IMPL;

using namespace vixl::aarch64;

static const auto& _vertex = x0;
static const auto& _index = x1;
static const auto& _dscan = x2;
static const auto& _locals = x3;
static const auto& _scratchaddr = x7;
static const auto& _vscratch = v31;

#define _local(field) MemOperand(_locals, offsetof(GSScanlineLocalData, field))
#define armAsm (&m_emitter)

GSSetupPrimCodeGenerator::GSSetupPrimCodeGenerator(u64 key, void* code, size_t maxsize)
	: m_emitter(static_cast<vixl::byte*>(code), maxsize, vixl::aarch64::PositionDependentCode)
	, m_sel(key)
{
	m_en.z = m_sel.zb ? 1 : 0;
	m_en.f = m_sel.fb && m_sel.fge ? 1 : 0;
	m_en.t = m_sel.fb && m_sel.tfx != TFX_NONE ? 1 : 0;
	m_en.c = m_sel.fb && !(m_sel.tfx == TFX_DECAL && m_sel.tcc) ? 1 : 0;
}

void GSSetupPrimCodeGenerator::Generate()
{
	const bool needs_shift = ((m_en.z || m_en.f) && m_sel.prim != GS_SPRITE_CLASS) || m_en.t || (m_en.c && m_sel.iip);
	if (needs_shift)
	{
		armAsm->Mov(x4, reinterpret_cast<intptr_t>(g_const.m_shift_128b));
		for (int i = 0; i < (m_sel.notest ? 2 : 5); i++)
		{
			armAsm->Ldr(VRegister(3 + i, kFormat16B), MemOperand(x4, i * sizeof(g_const.m_shift_128b[0])));
		}
	}

	Depth();

	Texture();

	Color();

	armAsm->Ret();

	armAsm->FinalizeCode();

	Perf::any.RegisterKey(GetCode(), GetSize(), "GSSetupPrim_", m_sel.key);
}

void GSSetupPrimCodeGenerator::Depth()
{
	if (!m_en.z && !m_en.f)
	{
		return;
	}

	if (m_sel.prim != GS_SPRITE_CLASS)
	{
		if (m_en.f)
		{
			// GSVector4 df = t.wwww();
			armAsm->Add(_scratchaddr, _dscan, offsetof(GSVertexSW, t.w));
			armAsm->Ld1r(v1.V4S(), MemOperand(_scratchaddr));

			// m_local.d4.f = GSVector4i(df * 4.0f).xxzzlh();

			armAsm->Fmul(v2.V4S(), v1.V4S(), v3.V4S());
			armAsm->Fcvtzs(v2.V4S(), v2.V4S());
			armAsm->Trn1(v2.V8H(), v2.V8H(), v2.V8H());

			armAsm->Str(v2.V4S(), _local(d4.f));

			for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
			{
				// m_local.d[i].f = GSVector4i(df * m_shift[i]).xxzzlh();

				armAsm->Fmul(v2.V4S(), v1.V4S(), VRegister(4 + i, kFormat4S));
				armAsm->Fcvtzs(v2.V4S(), v2.V4S());
				armAsm->Trn1(v2.V8H(), v2.V8H(), v2.V8H());

				armAsm->Str(v2.V4S(), _local(d[i].f));
			}
		}

		if (m_en.z)
		{
			// VectorF dz = VectorF::broadcast64(&dscan.p.z)
			armAsm->Add(_scratchaddr, _dscan, offsetof(GSVertexSW, p.z));
			armAsm->Ld1r(_vscratch.V2D(), MemOperand(_scratchaddr));

			// m_local.d4.z = dz.mul64(GSVector4::f32to64(shift));
			armAsm->Fcvtl(v1.V2D(), v3.V2S());
			armAsm->Fmul(v1.V2D(), v1.V2D(), _vscratch.V2D());
			armAsm->Str(v1.V2D(), _local(d4.z));

			armAsm->Fcvtn(v0.V2S(), _vscratch.V2D());
			armAsm->Fcvtn2(v0.V4S(), _vscratch.V2D());

			for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
			{
				// m_local.d[i].z0 = dz.mul64(VectorF::f32to64(half_shift[2 * i + 2]));
				// m_local.d[i].z1 = dz.mul64(VectorF::f32to64(half_shift[2 * i + 3]));

				armAsm->Fmul(v1.V4S(), v0.V4S(), VRegister(4 + i, kFormat4S));
				armAsm->Str(v1.V4S(), _local(d[i].z));
			}
		}
	}
	else
	{
		// GSVector4 p = vertex[index[1]].p;

		armAsm->Ldrh(w4, MemOperand(_index, sizeof(u16)));
		armAsm->Lsl(w4, w4, 6); // * sizeof(GSVertexSW)
		armAsm->Add(x4, _vertex, x4);

		if (m_en.f)
		{
			// m_local.p.f = GSVector4i(p).zzzzh().zzzz();

			armAsm->Ldr(v0, MemOperand(x4, offsetof(GSVertexSW, p)));

			armAsm->Fcvtzs(v1.V4S(), v0.V4S());
			armAsm->Dup(v1.V8H(), v1.V8H(), 6);

			armAsm->Str(v1, MemOperand(_locals, offsetof(GSScanlineLocalData, p.f)));
		}

		if (m_en.z)
		{
			// uint32 z is bypassed in t.w

			armAsm->Add(_scratchaddr, x4, offsetof(GSVertexSW, t.w));
			armAsm->Ld1r(v0.V4S(), MemOperand(_scratchaddr));
			armAsm->Str(v0, MemOperand(_locals, offsetof(GSScanlineLocalData, p.z)));
		}
	}
}

void GSSetupPrimCodeGenerator::Texture()
{
	if (!m_en.t)
	{
		return;
	}

	// GSVector4 t = dscan.t;

	armAsm->Ldr(v0, MemOperand(_dscan, offsetof(GSVertexSW, t)));
	armAsm->Fmul(v1.V4S(), v0.V4S(), v3.V4S());

	if (m_sel.fst)
	{
		// m_local.d4.stq = GSVector4i(t * 4.0f);
		armAsm->Fcvtzs(v1.V4S(), v1.V4S());
		armAsm->Str(v1, MemOperand(_locals, offsetof(GSScanlineLocalData, d4.stq)));
	}
	else
	{
		// m_local.d4.stq = t * 4.0f;
		armAsm->Str(v1, MemOperand(_locals, offsetof(GSScanlineLocalData, d4.stq)));
	}

	for (int j = 0, k = m_sel.fst ? 2 : 3; j < k; j++)
	{
		// GSVector4 ds = t.xxxx();
		// GSVector4 dt = t.yyyy();
		// GSVector4 dq = t.zzzz();

		armAsm->Dup(v1.V4S(), v0.V4S(), j);

		for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
		{
			// GSVector4 v = ds/dt * m_shift[i];

			armAsm->Fmul(v2.V4S(), v1.V4S(), VRegister(4 + i, 128, 4));

			if (m_sel.fst)
			{
				// m_local.d[i].s/t = GSVector4i(v);

				armAsm->Fcvtzs(v2.V4S(), v2.V4S());

				switch (j)
				{
					case 0: armAsm->Str(v2, _local(d[i].s)); break;
					case 1: armAsm->Str(v2, _local(d[i].t)); break;
				}
			}
			else
			{
				// m_local.d[i].s/t/q = v;

				switch (j)
				{
					case 0: armAsm->Str(v2, _local(d[i].s)); break;
					case 1: armAsm->Str(v2, _local(d[i].t)); break;
					case 2: armAsm->Str(v2, _local(d[i].q)); break;
				}
			}
		}
	}
}

void GSSetupPrimCodeGenerator::Color()
{
	if (!m_en.c)
	{
		return;
	}

	if (m_sel.iip)
	{
		// GSVector4 c = dscan.c;
		armAsm->Ldr(v16, MemOperand(_dscan, offsetof(GSVertexSW, c)));

		// m_local.d4.c = GSVector4i(c * 4.0f).xzyw().ps32();

		armAsm->Fmul(v2.V4S(), v16.V4S(), v3.V4S());
		armAsm->Fcvtzs(v2.V4S(), v2.V4S());
		armAsm->Rev64(_vscratch.V4S(), v2.V4S());
		armAsm->Uzp1(v2.V4S(), v2.V4S(), _vscratch.V4S());
		armAsm->Sqxtn(v2.V4H(), v2.V4S());
		armAsm->Dup(v2.V2D(), v2.V2D(), 0);
		armAsm->Str(v2, MemOperand(_locals, offsetof(GSScanlineLocalData, d4.c)));

		// GSVector4 dr = c.xxxx();
		// GSVector4 db = c.zzzz();

		armAsm->Dup(v0.V4S(), v16.V4S(), 0);
		armAsm->Dup(v1.V4S(), v16.V4S(), 2);

		for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
		{
			// GSVector4i r = GSVector4i(dr * m_shift[i]).ps32();

			armAsm->Fmul(v2.V4S(), v0.V4S(), VRegister(4 + i, kFormat4S));
			armAsm->Fcvtzs(v2.V4S(), v2.V4S());
			armAsm->Sqxtn(v2.V4H(), v2.V4S());
			armAsm->Dup(v2.V2D(), v2.V2D(), 0);

			// GSVector4i b = GSVector4i(db * m_shift[i]).ps32();

			armAsm->Fmul(v3.V4S(), v1.V4S(), VRegister(4 + i, kFormat4S));
			armAsm->Fcvtzs(v3.V4S(), v3.V4S());
			armAsm->Sqxtn(v3.V4H(), v3.V4S());
			armAsm->Dup(v3.V2D(), v3.V2D(), 0);

			// m_local.d[i].rb = r.upl16(b);

			armAsm->Zip1(v2.V8H(), v2.V8H(), v3.V8H());
			armAsm->Str(v2, _local(d[i].rb));
		}

		// GSVector4 c = dscan.c;

		// GSVector4 dg = c.yyyy();
		// GSVector4 da = c.wwww();

		armAsm->Dup(v0.V4S(), v16.V4S(), 1);
		armAsm->Dup(v1.V4S(), v16.V4S(), 3);

		for (int i = 0; i < (m_sel.notest ? 1 : 4); i++)
		{
			// GSVector4i g = GSVector4i(dg * m_shift[i]).ps32();

			armAsm->Fmul(v2.V4S(), v0.V4S(), VRegister(4 + i, kFormat4S));
			armAsm->Fcvtzs(v2.V4S(), v2.V4S());
			armAsm->Sqxtn(v2.V4H(), v2.V4S());
			armAsm->Dup(v2.V2D(), v2.V2D(), 0);

			// GSVector4i a = GSVector4i(da * m_shift[i]).ps32();

			armAsm->Fmul(v3.V4S(), v1.V4S(), VRegister(4 + i, kFormat4S));
			armAsm->Fcvtzs(v3.V4S(), v3.V4S());
			armAsm->Sqxtn(v3.V4H(), v3.V4S());
			armAsm->Dup(v3.V2D(), v3.V2D(), 0);

			// m_local.d[i].ga = g.upl16(a);

			armAsm->Zip1(v2.V8H(), v2.V8H(), v3.V8H());
			armAsm->Str(v2, _local(d[i].ga));
		}
	}
	else
	{
		// GSVector4i c = GSVector4i(vertex[index[last].c);

		int last = 0;

		switch (m_sel.prim)
		{
			case GS_POINT_CLASS:    last = 0; break;
			case GS_LINE_CLASS:     last = 1; break;
			case GS_TRIANGLE_CLASS: last = 2; break;
			case GS_SPRITE_CLASS:   last = 1; break;
		}

		if (!(m_sel.prim == GS_SPRITE_CLASS && (m_en.z || m_en.f))) // if this is a sprite, the last vertex was already loaded in Depth()
		{
			armAsm->Ldrh(w4, MemOperand(_index, sizeof(u16) * last));
			armAsm->Lsl(w4, w4, 6); // * sizeof(GSVertexSW)
			armAsm->Add(x4, _vertex, x4);
		}

		armAsm->Ldr(v0, MemOperand(x4, offsetof(GSVertexSW, c)));
		armAsm->Fcvtzs(v0.V4S(), v0.V4S());

		// c = c.upl16(c.zwxy());

		armAsm->Ext(v1.V16B(), v0.V16B(), v0.V16B(), 8);
		armAsm->Zip1(v0.V8H(), v0.V8H(), v1.V8H());

		// if (!tme) c = c.srl16(7);

		if (m_sel.tfx == TFX_NONE)
			armAsm->Ushr(v0.V8H(), v0.V8H(), 7);

		// m_local.c.rb = c.xxxx();
		// m_local.c.ga = c.zzzz();

		armAsm->Dup(v1.V4S(), v0.V4S(), 0);
		armAsm->Dup(v2.V4S(), v0.V4S(), 2);

		armAsm->Str(v1, _local(c.rb));
		armAsm->Str(v2, _local(c.ga));
	}
}
