// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/SW/GSScanlineEnvironment.h"
#include "GS/Renderers/SW/GSNewCodeGenerator.h"
#include "GS/GSUtil.h"
#include "GS/MultiISA.h"

MULTI_ISA_UNSHARED_START

class GSSetupPrimCodeGenerator : public Xbyak::CodeGenerator
{
	void operator=(const GSSetupPrimCodeGenerator&);

	GSScanlineSelector m_sel;

	struct
	{
		u32 z : 1, f : 1, t : 1, c : 1;
	} m_en;

public:
	GSSetupPrimCodeGenerator(u64 key, void* code, size_t maxsize);
};

MULTI_ISA_UNSHARED_END
