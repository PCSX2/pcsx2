// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) macro-mode analysis — Phase 7.9 (M0).
//
// Arch-neutral analysis ported faithfully from the x86 recompiler. This file is
// only compiled on ARM64 (pcsx2arm64Sources). It holds no VIXL / NEON emission;
// it is pure decode/analysis over EE opcodes, so the logic is copied verbatim
// from pcsx2/x86/ix86-32/iR5900.cpp (cop2flags) and pcsx2/x86/iR5900Analysis.cpp
// (the COP2 passes, ported in M1). Keeping it in its own TU mirrors the x86
// split and keeps aR5900.cpp focused on codegen.

#include "arm64/aR5900Analysis.h"

// Set per-op in recRecompile (aR5900.cpp); the macro-mode emit reads the COP2_*
// bits on g_pCurInstInfo->info to drive lazy VU0 sync. Defined here so both the
// analysis passes and the rec share one definition.
EEINST* g_pCurInstInfo = nullptr;

// opcode 'code' modifies:
// 1: status
// 2: MAC
// 4: clip
//
// Verbatim port of cop2flags() from pcsx2/x86/ix86-32/iR5900.cpp:1384 — pure bit
// decode, no x86 specifics. (022 == 0x12 == COP2 primary opcode.)
int cop2flags(u32 code)
{
	if (code >> 26 != 022)
		return 0; // not COP2
	if ((code >> 25 & 1) == 0)
		return 0; // a branch or transfer instruction

	switch (code >> 2 & 15)
	{
		case 15:
			switch (code >> 6 & 0x1f)
			{
				case 4: // ITOF*
				case 5: // FTOI*
				case 12: // MOVE MR32
				case 13: // LQI SQI LQD SQD
				case 15: // MTIR MFIR ILWR ISWR
				case 16: // RNEXT RGET RINIT RXOR
					return 0;
				case 7: // MULAq, ABS, MULAi, CLIP
					if ((code & 3) == 1) // ABS
						return 0;
					if ((code & 3) == 3) // CLIP
						return 4;
					return 3;
				case 11: // SUBA, MSUBA, OPMULA, NOP
					if ((code & 3) == 3) // NOP
						return 0;
					return 3;
				case 14: // DIV, SQRT, RSQRT, WAITQ
					if ((code & 3) == 3) // WAITQ
						return 0;
					return 1; // but different timing, ugh
				default:
					break;
			}
			break;
		case 4: // MAXbc
		case 5: // MINbc
		case 12: // IADD, ISUB, IADDI
		case 13: // IAND, IOR
		case 14: // VCALLMS, VCALLMSR
			return 0;
		case 7:
			if ((code & 1) == 1) // MAXi, MINIi
				return 0;
			return 3;
		case 10:
			if ((code & 3) == 3) // MAX
				return 0;
			return 3;
		case 11:
			if ((code & 3) == 3) // MINI
				return 0;
			return 3;
		default:
			break;
	}
	return 3;
}
