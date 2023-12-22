// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSSetupPrimCodeGenerator.h"
#include "GSSetupPrimCodeGenerator.all.h"

MULTI_ISA_UNSHARED_IMPL;

GSSetupPrimCodeGenerator::GSSetupPrimCodeGenerator(u64 key, void* code, size_t maxsize)
	: Xbyak::CodeGenerator(maxsize, code)
{
	m_sel.key = key;

	m_en.z = m_sel.zb ? 1 : 0;
	m_en.f = m_sel.fb && m_sel.fge ? 1 : 0;
	m_en.t = m_sel.fb && m_sel.tfx != TFX_NONE ? 1 : 0;
	m_en.c = m_sel.fb && !(m_sel.tfx == TFX_DECAL && m_sel.tcc) ? 1 : 0;

	GSSetupPrimCodeGenerator2(this, g_cpu, key).Generate();
}
