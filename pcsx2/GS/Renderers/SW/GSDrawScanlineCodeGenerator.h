// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/SW/GSScanlineEnvironment.h"
#include "GS/Renderers/SW/GSNewCodeGenerator.h"
#include "GS/GSUtil.h"
#include "GS/MultiISA.h"

MULTI_ISA_UNSHARED_START

class GSDrawScanlineCodeGenerator : public Xbyak::CodeGenerator
{
	GSDrawScanlineCodeGenerator(const GSDrawScanlineCodeGenerator&) = delete;
	void operator=(const GSDrawScanlineCodeGenerator&) = delete;

	GSScanlineSelector m_sel;

public:
	GSDrawScanlineCodeGenerator(u64 key, void* code, size_t maxsize);
};

MULTI_ISA_UNSHARED_END
