// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
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
	// Return RX entry pointer, not the RW write pointer.
	const u8* GetCode() const { return m_code_rx; }

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

	// RX entry pointer; GetCode() must return RX even when the emitter writes through the RW alias.
	const u8* m_code_rx;
};
