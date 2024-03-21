// SPDX-FileCopyrightText: 2021-2023 Connor McLaughlin <stenzek@gmail.com>, PCSX2 Team
// SPDX-License-Identifier: GPL-3.0

#pragma once

#include "GS/Renderers/Common/GSFunctionMap.h"
#include "GS/Renderers/SW/GSScanlineEnvironment.h"

#include "vixl/aarch64/macro-assembler-aarch64.h"

class GSSetupPrimCodeGenerator
{
public:
	GSSetupPrimCodeGenerator(u64 key, void* code, size_t maxsize);
	void Generate();

	size_t GetSize() const { return m_emitter.GetSizeOfCodeGenerated(); }
	const u8* GetCode() const { return m_emitter.GetBuffer().GetStartAddress<const u8*>(); }

private:
	void Depth();
	void Texture();
	void Color();

	vixl::aarch64::MacroAssembler m_emitter;

	GSScanlineSelector m_sel;

	struct
	{
		u32 z : 1, f : 1, t : 1, c : 1;
	} m_en;
};
