// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#include "common/Console.h"
#include "MTVU.h"
#include "SaveState.h"
#include "vtlb.h"

#include "common/Assertions.h"

void vtlb_DynBackpatchLoadStore(uptr code_address, u32 code_size, u32 guest_pc, u32 guest_addr, u32 gpr_bitmask, u32 fpr_bitmask, u8 address_register, u8 data_register, u8 size_in_bits, bool is_signed, bool is_load, bool is_fpr)
{
  pxFailRel("Not implemented.");
}

bool SaveStateBase::vuJITFreeze()
{
	if(IsSaving())
		vu1Thread.WaitVU();

	Console.Warning("recompiler state is stubbed in arm64!");

	// HACK!!

	// size of microRegInfo structure
	std::array<u8,96> empty_data{};
	Freeze(empty_data);
	Freeze(empty_data);
	return true;
}
